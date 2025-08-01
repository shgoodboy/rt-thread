/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT Driver for i.MX RT1170
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/dev_mmcsd_core.h>
#include <drivers/dev_sdio.h>

#include <board.h>
#include <fsl_usdhc.h>
#include <fsl_gpio.h>
#include <fsl_iomuxc.h>
#include <fsl_clock.h>

#include "drv_esp32_sdio_at_imxrt.h"

#define DBG_TAG               "ESP32_SDIO_IMXRT"
#define DBG_LVL               DBG_INFO
#include <rtdbg.h>

#ifdef BSP_USING_ESP32_SDIO_AT

/* i.MX RT1170-specific configuration */
#define IMXRT_ESP32_SDIO_USDHC_BASE     USDHC2_BASE
#define IMXRT_ESP32_SDIO_USDHC_IRQ      USDHC2_IRQn
#define IMXRT_ESP32_SDIO_CLOCK_NAME     kCLOCK_Usdhc2

/* ESP32 SDIO Configuration for i.MX RT1170 */
#define ESP32_SDIO_BLOCK_SIZE           512
#define ESP32_SDIO_MAX_FREQ             (25000000U)  /* 25MHz */
#define ESP32_SDIO_TIMEOUT              (5000)       /* 5 seconds */

/* USDHC ADMA2 Configuration */
#define USDHC_ADMA_TABLE_WORDS          (32U)
#define USDHC_ADMA2_ADDR_ALIGN          (4U)

/* ESP32 SDIO Device Structure for i.MX RT1170 */
struct imxrt_esp32_sdio_device
{
    struct rt_device device;
    struct rt_mmcsd_card *card;
    struct rt_sdio_function *func_at;
    struct rt_sdio_function *func_data;
    
    /* i.MX RT1170 USDHC specific */
    USDHC_Type *usdhc_base;
    usdhc_host_t usdhc_host;
    usdhc_card_t usdhc_card;
    
    /* Synchronization */
    rt_mutex_t lock;
    rt_sem_t cmd_sem;
    rt_sem_t resp_sem;
    rt_event_t event;
    
    /* Buffers */
    rt_uint8_t *cmd_buffer;
    rt_uint8_t *resp_buffer;
    rt_uint32_t cmd_len;
    rt_uint32_t resp_len;
    
    /* Status */
    rt_bool_t initialized;
    rt_bool_t card_inserted;
    
    /* Response handling thread */
    rt_thread_t resp_thread;
};

/* ADMA2 descriptor table */
AT_NONCACHEABLE_SECTION_ALIGN(uint32_t g_esp32_adma2_table[USDHC_ADMA_TABLE_WORDS], USDHC_ADMA2_ADDR_ALIGN);

static struct imxrt_esp32_sdio_device *g_esp32_sdio_dev = RT_NULL;

/* Function prototypes */
static rt_err_t imxrt_esp32_sdio_init(rt_device_t dev);
static rt_err_t imxrt_esp32_sdio_open(rt_device_t dev, rt_uint16_t oflag);
static rt_err_t imxrt_esp32_sdio_close(rt_device_t dev);
static rt_ssize_t imxrt_esp32_sdio_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
static rt_ssize_t imxrt_esp32_sdio_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
static rt_err_t imxrt_esp32_sdio_control(rt_device_t dev, int cmd, void *args);

/* Device operations */
static const struct rt_device_ops imxrt_esp32_sdio_ops =
{
    imxrt_esp32_sdio_init,
    imxrt_esp32_sdio_open,
    imxrt_esp32_sdio_close,
    imxrt_esp32_sdio_read,
    imxrt_esp32_sdio_write,
    imxrt_esp32_sdio_control
};

/**
 * @brief Configure i.MX RT1170 pins for ESP32 SDIO interface
 */
