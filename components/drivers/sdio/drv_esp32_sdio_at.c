/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT Command Driver
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/dev_mmcsd_core.h>
#include <drivers/dev_sdio.h>
#include "drv_esp32_sdio_at.h"

#define DBG_TAG               "ESP32_SDIO_AT"
#define DBG_LVL               DBG_INFO
#include <rtdbg.h>

/* ESP32 SDIO AT Command Driver Configuration */
#define ESP32_SDIO_BLOCK_SIZE       512
#define ESP32_SDIO_MAX_RESPONSE     2048
#define ESP32_SDIO_CMD_TIMEOUT      5000
#define ESP32_SDIO_THREAD_STACK     2048
#define ESP32_SDIO_THREAD_PRIORITY  20

/* ESP32 SDIO Function Numbers */
#define ESP32_SDIO_FUNC_AT          1
#define ESP32_SDIO_FUNC_DATA        2

/* ESP32 SDIO Registers */
#define ESP32_SDIO_REG_CMD_STATUS   0x00
#define ESP32_SDIO_REG_CMD_DATA     0x04
#define ESP32_SDIO_REG_RESP_STATUS  0x08
#define ESP32_SDIO_REG_RESP_DATA    0x0C

/* Status Flags */
#define ESP32_SDIO_STATUS_CMD_READY    0x01
#define ESP32_SDIO_STATUS_RESP_READY   0x02
#define ESP32_SDIO_STATUS_ERROR        0x80

/* ESP32 SDIO AT Device Structure */
struct esp32_sdio_at_device
{
    struct rt_device device;
    struct rt_mmcsd_card *card;
    struct rt_sdio_function *func_at;
    struct rt_sdio_function *func_data;
    
    rt_mutex_t lock;
    rt_sem_t cmd_sem;
    rt_sem_t resp_sem;
    
    rt_thread_t resp_thread;
    rt_uint8_t *cmd_buffer;
    rt_uint8_t *resp_buffer;
    
    rt_uint32_t cmd_len;
    rt_uint32_t resp_len;
    rt_bool_t initialized;
};

static struct esp32_sdio_at_device *g_esp32_at_dev = RT_NULL;

/* Function Prototypes */
static rt_err_t esp32_sdio_at_init(rt_device_t dev);
static rt_err_t esp32_sdio_at_open(rt_device_t dev, rt_uint16_t oflag);
static rt_err_t esp32_sdio_at_close(rt_device_t dev);
static rt_ssize_t esp32_sdio_at_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
static rt_ssize_t esp32_sdio_at_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
static rt_err_t esp32_sdio_at_control(rt_device_t dev, int cmd, void *args);

/* Device Operations */
static const struct rt_device_ops esp32_sdio_at_ops =
{
    esp32_sdio_at_init,
    esp32_sdio_at_open,
    esp32_sdio_at_close,
    esp32_sdio_at_read,
    esp32_sdio_at_write,
    esp32_sdio_at_control
};

/**
 * @brief SDIO read register from ESP32
 */
static rt_uint32_t esp32_sdio_read_reg(struct rt_sdio_function *func, rt_uint32_t reg)
{
    rt_uint32_t value = 0;
    rt_err_t err;
    
    err = sdio_io_readl(func, reg, &value);
    if (err != RT_EOK)
    {
        LOG_E("Failed to read register 0x%x: %d", reg, err);
        return 0;
    }
    
    return value;
}

/**
 * @brief SDIO write register to ESP32
 */
static rt_err_t esp32_sdio_write_reg(struct rt_sdio_function *func, rt_uint32_t reg, rt_uint32_t value)
{
    rt_err_t err;
    
    err = sdio_io_writel(func, reg, value);
    if (err != RT_EOK)
    {
        LOG_E("Failed to write register 0x%x: %d", reg, err);
    }
    
    return err;
}

/**
 * @brief Send AT command to ESP32 via SDIO
 */
