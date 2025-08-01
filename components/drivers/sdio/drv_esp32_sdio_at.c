/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-XX     Assistant    first version - ESP32 SDIO AT command driver
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "drv_esp32_sdio_at.h"

#define DBG_TAG               "ESP32_SDIO_AT"
#ifdef RT_ESP32_SDIO_AT_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif
#include <rtdbg.h>

/* Global driver context */
static esp32_sdio_at_ctx_t *g_esp32_ctx = RT_NULL;
static rt_uint8_t g_debug_level = 1;

/* Internal function prototypes */
static rt_err_t esp32_sdio_write_reg(struct rt_sdio_function *func, rt_uint32_t reg, rt_uint8_t val);
static rt_uint8_t esp32_sdio_read_reg(struct rt_sdio_function *func, rt_uint32_t reg, rt_err_t *err);
static rt_err_t esp32_sdio_write_data(struct rt_sdio_function *func, rt_uint32_t addr, 
                                     rt_uint8_t *buf, rt_uint32_t len);
static rt_err_t esp32_sdio_read_data(struct rt_sdio_function *func, rt_uint32_t addr, 
                                    rt_uint8_t *buf, rt_uint32_t len);
static void esp32_sdio_irq_handler(struct rt_sdio_function *func);
static void esp32_sdio_irq_thread_entry(void *parameter);
static rt_err_t esp32_sdio_wait_ready(struct rt_sdio_function *func, rt_uint32_t timeout);
static esp32_at_resp_type_t esp32_parse_response(const char *response);

/**
 * @brief Write to ESP32 SDIO register
 */
static rt_err_t esp32_sdio_write_reg(struct rt_sdio_function *func, rt_uint32_t reg, rt_uint8_t val)
{
    rt_err_t ret;
    
    ret = sdio_io_writeb(func, reg, val);
    if (ret != RT_EOK)
    {
        if (g_debug_level >= 1)
            LOG_E("Failed to write reg 0x%08x: %d", reg, ret);
    }
    
    return ret;
}

/**
 * @brief Read from ESP32 SDIO register
 */
static rt_uint8_t esp32_sdio_read_reg(struct rt_sdio_function *func, rt_uint32_t reg, rt_err_t *err)
{
    rt_int32_t error = 0;
    rt_uint8_t val;
    
    val = sdio_io_readb(func, reg, &error);
    if (error != 0)
    {
        if (g_debug_level >= 1)
            LOG_E("Failed to read reg 0x%08x: %d", reg, error);
        if (err) *err = error;
        return 0;
    }
    
    if (err) *err = RT_EOK;
    return val;
}

/**
 * @brief Write data to ESP32 via SDIO
 */
static rt_err_t esp32_sdio_write_data(struct rt_sdio_function *func, rt_uint32_t addr, 
                                     rt_uint8_t *buf, rt_uint32_t len)
{
    rt_err_t ret;
    
    if (len <= func->cur_blk_size)
    {
        ret = sdio_io_write_multi_incr_b(func, addr, buf, len);
    }
    else
    {
        ret = sdio_io_rw_extended_block(func, 1, addr, 1, buf, len);
    }
    
    if (ret != RT_EOK)
    {
        if (g_debug_level >= 1)
            LOG_E("Failed to write data to addr 0x%08x, len %d: %d", addr, len, ret);
    }
    
    return ret;
}

/**
 * @brief Read data from ESP32 via SDIO
 */
static rt_err_t esp32_sdio_read_data(struct rt_sdio_function *func, rt_uint32_t addr, 
                                    rt_uint8_t *buf, rt_uint32_t len)
{
    rt_err_t ret;
    
    if (len <= func->cur_blk_size)
    {
        ret = sdio_io_read_multi_incr_b(func, addr, buf, len);
    }
    else
    {
        ret = sdio_io_rw_extended_block(func, 0, addr, 1, buf, len);
    }
    
    if (ret != RT_EOK)
    {
        if (g_debug_level >= 1)
            LOG_E("Failed to read data from addr 0x%08x, len %d: %d", addr, len, ret);
    }
    
    return ret;
}

/**
 * @brief SDIO interrupt handler
 */