static void imxrt_esp32_sdio_pin_config(void)
{
    /* Configure USDHC2 pins for ESP32 SDIO interface */
    
    /* USDHC2_CLK - GPIO_SD_B2_05 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_05_USDHC2_CLK, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_05_USDHC2_CLK,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    /* USDHC2_CMD - GPIO_SD_B2_04 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_04_USDHC2_CMD, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_04_USDHC2_CMD,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    /* USDHC2_DATA0 - GPIO_SD_B2_08 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_08_USDHC2_DATA0, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_08_USDHC2_DATA0,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    /* USDHC2_DATA1 - GPIO_SD_B2_09 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_09_USDHC2_DATA1, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_09_USDHC2_DATA1,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    /* USDHC2_DATA2 - GPIO_SD_B2_10 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_10_USDHC2_DATA2, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_10_USDHC2_DATA2,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    /* USDHC2_DATA3 - GPIO_SD_B2_11 */
    IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B2_11_USDHC2_DATA3, 0U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B2_11_USDHC2_DATA3,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(2U) |
                        IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK |
                        IOMUXC_SW_PAD_CTL_PAD_PUS(1U) |
                        IOMUXC_SW_PAD_CTL_PAD_DSE(1U));

    LOG_D("i.MX RT1170 ESP32 SDIO pins configured");
}

/**
 * @brief Initialize USDHC host for ESP32 SDIO communication
 */
static rt_err_t imxrt_esp32_usdhc_init(struct imxrt_esp32_sdio_device *esp32_dev)
{
    usdhc_host_t *host = &esp32_dev->usdhc_host;
    status_t status;
    
    /* Enable USDHC2 clock */
    CLOCK_EnableClock(IMXRT_ESP32_SDIO_CLOCK_NAME);
    
    /* Configure USDHC host */
    host->base = (USDHC_Type *)IMXRT_ESP32_SDIO_USDHC_BASE;
    host->sourceClock_Hz = CLOCK_GetFreq(kCLOCK_SysPll2Pfd2);
    
    /* Initialize USDHC host */
    usdhc_host_config_t host_config = {0};
    USDHC_GetDefaultHostConfig(&host_config);
    
    host_config.dataTimeout = 0xFU;
    host_config.endianMode = kUSDHC_EndianModeLittle;
    host_config.dmaMode = kUSDHC_DmaModeAdma2;
    host_config.readWatermarkLevel = 0x80U;
    host_config.writeWatermarkLevel = 0x80U;
    host_config.readBurstLen = 8U;
    host_config.writeBurstLen = 8U;
    
    status = USDHC_HostInit(host->base, &host_config);
    if (status != kStatus_Success)
    {
        LOG_E("USDHC host initialization failed: %d", status);
        return -RT_ERROR;
    }
    
    /* Set ADMA2 descriptor table */
    esp32_dev->usdhc_host.dmaDesBuffer = g_esp32_adma2_table;
    esp32_dev->usdhc_host.dmaDesBufferWordsNum = USDHC_ADMA_TABLE_WORDS;
    
    LOG_I("USDHC host initialized for ESP32 SDIO");
    return RT_EOK;
}

/**
 * @brief ESP32 SDIO card detection and initialization
 */
static rt_err_t imxrt_esp32_card_init(struct imxrt_esp32_sdio_device *esp32_dev)
{
    usdhc_card_t *card = &esp32_dev->usdhc_card;
    status_t status;
    
    /* Initialize card structure */
    rt_memset(card, 0, sizeof(usdhc_card_t));
    card->host = &esp32_dev->usdhc_host;
    card->usrParam.cd = NULL;  /* No card detect for ESP32 */
    card->usrParam.pwr = NULL; /* No power control */
    
    /* Initialize SDIO card */
    status = SDIO_CardInit(card);
    if (status != kStatus_Success)
    {
        LOG_E("ESP32 SDIO card initialization failed: %d", status);
        return -RT_ERROR;
    }
    
    LOG_I("ESP32 SDIO card initialized");
    LOG_I("  RCA: 0x%04X", card->relativeAddress);
    LOG_I("  Clock: %d Hz", card->busClock_Hz);
    LOG_I("  Bus Width: %d", card->busWidth);
    
    esp32_dev->card_inserted = RT_TRUE;
    return RT_EOK;
}

