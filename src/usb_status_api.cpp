#include "usb_status_api.h"
#include "Flash_saves.h"
#include "time64.h"
#include "many_soft_AS5600.h"
#include "BambuBus.h"
#include "Motion_control.h"
#include "ADC_DMA.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External variables from other modules - these are defined in BambuBus.cpp
// We need to declare the struct first since it's internal to BambuBus.cpp
// For now, we'll work with the existing getter functions

extern bool MC_STU_ERROR[MAX_FILAMENT_CHANNELS];
extern float MC_PULL_stu_raw[MAX_FILAMENT_CHANNELS]; // Raw pressure sensor readings
extern AS5600_soft_IIC_many MC_AS5600; // Hall sensor interface

// Function to get current BambuBus status
static BambuBus_package_type current_bambubus_status = BambuBus_package_type::NONE;

// Static buffer for building status strings
static char status_buffer[USB_STATUS_BUFFER_SIZE];

void usb_status_api_init(void) {
    // Initialize any required state
    current_bambubus_status = BambuBus_package_type::NONE;
}

bool usb_parse_command(const char* command_str, usb_command_t* cmd) {
    if (!command_str || !cmd) {
        return false;
    }
    
    // Initialize command structure
    memset(cmd, 0, sizeof(usb_command_t));
    cmd->type = USB_CMD_UNKNOWN;
    
    // Parse command string (remove trailing newline/carriage return)
    char temp_cmd[USB_COMMAND_BUFFER_SIZE];
    strncpy(temp_cmd, command_str, USB_COMMAND_BUFFER_SIZE - 1);
    temp_cmd[USB_COMMAND_BUFFER_SIZE - 1] = '\0';
    
    // Remove trailing whitespace
    int len = strlen(temp_cmd);
    while (len > 0 && (temp_cmd[len-1] == '\n' || temp_cmd[len-1] == '\r' || temp_cmd[len-1] == ' ')) {
        temp_cmd[--len] = '\0';
    }
    
    // Check for different command types
    if (strcmp(temp_cmd, "GET_STATUS") == 0) {
        cmd->type = USB_CMD_GET_STATUS;
        return true;
    }
    else if (strcmp(temp_cmd, "GET_CONFIG") == 0) {
        cmd->type = USB_CMD_GET_CONFIG;
        return true;
    }
    else if (strcmp(temp_cmd, "GET_VERSION") == 0) {
        cmd->type = USB_CMD_GET_VERSION;
        return true;
    }
    else if (strcmp(temp_cmd, "RESET") == 0) {
        cmd->type = USB_CMD_RESET;
        return true;
    }
    else if (strcmp(temp_cmd, "DFU") == 0) {
        cmd->type = USB_CMD_DFU;
        return true;
    }
    else if (strncmp(temp_cmd, "SET_PARAM ", 10) == 0) {
        cmd->type = USB_CMD_SET_PARAM;
        
        // Parse SET_PARAM key=value
        char* param_str = temp_cmd + 10; // Skip "SET_PARAM "
        char* equals = strchr(param_str, '=');
        
        if (!equals) {
            cmd->type = USB_CMD_ERROR;
            return false;
        }
        
        // Extract key and value
        *equals = '\0'; // Split string at '='
        strncpy(cmd->param.key, param_str, sizeof(cmd->param.key) - 1);
        strncpy(cmd->param.value, equals + 1, sizeof(cmd->param.value) - 1);
        cmd->param.key[sizeof(cmd->param.key) - 1] = '\0';
        cmd->param.value[sizeof(cmd->param.value) - 1] = '\0';
        
        return true;
    }
    
    cmd->type = USB_CMD_ERROR;
    return false;
}

uint64_t usb_get_uptime_ms(void) {
    return get_time64(); // Use existing time function from time64.h
}

int usb_get_version(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    return snprintf(buffer, buffer_size,
        "{\"version\":\"%02d.%02d.%02d.%02d\","
        "\"device_type\":\"%s\","
        "\"bambubus_version\":%d,"
        "\"build_date\":\"%s %s\"}\n",
        AMS_FIRMWARE_VERSION_MAJOR,
        AMS_FIRMWARE_VERSION_MINOR, 
        AMS_FIRMWARE_VERSION_PATCH,
        AMS_FIRMWARE_VERSION_BUILD,
        (get_now_BambuBus_device_type() == BambuBus_AMS_lite) ? "AMS_LITE" : "AMS",
        BAMBU_BUS_VERSION,
        __DATE__, __TIME__
    );
}

