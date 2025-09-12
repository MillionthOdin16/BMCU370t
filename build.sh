#!/bin/bash
# BMCU370 Firmware Build Script
# Builds both BMCU370 and ESP32 firmware targets

set -e

echo "======================================"
echo "BMCU370 Firmware Build Script"
echo "======================================"

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "Error: PlatformIO CLI not found. Please install it first:"
    echo "pip install platformio"
    exit 1
fi

# Create output directory
mkdir -p build-output

echo ""
echo "Building BMCU370 firmware..."
echo "=============================="
pio run --environment genericCH32V203C8T6

if [ $? -eq 0 ]; then
    echo "✅ BMCU370 build successful"
    cp .pio/build/genericCH32V203C8T6/firmware.bin build-output/bmcu370_firmware.bin
    cp .pio/build/genericCH32V203C8T6/firmware.hex build-output/bmcu370_firmware.hex
    cp .pio/build/genericCH32V203C8T6/firmware.elf build-output/bmcu370_firmware.elf
else
    echo "❌ BMCU370 build failed"
    exit 1
fi

echo ""
echo "Building ESP32 firmware..."
echo "=========================="
cd esp32_firmware
pio run --environment esp32s3

if [ $? -eq 0 ]; then
    echo "✅ ESP32 build successful"
    cp .pio/build/esp32s3/firmware.bin ../build-output/esp32_firmware.bin
    cp .pio/build/esp32s3/firmware.elf ../build-output/esp32_firmware.elf
    
    # Copy additional ESP32 files if they exist
    if [ -f .pio/build/esp32s3/bootloader.bin ]; then
        cp .pio/build/esp32s3/bootloader.bin ../build-output/esp32_bootloader.bin
    fi
    if [ -f .pio/build/esp32s3/partitions.bin ]; then
        cp .pio/build/esp32s3/partitions.bin ../build-output/esp32_partitions.bin
    fi
else
    echo "❌ ESP32 build failed"
    exit 1
fi

# Build filesystem
echo ""
echo "Building ESP32 filesystem..."
echo "============================="
pio run --target buildfs --environment esp32s3

if [ $? -eq 0 ]; then
    echo "✅ ESP32 filesystem build successful"
    if [ -f .pio/build/esp32s3/littlefs.bin ]; then
        cp .pio/build/esp32s3/littlefs.bin ../build-output/esp32_littlefs.bin
    fi
else
    echo "⚠️  ESP32 filesystem build failed (optional)"
fi

cd ..

# Create build info
echo ""
echo "Creating build information..."
echo "============================="

cat > build-output/BUILD_INFO.txt << EOF
BMCU370 USB-ESP Interface Firmware Build
=========================================

Build Date: $(date)
Build Host: $(hostname)
Git Commit: $(git rev-parse HEAD 2>/dev/null || echo "Unknown")
Git Branch: $(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "Unknown")

Files Built:
- bmcu370_firmware.bin - BMCU370 main firmware (DFU flashable)
- bmcu370_firmware.hex - BMCU370 firmware (Intel HEX format)
- bmcu370_firmware.elf - BMCU370 firmware with debug symbols
- esp32_firmware.bin - ESP32-S3 main application
- esp32_bootloader.bin - ESP32 bootloader
- esp32_partitions.bin - ESP32 partition table
- esp32_littlefs.bin - ESP32 web interface filesystem

Flash Instructions:
===================

BMCU370 (DFU Mode):
  dfu-util -a 0 -D bmcu370_firmware.bin

ESP32-S3 (Complete):
  esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z \\
    0x0000 esp32_bootloader.bin \\
    0x8000 esp32_partitions.bin \\
    0x10000 esp32_firmware.bin \\
    0x110000 esp32_littlefs.bin

ESP32-S3 (Firmware Only):
  esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z 0x10000 esp32_firmware.bin

EOF

echo ""
echo "======================================"
echo "✅ Build completed successfully!"
echo "======================================"
echo "Output files in: build-output/"
echo ""
echo "BMCU370 files:"
echo "  - bmcu370_firmware.bin"
echo "  - bmcu370_firmware.hex"
echo "  - bmcu370_firmware.elf"
echo ""
echo "ESP32 files:"
echo "  - esp32_firmware.bin"
echo "  - esp32_bootloader.bin (if available)"
echo "  - esp32_partitions.bin (if available)"
echo "  - esp32_littlefs.bin (if available)"
echo ""
echo "Documentation:"
echo "  - BUILD_INFO.txt"
echo ""
echo "Ready for deployment! 🚀"