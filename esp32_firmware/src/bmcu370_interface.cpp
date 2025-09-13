#include "bmcu370_interface.h"
#include <esp_log.h>

static const char* TAG = "BMCU370_Interface";

// BMCU370_USB_Host Implementation
BMCU370_USB_Host::BMCU370_USB_Host() 
    : initialized(false), device_connected(false), last_connect_attempt(0),
      vid(BMCU370_VID), pid(BMCU370_PID), 
      interface_class(USB_INTERFACE_CLASS), interface_subclass(USB_INTERFACE_SUBCLASS) {
    memset(command_buffer, 0, sizeof(command_buffer));
    memset(response_buffer, 0, sizeof(response_buffer));
}

BMCU370_USB_Host::~BMCU370_USB_Host() {
    disconnect();
}

bool BMCU370_USB_Host::init() {
    if (initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing USB host interface");
    
    // TODO: Initialize USB host stack for ESP32-S3
    // This would involve:
    // 1. Configure USB host pins
    // 2. Initialize USB host library
    // 3. Register CDC-ACM driver
    // 4. Set up event handlers
    
    // For now, mark as initialized (placeholder)
    initialized = true;
    ESP_LOGI(TAG, "USB host interface initialized (placeholder)");
    
    return true;
}

bool BMCU370_USB_Host::connect() {
    if (!initialized) {
        ESP_LOGE(TAG, "USB host not initialized");
        return false;
    }
    
    unsigned long current_time = millis();
    if (current_time - last_connect_attempt < USB_RETRY_DELAY_MS) {
        return device_connected; // Too soon to retry
    }
    
    last_connect_attempt = current_time;
    
    // Only log connection attempts every 30 seconds to reduce spam
    static unsigned long last_log_time = 0;
    bool should_log = (current_time - last_log_time) > 30000;
    
    if (should_log) {
        ESP_LOGI(TAG, "Attempting to connect to BMCU370...");
        last_log_time = current_time;
    }
    
    // TODO: Implement actual USB device enumeration and connection
    // This would involve:
    // 1. Scan for USB devices
    // 2. Match VID/PID
    // 3. Open CDC-ACM interface
    // 4. Configure communication parameters
    
    // Placeholder: simulate connection attempt
    device_connected = false; // Will be true when real hardware is connected
    
    if (device_connected) {
        ESP_LOGI(TAG, "Successfully connected to BMCU370");
        printDeviceInfo();
    } else if (should_log) {
        ESP_LOGW(TAG, "BMCU370 device not found (will retry every 30s)");
    }
    
    return device_connected;
}

void BMCU370_USB_Host::disconnect() {
    if (device_connected) {
        ESP_LOGI(TAG, "Disconnecting from BMCU370");
        closeCDCInterface();
        device_connected = false;
    }
}

bool BMCU370_USB_Host::sendCommand(const String& cmd) {
    if (!device_connected) {
        ESP_LOGE(TAG, "Device not connected");
        return false;
    }
    
    if (cmd.length() >= USB_COMMAND_BUFFER_SIZE - 2) { // -2 for \n and \0
        ESP_LOGE(TAG, "Command too long");
        return false;
    }
    
    snprintf(command_buffer, sizeof(command_buffer), "%s\n", cmd.c_str());
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Sending command: %s", cmd.c_str());
#endif
    
    return writeCommand(command_buffer);
}

String BMCU370_USB_Host::readResponse(uint32_t timeout_ms) {
    if (!device_connected) {
        ESP_LOGE(TAG, "Device not connected");
        return "";
    }
    
    int bytes_read = readResponse(response_buffer, sizeof(response_buffer), timeout_ms);
    if (bytes_read <= 0) {
        ESP_LOGW(TAG, "No response received");
        return "";
    }
    
    response_buffer[bytes_read] = '\0';
    String result(response_buffer);
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Received response (%d bytes): %s", bytes_read, response_buffer);
#endif
    
    return result;
}

bool BMCU370_USB_Host::sendCommandAndGetResponse(const String& cmd, String& response, uint32_t timeout_ms) {
    if (!sendCommand(cmd)) {
        return false;
    }
    
    response = readResponse(timeout_ms);
    return !response.isEmpty();
}

void BMCU370_USB_Host::printDeviceInfo() {
    ESP_LOGI(TAG, "BMCU370 Device Info:");
    ESP_LOGI(TAG, "  VID: 0x%04X", vid);
    ESP_LOGI(TAG, "  PID: 0x%04X", pid);
    ESP_LOGI(TAG, "  Interface Class: 0x%02X", interface_class);
    ESP_LOGI(TAG, "  Interface Subclass: 0x%02X", interface_subclass);
}

// Placeholder implementations for internal methods
bool BMCU370_USB_Host::enumerateDevice() {
    // TODO: Implement USB device enumeration
    return false;
}

bool BMCU370_USB_Host::openCDCInterface() {
    // TODO: Implement CDC interface opening
    return false;
}

void BMCU370_USB_Host::closeCDCInterface() {
    // TODO: Implement CDC interface closing
}

bool BMCU370_USB_Host::writeCommand(const char* command) {
    // TODO: Implement USB write
    return false;
}

int BMCU370_USB_Host::readResponse(char* buffer, size_t buffer_size, uint32_t timeout_ms) {
    // TODO: Implement USB read with timeout
    return 0;
}

String BMCU370_USB_Host::getLastError() const {
    // TODO: Return last USB error
    return "Not implemented";
}

// BMCU370_Interface Implementation
BMCU370_Interface::BMCU370_Interface() 
    : last_status_update(0), last_config_update(0), 
      connection_state_changed(false), was_connected_last_update(false),
      device_online(false), command_count(0), error_count(0) {
}

BMCU370_Interface::~BMCU370_Interface() {
}

bool BMCU370_Interface::init() {
    ESP_LOGI(TAG, "Initializing BMCU370 interface");
    
    if (!usb_host.init()) {
        ESP_LOGE(TAG, "Failed to initialize USB host");
        return false;
    }
    
    // Initialize JSON documents
    status_cache.clear();
    config_cache.clear();
    
    ESP_LOGI(TAG, "BMCU370 interface initialized successfully");
    return true;
}

bool BMCU370_Interface::updateStatus() {
    bool connected = usb_host.connect();
    updateConnectionState(connected);
    
    if (!connected) {
        return false;
    }
    
    String response;
    if (!usb_host.sendCommandAndGetResponse("GET_STATUS", response)) {
        error_count++;
        last_error = "Failed to get status from BMCU370";
        ESP_LOGW(TAG, "%s", last_error.c_str());
        return false;
    }
    
    if (!parseJsonResponse(response, status_cache)) {
        error_count++;
        last_error = "Failed to parse status JSON";
        ESP_LOGW(TAG, "%s", last_error.c_str());
        return false;
    }
    
    if (!validateStatusResponse(status_cache)) {
        error_count++;
        last_error = "Invalid status response format";
        ESP_LOGW(TAG, "%s", last_error.c_str());
        return false;
    }
    
    command_count++;
    last_status_update = millis();
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Status updated successfully");
#endif
    
    return true;
}

bool BMCU370_Interface::updateConfig() {
    if (!device_online) {
        return false;
    }
    
    String response;
    if (!usb_host.sendCommandAndGetResponse("GET_CONFIG", response)) {
        error_count++;
        last_error = "Failed to get config from BMCU370";
        return false;
    }
    
    if (!parseJsonResponse(response, config_cache)) {
        error_count++;
        last_error = "Failed to parse config JSON";
        return false;
    }
    
    if (!validateConfigResponse(config_cache)) {
        error_count++;
        last_error = "Invalid config response format";
        return false;
    }
    
    command_count++;
    last_config_update = millis();
    return true;
}

bool BMCU370_Interface::setParameter(const String& key, const String& value) {
    if (!device_online) {
        last_error = "Device not connected";
        return false;
    }
    
    String command = "SET_PARAM " + key + "=" + value;
    String response;
    
    if (!usb_host.sendCommandAndGetResponse(command, response)) {
        error_count++;
        last_error = "Failed to set parameter: " + key;
        return false;
    }
    
    // Check if response indicates success
    if (response.indexOf("OK") == -1) {
        error_count++;
        last_error = "Parameter set failed: " + response;
        return false;
    }
    
    command_count++;
    ESP_LOGI(TAG, "Parameter set successfully: %s=%s", key.c_str(), value.c_str());
    
    // Update config cache
    updateConfig();
    
    return true;
}

bool BMCU370_Interface::setLEDBrightness(int channel, int brightness) {
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        last_error = "Invalid channel number";
        return false;
    }
    
    if (brightness < 0 || brightness > 255) {
        last_error = "Invalid brightness value (0-255)";
        return false;
    }
    
    String key = "led_brightness_ch" + String(channel);
    return setParameter(key, String(brightness));
}

