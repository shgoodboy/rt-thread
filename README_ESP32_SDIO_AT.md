# ESP32 SDIO AT Command Driver for RT-Thread

This implementation provides a comprehensive SDIO-based AT command driver for ESP32 modules in RT-Thread OS, enabling WiFi communication and TCP/IP networking capabilities.

## Features

- **SDIO Communication**: High-speed SDIO interface for ESP32 communication
- **AT Command Support**: Full AT command set implementation for ESP32
- **WiFi Management**: Connect, disconnect, and manage WiFi connections
- **TCP/IP Networking**: Establish TCP connections and send/receive data
- **Thread-Safe Operations**: Mutex and semaphore protection for concurrent access
- **Response Handling**: Dedicated thread for asynchronous response processing
- **MSH Commands**: Interactive shell commands for testing and debugging
- **Example Application**: Comprehensive example demonstrating all features

## Architecture

### Driver Components

1. **Core Driver** (`drv_esp32_sdio_at.c`)
   - SDIO device registration and management
   - Low-level SDIO communication functions
   - Device initialization and configuration
   - Response handling thread

2. **AT API Layer** (`esp32_at_api.c`)
   - High-level AT command functions
   - WiFi management APIs
   - TCP/IP networking functions
   - Error handling and response parsing

3. **Header File** (`drv_esp32_sdio_at.h`)
   - Function prototypes and definitions
   - AT command constants
   - Configuration macros
   - Utility functions

4. **Example Application** (`esp32_sdio_at_example.c`)
   - Demonstration of all driver features
   - MSH command implementations
   - Test routines and usage examples

## Hardware Requirements

### SDIO Connection

Connect ESP32 to your RT-Thread board via SDIO interface:

```
RT-Thread Board    ESP32 Module
--------------     ------------
SDIO_CLK    <----> GPIO14 (CLK)
SDIO_CMD    <----> GPIO15 (CMD)
SDIO_D0     <----> GPIO2  (D0)
SDIO_D1     <----> GPIO4  (D1)
SDIO_D2     <----> GPIO12 (D2)
SDIO_D3     <----> GPIO13 (D3)
GND         <----> GND
3.3V        <----> 3.3V
```

### ESP32 Configuration

The ESP32 must be configured with SDIO AT firmware. Key configuration:
- SDIO interface enabled
- AT command support
- Vendor ID: 0x6666, Product ID: 0x1111 (configurable)
- Function 1: AT commands
- Function 2: Data transfer

## Software Configuration

### Kconfig Options

Enable the driver in RT-Thread configuration:

```
RT-Thread Components
└── Device Drivers
    └── Using SD/MMC device drivers
        └── Using ESP32 SDIO AT Command Driver
```

Available configuration options:

- `ESP32_SDIO_AT_BLOCK_SIZE`: SDIO block size (default: 512)
- `ESP32_SDIO_AT_MAX_RESPONSE`: Maximum response size (default: 2048)
- `ESP32_SDIO_AT_CMD_TIMEOUT`: Command timeout in ms (default: 5000)
- `ESP32_SDIO_AT_THREAD_STACK`: Response thread stack size (default: 2048)
- `ESP32_SDIO_AT_THREAD_PRIORITY`: Response thread priority (default: 20)

### Build Integration

The driver is automatically included when `RT_USING_ESP32_SDIO_AT` is enabled.

## API Reference

### Basic Functions

```c
/* Test ESP32 connectivity */
rt_err_t esp32_at_test(void);

/* Reset ESP32 module */
rt_err_t esp32_at_reset(void);

/* Get firmware version */
rt_err_t esp32_at_get_version(char *version, rt_size_t size);

/* Send custom AT command */
rt_err_t esp32_at_send_command(const char *cmd, char *response, 
                               rt_size_t resp_size, rt_uint32_t timeout);
```

### WiFi Management

```c
/* Set WiFi mode (STA/AP/STA+AP) */
rt_err_t esp32_at_set_wifi_mode(rt_uint8_t mode);

/* Connect to WiFi network */
rt_err_t esp32_at_wifi_connect(const char *ssid, const char *password);

/* Disconnect from WiFi */
rt_err_t esp32_at_wifi_disconnect(void);

/* Get WiFi status */
rt_err_t esp32_at_get_wifi_status(char *status, rt_size_t size);

/* Get IP information */
rt_err_t esp32_at_get_ip_info(char *ip_info, rt_size_t size);
```

### TCP/IP Networking

```c
/* Establish TCP connection */
rt_err_t esp32_at_tcp_connect(const char *host, rt_uint16_t port);

/* Send data via TCP */
rt_err_t esp32_at_tcp_send(const void *data, rt_size_t length);

/* Close TCP connection */
rt_err_t esp32_at_tcp_close(void);
```