static rt_err_t esp32_sdio_send_at_command(struct esp32_sdio_at_device *dev, const char *cmd, rt_size_t len)
{
    rt_err_t err;
    rt_uint32_t status;
    rt_uint32_t timeout = ESP32_SDIO_CMD_TIMEOUT;
    
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(cmd != RT_NULL);
    RT_ASSERT(len > 0);
    
    /* Wait for ESP32 to be ready for command */
    while (timeout--)
    {
        status = esp32_sdio_read_reg(dev->func_at, ESP32_SDIO_REG_CMD_STATUS);
        if (status & ESP32_SDIO_STATUS_CMD_READY)
            break;
        rt_thread_mdelay(1);
    }
    
    if (timeout == 0)
    {
        LOG_E("ESP32 not ready for command");
        return -RT_ETIMEOUT;
    }
    
    /* Copy command to buffer and ensure null termination */
    rt_memset(dev->cmd_buffer, 0, ESP32_SDIO_BLOCK_SIZE);
    rt_memcpy(dev->cmd_buffer, cmd, len);
    dev->cmd_len = len;
    
    /* Write command length */
    err = esp32_sdio_write_reg(dev->func_at, ESP32_SDIO_REG_CMD_DATA, len);
    if (err != RT_EOK)
    {
        LOG_E("Failed to write command length");
        return err;
    }
    
    /* Write command data via SDIO block transfer */
    err = sdio_io_write_multi_fifo_b(dev->func_at, ESP32_SDIO_REG_CMD_DATA, 
                                     dev->cmd_buffer, ESP32_SDIO_BLOCK_SIZE);
    if (err != RT_EOK)
    {
        LOG_E("Failed to write command data");
        return err;
    }
    
    LOG_D("Sent AT command: %.*s", (int)len, cmd);
    return RT_EOK;
}

/**
 * @brief Response handling thread
 */
static void esp32_sdio_resp_thread_entry(void *parameter)
{
    struct esp32_sdio_at_device *dev = (struct esp32_sdio_at_device *)parameter;
    rt_uint32_t status;
    rt_err_t err;
    
    LOG_D("ESP32 SDIO response thread started");
    
    while (1)
    {
        /* Check for response ready */
        status = esp32_sdio_read_reg(dev->func_at, ESP32_SDIO_REG_RESP_STATUS);
        
        if (status & ESP32_SDIO_STATUS_RESP_READY)
        {
            /* Read response length */
            rt_uint32_t resp_len = esp32_sdio_read_reg(dev->func_at, ESP32_SDIO_REG_RESP_DATA);
            
            if (resp_len > 0 && resp_len <= ESP32_SDIO_MAX_RESPONSE)
            {
                /* Clear response buffer */
                rt_memset(dev->resp_buffer, 0, ESP32_SDIO_MAX_RESPONSE);
                
                /* Read response data */
                rt_uint32_t blocks = (resp_len + ESP32_SDIO_BLOCK_SIZE - 1) / ESP32_SDIO_BLOCK_SIZE;
                err = sdio_io_read_multi_fifo_b(dev->func_at, ESP32_SDIO_REG_RESP_DATA,
                                                dev->resp_buffer, blocks * ESP32_SDIO_BLOCK_SIZE);
                
                if (err == RT_EOK)
                {
                    dev->resp_len = resp_len;
                    LOG_D("Received response: %.*s", (int)resp_len, dev->resp_buffer);
                    rt_sem_release(dev->resp_sem);
                }
                else
                {
                    LOG_E("Failed to read response data");
                }
            }
            
            /* Clear response ready flag */
            esp32_sdio_write_reg(dev->func_at, ESP32_SDIO_REG_RESP_STATUS, 0);
        }
        
        if (status & ESP32_SDIO_STATUS_ERROR)
        {
            LOG_E("ESP32 SDIO error status detected");
            /* Clear error flag */
            esp32_sdio_write_reg(dev->func_at, ESP32_SDIO_REG_RESP_STATUS, 0);
        }
        
        rt_thread_mdelay(10);
    }
}

/**
 * @brief Initialize ESP32 SDIO AT device
 */
