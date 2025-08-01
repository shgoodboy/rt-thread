/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT Command Driver Header
 */

#ifndef __DRV_ESP32_SDIO_AT_H__
#define __DRV_ESP32_SDIO_AT_H__

#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32 SDIO AT Control Commands */
#define ESP32_SDIO_AT_CMD_RESET      0x01
#define ESP32_SDIO_AT_CMD_GET_STATUS 0x02

/* ESP32 AT Command Response Codes */
#define ESP32_AT_RESP_OK             "OK"
#define ESP32_AT_RESP_ERROR          "ERROR"
#define ESP32_AT_RESP_FAIL           "FAIL"
#define ESP32_AT_RESP_READY          "ready"

/* Common AT Commands */
#define ESP32_AT_CMD_TEST            "AT\r\n"
#define ESP32_AT_CMD_RESET           "AT+RST\r\n"
#define ESP32_AT_CMD_VERSION         "AT+GMR\r\n"
#define ESP32_AT_CMD_WIFI_MODE       "AT+CWMODE"
#define ESP32_AT_CMD_WIFI_CONNECT    "AT+CWJAP"
#define ESP32_AT_CMD_WIFI_DISCONNECT "AT+CWQAP\r\n"
#define ESP32_AT_CMD_WIFI_STATUS     "AT+CWJAP?\r\n"
#define ESP32_AT_CMD_IP_STATUS       "AT+CIFSR\r\n"
#define ESP32_AT_CMD_TCP_CONNECT     "AT+CIPSTART"
#define ESP32_AT_CMD_TCP_SEND        "AT+CIPSEND"
#define ESP32_AT_CMD_TCP_CLOSE       "AT+CIPCLOSE\r\n"

/* WiFi Modes */
#define ESP32_WIFI_MODE_STA          1
#define ESP32_WIFI_MODE_AP           2
#define ESP32_WIFI_MODE_STA_AP       3

/* Function Prototypes */

/**
 * @brief Initialize ESP32 SDIO AT driver
 * @return RT_EOK on success, error code on failure
 */
int rt_hw_esp32_sdio_at_init(void);

/**
 * @brief Send AT command and wait for response
 * @param cmd AT command string (null-terminated)
 * @param response Buffer to store response
 * @param resp_size Size of response buffer
 * @param timeout Timeout in milliseconds (0 for default)
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_at_send_command(const char *cmd, char *response, rt_size_t resp_size, rt_uint32_t timeout);

/**
 * @brief Test ESP32 connectivity
 * @return RT_EOK if ESP32 responds to AT command
 */
rt_err_t esp32_at_test(void);

/**
 * @brief Reset ESP32 module
 * @return RT_EOK on success
 */
rt_err_t esp32_at_reset(void);

/**
 * @brief Get ESP32 firmware version
 * @param version Buffer to store version string
 * @param size Size of version buffer
 * @return RT_EOK on success
 */
rt_err_t esp32_at_get_version(char *version, rt_size_t size);

/**
 * @brief Set ESP32 WiFi mode
 * @param mode WiFi mode (1=STA, 2=AP, 3=STA+AP)
 * @return RT_EOK on success
 */
rt_err_t esp32_at_set_wifi_mode(rt_uint8_t mode);

/**
 * @brief Connect to WiFi network
 * @param ssid Network SSID
 * @param password Network password
 * @return RT_EOK on success
 */
rt_err_t esp32_at_wifi_connect(const char *ssid, const char *password);

/**
 * @brief Disconnect from WiFi network
 * @return RT_EOK on success
 */
rt_err_t esp32_at_wifi_disconnect(void);

/**
 * @brief Get WiFi connection status
 * @param status Buffer to store status information
 * @param size Size of status buffer
 * @return RT_EOK on success
 */
rt_err_t esp32_at_get_wifi_status(char *status, rt_size_t size);

/**
 * @brief Get IP address information
 * @param ip_info Buffer to store IP information
 * @param size Size of IP info buffer
 * @return RT_EOK on success
 */
rt_err_t esp32_at_get_ip_info(char *ip_info, rt_size_t size);

/**
 * @brief Establish TCP connection
 * @param host Remote host address
 * @param port Remote port number
 * @return RT_EOK on success
 */
rt_err_t esp32_at_tcp_connect(const char *host, rt_uint16_t port);

/**
 * @brief Send data via TCP connection
 * @param data Data to send
 * @param length Length of data
 * @return RT_EOK on success
 */
rt_err_t esp32_at_tcp_send(const void *data, rt_size_t length);

/**
 * @brief Close TCP connection
 * @return RT_EOK on success
 */
rt_err_t esp32_at_tcp_close(void);

/* Utility Macros */
#define ESP32_AT_CHECK_RESPONSE(resp, expected) \
    (rt_strstr(resp, expected) != RT_NULL)

#define ESP32_AT_IS_OK(resp) \
    ESP32_AT_CHECK_RESPONSE(resp, ESP32_AT_RESP_OK)

#define ESP32_AT_IS_ERROR(resp) \
    (ESP32_AT_CHECK_RESPONSE(resp, ESP32_AT_RESP_ERROR) || \
     ESP32_AT_CHECK_RESPONSE(resp, ESP32_AT_RESP_FAIL))

#ifdef __cplusplus
}
#endif

#endif /* __DRV_ESP32_SDIO_AT_H__ */