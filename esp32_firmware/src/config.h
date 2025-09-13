#ifndef CONFIG_H
#define CONFIG_H

// ESP32-S3 N4R2 Hardware identification
#ifdef ESP32_S3_N4R2_VARIANT
#define HARDWARE_VARIANT "ESP32-S3 N4R2"
#define FLASH_SIZE_MB 4
#define PSRAM_SIZE_MB 2
#define PSRAM_TYPE "OPI"
#else
#define HARDWARE_VARIANT "ESP32-S3"
#define FLASH_SIZE_MB 4
#define PSRAM_SIZE_MB 2
#define PSRAM_TYPE "Unknown"
#endif

// Version information
#ifndef BMCU370_INTERFACE_VERSION
#define BMCU370_INTERFACE_VERSION "1.0.0"
#endif

// Timing configuration
#define STATUS_UPDATE_INTERVAL_MS   1000    // Update BMCU370 status every 1 second
#define WEBSOCKET_UPDATE_INTERVAL   500     // WebSocket updates every 500ms
#define USB_TIMEOUT_MS              1000    // USB communication timeout
#define USB_RETRY_DELAY_MS          2000    // Delay between USB connection retries

// Buffer sizes - optimized for ESP32-S3 N4R2 with 2MB OPI PSRAM
#ifdef BOARD_HAS_PSRAM
  #ifdef ESP32_S3_N4R2_VARIANT
    // Enhanced buffer sizes for N4R2 variant with OPI PSRAM
    #define USB_COMMAND_BUFFER_SIZE     1024    // Larger command buffer for N4R2
    #define USB_RESPONSE_BUFFER_SIZE    8192    // Enhanced response buffer with OPI PSRAM
    #define JSON_BUFFER_SIZE            16384   // Large JSON document buffer for N4R2
    #define WEBSOCKET_BUFFER_SIZE       4096    // Enhanced WebSocket buffer
  #else
    #define USB_COMMAND_BUFFER_SIZE     512     // Standard PSRAM command buffer
    #define USB_RESPONSE_BUFFER_SIZE    4096    // Standard PSRAM response buffer
    #define JSON_BUFFER_SIZE            8192    // Standard PSRAM JSON buffer
    #define WEBSOCKET_BUFFER_SIZE       2048    // Standard WebSocket buffer
  #endif
#else
#define USB_COMMAND_BUFFER_SIZE     256     // Standard command buffer size
#define USB_RESPONSE_BUFFER_SIZE    2048    // Standard response buffer size
#define JSON_BUFFER_SIZE            4096    // Standard JSON document buffer size
#define WEBSOCKET_BUFFER_SIZE       1024    // Standard WebSocket buffer
#endif

// WiFi configuration
#define WIFI_AP_SSID               "BMCU370-Config"
#define WIFI_AP_PASSWORD           "bmcu370setup"
#define WIFI_CONNECT_TIMEOUT_MS    10000    // 10 seconds
#define WIFI_PORTAL_TIMEOUT_MS     300000   // 5 minutes in config mode

// Web server configuration
#define WEB_SERVER_PORT            80
#define WEBSOCKET_MAX_CLIENTS      4
#define API_RATE_LIMIT_MS          100      // Minimum time between API calls

// BMCU370 USB interface configuration
#define BMCU370_VID                0x0403   // FTDI vendor ID (example)
#define BMCU370_PID                0x6001   // FTDI product ID (example)
#define USB_INTERFACE_CLASS        0x02     // CDC class
#define USB_INTERFACE_SUBCLASS     0x02     // ACM subclass

// System limits
#define MAX_FILAMENT_CHANNELS      4        // Maximum number of filament channels
#define MAX_LOG_ENTRIES           100       // Maximum log entries to keep
#define CONFIG_SAVE_INTERVAL_MS   30000     // Save config every 30 seconds if changed

// Debug configuration
#define DEBUG_USB_COMMUNICATION    1        // Enable USB communication debugging
#define DEBUG_WEB_REQUESTS         0        // Enable web request debugging
#define DEBUG_WEBSOCKET           1        // Enable WebSocket debugging

#endif // CONFIG_H