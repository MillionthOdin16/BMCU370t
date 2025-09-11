# Hardware Configuration Guide

This guide provides detailed information about the hardware setup and configuration for the BMCU370 system, based on the official schematics dated September 11, 2025.

## Overview

The BMCU370 is built around the CH32V203C8T6 microcontroller, a RISC-V based MCU designed for embedded applications. The hardware consists of two main PCB modules:

1. **Main Controller Board** - Contains the CH32V203 MCU, power management, motor drivers, and communication interfaces
2. **Sensor Interface Board** - Contains optical sensors (ITR9606) for filament detection and additional interface circuitry

## Schematic Documentation

The complete hardware design is documented in the following schematic files:

- **Main Schematic** (Controller Board): `SCH_Schematic1_2025-09-11.pdf`
- **Sensor Board Schematic**: `SCH_Schematic1_1_2025-09-11.pdf`
- **Vector Graphics**: Available in SVG and PNG formats
- **Netlists**: Component and connection details in `Netlist_Schematic1_*.tel.txt`
- **PCB Design**: EasyEDA project file `pbmcu_c_hall.epro`
- **Manufacturing**: Gerber files in `pcb_gerber_mainboard_enhanced_security_patch.zip`

## Microcontroller Specifications

- **CPU**: RISC-V 32-bit core @ 144MHz
- **Flash Memory**: 64KB
- **RAM**: 20KB
- **Package**: LQFP-48 (7.0×7.0mm, 0.50mm pitch)
- **GPIO Pins**: 37 available
- **Timers**: 4x 16-bit, 2x watchdog
- **Communication**: 3x USART, 2x I2C, 2x SPI
- **ADC**: 12-bit, 10 channels with DMA support
- **Programming**: SWD interface (SWCLK, SWIO)
- **Reset**: External reset with RC circuit
- **Boot Mode**: BOOT0/BOOT1 configuration pins

## Main Board Component Specifications

### Power Management
- **Input Power**: 24V via CONN-TH_4P-P3.00 connector (CN1)
- **Voltage Regulator**: SOT-23-6 package (U1) - 24V to 3.3V conversion
- **Power Filter**: 10µH inductor (L1), 22µF capacitors (C2, C3, C4)
- **Decoupling**: 100nF capacitors (C5-C16) throughout the board
- **Crystal**: 47pF load capacitors (C1) for main oscillator

### Motor Drivers
- **Driver ICs**: 4x ESOP-8 packages (U8, U9, U10, U11) 
- **Current Sensing**: 680mΩ sense resistors (R3, R4, R5, R6)
- **Output Filtering**: 100nF capacitors (C13-C16) on 24V motor supply
- **Control Signals**: H-bridge control from MCU pins
- **Protection**: SMB diode (D1) for reverse polarity protection

### Communication Interface
- **RS485 Transceiver**: SOP-8 package (U7)
- **Termination**: 120Ω resistor (R9) 
- **Line Protection**: 10Ω series resistors (R7, R8)
- **ESD Protection**: SOT-23-3 diode (D2)
- **External Interface**: 4-pin connector (CN1) for RS485 bus

### Sensor Interfaces
- **Hall Sensor Connectors**: 4x CONN-SMD_10P-P2.00 (CN2, CN3, CN4, CN5)
- **I2C Pull-ups**: 3x 8-resistor arrays (RN1, RN2, RN3) - 10kΩ values
- **Voltage References**: Dedicated VREF networks for sensor power

### RGB LED Control  
- **Status LED**: 5050RGBC package (LED1) on main board
- **PWM Outputs**: 4 channels for external RGB strips (PA11, PA8, PB1, PB0)
- **Level Shifting**: Built into MCU GPIO configuration

### User Interface
- **Reset Button**: B3U-1000PM tactile switch (SW1)
- **Boot Button**: B3U-1000PM tactile switch (SW2) 
- **Status Indicators**: Accessible via main RGB LED
- **Programming Header**: 4-pin 2.54mm header (H1, H2) for SWD

