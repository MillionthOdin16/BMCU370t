# ESP32 Web Interface Implementation Plan

## Overview

This document outlines the complete implementation plan for adding an ESP32-based web interface to the BMCU370 system. The ESP32 will communicate with the BMCU370 via the existing USB-C connector and provide a web-based status monitoring and parameter configuration interface.

## Current System Status

**BMCU370 Hardware:**
- CH32V203C8T6 microcontroller @ 144MHz
- USB-C connector on pins PA11/PA12 (USB_DM/USB_DP)
- Currently used for DFU firmware programming only
- Memory usage: 62.5% flash (40,984/65,536 bytes), 58.8% RAM (12,052/20,480 bytes)

**Current Communication Interfaces:**
- BambuBus (RS485) - Primary communication with Bambu Lab printers
- Debug UART - 115200 baud debug logging
- USB DFU - Firmware programming

## Architecture Overview

```
[Bambu Lab Printer] ←RS485→ [BMCU370] ←USB-C→ [ESP32] ←WiFi→ [Web Browser]
                               ↓                    ↓
                          [Hall Sensors]      [Web Server]
                          [RGB LEDs]          [REST API]
                          [Motors]            [WebSocket]
                          [Flash Config]      [WiFi Portal]
```

## Implementation Phases

### Phase 1: BMCU370 USB Communication Foundation

#### 1.1 USB CDC-ACM Implementation
**Objective:** Add USB serial communication to BMCU370 firmware

**Tasks:**
- Add USB device stack (CDC-ACM class) to PlatformIO dependencies
- Implement USB device descriptor for serial communication
- Add USB interrupt handlers and enumeration logic
- Create dual-mode operation (DFU programming vs CDC communication)

**Memory Impact:** ~3-5KB flash, ~1KB RAM

#### 1.2 Status Reporting API
**Objective:** Create functions to export system status in structured format

**Status Information to Export:**
```json
{
  "system": {
    "uptime": 12345678,
    "version": "00.00.06.49",
    "bambubus_status": "online",
    "device_type": "AMS",
    "error_count": 0
  },
  "channels": [
    {
      "id": 0,
      "filament": {
        "status": "online",
        "name": "PLA Basic",
        "color": {"r": 255, "g": 255, "b": 255, "a": 255},
        "temperature": {"min": 190, "max": 220},
        "meters_remaining": 125.5
      },
      "motion": {
        "state": "idle",
        "position": 1024,
        "speed": 0.0,
        "pressure": 1650
      },
      "rgb": {
        "brightness": 15,
        "current_color": {"r": 0, "g": 255, "b": 0}
      },
      "sensors": {
        "hall_position": 1024,
        "filament_present": true
      },
      "errors": []
    }
  ],
  "config": {
    "led_brightness": {"main": 35, "channels": 15},
    "voltage_thresholds": {"high": 1.85, "low": 1.45},
    "motion_params": {"send_time": 1200, "filter_k": 100}
  }
}
```

**Implementation:**
- Create `usb_status_api.cpp/h` module
- Add JSON serialization functions (lightweight, manual formatting)
- Implement status collection from all subsystems
- Add parameter validation and update functions

#### 1.3 USB Communication Protocol
**Command Protocol:**
```
GET_STATUS\n          -> Returns full status JSON
GET_CONFIG\n          -> Returns configuration JSON  
SET_PARAM key=value\n -> Updates parameter
GET_VERSION\n         -> Returns version info
RESET\n               -> Software reset
```

**Implementation:**
- Create `usb_protocol.cpp/h` module
- Add command parser with line-based protocol
- Implement response formatting
- Add error handling and validation

### Phase 2: ESP32 Firmware Development

#### 2.1 Hardware Requirements
**ESP32 Board Selection:**
- **ESP32-S3** with native USB host capability, OR
- **ESP32 + USB Host Shield**

**Recommended Development Board:**
- ESP32-S3-DevKitC-1 (native USB host)
- 240MHz dual-core, 512KB SRAM, WiFi

#### 2.2 USB Host Implementation
**Objective:** Enable ESP32 to communicate with BMCU370 via USB

**Libraries:**
- ESP32 USB Host Library (for ESP32-S3)
- USB CDC driver for serial communication

**Implementation:**
```cpp
// Core USB host functionality
class BMCU370_USB_Host {
private:
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t device_hdl;
    
public:
    bool connect();
    bool sendCommand(const String& cmd);
    String readResponse(uint32_t timeout_ms = 1000);
    bool isConnected();
    void disconnect();
};
```

#### 2.3 Communication Layer
**Objective:** High-level interface for BMCU370 communication

```cpp
class BMCU370_Interface {
private:
    BMCU370_USB_Host* usb_host;
    JsonDocument status_cache;
    unsigned long last_update;
    
public:
    bool updateStatus();
    JsonDocument getStatus();
    JsonDocument getConfig();
    bool setParameter(const String& key, const String& value);
    bool isOnline();
};
```

#### 2.4 Web Server Implementation
**Framework:** ESP32 AsyncWebServer

**Features:**
- Responsive web interface (HTML/CSS/JavaScript)
- REST API endpoints
- WebSocket for real-time updates
- Static file serving from SPIFFS/LittleFS

**API Endpoints:**
```
GET  /api/status      -> Current BMCU370 status
GET  /api/config      -> Current configuration
POST /api/config      -> Update configuration
GET  /api/logs        -> Recent log entries
GET  /ws              -> WebSocket connection
```

