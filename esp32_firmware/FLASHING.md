# ESP32-S3 N4R2 Flashing Instructions (4MB Flash + 2MB PSRAM)

## Critical Flash Size Fix for ESP32-S3 N4R2

### Issue: Binary Built for 8MB Flash on 4MB Hardware
If you see this error:
```
E (226) spi_flash: Detected size(4096k) smaller than the size in the binary image header(8192k). Probe failed.
assert failed: do_core_init startup.c:328 (flash_ret == ESP_OK)
```

This means the firmware was compiled with 8MB flash settings but your hardware only has 4MB.

### Solution: Force Flash Size During Upload
Use esptool with explicit 4MB flash size specification:
```bash
# CRITICAL: Force 4MB flash size during upload
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Alternative: Use Detect Mode
```bash
# Let esptool detect and override flash size
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size detect --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

## Hardware Compatibility
This firmware is specifically optimized for ESP32-S3 N4R2 variant:
- **N4**: 4MB NAND Flash Memory (confirmed by your chip)
- **R2**: 2MB OPI PSRAM (high-speed mode)
- **USB OTG Support** for BMCU370 communication
- **Enhanced performance** with 240MHz dual-core processing

## LittleFS Flashing Issues - Solutions

If you're experiencing LittleFS flashing failures like:
```
Failed to flash DeflBlock
Failed to flashDeflBlock 
Flash file1 failed...
```

### Solution 1: Use Optimized ESP32-S3 N4R2 Configuration
The firmware now includes ESP32-S3 N4R2 specific optimizations:
- **Correct board definition** for N4R2 variant (4MB Flash + 2MB PSRAM)
- **Optimized partition table** with proper sector alignment for N4 flash
- **OPI PSRAM support** for R2 variant with 80MHz speed
- **Enhanced memory management** for larger buffers and better performance

### Solution 2: Flash with ESP32-S3 N4R2 Optimized Parameters
For ESP32-S3 N4R2, use these specifically tuned parameters:
```bash
# Complete firmware flash with forced 4MB detection
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin

# Alternative with auto-detection override
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size detect --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Solution 3: Stage Flashing for Problematic Connections
1. **Flash main firmware first**:
   ```bash
   # Force 4MB flash size recognition
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
     write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
     0x10000 firmware.bin
   ```

2. **Flash LittleFS separately** (if needed):
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
     write_flash --flash_size 4MB --flash_mode dio 0x310000 littlefs.bin
   ```

### Solution 4: Lower Baud Rate for Stability
If flashing still fails, reduce baud rate:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 \
  write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Solution 4: PlatformIO Commands
Using PlatformIO (if available):
```bash
# Flash main firmware
pio run --target upload

# Flash filesystem only
pio run --target uploadfs
```

### Solution 5: Skip LittleFS Initially
The web interface will work without LittleFS initially. You can:
1. Flash main firmware only
2. Upload web files via the web interface later
3. Or access the device via IP address and use the REST API

## Memory Layout (ESP32-S3 N4R2: 4MB Flash + 2MB PSRAM)
**Flash Memory (N4 - 4MB total):**
- **Bootloader**: 0x0 - 0x8FFF (36KB)
- **Partition Table**: 0x8000 - 0x8FFF (4KB)
- **NVS**: 0x9000 - 0xDFFF (20KB)
- **OTA Data**: 0xE000 - 0xFFFF (8KB)
- **App0** (Primary): 0x10000 - 0x18FFFF (1.5MB)
- **App1** (OTA): 0x190000 - 0x30FFFF (1.5MB) 
- **LittleFS**: 0x310000 - 0x3DFFFF (832KB)
- **Core Dump**: 0x3E0000 - 0x3EFFFF (64KB)

**PSRAM (R2 - 2MB OPI mode):**
- High-speed OPI interface at 80MHz
- Used for large JSON buffers and web interface caching
- Automatically managed by ESP32-S3 firmware with enhanced allocation
- Supports up to 2MB of additional working memory

**Memory Usage:**
- Flash: 950KB (61.0% of app partition)
- RAM: 803KB dynamic + 320KB static
- PSRAM: Available for buffer expansion

## Hardware Verification
Your ESP32-S3 N4R2 should report:
```
Chip: ESP32-S3 (QFN56) rev 0.1
Chip features: Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, 
Embedded Flash 4MB (N4), XMC, Embedded PSRAM 2MB (R2), AP_3v3
```

## Default Access
After successful firmware flash:
- **AP Mode**: Connect to "BMCU370-Web" network
- **Web Interface**: http://192.168.4.1
- **Default WiFi Password**: "bmcu370web"

## Troubleshooting
- Ensure ESP32-S3 is in download mode (hold BOOT button during power-on)
- Check USB cable and connection
- Verify correct COM port
- Try different USB ports or cables
- Ensure adequate power supply (USB 3.0 recommended)