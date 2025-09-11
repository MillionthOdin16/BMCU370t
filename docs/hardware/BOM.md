# BMCU370 Bill of Materials (BOM)

This document provides a comprehensive bill of materials for building the BMCU370 multi-material unit, including all electronic components, hardware, and manufacturing requirements.

## Overview

The BMCU370 consists of two main PCB assemblies:
1. **Main Controller Board** - Contains microcontroller, power management, motor drivers, and communication
2. **Sensor Interface Board** - Contains optical sensors for filament detection and signal conditioning

## Main Controller Board Components

### Microcontroller and Core Processing

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U2 | CH32V203C8T6 | LQFP-48 | 1 | RISC-V 144MHz, 64KB Flash, 20KB RAM | Main microcontroller | WCH |
| C1 | - | C0603 | 2 | 47pF | Crystal load capacitors | Generic |
| Y1 | - | HC-49S | 1 | 8MHz | Main crystal oscillator | Generic |

### Power Management

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U1 | - | SOT-23-6 | 1 | 24V to 3.3V, 1A+ | Switching voltage regulator | Generic |
| L1 | - | IND-SMD_L7.3-W6.8 | 1 | 10µH | Switching regulator inductor | Generic |
| C2,C3,C4 | - | C0805 | 3 | 22µF | Power supply filtering capacitors | Generic |
| C5-C12 | - | C0603 | 8 | 100nF | Decoupling capacitors (logic) | Generic |
| C13-C16 | - | C0603 | 4 | 100nF | Decoupling capacitors (motor supply) | Generic |
| D1 | - | SMB | 1 | - | Reverse polarity protection diode | Generic |

### Motor Control

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U8,U9,U10,U11 | - | ESOP-8 | 4 | H-bridge motor driver | Motor driver ICs | Generic |
| R3,R4,R5,R6 | - | R1210 | 4 | 680mΩ | Current sense resistors | Generic |

### Communication Interface

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U7 | - | SOP-8 | 1 | RS485 transceiver | RS485 communication chip | Generic |
| R7,R8 | - | R0603 | 2 | 10Ω | Line protection resistors | Generic |
| R9 | - | R0603 | 1 | 120Ω | RS485 termination resistor | Generic |
| D2 | - | SOT-23-3 | 1 | - | ESD protection diode | Generic |

### User Interface

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| LED1 | WS2812B | LED-SMD_4P-L5.0-W5.0 | 1 | 5050RGBC | Addressable RGB LED | WorldSemi |
| SW1,SW2 | B3U-1000PM | KEY-SMD | 2 | Tactile switch | Reset and Boot switches | Omron |
| USB1 | - | USB-C | 1 | USB 2.0 Full Speed | Programming and communication port | Generic |

### Signal Conditioning

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| RN1,RN2,RN3 | - | RES-ARRAY-SMD_0603-8P | 3 | 10kΩ | I2C pull-up resistor arrays | Generic |
| R1 | - | R0603 | 1 | 68kΩ | Voltage reference resistor | Generic |
| R2 | - | R0603 | 1 | 15kΩ | Voltage reference resistor | Generic |

### Connectors - Main Board

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| CN1 | HC-MX3.0-2X2AW | CONN-TH_4P-P3.00 | 1 | 4-pin, 3.0mm pitch | Main power/communication connector | Generic |
| CN2,CN3,CN4,CN5 | 2.0-10P-WT | CONN-SMD_10P-P2.00 | 4 | 10-pin, 2.0mm pitch | Channel interface connectors | Generic |
| H1,H2 | - | HDR-TH_4P-P2.54-V-M | 2 | 4-pin, 2.54mm pitch | Programming headers | Generic |

## Sensor Interface Board Components

### Optical Sensors

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U7,U8 | ITR9606 | OPTO-TH_4P | 2 | Optical interrupt sensor | Filament detection sensors | Everlight |
| R1,R2 | - | R0603 | 2 | 470Ω | LED current limiting resistors | Generic |
| R3,R4 | - | R0603 | 2 | 1kΩ | Sensor pull-up resistors | Generic |

### Signal Processing

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| U1 | - | SOIC-8 | 1 | Op-amp/Comparator | Signal conditioning IC | Generic |
| U6 | - | SOIC-8 | 1 | Op-amp/Comparator | Signal conditioning IC | Generic |
| RN1,RN2 | - | RES-ARRAY-SMD_0603-8P | 2 | 10kΩ | Signal conditioning resistor arrays | Generic |

