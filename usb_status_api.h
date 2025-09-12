#pragma once

#include "main.h"
#include "config.h"
#include "BambuBus.h"
#include "Motion_control.h"
#include "ADC_DMA.h"
#include "many_soft_AS5600.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * USB Status API for BMCU370
 * 
 * This module provides a standardized interface for exporting system status
 * and configuration data via USB communication. The API supports JSON-like
 * string formatting for easy parsing by external systems.
 */

// Maximum buffer sizes for status strings
#define USB_STATUS_BUFFER_SIZE      2048
#define USB_COMMAND_BUFFER_SIZE     128
#define USB_RESPONSE_BUFFER_SIZE    512

/**
 * USB communication command types
 */
typedef enum {
    USB_CMD_UNKNOWN = 0,
    USB_CMD_GET_STATUS,     ///< Request full system status
    USB_CMD_GET_CONFIG,     ///< Request configuration parameters
    USB_CMD_SET_PARAM,      ///< Set configuration parameter
    USB_CMD_GET_VERSION,    ///< Request version information
    USB_CMD_RESET,          ///< Software reset request
    USB_CMD_ERROR           ///< Command parsing error
} usb_command_type_t;

/**
 * USB command structure for parameter updates
 */
typedef struct {
    char key[32];           ///< Parameter key name
    char value[32];         ///< Parameter value string
} usb_param_t;

/**
 * USB command structure
 */
typedef struct {
    usb_command_type_t type;
    usb_param_t param;      ///< Used for SET_PARAM commands
} usb_command_t;

/**
 * Initialize USB status API
 * Call once during system initialization
 */
void usb_status_api_init(void);

/**
 * Parse incoming USB command string
 * @param command_str Null-terminated command string
 * @param cmd Output command structure
 * @return true if command parsed successfully, false on error
 */
bool usb_parse_command(const char* command_str, usb_command_t* cmd);

/**
 * Generate full system status string in JSON-like format
 * @param buffer Output buffer (must be >= USB_STATUS_BUFFER_SIZE)
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int usb_get_full_status(char* buffer, size_t buffer_size);

/**
 * Generate configuration parameters string in JSON-like format
 * @param buffer Output buffer (must be >= USB_RESPONSE_BUFFER_SIZE)
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int usb_get_config(char* buffer, size_t buffer_size);

/**
 * Generate version information string
 * @param buffer Output buffer (must be >= USB_RESPONSE_BUFFER_SIZE)
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int usb_get_version(char* buffer, size_t buffer_size);

/**
 * Update configuration parameter
 * @param key Parameter key name
 * @param value Parameter value string
 * @return true if parameter was updated successfully, false on error
 */
bool usb_set_parameter(const char* key, const char* value);

/**
 * Process USB command and generate response
 * @param cmd Command structure from usb_parse_command()
 * @param response_buffer Output buffer for response
 * @param buffer_size Size of response buffer
 * @return Number of characters written to response buffer
 */
int usb_process_command(const usb_command_t* cmd, char* response_buffer, size_t buffer_size);

/**
 * Get system uptime in milliseconds
 * @return Uptime in milliseconds since system start
 */
uint64_t usb_get_uptime_ms(void);

/**
 * Format error response
 * @param error_msg Error message string
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written
 */
int usb_format_error_response(const char* error_msg, char* buffer, size_t buffer_size);

/**
 * Update current BambuBus status for status reporting
 * Call from main loop when BambuBus status changes
 * @param status Current BambuBus package type
 */
void usb_status_update_bambubus_status(BambuBus_package_type status);

#ifdef __cplusplus
}
#endif