## Sensor Board Component Specifications

### Optical Sensors
- **Sensor Type**: ITR9606 optical sensors (U7, U8)
- **Package**: Through-hole 4-pin
- **Function**: Filament presence detection
- **LED Drive**: 470Ω current limiting resistors (R1, R2)
- **Pull-up Resistors**: 1kΩ values (R3, R4)
- **Supply Voltage**: 3.3V operation

### Signal Processing
- **Amplifier/Comparator**: SOIC-8 and SOIC-8 packages (U1, U6)
- **Reference Voltage**: Precision voltage reference network
- **Output Buffering**: Multiple 10kΩ resistor arrays (RN1, RN2)
- **Decoupling**: 100nF capacitors (C6, C7, C8)

### Interface Connector
- **Main Connector**: CONN-SMD_10P-P2.00 (CN2)
- **Signals**: 3.3V, GND, sensor outputs, I2C, RGB control
- **Status LEDs**: 0603 package indicators (LED2, LED3)
- **RGB LED**: 5050RGBC package (LED1)

### Test Points
- **Test Points**: 0.5mm test points (TP6-TP10)
- **Functions**: K_ONLINE, K_PULL, RGB_OUT, 3.3V, GND
- **Accessibility**: For production testing and debugging

## Pin Assignment and Signal Mapping

### RGB LED Outputs (PWM)
Based on the schematic netlists, the RGB LED control pins are mapped as follows:
```
RGB_OUT1 (Channel 0): U2.18 (MCU pin 18) - Connected to CN2.4
RGB_OUT2 (Channel 1): U2.19 (MCU pin 19) - Connected to CN3.4  
RGB_OUT3 (Channel 2): U2.29 (MCU pin 29) - Connected to CN4.4
RGB_OUT4 (Channel 3): U2.32 (MCU pin 32) - Connected to CN5.4
SYS_RGB (Main Board): U2.6 (MCU pin 6) - Connected to LED1.4
```

### I2C Hall Sensor Interfaces
Each channel has dedicated I2C lines with 10kΩ pull-up resistors:
```
Channel 0: SCL=U2.10 (MCU pin 10), SDA=U2.11 (MCU pin 11) - CN2.5/CN2.6
Channel 1: SCL=U2.12 (MCU pin 12), SDA=U2.13 (MCU pin 13) - CN3.5/CN3.6
Channel 2: SCL=U2.14 (MCU pin 14), SDA=U2.15 (MCU pin 15) - CN4.5/CN4.6
Channel 3: SCL=U2.16 (MCU pin 16), SDA=U2.17 (MCU pin 17) - CN5.5/CN5.6
```

### ADC Inputs (Pressure Sensors)
Analog inputs for filament detection pressure sensors:
```
K_PULL1: U2.25 (MCU pin 25) - Connected to CN2.1
K_PULL2: U2.26 (MCU pin 26) - Connected to CN3.1
K_PULL3: U2.27 (MCU pin 27) - Connected to CN4.1
K_PULL4: U2.28 (MCU pin 28) - Connected to CN5.1

K_ONLINE1: U2.2 (MCU pin 2) - Connected to CN2.2
K_ONLINE2: U2.3 (MCU pin 3) - Connected to CN3.2
K_ONLINE3: U2.4 (MCU pin 4) - Connected to CN4.2
K_ONLINE4: U2.5 (MCU pin 5) - Connected to CN5.2
```

### Motor Control (PWM H-Bridge)
Bidirectional motor control with H-bridge drivers:
```
MOTOR1: H=U2.38 (pin 38), L=U2.39 (pin 39) → U8 → CN2.9/CN2.10
MOTOR2: H=U2.40 (pin 40), L=U2.41 (pin 41) → U9 → CN3.9/CN3.10  
MOTOR3: H=U2.42 (pin 42), L=U2.43 (pin 43) → U10 → CN4.9/CN4.10
MOTOR4: H=U2.45 (pin 45), L=U2.46 (pin 46) → U11 → CN5.9/CN5.10
```

