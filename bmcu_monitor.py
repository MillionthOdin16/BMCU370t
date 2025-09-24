#!/usr/bin/env python3
"""
BMCU Sensor Monitor

This script connects to the BMCU via USB serial and displays real-time sensor readings
including pressure sensors, position sensors, and system states.

Usage:
  python bmcu_monitor.py [PORT]

Default port on Windows: COM3, COM4, etc.
Default port on Linux/Mac: /dev/ttyUSB0, /dev/ttyACM0, etc.

Requirements:
  pip install pyserial
"""

import sys
import serial
import time
import re
from datetime import datetime

def find_serial_port():
    """Auto-detect BMCU serial port"""
    import serial.tools.list_ports
    
    # Common port patterns for BMCU
    port_patterns = [
        r'COM\d+',      # Windows
        r'/dev/ttyUSB\d+',  # Linux USB-Serial
        r'/dev/ttyACM\d+',  # Linux USB-CDC
        r'/dev/cu\.usbserial.*',  # macOS
        r'/dev/cu\.usbmodem.*'    # macOS
    ]
    
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Look for WCH (WinChipHead) or CH32V devices
        if any(x in port.description.upper() for x in ['WCH', 'CH32', 'USB SERIAL']):
            return port.device
            
        # Fallback to common patterns
        for pattern in port_patterns:
            if re.match(pattern, port.device):
                return port.device
    
    return None

def parse_sensor_data(line):
    """Parse sensor data from debug output"""
    data = {}
    
    # Parse pressure readings: "Pressure (V): CH0:1.650 CH1:1.680 ..."
    pressure_match = re.search(r'Pressure \(V\): (.+)', line)
    if pressure_match:
        readings = pressure_match.group(1)
        channels = re.findall(r'CH(\d+):([\d.]+)', readings)
        data['pressure'] = {int(ch): float(val) for ch, val in channels}
    
    # Parse position readings: "Position (V): CH0:1.250 CH1:1.320 ..."  
    position_match = re.search(r'Position \(V\): (.+)', line)
    if position_match:
        readings = position_match.group(1)
        channels = re.findall(r'CH(\d+):([\d.]+)', readings)
        data['position'] = {int(ch): float(val) for ch, val in channels}
        
    # Parse pressure states: "Press State:  CH0:NORM CH1:HIGH ..."
    press_state_match = re.search(r'Press State:\s+(.+)', line)
    if press_state_match:
        readings = press_state_match.group(1)
        channels = re.findall(r'CH(\d+):(\w+)', readings)
        data['press_state'] = {int(ch): state for ch, state in channels}
        
    # Parse online states: "Online State: CH0:ON  CH1:OFF ..."
    online_match = re.search(r'Online State: (.+)', line)
    if online_match:
        readings = online_match.group(1)
        channels = re.findall(r'CH(\d+):(\w+)', readings)
        data['online'] = {int(ch): state for ch, state in channels}
        
    # Parse position states: "Position:     CH0:IDLE CH1:SEND ..."
    pos_state_match = re.search(r'Position:\s+(.+)', line)
    if pos_state_match:
        readings = pos_state_match.group(1)
        channels = re.findall(r'CH(\d+):(\w+)', readings)
        data['position_state'] = {int(ch): state for ch, state in channels}
    
    return data

def format_sensor_display(sensor_data):
    """Format sensor data for clean display"""
    if not sensor_data:
        return ""
    
    lines = []
    lines.append(f"\n{'='*60}")
    lines.append(f"BMCU Sensor Monitor - {datetime.now().strftime('%H:%M:%S')}")
    lines.append(f"{'='*60}")
    
    # Channel headers
    lines.append(f"{'Channel:':<12} {'CH0':<8} {'CH1':<8} {'CH2':<8} {'CH3':<8}")
    lines.append(f"{'-'*60}")
    
    # Pressure readings
    if 'pressure' in sensor_data:
        pressure_line = f"{'Pressure(V):':<12}"
        for ch in range(4):
            val = sensor_data['pressure'].get(ch, 0.0)
            pressure_line += f"{val:<8.3f}"
        lines.append(pressure_line)
    
    # Position readings  
    if 'position' in sensor_data:
        position_line = f"{'Position(V):':<12}"
        for ch in range(4):
            val = sensor_data['position'].get(ch, 0.0)
            position_line += f"{val:<8.3f}"
        lines.append(position_line)
        
    lines.append(f"{'-'*60}")
    
    # State information
    if 'press_state' in sensor_data:
        state_line = f"{'Press State:':<12}"
        for ch in range(4):
            state = sensor_data['press_state'].get(ch, 'UNK')
            state_line += f"{state:<8}"
        lines.append(state_line)
        
    if 'online' in sensor_data:
        online_line = f"{'Online:':<12}"
        for ch in range(4):
            state = sensor_data['online'].get(ch, 'UNK')
            online_line += f"{state:<8}"
        lines.append(online_line)
        
    if 'position_state' in sensor_data:
        pos_line = f"{'Position:':<12}"
        for ch in range(4):
            state = sensor_data['position_state'].get(ch, 'UNK')
            pos_line += f"{state:<8}"
        lines.append(pos_line)
    
    return '\n'.join(lines)

def main():
    print("BMCU Sensor Monitor")
    print("=" * 40)
    
    # Determine serial port
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = find_serial_port()
        if not port:
            print("No BMCU device found. Please specify port manually:")
            print("  python bmcu_monitor.py COM3")
            print("  python bmcu_monitor.py /dev/ttyUSB0")
            return
    
    print(f"Connecting to BMCU on port: {port}")
    print(f"Baud rate: 115200")
    print("Press Ctrl+C to exit\n")
    
    try:
        # Connect to serial port
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)  # Allow connection to stabilize
        
        print("Connected! Waiting for sensor data...")
        print("Note: Data updates every 5 seconds from the BMCU")
        
        current_sensor_data = {}
        raw_mode = False  # Set to True to see raw debug output
        
        while True:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    if raw_mode:
                        print(line)
                    else:
                        # Parse sensor data
                        parsed = parse_sensor_data(line)
                        if parsed:
                            current_sensor_data.update(parsed)
                            
                        # Display complete sensor reading when we have all data
                        if 'pressure' in parsed or 'position' in parsed:
                            display = format_sensor_display(current_sensor_data)
                            if display:
                                print(display)
                                # Clear previous data for next reading
                                current_sensor_data = {}
                        
            except UnicodeDecodeError:
                # Skip invalid characters
                continue
                
    except serial.SerialException as e:
        print(f"Serial connection error: {e}")
        print("Make sure:")
        print("1. BMCU is connected via USB")
        print("2. Correct port is specified")
        print("3. No other program is using the port")
        
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")
        
    finally:
        try:
            ser.close()
        except:
            pass

if __name__ == "__main__":
    main()