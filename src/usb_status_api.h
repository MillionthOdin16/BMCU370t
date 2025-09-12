#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations to avoid circular dependencies
enum class BambuBus_package_type;

// Buffer sizes
#define USB_STATUS_BUFFER_SIZE      2048    ///< Buffer size for status responses
#define USB_COMMAND_BUFFER_SIZE     256     ///< Buffer size for command strings
#define USB_PARAM_KEY_SIZE          64      ///< Maximum parameter key length
#define USB_PARAM_VALUE_SIZE        64      ///< Maximum parameter value length

/**
 * USB command types
 */
typedef enum {
    USB_CMD_UNKNOWN = 0,        ///< Unknown/invalid command
    USB_CMD_GET_STATUS,         ///< Get full system status
    USB_CMD_GET_CONFIG,         ///< Get configuration parameters
    USB_CMD_GET_VERSION,        ///< Get version information
    USB_CMD_SET_PARAM,          ///< Set parameter value
    USB_CMD_RESET,              ///< Software reset
    USB_CMD_ERROR               ///< Command parsing error
} usb_command_type_t;

/**
 * Parameter structure for SET_PARAM commands
 */
typedef struct {
    char key[USB_PARAM_KEY_SIZE];       ///< Parameter key
    char value[USB_PARAM_VALUE_SIZE];   ///< Parameter value
} usb_param_t;

/**
 * USB command structure
 */
typedef struct {
    usb_command_type_t type;    ///< Command type
    usb_param_t param;          ///< Parameter data (for SET_PARAM)
} usb_command_t;

/**
 * Initialize USB status API
 */
void usb_status_api_init(void);

/**
 * Parse command string into command structure
 * @param command_str Null-terminated command string
 * @param cmd Output command structure
 * @return true if parsed successfully, false otherwise
 */
bool usb_parse_command(const char* command_str, usb_command_t* cmd);

/**
 * Get system uptime in milliseconds
 * @return Uptime in milliseconds
 */
uint64_t usb_get_uptime_ms(void);

/**
 * Get version information as JSON string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, 0 on error
 */
int usb_get_version(char* buffer, size_t buffer_size);

/**
 * Get configuration parameters as JSON string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, 0 on error
 */
int usb_get_config(char* buffer, size_t buffer_size);

/**
 * Get full system status as JSON string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, 0 on error
 */
int usb_get_full_status(char* buffer, size_t buffer_size);

/**
 * Set a parameter value
 * @param key Parameter key
 * @param value Parameter value
 * @return true if parameter was set successfully, false otherwise
 */
bool usb_set_parameter(const char* key, const char* value);

/**
 * Format error response as JSON
 * @param error_msg Error message
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, 0 on error
 */
int usb_format_error_response(const char* error_msg, char* buffer, size_t buffer_size);

/**
 * Process USB command and generate response
 * @param cmd Command to process
 * @param response_buffer Output buffer for response
 * @param buffer_size Buffer size
 * @return Number of bytes written to response buffer
 */
int usb_process_command(const usb_command_t* cmd, char* response_buffer, size_t buffer_size);

/**
 * Update BambuBus status for reporting
 * @param status Current BambuBus status
 */
void usb_status_update_bambubus_status(BambuBus_package_type status);

#ifdef __cplusplus
}
#endif