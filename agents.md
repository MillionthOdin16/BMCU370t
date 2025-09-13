# Agent Instructions for BMCU370 Project

This document provides instructions for agents working on the BMCU370 firmware project.

## 1. Building the Firmware

To build both the BMCU370 and ESP32 firmware, use the provided build script:

```bash
./build.sh
```

This script will compile both firmwares and place the output files in the `build-output/` directory.

To build a specific firmware target:
- **BMCU370**: `pio run --environment genericCH32V203C8T6`
- **ESP32**: `cd esp32_firmware && pio run --environment esp32s3`

## 2. Caching

To significantly speed up subsequent builds, the following directories should be cached:

- `./.pio`: Caches the compiled object files and library dependencies for the BMCU370 firmware.
- `esp32_firmware/.pio`: Caches the compiled object files and library dependencies for the ESP32 firmware.
- The PlatformIO global cache directory, which is typically `~/.platformio`.

**Do not** cache the `build-output/` directory, as it should contain the artifacts from the most recent build.

## 3. Testing

There is currently no automated test suite for this project. All changes should be validated through code review and building.

## 4. Flashing the Firmware

The `build.sh` script and the `README.md` file contain instructions for flashing the firmware using `dfu-util` for the BMCU370 and `esptool.py` for the ESP32. These commands are for reference and should not be run in the development environment unless you have the hardware connected.
