#pragma once

// BMCU370 Configuration Constants
// These values are extracted from the existing codebase and documentation

// Version Information
#define AMS_FIRMWARE_VERSION_MAJOR      0
#define AMS_FIRMWARE_VERSION_MINOR      0
#define AMS_FIRMWARE_VERSION_PATCH      6
#define AMS_FIRMWARE_VERSION_BUILD      49

// BambuBus Version (from BambuBus.h)
#define BAMBU_BUS_VERSION               5

// Maximum number of filament channels (from BambuBus.h)
#define MAX_FILAMENT_CHANNELS           4

// LED Brightness Settings (from main.cpp analysis)
#define BRIGHTNESS_MAIN_BOARD           35
#define BRIGHTNESS_CHANNEL              15

// Voltage Thresholds (from Motion_control.cpp)
#define PULL_VOLTAGE_HIGH               1.85f
#define PULL_VOLTAGE_LOW                1.45f
#define PULL_VOLTAGE_SEND_MAX           1.7f    // From Motion_control.cpp define

// Motion Control Parameters (from Motion_control.cpp)
#define ASSIST_SEND_TIME_MS             1200    // From Assist_send_time variable
#define SPEED_FILTER_K                  100     // From speed_filter_k constant

// RGB Update Timing (estimated from main.cpp - 3 second intervals observed)
#define RGB_UPDATE_INTERVAL_MS          3000

// Filament movement distances (from Motion_control.cpp)
#define P1X_OUT_FILAMENT_MM             200.0f  // From P1X_OUT_filament_meters
#define P1X_OUT_FILAMENT_EXT_MM         700.0f  // External distance (estimated)

// USB Communication Buffer Sizes
#define USB_STATUS_BUFFER_SIZE          2048
#define USB_COMMAND_BUFFER_SIZE         128
#define USB_RESPONSE_BUFFER_SIZE        512
#define USB_RX_BUFFER_SIZE              256
#define USB_TX_BUFFER_SIZE              2048
#define USB_COMMAND_QUEUE_SIZE          8

// USB PIN CONFLICT RESOLUTION
// IMPORTANT: PA11 is used for both USB_DM and Channel 0 RGB LEDs
// When USB_CDC_ENABLED=1, Channel 0 RGB LEDs are automatically disabled
// to prevent hardware conflicts. This reduces LED channels from 4 to 3.
//
// Configuration options:
// USB_CDC_ENABLED=1, USB_LED_CONFLICT_RESOLUTION=1: USB enabled, Ch0 LEDs disabled  
// USB_CDC_ENABLED=0: USB disabled, all 4 LED channels enabled
//
#ifndef USB_LED_CONFLICT_RESOLUTION
#ifdef USB_CDC_ENABLED
#define USB_LED_CONFLICT_RESOLUTION     1  // Enable conflict resolution when USB is enabled
#else  
#define USB_LED_CONFLICT_RESOLUTION     0  // No conflict resolution needed when USB disabled
#endif
#endif

// Pin assignments for reference:
// PA11: USB_DM (when USB enabled) OR Channel 0 RGB (when USB disabled)
// PA12: USB_DP (when USB enabled) 
// PA8:  Channel 1 RGB (always available)
// PB1:  Channel 2 RGB (always available)  
// PB0:  Channel 3 RGB (always available)
// PD1:  Main board RGB (always available)