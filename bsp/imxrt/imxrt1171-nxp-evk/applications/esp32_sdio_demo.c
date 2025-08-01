/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-12-XX     Assistant    ESP32 SDIO AT command demo for IMXRT1171
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>

#ifdef BSP_USING_ESP32_SDIO

#define ESP32_SDIO_DEVICE_NAME  "esp32_sdio"
#define AT_RESPONSE_BUFFER_SIZE 512

/* AT command control structure */
struct esp32_at_cmd {
    const char *cmd;
    char *response;
    rt_size_t resp_len;
};

/* ESP32 SDIO AT command demo */
static void esp32_sdio_demo(void)
{
    rt_device_t esp32_dev;
    rt_err_t ret;
    char response[AT_RESPONSE_BUFFER_SIZE];
    struct esp32_at_cmd at_cmd;
    
    rt_kprintf("ESP32 SDIO AT Command Demo\n");
    rt_kprintf("==========================\n");
    
    /* Find ESP32 SDIO device */
    esp32_dev = rt_device_find(ESP32_SDIO_DEVICE_NAME);
    if (esp32_dev == RT_NULL) {
        rt_kprintf("Error: ESP32 SDIO device not found!\n");
        return;
    }
    
    /* Open ESP32 SDIO device */
    ret = rt_device_open(esp32_dev, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK) {
        rt_kprintf("Error: Failed to open ESP32 SDIO device! (ret=%d)\n", ret);
        return;
    }
    
    rt_kprintf("ESP32 SDIO device opened successfully\n");
    
    /* Test basic AT command */
    rt_kprintf("\n1. Testing basic AT command...\n");
    at_cmd.cmd = "AT";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("AT Response: %s\n", response);
    } else {
        rt_kprintf("AT command failed! (ret=%d)\n", ret);
    }
    
    /* Get firmware version */
    rt_kprintf("\n2. Getting firmware version...\n");
    at_cmd.cmd = "AT+GMR";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("Firmware Version: %s\n", response);
    } else {
        rt_kprintf("Get firmware version failed! (ret=%d)\n", ret);
    }
    
    /* Set WiFi mode */
    rt_kprintf("\n3. Setting WiFi mode to Station...\n");
    at_cmd.cmd = "AT+CWMODE=1";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("Set WiFi Mode Response: %s\n", response);
    } else {
        rt_kprintf("Set WiFi mode failed! (ret=%d)\n", ret);
    }
    
    /* Scan WiFi networks */
    rt_kprintf("\n4. Scanning WiFi networks...\n");
    at_cmd.cmd = "AT+CWLAP";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("WiFi Scan Results: %s\n", response);
    } else {
        rt_kprintf("WiFi scan failed! (ret=%d)\n", ret);
    }
    
    /* Connect to WiFi (example - modify SSID and password as needed) */
    rt_kprintf("\n5. Connecting to WiFi (example)...\n");
    at_cmd.cmd = "AT+CWJAP=\"YourSSID\",\"YourPassword\"";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("WiFi Connect Response: %s\n", response);
    } else {
        rt_kprintf("WiFi connect failed! (ret=%d)\n", ret);
    }
    
    /* Get IP address */
    rt_kprintf("\n6. Getting IP address...\n");
    at_cmd.cmd = "AT+CIFSR";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("IP Address Info: %s\n", response);
    } else {
        rt_kprintf("Get IP address failed! (ret=%d)\n", ret);
    }
    
    /* Close device */
    rt_device_close(esp32_dev);
    rt_kprintf("\nESP32 SDIO demo completed\n");
}

