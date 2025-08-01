/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-01-15     Assistant     ESP32 SDIO AT Driver Header for i.MX RT1170
 */

#ifndef __DRV_ESP32_SDIO_AT_IMXRT_H__
#define __DRV_ESP32_SDIO_AT_IMXRT_H__

#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32 SDIO Configuration for i.MX RT1170 */
#define ESP32_SDIO_MAX_RESPONSE     2048

/* ESP32 SDIO Function Numbers */
#define ESP32_SDIO_FUNC_AT          1
#define ESP32_SDIO_FUNC_DATA        2

/* ESP32 SDIO Registers */
#define ESP32_SDIO_REG_CMD_STATUS   0x00
#define ESP32_SDIO_REG_CMD_DATA     0x04
#define ESP32_SDIO_REG_RESP_STATUS  0x08
#define ESP32_SDIO_REG_RESP_DATA    0x0C

/* ESP32 SDIO Events */
#define ESP32_SDIO_EVENT_RESP_READY 0x01
#define ESP32_SDIO_EVENT_ERROR      0x02

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

/* i.MX RT1170 Pin Mapping for ESP32 SDIO */
/*
 * USDHC2 Pin Configuration:
 * - USDHC2_CLK  -> GPIO_SD_B2_05 -> ESP32 CLK
 * - USDHC2_CMD  -> GPIO_SD_B2_04 -> ESP32 CMD
 * - USDHC2_DATA0 -> GPIO_SD_B2_08 -> ESP32 D0
 * - USDHC2_DATA1 -> GPIO_SD_B2_09 -> ESP32 D1
 * - USDHC2_DATA2 -> GPIO_SD_B2_10 -> ESP32 D2
 * - USDHC2_DATA3 -> GPIO_SD_B2_11 -> ESP32 D3
 */

/* Function Prototypes */

/**
 * @brief Initialize i.MX RT1170 ESP32 SDIO AT driver
 * @return RT_EOK on success, error code on failure
 */
int rt_hw_imxrt_esp32_sdio_at_init(void);

/**
 * @brief Send AT command and wait for response (compatible with generic API)
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

#endif /* __DRV_ESP32_SDIO_AT_IMXRT_H__ */