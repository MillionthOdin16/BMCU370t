# BMCU370 Firmware

**Bambu Multi-Color Unit (BMCU) - CH32V203 Microcontroller Firmware**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#building)
[![Dev Build](https://img.shields.io/badge/dev%20build-automated-blue)](#dev-builds)
[![License](https://img.shields.io/badge/license-see%20LICENSE-blue)](./LICENSE)
[![Platform](https://img.shields.io/badge/platform-CH32V203-orange)](#hardware)

This is the latest BMCU Xing-C modified version (BMCU-C Hall sensor version V0.1-0020) source code with personal optimizations. Original project: [Xing-C/BMCU370x](https://github.com/Xing-C/BMCU370x).

## Overview

The BMCU370 is a sophisticated multi-material 3D printing controller designed for Bambu Lab printers. This firmware provides:

- **Multi-channel filament management** (4 channels)
- **Automatic Material System (AMS)** compatibility
- **Hall sensor-based position feedback** using AS5600 sensors
- **Automatic motor direction detection** during filament feeding operations
- **RGB LED status indication** with NeoPixel strips
- **BambuBus protocol communication** with printers
- **Non-volatile storage** for filament profiles and settings
- **Real-time motion control** with feedback loops

## Hardware Specifications

- **Microcontroller**: CH32V203C8T6 (RISC-V core, 144MHz)
- **Memory**: 20KB RAM, 64KB Flash
- **Communication**: UART-based BambuBus protocol
- **Sensors**: 4x AS5600 Hall sensors for rotary position
- **Actuators**: 4-channel PWM motor control
- **Indicators**: RGB LED strips (NeoPixel compatible)
- **ADC**: 8-channel DMA-enabled analog input
- **Schematics**: Official PCB schematics available in [`docs/hardware/`](docs/hardware/)

## Resources

- **English Wiki**: https://wiki.yuekai.fr/
- **中文Wiki**: https://bmcu.wanzii.cn/
- **BambuBus Protocol Documentation**: [Bambu-Research-Group/Bambu-Bus](https://github.com/Bambu-Research-Group/Bambu-Bus)
  - Comprehensive BambuBus protocol specifications and tools
  - Packet parsing utilities and simulators
  - Research data from the Bambu Research Group

## Documentation

📚 **Comprehensive documentation is available in the [`docs/`](docs/) directory:**

- **[Quick Start Guide](docs/firmware/DEV-BUILD-QUICK-START.md)** - Get started in 2 minutes
- **[API Documentation](docs/firmware/API.md)** - Complete firmware API reference
- **[Hardware Guide](docs/hardware/HARDWARE.md)** - Hardware specifications and setup
- **[Assembly Instructions](docs/assembly/)** - Physical assembly documentation
- **[CI/CD Guide](docs/firmware/CI-CD.md)** - Build automation and deployment

For a complete overview of available documentation, see the [docs README](docs/README.md).

## Building

### Prerequisites

1. **PlatformIO IDE** or **PlatformIO CLI**
   ```bash
   pip install platformio
   ```

2. **CH32V Development Platform** (automatically installed by PlatformIO)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/MillionthOdin16/BMCU370.git
cd BMCU370

# Build the firmware
pio run

# Upload to device (requires ST-Link or compatible programmer)
pio run --target upload
```

### Project Structure

```
src/
├── main.cpp/h           # Main application and RGB control
├── BambuBus.cpp/h       # Communication protocol implementation
├── Motion_control.cpp/h # Filament motion and motor control
├── Flash_saves.cpp/h    # Non-volatile storage management
├── ADC_DMA.cpp/h        # ADC with DMA for sensor readings
├── Debug_log.cpp/h      # Debug logging system
├── many_soft_AS5600.cpp/h # Hall sensor interface
├── Adafruit_NeoPixel.cpp/h # RGB LED control library
└── time64.cpp/h         # 64-bit timestamp utilities

docs/hardware/
├── HARDWARE.md          # Hardware configuration guide
├── SCH_Schematic1_2025-09-11.pdf # Complete PCB schematic
└── SCH_Schematic1_1_2025-09-11.pdf # PCB schematic sheet 1
```

## Dev Builds

🚀 **Automated Dev Builds** are now available! Every commit and pull request automatically triggers a development build with:

- ⚡ **Fast builds** with comprehensive caching (50-80% faster)
- 📝 **Automatic changelog** generation since last build
- 💬 **PR comments** with build information and changelog
- 📦 **Build artifacts** ready for download and testing

### Quick Start:

1. **Push code** → Dev build triggers automatically
2. **Check GitHub Actions** for build status  
3. **Download artifacts** from Actions page
4. **Review changelog** in PR comments (for PRs)

### Artifacts Available:
- **Firmware binary** (`.bin`) - Ready to flash
- **ELF file** (`.elf`) - Debug symbols  
- **Build info** - Complete metadata
- **Changelog** - What changed since last build
- **Checksums** - SHA256 verification

📚 **Documentation:**
- [Quick Start Guide](docs/firmware/DEV-BUILD-QUICK-START.md) - Get started in 2 minutes
- [Complete CI/CD Guide](docs/firmware/CI-CD.md) - Full documentation and best practices

---

## Configuration

### Hardware Configuration

Key hardware parameters are defined in `main.h`:
- **LED_PA11_NUM**: Number of LEDs on channel PA11 (default: 2)
- **LED_PA8_NUM**: Number of LEDs on channel PA8 (default: 2)
- **LED_PB1_NUM**: Number of LEDs on channel PB1 (default: 2)
- **LED_PB0_NUM**: Number of LEDs on channel PB0 (default: 2)
- **LED_PD1_NUM**: Main board status LED count (default: 1)

📋 **For detailed pin assignments and electrical specifications, see the [Hardware Guide](docs/hardware/HARDWARE.md) and [PCB schematics](docs/hardware/).**

### Filament Settings

Each filament channel supports:
- **Material Type**: Default PETG (customizable via NFC)
- **Temperature Range**: 220-240°C (default)
- **Color Information**: RGBA values for visual identification
- **Length Tracking**: Precise filament usage measurement

## Features

### Multi-Channel Filament Management
- Supports up to 4 filament channels simultaneously
- Individual channel status monitoring via RGB indicators
- Automatic filament detection and insertion assistance

### Communication Protocol
- **BambuBus Protocol**: Custom UART-based communication with Bambu printers (1,228,800 bps)
- **Packet Formats**: Supports both long and short header packets with CRC verification
- **Device Addressing**: Multi-device communication with specific address assignments
- **CRC Error Detection**: Built-in data integrity checking using CRC8/CRC16
- **Hot-swap Support**: Dynamic device detection and configuration

### Sensor Integration
- **Hall Sensors (AS5600)**: Magnetic rotary position sensors for each channel
- **Pressure Sensors**: Filament insertion/removal detection
- **Temperature Monitoring**: Real-time temperature feedback

### LED Status System
- **Channel Status**: Individual RGB indicators per filament channel
  - 🔴 Red: Offline/Error
  - 🔵 Blue: Standby/Low pressure
  - 🟢 Green: Active/Normal operation
  - 🟡 Yellow: Material loading/unloading
- **Main Board Status**: System-wide status indication
  - 🔴 Red breathing: Not connected to printer
  - ⚪ White breathing: Normal operation

## Current Release

### Version 2.0.0 (September 4, 2024)
**🚀 Complete firmware rewrite with major enhancements**

- ✅ **Complete architecture rewrite** with modular design and improved maintainability
- ✅ **Automated CI/CD pipeline** with GitHub Actions for reliable builds
- ✅ **PlatformIO integration** for modern development workflow
- ✅ **Comprehensive documentation** including API and hardware guides
- ✅ **Configurable firmware versioning** for AMS compatibility management
- ✅ **Enhanced BambuBus protocol** with improved error handling and CRC validation
- ✅ **Debug logging system** with configurable levels
- ✅ **Optimized memory usage** (57.6% flash, 46.5% RAM)
- ✅ **Cross-platform build support** via PlatformIO
- ✅ **Automated release management** with build artifacts and documentation

[**📥 Download Latest Release**](https://github.com/MillionthOdin16/BMCU370/releases/latest)

## Previous Release History

### Version 0020 (July 17, 2025) - Original krrr/BMCU370
- ✅ Fixed lighting logic errors causing some states not to illuminate
- ✅ Fixed unexpected channel online issues
- ✅ Corrected anti-disconnect logic (was previously ineffective)
- ✅ Rewrote lighting system, fixed flickering issues, reduced refresh rate
- ✅ Added 3-second red light retry for channel errors during operation

### Version 0019 (July 6, 2025) - Original krrr/BMCU370
- ✅ Compatible with dual micro-switch Hall sensor versions
- ✅ P1X1 printer now supports 16-color configuration
- ✅ Fixed filament information saving issues with latest firmware (00.01.06.62)
- ✅ Improved online logic detection to prevent false channel online states
- ✅ Enhanced motor control logic with voltage-specific handling
- ✅ Reduced buffer and main board LED brightness for better thermal management
- ✅ Removed A1 control dependency in material retraction

For complete version history, see [CHANGELOG.md](CHANGELOG.md).

## Firmware Version Configuration

### Critical Importance

The firmware version reported by the BMCU370 is **essential for printer compatibility**. Bambu Lab printers query the AMS firmware version during initialization and may **reject AMS units with incompatible firmware versions**. This can result in the printer not recognizing the AMS at all.

### Current Firmware Versions

The firmware reports different versions depending on the detected device type:

- **AMS (8-channel)**: Version `00.00.06.49`
- **AMS Lite**: Version `00.01.02.03`

### Version Configuration

Firmware versions are configured in `src/config.h`:

```c
// AMS (8-channel) Firmware Version
#define AMS_FIRMWARE_VERSION_MAJOR      0x00
#define AMS_FIRMWARE_VERSION_MINOR      0x00  
#define AMS_FIRMWARE_VERSION_PATCH      0x06
#define AMS_FIRMWARE_VERSION_BUILD      0x31    // 0x31 = 49 decimal

// AMS Lite Firmware Version  
#define AMS_LITE_FIRMWARE_VERSION_MAJOR 0x00
#define AMS_LITE_FIRMWARE_VERSION_MINOR 0x01
#define AMS_LITE_FIRMWARE_VERSION_PATCH 0x02
#define AMS_LITE_FIRMWARE_VERSION_BUILD 0x03
```

### How Version Reporting Works

1. **Printer Query**: The printer sends a BambuBus packet (type `0x103`) requesting version information
2. **Device Response**: BMCU370 responds with the appropriate version based on its detected device type (AMS vs AMS Lite)
3. **Compatibility Check**: The printer validates the version against its internal compatibility list
4. **Result**: Compatible versions allow normal operation; incompatible versions may cause rejection

### Updating Firmware Versions

⚠️ **Warning**: Only change firmware versions if you understand the compatibility requirements.

To update firmware versions:

1. Edit the version constants in `src/config.h`
2. Rebuild and flash the firmware
3. Test with your specific printer model and firmware version

### Version Format Details

Versions are transmitted as 4-byte arrays in little-endian format:
- Byte 0: Build number (LSB)  
- Byte 1: Patch version
- Byte 2: Minor version
- Byte 3: Major version (MSB)

Example: Version `00.00.06.49` is transmitted as `{0x31, 0x06, 0x00, 0x00}`

### Automatic Builds

This repository includes GitHub Actions workflows that automatically build firmware when new tags are created. To trigger a new firmware build:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow will:
- Build the firmware for CH32V203C8T6
- Create a GitHub release with firmware binaries
- Include build information and flashing instructions

## Troubleshooting

### Common Issues

**Problem**: LEDs not lighting up correctly
- **Cause**: Channel detection error or power supply issue
- **Solution**: Check connections and power supply voltage (5V required for RGB LEDs)

**Problem**: Filament not feeding properly
- **Cause**: Hall sensor calibration or mechanical obstruction
- **Solution**: Verify AS5600 sensor positions and clear filament path

**Problem**: Motor direction reversed on channels 1 and 2 (and sometimes 3)
- **Cause**: Inconsistent magnet polarity orientation during assembly affecting AS5600 Hall sensor readings
- **Solution**: ✅ **Fixed in firmware v2.0.0+** - Automatic direction learning during normal filament loading
- **Details**: See [Automatic Direction Detection Documentation](docs/firmware/AUTOMATIC_DIRECTION_DETECTION.md)

**Problem**: Communication timeout with printer
- **Cause**: BambuBus protocol error or cable issue
- **Solution**: Check UART connections and baud rate settings (115200)

### Debug Output

Enable debug logging by ensuring `Debug_log_on` is defined in `Debug_log.h`:
```cpp
#define Debug_log_on
```

Connect UART at 115200 baud to view debug messages.