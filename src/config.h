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
// Communication and Error Handling Configuration
// =============================================================================

#define DEBUG_UART_BAUDRATE     115200      ///< Debug UART baud rate
#define BAMBU_BUS_VERSION       5           ///< BambuBus protocol version

// Communication robustness improvements
#define COMMUNICATION_RETRY_COUNT           3           ///< Number of retries for failed communications
#define COMMUNICATION_TIMEOUT_MS            1000        ///< Communication timeout in milliseconds
#define COMMUNICATION_RETRY_BACKOFF_MS      100         ///< Initial retry backoff delay
#define COMMUNICATION_MAX_BACKOFF_MS        2000        ///< Maximum retry backoff delay
#define COMMUNICATION_ERROR_RECOVERY_ENABLED true       ///< Enable automatic error recovery
#define HEARTBEAT_TIMEOUT_MS                5000        ///< Heartbeat timeout for connection monitoring
#define HEARTBEAT_RETRY_INTERVAL_MS         1000        ///< Heartbeat retry interval

// Watchdog and system monitoring
#define WATCHDOG_ENABLED                    true        ///< Enable hardware watchdog
#define WATCHDOG_TIMEOUT_MS                 2000        ///< Watchdog timeout (2 seconds)
#define SYSTEM_HEALTH_CHECK_INTERVAL_MS     500         ///< System health check interval
#define VOLTAGE_MONITORING_ENABLED          true        ///< Enable supply voltage monitoring
#define VOLTAGE_MIN_THRESHOLD               3.0f        ///< Minimum operating voltage (V)
#define VOLTAGE_MAX_THRESHOLD               3.6f        ///< Maximum operating voltage (V)

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

// Voltage thresholds for filament detection (in Volts)
#define PULL_VOLTAGE_HIGH       1.85f       ///< High pressure threshold (red LED)
#define PULL_VOLTAGE_LOW        1.45f       ///< Low pressure threshold (blue LED)
#define PULL_VOLTAGE_SEND_MAX   1.7f        ///< Maximum voltage for sending filament

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

// Sensor fault detection and robustness
#define SENSOR_FAULT_THRESHOLD          5           ///< Consecutive failed readings before marking sensor as faulty
#define SENSOR_RECOVERY_THRESHOLD       3           ///< Consecutive good readings before marking sensor as recovered
#define SENSOR_NOISE_FILTER_SAMPLES     8           ///< Number of samples for noise filtering
#define SENSOR_MAX_NOISE_RATIO          0.1f        ///< Maximum acceptable noise ratio (10%)
#define SENSOR_CALIBRATION_INTERVAL_MS  30000       ///< Auto-calibration interval (30 seconds)
#define SENSOR_TEMPERATURE_COMP_ENABLED true        ///< Enable temperature compensation
#define SENSOR_DRIFT_DETECTION_ENABLED  true        ///< Enable sensor drift detection

// ADC filtering and accuracy improvements
#define ADC_STABILITY_THRESHOLD         0.01f       ///< ADC reading stability threshold (10mV)
#define ADC_OUTLIER_DETECTION_ENABLED   true        ///< Enable outlier detection and rejection
#define ADC_OUTLIER_THRESHOLD           2.0f        ///< Standard deviations for outlier detection
#define ADC_ADAPTIVE_FILTER_ENABLED     true        ///< Enable adaptive filtering based on signal quality

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
// Advanced Filament Type Configurations
// =============================================================================

// PLA Filament Configuration
#define PLA_TEMP_MIN            190         ///< PLA minimum temperature (°C)
#define PLA_TEMP_MAX            220         ///< PLA maximum temperature (°C)
#define PLA_FEED_SPEED_FAST     80          ///< PLA fast feeding speed (mm/s)
#define PLA_FEED_SPEED_SLOW     20          ///< PLA slow feeding speed (mm/s)
#define PLA_RETRACT_SPEED       30          ///< PLA retraction speed (mm/s)
#define PLA_PRESSURE_THRESHOLD  1.6f        ///< PLA specific pressure threshold