/**
 * @brief Send SDIO command to ESP32
 */
static rt_err_t imxrt_esp32_send_sdio_cmd(struct imxrt_esp32_sdio_device *esp32_dev,
                                          rt_uint32_t func_num, rt_uint32_t reg_addr,
                                          rt_uint32_t arg, rt_bool_t write_flag,
                                          rt_uint8_t *data, rt_uint32_t data_len)
{
    usdhc_command_t command = {0};
    usdhc_data_t data_config = {0};
    status_t status;
    
    /* Configure SDIO command */
    if (data && data_len > 0)
    {
        /* CMD53 - Extended I/O R/W */
        command.index = kSDIO_IOReadWriteExtended;
        command.argument = SDIO_IO_RW_EXTENDED_ARG(write_flag ? 1 : 0, func_num, reg_addr, 
                                                   1, /* increment address */
                                                   data_len);
        command.type = kCARD_CommandTypeNormal;
        command.responseType = kCARD_ResponseTypeR5;
        
        /* Configure data */
        data_config.enableAutoCommand12 = false;
        data_config.enableAutoCommand23 = false;
        data_config.enableIgnoreError = false;
        data_config.dataType = kUSDHC_TransferDataNormal;
        data_config.blockSize = ESP32_SDIO_BLOCK_SIZE;
        data_config.blockCount = (data_len + ESP32_SDIO_BLOCK_SIZE - 1) / ESP32_SDIO_BLOCK_SIZE;
        data_config.rxData = write_flag ? NULL : (rt_uint32_t *)data;
        data_config.txData = write_flag ? (rt_uint32_t *)data : NULL;
        
        command.data = &data_config;
    }
    else
    {
        /* CMD52 - Direct I/O R/W */
        command.index = kSDIO_IOReadWriteDirect;
        command.argument = SDIO_IO_RW_DIRECT_ARG(write_flag ? 1 : 0, func_num, reg_addr, 
                                                 write_flag ? (arg & 0xFF) : 0);
        command.type = kCARD_CommandTypeNormal;
        command.responseType = kCARD_ResponseTypeR5;
        command.data = NULL;
    }
    
    /* Send command */
    status = USDHC_SendCommand(esp32_dev->usdhc_host.base, &command);
    if (status != kStatus_Success)
    {
        LOG_E("SDIO command failed: %d", status);
        return -RT_ERROR;
    }
    
    /* Check response */
    if (command.response[0] & 0xFF00)
    {
        LOG_E("SDIO command error response: 0x%08X", command.response[0]);
        return -RT_ERROR;
    }
    
    return RT_EOK;
}

/**
 * @brief Write data to ESP32 via SDIO
 */
static rt_err_t imxrt_esp32_sdio_write_data(struct imxrt_esp32_sdio_device *esp32_dev,
                                            rt_uint32_t func_num, rt_uint32_t reg_addr,
                                            const rt_uint8_t *data, rt_uint32_t data_len)
{
    rt_err_t err;
    rt_uint8_t *aligned_buffer;
    
    /* Ensure data is cache-aligned for DMA */
    aligned_buffer = rt_malloc_align(data_len, 32);
    if (!aligned_buffer)
    {
        LOG_E("Failed to allocate aligned buffer");
        return -RT_ENOMEM;
    }
    
    rt_memcpy(aligned_buffer, data, data_len);
    
    /* Clean cache before DMA */
    SCB_CleanDCache_by_Addr((uint32_t *)aligned_buffer, data_len);
    
    err = imxrt_esp32_send_sdio_cmd(esp32_dev, func_num, reg_addr, 0, RT_TRUE, 
                                    aligned_buffer, data_len);
    
    rt_free_align(aligned_buffer);
    return err;
}

