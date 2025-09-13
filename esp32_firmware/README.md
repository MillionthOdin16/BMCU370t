# ESP32-S3 BMCU370 Web Interface

Complete web-based interface for BMCU370 filament monitoring via ESP32-S3 USB host communication.

## ESP32-S3 Hardware Requirements ✅

This firmware is **optimized specifically for ESP32-S3** with:
- **4MB Flash Memory** (confirmed by your hardware)  
- **2MB PSRAM** (OPI mode for enhanced buffer capacity)
- **USB OTG Support** (native ESP32-S3 capability for BMCU370 communication)
- **WiFi + Bluetooth** (dual-core processing)

Your hardware reports: `ESP32-S3 (QFN56) (rev 0.1), Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded Flash 4MB, XMC, Embedded PSRAM 2MB, AP_3v3` ✅

## Key ESP32-S3 Optimizations

### 🔧 **Hardware-Specific Configuration**
- **Correct board definition** for 4MB+PSRAM variant
- **DIO flash mode** at 80MHz for optimal performance  
- **OPI PSRAM support** with automatic cache management
- **USB OTG host mode** for BMCU370 CDC-ACM communication

### 🗂️ **Optimized Memory Layout** 
```
Flash Memory (4MB):
├── App0 (Primary): 1.5MB (plenty of room for features)
├── App1 (OTA):     1.5MB (full OTA update support)  
├── LittleFS:       832KB (web interface files)
└── NVS + Other:    ~160KB (configuration & system)

PSRAM (2MB):
└── Large JSON buffers, web interface caching, USB communication buffers
```

### 📦 **Memory Usage**
- **Flash**: 950KB/1536KB (61.0% - excellent headroom)
- **RAM**: 803KB dynamic + 320KB static 
- **PSRAM**: Available for expansion (historical data, large responses)

## Firmware Features

### Phase 2.1-2.2: USB Host Implementation ✅
- ESP32-S3 USB host interface
- BMCU370 device enumeration and communication
- CDC-ACM driver for serial communication

### Phase 2.3: Communication Layer ✅
- High-level BMCU370 interface
- JSON status parsing and validation
- Parameter configuration commands
- Error handling and retry logic

### Phase 2.4: Web Server Implementation ✅
- AsyncWebServer with REST API endpoints
- WebSocket for real-time updates
- Static file serving from LittleFS
- Rate limiting and error handling

### Phase 2.5: Web Interface Development ✅
- Responsive HTML5/CSS3 interface
- JavaScript for dynamic updates
- Real-time dashboard with channel details
- Configuration forms and system controls

## Directory Structure

```
esp32_firmware/
├── platformio.ini          # PlatformIO configuration
├── src/                     # Source code
│   ├── main.cpp            # Main application entry point
│   ├── config.h            # Configuration constants
│   ├── bmcu370_interface.h # BMCU370 USB interface
│   ├── bmcu370_interface.cpp
│   ├── wifi_manager.h      # WiFi management
│   ├── wifi_manager.cpp
│   ├── web_server.h        # Web server and API
│   └── web_server.cpp
└── data/                   # Web interface files
    ├── index.html          # Main web interface
    ├── css/
    │   └── style.css       # Styling
    └── js/
        └── app.js          # JavaScript application
```

## API Endpoints

### Status and Monitoring
- `GET /api/status` - Get BMCU370 status (JSON)
- `GET /api/config` - Get current configuration
- `GET /api/logs` - Get system logs and statistics

### Configuration
- `POST /api/config` - Update configuration parameter
  - Body: `key=value` (form data)

### System Control
- `POST /api/system` - Execute system commands
  - `action=reset_bmcu370` - Reset BMCU370 device
  - `action=dfu_mode` - Enter DFU mode for firmware update
  - `action=reset_esp32` - Reset ESP32

### Network Management
- `GET /api/wifi/scan` - Scan for WiFi networks
- `POST /api/wifi/connect` - Connect to WiFi network
  - Body: `ssid=network_name&password=password`

### WebSocket
- `ws://device_ip/ws` - Real-time status updates

## Web Interface

### Dashboard Tab
- System overview (version, uptime, BambuBus status)
- Filament channel cards with status indicators
- Detailed channel information (filament, motion, LEDs, sensors)

### Configuration Tab
- LED brightness controls (main and channel LEDs)
- Voltage thresholds configuration
- Motion parameter adjustment

### Diagnostics Tab
- System statistics and error counts
- System control buttons (reset, DFU mode)
- Recent log entries

### Network Tab
- WiFi status and connection information
- Network scanner
- WiFi connection form

## Building and Flashing

### Prerequisites
- PlatformIO IDE or CLI
- ESP32-S3 development board

### Build Commands
```bash
# Build firmware
pio run

# Upload firmware
pio run --target upload

# Upload filesystem (web interface)
pio run --target uploadfs

# Monitor serial output
pio device monitor
```

### Configuration
The firmware automatically:
1. Initializes USB host interface
2. Starts in WiFi AP mode if no saved credentials
3. Starts web server on port 80
4. Begins monitoring BMCU370 connection

### Default WiFi AP
- **SSID**: `BMCU370-Config`
- **Password**: `bmcu370setup`
- **IP**: `192.168.4.1`

## Connection Setup

1. **Power on ESP32-S3** with firmware flashed
2. **Connect USB-C cable** between ESP32-S3 and BMCU370
3. **Connect to WiFi network**:
   - Either connect ESP32 to existing WiFi via web interface
   - Or connect your device to ESP32's AP mode
4. **Access web interface** at ESP32's IP address
5. **Monitor BMCU370 status** and configure parameters

## Troubleshooting

### USB Connection Issues
- Ensure ESP32-S3 has native USB host capability
- Check USB-C cable connection
- Verify BMCU370 is in CDC mode (not DFU)
- Monitor serial output for USB enumeration messages

### WiFi Connection Issues
- Reset WiFi credentials via web interface
- Check WiFi network password
- Ensure ESP32 is within WiFi range
- Monitor serial output for connection status

### Web Interface Issues
- Clear browser cache
- Check ESP32 IP address
- Ensure web server is running (port 80)
- Upload filesystem with web interface files

## Development Notes

### Memory Usage
- **Flash**: ~200KB for firmware + ~50KB for web interface
- **RAM**: ~100KB during operation
- Optimized for ESP32-S3 with 8MB flash

### Real-time Updates
- WebSocket updates every 500ms when clients connected
- USB polling every 1 second
- Automatic reconnection on USB disconnect

### Future Enhancements
- Historical data logging
- OTA firmware updates
- MQTT integration
- Mobile app interface

## Version Information

- **ESP32 Firmware Version**: 1.0.0
- **BMCU370 Interface**: Compatible with Phase 1 USB implementation
- **Web Interface**: HTML5 responsive design
- **Framework**: Arduino/ESP-IDF via PlatformIO