#### 2.5 Web Interface Development
**Technologies:**
- HTML5 with responsive CSS (Bootstrap or similar)
- JavaScript for dynamic updates
- Chart.js for historical data visualization
- WebSocket API for real-time updates

**Pages:**
1. **Dashboard** - Live status overview with visual indicators
2. **Configuration** - Parameter adjustment forms
3. **Diagnostics** - System logs and error information
4. **Network** - WiFi configuration and OTA updates

### Phase 3: Integration and Testing

#### 3.1 Hardware Setup
**Connection Diagram:**
```
ESP32-S3 USB Host Port ←USB-C Cable→ BMCU370 USB-C Connector
ESP32-S3 Power        ←USB Cable→   Computer/Power Supply
```

#### 3.2 Software Integration
1. Flash updated BMCU370 firmware with USB CDC support
2. Flash ESP32 firmware with web interface
3. Connect ESP32 to WiFi network
4. Connect USB-C cable between devices
5. Access web interface via ESP32 IP address

#### 3.3 Testing Scenarios
**Functional Tests:**
- USB enumeration and communication
- Status data accuracy vs actual BMCU370 state
- Parameter updates reflect in BMCU370 behavior
- Web interface responsiveness and real-time updates
- Network connectivity and WiFi management

**Stress Tests:**
- Continuous operation for 24+ hours
- USB disconnect/reconnect scenarios
- High-frequency status updates
- Multiple concurrent web browser connections

### Phase 4: Advanced Features

#### 4.1 Historical Data Logging
- Store sensor readings in ESP32 flash memory
- Implement data retention policies (circular buffer)
- Add export functionality (CSV/JSON)
- Create trend analysis and alerting

#### 4.2 OTA Updates
- Implement ESP32 OTA web interface
- Add BMCU370 firmware update via USB (future enhancement)
- Version management and rollback capability

#### 4.3 Network Integration
- MQTT client for external monitoring systems
- HTTP webhooks for alerts and notifications
- Network discovery and auto-configuration

## Resource Requirements

### BMCU370 Firmware Changes
**Memory Impact:**
- Flash: +8-10KB (USB stack + API + protocol)
- RAM: +2-3KB (USB buffers + status structures)
- **Estimated Total:** 51-53KB flash (78-81%), 14-15KB RAM (68-73%)

**Remaining Capacity:** 
- Flash: ~12-14KB available for future enhancements
- RAM: ~5-6KB available

### ESP32 Requirements
**Hardware:**
- ESP32-S3 development board (~$15-25)
- USB-C cable for BMCU370 connection
- Case/enclosure for integration

**Software Libraries:**
- USB Host Library (built-in)
- AsyncWebServer (~50KB)
- ArduinoJSON (~30KB)
- WiFi libraries (built-in)

## Development Timeline

### Phase 1: Foundation (3-4 weeks)
- Week 1: USB CDC implementation and testing
- Week 2: Status API development and validation
- Week 3: Communication protocol and integration
- Week 4: Testing and optimization

### Phase 2: ESP32 Development (4-5 weeks)
- Week 1: USB host implementation
- Week 2: Communication layer development
- Week 3: Web server and API implementation
- Week 4: Web interface development
- Week 5: Integration and testing

### Phase 3: Integration (2-3 weeks)
- Week 1: Hardware integration and basic testing
- Week 2: Comprehensive testing and bug fixes
- Week 3: Documentation and deployment preparation

### Phase 4: Advanced Features (3-4 weeks)
- Week 1: Historical data logging
- Week 2: OTA update implementation
- Week 3: Network integration features
- Week 4: Final testing and optimization

**Total Estimated Timeline: 12-16 weeks**

## Risk Mitigation

### Technical Risks
1. **Memory Constraints**: Monitor flash/RAM usage carefully, implement modular design
2. **USB Compatibility**: Test with different ESP32 boards and USB implementations
3. **Real-time Requirements**: Ensure web interface doesn't interfere with critical BambuBus communication

### Implementation Risks
1. **Pin Conflicts**: PA11 conflict between USB and RGB Channel 0 - document limitation
2. **Power Requirements**: Verify ESP32 can provide adequate power to BMCU370 via USB
3. **EMI/Interference**: Test for electrical interference between devices

### Mitigation Strategies
- Incremental development with frequent testing
- Fallback to UART communication if USB proves problematic
- Modular architecture allowing feature removal if memory constrained
- Comprehensive testing with actual printer hardware

## Success Criteria

### Minimum Viable Product (MVP)
- [ ] BMCU370 reports status via USB CDC interface
- [ ] ESP32 can enumerate and communicate with BMCU370
- [ ] Web interface displays real-time status of all 4 channels
- [ ] Basic parameter configuration (LED brightness, thresholds)
- [ ] Stable operation for 24+ hours

### Full Feature Set
- [ ] Real-time WebSocket updates (< 1 second latency)
- [ ] Historical data visualization
- [ ] Mobile-responsive web interface
- [ ] OTA update capability
- [ ] Network integration (MQTT/webhooks)
- [ ] Comprehensive error handling and recovery

This implementation will provide BMCU370 users with powerful remote monitoring and configuration capabilities while maintaining full compatibility with existing Bambu Lab printer integration.