/**
 * @brief Read data from ESP32 via SDIO
 */
static rt_err_t imxrt_esp32_sdio_read_data(struct imxrt_esp32_sdio_device *esp32_dev,
                                           rt_uint32_t func_num, rt_uint32_t reg_addr,
                                           rt_uint8_t *data, rt_uint32_t data_len)
{
    rt_err_t err;
    rt_uint8_t *aligned_buffer;
    
    /* Ensure data is cache-aligned for DMA */
    aligned_buffer = rt_malloc_align(data_len, 32);
    if (!aligned_buffer)
    {
        LOG_E("Failed to allocate aligned buffer");
        return -RT_ENOMEM;
    }
    
    /* Invalidate cache before DMA */
    SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_buffer, data_len);
    
    err = imxrt_esp32_send_sdio_cmd(esp32_dev, func_num, reg_addr, 0, RT_FALSE, 
                                    aligned_buffer, data_len);
    
    if (err == RT_EOK)
    {
        /* Invalidate cache after DMA */
        SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_buffer, data_len);
        rt_memcpy(data, aligned_buffer, data_len);
    }
    
    rt_free_align(aligned_buffer);
    return err;
}

/**
 * @brief Response handling thread for ESP32 SDIO
 */
static void imxrt_esp32_resp_thread_entry(void *parameter)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)parameter;
    rt_uint32_t event;
    rt_err_t err;
    
    LOG_D("ESP32 SDIO response thread started");
    
    while (1)
    {
        /* Wait for response event */
        err = rt_event_recv(esp32_dev->event, ESP32_SDIO_EVENT_RESP_READY,
                            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                            RT_WAITING_FOREVER, &event);
        
        if (err == RT_EOK && (event & ESP32_SDIO_EVENT_RESP_READY))
        {
            /* Read response data from ESP32 */
            err = imxrt_esp32_sdio_read_data(esp32_dev, ESP32_SDIO_FUNC_AT,
                                             ESP32_SDIO_REG_RESP_DATA,
                                             esp32_dev->resp_buffer,
                                             ESP32_SDIO_MAX_RESPONSE);
            
            if (err == RT_EOK)
            {
                /* Signal response ready */
                rt_sem_release(esp32_dev->resp_sem);
            }
        }
        
        rt_thread_mdelay(10);
    }
}

/**
 * @brief Initialize ESP32 SDIO device
 */
static rt_err_t imxrt_esp32_sdio_init(rt_device_t dev)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)dev;
    rt_err_t err;
    
    if (esp32_dev->initialized)
        return RT_EOK;
    
    LOG_I("Initializing ESP32 SDIO on i.MX RT1170");
    
    /* Configure pins */
    imxrt_esp32_sdio_pin_config();
    
    /* Initialize USDHC host */
    err = imxrt_esp32_usdhc_init(esp32_dev);
    if (err != RT_EOK)
    {
        LOG_E("USDHC initialization failed");
        return err;
    }
    
    /* Initialize ESP32 SDIO card */
    err = imxrt_esp32_card_init(esp32_dev);
    if (err != RT_EOK)
    {
        LOG_E("ESP32 SDIO card initialization failed");
        return err;
    }
    
    /* Allocate buffers */
    esp32_dev->cmd_buffer = rt_malloc_align(ESP32_SDIO_BLOCK_SIZE, 32);
    esp32_dev->resp_buffer = rt_malloc_align(ESP32_SDIO_MAX_RESPONSE, 32);
    
    if (!esp32_dev->cmd_buffer || !esp32_dev->resp_buffer)
    {
        LOG_E("Failed to allocate SDIO buffers");
        if (esp32_dev->cmd_buffer) rt_free_align(esp32_dev->cmd_buffer);
        if (esp32_dev->resp_buffer) rt_free_align(esp32_dev->resp_buffer);
        return -RT_ENOMEM;
    }
    
    /* Create response handling thread */
    esp32_dev->resp_thread = rt_thread_create("esp32_resp",
                                              imxrt_esp32_resp_thread_entry,
                                              esp32_dev,
                                              2048,
                                              20,
                                              20);
    
    if (esp32_dev->resp_thread == RT_NULL)
    {
        LOG_E("Failed to create response thread");
        rt_free_align(esp32_dev->cmd_buffer);
        rt_free_align(esp32_dev->resp_buffer);
        return -RT_ERROR;
    }
    
    rt_thread_startup(esp32_dev->resp_thread);
    
    esp32_dev->initialized = RT_TRUE;
    LOG_I("ESP32 SDIO device initialized successfully");
    
    return RT_EOK;
}