### Communication Interfaces
```
USART1 (BambuBus): 
  - TX: U2.30 (MCU pin 30) → U7.4 → RS485 transceiver
  - RX: U2.31 (MCU pin 31) → U7.1 → RS485 transceiver  
  - RTS: U2.33 (MCU pin 33) → U7.2/U7.3 → Direction control

USART2 (Debug):
  - TX: U2.21 (MCU pin 21) → H1.1 (EXIT_TX)
  - RX: U2.22 (MCU pin 22) → H1.2 (EXIT_RX)
```

### Programming and Debug Interface
```
SWD Programming:
  - SWCLK: U2.37 (MCU pin 37) → H2.2
  - SWIO: U2.34 (MCU pin 34) → H2.1
  - 3.3V: H1.3, H2.3
  - GND: H1.4, H2.4

Reset Circuit:
  - NRST: U2.7 (MCU pin 7) → SW1, C6, RN3.4
  - BOOT0: U2.44 (MCU pin 44) → SW2, RN3.1  
  - BOOT1: U2.20 (MCU pin 20) → RN3.2
```

### Power Distribution
```
24V Input: CN1.1 → L1, U1 → Voltage regulation
3.3V Rails: Multiple distribution points with decoupling
GND: Star ground configuration with multiple connection points
```

## Component Specifications

### AS5600 Hall Sensors

**Purpose**: Rotary position sensing for filament movement tracking

**Specifications**:
- 12-bit resolution (4096 positions per revolution)
- Contactless magnetic sensing
- I2C interface (address 0x36)
- 3.3V operation (as shown in schematics)
- Response time: <1ms
- Supply current: ~10mA per sensor

**Installation**:
- Mount sensor PCB near rotating magnet
- Magnet should be diametrically magnetized
- 0.5-3mm air gap recommended
- Ensure magnet is centered over sensor
- **CRITICAL: Magnet polarity orientation must be consistent across all channels**
  - Install all magnets with the same pole (North or South) facing the sensor
  - Inconsistent magnet polarity will cause motor direction detection errors
  - Mark magnets during assembly to ensure consistent orientation

### Pressure Sensors (ITR9606 Optical)

**Purpose**: Filament insertion/removal detection  

**Type**: ITR9606 optical interrupt sensors (through-hole package)
**Interface**: Analog output via optical coupling
**Drive Circuit**: 
- LED current: 470Ω limiting resistors (3.3V supply)
- Pull-up resistors: 1kΩ on sensor outputs
- Signal conditioning via dedicated amplifier/comparator ICs

**Operating Characteristics**:
- Supply voltage: 3.3V 
- Detection range: Optimized for filament presence
- Response time: <1ms typical
- Output: Digital signal levels compatible with MCU ADC

### RGB LEDs (NeoPixel Compatible)

**Purpose**: Visual status indication

**Main Board LED**:
- Package: 5050RGBC (5.0×5.0mm, 4-pin)
- Type: WS2812B or compatible addressable RGB LED
- Supply: 3.3V logic, 5V power (if external power used)
- Control: Single data line from MCU pin U2.6

**Channel LEDs** (External):
- Interface: PWM control via RGB_OUT1-4 signals
- Drive capability: Determined by external driver circuits
- 24-bit color depth (8 bits per channel)
- Data transmission: 800kHz NeoPixel protocol

**LED Configuration**:
- Up to 2 LEDs per filament channel (configurable in firmware)
- 1 LED for main board status indication
- Total: 9 LEDs in default configuration

### Motor Drivers

**Driver ICs**: 4× H-bridge motor drivers in ESOP-8 packages (U8-U11)

