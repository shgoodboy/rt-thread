/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-XX     Assistant    ESP32 SDIO AT command example
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdio.h>
#include "../components/drivers/sdio/drv_esp32_sdio_at.h"

#define EXAMPLE_WIFI_SSID       "YourWiFiSSID"
#define EXAMPLE_WIFI_PASSWORD   "YourWiFiPassword"
#define EXAMPLE_TCP_SERVER      "httpbin.org"
#define EXAMPLE_TCP_PORT        80

/* Example thread parameters */
#define EXAMPLE_THREAD_STACK_SIZE   2048
#define EXAMPLE_THREAD_PRIORITY     20
#define EXAMPLE_THREAD_TIMESLICE    10

static rt_thread_t example_thread = RT_NULL;
static rt_bool_t example_running = RT_FALSE;

/**
 * @brief Test basic AT commands
 */
static rt_err_t test_basic_commands(void)
{
    char response[256];
    rt_err_t ret;
    
    rt_kprintf("\n=== Testing Basic AT Commands ===\n");
    
    /* Test AT command */
    rt_kprintf("Testing AT command...\n");
    ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_TEST, response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("AT Response: %s\n", response);
    }
    else
    {
        rt_kprintf("AT command failed: %d\n", ret);
        return ret;
    }
    
    /* Get version information */
    rt_kprintf("Getting version information...\n");
    ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_VERSION, response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("Version: %s\n", response);
    }
    else
    {
        rt_kprintf("Version command failed: %d\n", ret);
        return ret;
    }
    
    return RT_EOK;
}

/**
 * @brief Test WiFi operations
 */
static rt_err_t test_wifi_operations(void)
{
    char response[256];
    char command[128];
    rt_err_t ret;
    
    rt_kprintf("\n=== Testing WiFi Operations ===\n");
    
    /* Set WiFi mode to Station */
    rt_kprintf("Setting WiFi mode to Station...\n");
    rt_snprintf(command, sizeof(command), "%s=1", ESP32_AT_CMD_WIFI_MODE);
    ret = esp32_sdio_at_send_simple(command, response, sizeof(response), 5000);
    if (ret != RT_EOK)
    {
        rt_kprintf("Failed to set WiFi mode: %d\n", ret);
        return ret;
    }
    rt_kprintf("WiFi mode set: %s\n", response);
    
    /* Connect to WiFi */
    rt_kprintf("Connecting to WiFi: %s\n", EXAMPLE_WIFI_SSID);
    rt_snprintf(command, sizeof(command), "%s=\"%s\",\"%s\"", 
                ESP32_AT_CMD_WIFI_CONNECT, EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASSWORD);
    ret = esp32_sdio_at_send_simple(command, response, sizeof(response), 15000);
    if (ret != RT_EOK)
    {
        rt_kprintf("Failed to connect to WiFi: %d\n", ret);
        return ret;
    }
    rt_kprintf("WiFi connection result: %s\n", response);
    
    /* Check WiFi status */
    rt_kprintf("Checking WiFi status...\n");
    ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_WIFI_STATUS, response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("WiFi status: %s\n", response);
    }
    
    /* Check IP status */
    rt_kprintf("Checking IP status...\n");
    ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_IP_STATUS, response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("IP status: %s\n", response);
    }
    
    return RT_EOK;
}

/**
 * @brief Test TCP operations
 */
static rt_err_t test_tcp_operations(void)
{
    char response[512];
    char command[128];
    rt_err_t ret;
    
    rt_kprintf("\n=== Testing TCP Operations ===\n");
    
    /* Connect to TCP server */
    rt_kprintf("Connecting to TCP server: %s:%d\n", EXAMPLE_TCP_SERVER, EXAMPLE_TCP_PORT);
    rt_snprintf(command, sizeof(command), "%s=\"TCP\",\"%s\",%d", 
                ESP32_AT_CMD_TCP_CONNECT, EXAMPLE_TCP_SERVER, EXAMPLE_TCP_PORT);
    ret = esp32_sdio_at_send_simple(command, response, sizeof(response), 10000);
    if (ret != RT_EOK)
    {
        rt_kprintf("Failed to connect to TCP server: %d\n", ret);
        return ret;
    }
    rt_kprintf("TCP connection result: %s\n", response);
    
    /* Send HTTP GET request */
    rt_kprintf("Sending HTTP GET request...\n");
    const char *http_request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    rt_snprintf(command, sizeof(command), "%s=%d", ESP32_AT_CMD_TCP_SEND, rt_strlen(http_request));
    ret = esp32_sdio_at_send_simple(command, response, sizeof(response), 5000);
    if (ret != RT_EOK)
    {
        rt_kprintf("Failed to prepare TCP send: %d\n", ret);
        return ret;
    }
    
    /* Send the actual data */
    ret = esp32_sdio_at_send_simple(http_request, response, sizeof(response), 10000);
    if (ret == RT_EOK)
    {
        rt_kprintf("HTTP response: %s\n", response);
    }
    else
    {
        rt_kprintf("Failed to send HTTP request: %d\n", ret);
    }
    
    /* Close TCP connection */
    rt_kprintf("Closing TCP connection...\n");
    ret = esp32_sdio_at_send_simple(ESP32_AT_CMD_TCP_CLOSE, response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("TCP close result: %s\n", response);
    }
    
    return RT_EOK;
}

/**
 * @brief Show driver statistics
 */
