#include "bmcu370_interface.h"
#include <esp_log.h>

static const char* TAG = "BMCU370_Interface";

// BMCU370_Interface Implementation
BMCU370_Interface::BMCU370_Interface()
    : last_status_update(0), last_config_update(0),
      device_online(false), command_count(0), error_count(0) {
}

BMCU370_Interface::~BMCU370_Interface() {
}

bool BMCU370_Interface::init() {
    ESP_LOGI(TAG, "Initializing BMCU370 interface with USBHostSerial");
    
    // The USBHostSerial library is initialized when begin() is called.
    // We can pass the VID/PID of the device we are looking for.
    bmcu_serial.begin(BMCU370_VID, BMCU370_PID);

    // Initialize JSON documents
    status_cache.clear();
    config_cache.clear();
    
    ESP_LOGI(TAG, "BMCU370 interface initialized. Waiting for device connection...");
    return true;
}

void BMCU370_Interface::handleUSB() {
    // This method should be called in the main loop to handle USB events.
    bmcu_serial.task();

    // Check for connection/disconnection
    if (bmcu_serial.connected() != device_online) {
        device_online = bmcu_serial.connected();
        if (device_online) {
            ESP_LOGI(TAG, "BMCU370 device connected via USB");
            // Clear any old data
            while(bmcu_serial.available()) {
                bmcu_serial.read();
            }
        } else {
            ESP_LOGW(TAG, "BMCU370 device disconnected");
            // Clear cached data as it is no longer valid
            status_cache.clear();
            config_cache.clear();
        }
    }
}

bool BMCU370_Interface::sendCommandAndGetResponse(const String& cmd, String& response, uint32_t timeout_ms) {
    if (!device_online) {
        last_error = "Device not connected";
        return false;
    }

    // Clear any residual data in the buffer
    while(bmcu_serial.available()) {
        bmcu_serial.read();
    }

    bmcu_serial.println(cmd);
    command_count++;

#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Sent command: %s", cmd.c_str());
#endif

    unsigned long start_time = millis();
    while (millis() - start_time < timeout_ms) {
        if (bmcu_serial.available() > 0) {
            response = bmcu_serial.readStringUntil('\n');
            response.trim(); // Remove any trailing whitespace/newline
#if DEBUG_USB_COMMUNICATION
            ESP_LOGI(TAG, "Received response: %s", response.c_str());
#endif
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy-waiting
    }

    ESP_LOGW(TAG, "Timeout waiting for response to command: %s", cmd.c_str());
    last_error = "Timeout waiting for response from BMCU";
    error_count++;
    return false;
}


bool BMCU370_Interface::updateStatus() {
    if (!device_online) {
        return false;
    }
    
    String response;
    if (!sendCommandAndGetResponse("GET_STATUS", response)) {
        last_error = "Failed to get status from BMCU370";
        return false;
    }
    
    if (!parseJsonResponse(response, status_cache)) {
        last_error = "Failed to parse status JSON";
        return false;
    }
    
    if (!validateStatusResponse(status_cache)) {
        last_error = "Invalid status response format";
        return false;
    }
    
    last_status_update = millis();
    return true;
}

bool BMCU370_Interface::updateConfig() {
    if (!device_online) {
        return false;
    }
    
    String response;
    if (!sendCommandAndGetResponse("GET_CONFIG", response)) {
        last_error = "Failed to get config from BMCU370";
        return false;
    }
    
    if (!parseJsonResponse(response, config_cache)) {
        last_error = "Failed to parse config JSON";
        return false;
    }
    
    if (!validateConfigResponse(config_cache)) {
        last_error = "Invalid config response format";
        return false;
    }
    
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
    
    if (!sendCommandAndGetResponse(command, response)) {
        last_error = "Failed to set parameter: " + key;
        return false;
    }
    
    // Check if response indicates success
    if (response.indexOf("OK") == -1) {
        last_error = "Parameter set failed: " + response;
        error_count++;
        return false;
    }
    
    ESP_LOGI(TAG, "Parameter set successfully: %s=%s", key.c_str(), value.c_str());
    
    // Update config cache after a short delay
    vTaskDelay(pdMS_TO_TICKS(100));
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
    bool result = sendCommandAndGetResponse("RESET", response);
    
    if (result) {
        ESP_LOGI(TAG, "Device reset command sent");
        // Device will disconnect after reset
        device_online = false;
    } else {
        last_error = "Failed to send reset command";
    }
    
    return result;
}

bool BMCU370_Interface::enterDFUMode() {
    if (!device_online) {
        return false;
    }
    
    String response;
    bool result = sendCommandAndGetResponse("DFU", response);
    
    if (result) {
        ESP_LOGI(TAG, "DFU mode command sent");
        // Device will disconnect and enter DFU mode
        device_online = false;
    } else {
        last_error = "Failed to send DFU command";
    }
    
    return result;
}

String BMCU370_Interface::getVersion() {
    if (!device_online) {
        return "";
    }
    
    String response;
    if (sendCommandAndGetResponse("GET_VERSION", response)) {
        return response;
    } else {
        last_error = "Failed to get version";
        return "";
    }
}

bool BMCU370_Interface::parseJsonResponse(const String& response, JsonDocument& doc) {
    doc.clear();
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s. Response was: %s", error.c_str(), response.c_str());
        error_count++;
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateStatusResponse(const JsonDocument& doc) {
    if (!doc["system"].is<JsonObject>() || !doc["channels"].is<JsonArray>()) {
        ESP_LOGE(TAG, "Status response missing required fields ('system' or 'channels')");
        error_count++;
        return false;
    }
    
    JsonObjectConst system = doc["system"].as<JsonObjectConst>();
    if (!system["version"] || !system["uptime"]) {
        ESP_LOGE(TAG, "System object missing required fields ('version' or 'uptime')");
        error_count++;
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateConfigResponse(const JsonDocument& doc) {
    if (!doc["config"].is<JsonObject>()) {
        ESP_LOGE(TAG, "Config response missing 'config' object");
        error_count++;
        return false;
    }
    
    return true;
}

String BMCU370_Interface::getConnectionStatus() const {
    return device_online ? "Connected" : "Disconnected";
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