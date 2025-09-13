# LittleFS Troubleshooting Guide

## LittleFS Mounting Issues

If you see these error messages in the serial console:
```
File system is not mounted
[E] [vfs_api.cpp:24] open(): File system is not mounted
```

This indicates that the LittleFS filesystem partition is not properly flashed or mounted.

## Symptoms

1. **Repeated "File system is not mounted" errors** in serial console
2. **Web interface shows fallback mode** with limited functionality
3. **Historical data logging disabled** (memory-only mode)
4. **API endpoints work** but no web interface files are served

## Solutions

### Option 1: Reflash LittleFS Partition (Recommended)

1. **Flash only the LittleFS partition:**
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
     write_flash --flash_size 4MB 0x310000 littlefs.bin
   ```

2. **If flashing fails, try lower baud rate:**
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
     write_flash --flash_size 4MB 0x310000 littlefs.bin
   ```

### Option 2: Complete Firmware Reflash

1. **Flash everything including partition table:**
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
     write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
     0x0 bootloader.bin \
     0x8000 partitions.bin \
     0x10000 firmware.bin \
     0x310000 littlefs.bin
   ```

### Option 3: Erase and Reflash

1. **Completely erase flash:**
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash
   ```

2. **Then reflash everything:**
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
     write_flash --flash_size 4MB --flash_mode dio --flash_freq 80m \
     0x0 bootloader.bin \
     0x8000 partitions.bin \
     0x10000 firmware.bin \
     0x310000 littlefs.bin
   ```

## Fallback Mode Features

When LittleFS is not available, the ESP32 firmware automatically enters **Fallback Mode** with these features:

### Available Functionality:
- ✅ **WiFi connection and AP mode**
- ✅ **All API endpoints** (`/api/status`, `/api/config`, etc.)
- ✅ **BMCU370 communication via USB**
- ✅ **Basic web interface** (served from program memory)
- ✅ **WebSocket real-time updates**
- ✅ **System control** (reset, DFU mode)

### Limited Functionality:
- ❌ **No full web interface** (HTML/CSS/JS files)
- ❌ **No historical data persistence** (memory-only)
- ❌ **No file uploads** or web-based OTA updates

### Accessing Fallback Interface:

1. **Connect to WiFi network** or **BMCU370-Config** AP (password: `bmcu370pass`)
2. **Open browser** to ESP32 IP address (usually `http://192.168.4.1` in AP mode)
3. **Fallback interface** provides:
   - System status monitoring
   - API endpoint links
   - BMCU370 connection testing
   - LittleFS troubleshooting guidance

## Partition Table Information

**Current ESP32-S3 N4R2 partition layout:**
```
Name,    Type, SubType, Offset,  Size,     Flags
nvs,     data, nvs,     0x9000,  0x5000,
otadata, data, ota,     0xe000,  0x2000,
app0,    app,  ota_0,   0x10000, 0x280000,
spiffs,  data, spiffs,  0x310000,0x0F0000,
```

- **LittleFS Location:** `0x310000` (3,145,728 bytes offset)
- **LittleFS Size:** `0x0F0000` (983,040 bytes = ~960KB)
- **Total Flash:** 4MB (ESP32-S3 N4R2 variant)

## Prevention

To avoid LittleFS issues in the future:

1. **Always use `--flash_size 4MB`** parameter with esptool.py
2. **Flash LittleFS partition** every time you update firmware
3. **Use lower baud rates** (460800 or 115200) if experiencing flash errors
4. **Verify partition table** matches your ESP32-S3 variant
5. **Check serial output** during boot for mount success/failure

## Debug Information

The firmware provides detailed debug output during LittleFS initialization:

```
File system initialized successfully
LittleFS contents:
  /index.html (2048 bytes)
  /css/style.css (4096 bytes)
  /js/app.js (8192 bytes)
```

If you see this output, LittleFS is working correctly. If you see:
```
ERROR: Failed to initialize file system
Web interface will use fallback mode (API only)
```

Then follow the troubleshooting steps above.