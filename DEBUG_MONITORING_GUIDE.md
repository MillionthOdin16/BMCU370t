# BMCU Debug Monitoring System

## Overview

The BMCU firmware includes a comprehensive debugging system that provides real-time sensor readings and system status via USB serial connection. This allows you to monitor the internal state of the system and diagnose issues.

## Hardware Connection

The BMCU debug system uses **USART3** on pins:
- **TX (Transmit)**: PB10 - Debug output to PC
- **RX (Receive)**: PB11 - Commands from PC (currently unused)
- **Baud Rate**: 115200
- **Data Format**: 8N1 (8 data bits, no parity, 1 stop bit)

## Debug Output Features

### 1. Continuous Sensor Monitoring
Every 5 seconds, the system outputs comprehensive sensor readings:

```
=== BMCU Sensor Monitor ===
Pressure (V): CH0:1.650 CH1:1.680 CH2:1.720 CH3:1.600 
Position (V): CH0:1.250 CH1:1.320 CH2:0.980 CH3:1.450 
Press State:  CH0:NORM CH1:NORM CH2:HIGH CH3:NORM 
Online State: CH0:ON  CH1:ON  CH2:OFF CH3:ON  
Position:     CH0:IDLE CH1:SEND CH2:IDLE CH3:USE 
Thresholds: HIGH>1.85V, LOW<1.45V
============================
```

### 2. System State Changes
The system logs important state changes:
- `BambuBus_online` - Communication established with printer
- `BambuBus_offline` - Communication lost
- `Run_To_AMS_lite` - Operating in AMS Lite mode
- `Run_To_AMS` - Operating in full AMS mode

### 3. Individual Channel Debug
Detailed readings for channel 0 are logged continuously for debugging:
```
MC_PULL_stu_raw = 1.650  MC_ONLINE_key_stu_raw = 1.250  通道：0
```

## Data Interpretation

### Pressure Sensor Readings (MC_PULL_stu_raw)
- **Range**: 0V - 3.3V
- **Normal Operation**: ~1.65V
- **High Pressure**: >1.85V (filament stuck or excessive resistance)
- **Low Pressure**: <1.45V (no filament or insufficient contact)

### Position Sensor Readings (MC_ONLINE_key_stu_raw)  
- **Range**: 0V - 3.3V
- **Filament Present**: >1.65V
- **No Filament**: <1.65V

### Pressure States
- **NORM**: Normal pressure range (1.45V - 1.85V)
- **HIGH**: Excessive pressure (>1.85V) - possible blockage
- **LOW**: Insufficient pressure (<1.45V) - no filament contact

### Online States
- **ON**: Filament detected and ready
- **ON***: Filament detected with special condition
- **OFF**: No filament detected

### Position States
- **IDLE**: Channel inactive, no operation
- **SEND**: Actively feeding filament out
- **USE**: In use by printer (active printing)
- **PULL**: Retracting filament back
- **RETC**: Redetecting filament position

## Using the Debug System

### Method 1: Python Monitor Script (Recommended)

Use the provided `bmcu_monitor.py` script for a clean, formatted display:

```bash
# Install required dependency
pip install pyserial

# Auto-detect and connect
python bmcu_monitor.py

# Or specify port manually
python bmcu_monitor.py COM3        # Windows
python bmcu_monitor.py /dev/ttyUSB0 # Linux
```

The script provides:
- Auto-detection of BMCU device
- Clean, formatted sensor display
- Real-time monitoring with timestamps
- Channel-organized data layout

### Method 2: Serial Terminal

Use any serial terminal program:
- **Windows**: PuTTY, TeraTerm, or Arduino Serial Monitor
- **Linux/Mac**: minicom, screen, or cu
- **Settings**: 115200 baud, 8N1, no flow control

Example commands:
```bash
# Linux/Mac with screen
screen /dev/ttyUSB0 115200

# Linux/Mac with minicom  
minicom -D /dev/ttyUSB0 -b 115200

# Windows with PuTTY
# Set Serial, COM3, 115200, 8N1
```

## Troubleshooting

### No Data Received
1. **Check USB Connection**: Ensure BMCU is connected via USB cable
2. **Verify Port**: Make sure correct serial port is selected
3. **Port Access**: Close other programs using the same port
4. **Driver Issues**: Install WCH USB-to-Serial drivers if needed

### Garbled Data
1. **Baud Rate**: Verify 115200 baud rate setting
2. **USB Cable**: Try a different USB cable (data capable, not just power)
3. **Port Settings**: Ensure 8N1 format with no flow control

### Missing Sensor Data
1. **Firmware Version**: Ensure you're using `firmware_with_debug_monitoring.bin`
2. **System Status**: Debug output only appears when system is running normally
3. **Wait Time**: Sensor reports update every 5 seconds

## Debug System Architecture

### Hardware Level
- **USART3**: Dedicated debug UART peripheral
- **DMA Transfer**: Efficient data transmission without CPU blocking
- **Buffer Management**: Circular buffering for continuous operation

### Software Level
- **Modular Design**: Separate debug functions for different data types
- **Configurable**: Can be easily enabled/disabled via `Debug_log_on` define
- **Non-blocking**: Debug output doesn't interfere with normal operation

### Performance Impact
- **RAM Usage**: ~46.8% (minimal increase from debug features)
- **Flash Usage**: ~61.7% (acceptable overhead for debugging)
- **CPU Overhead**: <1% due to DMA-based transmission

## Advanced Features

### Custom Debug Commands
The debug system is designed to be extensible. Additional debug commands can be added:

```cpp
// Add to Debug_log.cpp
void Debug_log_custom_data() {
    // Custom debug output
    char buffer[256];
    int len = sprintf(buffer, "Custom: %d\n", custom_value);
    Debug_log_write_num(buffer, len);
}
```

### Conditional Debug Output
Debug output can be controlled dynamically:

```cpp
#ifdef Debug_log_on
    if (some_condition) {
        DEBUG_MY("Condition met\n");
    }
#endif
```

This comprehensive debug system provides the visibility needed to understand the BMCU's internal operation and diagnose issues effectively.