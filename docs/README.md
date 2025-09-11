# BMCU370 Documentation

This directory contains comprehensive documentation for the BMCU370 firmware project, organized by category for easy navigation.

## Directory Structure

### 📁 `firmware/`
Firmware-specific documentation including APIs, protocols, and development guides:
- **API.md** - BambuBus protocol and firmware API documentation
- **AUTOMATIC_DIRECTION_DETECTION.md** - Motor direction auto-detection system
- **MOTOR_DIRECTION_FIX.md** - Historical motor direction issues and solutions
- **CI-CD.md** - Continuous integration and deployment documentation
- **DEV-BUILD-QUICK-START.md** - Quick start guide for development builds

### 📁 `hardware/`
Hardware-related documentation and schematic files:
- **HARDWARE.md** - Complete hardware specifications, components, and assembly
- **SCHEMATICS.md** - Detailed schematic file navigation and component reference
- **Official Schematics (Sept 11, 2025)**:
  - `SCH_Schematic1_2025-09-11.pdf` - Main controller board schematic
  - `SCH_Schematic1_1_2025-09-11.pdf` - Sensor interface board schematic
  - Vector graphics in PNG and SVG formats for web viewing
- **Design Files**:
  - `Netlist_Schematic1_*.tel.txt` - Component netlists with part numbers
  - `pbmcu_c_hall.epro` - EasyEDA project file
  - `pcb_gerber_mainboard_enhanced_security_patch.zip` - PCB manufacturing files

### 📁 `assembly/`
Physical assembly and packaging documentation:
- **Additional information for BMCU 370C kit.pdf** - Supplementary kit information
- **BMCU-370C-TL-packaging-list.pdf** - Component packaging list
- **BMU370C Assembly Instructions.pdf** - Complete assembly instructions

### 📁 `tools/`
Development and programming tools:
- **WCHISPTool.zip** - WCH programming tool for firmware upload

## Quick Navigation

| Topic | File | Description |
|-------|------|-------------|
| **Getting Started** | [firmware/DEV-BUILD-QUICK-START.md](firmware/DEV-BUILD-QUICK-START.md) | Quick development setup |
| **API Reference** | [firmware/API.md](firmware/API.md) | Complete API documentation |
| **Hardware Setup** | [hardware/HARDWARE.md](hardware/HARDWARE.md) | Hardware configuration and schematics |
| **Schematic Files** | [hardware/](hardware/) | Official PDF/PNG/SVG schematics and netlists |
| **Assembly Guide** | [assembly/BMU370C Assembly Instructions.pdf](assembly/BMU370C%20Assembly%20Instructions.pdf) | Physical assembly |
| **CI/CD Pipeline** | [firmware/CI-CD.md](firmware/CI-CD.md) | Build automation |

## Documentation Standards

- **Markdown files** use `.md` extension with consistent formatting
- **PDF files** are organized by purpose (assembly, hardware, etc.)
- **Binary tools** are kept to minimum and documented with purpose
- **Version-specific info** is clearly marked with applicable firmware versions

For the main project documentation, see the [README.md](../README.md) in the root directory.