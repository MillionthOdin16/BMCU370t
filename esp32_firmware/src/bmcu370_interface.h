#ifndef BMCU370_INTERFACE_H
#define BMCU370_INTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// USB Host interface for BMCU370 communication
class BMCU370_USB_Host {
private:
    bool initialized;
    bool device_connected;
    unsigned long last_connect_attempt;
    
    // USB device information
    uint16_t vid, pid;
    uint8_t interface_class, interface_subclass;
    
    // Communication buffers
    char command_buffer[USB_COMMAND_BUFFER_SIZE];
    char response_buffer[USB_RESPONSE_BUFFER_SIZE];
    
    // Internal methods
    bool enumerateDevice();
    bool openCDCInterface();
    void closeCDCInterface();
    bool writeCommand(const char* command);
    int readResponse(char* buffer, size_t buffer_size, uint32_t timeout_ms);
    bool checkConnection();  // New method to check connection status
    
public:
    BMCU370_USB_Host();
    ~BMCU370_USB_Host();
    
    bool init();
    bool connect();
    void disconnect();
    bool isConnected() const { return device_connected; }
    
    // Communication methods
    bool sendCommand(const String& cmd);
    String readResponse(uint32_t timeout_ms = USB_TIMEOUT_MS);
    bool sendCommandAndGetResponse(const String& cmd, String& response, uint32_t timeout_ms = USB_TIMEOUT_MS);
    
    // Utility methods
    void printDeviceInfo();
    String getLastError() const;
};

// High-level BMCU370 interface
class BMCU370_Interface {
private:
    BMCU370_USB_Host usb_host;
    JsonDocument status_cache;
    JsonDocument config_cache;
    
    unsigned long last_status_update;
    unsigned long last_config_update;
    bool connection_state_changed;
    bool was_connected_last_update;
    
    // Communication state
    bool device_online;
    uint32_t command_count;
    uint32_t error_count;
    String last_error;
    
    // Internal methods
    bool parseJsonResponse(const String& response, JsonDocument& doc);
    bool validateStatusResponse(const JsonDocument& doc);
    bool validateConfigResponse(const JsonDocument& doc);
    void updateConnectionState(bool connected);
    
public:
    BMCU370_Interface();
    ~BMCU370_Interface();
    
    // Initialization and connection
    bool init();
    bool isConnected() const { return device_online; }
    bool wasConnectedLastUpdate() const { return was_connected_last_update; }
    
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