static rt_err_t esp32_sdio_at_init(rt_device_t dev)
{
    struct esp32_sdio_at_device *at_dev = (struct esp32_sdio_at_device *)dev;
    rt_err_t err;
    
    if (at_dev->initialized)
        return RT_EOK;
    
    /* Enable SDIO functions */
    err = sdio_enable_func(at_dev->func_at);
    if (err != RT_EOK)
    {
        LOG_E("Failed to enable AT function: %d", err);
        return err;
    }
    
    err = sdio_enable_func(at_dev->func_data);
    if (err != RT_EOK)
    {
        LOG_E("Failed to enable data function: %d", err);
        sdio_disable_func(at_dev->func_at);
        return err;
    }
    
    /* Set block size */
    err = sdio_set_block_size(at_dev->func_at, ESP32_SDIO_BLOCK_SIZE);
    if (err != RT_EOK)
    {
        LOG_E("Failed to set AT function block size: %d", err);
        goto cleanup;
    }
    
    err = sdio_set_block_size(at_dev->func_data, ESP32_SDIO_BLOCK_SIZE);
    if (err != RT_EOK)
    {
        LOG_E("Failed to set data function block size: %d", err);
        goto cleanup;
    }
    
    /* Allocate buffers */
    at_dev->cmd_buffer = rt_malloc(ESP32_SDIO_BLOCK_SIZE);
    at_dev->resp_buffer = rt_malloc(ESP32_SDIO_MAX_RESPONSE);
    
    if (!at_dev->cmd_buffer || !at_dev->resp_buffer)
    {
        LOG_E("Failed to allocate buffers");
        err = -RT_ENOMEM;
        goto cleanup;
    }
    
    /* Create response handling thread */
    at_dev->resp_thread = rt_thread_create("esp32_resp",
                                           esp32_sdio_resp_thread_entry,
                                           at_dev,
                                           ESP32_SDIO_THREAD_STACK,
                                           ESP32_SDIO_THREAD_PRIORITY,
                                           20);
    
    if (at_dev->resp_thread == RT_NULL)
    {
        LOG_E("Failed to create response thread");
        err = -RT_ERROR;
        goto cleanup;
    }
    
    rt_thread_startup(at_dev->resp_thread);
    
    at_dev->initialized = RT_TRUE;
    LOG_I("ESP32 SDIO AT device initialized successfully");
    
    return RT_EOK;
    
cleanup:
    if (at_dev->cmd_buffer)
    {
        rt_free(at_dev->cmd_buffer);
        at_dev->cmd_buffer = RT_NULL;
    }
    if (at_dev->resp_buffer)
    {
        rt_free(at_dev->resp_buffer);
        at_dev->resp_buffer = RT_NULL;
    }
    sdio_disable_func(at_dev->func_data);
    sdio_disable_func(at_dev->func_at);
    
    return err;
}

/**
 * @brief Open ESP32 SDIO AT device
 */
static rt_err_t esp32_sdio_at_open(rt_device_t dev, rt_uint16_t oflag)
{
    struct esp32_sdio_at_device *at_dev = (struct esp32_sdio_at_device *)dev;
    
    if (!at_dev->initialized)
    {
        rt_err_t err = esp32_sdio_at_init(dev);
        if (err != RT_EOK)
            return err;
    }
    
    return RT_EOK;
}

/**
 * @brief Close ESP32 SDIO AT device
 */
static rt_err_t esp32_sdio_at_close(rt_device_t dev)
{
    /* Keep device open for continuous operation */
    return RT_EOK;
}

/**
 * @brief Read response from ESP32
 */
static rt_ssize_t esp32_sdio_at_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    struct esp32_sdio_at_device *at_dev = (struct esp32_sdio_at_device *)dev;
    rt_err_t err;
    rt_size_t copy_len;
    
    if (!at_dev->initialized)
        return -RT_ERROR;
    
    /* Wait for response */
    err = rt_sem_take(at_dev->resp_sem, rt_tick_from_millisecond(ESP32_SDIO_CMD_TIMEOUT));
    if (err != RT_EOK)
    {
        LOG_W("No response received within timeout");
        return -RT_ETIMEOUT;
    }
    
    rt_mutex_take(at_dev->lock, RT_WAITING_FOREVER);
    
    copy_len = (at_dev->resp_len < size) ? at_dev->resp_len : size;
    rt_memcpy(buffer, at_dev->resp_buffer, copy_len);
    
    rt_mutex_release(at_dev->lock);
    
    return copy_len;
}