/* Interactive AT command shell */
static void esp32_at_shell(int argc, char **argv)
{
    rt_device_t esp32_dev;
    rt_err_t ret;
    char response[AT_RESPONSE_BUFFER_SIZE];
    struct esp32_at_cmd at_cmd;
    char cmd_buffer[256];
    
    if (argc < 2) {
        rt_kprintf("Usage: esp32_at <AT_command>\n");
        rt_kprintf("Example: esp32_at AT+GMR\n");
        return;
    }
    
    /* Construct AT command from arguments */
    rt_memset(cmd_buffer, 0, sizeof(cmd_buffer));
    for (int i = 1; i < argc; i++) {
        rt_strcat(cmd_buffer, argv[i]);
        if (i < argc - 1) {
            rt_strcat(cmd_buffer, " ");
        }
    }
    
    /* Find ESP32 SDIO device */
    esp32_dev = rt_device_find(ESP32_SDIO_DEVICE_NAME);
    if (esp32_dev == RT_NULL) {
        rt_kprintf("Error: ESP32 SDIO device not found!\n");
        return;
    }
    
    /* Open ESP32 SDIO device */
    ret = rt_device_open(esp32_dev, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK) {
        rt_kprintf("Error: Failed to open ESP32 SDIO device! (ret=%d)\n", ret);
        return;
    }
    
    /* Send AT command */
    at_cmd.cmd = cmd_buffer;
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    rt_kprintf("Sending: %s\n", cmd_buffer);
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("Response: %s\n", response);
    } else {
        rt_kprintf("AT command failed! (ret=%d)\n", ret);
    }
    
    /* Close device */
    rt_device_close(esp32_dev);
}

/* WiFi connection helper */
static void esp32_wifi_connect(int argc, char **argv)
{
    rt_device_t esp32_dev;
    rt_err_t ret;
    char response[AT_RESPONSE_BUFFER_SIZE];
    struct esp32_at_cmd at_cmd;
    char connect_cmd[256];
    
    if (argc != 3) {
        rt_kprintf("Usage: esp32_wifi <SSID> <Password>\n");
        rt_kprintf("Example: esp32_wifi MyWiFi MyPassword\n");
        return;
    }
    
    /* Find ESP32 SDIO device */
    esp32_dev = rt_device_find(ESP32_SDIO_DEVICE_NAME);
    if (esp32_dev == RT_NULL) {
        rt_kprintf("Error: ESP32 SDIO device not found!\n");
        return;
    }
    
    /* Open ESP32 SDIO device */
    ret = rt_device_open(esp32_dev, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK) {
        rt_kprintf("Error: Failed to open ESP32 SDIO device! (ret=%d)\n", ret);
        return;
    }
    
    /* Set WiFi mode to Station */
    rt_kprintf("Setting WiFi mode to Station...\n");
    at_cmd.cmd = "AT+CWMODE=1";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret != RT_EOK) {
        rt_kprintf("Failed to set WiFi mode!\n");
        rt_device_close(esp32_dev);
        return;
    }
    
    /* Connect to WiFi */
    rt_snprintf(connect_cmd, sizeof(connect_cmd), "AT+CWJAP=\"%s\",\"%s\"", argv[1], argv[2]);
    rt_kprintf("Connecting to WiFi: %s\n", argv[1]);
    
    at_cmd.cmd = connect_cmd;
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    if (ret == RT_EOK) {
        rt_kprintf("WiFi connection successful!\n");
        rt_kprintf("Response: %s\n", response);
        
        /* Get IP address */
        rt_kprintf("Getting IP address...\n");
        at_cmd.cmd = "AT+CIFSR";
        at_cmd.response = response;
        at_cmd.resp_len = sizeof(response);
        
        ret = rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
        if (ret == RT_EOK) {
            rt_kprintf("IP Address: %s\n", response);
        }
    } else {
        rt_kprintf("WiFi connection failed!\n");
        rt_kprintf("Response: %s\n", response);
    }
    
    /* Close device */
    rt_device_close(esp32_dev);
}

/* Export shell commands */
MSH_CMD_EXPORT(esp32_sdio_demo, ESP32 SDIO AT command demo);
MSH_CMD_EXPORT_ALIAS(esp32_at_shell, esp32_at, Send AT command to ESP32 via SDIO);
MSH_CMD_EXPORT_ALIAS(esp32_wifi_connect, esp32_wifi, Connect ESP32 to WiFi network);

#endif /* BSP_USING_ESP32_SDIO */