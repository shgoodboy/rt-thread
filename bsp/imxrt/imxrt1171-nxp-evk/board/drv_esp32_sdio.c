/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-12-XX     Assistant    ESP32 SDIO AT command driver for IMXRT1171
 */

#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>

#ifdef BSP_USING_ESP32_SDIO

#include "fsl_usdhc.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "board.h"

#define ESP32_SDIO_DEBUG
#ifdef ESP32_SDIO_DEBUG
#define ESP32_DBG(fmt, ...)  rt_kprintf("[ESP32_SDIO] " fmt "\n", ##__VA_ARGS__)
#else
#define ESP32_DBG(fmt, ...)
#endif

/* ESP32 SDIO Function definitions */
#define ESP32_SDIO_FUNC_0       0   /* Function 0 - Configuration */
#define ESP32_SDIO_FUNC_1       1   /* Function 1 - Data transfer */

/* ESP32 SDIO Registers */
#define ESP32_SDIO_REG_FUNC0_INT_STATUS     0x05
#define ESP32_SDIO_REG_FUNC1_INT_STATUS     0x08
#define ESP32_SDIO_REG_FUNC1_INT_ENABLE     0x09
#define ESP32_SDIO_REG_FUNC1_BLOCK_SIZE     0x10

/* ESP32 SDIO Commands */
#define ESP32_SDIO_CMD_AT_WRITE             0x01
#define ESP32_SDIO_CMD_AT_READ              0x02
#define ESP32_SDIO_CMD_STATUS               0x03

/* Buffer sizes */
#define ESP32_SDIO_BLOCK_SIZE               512
#define ESP32_SDIO_MAX_AT_CMD_LEN           256
#define ESP32_SDIO_MAX_AT_RESP_LEN          1024

/* ESP32 Control pins */
#define ESP32_RESET_PIN                     23  /* GPIO9_IO23 */
#define ESP32_ENABLE_PIN                    24  /* GPIO9_IO24 */

struct esp32_sdio_device
{
    struct rt_device parent;
    USDHC_Type *usdhc_base;
    struct rt_mutex lock;
    struct rt_semaphore tx_sem;
    struct rt_semaphore rx_sem;
    
    rt_uint8_t *tx_buffer;
    rt_uint8_t *rx_buffer;
    rt_size_t tx_len;
    rt_size_t rx_len;
    
    rt_bool_t initialized;
};

static struct esp32_sdio_device esp32_sdio_dev;

/* USDHC configuration for ESP32 SDIO */
static usdhc_config_t esp32_usdhc_config = {
    .dataTimeout = 0xFU,
    .endianMode = kUSDHC_EndianModeLittle,
    .readWatermarkLevel = 0x80U,
    .writeWatermarkLevel = 0x80U,
};

/* ESP32 Hardware Reset */
static void esp32_hw_reset(void)
{
    ESP32_DBG("ESP32 hardware reset");
    
    /* Reset ESP32 */
    GPIO_PinWrite(GPIO9, ESP32_RESET_PIN, 0);
    rt_thread_mdelay(10);
    GPIO_PinWrite(GPIO9, ESP32_RESET_PIN, 1);
    rt_thread_mdelay(100);
}

/* ESP32 Hardware Enable */
static void esp32_hw_enable(rt_bool_t enable)
{
    ESP32_DBG("ESP32 hardware %s", enable ? "enable" : "disable");
    GPIO_PinWrite(GPIO9, ESP32_ENABLE_PIN, enable ? 1 : 0);
    if (enable) {
        rt_thread_mdelay(50);
    }
}