// PETG Filament Configuration  
#define PETG_TEMP_MIN           220         ///< PETG minimum temperature (°C)
#define PETG_TEMP_MAX           250         ///< PETG maximum temperature (°C)
#define PETG_FEED_SPEED_FAST    60          ///< PETG fast feeding speed (mm/s)
#define PETG_FEED_SPEED_SLOW    15          ///< PETG slow feeding speed (mm/s)
#define PETG_RETRACT_SPEED      25          ///< PETG retraction speed (mm/s)
#define PETG_PRESSURE_THRESHOLD 1.75f       ///< PETG specific pressure threshold

// ABS Filament Configuration
#define ABS_TEMP_MIN            230         ///< ABS minimum temperature (°C)
#define ABS_TEMP_MAX            260         ///< ABS maximum temperature (°C)
#define ABS_FEED_SPEED_FAST     70          ///< ABS fast feeding speed (mm/s)
#define ABS_FEED_SPEED_SLOW     18          ///< ABS slow feeding speed (mm/s)
#define ABS_RETRACT_SPEED       28          ///< ABS retraction speed (mm/s)
#define ABS_PRESSURE_THRESHOLD  1.8f        ///< ABS specific pressure threshold

// TPU Filament Configuration (Flexible)
#define TPU_TEMP_MIN            210         ///< TPU minimum temperature (°C)
#define TPU_TEMP_MAX            240         ///< TPU maximum temperature (°C)
#define TPU_FEED_SPEED_FAST     25          ///< TPU fast feeding speed (mm/s) - much slower
#define TPU_FEED_SPEED_SLOW     8           ///< TPU slow feeding speed (mm/s)
#define TPU_RETRACT_SPEED       10          ///< TPU retraction speed (mm/s) - very slow
#define TPU_PRESSURE_THRESHOLD  1.5f        ///< TPU specific pressure threshold - lower

// WOOD Filament Configuration
#define WOOD_TEMP_MIN           180         ///< WOOD minimum temperature (°C)
#define WOOD_TEMP_MAX           210         ///< WOOD maximum temperature (°C)
#define WOOD_FEED_SPEED_FAST    50          ///< WOOD fast feeding speed (mm/s)
#define WOOD_FEED_SPEED_SLOW    12          ///< WOOD slow feeding speed (mm/s)
#define WOOD_RETRACT_SPEED      20          ///< WOOD retraction speed (mm/s)
#define WOOD_PRESSURE_THRESHOLD 1.55f       ///< WOOD specific pressure threshold

// Adaptive filament handling
#define ADAPTIVE_SPEED_ENABLED  true        ///< Enable adaptive speed based on filament type
#define ADAPTIVE_PRESSURE_ENABLED true      ///< Enable adaptive pressure thresholds
#define FILAMENT_TYPE_DETECTION_ENABLED true ///< Enable automatic filament type detection via NFC

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
// Timing and Responsiveness Configuration
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

// Interrupt and timing priorities
#define MAIN_LOOP_PRIORITY              0           ///< Main loop task priority
#define SENSOR_UPDATE_PRIORITY          1           ///< Sensor update priority (highest)
#define COMMUNICATION_PRIORITY          2           ///< Communication handling priority
#define RGB_UPDATE_PRIORITY             3           ///< RGB update priority (lowest)

// Timing intervals for responsiveness
#define SENSOR_UPDATE_INTERVAL_US       500         ///< Sensor update interval (500μs = 2kHz)
#define FAST_LOOP_INTERVAL_US           100         ///< Fast loop interval for critical operations
#define COMMUNICATION_POLL_INTERVAL_US  1000        ///< Communication polling interval (1ms)
#define RGB_UPDATE_FAST_INTERVAL_MS     50          ///< Fast RGB update for animations (20Hz)

// Motion control responsiveness
#define MOTOR_CONTROL_UPDATE_RATE_HZ    1000        ///< Motor control update rate (1kHz)
#define POSITION_FEEDBACK_RATE_HZ       500         ///< Position feedback sampling rate (500Hz)
#define SPEED_CALCULATION_FILTER_TC     10          ///< Speed calculation time constant (samples)

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