#!/usr/bin/env python3
"""
BMCU Filament Data Backup and Restore Tool

This tool helps preserve filament configurations across firmware updates by backing up 
and restoring the flash data sectors where BMCU stores filament information.

Usage:
  python backup_restore_data.py backup <port>     - Backup current filament data
  python backup_restore_data.py restore <port>    - Restore previously backed up data
  python backup_restore_data.py flash <port> <firmware.bin> - Flash firmware and preserve data

Memory Layout (CH32V203C8T6 - 64KB Flash):
- 0x08000000-0x0800DFFF: Firmware code (up to 56KB)
- 0x0800E000-0x0800EFFF: Motion control data (4KB)
- 0x0800F000-0x0800FFFF: BambuBus filament data (4KB)
"""

import sys
import os
import serial
import struct
import time
import json
from datetime import datetime

# Flash memory addresses for data storage
MOTION_DATA_ADDR = 0x0800E000
BAMBUBUS_DATA_ADDR = 0x0800F000
DATA_SECTOR_SIZE = 0x1000  # 4KB per sector

class BMCUDataManager:
    def __init__(self, port):
        self.port = port
        self.backup_file = f"bmcu_data_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        
    def connect_device(self):
        """Connect to BMCU device"""
        try:
            self.ser = serial.Serial(self.port, 115200, timeout=5)
            time.sleep(2)  # Allow connection to stabilize
            return True
        except Exception as e:
            print(f"Error connecting to device: {e}")
            return False
            
    def disconnect_device(self):
        """Disconnect from device"""
        if hasattr(self, 'ser'):
            self.ser.close()
            
    def read_flash_sector(self, address, size):
        """Read flash memory sector (placeholder - would need actual protocol)"""
        # This would need to implement the actual CH32V flash reading protocol
        # For now, return a placeholder that explains the process
        print(f"Reading flash sector at 0x{address:08X} (size: {size} bytes)")
        print("Note: This requires implementing the CH32V flash reading protocol")
        print("Typical methods:")
        print("1. Use WCH-Link debugger with OpenOCD")
        print("2. Implement custom bootloader command")
        print("3. Use existing firmware debug interface")
        return None
        
    def write_flash_sector(self, address, data):
        """Write flash memory sector (placeholder - would need actual protocol)"""
        print(f"Writing flash sector at 0x{address:08X} ({len(data)} bytes)")
        print("Note: This requires implementing the CH32V flash writing protocol")
        return False
        
    def backup_data(self):
        """Backup filament data from BMCU"""
        print("BMCU Filament Data Backup Tool")
        print("=" * 40)
        
        if not self.connect_device():
            return False
            
        try:
            # Read motion control data
            motion_data = self.read_flash_sector(MOTION_DATA_ADDR, DATA_SECTOR_SIZE)
            
            # Read BambuBus filament data  
            bambubus_data = self.read_flash_sector(BAMBUBUS_DATA_ADDR, DATA_SECTOR_SIZE)
            
            if motion_data is None or bambubus_data is None:
                print("Error: Could not read flash data")
                print("\nTO MANUALLY BACKUP DATA:")
                print("1. Use WCH-Link with OpenOCD:")
                print(f"   openocd -f interface/wch-link.cfg -f target/ch32v203.cfg")
                print(f"   telnet localhost 4444")
                print(f"   flash read_bank 0 motion_data.bin 0xE000 0x1000")
                print(f"   flash read_bank 0 bambubus_data.bin 0xF000 0x1000")
                print("\n2. Or use WCH ISP tool to read specific flash regions")
                return False
                
            # Create backup file
            backup_data = {
                'timestamp': datetime.now().isoformat(),
                'device': 'BMCU370t',
                'motion_data': motion_data.hex() if motion_data else None,
                'bambubus_data': bambubus_data.hex() if bambubus_data else None,
                'addresses': {
                    'motion_data': hex(MOTION_DATA_ADDR),
                    'bambubus_data': hex(BAMBUBUS_DATA_ADDR)
                }
            }
            
            with open(self.backup_file, 'w') as f:
                json.dump(backup_data, f, indent=2)
                
            print(f"Backup saved to: {self.backup_file}")
            return True
            
        finally:
            self.disconnect_device()
            
    def restore_data(self, backup_file=None):
        """Restore filament data to BMCU"""
        print("BMCU Filament Data Restore Tool")
        print("=" * 40)
        
        # Find backup file
        if backup_file is None:
            backup_files = [f for f in os.listdir('.') if f.startswith('bmcu_data_backup_') and f.endswith('.json')]
            if not backup_files:
                print("No backup files found!")
                return False
            backup_file = max(backup_files)  # Use most recent
            
        if not os.path.exists(backup_file):
            print(f"Backup file not found: {backup_file}")
            return False
            
        # Load backup data
        with open(backup_file, 'r') as f:
            backup_data = json.load(f)
            
        print(f"Restoring from backup: {backup_file}")
        print(f"Backup timestamp: {backup_data['timestamp']}")
        
        if not self.connect_device():
            return False
            
        try:
            success = True
            
            # Restore motion control data
            if backup_data['motion_data']:
                motion_data = bytes.fromhex(backup_data['motion_data'])
                if not self.write_flash_sector(MOTION_DATA_ADDR, motion_data):
                    print("Error writing motion control data")
                    success = False
                    
            # Restore BambuBus filament data
            if backup_data['bambubus_data']:
                bambubus_data = bytes.fromhex(backup_data['bambubus_data'])
                if not self.write_flash_sector(BAMBUBUS_DATA_ADDR, bambubus_data):
                    print("Error writing BambuBus data")
                    success = False
                    
            if not success:
                print("\nTO MANUALLY RESTORE DATA:")
                print("1. Use WCH-Link with OpenOCD:")
                print(f"   openocd -f interface/wch-link.cfg -f target/ch32v203.cfg")
                print(f"   telnet localhost 4444")
                print(f"   flash write_bank 0 motion_data.bin 0xE000")
                print(f"   flash write_bank 0 bambubus_data.bin 0xF000")
                print("\n2. Create binary files from backup:")
                
                # Create binary files for manual restore
                if backup_data['motion_data']:
                    with open('motion_data.bin', 'wb') as f:
                        f.write(bytes.fromhex(backup_data['motion_data']))
                    print(f"   Created: motion_data.bin")
                    
                if backup_data['bambubus_data']:
                    with open('bambubus_data.bin', 'wb') as f:
                        f.write(bytes.fromhex(backup_data['bambubus_data']))
                    print(f"   Created: bambubus_data.bin")
                    
            return success
            
        finally:
            self.disconnect_device()
            
    def flash_with_preserve(self, firmware_file):
        """Flash firmware while preserving data"""
        print("BMCU Flash with Data Preservation")
        print("=" * 40)
        
        # First backup current data
        print("Step 1: Backing up current data...")
        if not self.backup_data():
            print("Backup failed - aborting flash process")
            return False
            
        # Flash firmware (user would do this manually)
        print("\nStep 2: Flash firmware")
        print("Please flash the firmware using your preferred method:")
        print(f"  - WCH ISP Tool with {firmware_file}")
        print(f"  - PlatformIO: pio run -t upload")
        print(f"  - OpenOCD with WCH-Link")
        
        input("Press Enter after firmware flashing is complete...")
        
        # Restore data
        print("\nStep 3: Restoring filament data...")
        return self.restore_data(self.backup_file)

