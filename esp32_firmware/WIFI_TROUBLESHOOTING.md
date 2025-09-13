# WiFi and System Troubleshooting Guide

## WiFi Scanning Watchdog Timeout Fix

### Problem
The ESP32 would experience watchdog timeouts during WiFi network scanning, causing system reboots and instability.

### Root Cause
The synchronous `WiFi.scanNetworks()` call was blocking the main loop for too long, causing the task watchdog to trigger.

### Solution
Implemented asynchronous WiFi scanning with polling:
- `WiFi.scanNetworks(true)` for non-blocking scan initiation
- Client polls `/api/wifi/scan` endpoint for results
- Status indicators: `"started"`, `"scanning"`, `"complete"`

### Usage
1. Navigate to the Network tab
2. Click "Scan for Networks"
3. Wait for results (2-8 seconds depending on environment)
4. Click on any network to auto-fill SSID field

## WiFi Connection Issues

### Enhanced Connection Handling
- **Improved error detection**: Specific error messages for common failure modes
- **Better state management**: Proper WiFi mode switching during connection attempts
- **Detailed feedback**: API responses include specific error reasons

### Common Issues and Solutions

#### "Connection failed - wrong password or network issue"
- **Cause**: `WL_CONNECT_FAILED` status
- **Solution**: Verify password case-sensitivity and special characters

#### "Network not found - SSID may be hidden or out of range"
- **Cause**: `WL_NO_SSID_AVAIL` status
- **Solution**: Move closer to router or manually enter hidden network SSID

#### "Connection lost during handshake" 
- **Cause**: `WL_CONNECTION_LOST` status
- **Solution**: Check router compatibility or try 2.4GHz band

## LittleFS Filesystem Issues

### Console Spam Reduction
**Problem**: Repeated "File system is not mounted" errors flooding serial output.

**Solution**: Rate-limited filesystem access with intelligent caching:
- Filesystem availability checked once every 30 seconds max
- Error messages limited to once per minute
- Graceful fallback to memory-only operation

### LittleFS Mounting Troubleshooting

#### Partition Table Issues
If LittleFS won't mount, the partition table might be misaligned:

```bash
# Flash with correct partition table
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB 0x8000 partitions.bin
```

#### Manual LittleFS Creation
```bash
# Create and flash LittleFS partition
mkspiffs -c data/ -b 4096 -p 256 -s 589824 littlefs.bin
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash --flash_size 4MB 0x350000 littlefs.bin
```

#### Fallback Mode
If LittleFS continues to fail:
- System automatically uses enhanced fallback web interface
- All functionality available except file-based web assets
- WiFi configuration works fully in fallback mode

## Network Configuration

### AP Mode Details
- **SSID**: `BMCU370-Config`
- **Password**: `bmcu370pass`
- **IP Address**: `192.168.4.1`
- **Timeout**: 5 minutes then automatic retry with saved credentials

### Station Mode
- Automatic reconnection every 30 seconds if disconnected
- Credentials saved in NVS (non-volatile storage)
- Signal strength and connection status monitoring

## API Endpoints

### WiFi Management
- `GET /api/wifi/status` - Current WiFi status and network info
- `GET /api/wifi/scan` - Async network scanning (poll for results)
- `POST /api/wifi/connect` - Connect to network (ssid, password parameters)

### System Status
- `GET /api/status` - BMCU370 connection and system status
- `GET /api/system` - Hardware info and performance metrics

## Advanced Troubleshooting

### Watchdog Issues
If experiencing frequent reboots:
1. Check for blocking operations in main loop
2. Verify `delay()` calls are minimal (< 100ms)
3. Use `yield()` in long-running operations

### Memory Issues
Monitor heap usage:
```cpp
ESP.getFreeHeap()  // Available RAM
ESP.getFreePsram() // Available PSRAM (N4R2 variant)
```

### Serial Debugging
Enable detailed logging by setting:
```cpp
#define CORE_DEBUG_LEVEL 4  // Maximum verbosity
```

## Performance Characteristics

### WiFi Scanning
- **Duration**: 2-8 seconds depending on environment
- **Networks**: Typically detects 5-20 networks
- **Memory**: ~2KB buffer for scan results

### Connection Times
- **Known network**: 2-5 seconds
- **New network**: 5-10 seconds  
- **Retry interval**: 30 seconds for automatic reconnection

### System Resources
- **RAM usage**: 18.1% (59KB/327KB total)
- **Flash usage**: 56.2% (957KB/1703KB total)
- **PSRAM**: 2MB available for large buffers