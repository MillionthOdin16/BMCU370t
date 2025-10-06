#pragma once

/**
 * BMCU370 Configuration Header
 * 
 * This file contains all hardware-specific constants and configuration
 * parameters for the BMCU370 firmware. Modify these values to match
 * your specific hardware setup.
 */

// =============================================================================
// Hardware Configuration
// =============================================================================

// RGB LED Configuration
#define LED_PA11_NUM    2    ///< Number of RGB LEDs on channel PA11
#define LED_PA8_NUM     2    ///< Number of RGB LEDs on channel PA8
#define LED_PB1_NUM     2    ///< Number of RGB LEDs on channel PB1
#define LED_PB0_NUM     2    ///< Number of RGB LEDs on channel PB0
#define LED_PD1_NUM     1    ///< Number of RGB LEDs on main board (PD1)

// RGB LED Brightness (0-255)
#define BRIGHTNESS_MAIN_BOARD   35   ///< Main board LED brightness
#define BRIGHTNESS_CHANNEL      15   ///< Channel LED brightness

// System Clock Configuration
#define SYSTEM_CLOCK_HZ         144000000UL  ///< System clock frequency in Hz

// =============================================================================
// Communication Configuration
// =============================================================================

#define DEBUG_UART_BAUDRATE     115200      ///< Debug UART baud rate
#define BAMBU_BUS_VERSION       5           ///< BambuBus protocol version

// =============================================================================
// Firmware Version Configuration
// =============================================================================
// 
// IMPORTANT: The AMS firmware version reported to the printer is critical for
// compatibility. Bambu Lab printers may reject AMS units with firmware versions
// that are too old or incompatible. These versions are sent in response to
// version query packets from the printer.
//
// Version format: Little-endian 4-byte array representing Major.Minor.Patch.Build
// Example: {0x31, 0x06, 0x00, 0x00} = version 00.00.06.49

// AMS (8-channel) Firmware Version - version 00.00.06.49
#define AMS_FIRMWARE_VERSION_MAJOR      0x00    ///< Major version (displayed as XX in XX.XX.XX.XX)
#define AMS_FIRMWARE_VERSION_MINOR      0x00    ///< Minor version (displayed as XX in XX.XX.XX.XX)
#define AMS_FIRMWARE_VERSION_PATCH      0x06    ///< Patch version (displayed as 06 in XX.XX.06.XX)
#define AMS_FIRMWARE_VERSION_BUILD      0x31    ///< Build version (displayed as 49 in XX.XX.XX.49)

// AMS Lite Firmware Version - version 00.01.02.03
#define AMS_LITE_FIRMWARE_VERSION_MAJOR 0x00    ///< Major version (displayed as 00 in 00.01.02.03)
#define AMS_LITE_FIRMWARE_VERSION_MINOR 0x01    ///< Minor version (displayed as 01 in 00.01.02.03)
#define AMS_LITE_FIRMWARE_VERSION_PATCH 0x02    ///< Patch version (displayed as 02 in 00.01.02.03)
#define AMS_LITE_FIRMWARE_VERSION_BUILD 0x03    ///< Build version (displayed as 03 in 00.01.02.03)

// =============================================================================
// Motion Control Configuration
// =============================================================================

// Legacy voltage thresholds for filament detection (in Volts) - used as fallback
#define PULL_VOLTAGE_HIGH       1.85f       ///< High pressure threshold (red LED) - fallback
#define PULL_VOLTAGE_LOW        1.45f       ///< Low pressure threshold (blue LED) - fallback
#define PULL_VOLTAGE_SEND_MAX   1.7f        ///< Maximum voltage for sending filament

// =============================================================================
// Adaptive Pressure Control Configuration
// =============================================================================

/**
 * Adaptive Pressure Control System
 * 
 * This system learns individual sensor characteristics for each channel and
 * dynamically adjusts pressure thresholds based on sensor variations.
 * This provides more responsive and accurate filament pressure control.
 */
#define ADAPTIVE_PRESSURE_ENABLED       true    ///< Enable adaptive pressure control system
#define PRESSURE_DEADBAND_SCALE         0.15f   ///< Deadband around zero as fraction of sensor range (0.1-0.3 recommended)
#define PRESSURE_HIGH_THRESHOLD_SCALE   0.25f   ///< High pressure threshold as fraction of sensor range (0.2-0.4 recommended)
#define PRESSURE_LOW_THRESHOLD_SCALE    0.25f   ///< Low pressure threshold as fraction of sensor range (0.2-0.4 recommended)

// Sensor calibration parameters
#define PRESSURE_CALIBRATION_SAMPLES    50      ///< Number of samples for initial calibration (30-100 recommended)
#define PRESSURE_CALIBRATION_TIME_MS    5000    ///< Maximum time for calibration in milliseconds
#define PRESSURE_RANGE_MIN_VOLTAGE      0.2f    ///< Minimum expected sensor range in volts
#define PRESSURE_RANGE_MAX_VOLTAGE      2.8f    ///< Maximum expected sensor range in volts
#define PRESSURE_ZERO_TOLERANCE         0.05f   ///< Tolerance for zero point detection in volts
#define PRESSURE_AUTO_RECALIBRATION     true    ///< Enable automatic recalibration during idle periods

