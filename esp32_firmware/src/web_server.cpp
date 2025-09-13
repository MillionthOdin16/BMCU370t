#include "web_server.h"
#include "wifi_manager.h"
#include "historical_data.h"
#include "ota_manager.h"
#include <esp_log.h>

static const char* TAG = "WebServer";

// External references
extern WiFiManager wifi_manager;
extern OTAManager ota_manager;

WebServerManager::WebServerManager() 
    : server(WEB_SERVER_PORT), websocket("/ws"), bmcu_interface(nullptr), history_manager(nullptr),
      last_websocket_update(0), api_request_count(0), 
      websocket_message_count(0), error_count(0) {
    
    // Initialize rate limiting arrays
    for (int i = 0; i < WEBSOCKET_MAX_CLIENTS; i++) {
        last_api_call[i] = 0;
    }
}

WebServerManager::~WebServerManager() {
}

bool WebServerManager::init(BMCU370_Interface* interface, HistoricalDataManager* history) {
    if (!interface) {
        ESP_LOGE(TAG, "BMCU370 interface is null");
        return false;
    }
    
    bmcu_interface = interface;
    history_manager = history;
    
    ESP_LOGI(TAG, "Initializing web server on port %d", WEB_SERVER_PORT);
    
    // Setup routes and handlers
    setupRoutes();
    setupWebSocket();
    setupStaticFiles();
    
    // Start server
    server.begin();
    
    ESP_LOGI(TAG, "Web server started successfully");
    return true;
}

void WebServerManager::handle() {
    unsigned long current_time = millis();
    
    // Broadcast status updates via WebSocket
    if (current_time - last_websocket_update >= WEBSOCKET_UPDATE_INTERVAL) {
        if (websocket.count() > 0 && bmcu_interface && bmcu_interface->isConnected()) {
            broadcastStatus();
        }
        last_websocket_update = current_time;
    }
    
    // Cleanup closed WebSocket connections
    websocket.cleanupClients();
}

void WebServerManager::setupRoutes() {
    ESP_LOGI(TAG, "Setting up API routes");
    
    // API Routes
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStatus(request);
    });
    
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetConfig(request);
    });
    
    server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleSetConfig(request);
    });
    
    server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetLogs(request);
    });
    
    server.on("/api/system", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleSystemControl(request);
    });
    
    server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleWiFiScan(request);
    });
    
    server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleWiFiConnect(request);
    });
    
    server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleWiFiStatus(request);
    });
    
    // Historical data endpoints
    server.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetHistoricalData(request);
    });
    
    server.on("/api/history/summary", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetDataSummary(request);
    });
    
    server.on("/api/history/trends", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetTrendAnalysis(request);
    });
    
    server.on("/api/history/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleClearHistory(request);
    });
    
    // Setup OTA web handlers
    ota_manager.setupWebHandlers(&server);
    
    // Default route for SPA
    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
        } else {
            // Serve index.html for any non-API route (SPA routing)
            request->send(LittleFS, "/index.html", "text/html");
        }
    });
    
    ESP_LOGI(TAG, "API routes configured");
}

void WebServerManager::setupWebSocket() {
    ESP_LOGI(TAG, "Setting up WebSocket");
    
    websocket.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                            AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server.addHandler(&websocket);
    
    ESP_LOGI(TAG, "WebSocket configured");
}

