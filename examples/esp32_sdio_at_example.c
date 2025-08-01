/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT Example Application
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include <string.h>

/* Include ESP32 SDIO AT driver header */
#include "../components/drivers/sdio/drv_esp32_sdio_at.h"

#define DBG_TAG               "ESP32_EXAMPLE"
#define DBG_LVL               DBG_INFO
#include <rtdbg.h>

/* Example configuration */
#define EXAMPLE_WIFI_SSID     "YourWiFiSSID"
#define EXAMPLE_WIFI_PASSWORD "YourWiFiPassword"
#define EXAMPLE_TCP_HOST      "httpbin.org"
#define EXAMPLE_TCP_PORT      80
#define EXAMPLE_HTTP_REQUEST  "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n"

/* Function prototypes */
static void esp32_at_example_basic_test(void);
static void esp32_at_example_wifi_test(void);
static void esp32_at_example_tcp_test(void);
static void esp32_at_example_full_demo(void);

/**
 * @brief Basic ESP32 AT command test
 */
static void esp32_at_example_basic_test(void)
{
    rt_err_t err;
    char version[256];
    
    LOG_I("=== ESP32 Basic AT Command Test ===");
    
    /* Test ESP32 connectivity */
    LOG_I("Testing ESP32 connectivity...");
    err = esp32_at_test();
    if (err != RT_EOK)
    {
        LOG_E("ESP32 connectivity test failed: %d", err);
        return;
    }
    LOG_I("ESP32 connectivity test: PASSED");
    
    /* Get firmware version */
    LOG_I("Getting ESP32 firmware version...");
    err = esp32_at_get_version(version, sizeof(version));
    if (err == RT_EOK)
    {
        LOG_I("ESP32 firmware version: %s", version);
    }
    else
    {
        LOG_E("Failed to get firmware version: %d", err);
    }
    
    /* Reset ESP32 */
    LOG_I("Resetting ESP32 module...");
    err = esp32_at_reset();
    if (err == RT_EOK)
    {
        LOG_I("ESP32 reset: PASSED");
    }
    else
    {
        LOG_E("ESP32 reset failed: %d", err);
    }
    
    LOG_I("=== Basic AT Command Test Complete ===\n");
}

/**
 * @brief WiFi functionality test
 */
static void esp32_at_example_wifi_test(void)
{
    rt_err_t err;
    char status[256];
    char ip_info[256];
    
    LOG_I("=== ESP32 WiFi Functionality Test ===");
    
    /* Set WiFi mode to Station */
    LOG_I("Setting WiFi mode to Station...");
    err = esp32_at_set_wifi_mode(ESP32_WIFI_MODE_STA);
    if (err != RT_EOK)
    {
        LOG_E("Failed to set WiFi mode: %d", err);
        return;
    }
    LOG_I("WiFi mode set to Station: PASSED");
    
    /* Connect to WiFi network */
    LOG_I("Connecting to WiFi network: %s", EXAMPLE_WIFI_SSID);
    err = esp32_at_wifi_connect(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASSWORD);
    if (err != RT_EOK)
    {
        LOG_E("WiFi connection failed: %d", err);
        LOG_W("Please check SSID and password in the example configuration");
        return;
    }
    LOG_I("WiFi connection: PASSED");
    
    /* Get WiFi status */
    LOG_I("Getting WiFi connection status...");
    err = esp32_at_get_wifi_status(status, sizeof(status));
    if (err == RT_EOK)
    {
        LOG_I("WiFi status: %s", status);
    }
    else
    {
        LOG_E("Failed to get WiFi status: %d", err);
    }
    
    /* Get IP information */
    LOG_I("Getting IP address information...");
    err = esp32_at_get_ip_info(ip_info, sizeof(ip_info));
    if (err == RT_EOK)
    {
        LOG_I("IP information: %s", ip_info);
    }
    else
    {
        LOG_E("Failed to get IP information: %d", err);
    }
    
    LOG_I("=== WiFi Functionality Test Complete ===\n");
}

/**
 * @brief TCP communication test
 */
