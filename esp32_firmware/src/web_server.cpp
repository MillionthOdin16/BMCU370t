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
      littlefs_available(false), last_websocket_update(0), last_error_log(0), consecutive_errors(0),
      api_request_count(0), websocket_message_count(0), error_count(0), wifi_scan_requested(false) {
    
    // The client_last_call map for rate limiting is default-initialized.
}

WebServerManager::~WebServerManager() {
}

bool WebServerManager::init(BMCU370_Interface* interface, HistoricalDataManager* history, bool littlefs_mounted) {
    if (!interface) {
        ESP_LOGE(TAG, "BMCU370 interface is null");
        return false;
    }
    
    bmcu_interface = interface;
    history_manager = history;
    littlefs_available = littlefs_mounted;
    
    ESP_LOGI(TAG, "Initializing web server on port %d", WEB_SERVER_PORT);
    ESP_LOGI(TAG, "LittleFS available: %s", littlefs_available ? "Yes" : "No - using fallback mode");
    
    // Setup routes and handlers
    setupRoutes();
    setupWebSocket();
    
    // Setup static files or fallback interface
    if (littlefs_available) {
        setupStaticFiles();
    } else {
        setupFallbackInterface();
    }
    
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
    
    // Handle WiFi scan results
    if (wifi_scan_requested) {
        int16_t scanResult = WiFi.scanComplete();
        if (scanResult >= 0) {
            ESP_LOGI(TAG, "WiFi scan completed with %d networks.", scanResult);
            JsonDocument doc;
            doc["type"] = "wifi_scan_result";
            JsonArray networks = doc.createNestedArray("networks");

            if (scanResult > 0) {
                for (int i = 0; i < scanResult; ++i) {
                    JsonObject network = networks.createNestedObject();
                    network["ssid"] = WiFi.SSID(i);
                    network["rssi"] = WiFi.RSSI(i);
                    network["encryption"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted";
                }
            }

            String response;
            serializeJson(doc, response);
            websocket.textAll(response);

            WiFi.scanDelete();
            wifi_scan_requested = false;
        }
        // if scan is still running, do nothing and wait for next handle() call
    }

    // Broadcast OTA progress
    if (ota_manager.isActive()) {
        JsonDocument doc;
        doc["type"] = "ota_progress";
        doc["progress"] = ota_manager.getProgress();
        doc["state"] = (int)ota_manager.getState();
        doc["error"] = ota_manager.getLastError();

        String response;
        serializeJson(doc, response);
        websocket.textAll(response);
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
    ESP_LOGI(TAG, "Setting up static file serving from LittleFS");
    
    if (!littlefs_available) {
        ESP_LOGE(TAG, "Cannot setup static files - LittleFS not available");
        return;
    }
    
    // Serve static files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // Set cache headers for static assets
    server.serveStatic("/css/", LittleFS, "/css/").setCacheControl("max-age=3600");
    server.serveStatic("/js/", LittleFS, "/js/").setCacheControl("max-age=3600");
    server.serveStatic("/img/", LittleFS, "/img/").setCacheControl("max-age=86400");
    
    ESP_LOGI(TAG, "Static file serving configured");
}

void WebServerManager::setupFallbackInterface() {
    ESP_LOGI(TAG, "Setting up enhanced fallback web interface (no LittleFS)");
    
    // Serve a comprehensive HTML page from program memory with full WiFi setup
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<!DOCTYPE html><html><head><title>BMCU370 Interface - Setup Mode</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css'>";
        html += "<style>";
        html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
        html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; ";
        html += "background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }";
        html += ".container { max-width: 800px; margin: 0 auto; }";
        html += ".card { background: white; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); ";
        html += "margin-bottom: 20px; overflow: hidden; }";
        html += ".header { background: linear-gradient(45deg, #2196F3, #21CBF3); color: white; padding: 30px; text-align: center; }";
        html += ".header h1 { font-size: 2em; margin-bottom: 10px; }";
        html += ".header .subtitle { opacity: 0.9; font-size: 1.1em; }";
        html += ".status { padding: 15px; margin: 15px; border-radius: 8px; font-weight: 500; }";
        html += ".status.warning { background: #f8d7da; border: 1px solid #f5c6cb; color: #721c24; }"; // Red for critical
        html += ".status.info { background: #d1ecf1; border: 1px solid #bee5eb; color: #0c5460; }";
        html += ".status.success { background: #d4edda; border: 1px solid #c3e6cb; color: #155724; }";
        html += ".section { padding: 25px; border-bottom: 1px solid #eee; }";
        html += ".section:last-child { border-bottom: none; }";
        html += ".section h3 { color: #333; margin-bottom: 15px; font-size: 1.3em; }";
        html += ".form-group { margin-bottom: 20px; }";
        html += ".form-group label { display: block; margin-bottom: 8px; font-weight: 600; color: #555; }";
        html += ".form-control { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 8px; ";
        html += "font-size: 16px; transition: border-color 0.3s; }";
        html += ".form-control:focus { outline: none; border-color: #2196F3; }";
        html += ".btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 16px; ";
        html += "font-weight: 600; cursor: pointer; transition: all 0.3s; margin: 5px; text-decoration: none; display: inline-block; }";
        html += ".btn-primary { background: #2196F3; color: white; }";
        html += ".btn-primary:hover { background: #1976D2; transform: translateY(-2px); }";
        html += ".btn-success { background: #4CAF50; color: white; }";
        html += ".btn-success:hover { background: #45a049; transform: translateY(-2px); }";
        html += ".btn-secondary { background: #6c757d; color: white; }";
        html += ".btn-secondary:hover { background: #5a6268; transform: translateY(-2px); }";
        html += ".networks-list { max-height: 300px; overflow-y: auto; border: 2px solid #eee; border-radius: 8px; margin-top: 10px; }";
        html += ".network-item { padding: 15px; border-bottom: 1px solid #eee; cursor: pointer; transition: background 0.3s; }";
        html += ".network-item:hover { background: #f8f9fa; }";
        html += ".network-item:last-child { border-bottom: none; }";
        html += ".network-info { display: flex; justify-content: space-between; align-items: center; }";
        html += ".network-name { font-weight: 600; color: #333; }";
        html += ".network-signal { color: #666; font-size: 0.9em; }";
        html += ".network-security { background: #e9ecef; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }";
        html += ".no-networks { text-align: center; padding: 30px; color: #666; }";
        html += ".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }";
        html += ".info-item { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #eee; }";
        html += ".info-item:last-child { border-bottom: none; }";
        html += ".info-item label { font-weight: 600; color: #555; }";
        html += ".status-badge { padding: 4px 12px; border-radius: 20px; font-size: 0.9em; font-weight: 600; }";
        html += ".status-connected { background: #d4edda; color: #155724; }";
        html += ".status-disconnected { background: #f8d7da; color: #721c24; }";
        html += ".help-text { background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 15px 0; color: #6c757d; }";
        html += ".loading { display: none; text-align: center; padding: 20px; }";
        html += ".spinner { border: 3px solid #f3f3f3; border-top: 3px solid #2196F3; border-radius: 50%; ";
        html += "width: 30px; height: 30px; animation: spin 1s linear infinite; margin: 0 auto 10px; }";
        html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
        html += "pre { background: #f8f9fa; padding: 15px; border-radius: 8px; overflow-x: auto; font-size: 14px; margin-top: 10px; }";
        html += ".toast { position: fixed; top: 20px; right: 20px; background: #333; color: white; ";
        html += "padding: 15px 20px; border-radius: 8px; z-index: 1000; display: none; }";
        html += ".toast.success { background: #4CAF50; }";
        html += ".toast.error { background: #f44336; }";
        html += "</style></head><body>";
        
        html += "<div class='container'>";
        
        // Header
        html += "<div class='card'>";
        html += "<div class='header'>";
        html += "<h1><i class='fas fa-wifi'></i> BMCU370 WiFi Setup</h1>";
        html += "<div class='subtitle'>Connect your BMCU370 to your home network</div>";
        html += "</div>";
        
        html += "<div class='status warning'>";
        html += "<i class='fas fa-exclamation-triangle'></i> ";
        html += "<strong>CRITICAL:</strong> Main web interface not found! The device has started in a limited Fallback Mode. This usually means the LittleFS partition containing the web files is missing or corrupt.";
        html += "</div>";
        html += "</div>";
        
        // WiFi Status
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-signal'></i> Current Network Status</h3>";
        html += "<div class='grid'>";
        html += "<div class='info-item'><label>WiFi Status:</label><span id='wifi-status' class='status-badge'>Checking...</span></div>";
        html += "<div class='info-item'><label>Current Network:</label><span id='wifi-ssid'>--</span></div>";
        html += "<div class='info-item'><label>IP Address:</label><span id='wifi-ip'>--</span></div>";
        html += "<div class='info-item'><label>Signal Strength:</label><span id='wifi-signal'>--</span></div>";
        html += "</div>";
        html += "<div class='help-text'>";
        html += "<i class='fas fa-info-circle'></i> ";
        html += "You are currently connected to the <strong>BMCU370-Config</strong> setup network. ";
        html += "Use the form below to connect to your home WiFi network.";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Network Scanner
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-search'></i> Available Networks</h3>";
        html += "<p>Scan for available WiFi networks and click on one to select it:</p>";
        html += "<button class='btn btn-primary' onclick='scanNetworks()'>";
        html += "<i class='fas fa-search'></i> Scan for Networks";
        html += "</button>";
        html += "<div class='loading' id='scan-loading'>";
        html += "<div class='spinner'></div>";
        html += "<p>Scanning for networks...</p>";
        html += "</div>";
        html += "<div class='networks-list' id='networks-list'>";
        html += "<div class='no-networks'>Click \"Scan for Networks\" to see available WiFi networks</div>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Connection Form
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-link'></i> Connect to WiFi Network</h3>";
        html += "<p>Enter your WiFi network credentials:</p>";
        html += "<form id='wifi-form' onsubmit='connectWiFi(event)'>";
        html += "<div class='form-group'>";
        html += "<label for='ssid'>Network Name (SSID):</label>";
        html += "<input type='text' id='ssid' class='form-control' placeholder='Enter network name or select from scan results' required>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label for='password'>Password:</label>";
        html += "<input type='password' id='password' class='form-control' placeholder='Enter WiFi password (leave blank for open networks)'>";
        html += "</div>";
        html += "<button type='submit' class='btn btn-success'>";
        html += "<i class='fas fa-wifi'></i> Connect to Network";
        html += "</button>";
        html += "<div class='loading' id='connect-loading'>";
        html += "<div class='spinner'></div>";
        html += "<p>Connecting to network...</p>";
        html += "</div>";
        html += "</form>";
        html += "<div class='help-text'>";
        html += "<i class='fas fa-lightbulb'></i> ";
        html += "After connecting successfully, the device will remember your network and connect automatically in the future.";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // System Information
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-microchip'></i> System Information</h3>";
        html += "<div class='grid'>";
        html += "<div class='info-item'><label>BMCU370 Status:</label><span id='bmcu-status' class='status-badge'>Checking...</span></div>";
        html += "<div class='info-item'><label>Device Type:</label><span>ESP32-S3 N4R2</span></div>";
        html += "<div class='info-item'><label>Interface Version:</label><span>1.0.0</span></div>";
        html += "<div class='info-item'><label>Config Mode IP:</label><span>192.168.4.1</span></div>";
        html += "</div>";
        html += "<button class='btn btn-secondary' onclick='checkBMCU()'>";
        html += "<i class='fas fa-sync'></i> Check BMCU370 Connection";
        html += "</button>";
        html += "</div>";
        html += "</div>";
        
        // OTA Firmware Update
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-upload'></i> Firmware Update</h3>";
        html += "<div class='status info'>";
        html += "<strong>OTA Status:</strong> <span id='ota-status'>Checking...</span>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label for='firmware-file'>Select Firmware File (.bin):</label>";
        html += "<input type='file' id='firmware-file' accept='.bin' class='form-control'>";
        html += "</div>";
        html += "<div id='file-info' style='display: none; margin: 10px 0; padding: 10px; background: #e8f5e8; border-radius: 5px;'>";
        html += "<span id='file-name'></span> (<span id='file-size'></span>)";
        html += "</div>";
        html += "<div class='upload-controls'>";
        html += "<button class='btn btn-success' id='upload-btn' onclick='uploadFirmware()' disabled>";
        html += "<i class='fas fa-upload'></i> Upload Firmware";
        html += "</button>";
        html += "<button class='btn btn-secondary' id='abort-btn' onclick='abortUpload()' style='display: none;'>";
        html += "<i class='fas fa-stop'></i> Abort";
        html += "</button>";
        html += "</div>";
        html += "<div id='upload-progress' style='display: none; margin-top: 15px;'>";
        html += "<div style='background: #ecf0f1; border-radius: 10px; overflow: hidden; height: 20px; margin-bottom: 10px;'>";
        html += "<div id='progress-bar' style='height: 100%; background: linear-gradient(45deg, #3498db, #2980b9); width: 0%; transition: width 0.3s;'></div>";
        html += "</div>";
        html += "<div style='display: flex; justify-content: space-between;'>";
        html += "<span id='progress-percent'>0%</span>";
        html += "<span id='progress-status'>Ready</span>";
        html += "</div>";
        html += "</div>";
        html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-top: 10px; color: #856404;'>";
        html += "<i class='fas fa-exclamation-triangle'></i> ";
        html += "<strong>Warning:</strong> Do not power off during firmware update!";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Troubleshooting
        html += "<div class='card'>";
        html += "<div class='section'>";
        html += "<h3><i class='fas fa-tools'></i> Troubleshooting</h3>";
        html += "<div class='status info'>";
        html += "<strong>Action Required:</strong> To restore the full web interface, you must flash the LittleFS partition. This will not affect your settings.";
        html += "</div>";
        html += "<p><strong>To fix LittleFS:</strong></p>";
        html += "<pre>esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \\<br>";
        html += "  write_flash --flash_size 4MB 0x310000 littlefs.bin</pre>";
        html += "<p><strong>API Endpoints (for advanced users):</strong></p>";
        html += "<ul>";
        html += "<li><a href='/api/status' target='_blank'>GET /api/status</a> - System status</li>";
        html += "<li><a href='/api/wifi/status' target='_blank'>GET /api/wifi/status</a> - WiFi status</li>";
        html += "<li><a href='/api/wifi/scan' target='_blank'>GET /api/wifi/scan</a> - Network scan</li>";
        html += "</ul>";
        html += "</div>";
        html += "</div>";
        
        html += "</div>";
        
        // Toast notification
        html += "<div class='toast' id='toast'><span id='toast-message'></span></div>";
        
        // JavaScript for functionality
        html += "<script>";
        html += "let scanTimeout = null;";
        html += "function showToast(message, type = 'info') {";
        html += "  const toast = document.getElementById('toast');";
        html += "  const msg = document.getElementById('toast-message');";
        html += "  msg.textContent = message;";
        html += "  toast.className = 'toast ' + type;";
        html += "  toast.style.display = 'block';";
        html += "  setTimeout(() => { toast.style.display = 'none'; }, 5000);";
        html += "}";
        
        html += "function updateStatus() {";
        html += "  fetch('/api/wifi/status')";
        html += "    .then(r => r.json())";
        html += "    .then(d => {";
        html += "      const status = d.connected ? 'Connected' : (d.ap_active ? 'Config Mode' : 'Disconnected');";
        html += "      const statusClass = d.connected ? 'status-connected' : 'status-disconnected';";
        html += "      document.getElementById('wifi-status').textContent = status;";
        html += "      document.getElementById('wifi-status').className = 'status-badge ' + statusClass;";
        html += "      document.getElementById('wifi-ssid').textContent = d.ssid || 'BMCU370-Config';";
        html += "      document.getElementById('wifi-ip').textContent = d.ip || d.ap_ip || '--';";
        html += "      document.getElementById('wifi-signal').textContent = d.signal ? d.signal + ' dBm' : '--';";
        html += "    })";
        html += "    .catch(() => {";
        html += "      document.getElementById('wifi-status').textContent = 'Error';";
        html += "      document.getElementById('wifi-status').className = 'status-badge status-disconnected';";
        html += "    });";
        
        html += "  fetch('/api/status')";
        html += "    .then(r => r.json())";
        html += "    .then(d => {";
        html += "      const bmcuStatus = d.connected ? 'Connected' : 'Disconnected';";
        html += "      const statusClass = d.connected ? 'status-connected' : 'status-disconnected';";
        html += "      document.getElementById('bmcu-status').textContent = bmcuStatus;";
        html += "      document.getElementById('bmcu-status').className = 'status-badge ' + statusClass;";
        html += "    })";
        html += "    .catch(() => {";
        html += "      document.getElementById('bmcu-status').textContent = 'No Response';";
        html += "      document.getElementById('bmcu-status').className = 'status-badge status-disconnected';";
        html += "    });";
        html += "}";
        
        html += "function scanNetworks() {";
        html += "  const loading = document.getElementById('scan-loading');";
        html += "  const list = document.getElementById('networks-list');";
        html += "  loading.style.display = 'block';";
        html += "  list.innerHTML = '';";
        
        html += "  function pollScanResults() {";
        html += "    fetch('/api/wifi/scan')";
        html += "      .then(r => r.json())";
        html += "      .then(d => {";
        html += "        if (d.status === 'scanning' || d.status === 'started') {";
        html += "          setTimeout(pollScanResults, 2000);"; // Check again in 2 seconds
        html += "        } else if (d.status === 'complete' || d.networks) {";
        html += "          loading.style.display = 'none';";
        html += "          if (d.networks && d.networks.length > 0) {";
        html += "            list.innerHTML = d.networks.map(n => {";
        html += "              return '<div class=\"network-item\" onclick=\"selectNetwork(\\'' + n.ssid + '\\')\">' +";
        html += "                '<div class=\"network-info\">' +";
        html += "                  '<div><div class=\"network-name\">' + n.ssid + '</div>' +";
        html += "                  '<div class=\"network-security\">' + (n.encryption || 'Open') + '</div></div>' +";
        html += "                  '<div class=\"network-signal\">' + n.rssi + ' dBm</div>' +";
        html += "                '</div>' +";
        html += "              '</div>';";
        html += "            }).join('');";
        html += "            showToast(`Found ${d.networks.length} networks`, 'success');";
        html += "          } else {";
        html += "            list.innerHTML = '<div class=\"no-networks\">No networks found</div>';";
        html += "            showToast('No networks found', 'warning');";
        html += "          }";
        html += "        } else {";
        html += "          loading.style.display = 'none';";
        html += "          list.innerHTML = '<div class=\"no-networks\">Scan failed</div>';";
        html += "          showToast('Network scan failed', 'error');";
        html += "        }";
        html += "      })";
        html += "      .catch(err => {";
        html += "        loading.style.display = 'none';";
        html += "        list.innerHTML = '<div class=\"no-networks\">Scan failed</div>';";
        html += "        showToast('Network scan failed', 'error');";
        html += "      });";
        html += "  }";
        
        html += "  pollScanResults();"; // Start polling
        html += "}";
        
        html += "function selectNetwork(ssid) {";
        html += "  document.getElementById('ssid').value = ssid;";
        html += "  showToast(`Selected network: ${ssid}`, 'success');";
        html += "}";
        
        html += "function connectWiFi(event) {";
        html += "  event.preventDefault();";
        html += "  const ssid = document.getElementById('ssid').value;";
        html += "  const password = document.getElementById('password').value;";
        html += "  const loading = document.getElementById('connect-loading');";
        
        html += "  if (!ssid) {";
        html += "    showToast('Please enter a network name', 'error');";
        html += "    return;";
        html += "  }";
        
        html += "  loading.style.display = 'block';";
        html += "  const formData = new FormData();";
        html += "  formData.append('ssid', ssid);";
        html += "  formData.append('password', password);";
        
        html += "  fetch('/api/wifi/connect', { method: 'POST', body: new URLSearchParams(formData) })";
        html += "    .then(r => r.json())";
        html += "    .then(d => {";
        html += "      loading.style.display = 'none';";
        html += "      if (d.success) {";
        html += "        showToast('Connected successfully! Redirecting...', 'success');";
        html += "        setTimeout(() => {";
        html += "          if (d.ip) window.location.href = `http://${d.ip}`;";
        html += "          else updateStatus();";
        html += "        }, 3000);";
        html += "      } else {";
        html += "        showToast(`Connection failed: ${d.error || 'Unknown error'}`, 'error');";
        html += "      }";
        html += "    })";
        html += "    .catch(err => {";
        html += "      loading.style.display = 'none';";
        html += "      showToast('Connection request failed', 'error');";
        html += "    });";
        html += "}";
        
        html += "function checkBMCU() {";
        html += "  document.getElementById('bmcu-status').textContent = 'Checking...';";
        html += "  updateStatus();";
        html += "}";
        
        // OTA functionality
        html += "let selectedFile = null;";
        html += "let uploadInProgress = false;";
        
        html += "document.getElementById('firmware-file').addEventListener('change', function(e) {";
        html += "  const file = e.target.files[0];";
        html += "  if (file) {";
        html += "    if (!file.name.endsWith('.bin')) {";
        html += "      showToast('Please select a .bin firmware file', 'error');";
        html += "      return;";
        html += "    }";
        html += "    if (file.size > 2 * 1024 * 1024) {";
        html += "      showToast('File too large. Maximum size is 2MB', 'error');";
        html += "      return;";
        html += "    }";
        html += "    selectedFile = file;";
        html += "    document.getElementById('file-name').textContent = file.name;";
        html += "    document.getElementById('file-size').textContent = (file.size / 1024).toFixed(1) + ' KB';";
        html += "    document.getElementById('file-info').style.display = 'block';";
        html += "    document.getElementById('upload-btn').disabled = false;";
        html += "    showToast('Firmware file selected', 'success');";
        html += "  }";
        html += "});";
        
        html += "function uploadFirmware() {";
        html += "  if (!selectedFile || uploadInProgress) return;";
        html += "  uploadInProgress = true;";
        html += "  const formData = new FormData();";
        html += "  formData.append('update', selectedFile);";

        html += "  document.getElementById('upload-progress').style.display = 'block';";
        html += "  document.getElementById('upload-btn').style.display = 'none';";
        html += "  document.getElementById('abort-btn').style.display = 'inline-block';";
        html += "  updateProgress(0, 'Starting upload...');";
        
        html += "  fetch('/update', { method: 'POST', body: formData, onprogress: (e) => { if(e.lengthComputable) { updateProgress(Math.round((e.loaded/e.total)*100), 'Uploading...'); } } })";
        html += "    .then(r => r.json())";
        html += "    .then(d => {";
        html += "      if (d.status === 'ok' || d.success) {";
        html += "        updateProgress(100, 'Upload complete! Restarting...');";
        html += "        showToast('Firmware uploaded successfully! Device restarting.', 'success');";
        html += "        setTimeout(() => window.location.reload(), 10000);";
        html += "      } else {";
        html += "        throw new Error(d.error || 'Upload failed');";
        html += "      }";
        html += "    })";
        html += "    .catch(err => {";
        html += "      updateProgress(0, 'Upload failed');";
        html += "      showToast('Upload failed: ' + err.message, 'error');";
        html += "      resetUploadUI();";
        html += "    });";
        html += "}";
        
        html += "function abortUpload() {";
        html += "  fetch('/api/ota/abort', { method: 'POST' })";
        html += "    .then(() => {";
        html += "      showToast('Upload aborted', 'warning');";
        html += "      resetUploadUI();";
        html += "    })";
        html += "    .catch(err => console.error('Abort failed:', err));";
        html += "}";
        
        html += "function updateProgress(percent, status) {";
        html += "  document.getElementById('progress-bar').style.width = percent + '%';";
        html += "  document.getElementById('progress-percent').textContent = percent + '%';";
        html += "  document.getElementById('progress-status').textContent = status;";
        html += "}";
        
        html += "function resetUploadUI() {";
        html += "  uploadInProgress = false;";
        html += "  document.getElementById('upload-progress').style.display = 'none';";
        html += "  document.getElementById('upload-btn').style.display = 'inline-block';";
        html += "  document.getElementById('abort-btn').style.display = 'none';";
        html += "  updateProgress(0, 'Ready');";
        html += "}";
        
        html += "function updateOTAStatus() {";
        html += "  fetch('/api/ota/status')";
        html += "    .then(r => r.json())";
        html += "    .then(d => {";
        html += "      let status = 'Idle';";
        html += "      switch(parseInt(d.state)) {";
        html += "        case 1: status = 'Starting'; break;";
        html += "        case 2: status = 'In Progress (' + d.progress + '%)'; break;";
        html += "        case 3: status = 'Success'; break;";
        html += "        case 4: status = 'Error'; break;";
        html += "      }";
        html += "      document.getElementById('ota-status').textContent = status;";
        html += "    })";
        html += "    .catch(() => {";
        html += "      document.getElementById('ota-status').textContent = 'Unknown';";
        html += "    });";
        html += "}";
        
        html += "// Initialize";
        html += "updateStatus();";
        html += "updateOTAStatus();";
        html += "setInterval(updateStatus, 15000);";
        html += "setInterval(updateOTAStatus, 5000);";
        html += "</script>";
        html += "</body></html>";
        
        request->send(200, "text/html", html);
    });
    
    ESP_LOGI(TAG, "Enhanced fallback interface configured with full WiFi setup");
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

    // Always update status, which handles connected/disconnected states internally
    bmcu_interface->updateStatus();

    JsonDocument status = bmcu_interface->getStatus();

    // Add ESP32-specific info to the status object
    JsonObject system = status.is<JsonObject>() ? status["system"].as<JsonObject>() : status.createNestedObject("system");
    system["esp32_version"] = BMCU370_INTERFACE_VERSION;
    system["esp32_build_date"] = __DATE__ " " __TIME__;
    system["esp32_free_heap"] = ESP.getFreeHeap();
    system["esp32_flash_size"] = ESP.getFlashChipSize();

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
    
    // Input validation for action parameter
    if (action.length() == 0 || action.length() > 50) {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Invalid action parameter length\"}");
        return;
    }
    
    // Sanitize action - only allow alphanumeric and underscore
    for (int i = 0; i < action.length(); i++) {
        char c = action[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_')) {
            error_count++;
            request->send(400, "application/json", "{\"error\":\"Invalid characters in action parameter\"}");
            return;
        }
    }
    
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
        request->send(400, "application/json", "{\"error\":\"Unknown action: only reset_bmcu370, dfu_mode, and reset_esp32 are supported\"}");
    }
    
    api_request_count++;
}