void WebServerManager::setupStaticFiles() {
    ESP_LOGI(TAG, "Setting up static file serving");
    
    // Serve static files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // Set cache headers for static assets
    server.serveStatic("/css/", LittleFS, "/css/").setCacheControl("max-age=3600");
    server.serveStatic("/js/", LittleFS, "/js/").setCacheControl("max-age=3600");
    server.serveStatic("/img/", LittleFS, "/img/").setCacheControl("max-age=86400");
    
    ESP_LOGI(TAG, "Static file serving configured");
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest* request) {
    logRequest(request, "/api/status");
    
    if (isRateLimited(request)) {
        request->send(429, "application/json", "{\"error\":\"Rate limited\"}");
        return;
    }
    
    if (!bmcu_interface) {
        error_count++;
        request->send(500, "application/json", "{\"error\":\"BMCU370 interface not initialized\"}");
        return;
    }
    
    if (!bmcu_interface->isConnected()) {
        request->send(503, "application/json", "{\"error\":\"BMCU370 not connected\"}");
        return;
    }
    
    JsonDocument status = bmcu_interface->getStatus();
    String response;
    serializeJson(status, response);
    
    request->send(200, "application/json", response);
    api_request_count++;
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest* request) {
    logRequest(request, "/api/config");
    
    if (isRateLimited(request)) {
        request->send(429, "application/json", "{\"error\":\"Rate limited\"}");
        return;
    }
    
    if (!bmcu_interface) {
        error_count++;
        request->send(500, "application/json", "{\"error\":\"BMCU370 interface not initialized\"}");
        return;
    }
    
    if (!bmcu_interface->isConnected()) {
        request->send(503, "application/json", "{\"error\":\"BMCU370 not connected\"}");
        return;
    }
    
    JsonDocument config = bmcu_interface->getConfig();
    String response;
    serializeJson(config, response);
    
    request->send(200, "application/json", response);
    api_request_count++;
}

void WebServerManager::handleSetConfig(AsyncWebServerRequest* request) {
    logRequest(request, "/api/config [POST]");
    
    if (isRateLimited(request)) {
        request->send(429, "application/json", "{\"error\":\"Rate limited\"}");
        return;
    }
    
    if (!bmcu_interface) {
        error_count++;
        request->send(500, "application/json", "{\"error\":\"BMCU370 interface not initialized\"}");
        return;
    }
    
    if (!bmcu_interface->isConnected()) {
        request->send(503, "application/json", "{\"error\":\"BMCU370 not connected\"}");
        return;
    }
    
    // Handle POST body data
    String key = "";
    String value = "";
    
    if (request->hasParam("key", true) && request->hasParam("value", true)) {
        key = request->getParam("key", true)->value();
        value = request->getParam("value", true)->value();
    } else {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Missing key or value parameter\"}");
        return;
    }
    
    bool success = bmcu_interface->setParameter(key, value);
    if (success) {
        request->send(200, "application/json", "{\"success\":true}");
        api_request_count++;
    } else {
        error_count++;
        String error = "{\"error\":\"Failed to set parameter: " + bmcu_interface->getLastError() + "\"}";
        request->send(400, "application/json", error);
    }
}

void WebServerManager::handleGetLogs(AsyncWebServerRequest* request) {
    logRequest(request, "/api/logs");
    
    // Create a simple log response
    JsonDocument logs;
    logs["esp32_logs"].to<JsonArray>();
    logs["bmcu370_interface"]["command_count"] = bmcu_interface ? bmcu_interface->getCommandCount() : 0;
    logs["bmcu370_interface"]["error_count"] = bmcu_interface ? bmcu_interface->getErrorCount() : 0;
    logs["bmcu370_interface"]["last_error"] = bmcu_interface ? bmcu_interface->getLastError() : "N/A";
    logs["web_server"]["request_count"] = api_request_count;
    logs["web_server"]["websocket_messages"] = websocket_message_count;
    logs["web_server"]["error_count"] = error_count;
    logs["web_server"]["connected_clients"] = websocket.count();
    
    String response;
    serializeJson(logs, response);
    
    request->send(200, "application/json", response);
    api_request_count++;
}

