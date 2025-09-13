# ESP32-S3 Flashing Instructions

## LittleFS Flashing Issues - Solutions

If you're experiencing LittleFS flashing failures like:
```
Failed to flash DeflBlock
Failed to flashDeflBlock 
Flash file1 failed...
```

Try these solutions:

### Solution 1: Use Optimized Partition Table
The firmware now includes an optimized partition table (`partitions.csv`) that allocates 1MB for LittleFS instead of 1.4MB, reducing flashing issues.

### Solution 2: Flash in Stages
1. **Flash main firmware first**:
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
   ```

2. **Flash LittleFS separately** (if needed):
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x290000 littlefs.bin
   ```

### Solution 3: Lower Baud Rate
If flashing still fails, try reducing the baud rate:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x290000 littlefs.bin
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

## Memory Layout
- **App0**: 0x10000 - 0x14FFFF (1.25MB)
- **App1**: 0x150000 - 0x28FFFF (1.25MB) 
- **LittleFS**: 0x290000 - 0x38FFFF (1MB)
- **Core Dump**: 0x390000 - 0x39FFFF (64KB)

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