def print_usage():
    print("BMCU Filament Data Backup/Restore Tool")
    print("=" * 40)
    print("Usage:")
    print("  python backup_restore_data.py backup <port>")
    print("  python backup_restore_data.py restore <port> [backup_file]")
    print("  python backup_restore_data.py flash <port> <firmware.bin>")
    print()
    print("Examples:")
    print("  python backup_restore_data.py backup COM3")
    print("  python backup_restore_data.py restore COM3")
    print("  python backup_restore_data.py flash COM3 firmware.bin")
    print()
    print("Note: This tool provides the framework for data preservation.")
    print("Actual flash reading/writing requires implementing the CH32V protocol")
    print("or using external tools like WCH-Link + OpenOCD.")

def main():
    if len(sys.argv) < 3:
        print_usage()
        sys.exit(1)
        
    command = sys.argv[1]
    port = sys.argv[2]
    
    manager = BMCUDataManager(port)
    
    if command == "backup":
        success = manager.backup_data()
        sys.exit(0 if success else 1)
        
    elif command == "restore":
        backup_file = sys.argv[3] if len(sys.argv) > 3 else None
        success = manager.restore_data(backup_file)
        sys.exit(0 if success else 1)
        
    elif command == "flash":
        if len(sys.argv) < 4:
            print("Error: firmware file required for flash command")
            print_usage()
            sys.exit(1)
        firmware_file = sys.argv[3]
        success = manager.flash_with_preserve(firmware_file)
        sys.exit(0 if success else 1)
        
    else:
        print(f"Unknown command: {command}")
        print_usage()
        sys.exit(1)

if __name__ == "__main__":
    main()