/**
 * @brief Open ESP32 SDIO device
 */
static rt_err_t imxrt_esp32_sdio_open(rt_device_t dev, rt_uint16_t oflag)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)dev;
    
    if (!esp32_dev->initialized)
    {
        rt_err_t err = imxrt_esp32_sdio_init(dev);
        if (err != RT_EOK)
            return err;
    }
    
    return RT_EOK;
}

/**
 * @brief Close ESP32 SDIO device
 */
static rt_err_t imxrt_esp32_sdio_close(rt_device_t dev)
{
    return RT_EOK;
}

/**
 * @brief Read from ESP32 SDIO device
 */
static rt_ssize_t imxrt_esp32_sdio_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)dev;
    rt_err_t err;
    rt_size_t copy_len;
    
    if (!esp32_dev->initialized)
        return -RT_ERROR;
    
    /* Wait for response */
    err = rt_sem_take(esp32_dev->resp_sem, rt_tick_from_millisecond(ESP32_SDIO_TIMEOUT));
    if (err != RT_EOK)
    {
        LOG_W("No response received within timeout");
        return -RT_ETIMEOUT;
    }
    
    rt_mutex_take(esp32_dev->lock, RT_WAITING_FOREVER);
    
    copy_len = (esp32_dev->resp_len < size) ? esp32_dev->resp_len : size;
    rt_memcpy(buffer, esp32_dev->resp_buffer, copy_len);
    
    rt_mutex_release(esp32_dev->lock);
    
    return copy_len;
}

/**
 * @brief Write to ESP32 SDIO device
 */
static rt_ssize_t imxrt_esp32_sdio_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)dev;
    rt_err_t err;
    
    if (!esp32_dev->initialized)
        return -RT_ERROR;
    
    if (size == 0 || size > ESP32_SDIO_BLOCK_SIZE)
        return -RT_EINVAL;
    
    rt_mutex_take(esp32_dev->lock, RT_WAITING_FOREVER);
    
    /* Copy command to buffer */
    rt_memset(esp32_dev->cmd_buffer, 0, ESP32_SDIO_BLOCK_SIZE);
    rt_memcpy(esp32_dev->cmd_buffer, buffer, size);
    esp32_dev->cmd_len = size;
    
    /* Send command to ESP32 */
    err = imxrt_esp32_sdio_write_data(esp32_dev, ESP32_SDIO_FUNC_AT,
                                      ESP32_SDIO_REG_CMD_DATA,
                                      esp32_dev->cmd_buffer, ESP32_SDIO_BLOCK_SIZE);
    
    rt_mutex_release(esp32_dev->lock);
    
    if (err == RT_EOK)
    {
        /* Trigger response event */
        rt_event_send(esp32_dev->event, ESP32_SDIO_EVENT_RESP_READY);
        return size;
    }
    
    return err;
}

/**
 * @brief Control ESP32 SDIO device
 */
