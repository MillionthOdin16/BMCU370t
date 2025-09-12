#include "wifi_manager.h"
#include <esp_log.h>

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager() 
    : ap_mode_active(false), config_mode_active(false), 
      config_mode_start_time(0), last_connection_attempt(0),
      wifi_connected(false), credentials_saved(false) {
}

WiFiManager::~WiFiManager() {
    prefs.end();
}

bool WiFiManager::init() {
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    // Initialize preferences
    if (!prefs.begin("wifi_config", false)) {
        ESP_LOGE(TAG, "Failed to initialize preferences");
        return false;
    }
    
    // Load saved credentials
    credentials_saved = loadCredentials();
    
    // Set WiFi mode
    WiFi.mode(WIFI_AP_STA);
    
    ESP_LOGI(TAG, "WiFi Manager initialized");
    if (credentials_saved) {
        ESP_LOGI(TAG, "Saved credentials found for SSID: %s", saved_ssid.c_str());
    } else {
        ESP_LOGI(TAG, "No saved credentials found");
    }
    
    return true;
}

void WiFiManager::handle() {
    unsigned long current_time = millis();
    
    // Check WiFi connection status
    bool currently_connected = (WiFi.status() == WL_CONNECTED);
    if (currently_connected != wifi_connected) {
        wifi_connected = currently_connected;
        if (wifi_connected) {
            ESP_LOGI(TAG, "WiFi connected to %s", WiFi.SSID().c_str());
            ESP_LOGI(TAG, "IP address: %s", WiFi.localIP().toString().c_str());
            
            // Stop AP mode if we successfully connected
            if (ap_mode_active) {
                stopAccessPoint();
            }
        } else {
            ESP_LOGW(TAG, "WiFi disconnected");
        }
    }
    
    // Handle config mode timeout
    if (config_mode_active && current_time - config_mode_start_time > WIFI_PORTAL_TIMEOUT_MS) {
        ESP_LOGI(TAG, "Config mode timeout, stopping");
        stopConfigMode();
    }
    
    // Attempt to reconnect if disconnected and we have credentials
    if (!wifi_connected && !ap_mode_active && credentials_saved) {
        if (current_time - last_connection_attempt > 30000) { // Try every 30 seconds
            ESP_LOGI(TAG, "Attempting to reconnect to saved network");
            connectToSavedNetwork();
            last_connection_attempt = current_time;
        }
    }
}

bool WiFiManager::connectOrStartAP() {
    ESP_LOGI(TAG, "Attempting WiFi connection or starting AP");
    
    // Try to connect to saved network first
    if (credentials_saved && connectToSavedNetwork()) {
        return true;
    }
    
    // If no saved credentials or connection failed, start AP mode
    ESP_LOGI(TAG, "Starting access point mode");
    return startAccessPoint();
}

bool WiFiManager::connectToNetwork(const String& ssid, const String& password) {
    ESP_LOGI(TAG, "Connecting to network: %s", ssid.c_str());
    
    if (attemptConnection(ssid, password, WIFI_CONNECT_TIMEOUT_MS)) {
        // Save credentials on successful connection
        saveCredentials(ssid, password);
        saved_ssid = ssid;
        saved_password = password;
        credentials_saved = true;
        
        ESP_LOGI(TAG, "Successfully connected and saved credentials");
        return true;
    }
    
    ESP_LOGE(TAG, "Failed to connect to network: %s", ssid.c_str());
    return false;
}

void WiFiManager::startConfigMode(uint32_t timeout_ms) {
    ESP_LOGI(TAG, "Starting configuration mode");
    
    config_mode_active = true;
    config_mode_start_time = millis();
    
    // Start AP mode if not already active
    if (!ap_mode_active) {
        startAccessPoint();
    }
    
    ESP_LOGI(TAG, "Configuration mode started (timeout: %lu ms)", timeout_ms);
}

void WiFiManager::stopConfigMode() {
    if (config_mode_active) {
        ESP_LOGI(TAG, "Stopping configuration mode");
        config_mode_active = false;
        
        // Try to connect to saved network
        if (credentials_saved && !wifi_connected) {
            connectToSavedNetwork();
        }
    }
}

bool WiFiManager::loadCredentials() {
    saved_ssid = prefs.getString("ssid", "");
    saved_password = prefs.getString("password", "");
    
    return !saved_ssid.isEmpty() && !saved_password.isEmpty();
}

bool WiFiManager::saveCredentials(const String& ssid, const String& password) {
    if (prefs.putString("ssid", ssid) && prefs.putString("password", password)) {
        ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid.c_str());
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to save credentials");
        return false;
    }
}