/* SDIO Command execution */
static rt_err_t esp32_sdio_cmd(rt_uint8_t func, rt_uint32_t addr, rt_uint8_t *data, rt_size_t len, rt_bool_t write)
{
    usdhc_command_t command = {0};
    usdhc_data_t data_config = {0};
    status_t status;
    
    /* Setup command */
    command.index = write ? 53 : 52;  /* CMD53 for block I/O, CMD52 for byte I/O */
    command.argument = (func << 28) | (write ? (1 << 31) : 0) | (addr << 9) | len;
    command.responseType = kCARD_ResponseTypeR5;
    
    if (len > 1) {
        /* Block mode transfer */
        data_config.blockSize = ESP32_SDIO_BLOCK_SIZE;
        data_config.blockCount = (len + ESP32_SDIO_BLOCK_SIZE - 1) / ESP32_SDIO_BLOCK_SIZE;
        data_config.enableAutoCommand12 = false;
        data_config.enableAutoCommand23 = false;
        data_config.enableIgnoreError = false;
        data_config.dataType = kUSDHC_TransferDataNormal;
        
        if (write) {
            data_config.txData = (const rt_uint32_t *)data;
            data_config.rxData = RT_NULL;
        } else {
            data_config.txData = RT_NULL;
            data_config.rxData = (rt_uint32_t *)data;
        }
        
        usdhc_transfer_t transfer = {
            .command = &command,
            .data = &data_config
        };
        
        status = USDHC_TransferBlocking(esp32_sdio_dev.usdhc_base, RT_NULL, &transfer);
    } else {
        /* Byte mode transfer */
        usdhc_transfer_t transfer = {
            .command = &command,
            .data = RT_NULL
        };
        
        status = USDHC_TransferBlocking(esp32_sdio_dev.usdhc_base, RT_NULL, &transfer);
        
        if (!write && status == kStatus_Success) {
            *data = (rt_uint8_t)(command.response[0] & 0xFF);
        }
    }
    
    return (status == kStatus_Success) ? RT_EOK : -RT_ERROR;
}

/* Send AT command to ESP32 */
static rt_err_t esp32_send_at_command(const char *cmd, char *response, rt_size_t resp_len)
{
    rt_err_t ret = RT_EOK;
    rt_size_t cmd_len = rt_strlen(cmd);
    
    if (cmd_len > ESP32_SDIO_MAX_AT_CMD_LEN || resp_len > ESP32_SDIO_MAX_AT_RESP_LEN) {
        return -RT_EINVAL;
    }
    
    rt_mutex_take(&esp32_sdio_dev.lock, RT_WAITING_FOREVER);
    
    ESP32_DBG("Sending AT command: %s", cmd);
    
    /* Clear buffers */
    rt_memset(esp32_sdio_dev.tx_buffer, 0, ESP32_SDIO_BLOCK_SIZE);
    rt_memset(esp32_sdio_dev.rx_buffer, 0, ESP32_SDIO_BLOCK_SIZE);
    
    /* Copy command to tx buffer */
    rt_strncpy((char *)esp32_sdio_dev.tx_buffer, cmd, cmd_len);
    esp32_sdio_dev.tx_buffer[cmd_len] = '\r';
    esp32_sdio_dev.tx_buffer[cmd_len + 1] = '\n';
    esp32_sdio_dev.tx_len = cmd_len + 2;
    
    /* Send command via SDIO */
    ret = esp32_sdio_cmd(ESP32_SDIO_FUNC_1, 0x00, esp32_sdio_dev.tx_buffer, ESP32_SDIO_BLOCK_SIZE, RT_TRUE);
    if (ret != RT_EOK) {
        ESP32_DBG("Failed to send AT command");
        goto exit;
    }
    
    /* Wait for response */
    rt_thread_mdelay(100);  /* Give ESP32 time to process */
    
    /* Read response via SDIO */
    ret = esp32_sdio_cmd(ESP32_SDIO_FUNC_1, 0x00, esp32_sdio_dev.rx_buffer, ESP32_SDIO_BLOCK_SIZE, RT_FALSE);
    if (ret != RT_EOK) {
        ESP32_DBG("Failed to read AT response");
        goto exit;
    }
    
    /* Copy response */
    rt_size_t copy_len = rt_strlen((char *)esp32_sdio_dev.rx_buffer);
    if (copy_len > resp_len - 1) {
        copy_len = resp_len - 1;
    }
    rt_strncpy(response, (char *)esp32_sdio_dev.rx_buffer, copy_len);
    response[copy_len] = '\0';
    
    ESP32_DBG("Received AT response: %s", response);
    
exit:
    rt_mutex_release(&esp32_sdio_dev.lock);
    return ret;
}

/* Device open */
static rt_err_t esp32_sdio_open(rt_device_t dev, rt_uint16_t oflag)
{
    struct esp32_sdio_device *device = (struct esp32_sdio_device *)dev;
    
    if (device->initialized) {
        return RT_EOK;
    }
    
    ESP32_DBG("Opening ESP32 SDIO device");
    
    /* Enable ESP32 */
    esp32_hw_enable(RT_TRUE);
    
    /* Reset ESP32 */
    esp32_hw_reset();
    
    /* Initialize USDHC for SDIO mode */
    USDHC_Init(device->usdhc_base, &esp32_usdhc_config);
    
    /* Set SDIO clock to 25MHz */
    USDHC_SetSdClock(device->usdhc_base, CLOCK_GetRootClockFreq(kCLOCK_Root_Usdhc2), 25000000U);
    
    device->initialized = RT_TRUE;
    
    ESP32_DBG("ESP32 SDIO device opened successfully");
    return RT_EOK;
}