**Specifications**:
- Supply voltage: 24V motor supply, 3.3V logic
- Current sensing: 680mΩ sense resistors for overcurrent protection
- Control: PWM H-bridge control (High/Low side switching)
- Protection: Current limiting, thermal protection
- Output filtering: 100nF capacitors on each motor output

**Control Interface**:
- Direction control: H-bridge high/low side control
- Speed control: PWM frequency and duty cycle
- Current feedback: Via sense resistors for load monitoring

### Power Supply Circuit

**Input**: 24V via 4-pin connector (CN1)
- Connector: Through-hole, 3.0mm pitch (CONN-TH_4P-P3.00)
- Protection: SMB diode (D1) for reverse polarity protection
- Filtering: 22µF input capacitors (C2, C3, C4)

**Regulation**: 
- Primary regulator: SOT-23-6 package (U1) 
- Input: 24V, Output: 3.3V
- Inductor: 10µH switching regulator inductor (L1)
- Output filtering: Multiple 100nF and 22µF capacitors

**Distribution**:
- 3.3V: MCU, sensors, logic circuits, LED control
- 24V: Motor drivers, input to voltage regulator
- Current capacity: Designed for ~2A motor loads plus control circuits

### Communication Interface

**RS485 Transceiver**: SOP-8 package (U7)
- Standard: RS485 differential signaling
- Termination: 120Ω termination resistor (R9)
- Line protection: 10Ω series resistors (R7, R8)
- ESD protection: SOT-23-3 TVS diode (D2)
- Direction control: Automatic via RTS signal from MCU

**External Connector**: 4-pin through-hole connector (CN1)
- Pin 1: 24V power
- Pin 2: RS485_B (differential pair)  
- Pin 3: GND
- Pin 4: RS485_A (differential pair)

## Power Requirements

### Supply Voltages
- **Primary Input**: 24V DC via CN1 connector (recommended 1-2A capacity)
- **MCU and Logic**: 3.3V regulated (via on-board switching regulator U1)
- **Motor Supply**: 24V direct (with current sensing and protection)
- **Sensor Supply**: 3.3V regulated for all sensors and interfaces

### Current Consumption Estimates
- **MCU Core**: ~50mA @ 144MHz (CH32V203C8T6)
- **Motor Drivers**: Variable, typically 100-2000mA per channel under load
- **Hall Sensors**: ~10mA total for all 4 AS5600 sensors
- **Optical Sensors**: ~20mA total (ITR9606 LED drive current)
- **RGB LEDs**: ~20mA per color channel (60mA max per LED when all colors active)
- **Support Circuits**: ~30mA (regulators, communication, etc.)

### Power Supply Circuit Details

**Input Protection**:
- Reverse polarity protection via SMB diode (D1)
- Input filtering with 22µF capacitors (C2, C3, C4)
- 24V distribution to motor drivers with 100nF decoupling (C13-C16)

**3.3V Regulation**:
- Switching regulator topology with 10µH inductor (L1)
- Output filtering and decoupling throughout board
- Multiple 100nF capacitors (C5-C12) for digital circuits
- Separate 47pF capacitors (C1) for crystal oscillator

**Current Sensing**:
- 680mΩ sense resistors (R3-R6) for motor current monitoring
- Enables overcurrent protection and load feedback
- Precision sensing for motor control algorithms

### Power Distribution Strategy
- **Star ground configuration** minimizes ground loops
- **Separate analog and digital supplies** where applicable  
- **Dedicated motor supply rails** with filtering
- **Multiple decoupling capacitors** placed close to ICs
- **Wide traces** for power distribution (>0.5mm for 24V, >0.3mm for 3.3V)

### Power Supply Recommendations
- **24V Input**: Switching power supply, 2-3A minimum capacity
- **Ripple**: <100mV peak-to-peak on 24V rail  
- **Regulation**: ±5% on input voltage acceptable
- **Protection**: Input fusing recommended (3A fast-blow)
- **Connector**: Secure screw terminals or high-current connector
- **Wire gauge**: 18 AWG minimum for 24V input, 22 AWG for low-current signals

