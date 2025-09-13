# ESP32-S3 N4R2 Flash Size Error Fix

## Problem
If you see this error when the ESP32 boots:
```
E (226) spi_flash: Detected size(4096k) smaller than the size in the binary image header(8192k). Probe failed.
assert failed: do_core_init startup.c:328 (flash_ret == ESP_OK)
```

## Root Cause
The firmware binary was compiled with 8MB flash size settings, but your ESP32-S3 N4R2 hardware only has 4MB flash. This creates a mismatch between the binary header and actual hardware.

## Solution: Force 4MB Flash Size During Upload

### Option 1: Force 4MB Flash Size (Recommended)
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Option 2: Use Auto-Detection Override
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size detect --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Option 3: Firmware Only (Skip LittleFS initially)
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x10000 firmware.bin
```

## Verification
After successful flashing, the ESP32 should boot normally and show:
- No flash size errors
- Web interface accessible at 192.168.4.1 (AP mode)
- Serial output showing successful initialization

## Hardware Confirmation
Your ESP32-S3 N4R2 should identify as:
```
Chip: ESP32-S3 (QFN56) rev 0.1
Chip features: Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, 
Embedded Flash 4MB (N4), XMC, Embedded PSRAM 2MB (R2), AP_3v3
```

## Why This Fix Works
The `--flash_size 4MB` parameter tells esptool to override the binary header and write the correct flash size information during upload, resolving the hardware/firmware mismatch.