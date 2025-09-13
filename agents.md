# Agent Instructions for BMCU370 Project

This document provides instructions for agents working on the BMCU370 firmware project.

## 1. Environment Setup

This project has a setup script that should be run once to configure the environment. The script is located at `.shop/setup.sh`.

To run the setup script:
```bash
.shop/setup.sh
```

The script will:
1. Install `platformio` using `pip`.
2. Install `dfu-util` using `apt-get`.
3. Run an initial build of the firmware, which will download all necessary toolchains and libraries and populate the cache.

## 2. Building the Firmware

To build both the BMCU370 and ESP32 firmware, use the provided build script:

```bash
./build.sh
```

This script will compile both firmwares and place the output files in the `build-output/` directory.

To build a specific firmware target:
- **BMCU370**: `pio run --environment genericCH32V203C8T6`
- **ESP32**: `cd esp32_firmware && pio run --environment esp32s3`

## 3. Caching

To significantly speed up subsequent builds, the following directories should be cached:

- `./.pio`: Caches the compiled object files and library dependencies for the BMCU370 firmware.
- `esp32_firmware/.pio`: Caches the compiled object files and library dependencies for the ESP32 firmware.
- The PlatformIO global cache directory, which is typically `~/.platformio`.

**Do not** cache the `build-output/` directory, as it should contain the artifacts from the most recent build.

## 4. Testing

There is currently no automated test suite for this project. All changes should be manually tested by flashing the firmware to the hardware and verifying functionality.

## 5. Flashing the Firmware

The `build.sh` script and the `README.md` file contain instructions for flashing the firmware using `dfu-util` for the BMCU370 and `esptool.py` for the ESP32. These commands are for reference and should not be run in the development environment unless you have the hardware connected.
