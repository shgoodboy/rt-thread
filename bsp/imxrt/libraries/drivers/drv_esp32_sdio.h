/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-12-XX     Assistant    ESP32 SDIO AT command driver for imxrt1171
 */

#ifndef __DRV_ESP32_SDIO_H__
#define __DRV_ESP32_SDIO_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/mmcsd_core.h>
#include <drivers/sdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32 SDIO AT Command Configuration */
#define ESP32_SDIO_BLOCK_SIZE           512
#define ESP32_SDIO_MAX_TRANSFER_SIZE    2048
#define ESP32_SDIO_CMD_TIMEOUT          5000    /* 5 seconds */
#define ESP32_SDIO_RESP_TIMEOUT         10000   /* 10 seconds */

/* ESP32 SDIO Function Numbers */
#define ESP32_SDIO_FUNC_0               0       /* Control function */
#define ESP32_SDIO_FUNC_1               1       /* AT command function */
#define ESP32_SDIO_FUNC_2               2       /* Data function (if needed) */

/* ESP32 SDIO Register Addresses */
#define ESP32_SDIO_REG_CMD_READY        0x00
#define ESP32_SDIO_REG_CMD_LEN          0x04
#define ESP32_SDIO_REG_CMD_DATA         0x08
#define ESP32_SDIO_REG_RESP_READY       0x40
#define ESP32_SDIO_REG_RESP_LEN         0x44
#define ESP32_SDIO_REG_RESP_DATA        0x48

/* ESP32 SDIO Status Flags */
#define ESP32_SDIO_CMD_READY_FLAG       0x01
#define ESP32_SDIO_RESP_READY_FLAG      0x02
#define ESP32_SDIO_ERROR_FLAG           0x80

/* ESP32 SDIO Device Structure */
struct esp32_sdio_device
{
    struct rt_sdio_function *func;
    struct rt_device device;
    struct rt_mutex lock;
    struct rt_semaphore cmd_sem;
    struct rt_semaphore resp_sem;
    
    rt_uint8_t *cmd_buffer;
    rt_uint8_t *resp_buffer;
    rt_size_t cmd_len;
    rt_size_t resp_len;
    
    rt_bool_t initialized;
    rt_thread_t recv_thread;
    rt_mailbox_t resp_mb;
};

/* ESP32 SDIO AT Command Structure */
struct esp32_at_cmd
{
    char *cmd;
    rt_size_t cmd_len;
    char *resp;
    rt_size_t resp_len;
    rt_int32_t timeout;
};

/* Function Declarations */
rt_err_t esp32_sdio_probe(struct rt_sdio_function *func, const struct rt_sdio_device_id *id);
void esp32_sdio_remove(struct rt_sdio_function *func);

rt_err_t esp32_sdio_init(struct esp32_sdio_device *esp32_dev);
rt_err_t esp32_sdio_deinit(struct esp32_sdio_device *esp32_dev);

rt_err_t esp32_sdio_send_at_cmd(struct esp32_sdio_device *esp32_dev, const char *cmd, char *resp, rt_size_t resp_size, rt_int32_t timeout);
rt_err_t esp32_sdio_read_response(struct esp32_sdio_device *esp32_dev, char *resp, rt_size_t resp_size, rt_int32_t timeout);

rt_err_t esp32_sdio_write_reg(struct esp32_sdio_device *esp32_dev, rt_uint32_t addr, rt_uint32_t value);
rt_err_t esp32_sdio_read_reg(struct esp32_sdio_device *esp32_dev, rt_uint32_t addr, rt_uint32_t *value);

/* Device Registration */
rt_err_t rt_hw_esp32_sdio_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_ESP32_SDIO_H__ */