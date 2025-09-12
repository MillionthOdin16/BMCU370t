#include "historical_data.h"
#include "config.h"
#include <esp_log.h>

static const char* TAG = "HistoricalData";

HistoricalDataManager::HistoricalDataManager() 
    : last_sample_time(0), data_dirty(false) {
    data_buffer.reserve(MAX_HISTORY_ENTRIES);
}

bool HistoricalDataManager::init() {
    ESP_LOGI(TAG, "Initializing historical data manager");
    
    // Load existing data from file
    if (LittleFS.exists(HISTORY_FILE_PATH)) {
        if (loadFromFile()) {
            ESP_LOGI(TAG, "Loaded %d historical data points", data_buffer.size());
        } else {
            ESP_LOGW(TAG, "Failed to load historical data, starting fresh");
        }
    }
    
    // Clean up old data
    clearOldData();
    
    return true;
}

void HistoricalDataManager::addDataPoint(int channel_id, const JsonObjectConst& channel_data) {
    unsigned long now = millis();
    
    // Check if it's time for a new sample
    if (now - last_sample_time < HISTORY_SAMPLE_INTERVAL_MS) {
        return;
    }
    
    HistoricalDataPoint point;
    point.timestamp = now;
    point.channel_id = channel_id;
    
    // Extract sensor data
    if (channel_data["sensors"].is<JsonObjectConst>()) {
        JsonObjectConst sensors = channel_data["sensors"].as<JsonObjectConst>();
        point.hall_position = sensors["hall_position"].as<int>();
        point.filament_present = sensors["filament_present"].as<bool>();
    }
    
    // Extract motion data
    if (channel_data["motion"].is<JsonObjectConst>()) {
        JsonObjectConst motion = channel_data["motion"].as<JsonObjectConst>();
        point.motor_position = motion["position"].as<int>();
        point.motor_speed = motion["speed"].as<float>();
        point.pressure = motion["pressure"].as<int>();
    }
    
    // Extract LED data
    if (channel_data["rgb"].is<JsonObjectConst>()) {
        JsonObjectConst rgb = channel_data["rgb"].as<JsonObjectConst>();
        point.led_brightness = rgb["brightness"].as<int>();
    }
    
    // Add point to buffer
    data_buffer.push_back(point);
    data_dirty = true;
    last_sample_time = now;
    
    // Maintain buffer size
    maintainBufferSize();
    
    ESP_LOGD(TAG, "Added data point for channel %d (buffer size: %d)", 
             channel_id, data_buffer.size());
}

void HistoricalDataManager::addSystemDataPoint(const JsonObjectConst& system_data) {
    // Add system-level metrics
    HistoricalDataPoint point;
    point.timestamp = millis();
    point.channel_id = -1; // System-level data
    point.error_count = system_data["error_count"].as<int>();
    
    data_buffer.push_back(point);
    data_dirty = true;
    maintainBufferSize();
}

JsonDocument HistoricalDataManager::getHistoricalData(int channel_id, int hours) {
    JsonDocument doc;
    JsonArray data_array = doc["data"].to<JsonArray>();
    
    unsigned long cutoff_time = millis() - (hours * 3600000UL); // hours to milliseconds
    
    for (const auto& point : data_buffer) {
        if (point.timestamp < cutoff_time) continue;
        if (channel_id >= 0 && point.channel_id != channel_id) continue;
        
        JsonObject entry = data_array.add<JsonObject>();
        entry["timestamp"] = point.timestamp;
        entry["channel"] = point.channel_id;
        entry["hall_position"] = point.hall_position;
        entry["filament_present"] = point.filament_present;
        entry["motor_position"] = point.motor_position;
        entry["motor_speed"] = point.motor_speed;
        entry["pressure"] = point.pressure;
        entry["led_brightness"] = point.led_brightness;
        entry["voltage"] = point.voltage;
        entry["error_count"] = point.error_count;
    }
    
    doc["channel_id"] = channel_id;
    doc["hours"] = hours;
    doc["total_points"] = data_array.size();
    
    return doc;
}

JsonDocument HistoricalDataManager::getDataSummary() {
    JsonDocument doc;
    
    doc["total_points"] = data_buffer.size();
    doc["oldest_timestamp"] = getOldestTimestamp();
    doc["latest_timestamp"] = getLatestTimestamp();
    doc["sample_interval_ms"] = HISTORY_SAMPLE_INTERVAL_MS;
    doc["retention_days"] = HISTORY_RETENTION_DAYS;
    doc["max_entries"] = MAX_HISTORY_ENTRIES;
    
    // Channel statistics
    JsonObject channels = doc["channels"].to<JsonObject>();
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        int count = 0;
        for (const auto& point : data_buffer) {
            if (point.channel_id == i) count++;
        }
        channels[String(i)] = count;
    }
    
    return doc;
}