### Status Indicators

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| LED1 | WS2812B | LED-SMD_4P-L5.0-W5.0 | 1 | 5050RGBC | Addressable RGB LED | WorldSemi |
| LED2,LED3 | - | LED_0603 | 2 | Status indicator LEDs | Status LEDs | Generic |

### Power and Signal

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| C6,C7,C8 | - | C0603 | 3 | 100nF | Decoupling capacitors | Generic |

### Connectors - Sensor Board

| Ref Des | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|---------|-------------|---------|-----|---------------------|-------------|--------------|
| CN2 | 2.0-10P-WT | CONN-SMD_10P-P2.00 | 1 | 10-pin, 2.0mm pitch | Main interface connector | Generic |

## External Sensors and Components

### Hall Sensors (per channel - 4 sets required)

| Component | Part Number | Package | Qty | Value/Specification | Description | Manufacturer |
|-----------|-------------|---------|-----|---------------------|-------------|--------------|
| AS5600 | AS5600-ASOM | SOIC-8 | 4 | 12-bit magnetic encoder | Rotary position sensors | AMS |
| Magnet | - | Disc/Ring | 4 | Diametrically magnetized, Ø6-8mm | Neodymium magnets for AS5600 | Generic |

### Motor Assemblies (per channel - 4 sets required)

| Component | Part Number | Qty | Value/Specification | Description | Manufacturer |
|-----------|-------------|-----|---------------------|-------------|--------------|
| Stepper Motor | - | 4 | 24V, ~200-400 steps/rev | Filament feed motors | Generic |
| Gear Assembly | - | 4 | Reduction ratio varies | Motor to filament gear train | Custom |
| Motor Cable | - | 4 | 2-conductor, 18AWG | Motor power cables | Generic |

### RGB LED Strips (per channel - 4 sets required)

| Component | Part Number | Qty | Value/Specification | Description | Manufacturer |
|-----------|-------------|-----|---------------------|-------------|--------------|
| WS2812B Strip | - | 4 | 2 LEDs per channel (default) | Addressable RGB LED strips | WorldSemi |
| LED Cable | - | 4 | 3-conductor, 22AWG | LED control cables | Generic |

## Mechanical Components

### PCB Hardware

| Component | Specification | Qty | Description |
|-----------|---------------|-----|-------------|
| PCB - Main Board | 2-4 layer, 1.6mm thick | 1 | Main controller PCB |
| PCB - Sensor Board | 2 layer, 1.6mm thick | 4 | Sensor interface PCBs |
| M2 Screws | M2×6mm | 6 | PCB mounting screws |
| M2 Standoffs | M2×5mm | 6 | PCB mounting standoffs |

### Test Points

| Ref Des | Package | Qty | Description |
|---------|---------|-----|-------------|
| TP1,TP2,TP3 | M2螺丝 | 3 | Mounting holes - Main Board |
| TP6-TP10 | Test-Point-0.5mm | 5 | Test points - Sensor Board |

## Cable and Wiring

### Internal Wiring

| Cable Type | Specification | Qty | Length | Description |
|------------|---------------|-----|---------|-------------|
| I2C Cable | 4-conductor, 24AWG | 4 | 100-300mm | Hall sensor communication |
| Motor Cable | 2-conductor, 18AWG | 4 | 100-300mm | Motor power connections |
| Sensor Cable | 10-conductor ribbon | 4 | 100-300mm | Sensor board to main board |
| RGB Cable | 3-conductor, 22AWG | 4 | 100-300mm | RGB LED control |

### External Connections

| Cable Type | Specification | Qty | Length | Description |
|------------|---------------|-----|---------|-------------|
| Power Cable | 2-conductor, 16AWG | 1 | 500mm+ | 24V power input |
| BambuBus Cable | Shielded twisted pair | 1 | 500mm+ | RS485 communication |
| Programming Cable | USB-C | 1 | 1000mm | Firmware programming |

## Power Supply Requirements

### External Power Supply

| Specification | Value | Description |
|---------------|-------|-------------|
| Input Voltage | 24V DC ±10% | Main power input |
| Current Capacity | 2-3A minimum | Peak motor current + control circuits |
| Ripple | <100mV p-p | Power quality requirement |
| Protection | Overcurrent, short circuit | Safety features |
| Connector | Compatible with CN1 | Secure connection |

### Power Consumption Estimates