void WebServerManager::handleSystemControl(AsyncWebServerRequest* request) {
    logRequest(request, "/api/system [POST]");
    
    if (!request->hasParam("action", true)) {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Missing action parameter\"}");
        return;
    }
    
    String action = request->getParam("action", true)->value();
    
    if (action == "reset_bmcu370") {
        if (bmcu_interface && bmcu_interface->isConnected()) {
            bool success = bmcu_interface->resetDevice();
            if (success) {
                request->send(200, "application/json", "{\"success\":true,\"message\":\"BMCU370 reset command sent\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to reset BMCU370\"}");
            }
        } else {
            request->send(503, "application/json", "{\"error\":\"BMCU370 not connected\"}");
        }
    } else if (action == "dfu_mode") {
        if (bmcu_interface && bmcu_interface->isConnected()) {
            bool success = bmcu_interface->enterDFUMode();
            if (success) {
                request->send(200, "application/json", "{\"success\":true,\"message\":\"BMCU370 entering DFU mode\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to enter DFU mode\"}");
            }
        } else {
            request->send(503, "application/json", "{\"error\":\"BMCU370 not connected\"}");
        }
    } else if (action == "reset_esp32") {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"ESP32 restarting...\"}");
        delay(1000);
        ESP.restart();
    } else {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Unknown action\"}");
    }
    
    api_request_count++;
}

void WebServerManager::handleWiFiScan(AsyncWebServerRequest* request) {
    logRequest(request, "/api/wifi/scan");
    
    ESP_LOGI(TAG, "Starting WiFi scan...");
    int n = WiFi.scanNetworks();
    
    String json = "{\"networks\":[";
    
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"encryption\":\"" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted") + "\"";
            json += "}";
        }
    }
    
    json += "],\"count\":" + String(n) + "}";
    
    WiFi.scanDelete();
    
    request->send(200, "application/json", json);
    api_request_count++;
}

void WebServerManager::handleWiFiConnect(AsyncWebServerRequest* request) {
    logRequest(request, "/api/wifi/connect [POST]");
    
    if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Missing SSID or password\"}");
        return;
    }
    
    String ssid = request->getParam("ssid", true)->value();
    String password = request->getParam("password", true)->value();
    
    bool success = wifi_manager.connectToNetwork(ssid, password);
    if (success) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Connected to WiFi\"}");
    } else {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Failed to connect to WiFi\"}");
    }
    
    api_request_count++;
}

void WebServerManager::handleWiFiStatus(AsyncWebServerRequest* request) {
    logRequest(request, "/api/wifi/status");
    
    String json = "{";
    json += "\"connected\":" + String(wifi_manager.isConnected() ? "true" : "false") + ",";
    json += "\"ssid\":\"" + wifi_manager.getSSID() + "\",";
    json += "\"ip\":\"" + wifi_manager.getIPAddress() + "\",";
    json += "\"signal\":" + String(wifi_manager.getSignalStrength()) + ",";
    json += "\"ap_active\":" + String(wifi_manager.isAPActive() ? "true" : "false") + ",";
    json += "\"ap_ip\":\"" + wifi_manager.getAPIP() + "\"";
    json += "}";
    
    request->send(200, "application/json", json);
    api_request_count++;
}

void WebServerManager::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            ESP_LOGI(TAG, "WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
            // Send initial status
            if (bmcu_interface && bmcu_interface->isConnected()) {
                JsonDocument status = bmcu_interface->getStatus();
                String message;
                serializeJson(status, message);
                client->text(message);
            }
            break;
            
        case WS_EVT_DISCONNECT:
            ESP_LOGI(TAG, "WebSocket client #%u disconnected", client->id());
            break;
            
        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0; // Null terminate
                String message = (char*)data;
                
#if DEBUG_WEBSOCKET
                ESP_LOGI(TAG, "WebSocket message from client #%u: %s", client->id(), message.c_str());
#endif
                
                websocket_message_count++;
                
                // Handle WebSocket commands
                if (message == "get_status") {
                    if (bmcu_interface && bmcu_interface->isConnected()) {
                        JsonDocument status = bmcu_interface->getStatus();
                        String response;
                        serializeJson(status, response);
                        client->text(response);
                    } else {
                        sendErrorToClient(client, "BMCU370 not connected");
                    }
                } else {
                    sendErrorToClient(client, "Unknown command");
                }
            }
            break;
        }
        
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServerManager::broadcastStatus() {
    if (websocket.count() == 0 || !bmcu_interface) {
        return;
    }
    
    JsonDocument status = bmcu_interface->getStatus();
    String message;
    serializeJson(status, message);
    
    websocket.textAll(message);
    
#if DEBUG_WEBSOCKET
    ESP_LOGI(TAG, "Broadcasted status to %d clients", websocket.count());
#endif
}