## PCB Layout Considerations

### Signal Integrity
- **Crystal oscillator**: 47pF load capacitors (C1) with short, symmetric traces
- **High-speed digital signals**: Ground plane underneath for impedance control
- **Differential pairs**: RS485 signals (A/B) routed as matched-length differential pairs
- **I2C lines**: 10kΩ pull-up resistors (RN1, RN2, RN3) located close to connectors
- **PWM signals**: Proper ground return paths for motor driver switching

### Power Distribution
- **24V traces**: Wide traces (minimum 1.0mm) for motor current capacity
- **3.3V distribution**: Star configuration from regulator output
- **Ground plane**: Continuous ground plane with minimal splits
- **Via stitching**: Multiple vias connecting ground layers
- **Decoupling strategy**: 100nF capacitors within 5mm of each IC power pin

### Component Placement Strategy
- **Voltage regulator (U1)**: Central location with thermal relief
- **Motor drivers (U8-U11)**: Positioned near output connectors
- **MCU (U2)**: Central location with decoupling capacitors nearby
- **Connectors (CN1-CN5)**: Edge placement for external access
- **Crystal**: Adjacent to MCU with minimal trace length

### Thermal Management
- **Motor drivers**: Thermal pads connected to ground plane for heat dissipation
- **Voltage regulator**: Thermal via array underneath for heat transfer
- **Current sense resistors**: 1210 package for power dissipation
- **Component spacing**: Adequate spacing around heat-generating components

### EMI/EMC Design
- **Switching circuits**: Motor drivers isolated from sensitive analog circuits
- **Power supply filtering**: Input filter capacitors and inductors
- **Shield connections**: Ground plane connections for cable shields
- **Clock signals**: Minimal trace length with proper termination

## Mechanical Mounting and Assembly

### PCB Mounting
- **Mounting holes**: M2 screws (TP1, TP2, TP3) for mechanical support
- **Board thickness**: Standard 1.6mm PCB thickness
- **Copper weight**: 1oz minimum, 2oz recommended for power traces
- **Solder mask**: Green solder mask with white silkscreen
- **Surface finish**: HASL or ENIG for component soldering

### Connector Specifications

**Main Power/Communication Connector (CN1)**:
- Type: 4-pin through-hole, 3.0mm pitch
- Rating: 24V, 3A minimum
- Pins: 24V, RS485_A, GND, RS485_B
- Mating connector: Compatible screw terminal or Molex equivalent

**Channel Connectors (CN2-CN5)**:
- Type: 10-pin SMD, 2.0mm pitch
- Signals: 3.3V, GND, I2C (SCL/SDA), ADC inputs, motor outputs, RGB control
- Pin assignment per channel:
  ```
  Pin 1: K_PULL (ADC input)
  Pin 2: K_ONLINE (ADC input)  
  Pin 3: GND
  Pin 4: RGB_OUT (PWM signal)
  Pin 5: MCU_SCL (I2C clock)
  Pin 6: MCU_SDA (I2C data)
  Pin 7: GND
  Pin 8: 3.3V
  Pin 9: MOTOR_HOUT (High-side drive)
  Pin 10: MOTOR_LOUT (Low-side drive)
  ```

**Programming Headers**:
- H1: Debug UART (4-pin, 2.54mm pitch)
- H2: SWD Programming (4-pin, 2.54mm pitch)
- Standard 0.1" header compatible with common programming adapters

### AS5600 Sensor Mounting
- **Precise alignment required**: Sensors must be aligned with rotating magnets
- **Air gap tolerance**: 0.5-3.0mm (optimal ~1.5mm)
- **Mounting bracket design**: Rigid mounting to prevent vibration
- **Cable management**: Flexible ribbon or individual wires to main board
- **Environmental protection**: Consider dust/debris protection