// Enhanced pressure control parameters
#define PRESSURE_CONTROL_RESPONSIVE     true    ///< Enable more responsive pressure control
#define PRESSURE_CONTROL_DEADBAND_SMALL 0.02f   ///< Small deadband for responsive control in volts
#define PRESSURE_CONTROL_PID_P_SCALE    2.0f    ///< Scaling factor for PID proportional gain
#define PRESSURE_CONTROL_MAX_CORRECTION 800     ///< Maximum motor correction value for pressure control
#define PRESSURE_ACTIVE_CORRECTION      true    ///< Enable active pressure correction when drift detected
#define PRESSURE_DRIFT_THRESHOLD        0.03f   ///< Pressure drift threshold to trigger active correction (volts)
#define PRESSURE_CORRECTION_TIMEOUT_MS  2000    ///< Maximum time for active pressure correction (ms)
#define PRESSURE_ADAPTIVE_DISABLE_MASK  0x00    ///< Bitmask to disable adaptive control per channel (bit 0=CH0, bit 1=CH1, etc.)

// Pressure control timing
#define PRESSURE_UPDATE_INTERVAL_MS     25      ///< Pressure measurement and control update interval (ms) - 40 Hz
#define PRESSURE_TIMING_CONTROL_ENABLED true    ///< Enable pressure control timing limits

// Advanced signal filtering for pressure readings
#define PRESSURE_FILTERING_ENABLED      true    ///< Enable advanced signal filtering for pressure readings
#define PRESSURE_FILTER_WINDOW_SIZE     8       ///< Moving average filter window size (4-16 recommended)
#define PRESSURE_OUTLIER_REJECTION      true    ///< Enable outlier rejection for spurious readings
#define PRESSURE_OUTLIER_THRESHOLD      0.15f   ///< Outlier threshold as fraction of sensor range (0.1-0.2 recommended)
#define PRESSURE_LOWPASS_FILTER_ALPHA   0.3f    ///< Low-pass filter coefficient (0.1-0.5, lower = more filtering)

// Dynamic update rate adaptation
#define PRESSURE_ADAPTIVE_UPDATE_RATE   true    ///< Enable adaptive update rate based on system state
#define PRESSURE_UPDATE_RATE_IDLE_MS    50      ///< Slower update rate during idle periods (20 Hz)
#define PRESSURE_UPDATE_RATE_ACTIVE_MS  20      ///< Faster update rate during active operations (50 Hz)
#define PRESSURE_UPDATE_RATE_CRITICAL_MS 10     ///< Emergency fast update rate for critical deviations (100 Hz)

// Enhanced diagnostics and health monitoring
#define PRESSURE_DIAGNOSTICS_ENABLED    true    ///< Enable pressure sensor health monitoring and diagnostics
#define PRESSURE_DRIFT_MONITORING       true    ///< Monitor sensor drift over time
#define PRESSURE_DRIFT_WARNING_THRESHOLD 0.1f   ///< Threshold for sensor drift warning (volts)
#define PRESSURE_PERFORMANCE_METRICS    true    ///< Collect pressure control performance metrics
#define PRESSURE_FAULT_DETECTION        true    ///< Enable automatic fault detection and recovery
#define PRESSURE_STUCK_READING_THRESHOLD 100    ///< Number of identical readings to consider sensor stuck

// Timing constants (in milliseconds)
#define ASSIST_SEND_TIME_MS     1200        ///< Filament send assist duration
#define RGB_UPDATE_INTERVAL_MS  3000        ///< RGB update interval for error states

// Filament distances (in millimeters)
#define P1X_OUT_FILAMENT_MM     200.0f      ///< Internal filament retraction distance
#define P1X_OUT_FILAMENT_EXT_MM 700.0f      ///< External filament retraction distance

// Speed filtering constant
#define SPEED_FILTER_K          100         ///< Speed calculation filter coefficient

// =============================================================================
// Flash Memory Configuration
// =============================================================================

#define FLASH_SAVE_ADDRESS      0x0800F000UL ///< Flash memory address for persistent data
#define FLASH_MAGIC_NUMBER      0x40614061UL ///< Magic number for flash data validation

// =============================================================================
// Sensor Configuration
// =============================================================================

// AS5600 Hall sensor I2C pins
#define AS5600_SCL_PINS         {PB15, PB14, PB13, PB12}  ///< SCL pins for each channel
#define AS5600_SDA_PINS         {PD0, PC15, PC14, PC13}   ///< SDA pins for each channel

// Mathematical constants
#define AS5600_PI               3.1415926535897932384626433832795