void WebServerManager::sendErrorToClient(AsyncWebSocketClient* client, const String& error) {
    JsonDocument errorDoc;
    errorDoc["error"] = error;
    String message;
    serializeJson(errorDoc, message);
    client->text(message);
}

bool WebServerManager::isRateLimited(AsyncWebServerRequest* request) {
    unsigned long current_time = millis();
    String client_ip = getClientIP(request);
    
    // Simple rate limiting based on client IP
    // For a more sophisticated implementation, we'd use a hash map
    // For now, just check if enough time has passed since last request
    static unsigned long last_request_time = 0;
    
    if (current_time - last_request_time < API_RATE_LIMIT_MS) {
        return true;
    }
    
    last_request_time = current_time;
    return false;
}

String WebServerManager::getClientIP(AsyncWebServerRequest* request) {
    return request->client()->remoteIP().toString();
}

void WebServerManager::logRequest(AsyncWebServerRequest* request, const String& endpoint) {
#if DEBUG_WEB_REQUESTS
    ESP_LOGI(TAG, "%s %s from %s", 
        request->methodToString(), 
        endpoint.c_str(), 
        getClientIP(request).c_str());
#endif
}

void WebServerManager::printServerInfo() {
    ESP_LOGI(TAG, "=== Web Server Information ===");
    ESP_LOGI(TAG, "Port: %d", WEB_SERVER_PORT);
    ESP_LOGI(TAG, "API Requests: %lu", api_request_count);
    ESP_LOGI(TAG, "WebSocket Messages: %lu", websocket_message_count);
    ESP_LOGI(TAG, "Errors: %lu", error_count);
    ESP_LOGI(TAG, "Connected WebSocket Clients: %d", websocket.count());
    ESP_LOGI(TAG, "==============================");
}

// Historical data handlers
void WebServerManager::handleGetHistoricalData(AsyncWebServerRequest* request) {
    logRequest(request, "/api/history");
    
    if (!history_manager) {
        request->send(503, "application/json", "{\"error\":\"Historical data not available\"}");
        return;
    }
    
    // Parse query parameters
    int channel_id = -1;
    int hours = 24;
    
    if (request->hasParam("channel")) {
        channel_id = request->getParam("channel")->value().toInt();
    }
    
    if (request->hasParam("hours")) {
        hours = request->getParam("hours")->value().toInt();
        if (hours < 1) hours = 1;
        if (hours > 168) hours = 168; // Max 1 week
    }
    
    JsonDocument doc = history_manager->getHistoricalData(channel_id, hours);
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetDataSummary(AsyncWebServerRequest* request) {
    logRequest(request, "/api/history/summary");
    
    if (!history_manager) {
        request->send(503, "application/json", "{\"error\":\"Historical data not available\"}");
        return;
    }
    
    JsonDocument doc = history_manager->getDataSummary();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetTrendAnalysis(AsyncWebServerRequest* request) {
    logRequest(request, "/api/history/trends");
    
    if (!history_manager) {
        request->send(503, "application/json", "{\"error\":\"Historical data not available\"}");
        return;
    }
    
    JsonDocument doc = history_manager->getTrendAnalysis();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleClearHistory(AsyncWebServerRequest* request) {
    logRequest(request, "/api/history/clear [POST]");
    
    if (!history_manager) {
        request->send(503, "application/json", "{\"error\":\"Historical data not available\"}");
        return;
    }
    
    // Parse retention days parameter
    int retention_days = 0; // 0 means clear all
    
    if (request->hasParam("retention_days", true)) {
        retention_days = request->getParam("retention_days", true)->value().toInt();
    }
    
    history_manager->clearOldData(retention_days);
    history_manager->saveToFile();
    
    ESP_LOGI(TAG, "Historical data cleared (retention: %d days)", retention_days);
    request->send(200, "application/json", "{\"status\":\"cleared\"}");
}