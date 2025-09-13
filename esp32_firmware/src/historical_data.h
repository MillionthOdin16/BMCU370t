#ifndef HISTORICAL_DATA_H
#define HISTORICAL_DATA_H

#include <ArduinoJson.h>
#include <vector>
#include <LittleFS.h>

// Historical data configuration
#define HISTORY_SAMPLE_INTERVAL_MS    60000    // 1 minute sampling
#define MAX_HISTORY_ENTRIES          1440     // 24 hours of 1-minute samples
#define HISTORY_FILE_PATH            "/history.json"
#define HISTORY_RETENTION_DAYS       7        // Keep 7 days of data

struct HistoricalDataPoint {
    unsigned long timestamp;
    int channel_id;
    
    // Sensor data
    int hall_position;
    bool filament_present;
    
    // Motion data  
    int motor_position;
    float motor_speed;
    int pressure;
    
    // Environmental data
    int led_brightness;
    float voltage;
    int error_count;
};

class HistoricalDataManager {
private:
    std::vector<HistoricalDataPoint> data_buffer;
    unsigned long last_sample_time;
    bool data_dirty;
    bool littlefs_available;
    unsigned long last_filesystem_check;
    
public:
    HistoricalDataManager();
    
    // Data collection
    bool init();
    void addDataPoint(int channel_id, const JsonObjectConst& channel_data);
    void addSystemDataPoint(const JsonObjectConst& system_data);
    
    // Data retrieval
    JsonDocument getHistoricalData(int channel_id = -1, int hours = 24);
    JsonDocument getDataSummary();
    JsonDocument getTrendAnalysis();
    
    // Data management
    bool saveToFile();
    bool loadFromFile();
    void clearOldData(int retention_days = HISTORY_RETENTION_DAYS);
    
    // Statistics
    int getDataPointCount() const { return data_buffer.size(); }
    unsigned long getOldestTimestamp() const;
    unsigned long getLatestTimestamp() const;
    
private:
    void maintainBufferSize();
    JsonDocument createDataPoint(const HistoricalDataPoint& point);
};

#endif // HISTORICAL_DATA_H