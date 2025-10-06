# Adaptive Pressure Control System

## Overview

The Adaptive Pressure Control System addresses the fundamental issue of static pressure thresholds in filament feeding. Instead of using fixed voltage thresholds (1.45V-1.85V), the system learns individual sensor characteristics for each channel and dynamically adjusts control parameters based on sensor variations.

**Version 2.0** adds advanced signal filtering, dynamic update rate adaptation, and comprehensive diagnostics for enhanced stability and monitoring.

## Problem Solved

### Original Issue
- Hot end loses grip on filament when pressure drifts outside narrow static range
- Static thresholds don't account for individual sensor variations
- Each module has different:
  - Zero point (no pressure reading)
  - Sensor range for positive/negative force
  - Max/min pressure readings due to sensor placement differences

### Solution Benefits
- **Individualized Control**: Each channel adapts to its specific sensor characteristics
- **Advanced Filtering**: Noise reduction and outlier rejection for stable readings
- **Smart Update Rates**: Adaptive timing based on system activity (20-100 Hz)
- **Health Monitoring**: Sensor diagnostics and fault detection
- **Improved Responsiveness**: Smaller deadbands and enhanced PID control prevent pressure drift
- **Automatic Learning**: No manual calibration required - learns during normal operation
- **Backward Compatibility**: Falls back to original static thresholds if calibration unavailable

## Key Features

### 1. Per-Channel Sensor Calibration
- Learns zero point (neutral pressure) for each sensor
- Determines pressure range characteristics
- Calculates dynamic thresholds based on sensor properties
- Stores calibration data in flash memory for persistence

### 2. Advanced Signal Filtering (New in v2.0)
- **Moving Average Filter**: 8-sample window for noise reduction
- **Outlier Rejection**: Automatically rejects spurious readings
- **Low-Pass Filtering**: Configurable alpha coefficient for stability
- **Stability Detection**: Monitors reading consistency

### 3. Dynamic Update Rate Control (New in v2.0)
- **Adaptive Timing**: Adjusts update rate based on system activity
  - Idle: 20 Hz (50ms intervals) for power efficiency
  - Active: 50 Hz (20ms intervals) during feeding operations
  - Critical: 100 Hz (10ms intervals) for emergency response
- **Activity Detection**: Monitors feeding state and pressure deviations
- **Smooth Transitions**: Gradual rate changes prevent system shock

### 4. Enhanced Diagnostics (New in v2.0)
- **Sensor Health Monitoring**: Continuous assessment of sensor condition
- **Drift Detection**: Monitors long-term sensor calibration drift
- **Stuck Sensor Detection**: Identifies sensors with identical readings
- **Performance Metrics**: Tracks average/maximum errors and correction counts
- **Fault Recovery**: Automatic detection and logging of sensor issues

### 5. Dynamic Threshold Calculation
- High/low pressure thresholds calculated as percentage of sensor range
- Deadband around zero point scaled to sensor characteristics
- Individual sensor variations automatically compensated

### 6. Enhanced Pressure Control
- More responsive PID control with configurable scaling
- Active pressure correction when drift is detected
- Smaller deadbands for faster response to pressure changes
- Maximum correction limits to prevent excessive motor force

### 7. Automatic Calibration
- Calibrates sensors during idle periods when no filament present
- Periodic recalibration to maintain accuracy
- Manual calibration functions for testing and debugging

## Configuration Parameters

All parameters are defined in `src/config.h`:

### Main Control
```cpp
#define ADAPTIVE_PRESSURE_ENABLED       true    // Enable adaptive pressure control
#define PRESSURE_ADAPTIVE_DISABLE_MASK  0x00    // Disable per channel (bitmask)
```

### Calibration Settings
```cpp
#define PRESSURE_CALIBRATION_SAMPLES    50      // Samples for calibration (30-100)
#define PRESSURE_CALIBRATION_TIME_MS    5000    // Max calibration time (ms)
#define PRESSURE_AUTO_RECALIBRATION     true    // Enable auto recalibration
```

### Threshold Scaling
```cpp
#define PRESSURE_DEADBAND_SCALE         0.15f   // Deadband as fraction of range (0.1-0.3)
#define PRESSURE_HIGH_THRESHOLD_SCALE   0.25f   // High threshold fraction (0.2-0.4)
#define PRESSURE_LOW_THRESHOLD_SCALE    0.25f   // Low threshold fraction (0.2-0.4)
```

### Enhanced Control
```cpp
#define PRESSURE_CONTROL_RESPONSIVE     true    // Enable responsive control
#define PRESSURE_CONTROL_PID_P_SCALE    2.0f    // PID scaling factor
#define PRESSURE_ACTIVE_CORRECTION      true    // Enable active correction
#define PRESSURE_DRIFT_THRESHOLD        0.03f   // Drift threshold (volts)
```

### Timing Control
```cpp
#define PRESSURE_UPDATE_INTERVAL_MS     25      // Pressure update interval (ms) - 40 Hz
#define PRESSURE_TIMING_CONTROL_ENABLED true    // Enable timing control for stability
```

### Advanced Signal Filtering (New in v2.0)
```cpp
#define PRESSURE_FILTERING_ENABLED      true    // Enable signal filtering
#define PRESSURE_FILTER_WINDOW_SIZE     8       // Moving average window (4-16)
#define PRESSURE_OUTLIER_REJECTION      true    // Enable outlier rejection
#define PRESSURE_OUTLIER_THRESHOLD      0.15f   // Outlier threshold (0.1-0.2)
#define PRESSURE_LOWPASS_FILTER_ALPHA   0.3f    // Low-pass coefficient (0.1-0.5)
```

