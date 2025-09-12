# BMCU370 Development Guide

This guide covers development setup, building, and deployment for the BMCU370 USB-ESP interface project.

## Project Structure

```
BMCU370/
├── src/                          # BMCU370 firmware source code
│   ├── usb_protocol.cpp         # USB communication protocol
│   ├── usb_status_api.cpp       # JSON status reporting API
│   └── main.cpp                 # Main BMCU370 application
├── esp32_firmware/              # ESP32-S3 firmware
│   ├── src/                     # ESP32 source code
│   ├── data/                    # Web interface files
│   └── platformio.ini           # ESP32 build configuration
├── .github/workflows/           # CI/CD workflows
├── build.sh                     # Local build script
└── platformio.ini               # BMCU370 build configuration
```

## Development Setup

### Prerequisites

1. **PlatformIO CLI**:
   ```bash
   pip install platformio
   ```

2. **Git** (for version control)

3. **Hardware Tools**:
   - DFU programmer for BMCU370 firmware updates
   - ESP32-S3 development board with USB-OTG support
   - esptool.py for ESP32 flashing

### Quick Start

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd BMCU370
   ```

2. **Build all firmware**:
   ```bash
   ./build.sh
   ```

3. **Build individual targets**:
   ```bash
   # BMCU370 only
   pio run --environment genericCH32V203C8T6
   
   # ESP32 only
   cd esp32_firmware
   pio run --environment esp32s3
   ```

## Firmware Architecture

### BMCU370 Firmware Features

- **USB CDC-ACM Communication**: Real-time status reporting and configuration
- **DFU Mode Preservation**: Maintains firmware update capability
- **JSON Status API**: Structured data export for all system parameters
- **Command Protocol**: Remote configuration and control interface
- **Pin Conflict Resolution**: Smart LED channel management with USB

### ESP32 Firmware Features

- **USB Host Interface**: Automatic BMCU370 device enumeration
- **Web Server**: Modern responsive interface with real-time updates
- **WiFi Management**: AP mode setup and network configuration
- **Historical Data**: 24+ hour sensor logging with trend analysis
- **OTA Updates**: Web-based firmware update system

## Build Configuration

### BMCU370 Build Flags

```ini
build_flags= 
    -D SYSCLK_FREQ_144MHz_HSI=144000000
    -D USB_CDC_ENABLED=1              # Enable USB CDC interface
    -D USB_DFU_DUAL_MODE=1            # Preserve DFU functionality
```

### ESP32 Build Flags

```ini
build_flags = 
    -DARDUINO_USB_MODE=1              # Enable USB OTG mode
    -DCONFIG_TINYUSB_CDC_ENABLED=1    # Enable CDC-ACM host
    -DCORE_DEBUG_LEVEL=3              # Debug output
    -DBMCU370_INTERFACE_VERSION=\"1.0.0\"
```

## Memory Usage

### Current Usage (Build Status)

- **BMCU370**: 68.3% Flash (44,764/65,536 bytes), 63.6% RAM (13,024/20,480 bytes)
- **ESP32**: 71.0% Flash (930,777/1,310,720 bytes), 15.9% RAM (51,972/327,680 bytes)

Both firmwares maintain excellent memory headroom for future features.

## Development Workflow

### 1. Local Development

1. Make changes to source code
2. Build and test locally:
   ```bash
   ./build.sh
   ```
3. Flash to hardware for testing
4. Commit changes

### 2. CI/CD Pipeline

GitHub Actions automatically:
- Builds both firmware targets on every PR
- Runs memory usage analysis
- Creates firmware artifacts for download
- Packages complete release bundles

### 3. Release Process

1. Tag release: `git tag v1.0.0`
2. Push tag: `git push origin v1.0.0`
3. Download artifacts from GitHub Actions
4. Flash to production hardware

## Hardware Deployment

### BMCU370 Flashing

```bash
# Enter DFU mode (hold BOOT button during power-on)
dfu-util -a 0 -D bmcu370_firmware.bin
```

### ESP32 Flashing

```bash
# Complete flash (first time)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z \
  0x0000 esp32_bootloader.bin \
  0x8000 esp32_partitions.bin \
  0x10000 esp32_firmware.bin \
  0x110000 esp32_littlefs.bin

# Firmware update only
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z 0x10000 esp32_firmware.bin
```

## Network Setup

### First-Time WiFi Configuration

1. Power on ESP32 (creates AP "BMCU370-Setup")
2. Connect to AP (password: `bmcu370setup`)
3. Navigate to http://192.168.4.1
4. Configure WiFi credentials
5. Access main interface at http://esp32-bmcu370.local

### Web Interface Features

- **Dashboard**: Real-time system monitoring
- **Configuration**: LED, voltage, motion parameter controls
- **Diagnostics**: System statistics and log viewer
- **Network**: WiFi management and scanning
- **Updates**: OTA firmware update interface

## Troubleshooting

### Build Issues

1. **Platform not found**: Update PlatformIO platforms
   ```bash
   pio platform update
   ```

2. **Library dependencies**: Clean and rebuild
   ```bash
   pio run --target clean
   pio run
   ```

3. **Memory issues**: Check build flags and optimize code

### Hardware Issues

1. **BMCU370 not detected**: Check USB cable and DFU mode entry
2. **ESP32 connection**: Verify USB-OTG cable and power supply
3. **WiFi issues**: Reset network configuration via web interface

### Communication Issues

1. **USB enumeration**: Check USB host drivers on ESP32
2. **JSON parsing**: Verify BMCU370 status output format
3. **WebSocket disconnection**: Check network stability and reconnection logic

## Contributing

1. Fork the repository
2. Create feature branch: `git checkout -b feature/new-feature`
3. Make changes and test thoroughly
4. Submit pull request with detailed description
5. Ensure CI builds pass and memory usage is acceptable

## License

This project maintains the same license as the original BMCU370x project.