static rt_err_t imxrt_esp32_sdio_control(rt_device_t dev, int cmd, void *args)
{
    struct imxrt_esp32_sdio_device *esp32_dev = (struct imxrt_esp32_sdio_device *)dev;
    
    switch (cmd)
    {
    case ESP32_SDIO_AT_CMD_RESET:
        /* Reset ESP32 via SDIO */
        return imxrt_esp32_send_sdio_cmd(esp32_dev, ESP32_SDIO_FUNC_AT,
                                         ESP32_SDIO_REG_CMD_STATUS, 0xFF, RT_TRUE,
                                         NULL, 0);
        
    case ESP32_SDIO_AT_CMD_GET_STATUS:
        if (args)
        {
            rt_uint32_t *status = (rt_uint32_t *)args;
            rt_uint8_t status_byte;
            rt_err_t err = imxrt_esp32_send_sdio_cmd(esp32_dev, ESP32_SDIO_FUNC_AT,
                                                     ESP32_SDIO_REG_CMD_STATUS, 0, RT_FALSE,
                                                     &status_byte, 1);
            if (err == RT_EOK)
                *status = status_byte;
            return err;
        }
        return -RT_EINVAL;
        
    default:
        return -RT_EINVAL;
    }
}

/**
 * @brief Initialize i.MX RT1170 ESP32 SDIO AT driver
 */
int rt_hw_imxrt_esp32_sdio_at_init(void)
{
    struct imxrt_esp32_sdio_device *esp32_dev;
    rt_err_t err;
    
    LOG_I("Initializing i.MX RT1170 ESP32 SDIO AT driver");
    
    /* Allocate device structure */
    esp32_dev = rt_calloc(1, sizeof(struct imxrt_esp32_sdio_device));
    if (!esp32_dev)
    {
        LOG_E("Failed to allocate device structure");
        return -RT_ENOMEM;
    }
    
    /* Initialize device structure */
    esp32_dev->usdhc_base = (USDHC_Type *)IMXRT_ESP32_SDIO_USDHC_BASE;
    
    /* Create synchronization objects */
    esp32_dev->lock = rt_mutex_create("esp32_lock", RT_IPC_FLAG_PRIO);
    esp32_dev->cmd_sem = rt_sem_create("esp32_cmd", 0, RT_IPC_FLAG_PRIO);
    esp32_dev->resp_sem = rt_sem_create("esp32_resp", 0, RT_IPC_FLAG_PRIO);
    esp32_dev->event = rt_event_create("esp32_event", RT_IPC_FLAG_PRIO);
    
    if (!esp32_dev->lock || !esp32_dev->cmd_sem || !esp32_dev->resp_sem || !esp32_dev->event)
    {
        LOG_E("Failed to create synchronization objects");
        goto cleanup;
    }
    
    /* Initialize RT-Thread device */
    esp32_dev->device.type = RT_Device_Class_Char;
    esp32_dev->device.ops = &imxrt_esp32_sdio_ops;
    esp32_dev->device.user_data = esp32_dev;
    
    /* Register device */
    err = rt_device_register(&esp32_dev->device, "esp32_at", RT_DEVICE_FLAG_RDWR);
    if (err != RT_EOK)
    {
        LOG_E("Failed to register device: %d", err);
        goto cleanup;
    }
    
    g_esp32_sdio_dev = esp32_dev;
    
    LOG_I("i.MX RT1170 ESP32 SDIO AT driver initialized");
    return RT_EOK;
    
cleanup:
    if (esp32_dev->event) rt_event_delete(esp32_dev->event);
    if (esp32_dev->resp_sem) rt_sem_delete(esp32_dev->resp_sem);
    if (esp32_dev->cmd_sem) rt_sem_delete(esp32_dev->cmd_sem);
    if (esp32_dev->lock) rt_mutex_delete(esp32_dev->lock);
    rt_free(esp32_dev);
    return -RT_ERROR;
}

INIT_DEVICE_EXPORT(rt_hw_imxrt_esp32_sdio_at_init);

#endif /* BSP_USING_ESP32_SDIO_AT */