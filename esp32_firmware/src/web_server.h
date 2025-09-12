#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "bmcu370_interface.h"

// Forward declarations
class HistoricalDataManager;

class WebServerManager {
private:
    AsyncWebServer server;
    AsyncWebSocket websocket;
    BMCU370_Interface* bmcu_interface;
    HistoricalDataManager* history_manager;
    
    // Rate limiting
    unsigned long last_api_call[WEBSOCKET_MAX_CLIENTS];
    unsigned long last_websocket_update;
    
    // Request tracking
    uint32_t api_request_count;
    uint32_t websocket_message_count;
    uint32_t error_count;
    
    // Internal methods
    void setupRoutes();
    void setupWebSocket();
    void setupStaticFiles();
    
    // API handlers
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request);
    void handleGetLogs(AsyncWebServerRequest* request);
    void handleSystemControl(AsyncWebServerRequest* request);
    void handleWiFiScan(AsyncWebServerRequest* request);
    void handleWiFiConnect(AsyncWebServerRequest* request);
    
    // Historical data handlers
    void handleGetHistoricalData(AsyncWebServerRequest* request);
    void handleGetDataSummary(AsyncWebServerRequest* request);
    void handleGetTrendAnalysis(AsyncWebServerRequest* request);
    void handleClearHistory(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                            AwsEventType type, void* arg, uint8_t* data, size_t len);
    void broadcastStatus();
    void sendErrorToClient(AsyncWebSocketClient* client, const String& error);
    
    // Utility methods
    bool isRateLimited(AsyncWebServerRequest* request);
    String getClientIP(AsyncWebServerRequest* request);
    void logRequest(AsyncWebServerRequest* request, const String& endpoint);
    
public:
    WebServerManager();
    ~WebServerManager();
    
    // Initialization and control
    bool init(BMCU370_Interface* interface, HistoricalDataManager* history = nullptr);
    void handle();
    
    // Status information
    uint32_t getRequestCount() const { return api_request_count; }
    uint32_t getWebSocketMessageCount() const { return websocket_message_count; }
    uint32_t getErrorCount() const { return error_count; }
    uint16_t getConnectedClients() const { return websocket.count(); }
    
    // Utility methods
    void printServerInfo();
};

#endif // WEB_SERVER_H