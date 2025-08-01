/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT API Implementation
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "drv_esp32_sdio_at.h"

#define DBG_TAG               "ESP32_AT_API"
#define DBG_LVL               DBG_INFO
#include <rtdbg.h>

/* Internal helper functions */
static rt_err_t esp32_at_wait_for_response(const char *expected, rt_uint32_t timeout);
static rt_bool_t esp32_at_check_response_ok(const char *response);

/**
 * @brief Test ESP32 connectivity
 */
rt_err_t esp32_at_test(void)
{
    char response[64];
    rt_err_t err;
    
    LOG_D("Testing ESP32 connectivity");
    
    err = esp32_at_send_command(ESP32_AT_CMD_TEST, response, sizeof(response), 3000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to send AT test command: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response))
    {
        LOG_I("ESP32 AT test successful");
        return RT_EOK;
    }
    
    LOG_E("ESP32 AT test failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Reset ESP32 module
 */
rt_err_t esp32_at_reset(void)
{
    char response[128];
    rt_err_t err;
    
    LOG_I("Resetting ESP32 module");
    
    err = esp32_at_send_command(ESP32_AT_CMD_RESET, response, sizeof(response), 10000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to send reset command: %d", err);
        return err;
    }
    
    /* Wait for "ready" message after reset */
    if (rt_strstr(response, ESP32_AT_RESP_READY) != RT_NULL ||
        ESP32_AT_IS_OK(response))
    {
        LOG_I("ESP32 reset successful");
        /* Give ESP32 time to fully initialize */
        rt_thread_mdelay(2000);
        return RT_EOK;
    }
    
    LOG_E("ESP32 reset failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Get ESP32 firmware version
 */
rt_err_t esp32_at_get_version(char *version, rt_size_t size)
{
    char response[512];
    rt_err_t err;
    const char *version_start;
    
    if (!version || size == 0)
        return -RT_EINVAL;
    
    LOG_D("Getting ESP32 firmware version");
    
    err = esp32_at_send_command(ESP32_AT_CMD_VERSION, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to get version: %d", err);
        return err;
    }
    
    /* Extract version information from response */
    version_start = rt_strstr(response, "AT version:");
    if (version_start)
    {
        rt_strncpy(version, version_start, size - 1);
        version[size - 1] = '\0';
        LOG_I("ESP32 version: %s", version);
        return RT_EOK;
    }
    
    /* Fallback: copy entire response if version format is different */
    rt_strncpy(version, response, size - 1);
    version[size - 1] = '\0';
    
    return RT_EOK;
}

/**
 * @brief Set ESP32 WiFi mode
 */
rt_err_t esp32_at_set_wifi_mode(rt_uint8_t mode)
{
    char cmd[32];
    char response[128];
    rt_err_t err;
    
    if (mode < ESP32_WIFI_MODE_STA || mode > ESP32_WIFI_MODE_STA_AP)
        return -RT_EINVAL;
    
    LOG_D("Setting WiFi mode to %d", mode);
    
    rt_snprintf(cmd, sizeof(cmd), "%s=%d\r\n", ESP32_AT_CMD_WIFI_MODE, mode);
    
    err = esp32_at_send_command(cmd, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to set WiFi mode: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response))
    {
        LOG_I("WiFi mode set to %d successfully", mode);
        return RT_EOK;
    }
    
    LOG_E("Failed to set WiFi mode: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Connect to WiFi network
 */
rt_err_t esp32_at_wifi_connect(const char *ssid, const char *password)
{
    char cmd[256];
    char response[128];
    rt_err_t err;
    
    if (!ssid)
        return -RT_EINVAL;
    
    LOG_I("Connecting to WiFi network: %s", ssid);
    
    /* Format connection command */
    if (password && rt_strlen(password) > 0)
    {
        rt_snprintf(cmd, sizeof(cmd), "%s=\"%s\",\"%s\"\r\n", 
                   ESP32_AT_CMD_WIFI_CONNECT, ssid, password);
    }
    else
    {
        rt_snprintf(cmd, sizeof(cmd), "%s=\"%s\"\r\n", 
                   ESP32_AT_CMD_WIFI_CONNECT, ssid);
    }
    
    err = esp32_at_send_command(cmd, response, sizeof(response), 15000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to send WiFi connect command: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response))
    {
        LOG_I("WiFi connection successful");
        return RT_EOK;
    }
    
    LOG_E("WiFi connection failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Disconnect from WiFi network
 */
rt_err_t esp32_at_wifi_disconnect(void)
{
    char response[128];
    rt_err_t err;
    
    LOG_I("Disconnecting from WiFi network");
    
    err = esp32_at_send_command(ESP32_AT_CMD_WIFI_DISCONNECT, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to disconnect from WiFi: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response))
    {
        LOG_I("WiFi disconnection successful");
        return RT_EOK;
    }
    
    LOG_E("WiFi disconnection failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Get WiFi connection status
 */
rt_err_t esp32_at_get_wifi_status(char *status, rt_size_t size)
{
    char response[256];
    rt_err_t err;
    
    if (!status || size == 0)
        return -RT_EINVAL;
    
    LOG_D("Getting WiFi connection status");
    
    err = esp32_at_send_command(ESP32_AT_CMD_WIFI_STATUS, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to get WiFi status: %d", err);
        return err;
    }
    
    rt_strncpy(status, response, size - 1);
    status[size - 1] = '\0';
    
    LOG_D("WiFi status: %s", status);
    return RT_EOK;
}

/**
 * @brief Get IP address information
 */
rt_err_t esp32_at_get_ip_info(char *ip_info, rt_size_t size)
{
    char response[256];
    rt_err_t err;
    
    if (!ip_info || size == 0)
        return -RT_EINVAL;
    
    LOG_D("Getting IP address information");
    
    err = esp32_at_send_command(ESP32_AT_CMD_IP_STATUS, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to get IP info: %d", err);
        return err;
    }
    
    rt_strncpy(ip_info, response, size - 1);
    ip_info[size - 1] = '\0';
    
    LOG_D("IP info: %s", ip_info);
    return RT_EOK;
}

/**
 * @brief Establish TCP connection
 */
rt_err_t esp32_at_tcp_connect(const char *host, rt_uint16_t port)
{
    char cmd[128];
    char response[128];
    rt_err_t err;
    
    if (!host)
        return -RT_EINVAL;
    
    LOG_I("Establishing TCP connection to %s:%d", host, port);
    
    rt_snprintf(cmd, sizeof(cmd), "%s=\"TCP\",\"%s\",%d\r\n", 
               ESP32_AT_CMD_TCP_CONNECT, host, port);
    
    err = esp32_at_send_command(cmd, response, sizeof(response), 10000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to establish TCP connection: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response) || rt_strstr(response, "CONNECT") != RT_NULL)
    {
        LOG_I("TCP connection established successfully");
        return RT_EOK;
    }
    
    LOG_E("TCP connection failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Send data via TCP connection
 */
rt_err_t esp32_at_tcp_send(const void *data, rt_size_t length)
{
    char cmd[64];
    char response[128];
    rt_err_t err;
    rt_device_t dev;
    
    if (!data || length == 0)
        return -RT_EINVAL;
    
    LOG_D("Sending %d bytes via TCP", length);
    
    dev = rt_device_find("esp32_at");
    if (!dev)
    {
        LOG_E("ESP32 AT device not found");
        return -RT_ERROR;
    }
    
    /* Send CIPSEND command with data length */
    rt_snprintf(cmd, sizeof(cmd), "%s=%d\r\n", ESP32_AT_CMD_TCP_SEND, length);
    
    err = esp32_at_send_command(cmd, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to send CIPSEND command: %d", err);
        return err;
    }
    
    /* Check if ESP32 is ready to receive data */
    if (rt_strstr(response, ">") == RT_NULL)
    {
        LOG_E("ESP32 not ready for data transmission: %s", response);
        return -RT_ERROR;
    }
    
    /* Send actual data */
    rt_ssize_t sent = rt_device_write(dev, 0, data, length);
    if (sent != length)
    {
        LOG_E("Failed to send data: sent %d, expected %d", sent, length);
        return -RT_ERROR;
    }
    
    /* Wait for send confirmation */
    err = rt_device_read(dev, 0, response, sizeof(response) - 1);
    if (err > 0)
    {
        response[err] = '\0';
        if (rt_strstr(response, "SEND OK") != RT_NULL)
        {
            LOG_D("Data sent successfully");
            return RT_EOK;
        }
    }
    
    LOG_E("Data transmission failed: %s", response);
    return -RT_ERROR;
}

/**
 * @brief Close TCP connection
 */
rt_err_t esp32_at_tcp_close(void)
{
    char response[128];
    rt_err_t err;
    
    LOG_I("Closing TCP connection");
    
    err = esp32_at_send_command(ESP32_AT_CMD_TCP_CLOSE, response, sizeof(response), 5000);
    if (err != RT_EOK)
    {
        LOG_E("Failed to close TCP connection: %d", err);
        return err;
    }
    
    if (ESP32_AT_IS_OK(response) || rt_strstr(response, "CLOSED") != RT_NULL)
    {
        LOG_I("TCP connection closed successfully");
        return RT_EOK;
    }
    
    LOG_E("Failed to close TCP connection: %s", response);
    return -RT_ERROR;
}

/* Helper Functions */

/**
 * @brief Wait for specific response from ESP32
 */
static rt_err_t esp32_at_wait_for_response(const char *expected, rt_uint32_t timeout)
{
    rt_device_t dev;
    char response[256];
    rt_tick_t start_tick;
    rt_ssize_t ret;
    
    dev = rt_device_find("esp32_at");
    if (!dev)
        return -RT_ERROR;
    
    start_tick = rt_tick_get();
    
    while ((rt_tick_get() - start_tick) < rt_tick_from_millisecond(timeout))
    {
        ret = rt_device_read(dev, 0, response, sizeof(response) - 1);
        if (ret > 0)
        {
            response[ret] = '\0';
            if (rt_strstr(response, expected) != RT_NULL)
            {
                return RT_EOK;
            }
        }
        rt_thread_mdelay(100);
    }
    
    return -RT_ETIMEOUT;
}

/**
 * @brief Check if response indicates success
 */
static rt_bool_t esp32_at_check_response_ok(const char *response)
{
    if (!response)
        return RT_FALSE;
    
    return (rt_strstr(response, ESP32_AT_RESP_OK) != RT_NULL) &&
           (rt_strstr(response, ESP32_AT_RESP_ERROR) == RT_NULL) &&
           (rt_strstr(response, ESP32_AT_RESP_FAIL) == RT_NULL);
}