static void esp32_sdio_irq_handler(struct rt_sdio_function *func)
{
    esp32_sdio_at_ctx_t *ctx = (esp32_sdio_at_ctx_t *)sdio_get_drvdata(func);
    rt_uint8_t int_status;
    rt_err_t err;
    
    if (!ctx || !ctx->initialized)
        return;
    
    /* Read interrupt status */
    int_status = esp32_sdio_read_reg(func, ESP32_SDIO_REG_INT_STATUS, &err);
    if (err != RT_EOK)
        return;
    
    if (g_debug_level >= 3)
        LOG_D("IRQ: int_status=0x%02x", int_status);
    
    /* Handle command complete interrupt */
    if (int_status & ESP32_SDIO_INT_CMD_COMPLETE)
    {
        if (g_debug_level >= 2)
            LOG_I("Command complete interrupt");
        rt_sem_release(ctx->resp_sem);
    }
    
    /* Handle response ready interrupt */
    if (int_status & ESP32_SDIO_INT_RESP_READY)
    {
        if (g_debug_level >= 2)
            LOG_I("Response ready interrupt");
        rt_sem_release(ctx->resp_sem);
    }
    
    /* Handle error interrupt */
    if (int_status & ESP32_SDIO_INT_ERROR)
    {
        if (g_debug_level >= 1)
            LOG_E("Error interrupt");
        ctx->error_count++;
        rt_sem_release(ctx->resp_sem);
    }
    
    /* Clear interrupt status */
    esp32_sdio_write_reg(func, ESP32_SDIO_REG_INT_STATUS, int_status);
}

/**
 * @brief IRQ handling thread entry
 */
static void esp32_sdio_irq_thread_entry(void *parameter)
{
    esp32_sdio_at_ctx_t *ctx = (esp32_sdio_at_ctx_t *)parameter;
    
    while (ctx->initialized)
    {
        /* Wait for interrupt or timeout */
        rt_thread_mdelay(10);
        
        /* Check if we need to handle any pending operations */
        if (!ctx->connected)
        {
            rt_thread_mdelay(1000);
            continue;
        }
    }
}

/**
 * @brief Wait for ESP32 to be ready
 */
static rt_err_t esp32_sdio_wait_ready(struct rt_sdio_function *func, rt_uint32_t timeout)
{
    rt_uint32_t start_time = rt_tick_get();
    rt_uint8_t status;
    rt_err_t err;
    
    while ((rt_tick_get() - start_time) < rt_tick_from_millisecond(timeout))
    {
        status = esp32_sdio_read_reg(func, ESP32_SDIO_REG_STATUS, &err);
        if (err != RT_EOK)
            return err;
        
        if (status & ESP32_SDIO_STATUS_READY)
            return RT_EOK;
        
        rt_thread_mdelay(10);
    }
    
    return -RT_ETIMEOUT;
}

/**
 * @brief Parse AT command response
 */
static esp32_at_resp_type_t esp32_parse_response(const char *response)
{
    if (!response)
        return ESP32_AT_RESP_UNKNOWN;
    
    if (rt_strstr(response, ESP32_AT_RESP_OK_STR))
        return ESP32_AT_RESP_OK;
    else if (rt_strstr(response, ESP32_AT_RESP_ERROR_STR))
        return ESP32_AT_RESP_ERROR;
    else if (rt_strstr(response, ESP32_AT_RESP_FAIL_STR))
        return ESP32_AT_RESP_FAIL;
    else if (rt_strlen(response) > 0)
        return ESP32_AT_RESP_DATA;
    
    return ESP32_AT_RESP_UNKNOWN;
}

/**
 * @brief Initialize ESP32 SDIO AT command driver
 */
