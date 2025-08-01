/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-XX     Assistant    ESP32 SDIO probe and registration
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/dev_sdio.h>
#include <drivers/mmcsd_card.h>
#include "drv_esp32_sdio_at.h"

#define DBG_TAG               "ESP32_PROBE"
#ifdef RT_ESP32_SDIO_AT_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif
#include <rtdbg.h>

/* ESP32 SDIO device identification */
#define ESP32_SDIO_VENDOR_ID    0x6666  /* ESP32 vendor ID (example) */
#define ESP32_SDIO_DEVICE_ID    0x1111  /* ESP32 device ID (example) */

/* ESP32 SDIO driver structure */
static struct rt_sdio_driver esp32_sdio_driver;

/**
 * @brief Probe function called when ESP32 SDIO card is detected
 * @param card SDIO card pointer
 * @return RT_EOK on success, error code on failure
 */
static rt_int32_t esp32_sdio_probe(struct rt_mmcsd_card *card)
{
    rt_err_t ret;
    
    if (!card)
    {
        LOG_E("Invalid card parameter");
        return -RT_EINVAL;
    }
    
    LOG_I("ESP32 SDIO card detected");
    LOG_I("Card type: %d", card->card_type);
    LOG_I("Card capacity: %d KB", card->card_capacity);
    LOG_I("Card block size: %d", card->card_blksize);
    
    /* Initialize ESP32 SDIO AT driver */
    ret = esp32_sdio_at_init(card);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to initialize ESP32 SDIO AT driver: %d", ret);
        return ret;
    }
    
    LOG_I("ESP32 SDIO AT driver initialized successfully");
    
#ifdef RT_ESP32_SDIO_AT_EXAMPLE
    /* Auto-start example if enabled */
    extern rt_err_t esp32_sdio_at_example_start(void);
    rt_thread_mdelay(1000); /* Give driver time to stabilize */
    esp32_sdio_at_example_start();
#endif
    
    return RT_EOK;
}

/**
 * @brief Remove function called when ESP32 SDIO card is removed
 * @param card SDIO card pointer
 * @return RT_EOK on success, error code on failure
 */
static rt_int32_t esp32_sdio_remove(struct rt_mmcsd_card *card)
{
    LOG_I("ESP32 SDIO card removed");
    
#ifdef RT_ESP32_SDIO_AT_EXAMPLE
    /* Stop example if running */
    extern rt_err_t esp32_sdio_at_example_stop(void);
    esp32_sdio_at_example_stop();
#endif
    
    /* Deinitialize ESP32 SDIO AT driver */
    esp32_sdio_at_deinit();
    
    LOG_I("ESP32 SDIO AT driver deinitialized");
    return RT_EOK;
}

/* ESP32 SDIO device ID table */
static struct rt_sdio_device_id esp32_sdio_ids[] = {
    {
        .func_code = SDIO_FUNC_CODE_WLAN,  /* WiFi function */
        .manufacturer = ESP32_SDIO_VENDOR_ID,
        .product = ESP32_SDIO_DEVICE_ID,
    },
    {
        .func_code = SDIO_ANY_FUNC_ID,     /* Any function */
        .manufacturer = ESP32_SDIO_VENDOR_ID,
        .product = SDIO_ANY_PROD_ID,
    },
    /* Terminator */
    {
        .func_code = 0,
        .manufacturer = 0,
        .product = 0,
    }
};

/* ESP32 SDIO driver structure */
static struct rt_sdio_driver esp32_sdio_driver = {
    .name = "esp32_sdio_at",
    .probe = esp32_sdio_probe,
    .remove = esp32_sdio_remove,
    .id = esp32_sdio_ids,
};

/**
 * @brief Initialize ESP32 SDIO driver registration
 * @return RT_EOK on success, error code on failure
 */
static int esp32_sdio_driver_init(void)
{
    rt_err_t ret;
    
    LOG_I("Registering ESP32 SDIO driver");
    
    ret = sdio_register_driver(&esp32_sdio_driver);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to register ESP32 SDIO driver: %d", ret);
        return ret;
    }
    
    LOG_I("ESP32 SDIO driver registered successfully");
    return RT_EOK;
}

/**
 * @brief Deinitialize ESP32 SDIO driver registration
 */
static void esp32_sdio_driver_deinit(void)
{
    LOG_I("Unregistering ESP32 SDIO driver");
    sdio_unregister_driver(&esp32_sdio_driver);
}

/* Auto-initialize the driver */
INIT_DEVICE_EXPORT(esp32_sdio_driver_init);

/* MSH commands for manual control */
#ifdef RT_USING_FINSH
#include <finsh.h>

static void esp32_probe_info(void)
{
    rt_kprintf("ESP32 SDIO Driver Information:\n");
    rt_kprintf("Driver name: %s\n", esp32_sdio_driver.name);
    rt_kprintf("Vendor ID: 0x%04x\n", ESP32_SDIO_VENDOR_ID);
    rt_kprintf("Device ID: 0x%04x\n", ESP32_SDIO_DEVICE_ID);
    rt_kprintf("Driver ready: %s\n", esp32_sdio_at_is_ready() ? "Yes" : "No");
}
MSH_CMD_EXPORT(esp32_probe_info, Show ESP32 SDIO probe information);

static void esp32_manual_init(void)
{
    /* This function can be used to manually initialize the driver
     * if auto-detection fails */
    rt_kprintf("Manual ESP32 SDIO initialization not implemented\n");
    rt_kprintf("Use esp32_probe_info to check driver status\n");
}
MSH_CMD_EXPORT(esp32_manual_init, Manually initialize ESP32 SDIO driver);

#endif /* RT_USING_FINSH */