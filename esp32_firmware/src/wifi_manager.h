#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <Preferences.h>
#include "config.h"

class WiFiManager {
private:
    Preferences prefs;
    String saved_ssid;
    String saved_password;
    bool ap_mode_active;
    bool config_mode_active;
    unsigned long config_mode_start_time;
    unsigned long last_connection_attempt;
    
    // Connection status
    bool wifi_connected;
    bool credentials_saved;
    
    // Internal methods
    bool loadCredentials();
    bool saveCredentials(const String& ssid, const String& password);
    bool connectToSavedNetwork();
    bool startAccessPoint();
    void stopAccessPoint();
    bool attemptConnection(const String& ssid, const String& password, uint32_t timeout_ms);
    
public:
    WiFiManager();
    ~WiFiManager();
    
    // Initialization and main control
    bool init();
    void handle();
    
    // Connection management
    bool connectOrStartAP();
    bool connectToNetwork(const String& ssid, const String& password);
    bool isConnected() const { return wifi_connected; }
    bool isAPMode() const { return ap_mode_active; }
    bool isAPActive() const { return ap_mode_active; }
    bool isConfigMode() const { return config_mode_active; }
    
    // Configuration
    void startConfigMode(uint32_t timeout_ms = WIFI_PORTAL_TIMEOUT_MS);
    void stopConfigMode();
    bool hasCredentials() const { return credentials_saved; }
    
    // Network information
    String getSSID() const;
    String getIPAddress() const;
    String getAPIP() const;
    int getSignalStrength() const;
    
    // Utility methods
    void scanNetworks();
    void printNetworkInfo();
    void resetCredentials();
};

#endif // WIFI_MANAGER_H