// =============================================================================
// Default Filament Configuration
// =============================================================================

#define DEFAULT_FILAMENT_ID     "GFG00"     ///< Default filament ID
#define DEFAULT_FILAMENT_NAME   "PETG"      ///< Default filament material name
#define DEFAULT_TEMP_MIN        220         ///< Default minimum temperature (°C)
#define DEFAULT_TEMP_MAX        240         ///< Default maximum temperature (°C)

// Default RGBA color values (0-255)
#define DEFAULT_COLOR_R         0xFF        ///< Default red component
#define DEFAULT_COLOR_G         0xFF        ///< Default green component  
#define DEFAULT_COLOR_B         0xFF        ///< Default blue component
#define DEFAULT_COLOR_A         0xFF        ///< Default alpha component

#define DEFAULT_HUMIDITY_WET    12          ///< Default humidity threshold (%)

// =============================================================================
// Channel Configuration
// =============================================================================

#define MAX_FILAMENT_CHANNELS   4           ///< Maximum number of filament channels
#define MAX_FILAMENT_NUM        4           ///< Maximum filament number (same as channels)

// Channel state definitions
#define CHANNEL_OFFLINE         0           ///< Channel offline state
#define CHANNEL_ONLINE_BOTH     1           ///< Channel online with both micro-switches
#define CHANNEL_ONLINE_OUTER    2           ///< Channel online with outer switch only
#define CHANNEL_ONLINE_INNER    3           ///< Channel online with inner switch only

// =============================================================================
// Motor Direction Detection and Correction
// =============================================================================

/**
 * Automatic Direction Learning Configuration
 * 
 * The system can automatically learn motor direction during normal filament feeding
 * operations by correlating motor commands with actual movement detected by sensors.
 * This eliminates the need for hardware disassembly and provides more accurate
 * direction detection under real operating conditions.
 */
#define AUTO_DIRECTION_LEARNING_ENABLED    true     ///< Enable automatic direction learning during feeding
#define AUTO_DIRECTION_MIN_SAMPLES         3        ///< Minimum samples needed to confirm direction (3-10 recommended)
#define AUTO_DIRECTION_MIN_MOVEMENT_MM     2.0f     ///< Minimum movement required for valid sample in mm (1.0-5.0 recommended)
#define AUTO_DIRECTION_TIMEOUT_MS          5000     ///< Timeout for direction learning attempt in ms (3000-10000 recommended)
#define AUTO_DIRECTION_CONFIDENCE_THRESHOLD 0.7f    ///< Minimum confidence ratio required (0.6-0.9 recommended)
#define AUTO_DIRECTION_MAX_NOISE_MM        0.5f     ///< Maximum acceptable sensor noise per sample in mm
#define AUTO_DIRECTION_SAMPLE_INTERVAL_MS  100      ///< Minimum time between samples in ms
#define AUTO_DIRECTION_DEBUG_ENABLED       false    ///< Enable debug output for direction learning

/**
 * Legacy Motor Direction Correction (Fallback)
 * 
 * Static correction flags used as fallback when automatic learning is disabled
 * or fails. Set to true for channels that have reversed motor direction due to 
 * hardware differences.
 * 
 * Based on reported issues:
 * - Channels 1 and 2 are commonly affected
 * - Some units may have channel 3 affected instead
 * 
 * Set to true to invert the auto-detected direction for that channel.
 */
#define MOTOR_DIR_CORRECTION_CH0   false     ///< Channel 0 direction correction (fallback)
#define MOTOR_DIR_CORRECTION_CH1   true      ///< Channel 1 direction correction (fallback)
#define MOTOR_DIR_CORRECTION_CH2   true      ///< Channel 2 direction correction (fallback)  
#define MOTOR_DIR_CORRECTION_CH3   false     ///< Channel 3 direction correction (fallback)

// =============================================================================
// Timing Delays
// =============================================================================

/**
 * Microsecond delay calculation helper
 * @param time Delay time in microseconds
 */
#define DELAY_US_DIVISOR(time)  ((uint64_t)(8000000.0 / (time)))

/**
 * Millisecond delay calculation helper  
 * @param time Delay time in milliseconds
 */
#define DELAY_MS_DIVISOR(time)  ((uint64_t)(80000.0 / (time)))

// =============================================================================
// Error Codes
// =============================================================================

#define ERROR_NONE              0           ///< No error
#define ERROR_OFFLINE           -1          ///< Device offline error
#define ERROR_TIMEOUT           -2          ///< Communication timeout error
#define ERROR_CRC               -3          ///< CRC checksum error
#define ERROR_INVALID_DATA      -4          ///< Invalid data format error

// =============================================================================
// Device Type Identifiers
// =============================================================================

#define DEVICE_TYPE_NONE        0x0000      ///< No device detected
#define DEVICE_TYPE_AMS         0x0700      ///< AMS device type
#define DEVICE_TYPE_AMS_LITE    0x1200      ///< AMS Lite device type