rt_err_t esp32_sdio_at_init(struct rt_mmcsd_card *card)
{
    esp32_sdio_at_ctx_t *ctx;
    struct rt_sdio_function *func;
    rt_err_t ret = RT_EOK;
    
    if (!card)
    {
        LOG_E("Invalid card parameter");
        return -RT_EINVAL;
    }
    
    if (g_esp32_ctx)
    {
        LOG_W("ESP32 SDIO AT driver already initialized");
        return RT_EOK;
    }
    
    /* Allocate context */
    ctx = (esp32_sdio_at_ctx_t *)rt_malloc(sizeof(esp32_sdio_at_ctx_t));
    if (!ctx)
    {
        LOG_E("Failed to allocate context");
        return -RT_ENOMEM;
    }
    
    rt_memset(ctx, 0, sizeof(esp32_sdio_at_ctx_t));
    
    /* Get SDIO function */
    func = card->sdio_function[ESP32_SDIO_FUNC_AT];
    if (!func)
    {
        LOG_E("ESP32 AT function not found");
        ret = -RT_ENOSYS;
        goto error;
    }
    
    ctx->func = func;
    ctx->card = card;
    
    /* Create synchronization objects */
    ctx->cmd_mutex = rt_mutex_create("esp32_cmd", RT_IPC_FLAG_PRIO);
    if (!ctx->cmd_mutex)
    {
        LOG_E("Failed to create command mutex");
        ret = -RT_ENOMEM;
        goto error;
    }
    
    ctx->resp_sem = rt_sem_create("esp32_resp", 0, RT_IPC_FLAG_PRIO);
    if (!ctx->resp_sem)
    {
        LOG_E("Failed to create response semaphore");
        ret = -RT_ENOMEM;
        goto error;
    }
    
    /* Allocate buffers */
    ctx->tx_buffer = (rt_uint8_t *)rt_malloc(ESP32_SDIO_AT_BUFFER_SIZE);
    ctx->rx_buffer = (rt_uint8_t *)rt_malloc(ESP32_SDIO_AT_BUFFER_SIZE);
    if (!ctx->tx_buffer || !ctx->rx_buffer)
    {
        LOG_E("Failed to allocate buffers");
        ret = -RT_ENOMEM;
        goto error;
    }
    
    /* Enable SDIO function */
    ret = sdio_enable_func(func);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to enable SDIO function: %d", ret);
        goto error;
    }
    
    /* Set block size */
    ret = sdio_set_block_size(func, 512);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to set block size: %d", ret);
        goto error;
    }
    
    /* Attach interrupt handler */
    ret = sdio_attach_irq(func, esp32_sdio_irq_handler);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to attach IRQ handler: %d", ret);
        goto error;
    }
    
    /* Set driver data */
    sdio_set_drvdata(func, ctx);
    
    /* Create IRQ thread */
    ctx->irq_thread = rt_thread_create("esp32_irq", esp32_sdio_irq_thread_entry, ctx,
                                      1024, RT_THREAD_PRIORITY_MAX - 2, 10);
    if (!ctx->irq_thread)
    {
        LOG_E("Failed to create IRQ thread");
        ret = -RT_ENOMEM;
        goto error;
    }
    
    /* Enable interrupts */
    esp32_sdio_write_reg(func, ESP32_SDIO_REG_INT_EN, 
                        ESP32_SDIO_INT_CMD_COMPLETE | ESP32_SDIO_INT_RESP_READY | ESP32_SDIO_INT_ERROR);
    
    ctx->initialized = RT_TRUE;
    g_esp32_ctx = ctx;
    
    /* Start IRQ thread */
    rt_thread_startup(ctx->irq_thread);
    
    /* Wait for ESP32 to be ready */
    ret = esp32_sdio_wait_ready(func, 5000);
    if (ret == RT_EOK)
    {
        ctx->connected = RT_TRUE;
        LOG_I("ESP32 SDIO AT driver initialized successfully");
    }
    else
    {
        LOG_W("ESP32 not ready, but driver initialized");
    }
    
    return RT_EOK;

error:
    if (ctx)
    {
        if (ctx->cmd_mutex)
            rt_mutex_delete(ctx->cmd_mutex);
        if (ctx->resp_sem)
            rt_sem_delete(ctx->resp_sem);
        if (ctx->tx_buffer)
            rt_free(ctx->tx_buffer);
        if (ctx->rx_buffer)
            rt_free(ctx->rx_buffer);
        if (ctx->irq_thread)
            rt_thread_delete(ctx->irq_thread);
        rt_free(ctx);
    }
    
    return ret;
}

/**
 * @brief Deinitialize ESP32 SDIO AT command driver
 */