### Magnet Assembly Guidelines

**Critical for Motor Direction Detection:**

1. **Magnet Selection**:
   - Diametrically magnetized (not axially magnetized)
   - Neodymium N35 or stronger recommended
   - Diameter: 6-8mm typical for gear assembly
   - Thickness: 2-4mm for optimal field strength

2. **Installation Procedure**:
   - **Polarity marking**: Mark all magnets during manufacturing
   - **Consistent orientation**: Install all magnets with same pole facing sensor
   - **Verification**: Use compass or magnetic field detector before assembly
   - **Documentation**: Record magnet orientation in assembly records

3. **Quality Control**:
   - Include magnet polarity check in assembly verification
   - Test rotation direction on each channel before final assembly
   - **Note**: Inconsistent polarity is primary cause of direction reversal issues

### Cable and Wiring

**Internal Wiring**:
- **I2C cables**: Twisted pair or ribbon cable with ground return
- **Motor cables**: 18-20 AWG for motor power, shielded if high frequency
- **Power cables**: 16-18 AWG for 24V distribution
- **Signal cables**: 22-24 AWG for low-current signals

**External Connections**:
- **BambuBus cable**: Shielded twisted pair for RS485 communication
- **Power input**: 16 AWG minimum with appropriate connector rating
- **Strain relief**: All external cables should have strain relief

### Assembly Sequence

1. **PCB Assembly**:
   - Surface mount components first (reflow soldering)
   - Through-hole components (wave soldering or hand assembly)
   - Programming header installation
   - Initial electrical test

2. **Mechanical Assembly**:
   - Install in enclosure with proper mounting
   - Connect external cables with strain relief
   - Install sensor boards with alignment fixtures

3. **System Integration**:
   - Connect to motor assemblies with marked magnet polarity
   - Program firmware with hardware-specific configuration
   - Perform functional testing on all channels

4. **Final Testing**:
   - Power supply test (all voltage rails)
   - Communication test (BambuBus protocol)
   - Motor direction test (automatic learning or manual verification)
   - Sensor calibration and range testing

## Configuration Options

### Hardware Variants

The firmware supports multiple hardware configurations through `config.h`:

#### LED Count Configuration
```c
#define LED_PA11_NUM    2    // Channel 0 LED count
#define LED_PA8_NUM     2    // Channel 1 LED count  
#define LED_PB1_NUM     2    // Channel 2 LED count
#define LED_PB0_NUM     2    // Channel 3 LED count
#define LED_PD1_NUM     1    // Main board LED count
```

#### Sensor Thresholds
```c
#define PULL_VOLTAGE_HIGH    1.85f   // High pressure threshold
#define PULL_VOLTAGE_LOW     1.45f   // Low pressure threshold
```

#### Motion Control Parameters
```c
#define ASSIST_SEND_TIME_MS     1200    // Feed assist duration
#define P1X_OUT_FILAMENT_MM     200.0f  // Retraction distance
```

## Testing and Validation

### Initial Hardware Test
1. **Power-on Test**: Verify all voltage rails
2. **LED Test**: Check each RGB LED individually
3. **Sensor Test**: Verify AS5600 sensor communication
4. **ADC Test**: Check pressure sensor readings
5. **Communication Test**: Verify UART communication

### Calibration Procedures

#### Pressure Sensor Calibration
1. With no filament inserted, record "low" voltage
2. With filament fully inserted, record "high" voltage
3. Set thresholds with appropriate hysteresis
4. Test insertion/removal detection

#### Hall Sensor Calibration
1. Verify sensor communication on each I2C channel
2. Check magnet alignment and air gap
3. **Verify magnet polarity consistency across all channels**
4. Validate position readings through full rotation
5. Test movement tracking accuracy
6. **Confirm direction detection consistency between channels**

### New: Automatic Motor Direction Detection

