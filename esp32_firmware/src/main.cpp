#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "bmcu370_interface.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "historical_data.h"
#include "ota_manager.h"
#include "config.h"

// Global objects
BMCU370_Interface bmcu_interface;
WebServerManager web_server;
WiFiManager wifi_manager;
HistoricalDataManager history_manager;

// System status
unsigned long last_status_update = 0;
unsigned long last_heartbeat = 0;
unsigned long last_history_save = 0;
bool system_ready = false;
bool previous_connection_state = false; // Track previous BMCU370 connection state

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-S3 N4R2 BMCU370 Web Interface ===");
    Serial.println("Hardware: " + String(HARDWARE_VARIANT));
    Serial.println("Version: " + String(BMCU370_INTERFACE_VERSION));
    
    // Display ESP32-S3 N4R2 hardware information
    Serial.printf("ESP32-S3 Chip: %s\n", ESP.getChipModel());
    Serial.printf("Flash Size: %d MB (N%d variant)\n", ESP.getFlashChipSize() / (1024 * 1024), FLASH_SIZE_MB);
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    
#ifdef BOARD_HAS_PSRAM
    if (psramFound()) {
        Serial.printf("PSRAM Found: %d MB (%s mode)\n", ESP.getPsramSize() / (1024 * 1024), PSRAM_TYPE);
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
        #ifdef ESP32_S3_N4R2_VARIANT
        Serial.println("ESP32-S3 N4R2 variant detected - Enhanced buffer allocation enabled");
        #endif
    } else {
        Serial.printf("WARNING: PSRAM not found - expected %dMB %s PSRAM\n", PSRAM_SIZE_MB, PSRAM_TYPE);
    }
#endif
    
    // Initialize LittleFS filesystem with comprehensive error handling
    Serial.println("Initializing LittleFS filesystem...");
    Serial.printf("Expected partition: 0x310000-0x3E0000 (832KB)\n");
    
    bool littlefs_mounted = false;
    
    // Mount LittleFS without formatting
    Serial.println("Attempting to mount LittleFS...");
    littlefs_mounted = LittleFS.begin(false); // `false` = do not format if mount fails

    if (!littlefs_mounted) {
        Serial.println("ERROR: Failed to mount LittleFS partition.");
        Serial.println("The web interface files may be missing or the partition may be corrupt.");
        Serial.println("The system will continue in fallback mode.");
        Serial.println("To restore the web interface, re-flash the LittleFS binary.");
    }
    
    if (littlefs_mounted) {
        Serial.println("✅ LittleFS filesystem mounted successfully!");
        
        // Display comprehensive filesystem info
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        Serial.printf("📊 LittleFS Stats:\n");
        Serial.printf("   Total: %d bytes (%.1f KB)\n", totalBytes, totalBytes/1024.0);
        Serial.printf("   Used:  %d bytes (%.1f KB, %.1f%%)\n", 
                     usedBytes, usedBytes/1024.0, (float)usedBytes/totalBytes*100);
        Serial.printf("   Free:  %d bytes (%.1f KB)\n", 
                     totalBytes-usedBytes, (totalBytes-usedBytes)/1024.0);
        
        // List files for debugging
        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
            Serial.println("📁 LittleFS contents:");
            File file = root.openNextFile();
            int fileCount = 0;
            size_t totalFileSize = 0;
            while (file) {
                size_t fileSize = file.size();
                Serial.printf("   📄 %s (%d bytes)\n", file.name(), fileSize);
                totalFileSize += fileSize;
                file = root.openNextFile();
                fileCount++;
            }
            Serial.printf("   Total: %d files, %d bytes\n", fileCount, totalFileSize);
            
            if (fileCount == 0) {
                Serial.println("   ⚠️  No web files found - interface will run in fallback mode");
            }
        }
        
        // Test write capability
        File testFile = LittleFS.open("/test_write.txt", "w");
        if (testFile) {
            testFile.println("LittleFS write test");
            testFile.close();
            Serial.println("✅ LittleFS write test successful");
            LittleFS.remove("/test_write.txt");
        } else {
            Serial.println("⚠️  LittleFS write test failed - filesystem may be read-only");
        }
        
    } else {
        Serial.println("❌ CRITICAL: LittleFS mount failed after all attempts");
        Serial.println("\n🔧 Troubleshooting Steps:");
        Serial.println("1. Flash LittleFS partition:");
        Serial.println("   esptool.py --chip esp32s3 --port /dev/ttyUSB0 \\");
        Serial.println("   write_flash --flash_size 4MB 0x310000 littlefs.bin");
        Serial.println("2. Erase entire flash and reflash everything:");
        Serial.println("   esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash");
        Serial.println("3. Check partition table matches this firmware build");
        Serial.println("4. Verify ESP32-S3 N4R2 hardware (4MB flash required)");
        Serial.println("\n🌐 Running in enhanced fallback mode with full functionality");
    }
    
    // Initialize USB host interface
    if (!bmcu_interface.init()) {
        Serial.println("ERROR: Failed to initialize BMCU370 interface");
        // Continue anyway - might connect later
    } else {
        Serial.println("BMCU370 interface initialized");
    }
    
    // Initialize historical data manager
    if (!history_manager.init()) {
        Serial.println("ERROR: Failed to initialize historical data manager");
    } else {
        Serial.println("Historical data manager initialized");
    }
    
    // Initialize OTA manager
    if (!ota_manager.init()) {
        Serial.println("ERROR: Failed to initialize OTA manager");
    } else {
        Serial.println("OTA manager initialized");
    }
    
    // Initialize WiFi manager
    wifi_manager.init();
    
    // Wait for WiFi connection or start AP mode
    if (!wifi_manager.connectOrStartAP()) {
        Serial.println("WiFi connection failed, starting in AP mode");
    }
    
    // Start OTA after WiFi is connected
    ota_manager.begin();
    
    // Initialize web server (pass LittleFS status)
    web_server.init(&bmcu_interface, &history_manager, littlefs_mounted);
    
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

    // Handle USB host events
    bmcu_interface.handleUSB();
    
    // Handle WiFi manager
    wifi_manager.handle();
    
    // Handle OTA updates
    ota_manager.handle();
    
    // Update BMCU370 status periodically
    if (system_ready && (current_time - last_status_update >= STATUS_UPDATE_INTERVAL_MS)) {
        bool connected = bmcu_interface.updateStatus();
        if (connected != previous_connection_state) {
            Serial.print("BMCU370 connection status changed: ");
            Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
            previous_connection_state = connected;
        }
        
        // Add data to historical tracking if connected
        if (connected) {
            JsonDocument status = bmcu_interface.getStatus();
            if (status["channels"].is<JsonArray>()) {
                JsonArrayConst channels = status["channels"].as<JsonArrayConst>();
                int channel_id = 0;
                for (JsonObjectConst channel : channels) {
                    history_manager.addDataPoint(channel_id, channel);
                    channel_id++;
                }
            }
            
            // Add system data
            if (status["system"].is<JsonObject>()) {
                JsonObjectConst system = status["system"].as<JsonObjectConst>();
                history_manager.addSystemDataPoint(system);
            }
        }
        
        last_status_update = current_time;
    }
    
    // Save historical data periodically
    if (current_time - last_history_save >= 300000) { // 5 minutes
        history_manager.saveToFile();
        last_history_save = current_time;
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