bool WiFiManager::connectToSavedNetwork() {
    if (!credentials_saved) {
        return false;
    }
    
    return attemptConnection(saved_ssid, saved_password, WIFI_CONNECT_TIMEOUT_MS);
}

bool WiFiManager::startAccessPoint() {
    ESP_LOGI(TAG, "Starting access point: %s", WIFI_AP_SSID);
    
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    ap_mode_active = true;
    
    IPAddress ap_ip = WiFi.softAPIP();
    ESP_LOGI(TAG, "Access point started");
    ESP_LOGI(TAG, "AP IP address: %s", ap_ip.toString().c_str());
    ESP_LOGI(TAG, "AP SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "AP Password: %s", WIFI_AP_PASSWORD);
    
    return true;
}

void WiFiManager::stopAccessPoint() {
    if (ap_mode_active) {
        ESP_LOGI(TAG, "Stopping access point");
        WiFi.softAPdisconnect(true);
        ap_mode_active = false;
    }
}

bool WiFiManager::attemptConnection(const String& ssid, const String& password, uint32_t timeout_ms) {
    ESP_LOGI(TAG, "Attempting connection to: %s", ssid.c_str());
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long start_time = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_time < timeout_ms) {
        delay(500);
        ESP_LOGI(TAG, "Connecting...");
    }
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        ESP_LOGI(TAG, "Connected to %s", ssid.c_str());
        ESP_LOGI(TAG, "IP address: %s", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGE(TAG, "Connection timeout to %s", ssid.c_str());
    }
    
    return connected;
}

String WiFiManager::getSSID() const {
    if (wifi_connected) {
        return WiFi.SSID();
    }
    return "";
}

String WiFiManager::getIPAddress() const {
    if (wifi_connected) {
        return WiFi.localIP().toString();
    }
    return "";
}

String WiFiManager::getAPIP() const {
    if (ap_mode_active) {
        return WiFi.softAPIP().toString();
    }
    return "";
}

int WiFiManager::getSignalStrength() const {
    if (wifi_connected) {
        return WiFi.RSSI();
    }
    return 0;
}

void WiFiManager::scanNetworks() {
    ESP_LOGI(TAG, "Scanning for networks...");
    
    int n = WiFi.scanNetworks();
    if (n == 0) {
        ESP_LOGI(TAG, "No networks found");
    } else {
        ESP_LOGI(TAG, "Found %d networks:", n);
        for (int i = 0; i < n; ++i) {
            ESP_LOGI(TAG, "%d: %s (%d dBm) %s", 
                i + 1, 
                WiFi.SSID(i).c_str(), 
                WiFi.RSSI(i),
                WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
        }
    }
    
    WiFi.scanDelete();
}

void WiFiManager::printNetworkInfo() {
    ESP_LOGI(TAG, "=== WiFi Network Information ===");
    ESP_LOGI(TAG, "WiFi Status: %s", wifi_connected ? "Connected" : "Disconnected");
    
    if (wifi_connected) {
        ESP_LOGI(TAG, "SSID: %s", WiFi.SSID().c_str());
        ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "Signal Strength: %d dBm", WiFi.RSSI());
        ESP_LOGI(TAG, "Gateway: %s", WiFi.gatewayIP().toString().c_str());
        ESP_LOGI(TAG, "DNS: %s", WiFi.dnsIP().toString().c_str());
    }
    
    if (ap_mode_active) {
        ESP_LOGI(TAG, "AP Mode: Active");
        ESP_LOGI(TAG, "AP SSID: %s", WIFI_AP_SSID);
        ESP_LOGI(TAG, "AP IP: %s", WiFi.softAPIP().toString().c_str());
        ESP_LOGI(TAG, "Connected Clients: %d", WiFi.softAPgetStationNum());
    }
    
    ESP_LOGI(TAG, "Config Mode: %s", config_mode_active ? "Active" : "Inactive");
    ESP_LOGI(TAG, "Saved Credentials: %s", credentials_saved ? "Yes" : "No");
    ESP_LOGI(TAG, "===============================");
}

void WiFiManager::resetCredentials() {
    ESP_LOGI(TAG, "Resetting WiFi credentials");
    
    prefs.remove("ssid");
    prefs.remove("password");
    
    saved_ssid = "";
    saved_password = "";
    credentials_saved = false;
    
    // Disconnect if connected
    if (wifi_connected) {
        WiFi.disconnect();
    }
    
    ESP_LOGI(TAG, "WiFi credentials reset");
}