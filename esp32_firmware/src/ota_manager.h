#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <functional>

// OTA configuration
#define OTA_HOSTNAME           "BMCU370-ESP32"
#define OTA_PASSWORD           "bmcu370ota"
#define OTA_PORT               3232
#define MAX_UPLOAD_SIZE        (1024 * 1024)  // 1MB max upload

enum class OTAState {
    IDLE,
    STARTING,
    IN_PROGRESS,
    SUCCESS,
    ERROR
};

class OTAManager {
private:
    OTAState current_state;
    int progress_percent;
    String last_error;
    unsigned long start_time;
    size_t total_size;
    size_t written_size;
    
    // Web upload tracking
    bool web_upload_active;
    AsyncWebServerRequest* current_request;
    
    // Progress callback
    std::function<void(int)> on_progress_callback;

public:
    OTAManager();
    
    // Initialization
    bool init();
    void begin();
    void handle();
    
    // Status
    OTAState getState() const { return current_state; }
    int getProgress() const { return progress_percent; }
    String getLastError() const { return last_error; }
    bool isActive() const { return current_state == OTAState::IN_PROGRESS || current_state == OTAState::STARTING; }
    
    // Web server integration
    void setupWebHandlers(AsyncWebServer* server);
    
    // Callbacks
    void onProgress(std::function<void(int)> callback);

    // Manual control
    bool startOTA();
    void abortOTA();
    
private:
    // ArduinoOTA callbacks
    static void onStart();
    static void onEnd();
    static void onProgressCb(unsigned int progress, unsigned int total);
    static void onError(ota_error_t error);
    
    // Web upload handlers
    void handleUploadRequest(AsyncWebServerRequest *request);
    void handleUploadData(AsyncWebServerRequest *request, String filename, 
                         size_t index, uint8_t *data, size_t len, bool final);
    
    // Utilities
    void setState(OTAState state);
    void setError(const String& error);
    String getErrorString(ota_error_t error);
};

// Global instance
extern OTAManager ota_manager;

#endif // OTA_MANAGER_H