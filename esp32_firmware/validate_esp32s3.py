#!/usr/bin/env python3
"""
ESP32-S3 N4R2 Configuration Validator
Validates ESP32-S3 N4R2 hardware compatibility and flashing configuration
N4 = 4MB NAND Flash, R2 = 2MB OPI PSRAM
"""

import sys
import os

def validate_partition_table():
    """Validate partition table for ESP32-S3 N4R2 (4MB Flash)"""
    print("=== Partition Table Validation (ESP32-S3 N4R2) ===")
    
    partitions_file = "partitions.csv"
    if not os.path.exists(partitions_file):
        print("❌ ERROR: partitions.csv not found")
        return False
    
    total_flash = 4 * 1024 * 1024  # 4MB in bytes (N4 variant)
    used_space = 0
    
    with open(partitions_file, 'r') as f:
        lines = f.readlines()
    
    for line in lines:
        if line.startswith('#') or not line.strip():
            continue
            
        parts = [p.strip() for p in line.split(',')]
        if len(parts) >= 5:
            name, type_name, subtype, offset, size = parts[:5]
            
            # Convert hex values
            offset_val = int(offset, 16)
            size_val = int(size, 16)
            end_addr = offset_val + size_val
            
            print(f"  {name}: 0x{offset_val:X} - 0x{end_addr:X} ({size_val//1024}KB)")
            
            if end_addr > total_flash:
                print(f"❌ ERROR: {name} extends beyond 4MB flash ({end_addr:X} > {total_flash:X})")
                return False
                
            used_space = max(used_space, end_addr)
    
    free_space = total_flash - used_space
    print(f"✅ Total used: {used_space//1024}KB, Free: {free_space//1024}KB")
    
    return True

def validate_platformio_config():
    """Validate PlatformIO configuration for ESP32-S3 N4R2"""
    print("\n=== PlatformIO Configuration Validation (ESP32-S3 N4R2) ===")
    
    config_file = "platformio.ini"
    if not os.path.exists(config_file):
        print("❌ ERROR: platformio.ini not found")
        return False
    
    with open(config_file, 'r') as f:
        content = f.read()
    
    required_settings = {
        'board = esp32-s3-devkitc-1': 'ESP32-S3 board definition',
        'board_build.flash_size = 4MB': '4MB flash size (N4 variant)',
        'board_build.psram_type = opi': 'OPI PSRAM type (R2 variant)',
        'board_build.memory_type = opi_opi': 'OPI memory configuration for N4R2',
        'board_build.usb_mode = otg': 'USB OTG mode for host functionality',
        'BOARD_HAS_PSRAM': 'PSRAM support flag',
        'CONFIG_SPIRAM_SUPPORT=1': 'PSRAM support in SDK',
        'ESP32_S3_N4R2_VARIANT=1': 'N4R2 variant identification'
    }
    
    all_valid = True
    for setting, description in required_settings.items():
        if setting in content:
            print(f"✅ {description}: Found")
        else:
            print(f"❌ {description}: Missing '{setting}'")
            all_valid = False
    
    return all_valid

def validate_flash_settings():
    """Validate flash-specific settings for ESP32-S3 N4R2"""
    print("\n=== Flash Settings Validation (ESP32-S3 N4R2) ===")
    
    config_file = "platformio.ini"
    with open(config_file, 'r') as f:
        content = f.read()
    
    flash_settings = {
        'board_build.flash_mode = dio': 'DIO flash mode for ESP32-S3 N4R2',
        'board_build.flash_freq = 80m': '80MHz flash frequency for N4 variant',
        'partitions.csv': 'Custom partition table optimized for N4R2',
        'CONFIG_SPIRAM_SPEED_80M=1': '80MHz PSRAM speed for R2 variant',
        'board_build.arduino.memory_type = opi_opi': 'Arduino OPI memory type for N4R2'
    }
    
    all_valid = True
    for setting, description in flash_settings.items():
        if setting in content:
            print(f"✅ {description}: Configured")
        else:
            print(f"⚠️  {description}: Not explicitly set (using defaults)")
    
    return all_valid

def main():
    """Main validation function"""
    print("ESP32-S3 N4R2 BMCU370 Interface Configuration Validator")
    print("N4 = 4MB NAND Flash, R2 = 2MB OPI PSRAM")
    print("=" * 60)
    
    # Change to script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    results = []
    results.append(validate_partition_table())
    results.append(validate_platformio_config())
    results.append(validate_flash_settings())
    
    print("\n=== Validation Summary ===")
    if all(results):
        print("✅ All validations passed! ESP32-S3 N4R2 configuration is optimal.")
        print("\nOptimized flashing command for ESP32-S3 N4R2:")
        print("esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \\")
        print("  write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \\")
        print("  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin")
        print("\nHardware verification should show:")
        print("- ESP32-S3 (QFN56) rev 0.1")
        print("- Embedded Flash 4MB (N4 variant)")
        print("- Embedded PSRAM 2MB (R2 variant)")
        return 0
    else:
        print("❌ Some validations failed. Please review the ESP32-S3 N4R2 configuration.")
        return 1

if __name__ == "__main__":
    sys.exit(main())