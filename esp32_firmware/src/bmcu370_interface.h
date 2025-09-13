#ifndef BMCU370_INTERFACE_H
#define BMCU370_INTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
// #include "USBHostSerial.h" // Temporarily disabled - needs compatible version

// High-level BMCU370 interface
class BMCU370_Interface {
private:
    // USBHostSerial bmcu_serial; // Temporarily disabled - needs compatible version
    JsonDocument status_cache;
    JsonDocument config_cache;
    
    unsigned long last_status_update;
    unsigned long last_config_update;
    
    // Communication state
    bool device_online;
    uint32_t command_count;
    uint32_t error_count;
    String last_error;
    
    // Internal methods
    bool parseJsonResponse(const String& response, JsonDocument& doc);
    bool validateStatusResponse(const JsonDocument& doc);
    bool validateConfigResponse(const JsonDocument& doc);
    
    // Internal communication methods
    bool sendCommandAndGetResponse(const String& cmd, String& response, uint32_t timeout_ms = USB_TIMEOUT_MS);

public:
    BMCU370_Interface();
    ~BMCU370_Interface();
    
    // Initialization and connection
    bool init();
    void handleUSB(); // New method to handle USB events
    bool isConnected() const { return device_online; }
    
    // Status and configuration
    bool updateStatus();
    bool updateConfig();
    JsonDocument getStatus() const { return status_cache; }
    JsonDocument getConfig() const { return config_cache; }
    
    // Parameter control
    bool setParameter(const String& key, const String& value);
    bool setLEDBrightness(int channel, int brightness);
    bool setMotionParameter(const String& param, float value);
    
    // System control
    bool resetDevice();
    bool enterDFUMode();
    String getVersion();
    
    // Status information
    uint32_t getCommandCount() const { return command_count; }
    uint32_t getErrorCount() const { return error_count; }
    String getLastError() const { return last_error; }
    unsigned long getLastUpdateTime() const { return last_status_update; }
    
    // Utility methods
    String getConnectionStatus() const;
    void printDebugInfo();
};

#endif // BMCU370_INTERFACE_H