static void esp32_at_example_tcp_test(void)
{
    rt_err_t err;
    
    LOG_I("=== ESP32 TCP Communication Test ===");
    
    /* Establish TCP connection */
    LOG_I("Establishing TCP connection to %s:%d", EXAMPLE_TCP_HOST, EXAMPLE_TCP_PORT);
    err = esp32_at_tcp_connect(EXAMPLE_TCP_HOST, EXAMPLE_TCP_PORT);
    if (err != RT_EOK)
    {
        LOG_E("TCP connection failed: %d", err);
        return;
    }
    LOG_I("TCP connection established: PASSED");
    
    /* Send HTTP request */
    LOG_I("Sending HTTP request...");
    err = esp32_at_tcp_send(EXAMPLE_HTTP_REQUEST, rt_strlen(EXAMPLE_HTTP_REQUEST));
    if (err != RT_EOK)
    {
        LOG_E("Failed to send HTTP request: %d", err);
    }
    else
    {
        LOG_I("HTTP request sent: PASSED");
    }
    
    /* Wait for response (in real application, you would implement proper response handling) */
    rt_thread_mdelay(2000);
    
    /* Close TCP connection */
    LOG_I("Closing TCP connection...");
    err = esp32_at_tcp_close();
    if (err == RT_EOK)
    {
        LOG_I("TCP connection closed: PASSED");
    }
    else
    {
        LOG_E("Failed to close TCP connection: %d", err);
    }
    
    LOG_I("=== TCP Communication Test Complete ===\n");
}

/**
 * @brief Full ESP32 SDIO AT demonstration
 */
static void esp32_at_example_full_demo(void)
{
    LOG_I("Starting ESP32 SDIO AT Command Full Demonstration");
    LOG_I("================================================");
    
    /* Wait for system initialization */
    rt_thread_mdelay(1000);
    
    /* Run basic test */
    esp32_at_example_basic_test();
    rt_thread_mdelay(1000);
    
    /* Run WiFi test */
    esp32_at_example_wifi_test();
    rt_thread_mdelay(1000);
    
    /* Run TCP test */
    esp32_at_example_tcp_test();
    
    LOG_I("================================================");
    LOG_I("ESP32 SDIO AT Command Full Demonstration Complete");
}

/**
 * @brief ESP32 AT example thread entry
 */
static void esp32_at_example_thread_entry(void *parameter)
{
    /* Run full demonstration */
    esp32_at_example_full_demo();
    
    /* Keep thread alive for interactive testing */
    while (1)
    {
        rt_thread_mdelay(10000);
    }
}

/**
 * @brief Initialize ESP32 AT example application
 */
int esp32_at_example_init(void)
{
    rt_thread_t example_thread;
    
    LOG_I("Initializing ESP32 SDIO AT Example Application");
    
    /* Create example thread */
    example_thread = rt_thread_create("esp32_example",
                                      esp32_at_example_thread_entry,
                                      RT_NULL,
                                      4096,
                                      RT_THREAD_PRIORITY_MAX / 2,
                                      20);
    
    if (example_thread == RT_NULL)
    {
        LOG_E("Failed to create ESP32 example thread");
        return -RT_ERROR;
    }
    
    /* Start example thread */
    rt_thread_startup(example_thread);
    
    LOG_I("ESP32 SDIO AT Example Application started");
    return RT_EOK;
}

/* MSH command support for interactive testing */
#ifdef RT_USING_FINSH
#include <finsh.h>

/**
 * @brief MSH command: Test ESP32 connectivity
 */