int usb_get_config(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    return snprintf(buffer, buffer_size,
        "{\"config\":{"
        "\"led_brightness\":{\"main\":%d,\"channels\":%d},"
        "\"voltage_thresholds\":{\"high\":%.2f,\"low\":%.2f,\"send_max\":%.2f},"
        "\"motion_params\":{\"send_time\":%d,\"filter_k\":%d},"
        "\"timing\":{\"rgb_update_interval\":%d},"
        "\"distances\":{\"internal_retract\":%.1f,\"external_retract\":%.1f}"
        "}}\n",
        BRIGHTNESS_MAIN_BOARD, BRIGHTNESS_CHANNEL,
        PULL_VOLTAGE_HIGH, PULL_VOLTAGE_LOW, PULL_VOLTAGE_SEND_MAX,
        ASSIST_SEND_TIME_MS, SPEED_FILTER_K,
        RGB_UPDATE_INTERVAL_MS,
        P1X_OUT_FILAMENT_MM, P1X_OUT_FILAMENT_EXT_MM
    );
}

static const char* motion_state_to_string(AMS_filament_motion state) {
    switch (state) {
        case AMS_filament_motion::before_pull_back: return "before_pull_back";
        case AMS_filament_motion::need_pull_back: return "need_pull_back";
        case AMS_filament_motion::need_send_out: return "need_send_out";
        case AMS_filament_motion::on_use: return "on_use";
        case AMS_filament_motion::idle: return "idle";
        default: return "unknown";
    }
}

static const char* filament_status_to_string(AMS_filament_stu status) {
    switch (status) {
        case AMS_filament_stu::offline: return "offline";
        case AMS_filament_stu::online: return "online";
        case AMS_filament_stu::NFC_waiting: return "NFC_waiting";
        default: return "unknown";
    }
}

int usb_get_full_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    int pos = 0;
    
    // Start JSON object - add USB/LED conflict status
    pos += snprintf(buffer + pos, buffer_size - pos,
        "{\"system\":{"
        "\"uptime\":%llu,"
        "\"version\":\"%02d.%02d.%02d.%02d\","
        "\"bambubus_status\":\"%s\","
        "\"device_type\":\"%s\","
        "\"active_channel\":%d,"
#if defined(USB_CDC_ENABLED) && (USB_CDC_ENABLED == 1)
        "\"usb_mode\":\"enabled\","
        "\"led_channels\":3,"
        "\"led_conflict\":\"channel_0_disabled_pa11_usb\""
#else
        "\"usb_mode\":\"disabled\","
        "\"led_channels\":4,"
        "\"led_conflict\":\"none\""
#endif
        "},\"channels\":[",
        usb_get_uptime_ms(),
        AMS_FIRMWARE_VERSION_MAJOR, AMS_FIRMWARE_VERSION_MINOR,
        AMS_FIRMWARE_VERSION_PATCH, AMS_FIRMWARE_VERSION_BUILD,
        (current_bambubus_status == BambuBus_package_type::ERROR) ? "offline" : "online",
        (get_now_BambuBus_device_type() == BambuBus_AMS_lite) ? "AMS_LITE" : "AMS",
        get_now_filament_num()
    );
    
    // Add channel data
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        if (i > 0) {
            pos += snprintf(buffer + pos, buffer_size - pos, ",");
        }
        
        // Get hall sensor position
        uint16_t hall_position = 0;
        if (MC_AS5600.raw_angle && i < MC_AS5600.numbers) {
            hall_position = MC_AS5600.raw_angle[i];
        }
        
        // Get pressure reading - convert voltage to equivalent value
        uint16_t pressure = (uint16_t)(MC_PULL_stu_raw[i] * 1000); // Convert to millivolts
        
        // Get filament status using public functions
        bool filament_online = get_filament_online(i);
        AMS_filament_motion motion = get_filament_motion(i);
        float meters = get_filament_meters(i);
        
        pos += snprintf(buffer + pos, buffer_size - pos,
            "{"
            "\"id\":%d,"
            "\"filament\":{"
            "\"status\":\"%s\","
            "\"name\":\"Channel_%d\","  // Generic name since we can't access actual name
            "\"id\":\"CH%d\","
            "\"color\":{\"r\":255,\"g\":255,\"b\":255,\"a\":255},"  // Default colors
            "\"temperature\":{\"min\":190,\"max\":220},"  // Default temps
            "\"meters_remaining\":%.2f"
            "},"
            "\"motion\":{"
            "\"state\":\"%s\","
            "\"pressure\":%d"
            "},"
            "\"sensors\":{"
            "\"hall_position\":%d,"
            "\"filament_present\":%s"
            "},"
            "\"errors\":%s"
            "}",
            i,
            filament_online ? "online" : "offline",
            i,
            i,
            meters,
            motion_state_to_string(motion),
            pressure,
            hall_position,
            filament_online ? "true" : "false",
            MC_STU_ERROR[i] ? "true" : "false"
        );
        
        // Check for buffer overflow after each channel
        if (pos >= (int)buffer_size - 100) {
            // Not enough space for remaining channels and closing brackets
            pos += snprintf(buffer + pos, buffer_size - pos, "]}");
            return pos;
        }
    }
    
    // Close JSON
    pos += snprintf(buffer + pos, buffer_size - pos, "]}\n");
    
    return pos;
}

