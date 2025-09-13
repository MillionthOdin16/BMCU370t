# WiFi Configuration Guide for BMCU370 ESP32 Interface

## Overview

The BMCU370 ESP32 interface provides a comprehensive WiFi configuration system through its web interface. This guide explains how to connect your BMCU370 device to your home WiFi network.

## Quick Start

### Step 1: Connect to Config Network
1. Power on your BMCU370 device with ESP32 interface
2. On your phone/computer, look for WiFi network: **BMCU370-Config**
3. Connect using password: **bmcu370pass**
4. Open web browser and go to: `http://192.168.4.1`

### Step 2: Access WiFi Configuration
1. Click on the **Network** tab in the web interface
2. You'll see the WiFi configuration panel with three sections:
   - **WiFi Status**: Shows current connection status
   - **Available Networks**: Scan and select networks
   - **Connect to WiFi Network**: Enter credentials

### Step 3: Connect to Your Network
1. Click **"Scan Networks"** to see available WiFi networks
2. Click on your network name from the scan results (this auto-fills the SSID field)
3. Enter your WiFi password in the password field
4. Click **"Connect to Network"**
5. Wait for confirmation message

## Detailed Interface Features

### WiFi Status Display
- **Status**: Shows "Config Mode (AP)", "Connected", or "Disconnected"
- **SSID**: Current network name (or "BMCU370-Config" when in config mode)
- **IP Address**: Device IP address on the network
- **Signal Strength**: Connection quality in dBm

### Network Scanner
- Displays all available WiFi networks with:
  - Network name (SSID)
  - Signal strength (bars and dBm)
  - Security type (Open/Encrypted)
- Click any network to auto-fill the connection form

### Connection Form
- **Network Name (SSID)**: Enter manually or click from scan results
- **Password**: WiFi password (leave blank for open networks)
- **Connect Button**: Initiates connection attempt

## Network States

### Config Mode (Access Point)
- **Network**: BMCU370-Config
- **Password**: bmcu370pass
- **IP**: 192.168.4.1
- **Purpose**: Initial setup and fallback mode

### Connected Mode
- **Network**: Your home WiFi
- **IP**: Assigned by your router
- **Purpose**: Normal operation with internet access

### Automatic Behavior
- Device remembers successful connections
- Auto-reconnects to saved networks on power-up
- Falls back to config mode if saved network unavailable
- Config mode timeout: 10 minutes (then retry saved network)

## Troubleshooting

### Can't See BMCU370-Config Network
- Ensure device is powered on and ESP32 is running
- Check device is in config mode (LED indicators)
- Wait 30 seconds after power-on for AP to start

### Can't Connect to Home Network
- Verify WiFi password is correct
- Ensure network name (SSID) is exact (case-sensitive)
- Check signal strength is adequate (>-70 dBm recommended)
- Verify network supports 2.4GHz (ESP32 doesn't support 5GHz)

### Connection Drops
- Check signal strength at device location
- Verify router stability
- Device will automatically attempt reconnection every 30 seconds

### Web Interface Not Loading
- Ensure you're connected to the correct network
- Try different browsers
- Clear browser cache
- Check URL: `http://192.168.4.1` (config mode) or device IP (connected mode)

## Network Requirements

### Supported Networks
- ✅ 2.4GHz WiFi networks
- ✅ WPA/WPA2 security
- ✅ Open networks (no password)
- ❌ 5GHz networks (not supported by ESP32)
- ❌ Enterprise security (WPA2-Enterprise)

### Bandwidth Usage
- **Normal Operation**: <1KB/s (status updates)
- **Web Interface**: ~50KB initial load
- **Firmware Updates**: Up to 2MB (one-time)

## Advanced Features

### API Endpoints
For programmatic access:
- `GET /api/wifi/status` - Get connection status
- `GET /api/wifi/scan` - Scan for networks
- `POST /api/wifi/connect` - Connect to network

### Persistent Storage
- Network credentials saved in ESP32 flash memory
- Survives power cycles and firmware updates
- Use "Reset WiFi" button to clear saved credentials

### Security Notes
- Config mode network uses WPA2 security
- Credentials transmitted over HTTPS when possible
- No credential logging in device logs
- Config mode auto-disables after successful connection

## Support

If you continue to experience issues:
1. Check the **Diagnostics** tab for error logs
2. Use **Reset ESP32** button to restart networking
3. Use **Reset WiFi** to clear saved credentials
4. Check signal strength and router compatibility

The interface provides real-time feedback and detailed error messages to help diagnose connection issues.