### Dynamic Update Rate Control (New in v2.0)
```cpp
#define PRESSURE_ADAPTIVE_UPDATE_RATE   true    // Enable adaptive update rates
#define PRESSURE_UPDATE_RATE_IDLE_MS    50      // Idle rate: 20 Hz
#define PRESSURE_UPDATE_RATE_ACTIVE_MS  20      // Active rate: 50 Hz
#define PRESSURE_UPDATE_RATE_CRITICAL_MS 10     // Critical rate: 100 Hz
```

### Enhanced Diagnostics (New in v2.0)
```cpp
#define PRESSURE_DIAGNOSTICS_ENABLED    true    // Enable diagnostics
#define PRESSURE_DRIFT_MONITORING       true    // Monitor sensor drift
#define PRESSURE_DRIFT_WARNING_THRESHOLD 0.1f   // Drift warning threshold (volts)
#define PRESSURE_PERFORMANCE_METRICS    true    // Collect performance metrics
#define PRESSURE_FAULT_DETECTION        true    // Enable fault detection
#define PRESSURE_STUCK_READING_THRESHOLD 100    // Stuck sensor detection count
```

## Operation Modes

### 1. Automatic Mode (Default)
- System automatically calibrates sensors during idle periods
- Uses learned characteristics for pressure control
- Falls back to static thresholds if calibration unavailable

### 2. Manual Calibration
```cpp
// Force calibration of all channels (when no filament present)
pressure_sensor_force_calibrate_all();

// Calibrate specific channel
pressure_sensor_calibrate_channel(channel_number);

// Reset calibration for all channels
pressure_sensor_reset_all_calibration();
```

### 3. Disabled/Fallback Mode
- Set `ADAPTIVE_PRESSURE_ENABLED` to `false` to disable system
- Use `PRESSURE_ADAPTIVE_DISABLE_MASK` to disable specific channels
- System automatically falls back to original static thresholds

## Debug Information

When enabled, the system provides debug output showing:
- Calibration progress and results
- Learned sensor characteristics (zero point, range)
- Dynamic threshold calculations
- Active pressure correction events

Example debug output:
```
Starting pressure sensor calibration for channel 0
Pressure calibration complete for CH0: zero=1.623V, range=0.750V, deadband=1.511-1.735V
Loaded pressure calibration for CH1: zero=1.640V
```

## Tuning Guidelines

### For More Responsive Control
- Reduce `PRESSURE_DEADBAND_SCALE` (0.10-0.15)
- Increase `PRESSURE_CONTROL_PID_P_SCALE` (2.0-3.0)
- Reduce `PRESSURE_DRIFT_THRESHOLD` (0.02-0.03)

### For More Conservative Control
- Increase `PRESSURE_DEADBAND_SCALE` (0.20-0.30)
- Reduce `PRESSURE_CONTROL_PID_P_SCALE` (1.0-1.5)
- Increase `PRESSURE_DRIFT_THRESHOLD` (0.04-0.06)

### For Problematic Sensors
- Disable adaptive control for specific channels using bitmask
- Increase calibration samples (75-100)
- Extend calibration time (7000-10000 ms)

## Memory Usage

The adaptive pressure control system adds approximately:
- **320 bytes RAM**: Calibration data and state variables
- **700 bytes Flash**: Additional code for calibration and control logic

Total firmware memory usage remains well within safe limits:
- RAM: 55.2% (11,304/20,480 bytes)
- Flash: 62.1% (40,700/65,536 bytes)

## Troubleshooting

### System Not Learning
- Verify no filament is present during calibration attempts
- Check that channels are in idle state
- Ensure sufficient time between calibration attempts (30+ seconds)

### Inconsistent Behavior
- Reset calibration data and allow system to relearn
- Check sensor voltage readings are within expected range (0.2V-2.8V)
- Verify mechanical sensor installation and alignment

### Reverting to Original Behavior
- Set `ADAPTIVE_PRESSURE_ENABLED` to `false` in config.h
- Rebuild and flash firmware
- System will use original static thresholds (1.45V-1.85V)

## Timing Optimization

### Update Rate Control
The system implements configurable timing control for pressure measurement and correction:

- **Default Rate**: 40 Hz (25ms intervals)
- **Purpose**: Balance responsiveness with system stability
- **Benefits**: 
  - Reduces computational overhead
  - Prevents over-correction instability
  - Allows time for physical system response

### Configuration
```cpp
#define PRESSURE_UPDATE_INTERVAL_MS     25      // Update interval (20-50ms recommended)
#define PRESSURE_TIMING_CONTROL_ENABLED true    // Enable/disable timing control
```

### Recommended Settings
- **High-performance systems**: 20ms (50 Hz)
- **Standard operation**: 25ms (40 Hz) - default
- **Conservative/stable**: 50ms (20 Hz)

The timing control ensures pressure readings and corrections occur at optimal intervals rather than as fast as the main communication loop allows.

## Implementation Details

### Data Structures
- `PressureSensorCalibration`: Per-channel sensor characteristics
- `PressureCorrectionState`: Active correction state tracking
- Flash storage integration for persistent calibration data

### Key Functions
- `pressure_sensor_calibrate_channel()`: Learn sensor characteristics
- `get_dynamic_pressure_threshold_*()`: Calculate adaptive thresholds
- `pressure_sensor_auto_calibrate()`: Automatic calibration during idle
- Enhanced `_get_x_by_pressure()`: Improved pressure control with adaptive targets

The system is designed to be completely transparent to existing firmware operation while providing significantly improved pressure control accuracy and responsiveness.