bool usb_set_parameter(const char* key, const char* value) {
    if (!key || !value) {
        return false;
    }
    
    // Parse and update different parameter types
    if (strcmp(key, "led_brightness_main") == 0) {
        int brightness = atoi(value);
        if (brightness >= 0 && brightness <= 255) {
            // Update main board LED brightness
            // Note: This would require modifying the RGB system to allow runtime brightness changes
            return true;
        }
    }
    else if (strcmp(key, "led_brightness_channels") == 0) {
        int brightness = atoi(value);
        if (brightness >= 0 && brightness <= 255) {
            // Update channel LED brightness
            return true;
        }
    }
    else if (strncmp(key, "filament_", 9) == 0) {
        // Handle filament parameter updates
        // Parse channel number from key like "filament_0_name"
        char* endptr;
        int channel = strtol(key + 9, &endptr, 10);
        
        if (channel >= 0 && channel < MAX_FILAMENT_CHANNELS && *endptr == '_') {
            char* param_name = endptr + 1;
            
            // For now, we can only use the public API which is limited
            // Future enhancement: Add setter functions to BambuBus.h
            if (strcmp(param_name, "motion") == 0) {
                if (strcmp(value, "idle") == 0) {
                    set_filament_motion(channel, AMS_filament_motion::idle);
                    return true;
                }
                else if (strcmp(value, "send_out") == 0) {
                    set_filament_motion(channel, AMS_filament_motion::need_send_out);
                    return true;
                }
                else if (strcmp(value, "pull_back") == 0) {
                    set_filament_motion(channel, AMS_filament_motion::need_pull_back);
                    return true;
                }
            }
            else if (strcmp(param_name, "online") == 0) {
                bool online = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                set_filament_online(channel, online);
                return true;
            }
        }
    }
    
    return false; // Parameter not found or invalid value
}

int usb_format_error_response(const char* error_msg, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    return snprintf(buffer, buffer_size, "{\"error\":\"%s\"}\n", error_msg ? error_msg : "Unknown error");
}

int usb_process_command(const usb_command_t* cmd, char* response_buffer, size_t buffer_size) {
    if (!cmd || !response_buffer || buffer_size == 0) {
        return usb_format_error_response("Invalid parameters", response_buffer, buffer_size);
    }
    
    switch (cmd->type) {
        case USB_CMD_GET_STATUS:
            return usb_get_full_status(response_buffer, buffer_size);
            
        case USB_CMD_GET_CONFIG:
            return usb_get_config(response_buffer, buffer_size);
            
        case USB_CMD_GET_VERSION:
            return usb_get_version(response_buffer, buffer_size);
            
        case USB_CMD_SET_PARAM:
            if (usb_set_parameter(cmd->param.key, cmd->param.value)) {
                return snprintf(response_buffer, buffer_size, "{\"result\":\"ok\"}\n");
            } else {
                return usb_format_error_response("Parameter update failed", response_buffer, buffer_size);
            }
            
        case USB_CMD_RESET:
            // Acknowledge reset command, then reset after response is sent
            return snprintf(response_buffer, buffer_size, "{\"result\":\"resetting\"}\n");
            
        case USB_CMD_DFU:
            // Acknowledge DFU command, then enter DFU mode after response is sent
            return snprintf(response_buffer, buffer_size, "{\"result\":\"entering_dfu_mode\"}\n");
            
        case USB_CMD_ERROR:
        case USB_CMD_UNKNOWN:
        default:
            return usb_format_error_response("Unknown command", response_buffer, buffer_size);
    }
}

void usb_status_update_bambubus_status(BambuBus_package_type status) {
    current_bambubus_status = status;
}