/**
 * @brief Write AT command to ESP32
 */
static rt_ssize_t esp32_sdio_at_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    struct esp32_sdio_at_device *at_dev = (struct esp32_sdio_at_device *)dev;
    rt_err_t err;
    
    if (!at_dev->initialized)
        return -RT_ERROR;
    
    if (size == 0 || size > ESP32_SDIO_BLOCK_SIZE)
        return -RT_EINVAL;
    
    rt_mutex_take(at_dev->lock, RT_WAITING_FOREVER);
    
    err = esp32_sdio_send_at_command(at_dev, (const char *)buffer, size);
    
    rt_mutex_release(at_dev->lock);
    
    return (err == RT_EOK) ? size : err;
}

/**
 * @brief Control ESP32 SDIO AT device
 */
static rt_err_t esp32_sdio_at_control(rt_device_t dev, int cmd, void *args)
{
    struct esp32_sdio_at_device *at_dev = (struct esp32_sdio_at_device *)dev;
    
    switch (cmd)
    {
    case ESP32_SDIO_AT_CMD_RESET:
        /* Reset ESP32 via SDIO */
        return esp32_sdio_write_reg(at_dev->func_at, ESP32_SDIO_REG_CMD_STATUS, 0xFF);
        
    case ESP32_SDIO_AT_CMD_GET_STATUS:
        if (args)
        {
            rt_uint32_t *status = (rt_uint32_t *)args;
            *status = esp32_sdio_read_reg(at_dev->func_at, ESP32_SDIO_REG_CMD_STATUS);
            return RT_EOK;
        }
        return -RT_EINVAL;
        
    default:
        return -RT_EINVAL;
    }
}

/**
 * @brief SDIO probe function for ESP32 AT device
 */
static rt_int32_t esp32_sdio_at_probe(struct rt_sdio_function *func, const struct rt_sdio_device_id *id)
{
    struct esp32_sdio_at_device *at_dev;
    rt_err_t err;
    
    LOG_I("ESP32 SDIO AT device found (func: %d)", func->num);
    
    if (func->num != ESP32_SDIO_FUNC_AT)
        return -RT_ERROR;
    
    /* Allocate device structure */
    at_dev = rt_calloc(1, sizeof(struct esp32_sdio_at_device));
    if (!at_dev)
    {
        LOG_E("Failed to allocate device structure");
        return -RT_ENOMEM;
    }
    
    /* Initialize device structure */
    at_dev->card = func->card;
    at_dev->func_at = func;
    
    /* Find data function */
    at_dev->func_data = sdio_get_func(func->card, ESP32_SDIO_FUNC_DATA);
    if (!at_dev->func_data)
    {
        LOG_E("Data function not found");
        rt_free(at_dev);
        return -RT_ERROR;
    }
    
    /* Create synchronization objects */
    at_dev->lock = rt_mutex_create("esp32_lock", RT_IPC_FLAG_PRIO);
    at_dev->cmd_sem = rt_sem_create("esp32_cmd", 0, RT_IPC_FLAG_PRIO);
    at_dev->resp_sem = rt_sem_create("esp32_resp", 0, RT_IPC_FLAG_PRIO);
    
    if (!at_dev->lock || !at_dev->cmd_sem || !at_dev->resp_sem)
    {
        LOG_E("Failed to create synchronization objects");
        goto cleanup;
    }
    
    /* Initialize RT-Thread device */
    at_dev->device.type = RT_Device_Class_Char;
    at_dev->device.ops = &esp32_sdio_at_ops;
    at_dev->device.user_data = at_dev;
    
    /* Register device */
    err = rt_device_register(&at_dev->device, "esp32_at", RT_DEVICE_FLAG_RDWR);
    if (err != RT_EOK)
    {
        LOG_E("Failed to register device: %d", err);
        goto cleanup;
    }
    
    /* Set function private data */
    sdio_set_drvdata(func, at_dev);
    g_esp32_at_dev = at_dev;
    
    LOG_I("ESP32 SDIO AT device registered successfully");
    return RT_EOK;
    
cleanup:
    if (at_dev->resp_sem) rt_sem_delete(at_dev->resp_sem);
    if (at_dev->cmd_sem) rt_sem_delete(at_dev->cmd_sem);
    if (at_dev->lock) rt_mutex_delete(at_dev->lock);
    rt_free(at_dev);
    return -RT_ERROR;
}