void WebServerManager::handleWiFiScan(AsyncWebServerRequest* request) {
    logRequest(request, "/api/wifi/scan");
    
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING || wifi_scan_requested) {
        request->send(409, "application/json", "{\"error\":\"Scan already in progress\"}");
        return;
    }

    ESP_LOGI(TAG, "WiFi scan requested via HTTP, triggering WebSocket broadcast.");
    wifi_scan_requested = true;
    WiFi.scanNetworks(true, false, false, 300);
    
    request->send(202, "application/json", "{\"status\":\"scan_started\",\"message\":\"Scan started, result will be sent via WebSocket\"}");
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
    
    // Input validation
    if (ssid.length() == 0 || ssid.length() > 32) {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Invalid SSID length (1-32 characters required)\"}");
        return;
    }
    
    if (password.length() > 63) {
        error_count++;
        request->send(400, "application/json", "{\"error\":\"Password too long (maximum 63 characters)\"}");
        return;
    }
    
    // Sanitize input - remove any control characters
    for (int i = 0; i < ssid.length(); i++) {
        if (ssid[i] < 32 || ssid[i] == 127) {
            error_count++;
            request->send(400, "application/json", "{\"error\":\"Invalid characters in SSID\"}");
            return;
        }
    }
    
    ESP_LOGI(TAG, "WiFi connection request for SSID: %s", ssid.c_str());
    
    bool success = wifi_manager.connectToNetwork(ssid, password);
    if (success) {
        String json = "{\"success\":true,\"message\":\"Connected to WiFi\"";
        if (WiFi.status() == WL_CONNECTED) {
            json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
            json += ",\"ssid\":\"" + WiFi.SSID() + "\"";
        }
        json += "}";
        request->send(200, "application/json", json);
        ESP_LOGI(TAG, "WiFi connection successful to %s", ssid.c_str());
    } else {
        error_count++;
        String json = "{\"error\":\"Failed to connect to WiFi network";
        
        // Provide specific error details
        wl_status_t status = WiFi.status();
        switch (status) {
            case WL_CONNECT_FAILED:
                json += " - Wrong password or authentication failed";
                break;
            case WL_NO_SSID_AVAIL:
                json += " - Network not found";
                break;
            case WL_CONNECTION_LOST:
                json += " - Connection lost during handshake";
                break;
            default:
                json += " - Connection timeout or unknown error";
                break;
        }
        json += "\"}";
        
        request->send(400, "application/json", json);
        ESP_LOGE(TAG, "WiFi connection failed to %s (status: %d)", ssid.c_str(), status);
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
                JsonDocument doc;
                deserializeJson(doc, message);
                String command = doc["command"];

                if (command == "get_status") {
                    if (bmcu_interface && bmcu_interface->isConnected()) {
                        JsonDocument status = bmcu_interface->getStatus();
                        String response;
                        serializeJson(status, response);
                        client->text(response);
                    } else {
                        sendErrorToClient(client, "BMCU370 not connected");
                    }
                } else if (command == "start_wifi_scan") {
                    ESP_LOGI(TAG, "WiFi scan requested via WebSocket");
                    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
                        sendErrorToClient(client, "Scan already in progress");
                    } else {
                        wifi_scan_requested = true;
                        WiFi.scanNetworks(true, false, false, 300);
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
    status["type"] = "status"; // Add type for client-side handling

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

    // Check if the client has made a request before
    if (client_last_call.find(client_ip) != client_last_call.end()) {
        // Check if the last call was too recent
        if (current_time - client_last_call[client_ip] < API_RATE_LIMIT_MS) {
            return true; // Rate limited
        }
    }

    // Update the last call time for this client
    client_last_call[client_ip] = current_time;
    
    // Clean up old entries from the map to prevent it from growing indefinitely
    // Remove entries older than 10x the rate limit time
    if (client_last_call.size() > 50) { // Trigger cleanup when map size exceeds a threshold
        for (auto it = client_last_call.cbegin(); it != client_last_call.cend();) {
            if (current_time - it->second > (API_RATE_LIMIT_MS * 10)) {
                it = client_last_call.erase(it);
            } else {
                ++it;
            }
        }
    }

    return false; // Not rate limited
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