static void show_driver_stats(void)
{
    rt_uint32_t cmd_count, error_count, timeout_count;
    
    esp32_sdio_at_get_stats(&cmd_count, &error_count, &timeout_count);
    
    rt_kprintf("\n=== Driver Statistics ===\n");
    rt_kprintf("Commands sent: %d\n", cmd_count);
    rt_kprintf("Errors: %d\n", error_count);
    rt_kprintf("Timeouts: %d\n", timeout_count);
    rt_kprintf("Success rate: %.1f%%\n", 
               cmd_count > 0 ? (float)(cmd_count - error_count - timeout_count) * 100.0f / cmd_count : 0.0f);
}

/**
 * @brief Example thread entry function
 */
static void esp32_sdio_at_example_thread(void *parameter)
{
    rt_err_t ret;
    
    rt_kprintf("ESP32 SDIO AT Example Started\n");
    rt_kprintf("==============================\n");
    
    /* Wait for driver to be ready */
    rt_kprintf("Waiting for ESP32 to be ready...\n");
    while (!esp32_sdio_at_is_ready() && example_running)
    {
        rt_thread_mdelay(1000);
        rt_kprintf(".");
    }
    
    if (!example_running)
    {
        rt_kprintf("\nExample stopped\n");
        return;
    }
    
    rt_kprintf("\nESP32 is ready!\n");
    
    /* Enable debug output */
    esp32_sdio_at_set_debug(2);
    
    /* Test basic commands */
    ret = test_basic_commands();
    if (ret != RT_EOK)
    {
        rt_kprintf("Basic commands test failed\n");
        goto exit;
    }
    
    /* Test WiFi operations */
    ret = test_wifi_operations();
    if (ret != RT_EOK)
    {
        rt_kprintf("WiFi operations test failed\n");
        goto exit;
    }
    
    /* Wait a bit for WiFi connection to stabilize */
    rt_thread_mdelay(3000);
    
    /* Test TCP operations */
    ret = test_tcp_operations();
    if (ret != RT_EOK)
    {
        rt_kprintf("TCP operations test failed\n");
    }
    
exit:
    /* Show statistics */
    show_driver_stats();
    
    rt_kprintf("\nESP32 SDIO AT Example Completed\n");
    example_running = RT_FALSE;
}

/**
 * @brief Start ESP32 SDIO AT example
 */
rt_err_t esp32_sdio_at_example_start(void)
{
    if (example_running)
    {
        rt_kprintf("Example is already running\n");
        return -RT_EBUSY;
    }
    
    if (!esp32_sdio_at_is_ready())
    {
        rt_kprintf("ESP32 SDIO AT driver not initialized\n");
        return -RT_ENOSYS;
    }
    
    example_running = RT_TRUE;
    
    example_thread = rt_thread_create("esp32_example",
                                     esp32_sdio_at_example_thread,
                                     RT_NULL,
                                     EXAMPLE_THREAD_STACK_SIZE,
                                     EXAMPLE_THREAD_PRIORITY,
                                     EXAMPLE_THREAD_TIMESLICE);
    
    if (!example_thread)
    {
        rt_kprintf("Failed to create example thread\n");
        example_running = RT_FALSE;
        return -RT_ENOMEM;
    }
    
    rt_thread_startup(example_thread);
    return RT_EOK;
}

/**
 * @brief Stop ESP32 SDIO AT example
 */
rt_err_t esp32_sdio_at_example_stop(void)
{
    if (!example_running)
    {
        rt_kprintf("Example is not running\n");
        return RT_EOK;
    }
    
    example_running = RT_FALSE;
    
    if (example_thread)
    {
        rt_thread_delete(example_thread);
        example_thread = RT_NULL;
    }
    
    rt_kprintf("ESP32 SDIO AT example stopped\n");
    return RT_EOK;
}

/* MSH commands for testing */
#ifdef RT_USING_FINSH
#include <finsh.h>

static void esp32_example_start(void)
{
    esp32_sdio_at_example_start();
}
MSH_CMD_EXPORT(esp32_example_start, Start ESP32 SDIO AT example);

static void esp32_example_stop(void)
{
    esp32_sdio_at_example_stop();
}
MSH_CMD_EXPORT(esp32_example_stop, Stop ESP32 SDIO AT example);

static void esp32_at_test(int argc, char **argv)
{
    char response[256];
    rt_err_t ret;
    
    if (argc < 2)
    {
        rt_kprintf("Usage: esp32_at_test <command>\n");
        rt_kprintf("Example: esp32_at_test AT\n");
        return;
    }
    
    if (!esp32_sdio_at_is_ready())
    {
        rt_kprintf("ESP32 SDIO AT driver not ready\n");
        return;
    }
    
    ret = esp32_sdio_at_send_simple(argv[1], response, sizeof(response), 5000);
    if (ret == RT_EOK)
    {
        rt_kprintf("Response: %s\n", response);
    }
    else
    {
        rt_kprintf("Command failed: %d\n", ret);
    }
}
MSH_CMD_EXPORT(esp32_at_test, Send AT command to ESP32);

static void esp32_stats(void)
{
    show_driver_stats();
}
MSH_CMD_EXPORT(esp32_stats, Show ESP32 SDIO AT driver statistics);

static void esp32_reset(void)
{
    rt_err_t ret = esp32_sdio_at_reset();
    if (ret == RT_EOK)
    {
        rt_kprintf("ESP32 reset successfully\n");
    }
    else
    {
        rt_kprintf("ESP32 reset failed: %d\n", ret);
    }
}
MSH_CMD_EXPORT(esp32_reset, Reset ESP32 module);

#endif /* RT_USING_FINSH */