/**
 * @brief SDIO remove function for ESP32 AT device
 */
static void esp32_sdio_at_remove(struct rt_sdio_function *func)
{
    struct esp32_sdio_at_device *at_dev = sdio_get_drvdata(func);
    
    if (!at_dev)
        return;
    
    LOG_I("Removing ESP32 SDIO AT device");
    
    /* Stop response thread */
    if (at_dev->resp_thread)
    {
        rt_thread_delete(at_dev->resp_thread);
        at_dev->resp_thread = RT_NULL;
    }
    
    /* Disable functions */
    if (at_dev->func_data)
        sdio_disable_func(at_dev->func_data);
    if (at_dev->func_at)
        sdio_disable_func(at_dev->func_at);
    
    /* Free buffers */
    if (at_dev->cmd_buffer)
        rt_free(at_dev->cmd_buffer);
    if (at_dev->resp_buffer)
        rt_free(at_dev->resp_buffer);
    
    /* Unregister device */
    rt_device_unregister(&at_dev->device);
    
    /* Clean up synchronization objects */
    if (at_dev->resp_sem) rt_sem_delete(at_dev->resp_sem);
    if (at_dev->cmd_sem) rt_sem_delete(at_dev->cmd_sem);
    if (at_dev->lock) rt_mutex_delete(at_dev->lock);
    
    rt_free(at_dev);
    g_esp32_at_dev = RT_NULL;
    
    LOG_I("ESP32 SDIO AT device removed");
}

/* ESP32 SDIO Device ID Table */
static const struct rt_sdio_device_id esp32_sdio_at_ids[] =
{
    { SDIO_DEVICE(0x6666, 0x1111) }, /* ESP32 AT Command Interface */
    { /* end */ }
};

/* ESP32 SDIO Driver */
static struct rt_sdio_driver esp32_sdio_at_driver =
{
    .name = "esp32_sdio_at",
    .probe = esp32_sdio_at_probe,
    .remove = esp32_sdio_at_remove,
    .id_table = esp32_sdio_at_ids,
};

/**
 * @brief Register ESP32 SDIO AT driver
 */
int rt_hw_esp32_sdio_at_init(void)
{
    rt_err_t err;
    
    err = sdio_register_driver(&esp32_sdio_at_driver);
    if (err != RT_EOK)
    {
        LOG_E("Failed to register ESP32 SDIO AT driver: %d", err);
        return err;
    }
    
    LOG_I("ESP32 SDIO AT driver registered");
    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_esp32_sdio_at_init);

/* Public API Functions */

/**
 * @brief Send AT command and wait for response
 */
rt_err_t esp32_at_send_command(const char *cmd, char *response, rt_size_t resp_size, rt_uint32_t timeout)
{
    rt_device_t dev;
    rt_ssize_t ret;
    
    if (!cmd || !response)
        return -RT_EINVAL;
    
    dev = rt_device_find("esp32_at");
    if (!dev)
    {
        LOG_E("ESP32 AT device not found");
        return -RT_ERROR;
    }
    
    /* Open device if not already open */
    if (!(dev->open_flag & RT_DEVICE_OFLAG_OPEN))
    {
        rt_err_t err = rt_device_open(dev, RT_DEVICE_OFLAG_RDWR);
        if (err != RT_EOK)
        {
            LOG_E("Failed to open ESP32 AT device: %d", err);
            return err;
        }
    }
    
    /* Send command */
    ret = rt_device_write(dev, 0, cmd, rt_strlen(cmd));
    if (ret < 0)
    {
        LOG_E("Failed to send AT command: %d", (int)ret);
        return ret;
    }
    
    /* Read response */
    ret = rt_device_read(dev, 0, response, resp_size - 1);
    if (ret < 0)
    {
        LOG_E("Failed to read AT response: %d", (int)ret);
        return ret;
    }
    
    /* Null terminate response */
    response[ret] = '\0';
    
    return RT_EOK;
}