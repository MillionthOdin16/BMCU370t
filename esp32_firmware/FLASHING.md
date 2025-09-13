# ESP32-S3 Flashing Instructions (4MB Flash + 2MB PSRAM)

## Hardware Compatibility
This firmware is optimized for ESP32-S3 with:
- **4MB Flash Memory** (confirmed by your chip)
- **2MB PSRAM** (OPI mode)
- **USB OTG Support** for BMCU370 communication

## LittleFS Flashing Issues - Solutions

If you're experiencing LittleFS flashing failures like:
```
Failed to flash DeflBlock
Failed to flashDeflBlock 
Flash file1 failed...
```

### Solution 1: Use Optimized ESP32-S3 Configuration
The firmware now includes ESP32-S3 specific optimizations:
- **Correct board definition** for 4MB Flash + 2MB PSRAM
- **Optimized partition table** with proper sector alignment
- **PSRAM support** for larger buffers and better performance

### Solution 2: Flash with Correct Parameters
For ESP32-S3 4MB Flash, use these specific parameters:
```bash
# Complete firmware flash (recommended)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

### Solution 3: Stage Flashing for Problematic Connections
1. **Flash main firmware first**:
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
     write_flash --flash_mode dio --flash_freq 80m \
     0x10000 firmware.bin
   ```

2. **Flash LittleFS separately** (if needed):
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
     write_flash --flash_mode dio 0x310000 littlefs.bin
   ```

### Solution 4: Lower Baud Rate for Stability
If flashing still fails, reduce baud rate:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \
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

## Memory Layout (ESP32-S3 4MB Flash + 2MB PSRAM)
**Flash Memory (4MB total):**
- **Bootloader**: 0x0 - 0x8FFF (36KB)
- **Partition Table**: 0x8000 - 0x8FFF (4KB)
- **NVS**: 0x9000 - 0xDFFF (20KB)
- **OTA Data**: 0xE000 - 0xFFFF (8KB)
- **App0** (Primary): 0x10000 - 0x18FFFF (1.5MB)
- **App1** (OTA): 0x190000 - 0x30FFFF (1.5MB) 
- **LittleFS**: 0x310000 - 0x3DFFFF (832KB)
- **Core Dump**: 0x3E0000 - 0x3EFFFF (64KB)

**PSRAM (2MB):**
- Used for large JSON buffers and web interface caching
- Automatically managed by ESP32-S3 firmware

**Memory Usage:**
- Flash: 950KB (61.0% of app partition)
- RAM: 803KB dynamic + 320KB static
- PSRAM: Available for buffer expansion

## Hardware Verification
Your ESP32-S3 should report:
```
Chip features: Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, 
Embedded Flash 4MB, XMC, Embedded PSRAM 2MB, AP_3v3
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