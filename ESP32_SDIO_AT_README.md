# ESP32 SDIO AT Command Driver for RT-Thread

This implementation provides a complete SDIO-based AT command interface for ESP32 modules running on RT-Thread OS. It enables WiFi communication and network operations through AT commands sent over the SDIO bus.

## Features

- **SDIO Communication**: Direct SDIO interface to ESP32 without UART
- **AT Command Support**: Full AT command set for WiFi and TCP operations
- **Interrupt Handling**: Efficient interrupt-driven communication
- **Thread Safety**: Mutex-protected command execution
- **Error Handling**: Comprehensive error detection and recovery
- **Statistics**: Command success/failure tracking
- **Debug Support**: Configurable debug output levels
- **Auto-detection**: Automatic ESP32 card detection and initialization

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Application   │    │   Example App    │    │   MSH Commands  │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32 SDIO AT Driver                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  Command Layer  │  │  Response Parser│  │  Statistics     │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  SDIO Interface │  │  IRQ Handler    │  │  Buffer Mgmt    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                 │
┌─────────────────────────────────────────────────────────────────┐
│                    RT-Thread SDIO Framework                     │
└─────────────────────────────────────────────────────────────────┘
                                 │
┌─────────────────────────────────────────────────────────────────┐
│                         Hardware                                │
│                    (SDIO Controller)                            │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

```
components/drivers/sdio/
├── drv_esp32_sdio_at.h          # Header file with API definitions
├── drv_esp32_sdio_at.c          # Main driver implementation
├── drv_esp32_sdio_probe.c       # Auto-detection and registration
├── Kconfig                      # Configuration options
└── SConscript                   # Build configuration

examples/
└── esp32_sdio_at_example.c      # Example application
```

## Configuration

### Kconfig Options

Enable the driver in RT-Thread configuration:

```kconfig
CONFIG_RT_USING_SDIO=y
CONFIG_RT_USING_ESP32_SDIO_AT=y
CONFIG_RT_ESP32_SDIO_AT_BUFFER_SIZE=512
CONFIG_RT_ESP32_SDIO_AT_CMD_TIMEOUT=5000
CONFIG_RT_ESP32_SDIO_AT_RESP_TIMEOUT=10000
CONFIG_RT_ESP32_SDIO_AT_MAX_RETRY=3
CONFIG_RT_ESP32_SDIO_AT_DEBUG=y
CONFIG_RT_ESP32_SDIO_AT_EXAMPLE=y
```

### Hardware Requirements

- **SDIO Interface**: 4-bit SDIO bus connection
- **ESP32 Module**: ESP32 with SDIO AT firmware
- **Power Supply**: Stable 3.3V power supply
- **Clock**: SDIO clock up to 50MHz

### Pin Connections

| SDIO Signal | ESP32 Pin | Description |
|-------------|-----------|-------------|
| SDIO_CLK    | GPIO14    | Clock signal |
| SDIO_CMD    | GPIO15    | Command line |
| SDIO_D0     | GPIO2     | Data line 0 |
| SDIO_D1     | GPIO4     | Data line 1 |
| SDIO_D2     | GPIO12    | Data line 2 |
| SDIO_D3     | GPIO13    | Data line 3 |

## API Reference

### Core Functions

```c
/* Initialize the driver */
rt_err_t esp32_sdio_at_init(struct rt_mmcsd_card *card);

/* Send AT command */
rt_err_t esp32_sdio_at_send_cmd(esp32_at_cmd_t *cmd);

/* Send simple AT command */
rt_err_t esp32_sdio_at_send_simple(const char *cmd_str, char *resp_buf, 
                                   rt_size_t resp_size, rt_uint32_t timeout);

/* Check if ready */
rt_bool_t esp32_sdio_at_is_ready(void);

/* Get statistics */
void esp32_sdio_at_get_stats(rt_uint32_t *cmd_count, rt_uint32_t *error_count, 
                            rt_uint32_t *timeout_count);

/* Reset driver */
rt_err_t esp32_sdio_at_reset(void);

/* Deinitialize */
rt_err_t esp32_sdio_at_deinit(void);
```

### Data Structures

```c
/* AT Command Structure */
typedef struct {
    char *command;                      /* AT command string */
    char *response;                     /* Response buffer */
    rt_size_t resp_size;               /* Response buffer size */
    rt_uint32_t timeout;               /* Command timeout in ms */
    esp32_at_resp_type_t resp_type;    /* Expected response type */
} esp32_at_cmd_t;

/* Response Types */
typedef enum {
    ESP32_AT_RESP_OK = 0,
    ESP32_AT_RESP_ERROR,
    ESP32_AT_RESP_FAIL,
    ESP32_AT_RESP_TIMEOUT,
    ESP32_AT_RESP_DATA,
    ESP32_AT_RESP_UNKNOWN
} esp32_at_resp_type_t;
```

## Usage Examples

### Basic AT Command

```c
#include "drv_esp32_sdio_at.h"

void test_basic_at(void)
{
    char response[256];
    rt_err_t ret;
    
    /* Test basic AT command */
    ret = esp32_sdio_at_send_simple("AT", response, sizeof(response), 5000);
    if (ret == RT_EOK) {
        rt_kprintf("Response: %s\n", response);
    }
}
```

### WiFi Connection

