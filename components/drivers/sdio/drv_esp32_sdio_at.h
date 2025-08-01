/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-XX     Assistant    first version - ESP32 SDIO AT command driver
 */

#ifndef __DRV_ESP32_SDIO_AT_H__
#define __DRV_ESP32_SDIO_AT_H__

#include <rtthread.h>
#include <drivers/dev_sdio.h>
#include <drivers/mmcsd_card.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32 SDIO AT Command Driver Configuration */
#define ESP32_SDIO_AT_BUFFER_SIZE       512
#define ESP32_SDIO_AT_CMD_TIMEOUT       5000    /* 5 seconds */
#define ESP32_SDIO_AT_RESP_TIMEOUT      10000   /* 10 seconds */
#define ESP32_SDIO_AT_MAX_RETRY         3

/* ESP32 SDIO Function Numbers */
#define ESP32_SDIO_FUNC_AT              1       /* AT command function */
#define ESP32_SDIO_FUNC_DATA            2       /* Data transfer function */

/* ESP32 SDIO Register Addresses */
#define ESP32_SDIO_REG_CMD              0x00    /* Command register */
#define ESP32_SDIO_REG_STATUS           0x04    /* Status register */
#define ESP32_SDIO_REG_DATA_LEN         0x08    /* Data length register */
#define ESP32_SDIO_REG_DATA_ADDR        0x0C    /* Data address register */
#define ESP32_SDIO_REG_INT_EN           0x10    /* Interrupt enable register */
#define ESP32_SDIO_REG_INT_STATUS       0x14    /* Interrupt status register */

/* ESP32 SDIO Status Bits */
#define ESP32_SDIO_STATUS_READY         (1 << 0)
#define ESP32_SDIO_STATUS_CMD_PENDING   (1 << 1)
#define ESP32_SDIO_STATUS_RESP_READY    (1 << 2)
#define ESP32_SDIO_STATUS_ERROR         (1 << 3)
#define ESP32_SDIO_STATUS_BUSY          (1 << 4)

/* ESP32 SDIO Interrupt Bits */
#define ESP32_SDIO_INT_CMD_COMPLETE     (1 << 0)
#define ESP32_SDIO_INT_RESP_READY       (1 << 1)
#define ESP32_SDIO_INT_ERROR            (1 << 2)
#define ESP32_SDIO_INT_DATA_READY       (1 << 3)

/* AT Command Response Types */
typedef enum {
    ESP32_AT_RESP_OK = 0,
    ESP32_AT_RESP_ERROR,
    ESP32_AT_RESP_FAIL,
    ESP32_AT_RESP_TIMEOUT,
    ESP32_AT_RESP_DATA,
    ESP32_AT_RESP_UNKNOWN
} esp32_at_resp_type_t;

/* AT Command Structure */
typedef struct {
    char *command;                      /* AT command string */
    char *response;                     /* Response buffer */
    rt_size_t resp_size;               /* Response buffer size */
    rt_uint32_t timeout;               /* Command timeout in ms */
    esp32_at_resp_type_t resp_type;    /* Expected response type */
} esp32_at_cmd_t;

/* ESP32 SDIO AT Driver Context */
typedef struct {
    struct rt_sdio_function *func;      /* SDIO function */
    struct rt_mmcsd_card *card;         /* SDIO card */
    
    rt_mutex_t cmd_mutex;               /* Command mutex */
    rt_sem_t resp_sem;                  /* Response semaphore */
    rt_thread_t irq_thread;             /* IRQ handling thread */
    
    rt_uint8_t *tx_buffer;              /* Transmit buffer */
    rt_uint8_t *rx_buffer;              /* Receive buffer */
    rt_size_t tx_len;                   /* Transmit length */
    rt_size_t rx_len;                   /* Receive length */
    
    rt_bool_t initialized;              /* Initialization flag */
    rt_bool_t connected;                /* Connection status */
    
    /* Statistics */
    rt_uint32_t cmd_count;              /* Command count */
    rt_uint32_t error_count;            /* Error count */
    rt_uint32_t timeout_count;          /* Timeout count */
} esp32_sdio_at_ctx_t;

/* Function Prototypes */

/**
 * @brief Initialize ESP32 SDIO AT command driver
 * @param card SDIO card pointer
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_sdio_at_init(struct rt_mmcsd_card *card);

/**
 * @brief Deinitialize ESP32 SDIO AT command driver
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_sdio_at_deinit(void);

/**
 * @brief Send AT command to ESP32
 * @param cmd AT command structure
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_sdio_at_send_cmd(esp32_at_cmd_t *cmd);

/**
 * @brief Send simple AT command with string response
 * @param cmd_str Command string
 * @param resp_buf Response buffer
 * @param resp_size Response buffer size
 * @param timeout Timeout in milliseconds
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_sdio_at_send_simple(const char *cmd_str, char *resp_buf, 
                                   rt_size_t resp_size, rt_uint32_t timeout);

/**
 * @brief Check if ESP32 is ready for commands
 * @return RT_TRUE if ready, RT_FALSE otherwise
 */
rt_bool_t esp32_sdio_at_is_ready(void);

/**
 * @brief Get driver statistics
 * @param cmd_count Command count pointer (can be NULL)
 * @param error_count Error count pointer (can be NULL)
 * @param timeout_count Timeout count pointer (can be NULL)
 */
void esp32_sdio_at_get_stats(rt_uint32_t *cmd_count, rt_uint32_t *error_count, 
                             rt_uint32_t *timeout_count);

/**
 * @brief Reset ESP32 SDIO AT driver
 * @return RT_EOK on success, error code on failure
 */
rt_err_t esp32_sdio_at_reset(void);

/**
 * @brief Set debug level for ESP32 SDIO AT driver
 * @param level Debug level (0=off, 1=error, 2=info, 3=debug)
 */
void esp32_sdio_at_set_debug(rt_uint8_t level);

/* Common AT Commands */
#define ESP32_AT_CMD_TEST           "AT"
#define ESP32_AT_CMD_RESET          "AT+RST"
#define ESP32_AT_CMD_VERSION        "AT+GMR"
#define ESP32_AT_CMD_WIFI_MODE      "AT+CWMODE"
#define ESP32_AT_CMD_WIFI_CONNECT   "AT+CWJAP"
#define ESP32_AT_CMD_WIFI_DISCONNECT "AT+CWQAP"
#define ESP32_AT_CMD_WIFI_STATUS    "AT+CWSTATE"
#define ESP32_AT_CMD_IP_STATUS      "AT+CIPSTATUS"
#define ESP32_AT_CMD_TCP_CONNECT    "AT+CIPSTART"
#define ESP32_AT_CMD_TCP_SEND       "AT+CIPSEND"
#define ESP32_AT_CMD_TCP_CLOSE      "AT+CIPCLOSE"

/* Common AT Responses */
#define ESP32_AT_RESP_OK_STR        "OK"
#define ESP32_AT_RESP_ERROR_STR     "ERROR"
#define ESP32_AT_RESP_FAIL_STR      "FAIL"
#define ESP32_AT_RESP_READY_STR     "ready"

#ifdef __cplusplus
}
#endif

#endif /* __DRV_ESP32_SDIO_AT_H__ */