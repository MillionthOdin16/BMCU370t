#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "bmcu370_interface.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "config.h"

// Global objects
BMCU370_Interface bmcu_interface;
WebServerManager web_server;
WiFiManager wifi_manager;

// System status
unsigned long last_status_update = 0;
unsigned long last_heartbeat = 0;
bool system_ready = false;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 BMCU370 Web Interface ===");
    Serial.println("Version: " BMCU370_INTERFACE_VERSION);
    
    // Initialize file system for web interface
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: Failed to initialize file system");
        return;
    }
    Serial.println("File system initialized");
    
    // Initialize USB host interface
    if (!bmcu_interface.init()) {
        Serial.println("ERROR: Failed to initialize BMCU370 interface");
        // Continue anyway - might connect later
    } else {
        Serial.println("BMCU370 interface initialized");
    }
    
    // Initialize WiFi manager
    wifi_manager.init();
    
    // Wait for WiFi connection or start AP mode
    if (!wifi_manager.connectOrStartAP()) {
        Serial.println("WiFi connection failed, starting in AP mode");
    }
    
    // Initialize web server
    web_server.init(&bmcu_interface);
    
    Serial.println("=== Initialization Complete ===");
    Serial.print("WiFi Status: ");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to ");
        Serial.println(WiFi.SSID());
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("AP Mode");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    }
    
    system_ready = true;
}

void loop() {
    unsigned long current_time = millis();
    
    // Handle WiFi manager
    wifi_manager.handle();
    
    // Update BMCU370 status periodically
    if (system_ready && (current_time - last_status_update >= STATUS_UPDATE_INTERVAL_MS)) {
        bool connected = bmcu_interface.updateStatus();
        if (connected != bmcu_interface.wasConnectedLastUpdate()) {
            Serial.print("BMCU370 connection status changed: ");
            Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
        }
        last_status_update = current_time;
    }
    
    // Heartbeat
    if (current_time - last_heartbeat >= 30000) { // 30 seconds
        Serial.print("Heartbeat - WiFi: ");
        Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
        Serial.print(", BMCU370: ");
        Serial.print(bmcu_interface.isConnected() ? "OK" : "FAIL");
        Serial.print(", Free Heap: ");
        Serial.println(ESP.getFreeHeap());
        last_heartbeat = current_time;
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}