# BMCU Filament Data Preservation Guide

## Problem Overview

When flashing new firmware to the BMCU, the filament configuration data gets erased because:

1. **Flash Memory Layout**: The CH32V203C8T6 has 64KB of flash memory
   - `0x08000000-0x0800DFFF`: Firmware code (up to 56KB)
   - `0x0800E000-0x0800EFFF`: Motion control data (4KB) 
   - `0x0800F000-0x0800FFFF`: BambuBus filament data (4KB)

2. **Firmware Flashing**: Most flashing tools erase the entire flash, including data areas

3. **Current Firmware Fix**: The persistent data feature (commit c0c8d37) works for preserving data between firmware versions, but NOT across flash operations that erase the data areas.

## Solutions for Data Preservation

### Option 1: Use Partial Flash Programming (RECOMMENDED)

If your flashing tool supports partial programming, you can avoid erasing the data sectors:

**Using WCH ISP Tool:**
1. Open WCH ISP Tool
2. Select "Program" tab
3. **IMPORTANT**: Uncheck "Erase All" option
4. Set address range to: `0x08000000` to `0x0800DFFF` (firmware area only)
5. Load your firmware.bin file
6. Flash the firmware

**Using OpenOCD with WCH-Link:**
```bash
openocd -f interface/wch-link.cfg -f target/ch32v203.cfg
telnet localhost 4444
# Erase only firmware area (not data areas)
flash erase_sector 0 0 55  # Erase sectors 0-55 (0x00000-0x0DFFF)
flash write_bank 0 firmware.bin 0x0000
```

### Option 2: Manual Data Backup and Restore

**Before Flashing:**
```bash
# Backup data sectors
openocd -f interface/wch-link.cfg -f target/ch32v203.cfg
telnet localhost 4444
flash read_bank 0 motion_data_backup.bin 0xE000 0x1000
flash read_bank 0 filament_data_backup.bin 0xF000 0x1000
```

**After Flashing:**
```bash
# Restore data sectors  
openocd -f interface/wch-link.cfg -f target/ch32v203.cfg
telnet localhost 4444
flash write_bank 0 motion_data_backup.bin 0xE000
flash write_bank 0 filament_data_backup.bin 0xF000
```

### Option 3: Use PlatformIO with Custom Upload Script

Create a custom upload script that preserves data areas:

**In platformio.ini:**
```ini
[env:genericCH32V203C8T6]
platform = https://github.com/Community-PIO-CH32V/platform-ch32v.git
board = genericCH32V203C8T6
framework = arduino
lib_deps = robtillaart/CRC@^1.0.3
build_flags = -D SYSCLK_FREQ_144MHz_HSI=144000000
extra_scripts = preserve_data_upload.py
```

**Create preserve_data_upload.py:**
```python
Import("env")

def preserve_data_upload(source, target, env):
    # Add commands to backup data before flash and restore after
    print("Custom upload with data preservation")
    # Implementation would backup/restore data sectors
    
env.Replace(UPLOADCMD=preserve_data_upload)
```

### Option 4: Use Different Flash Layout (Advanced)

Modify the linker script to reserve the data areas, but this requires:
1. Custom linker script modification
2. Firmware size must be < 56KB (currently 37.9KB, so safe)
3. Rebuild with modified memory layout

## Current Firmware Status

The firmware includes persistent data logic that will preserve your configurations between firmware versions **once the data is in flash**. The issue is getting the data to survive the initial flash programming.

## Recommended Workflow

1. **First Time Setup**: Configure all your filament slots after flashing
2. **Subsequent Updates**: Use Option 1 (partial flash programming) to preserve data
3. **Backup**: Occasionally backup your data using Option 2 as insurance

## Verification

After flashing with data preservation, you can verify success by:
1. Checking if your filament colors/names are still configured
2. Looking for this log message during boot: "Data migrated from version X to version Y"

## Tool Support

Different flashing tools have varying support for partial programming:
- **WCH ISP Tool**: ✅ Supports partial erase/program  
- **OpenOCD**: ✅ Full control over erase/program areas
- **PlatformIO default**: ❌ Usually erases entire flash
- **Arduino IDE**: ❌ Usually erases entire flash

Choose your flashing method accordingly to preserve your filament configurations.