rt_err_t esp32_sdio_at_deinit(void)
{
    esp32_sdio_at_ctx_t *ctx = g_esp32_ctx;
    
    if (!ctx)
        return RT_EOK;
    
    ctx->initialized = RT_FALSE;
    ctx->connected = RT_FALSE;
    
    /* Disable interrupts */
    if (ctx->func)
    {
        esp32_sdio_write_reg(ctx->func, ESP32_SDIO_REG_INT_EN, 0);
        sdio_detach_irq(ctx->func);
        sdio_disable_func(ctx->func);
    }
    
    /* Delete thread */
    if (ctx->irq_thread)
    {
        rt_thread_delete(ctx->irq_thread);
    }
    
    /* Clean up resources */
    if (ctx->cmd_mutex)
        rt_mutex_delete(ctx->cmd_mutex);
    if (ctx->resp_sem)
        rt_sem_delete(ctx->resp_sem);
    if (ctx->tx_buffer)
        rt_free(ctx->tx_buffer);
    if (ctx->rx_buffer)
        rt_free(ctx->rx_buffer);
    
    rt_free(ctx);
    g_esp32_ctx = RT_NULL;
    
    LOG_I("ESP32 SDIO AT driver deinitialized");
    return RT_EOK;
}

/**
 * @brief Send AT command to ESP32
 */
rt_err_t esp32_sdio_at_send_cmd(esp32_at_cmd_t *cmd)
{
    esp32_sdio_at_ctx_t *ctx = g_esp32_ctx;
    rt_err_t ret = RT_EOK;
    rt_uint32_t cmd_len, resp_len;
    rt_uint8_t status;
    
    if (!ctx || !ctx->initialized || !cmd || !cmd->command)
    {
        return -RT_EINVAL;
    }
    
    if (!ctx->connected)
    {
        LOG_W("ESP32 not connected");
        return -RT_ENOTCONN;
    }
    
    /* Take command mutex */
    ret = rt_mutex_take(ctx->cmd_mutex, rt_tick_from_millisecond(cmd->timeout));
    if (ret != RT_EOK)
    {
        LOG_E("Failed to take command mutex");
        return ret;
    }
    
    ctx->cmd_count++;
    
    /* Prepare command */
    cmd_len = rt_strlen(cmd->command);
    if (cmd_len >= ESP32_SDIO_AT_BUFFER_SIZE - 2)
    {
        LOG_E("Command too long: %d", cmd_len);
        ret = -RT_EINVAL;
        goto exit;
    }
    
    rt_memcpy(ctx->tx_buffer, cmd->command, cmd_len);
    ctx->tx_buffer[cmd_len] = '\r';
    ctx->tx_buffer[cmd_len + 1] = '\n';
    cmd_len += 2;
    
    if (g_debug_level >= 2)
        LOG_I("Sending AT command: %s", cmd->command);
    
    /* Wait for ESP32 to be ready */
    ret = esp32_sdio_wait_ready(ctx->func, 1000);
    if (ret != RT_EOK)
    {
        LOG_E("ESP32 not ready for command");
        ctx->timeout_count++;
        goto exit;
    }
    
    /* Write command length */
    ret = esp32_sdio_write_reg(ctx->func, ESP32_SDIO_REG_DATA_LEN, cmd_len);
    if (ret != RT_EOK)
        goto exit;
    
    /* Write command data */
    ret = esp32_sdio_write_data(ctx->func, ESP32_SDIO_REG_CMD, ctx->tx_buffer, cmd_len);
    if (ret != RT_EOK)
        goto exit;
    
    /* Trigger command execution */
    ret = esp32_sdio_write_reg(ctx->func, ESP32_SDIO_REG_STATUS, ESP32_SDIO_STATUS_CMD_PENDING);
    if (ret != RT_EOK)
        goto exit;
    
    /* Wait for response */
    ret = rt_sem_take(ctx->resp_sem, rt_tick_from_millisecond(cmd->timeout));
    if (ret != RT_EOK)
    {
        LOG_E("Command timeout: %s", cmd->command);
        ctx->timeout_count++;
        cmd->resp_type = ESP32_AT_RESP_TIMEOUT;
        goto exit;
    }
    
    /* Check status */
    status = esp32_sdio_read_reg(ctx->func, ESP32_SDIO_REG_STATUS, &ret);
    if (ret != RT_EOK)
        goto exit;
    
    if (status & ESP32_SDIO_STATUS_ERROR)
    {
        LOG_E("Command error: %s", cmd->command);
        ctx->error_count++;
        cmd->resp_type = ESP32_AT_RESP_ERROR;
        ret = -RT_ERROR;
        goto exit;
    }
    
    /* Read response if buffer provided */
    if (cmd->response && cmd->resp_size > 0)
    {
        /* Read response length */
        resp_len = esp32_sdio_read_reg(ctx->func, ESP32_SDIO_REG_DATA_LEN, &ret);
        if (ret != RT_EOK)
            goto exit;
        
        if (resp_len > 0 && resp_len < ESP32_SDIO_AT_BUFFER_SIZE)
        {
            /* Read response data */
            ret = esp32_sdio_read_data(ctx->func, ESP32_SDIO_REG_CMD, ctx->rx_buffer, resp_len);
            if (ret != RT_EOK)
                goto exit;
            
            /* Copy to user buffer */
            rt_size_t copy_len = (resp_len < cmd->resp_size - 1) ? resp_len : (cmd->resp_size - 1);
            rt_memcpy(cmd->response, ctx->rx_buffer, copy_len);
            cmd->response[copy_len] = '\0';
            
            /* Parse response type */
            cmd->resp_type = esp32_parse_response(cmd->response);
            
            if (g_debug_level >= 2)
                LOG_I("Received response: %s", cmd->response);
        }
        else
        {
            cmd->response[0] = '\0';
            cmd->resp_type = ESP32_AT_RESP_UNKNOWN;
        }
    }

exit:
    rt_mutex_release(ctx->cmd_mutex);
    return ret;
}