JsonDocument HistoricalDataManager::getTrendAnalysis() {
    JsonDocument doc;
    
    if (data_buffer.size() < 2) {
        doc["error"] = "Insufficient data for trend analysis";
        return doc;
    }
    
    // Simple trend analysis for each channel
    JsonObject trends = doc["trends"].to<JsonObject>();
    
    for (int channel = 0; channel < MAX_FILAMENT_CHANNELS; channel++) {
        std::vector<int> positions;
        std::vector<int> pressures;
        
        // Collect recent data for this channel
        unsigned long recent_cutoff = millis() - 3600000; // Last hour
        for (const auto& point : data_buffer) {
            if (point.channel_id == channel && point.timestamp > recent_cutoff) {
                positions.push_back(point.motor_position);
                pressures.push_back(point.pressure);
            }
        }
        
        if (positions.size() > 1) {
            JsonObject channel_trend = trends[String(channel)].to<JsonObject>();
            
            // Calculate position trend
            int pos_start = positions.front();
            int pos_end = positions.back();
            channel_trend["position_change"] = pos_end - pos_start;
            channel_trend["position_trend"] = (pos_end > pos_start) ? "increasing" : "decreasing";
            
            // Calculate pressure trend
            int pressure_start = pressures.front();
            int pressure_end = pressures.back();
            channel_trend["pressure_change"] = pressure_end - pressure_start;
            channel_trend["pressure_trend"] = (pressure_end > pressure_start) ? "increasing" : "decreasing";
            
            channel_trend["data_points"] = positions.size();
        }
    }
    
    return doc;
}

bool HistoricalDataManager::saveToFile() {
    if (!data_dirty) return true;
    
    File file = LittleFS.open(HISTORY_FILE_PATH, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open history file for writing");
        return false;
    }
    
    JsonDocument doc;
    JsonArray data_array = doc["data"].to<JsonArray>();
    
    for (const auto& point : data_buffer) {
        JsonObject entry = data_array.add<JsonObject>();
        entry["ts"] = point.timestamp;
        entry["ch"] = point.channel_id;
        entry["hp"] = point.hall_position;
        entry["fp"] = point.filament_present;
        entry["mp"] = point.motor_position;
        entry["ms"] = point.motor_speed;
        entry["pr"] = point.pressure;
        entry["br"] = point.led_brightness;
        entry["vo"] = point.voltage;
        entry["er"] = point.error_count;
    }
    
    doc["version"] = 1;
    doc["saved_at"] = millis();
    
    if (serializeJson(doc, file) == 0) {
        ESP_LOGE(TAG, "Failed to write history data");
        file.close();
        return false;
    }
    
    file.close();
    data_dirty = false;
    
    ESP_LOGI(TAG, "Saved %d historical data points to file", data_buffer.size());
    return true;
}

bool HistoricalDataManager::loadFromFile() {
    File file = LittleFS.open(HISTORY_FILE_PATH, "r");
    if (!file) {
        ESP_LOGW(TAG, "History file not found");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        ESP_LOGE(TAG, "Failed to parse history file: %s", error.c_str());
        return false;
    }
    
    data_buffer.clear();
    
    JsonArrayConst data_array = doc["data"].as<JsonArrayConst>();
    for (JsonObjectConst entry : data_array) {
        HistoricalDataPoint point;
        point.timestamp = entry["ts"].as<unsigned long>();
        point.channel_id = entry["ch"].as<int>();
        point.hall_position = entry["hp"].as<int>();
        point.filament_present = entry["fp"].as<bool>();
        point.motor_position = entry["mp"].as<int>();
        point.motor_speed = entry["ms"].as<float>();
        point.pressure = entry["pr"].as<int>();
        point.led_brightness = entry["br"].as<int>();
        point.voltage = entry["vo"].as<float>();
        point.error_count = entry["er"].as<int>();
        
        data_buffer.push_back(point);
    }
    
    ESP_LOGI(TAG, "Loaded %d historical data points", data_buffer.size());
    return true;
}

void HistoricalDataManager::clearOldData(int retention_days) {
    unsigned long cutoff_time = millis() - (retention_days * 24UL * 3600UL * 1000UL);
    
    auto it = std::remove_if(data_buffer.begin(), data_buffer.end(),
        [cutoff_time](const HistoricalDataPoint& point) {
            return point.timestamp < cutoff_time;
        });
    
    if (it != data_buffer.end()) {
        int removed = std::distance(it, data_buffer.end());
        data_buffer.erase(it, data_buffer.end());
        data_dirty = true;
        ESP_LOGI(TAG, "Removed %d old data points", removed);
    }
}

unsigned long HistoricalDataManager::getOldestTimestamp() const {
    if (data_buffer.empty()) return 0;
    
    auto it = std::min_element(data_buffer.begin(), data_buffer.end(),
        [](const HistoricalDataPoint& a, const HistoricalDataPoint& b) {
            return a.timestamp < b.timestamp;
        });
    
    return it->timestamp;
}

unsigned long HistoricalDataManager::getLatestTimestamp() const {
    if (data_buffer.empty()) return 0;
    
    auto it = std::max_element(data_buffer.begin(), data_buffer.end(),
        [](const HistoricalDataPoint& a, const HistoricalDataPoint& b) {
            return a.timestamp < b.timestamp;
        });
    
    return it->timestamp;
}

void HistoricalDataManager::maintainBufferSize() {
    if (data_buffer.size() > MAX_HISTORY_ENTRIES) {
        // Remove oldest entries
        int to_remove = data_buffer.size() - MAX_HISTORY_ENTRIES;
        
        std::sort(data_buffer.begin(), data_buffer.end(),
            [](const HistoricalDataPoint& a, const HistoricalDataPoint& b) {
                return a.timestamp < b.timestamp;
            });
        
        data_buffer.erase(data_buffer.begin(), data_buffer.begin() + to_remove);
        data_dirty = true;
        
        ESP_LOGD(TAG, "Removed %d old entries to maintain buffer size", to_remove);
    }
}