## Usage Examples

### Basic ESP32 Test

```c
#include "drv_esp32_sdio_at.h"

void test_esp32_basic(void)
{
    rt_err_t err;
    char version[256];
    
    /* Test connectivity */
    err = esp32_at_test();
    if (err == RT_EOK) {
        rt_kprintf("ESP32 is responding\n");
    }
    
    /* Get version */
    err = esp32_at_get_version(version, sizeof(version));
    if (err == RT_EOK) {
        rt_kprintf("ESP32 version: %s\n", version);
    }
}
```

### WiFi Connection

```c
void connect_to_wifi(void)
{
    rt_err_t err;
    
    /* Set station mode */
    err = esp32_at_set_wifi_mode(ESP32_WIFI_MODE_STA);
    if (err != RT_EOK) {
        rt_kprintf("Failed to set WiFi mode\n");
        return;
    }
    
    /* Connect to network */
    err = esp32_at_wifi_connect("MyWiFi", "MyPassword");
    if (err == RT_EOK) {
        rt_kprintf("Connected to WiFi successfully\n");
    } else {
        rt_kprintf("WiFi connection failed\n");
    }
}
```

### TCP Communication

```c
void tcp_example(void)
{
    rt_err_t err;
    const char *data = "Hello, World!";
    
    /* Connect to server */
    err = esp32_at_tcp_connect("httpbin.org", 80);
    if (err != RT_EOK) {
        rt_kprintf("TCP connection failed\n");
        return;
    }
    
    /* Send HTTP request */
    const char *request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";
    err = esp32_at_tcp_send(request, strlen(request));
    if (err == RT_EOK) {
        rt_kprintf("HTTP request sent\n");
    }
    
    /* Close connection */
    esp32_at_tcp_close();
}
```

## MSH Commands

The driver provides interactive shell commands for testing:

```bash
# Basic commands
esp32_test          # Test ESP32 connectivity
esp32_rst           # Reset ESP32 module  
esp32_ver           # Get firmware version

# WiFi commands
esp32_mode <mode>   # Set WiFi mode (1=STA, 2=AP, 3=STA+AP)
esp32_connect <ssid> [password]  # Connect to WiFi
esp32_disconnect    # Disconnect from WiFi
esp32_status        # Get WiFi status
esp32_ip            # Get IP information

# TCP commands
esp32_tcp_conn <host> <port>  # Establish TCP connection
esp32_tcp_send <data>         # Send data via TCP
esp32_tcp_close               # Close TCP connection

# Demo command
esp32_demo          # Run full demonstration
```

## Error Handling

The driver uses RT-Thread standard error codes:

- `RT_EOK`: Success
- `-RT_ERROR`: General error
- `-RT_ETIMEOUT`: Operation timeout
- `-RT_EINVAL`: Invalid parameter
- `-RT_ENOMEM`: Out of memory

Check return values and handle errors appropriately in your application.

## Performance Considerations

### SDIO Configuration

- Use appropriate SDIO clock frequency for your hardware
- Ensure proper signal integrity for high-speed operation
- Consider power management for battery-powered applications

### Memory Usage

- Command buffer: 512 bytes (configurable)
- Response buffer: 2048 bytes (configurable)
- Thread stack: 2048 bytes (configurable)
- Total RAM usage: ~5KB + dynamic allocations

### Threading

- Response handling runs in dedicated thread
- All API calls are thread-safe
- Avoid blocking operations in interrupt context

## Troubleshooting

### Common Issues

1. **ESP32 not detected**
   - Check SDIO wiring connections
   - Verify ESP32 SDIO firmware
   - Check vendor/product ID configuration

2. **AT commands timeout**
   - Increase timeout values in configuration
   - Check ESP32 firmware compatibility
   - Verify SDIO clock frequency

3. **WiFi connection fails**
   - Check SSID and password
   - Verify WiFi network availability
   - Check ESP32 antenna connection

4. **TCP connection issues**
   - Ensure WiFi is connected first
   - Check network connectivity
   - Verify server address and port

### Debug Output

Enable debug output in configuration:
```c
#define ESP32_SDIO_AT_DEBUG 1
```

This provides detailed logging of SDIO operations and AT command exchanges.

## License

This driver is licensed under Apache-2.0, same as RT-Thread OS.

## Contributing

Contributions are welcome! Please follow RT-Thread coding standards and include appropriate tests for new features.

## Support

For issues and questions:
1. Check this documentation
2. Review example code
3. Enable debug output
4. Check RT-Thread community forums