/**
 * @brief Send simple AT command with string response
 */
rt_err_t esp32_sdio_at_send_simple(const char *cmd_str, char *resp_buf, 
                                  rt_size_t resp_size, rt_uint32_t timeout)
{
    esp32_at_cmd_t cmd;
    
    if (!cmd_str)
        return -RT_EINVAL;
    
    rt_memset(&cmd, 0, sizeof(cmd));
    cmd.command = (char *)cmd_str;
    cmd.response = resp_buf;
    cmd.resp_size = resp_size;
    cmd.timeout = timeout;
    
    return esp32_sdio_at_send_cmd(&cmd);
}

/**
 * @brief Check if ESP32 is ready for commands
 */
rt_bool_t esp32_sdio_at_is_ready(void)
{
    esp32_sdio_at_ctx_t *ctx = g_esp32_ctx;
    
    if (!ctx || !ctx->initialized)
        return RT_FALSE;
    
    return ctx->connected;
}

/**
 * @brief Get driver statistics
 */
void esp32_sdio_at_get_stats(rt_uint32_t *cmd_count, rt_uint32_t *error_count, 
                            rt_uint32_t *timeout_count)
{
    esp32_sdio_at_ctx_t *ctx = g_esp32_ctx;
    
    if (!ctx)
        return;
    
    if (cmd_count)
        *cmd_count = ctx->cmd_count;
    if (error_count)
        *error_count = ctx->error_count;
    if (timeout_count)
        *timeout_count = ctx->timeout_count;
}

/**
 * @brief Reset ESP32 SDIO AT driver
 */
rt_err_t esp32_sdio_at_reset(void)
{
    esp32_sdio_at_ctx_t *ctx = g_esp32_ctx;
    
    if (!ctx || !ctx->initialized)
        return -RT_EINVAL;
    
    /* Reset statistics */
    ctx->cmd_count = 0;
    ctx->error_count = 0;
    ctx->timeout_count = 0;
    
    /* Try to reset ESP32 */
    char resp_buf[64];
    rt_err_t ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_RESET, resp_buf, sizeof(resp_buf), 10000);
    
    if (ret == RT_EOK)
    {
        /* Wait for ESP32 to restart */
        rt_thread_mdelay(2000);
        ctx->connected = esp32_sdio_wait_ready(ctx->func, 5000) == RT_EOK;
    }
    
    return ret;
}

/**
 * @brief Set debug level for ESP32 SDIO AT driver
 */
void esp32_sdio_at_set_debug(rt_uint8_t level)
{
    g_debug_level = level;
    LOG_I("Debug level set to %d", level);
}