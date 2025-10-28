#include "ota_manager.h"
#include "config.h"
#include <esp_log.h>

static const char* TAG = "OTAManager";

// Global instance
OTAManager ota_manager;

// Static instance pointer for callbacks
static OTAManager* instance_ptr = nullptr;

OTAManager::OTAManager() 
    : current_state(OTAState::IDLE), progress_percent(0), start_time(0),
      total_size(0), written_size(0), web_upload_active(false), current_request(nullptr) {
    instance_ptr = this;
}

bool OTAManager::init() {
    ESP_LOGI(TAG, "Initializing OTA manager");
    
    // Set hostname for OTA
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setPort(OTA_PORT);
    
    // Set callbacks
    ArduinoOTA.onStart([]() {
        if (instance_ptr) instance_ptr->onStart();
    });
    
    ArduinoOTA.onEnd([]() {
        if (instance_ptr) instance_ptr->onEnd();
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if (instance_ptr) instance_ptr->onProgressCb(progress, total);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        if (instance_ptr) instance_ptr->onError(error);
    });
    
    return true;
}

void OTAManager::begin() {
    ArduinoOTA.begin();
    ESP_LOGI(TAG, "OTA server started on port %d", OTA_PORT);
}

void OTAManager::handle() {
    ArduinoOTA.handle();
}

void OTAManager::onProgress(std::function<void(int)> callback) {
    on_progress_callback = callback;
}

bool OTAManager::startOTA() {
    if (current_state != OTAState::IDLE) {
        ESP_LOGW(TAG, "OTA already in progress");
        return false;
    }
    
    setState(OTAState::STARTING);
    ESP_LOGI(TAG, "Starting OTA update");
    return true;
}

void OTAManager::abortOTA() {
    if (current_state == OTAState::IN_PROGRESS) {
        ESP_LOGW(TAG, "Aborting OTA update");
        setState(OTAState::ERROR);
        setError("Update aborted by user");
    }
}

void OTAManager::setupWebHandlers(AsyncWebServer* server) {
    // OTA status endpoint
    server->on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"state\":\"" + String((int)current_state) + "\",";
        response += "\"progress\":" + String(progress_percent) + ",";
        response += "\"error\":\"" + last_error + "\",";
        response += "\"active\":" + String(isActive() ? "true" : "false");
        response += "}";
        
        request->send(200, "application/json", response);
    });
    
    // Start OTA endpoint
    server->on("/api/ota/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (startOTA()) {
            request->send(200, "application/json", "{\"status\":\"started\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"OTA already active\"}");
        }
    });
    
    // Abort OTA endpoint
    server->on("/api/ota/abort", HTTP_POST, [this](AsyncWebServerRequest *request) {
        abortOTA();
        request->send(200, "application/json", "{\"status\":\"aborted\"}");
    });
    
    // Firmware upload endpoint
    server->on("/api/ota/upload", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
            handleUploadRequest(request);
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleUploadData(request, filename, index, data, len, final);
        }
    );
    
    ESP_LOGI(TAG, "OTA web handlers setup complete");
}

void OTAManager::handleUploadRequest(AsyncWebServerRequest *request) {
    if (web_upload_active) {
        request->send(200, "application/json", 
                     current_state == OTAState::SUCCESS ? 
                     "{\"status\":\"success\"}" : 
                     "{\"error\":\"" + last_error + "\"}");
    } else {
        request->send(400, "application/json", "{\"error\":\"No upload in progress\"}");
    }
    
    web_upload_active = false;
    current_request = nullptr;
}

