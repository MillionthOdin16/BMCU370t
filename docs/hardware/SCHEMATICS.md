# BMCU370 Schematic Documentation

This document provides an overview of the official schematic files for the BMCU370 hardware design, dated September 11, 2025.

## Schematic File Overview

### Main Controller Board
**File**: `SCH_Schematic1_2025-09-11.pdf`  
**Purpose**: Primary controller board containing the CH32V203C8T6 microcontroller and main system functionality.

**Key Components**:
- **U2**: CH32V203C8T6 microcontroller (LQFP-48 package)
- **U1**: 24V to 3.3V switching voltage regulator (SOT-23-6)
- **U8-U11**: Motor driver ICs (ESOP-8 packages)
- **U7**: RS485 communication transceiver (SOP-8)
- **L1**: 10µH switching regulator inductor
- **CN1**: Main power/communication connector (4-pin, 3.0mm pitch)
- **CN2-CN5**: Channel connectors (10-pin, 2.0mm pitch)

### Sensor Interface Board  
**File**: `SCH_Schematic1_1_2025-09-11.pdf`  
**Purpose**: Sensor interface board for optical filament detection.

**Key Components**:
- **U7, U8**: ITR9606 optical interrupt sensors
- **U1, U6**: Signal conditioning amplifiers/comparators (SOIC-8)
- **LED1**: RGB status indicator (5050RGBC package)
- **LED2, LED3**: Status LEDs (0603 package)
- **CN2**: Main interface connector (10-pin, 2.0mm pitch)

## File Formats Available

### PDF Files (Print/Documentation)
- `SCH_Schematic1_2025-09-11.pdf` - Main controller board
- `SCH_Schematic1_1_2025-09-11.pdf` - Sensor interface board

### Vector Graphics (Web/Editing)
- `SCH_Schematic1_1-P1_2025-09-11.svg` - Main controller (scalable)
- `SCH_Schematic1_1_1-P1_2025-09-11.svg` - Sensor board (scalable)

### Raster Graphics (Web/Documentation)
- `SCH_Schematic1_1-P1_2025-09-11.png` - Main controller (high resolution)
- `SCH_Schematic1_1_1-P1_2025-09-11.png` - Sensor board (high resolution)

## Component Information Files

### Netlists
- `Netlist_Schematic1_2025-09-11.tel.txt` - Main board component list
- `Netlist_Schematic1_1_2025-09-11.tel.txt` - Sensor board component list

**Netlist Format**: Each netlist contains:
- Package types and physical dimensions
- Component values (resistors, capacitors, etc.)
- Part number references where applicable
- Net connections between components

### Design Files
- `pbmcu_c_hall.epro` - Complete EasyEDA project file
- `pcb_gerber_mainboard_enhanced_security_patch.zip` - Manufacturing files

## Key Circuit Sections

### Power Supply (Main Board)
- **Input**: 24V DC via CN1 connector
- **Regulation**: Switching regulator (U1) with 10µH inductor (L1)
- **Output**: 3.3V for all logic circuits
- **Protection**: Reverse polarity diode (D1), input filtering

### Motor Control (Main Board)
- **Drivers**: 4× H-bridge drivers (U8-U11) 
- **Current Sensing**: 680mΩ resistors (R3-R6)
- **Supply**: Direct 24V with protection and filtering
- **Control**: PWM from MCU with direction control

### Communication (Main Board)
- **Protocol**: RS485 differential signaling via U7
- **Protection**: ESD diode (D2), series resistors (R7, R8)
- **Termination**: 120Ω resistor (R9)
- **Interface**: 4-pin connector with power and differential pair

### Sensor Interface (Sensor Board)
- **Optical Sensors**: ITR9606 sensors with LED drive circuits
- **Signal Conditioning**: Amplification and comparison circuits
- **Digital Interface**: Compatible with MCU ADC inputs
- **Status Display**: RGB LED and discrete status indicators

## Pin Assignments Summary

### MCU Power and Programming
```
Pin 1, 9, 24, 36, 48: 3.3V supply pins
Pin 8, 23, 35, 47: Ground pins  
Pin 7: NRST (reset input)
Pin 44: BOOT0 (boot mode selection)
Pin 37: SWCLK (SWD programming clock)
Pin 34: SWIO (SWD programming data)
```

### Channel Interfaces (Pins per channel)
```
Channel 0: I2C=10/11, ADC=25/2, Motor=38/39, RGB=18
Channel 1: I2C=12/13, ADC=26/3, Motor=40/41, RGB=19
Channel 2: I2C=14/15, ADC=27/4, Motor=42/43, RGB=29
Channel 3: I2C=16/17, ADC=28/5, Motor=45/46, RGB=32
```

### Communication
```
Pin 21: UART2_TX (debug output)
Pin 22: UART2_RX (debug input)
Pin 30: UART1_TX (BambuBus via RS485)
Pin 31: UART1_RX (BambuBus via RS485)
Pin 33: RTS (RS485 direction control)
```

## Design Verification

The schematic design has been verified through:
- **Netlist extraction**: All connections verified in netlist files
- **Component specification**: Part numbers and values documented
- **Signal integrity**: Proper impedance control and grounding
- **Power analysis**: Current capacity and thermal design verified
- **Manufacturing readiness**: Gerber files generated and verified

## Usage Notes

### For Hardware Development
- Use PDF files for detailed component analysis and debugging
- Reference netlists for exact component values and connections
- EasyEDA project file for modifications and PCB layout changes

### For Firmware Development  
- Pin assignments documented for GPIO configuration
- ADC channel mapping for sensor interface programming
- Communication interface specifications for protocol implementation

### For Manufacturing
- Gerber files contain all PCB manufacturing data
- Component placement verified against schematic
- Assembly notes available in main HARDWARE.md documentation

## Related Documentation

- **[HARDWARE.md](HARDWARE.md)** - Complete hardware specifications and assembly guide
- **[Main README](../../README.md)** - Project overview and build instructions
- **[Assembly Documentation](../assembly/)** - Physical assembly procedures
- **[Firmware API](../firmware/API.md)** - Software interface specifications

For questions about schematic details or component specifications, refer to the netlist files or contact the hardware design team.