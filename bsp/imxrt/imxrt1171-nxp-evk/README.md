# IMXRT1171-NXP-EVK BSP

## Introduction

This BSP (Board Support Package) is for the NXP IMXRT1171 evaluation board with ESP32 SDIO support for AT command communication.

### Key Features

- **MCU**: IMXRT1171DVMAA (ARM Cortex-M7, up to 1 GHz)
- **Memory**: 2MB OCRAM, 64MB SDRAM, 128MB QSPI Flash
- **ESP32 SDIO Interface**: Dedicated SDIO interface for ESP32 WiFi module communication
- **AT Command Support**: Full AT command set support over SDIO
- **High-Speed Communication**: Up to 25MHz SDIO clock for fast data transfer

## Hardware Connections

### ESP32 SDIO Connections

| IMXRT1171 Pin | ESP32 Pin | Function |
|---------------|-----------|----------|
| GPIO_SD_B2_00 | GPIO2     | SDIO_CMD |
| GPIO_SD_B2_01 | GPIO14    | SDIO_CLK |
| GPIO_SD_B2_02 | GPIO15    | SDIO_D0  |
| GPIO_SD_B2_03 | GPIO13    | SDIO_D1  |
| GPIO_SD_B2_04 | GPIO12    | SDIO_D2  |
| GPIO_SD_B2_05 | GPIO4     | SDIO_D3  |
| GPIO_AD_24    | EN        | ESP32_EN |
| GPIO_AD_25    | RST       | ESP32_RST|

### Power Supply

- ESP32: 3.3V
- IMXRT1171: 3.3V I/O, 1.8V Core

## Software Configuration

### Menuconfig Options

Enable the following options in menuconfig:

```
Hardware Drivers Config  --->
    On-chip Peripheral Drivers  --->
        [*] Enable SDIO
        [*]   Enable SDIO1 for ESP32
        [*] Enable ESP32 over SDIO
```

### Build Configuration

The BSP automatically configures:
- USDHC2 for ESP32 SDIO communication
- GPIO pins for SDIO interface
- ESP32 control pins (Reset and Enable)
- 25MHz SDIO clock frequency

## Usage Examples

### 1. Basic AT Command Demo

Run the built-in demo to test ESP32 communication:

```bash
msh> esp32_sdio_demo
```

This will:
- Initialize the ESP32 SDIO interface
- Send basic AT commands
- Get firmware version
- Scan WiFi networks
- Demonstrate connection process

### 2. Interactive AT Commands

Send custom AT commands to ESP32:

```bash
msh> esp32_at AT+GMR
msh> esp32_at AT+CWLAP
msh> esp32_at "AT+CWJAP=\"MyWiFi\",\"MyPassword\""
```

### 3. WiFi Connection Helper

Connect to a WiFi network easily:

```bash
msh> esp32_wifi MySSID MyPassword
```

### 4. Programming Interface

Use the ESP32 SDIO device in your applications:

```c
#include <rtthread.h>
#include <rtdevice.h>

/* AT command structure */
struct esp32_at_cmd {
    const char *cmd;
    char *response;
    rt_size_t resp_len;
};

void my_esp32_function(void)
{
    rt_device_t esp32_dev;
    char response[512];
    struct esp32_at_cmd at_cmd;
    
    /* Find and open ESP32 device */
    esp32_dev = rt_device_find("esp32_sdio");
    if (esp32_dev == RT_NULL) return;
    
    rt_device_open(esp32_dev, RT_DEVICE_FLAG_RDWR);
    
    /* Send AT command */
    at_cmd.cmd = "AT+GMR";
    at_cmd.response = response;
    at_cmd.resp_len = sizeof(response);
    
    rt_device_control(esp32_dev, RT_DEVICE_CTRL_CUSTOM + 1, &at_cmd);
    
    rt_kprintf("ESP32 Response: %s\n", response);
    
    rt_device_close(esp32_dev);
}
```

## ESP32 Firmware Requirements

### SDIO AT Firmware

The ESP32 must be flashed with SDIO-compatible AT firmware. You can:

1. **Use Pre-built AT Firmware**: Download from Espressif's AT firmware releases
2. **Build Custom Firmware**: Use ESP-IDF with AT command components and SDIO transport

### Recommended AT Firmware Configuration

```
CONFIG_AT_ENABLE=y
CONFIG_AT_SDIO_ENABLE=y
CONFIG_AT_WIFI_COMMAND_SUPPORT=y
CONFIG_AT_SOCKET_COMMAND_SUPPORT=y
```

## Supported AT Commands

The implementation supports all standard ESP32 AT commands including:

### Basic Commands
- `AT` - Test AT startup
- `AT+RST` - Restart module
- `AT+GMR` - View version info
- `AT+SLEEP` - Enter deep sleep

### WiFi Commands
- `AT+CWMODE` - Set WiFi mode
- `AT+CWJAP` - Connect to AP
- `AT+CWLAP` - List available APs
- `AT+CWQAP` - Disconnect from AP
- `AT+CIFSR` - Get local IP address

### TCP/IP Commands
- `AT+CIPSTART` - Establish TCP/UDP connection
- `AT+CIPSEND` - Send data
- `AT+CIPCLOSE` - Close connection
- `AT+CIPMUX` - Enable multiple connections

## Performance Characteristics

- **SDIO Clock**: 25 MHz
- **Data Transfer Rate**: Up to 12.5 MB/s theoretical
- **AT Command Latency**: < 10ms typical
- **WiFi Scan Time**: 1-3 seconds
- **Connection Establishment**: 2-5 seconds

## Troubleshooting

### Common Issues

1. **ESP32 Not Responding**
   - Check power supply (3.3V stable)
   - Verify SDIO pin connections
   - Ensure ESP32 has SDIO AT firmware

2. **SDIO Communication Errors**
   - Check signal integrity on SDIO lines
   - Verify pull-up resistors on CMD and DATA lines
   - Reduce SDIO clock if needed

3. **AT Command Timeouts**
   - Increase timeout values in driver
   - Check ESP32 firmware compatibility
   - Verify command syntax

### Debug Options

Enable debug output by defining `ESP32_SDIO_DEBUG` in the driver:

```c
#define ESP32_SDIO_DEBUG
```

This will provide detailed logging of:
- SDIO transactions
- AT command exchanges
- Error conditions
- Timing information

## Development Notes

### Driver Architecture

```
Application Layer
    ↓
ESP32 SDIO Driver (drv_esp32_sdio.c)
    ↓
USDHC HAL (NXP SDK)
    ↓
IMXRT1171 Hardware
```

### Key Files

- `board/drv_esp32_sdio.c` - ESP32 SDIO driver implementation
- `board/board.c` - Pin configuration and hardware setup
- `applications/esp32_sdio_demo.c` - Demo and test applications
- `board/Kconfig` - Configuration options

### Future Enhancements

- DMA-based data transfers for higher performance
- Interrupt-driven SDIO communication
- Power management integration
- Advanced error recovery mechanisms

## License

This BSP is released under the Apache 2.0 license. See the LICENSE file for details.

## Support

For technical support and questions:
- RT-Thread Community Forum
- GitHub Issues
- NXP Community Forums

---

**Note**: This BSP requires RT-Thread 4.1.0 or later and is specifically designed for IMXRT1171 with ESP32 SDIO communication.