**Version 2.1+ introduces automatic direction detection that eliminates the need for manual direction calibration and hardware disassembly.**

#### How It Works
The firmware now automatically learns correct motor direction during normal filament feeding operations by:

1. **Triggering during filament loading**: When filament feed begins, the system starts learning mode
2. **Correlating commands with movement**: Compares motor commands with actual AS5600 sensor readings
3. **Collecting multiple samples**: Gathers at least 3 samples of 2mm+ movement for accuracy
4. **Determining correct direction**: If commanded direction matches sensor movement, direction is correct; if opposite, it's inverted
5. **Automatic saving**: Learned direction is permanently saved to flash memory

#### Benefits Over Manual Calibration
- ✅ **No hardware disassembly required** - Works during normal operation
- ✅ **Eliminates magnet polarity guesswork** - Automatically adapts to any magnet orientation
- ✅ **Real-world accuracy** - Uses actual filament loading conditions
- ✅ **User-friendly** - Completely automatic with no user intervention
- ✅ **Addresses root cause** - Compensates for inconsistent magnet polarity during assembly

#### Configuration
Enable in `config.h`:
```c
#define AUTO_DIRECTION_LEARNING_ENABLED    true     // Enable automatic learning (recommended)
#define AUTO_DIRECTION_MIN_SAMPLES         3        // Samples needed for confidence
#define AUTO_DIRECTION_MIN_MOVEMENT_MM     2.0f     // Minimum movement per sample
```

#### Assembly Impact
With automatic direction detection enabled:
- **Magnet polarity consistency is no longer critical** for motor direction
- Quality control procedures can focus on mechanical alignment rather than polarity
- Field issues with direction reversal are automatically resolved during first use
- Legacy units with inconsistent magnet polarity are automatically corrected

**Note**: While automatic detection solves direction issues, consistent magnet polarity is still recommended for optimal sensor performance and manufacturing quality.

For detailed information, see [AUTOMATIC_DIRECTION_DETECTION.md](AUTOMATIC_DIRECTION_DETECTION.md).

### Troubleshooting Common Issues

#### LEDs Not Working
- Check 5V power supply voltage and current capacity
- Verify data line connections and signal integrity
- Test individual LED segments
- Check for correct LED type (WS2812B compatible)

#### Sensor Communication Errors
- Verify I2C pullup resistors (4.7kΩ typical)
- Check for address conflicts
- Test with oscilloscope or logic analyzer
- Ensure proper power supply to sensors

#### Motor Control Issues
- Check PWM signal generation
- Verify motor driver power supply
- Test motor connections and phases
- Check for overheating or overcurrent

### Performance Optimization

#### Real-time Performance
- Monitor main loop execution time
- Optimize sensor reading frequency
- Use DMA for ADC to reduce CPU load
- Consider interrupt-driven communication

#### Power Efficiency
- Reduce RGB LED brightness if thermal issues occur
- Implement sleep modes when idle (future enhancement)
- Optimize PWM frequencies for motor efficiency

## Safety Considerations

### Electrical Safety
- Use appropriate fuses for motor power supplies
- Implement overcurrent protection
- Ensure proper grounding of all components
- Use isolated power supplies where required

### Thermal Protection
- Monitor component temperatures
- Implement thermal shutdown for motor drivers
- Provide adequate cooling for high-power components

### Mechanical Safety
- Ensure proper guarding of moving parts
- Use appropriate torque limits for motors
- Implement position limits to prevent damage

## Future Hardware Enhancements

### Possible Improvements
- Add temperature sensors for thermal monitoring
- Implement current sensing for motor load monitoring
- Add Ethernet connectivity for remote monitoring
- Include buzzer for audio feedback
- Add SD card slot for configuration storage

### Expansion Capabilities
- Additional ADC channels available for more sensors
- Unused GPIO pins available for future features
- SPI interface available for high-speed peripherals
- Additional UART available for secondary communication