bool BMCU370_Interface::setMotionParameter(const String& param, float value) {
    String key = "motion_" + param;
    return setParameter(key, String(value, 2)); // 2 decimal places
}

bool BMCU370_Interface::resetDevice() {
    if (!device_online) {
        return false;
    }
    
    String response;
    bool result = usb_host.sendCommandAndGetResponse("RESET", response);
    
    if (result) {
        command_count++;
        ESP_LOGI(TAG, "Device reset command sent");
        // Device will disconnect after reset
        device_online = false;
    } else {
        error_count++;
        last_error = "Failed to send reset command";
    }
    
    return result;
}

bool BMCU370_Interface::enterDFUMode() {
    if (!device_online) {
        return false;
    }
    
    String response;
    bool result = usb_host.sendCommandAndGetResponse("DFU", response);
    
    if (result) {
        command_count++;
        ESP_LOGI(TAG, "DFU mode command sent");
        // Device will disconnect and enter DFU mode
        device_online = false;
    } else {
        error_count++;
        last_error = "Failed to send DFU command";
    }
    
    return result;
}

String BMCU370_Interface::getVersion() {
    if (!device_online) {
        return "";
    }
    
    String response;
    if (usb_host.sendCommandAndGetResponse("GET_VERSION", response)) {
        command_count++;
        return response;
    } else {
        error_count++;
        last_error = "Failed to get version";
        return "";
    }
}

