# ESP32-S3 N4R2 Boot Issues and Fixes

## Common Boot Errors and Solutions

### 1. Flash Size Mismatch Error
If you see this error when the ESP32 boots:
```
E (226) spi_flash: Detected size(4096k) smaller than the size in the binary image header(8192k). Probe failed.
assert failed: do_core_init startup.c:328 (flash_ret == ESP_OK)
```

**Solution:** Force 4MB flash size during upload:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### 2. eFuse Octal Flash Error (NEW)
If you see this error when the ESP32 boots:
```
E (223) cpu_start: Octal Flash option selected, but EFUSE not configured!
abort() was called at PC 0x4037728d on core 0
```

**Root Cause:** The firmware was configured for OPI (Octal) PSRAM mode, but your ESP32-S3 N4R2 hardware doesn't have the eFuses configured for octal mode.

**Solution:** The firmware has been updated to use QSPI (Quad) mode instead of OPI mode. Flash the latest firmware build.

### 3. Complete Recovery Flash Command
Use this command for a fresh start with all fixes applied:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

## Hardware Configuration Details

### ESP32-S3 N4R2 Specifications
- **N4**: 4MB NAND Flash (not 8MB)
- **R2**: 2MB PSRAM in QSPI mode (not OPI mode)
- **eFuses**: Not configured for octal flash operation

### Expected Boot Output (Success)
```
ESP-ROM:esp32s3-20210327
Build: Mar 27 2021
rst:0x1 (POWERON_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x00000000
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x41c
load:0x403c9700,len:0x9a8
load:0x403cc700,len:0x28d0
entry 0x403c98b8
I (xx) cpu_start: Pro cpu up.
I (xx) cpu_start: Starting app cpu...
```

## Verification Steps
1. No eFuse or flash size errors during boot
2. Web interface accessible at 192.168.4.1 (AP mode)
3. Serial monitor shows successful PSRAM initialization
4. Hardware correctly identified as ESP32-S3 N4R2 with 4MB Flash + 2MB PSRAM