#!/bin/bash
# Setup script for the BMCU370 project

set -e

echo "Installing dependencies..."

# Install PlatformIO
pip install platformio

# Install dfu-util
sudo apt-get update && sudo apt-get install -y dfu-util

echo "Dependencies installed."

echo "Performing initial build..."
./build.sh
echo "Initial build complete."