void BMCU370_Interface::updateConnectionState(bool connected) {
    if (connected != device_online) {
        connection_state_changed = true;
        was_connected_last_update = device_online;
        device_online = connected;
        
        if (connected) {
            ESP_LOGI(TAG, "BMCU370 connected");
        } else {
            ESP_LOGI(TAG, "BMCU370 disconnected");
        }
    } else {
        connection_state_changed = false;
        was_connected_last_update = device_online;
    }
}

bool BMCU370_Interface::parseJsonResponse(const String& response, JsonDocument& doc) {
    doc.clear();
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateStatusResponse(const JsonDocument& doc) {
    // Check for required top-level objects
    if (!doc["system"].is<JsonObject>() || !doc["channels"].is<JsonArray>()) {
        ESP_LOGE(TAG, "Status response missing required fields");
        return false;
    }
    
    // Validate system object
    JsonObjectConst system = doc["system"].as<JsonObjectConst>();
    if (!system["uptime"].is<int>() || !system["version"].is<const char*>()) {
        ESP_LOGE(TAG, "System object missing required fields");
        return false;
    }
    
    // Validate channels array
    JsonArrayConst channels = doc["channels"].as<JsonArrayConst>();
    if (channels.size() == 0 || channels.size() > MAX_FILAMENT_CHANNELS) {
        ESP_LOGE(TAG, "Invalid number of channels: %d", channels.size());
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateConfigResponse(const JsonDocument& doc) {
    // Check for config object
    if (!doc["config"].is<JsonObject>()) {
        ESP_LOGE(TAG, "Config response missing config object");
        return false;
    }
    
    return true;
}

String BMCU370_Interface::getConnectionStatus() const {
    if (device_online) {
        return "Connected";
    } else {
        return "Disconnected";
    }
}

void BMCU370_Interface::printDebugInfo() {
    ESP_LOGI(TAG, "=== BMCU370 Interface Debug Info ===");
    ESP_LOGI(TAG, "Connection Status: %s", getConnectionStatus().c_str());
    ESP_LOGI(TAG, "Commands Sent: %lu", command_count);
    ESP_LOGI(TAG, "Errors: %lu", error_count);
    ESP_LOGI(TAG, "Last Update: %lu ms ago", millis() - last_status_update);
    if (!last_error.isEmpty()) {
        ESP_LOGI(TAG, "Last Error: %s", last_error.c_str());
    }
    ESP_LOGI(TAG, "=======================================");
}