static int cmd_esp32_test(int argc, char **argv)
{
    rt_err_t err = esp32_at_test();
    if (err == RT_EOK)
    {
        rt_kprintf("ESP32 test: PASSED\n");
    }
    else
    {
        rt_kprintf("ESP32 test: FAILED (%d)\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_test, esp32_test, Test ESP32 connectivity);

/**
 * @brief MSH command: Reset ESP32
 */
static int cmd_esp32_reset(int argc, char **argv)
{
    rt_err_t err = esp32_at_reset();
    if (err == RT_EOK)
    {
        rt_kprintf("ESP32 reset: SUCCESS\n");
    }
    else
    {
        rt_kprintf("ESP32 reset: FAILED (%d)\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_reset, esp32_rst, Reset ESP32 module);

/**
 * @brief MSH command: Get ESP32 version
 */
static int cmd_esp32_version(int argc, char **argv)
{
    char version[256];
    rt_err_t err = esp32_at_get_version(version, sizeof(version));
    if (err == RT_EOK)
    {
        rt_kprintf("ESP32 version: %s\n", version);
    }
    else
    {
        rt_kprintf("Failed to get version: %d\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_version, esp32_ver, Get ESP32 firmware version);

/**
 * @brief MSH command: Set WiFi mode
 */
static int cmd_esp32_wifi_mode(int argc, char **argv)
{
    if (argc != 2)
    {
        rt_kprintf("Usage: esp32_mode <mode>\n");
        rt_kprintf("  mode: 1=STA, 2=AP, 3=STA+AP\n");
        return -1;
    }
    
    int mode = atoi(argv[1]);
    rt_err_t err = esp32_at_set_wifi_mode(mode);
    if (err == RT_EOK)
    {
        rt_kprintf("WiFi mode set to %d: SUCCESS\n", mode);
    }
    else
    {
        rt_kprintf("Failed to set WiFi mode: %d\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_wifi_mode, esp32_mode, Set ESP32 WiFi mode);

/**
 * @brief MSH command: Connect to WiFi
 */
static int cmd_esp32_wifi_connect(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        rt_kprintf("Usage: esp32_connect <ssid> [password]\n");
        return -1;
    }
    
    const char *ssid = argv[1];
    const char *password = (argc == 3) ? argv[2] : RT_NULL;
    
    rt_err_t err = esp32_at_wifi_connect(ssid, password);
    if (err == RT_EOK)
    {
        rt_kprintf("WiFi connection to '%s': SUCCESS\n", ssid);
    }
    else
    {
        rt_kprintf("WiFi connection to '%s': FAILED (%d)\n", ssid, err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_wifi_connect, esp32_connect, Connect to WiFi network);

/**
 * @brief MSH command: Disconnect from WiFi
 */
static int cmd_esp32_wifi_disconnect(int argc, char **argv)
{
    rt_err_t err = esp32_at_wifi_disconnect();
    if (err == RT_EOK)
    {
        rt_kprintf("WiFi disconnection: SUCCESS\n");
    }
    else
    {
        rt_kprintf("WiFi disconnection: FAILED (%d)\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_wifi_disconnect, esp32_disconnect, Disconnect from WiFi);

/**
 * @brief MSH command: Get WiFi status
 */
static int cmd_esp32_wifi_status(int argc, char **argv)
{
    char status[256];
    rt_err_t err = esp32_at_get_wifi_status(status, sizeof(status));
    if (err == RT_EOK)
    {
        rt_kprintf("WiFi status: %s\n", status);
    }
    else
    {
        rt_kprintf("Failed to get WiFi status: %d\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_wifi_status, esp32_status, Get WiFi connection status);

/**
 * @brief MSH command: Get IP information
 */
static int cmd_esp32_ip_info(int argc, char **argv)
{
    char ip_info[256];
    rt_err_t err = esp32_at_get_ip_info(ip_info, sizeof(ip_info));
    if (err == RT_EOK)
    {
        rt_kprintf("IP info: %s\n", ip_info);
    }
    else
    {
        rt_kprintf("Failed to get IP info: %d\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_ip_info, esp32_ip, Get IP address information);

/**
 * @brief MSH command: TCP connect
 */
static int cmd_esp32_tcp_connect(int argc, char **argv)
{
    if (argc != 3)
    {
        rt_kprintf("Usage: esp32_tcp_connect <host> <port>\n");
        return -1;
    }
    
    const char *host = argv[1];
    int port = atoi(argv[2]);
    
    rt_err_t err = esp32_at_tcp_connect(host, port);
    if (err == RT_EOK)
    {
        rt_kprintf("TCP connection to %s:%d: SUCCESS\n", host, port);
    }
    else
    {
        rt_kprintf("TCP connection to %s:%d: FAILED (%d)\n", host, port, err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_tcp_connect, esp32_tcp_conn, Establish TCP connection);

/**
 * @brief MSH command: TCP send
 */
static int cmd_esp32_tcp_send(int argc, char **argv)
{
    if (argc != 2)
    {
        rt_kprintf("Usage: esp32_tcp_send <data>\n");
        return -1;
    }
    
    const char *data = argv[1];
    rt_err_t err = esp32_at_tcp_send(data, rt_strlen(data));
    if (err == RT_EOK)
    {
        rt_kprintf("TCP send: SUCCESS\n");
    }
    else
    {
        rt_kprintf("TCP send: FAILED (%d)\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_tcp_send, esp32_tcp_send, Send data via TCP);

/**
 * @brief MSH command: TCP close
 */
static int cmd_esp32_tcp_close(int argc, char **argv)
{
    rt_err_t err = esp32_at_tcp_close();
    if (err == RT_EOK)
    {
        rt_kprintf("TCP close: SUCCESS\n");
    }
    else
    {
        rt_kprintf("TCP close: FAILED (%d)\n", err);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_tcp_close, esp32_tcp_close, Close TCP connection);

/**
 * @brief MSH command: Run full demo
 */
static int cmd_esp32_demo(int argc, char **argv)
{
    rt_kprintf("Starting ESP32 SDIO AT full demonstration...\n");
    esp32_at_example_full_demo();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp32_demo, esp32_demo, Run ESP32 SDIO AT full demo);

#endif /* RT_USING_FINSH */

/* Auto-initialize example application */
INIT_APP_EXPORT(esp32_at_example_init);