/* Device close */
static rt_err_t esp32_sdio_close(rt_device_t dev)
{
    struct esp32_sdio_device *device = (struct esp32_sdio_device *)dev;
    
    ESP32_DBG("Closing ESP32 SDIO device");
    
    /* Disable ESP32 */
    esp32_hw_enable(RT_FALSE);
    
    /* Deinitialize USDHC */
    USDHC_Deinit(device->usdhc_base);
    
    device->initialized = RT_FALSE;
    
    return RT_EOK;
}

/* Device control */
static rt_err_t esp32_sdio_control(rt_device_t dev, int cmd, void *args)
{
    struct esp32_sdio_device *device = (struct esp32_sdio_device *)dev;
    rt_err_t ret = RT_EOK;
    
    switch (cmd) {
        case RT_DEVICE_CTRL_CUSTOM + 1: /* Send AT command */
        {
            struct {
                const char *cmd;
                char *response;
                rt_size_t resp_len;
            } *at_cmd = args;
            
            if (at_cmd) {
                ret = esp32_send_at_command(at_cmd->cmd, at_cmd->response, at_cmd->resp_len);
            } else {
                ret = -RT_EINVAL;
            }
            break;
        }
        default:
            ret = -RT_EINVAL;
            break;
    }
    
    return ret;
}

/* Device read */
static rt_size_t esp32_sdio_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    /* Not implemented for AT command interface */
    return 0;
}

/* Device write */
static rt_size_t esp32_sdio_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    /* Not implemented for AT command interface */
    return 0;
}

/* Initialize ESP32 SDIO device */
int rt_hw_esp32_sdio_init(void)
{
    rt_err_t ret = RT_EOK;
    
    ESP32_DBG("Initializing ESP32 SDIO driver");
    
    /* Initialize device structure */
    rt_memset(&esp32_sdio_dev, 0, sizeof(esp32_sdio_dev));
    
    /* Set USDHC base address */
    esp32_sdio_dev.usdhc_base = USDHC2;
    
    /* Allocate buffers */
    esp32_sdio_dev.tx_buffer = rt_malloc_align(ESP32_SDIO_BLOCK_SIZE, 32);
    esp32_sdio_dev.rx_buffer = rt_malloc_align(ESP32_SDIO_BLOCK_SIZE, 32);
    
    if (!esp32_sdio_dev.tx_buffer || !esp32_sdio_dev.rx_buffer) {
        ESP32_DBG("Failed to allocate SDIO buffers");
        ret = -RT_ENOMEM;
        goto error;
    }
    
    /* Initialize synchronization objects */
    rt_mutex_init(&esp32_sdio_dev.lock, "esp32_sdio", RT_IPC_FLAG_PRIO);
    rt_sem_init(&esp32_sdio_dev.tx_sem, "esp32_tx", 0, RT_IPC_FLAG_PRIO);
    rt_sem_init(&esp32_sdio_dev.rx_sem, "esp32_rx", 0, RT_IPC_FLAG_PRIO);
    
    /* Register device */
    esp32_sdio_dev.parent.type = RT_Device_Class_Miscellaneous;
    esp32_sdio_dev.parent.init = RT_NULL;
    esp32_sdio_dev.parent.open = esp32_sdio_open;
    esp32_sdio_dev.parent.close = esp32_sdio_close;
    esp32_sdio_dev.parent.read = esp32_sdio_read;
    esp32_sdio_dev.parent.write = esp32_sdio_write;
    esp32_sdio_dev.parent.control = esp32_sdio_control;
    esp32_sdio_dev.parent.user_data = RT_NULL;
    
    ret = rt_device_register(&esp32_sdio_dev.parent, "esp32_sdio", RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK) {
        ESP32_DBG("Failed to register ESP32 SDIO device");
        goto error;
    }
    
    ESP32_DBG("ESP32 SDIO driver initialized successfully");
    return RT_EOK;
    
error:
    if (esp32_sdio_dev.tx_buffer) {
        rt_free_align(esp32_sdio_dev.tx_buffer);
    }
    if (esp32_sdio_dev.rx_buffer) {
        rt_free_align(esp32_sdio_dev.rx_buffer);
    }
    return ret;
}

INIT_DEVICE_EXPORT(rt_hw_esp32_sdio_init);

#endif /* BSP_USING_ESP32_SDIO */