```c
void connect_wifi(const char *ssid, const char *password)
{
    char command[128];
    char response[256];
    rt_err_t ret;
    
    /* Set WiFi mode to Station */
    ret = esp32_sdio_at_send_simple("AT+CWMODE=1", response, sizeof(response), 5000);
    if (ret != RT_EOK) {
        rt_kprintf("Failed to set WiFi mode\n");
        return;
    }
    
    /* Connect to WiFi */
    rt_snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    ret = esp32_sdio_at_send_simple(command, response, sizeof(response), 15000);
    if (ret == RT_EOK) {
        rt_kprintf("WiFi connected: %s\n", response);
    } else {
        rt_kprintf("WiFi connection failed\n");
    }
}
```

### TCP Communication

```c
void tcp_test(void)
{
    char response[512];
    rt_err_t ret;
    
    /* Connect to TCP server */
    ret = esp32_sdio_at_send_simple("AT+CIPSTART=\"TCP\",\"httpbin.org\",80", 
                                   response, sizeof(response), 10000);
    if (ret != RT_EOK) {
        rt_kprintf("TCP connection failed\n");
        return;
    }
    
    /* Send HTTP request */
    const char *http_req = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";
    char send_cmd[64];
    rt_snprintf(send_cmd, sizeof(send_cmd), "AT+CIPSEND=%d", rt_strlen(http_req));
    
    ret = esp32_sdio_at_send_simple(send_cmd, response, sizeof(response), 5000);
    if (ret == RT_EOK) {
        ret = esp32_sdio_at_send_simple(http_req, response, sizeof(response), 10000);
        rt_kprintf("HTTP Response: %s\n", response);
    }
    
    /* Close connection */
    esp32_sdio_at_send_simple("AT+CIPCLOSE", response, sizeof(response), 5000);
}
```

### Advanced Command Structure

```c
void advanced_command_example(void)
{
    esp32_at_cmd_t cmd;
    char response[256];
    
    /* Prepare command structure */
    rt_memset(&cmd, 0, sizeof(cmd));
    cmd.command = "AT+GMR";
    cmd.response = response;
    cmd.resp_size = sizeof(response);
    cmd.timeout = 5000;
    
    /* Send command */
    rt_err_t ret = esp32_sdio_at_send_cmd(&cmd);
    if (ret == RT_EOK) {
        rt_kprintf("Version info: %s\n", response);
        rt_kprintf("Response type: %d\n", cmd.resp_type);
    }
}
```

## MSH Commands

The driver provides several MSH (RT-Thread shell) commands for testing:

```bash
# Start example application
esp32_example_start

# Stop example application  
esp32_example_stop

# Send AT command manually
esp32_at_test AT+GMR

# Show driver statistics
esp32_stats

# Reset ESP32 module
esp32_reset

# Show probe information
esp32_probe_info
```

## Common AT Commands

| Command | Description | Example |
|---------|-------------|---------|
| `AT` | Test command | `AT` |
| `AT+RST` | Reset module | `AT+RST` |
| `AT+GMR` | Get version | `AT+GMR` |
| `AT+CWMODE=<mode>` | Set WiFi mode | `AT+CWMODE=1` |
| `AT+CWJAP="<ssid>","<pwd>"` | Join AP | `AT+CWJAP="MyWiFi","password"` |
| `AT+CWQAP` | Quit AP | `AT+CWQAP` |
| `AT+CIPSTATUS` | Get connection status | `AT+CIPSTATUS` |
| `AT+CIPSTART="<type>","<addr>",<port>` | Start connection | `AT+CIPSTART="TCP","google.com",80` |
| `AT+CIPSEND=<length>` | Send data | `AT+CIPSEND=10` |
| `AT+CIPCLOSE` | Close connection | `AT+CIPCLOSE` |

## Troubleshooting

### Common Issues

1. **Driver Not Ready**
   - Check SDIO hardware connections
   - Verify ESP32 power supply
   - Enable debug output: `esp32_sdio_at_set_debug(3)`

2. **Command Timeout**
   - Increase timeout values in configuration
   - Check ESP32 firmware compatibility
   - Verify SDIO clock frequency

3. **Connection Failures**
   - Check WiFi credentials
   - Verify network availability
   - Reset ESP32: `esp32_reset`

4. **Build Errors**
   - Enable required Kconfig options
   - Check include paths
   - Verify RT-Thread version compatibility

### Debug Output

Enable debug output to troubleshoot issues:

```c
esp32_sdio_at_set_debug(3);  // 0=off, 1=error, 2=info, 3=debug
```

### Statistics Monitoring

Monitor driver performance:

```c
rt_uint32_t cmd_count, error_count, timeout_count;
esp32_sdio_at_get_stats(&cmd_count, &error_count, &timeout_count);
rt_kprintf("Success rate: %.1f%%\n", 
           (float)(cmd_count - error_count - timeout_count) * 100.0f / cmd_count);
```

## Performance Considerations

- **Buffer Size**: Increase `RT_ESP32_SDIO_AT_BUFFER_SIZE` for large responses
- **Timeouts**: Adjust timeouts based on network conditions
- **Thread Priority**: IRQ thread runs at high priority for responsiveness
- **Memory Usage**: Driver allocates ~2KB for buffers and context

## Limitations

- Single ESP32 module support
- AT command firmware required on ESP32
- No concurrent command execution
- Limited to SDIO function 1 for AT commands

## Contributing

When contributing to this driver:

1. Follow RT-Thread coding standards
2. Add appropriate error handling
3. Update documentation for new features
4. Test with different ESP32 firmware versions
5. Maintain backward compatibility

## License

This driver is licensed under Apache 2.0 license, same as RT-Thread OS.

## Support

For issues and questions:
- Check RT-Thread documentation
- Review ESP32 AT command documentation
- Enable debug output for troubleshooting
- Check hardware connections and power supply