void OTAManager::handleUploadData(AsyncWebServerRequest *request, String filename, 
                                 size_t index, uint8_t *data, size_t len, bool final) {
    // Start upload on first chunk
    if (index == 0) {
        ESP_LOGI(TAG, "Starting firmware upload: %s", filename.c_str());
        
        setState(OTAState::STARTING);
        web_upload_active = true;
        current_request = request;
        
        // Get total size from request
        total_size = request->contentLength();
        if (total_size > MAX_UPLOAD_SIZE) {
            setError("File size exceeds maximum limit");
            setState(OTAState::ERROR);
            return;
        }
        written_size = 0;
        
        // Begin update
        if (!Update.begin(total_size)) {
            setError(Update.errorString());
            setState(OTAState::ERROR);
            return;
        }
        
        setState(OTAState::IN_PROGRESS);
    }
    
    // Write data chunk
    if (current_state == OTAState::IN_PROGRESS) {
        if (Update.write(data, len) != len) {
            setError(Update.errorString());
            setState(OTAState::ERROR);
            Update.abort();
            return;
        }
        
        written_size += len;
        progress_percent = (written_size * 100) / total_size;

        if (on_progress_callback) {
            on_progress_callback(progress_percent);
        }
        
        ESP_LOGD(TAG, "Upload progress: %d%% (%d/%d bytes)", 
                progress_percent, written_size, total_size);
    }
    
    // Finalize upload
    if (final) {
        if (current_state == OTAState::IN_PROGRESS) {
            if (Update.end(true)) {
                ESP_LOGI(TAG, "Firmware upload completed successfully");
                setState(OTAState::SUCCESS);
                
                // Schedule restart
                ESP_LOGI(TAG, "Restarting in 2 seconds...");
                delay(2000);
                ESP.restart();
            } else {
                setError(Update.errorString());
                setState(OTAState::ERROR);
                Update.abort();
            }
        }
    }
}

void OTAManager::setState(OTAState state) {
    if (current_state != state) {
        current_state = state;
        
        if (state == OTAState::STARTING) {
            start_time = millis();
            progress_percent = 0;
            last_error = "";
        } else if (state == OTAState::SUCCESS || state == OTAState::ERROR) {
            unsigned long duration = millis() - start_time;
            ESP_LOGI(TAG, "OTA update %s after %lu ms", 
                    state == OTAState::SUCCESS ? "completed" : "failed", duration);
        }
    }
}

void OTAManager::setError(const String& error) {
    last_error = error;
    ESP_LOGE(TAG, "OTA Error: %s", error.c_str());
}

String OTAManager::getErrorString(ota_error_t error) {
    switch (error) {
        case OTA_AUTH_ERROR: return "Authentication failed";
        case OTA_BEGIN_ERROR: return "Begin failed";
        case OTA_CONNECT_ERROR: return "Connection failed";
        case OTA_RECEIVE_ERROR: return "Receive failed";
        case OTA_END_ERROR: return "End failed";
        default: return "Unknown error";
    }
}

// Static callback implementations
void OTAManager::onStart() {
    if (instance_ptr) {
        instance_ptr->setState(OTAState::STARTING);
        
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        ESP_LOGI(TAG, "Starting OTA update: %s", type.c_str());
        
        instance_ptr->setState(OTAState::IN_PROGRESS);
    }
}

void OTAManager::onEnd() {
    if (instance_ptr) {
        instance_ptr->setState(OTAState::SUCCESS);
        ESP_LOGI(TAG, "OTA update completed");
    }
}

void OTAManager::onProgressCb(unsigned int progress, unsigned int total) {
    if (instance_ptr) {
        instance_ptr->progress_percent = (progress * 100) / total;
        
        // Log progress every 10%
        static int last_reported = -1;
        int current_percent = instance_ptr->progress_percent / 10;
        if (current_percent != last_reported) {
            ESP_LOGI(TAG, "OTA Progress: %d%%", instance_ptr->progress_percent);
            last_reported = current_percent;
        }

        if (instance_ptr->on_progress_callback) {
            instance_ptr->on_progress_callback(instance_ptr->progress_percent);
        }
    }
}

void OTAManager::onError(ota_error_t error) {
    if (instance_ptr) {
        instance_ptr->setState(OTAState::ERROR);
        instance_ptr->setError(instance_ptr->getErrorString(error));
    }
}