| Component | Current Draw | Description |
|-----------|--------------|-------------|
| MCU Core | ~50mA | CH32V203 @ 144MHz |
| Motor Drivers | 100-2000mA per channel | Variable with load |
| Hall Sensors | ~10mA total | 4× AS5600 sensors |
| Optical Sensors | ~20mA total | ITR9606 LED drive |
| RGB LEDs | ~60mA per LED | Maximum brightness |
| Support Circuits | ~30mA | Regulators, communication |

## Manufacturing and Assembly

### PCB Specifications

| Parameter | Main Board | Sensor Board | Notes |
|-----------|------------|--------------|-------|
| Layers | 2-4 | 2 | Depends on complexity |
| Thickness | 1.6mm | 1.6mm | Standard PCB thickness |
| Copper Weight | 1-2oz | 1oz | Heavier copper for power traces |
| Surface Finish | HASL/ENIG | HASL/ENIG | Lead-free compatible |
| Solder Mask | Green | Green | Standard color |
| Silkscreen | White | White | Component labels |

### Assembly Requirements

| Process | Components | Notes |
|---------|------------|-------|
| SMT Assembly | All surface mount components | Reflow soldering |
| Through-hole | Connectors, headers, switches | Wave soldering or hand assembly |
| Programming | Initial firmware load | Via SWD interface |
| Testing | Functional verification | All channels tested |

### Quality Control

| Test | Requirement | Method |
|------|-------------|--------|
| Power Supply | All voltage rails within spec | Multimeter measurement |
| Communication | BambuBus functionality | Protocol testing |
| Motor Control | All 4 channels operational | Motion testing |
| Sensor Function | Hall and optical sensors | Position/detection testing |
| LED Control | RGB functionality | Visual inspection |

## Component Sourcing

### Recommended Suppliers

| Component Category | Primary Supplier | Alternative |
|-------------------|------------------|-------------|
| Microcontroller | WCH | LCSC, Digikey |
| Passive Components | LCSC, JLCPCB | Digikey, Mouser |
| Connectors | JST, Molex | Generic alternatives |
| LEDs | WorldSemi (WS2812B) | SK6812, APA102 |
| Sensors | AMS (AS5600) | Digikey, Mouser |
| Optical Sensors | Everlight (ITR9606) | OSRAM, Vishay |

### Cost Estimates (USD, approximate)

| Category | Unit Cost | Extended Cost (4 channels) |
|----------|-----------|----------------------------|
| Main Board PCB + Assembly | $15-25 | $15-25 |
| Sensor Board PCB + Assembly | $5-8 | $20-32 |
| Microcontroller | $2-3 | $2-3 |
| Hall Sensors | $3-4 each | $12-16 |
| Motor Drivers | $1-2 each | $4-8 |
| Passive Components | $5-10 | $5-10 |
| Connectors | $5-10 | $5-10 |
| **Total Electronics** | **~$35-50** | **~$35-50** |

*Note: Costs exclude motors, magnets, mechanical assembly, and enclosure*

## Design Files and Manufacturing

### Required Files for Manufacturing

| File Type | Location | Description |
|-----------|----------|-------------|
| Gerber Files | `pcb_gerber_mainboard_enhanced_security_patch.zip` | PCB manufacturing |
| Pick & Place | Generated from EasyEDA | SMT assembly |
| Bill of Materials | This document | Component sourcing |
| Assembly Drawings | From schematic PDFs | Assembly guidance |
| Test Procedures | Firmware documentation | Quality control |

### EasyEDA Project Files

| File | Description |
|------|-------------|
| `pbmcu_c_hall.epro` | Complete EasyEDA project |
| Schematic PDFs | Human-readable schematics |
| Netlist files | Component and connection verification |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-01-27 | Initial BOM creation from schematic analysis |

## Notes

1. **Component Substitution**: Most passive components can be substituted with equivalent values from different manufacturers. Maintain voltage and current ratings.

2. **Programming Requirements**: Initial firmware programming requires SWD interface access via H1 or H2 headers.

3. **Magnet Polarity**: Critical for motor direction detection. All magnets should have consistent polarity orientation (same pole facing sensor).

4. **Quality Control**: Test all channels individually before final assembly. Motor direction learning will occur automatically during first use.

5. **Safety**: Use appropriate fuses and protection for 24V motor supply. Follow electrical safety guidelines for high-current circuits.

6. **Environmental**: Consider operating temperature range and humidity for component selection in harsh environments.

For technical support and component questions, refer to the complete hardware documentation in [HARDWARE.md](HARDWARE.md) and schematic files in [SCHEMATICS.md](SCHEMATICS.md).