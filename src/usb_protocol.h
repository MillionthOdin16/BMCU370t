#pragma once

#include "main.h"
#include "config.h"
#include "usb_status_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * USB Communication Protocol Handler
 * 
 * This module handles USB CDC-ACM communication for the BMCU370,
 * providing a command-line interface for status monitoring and
 * parameter configuration via USB.
 */

// USB communication buffer sizes (defined in config.h)

/**
 * USB communication state
 */
typedef enum {
    USB_STATE_DISCONNECTED = 0,
    USB_STATE_CONNECTED,
    USB_STATE_READY,
    USB_STATE_ERROR
} usb_comm_state_t;

/**
 * USB command queue entry
 */
typedef struct {
    char command[USB_COMMAND_BUFFER_SIZE];
    uint32_t timestamp;
    bool processed;
} usb_command_entry_t;

/**
 * Initialize USB communication protocol
 * Call once during system initialization
 */
void usb_protocol_init(void);

/**
 * Main USB communication handler
 * Call regularly from main loop to process incoming commands
 */
void usb_protocol_run(void);

/**
 * Get current USB communication state
 * @return Current state
 */
usb_comm_state_t usb_protocol_get_state(void);

/**
 * Check if USB is connected and ready for communication
 * @return true if ready, false if not connected or not ready
 */
bool usb_protocol_is_ready(void);

/**
 * Send response string via USB
 * @param response Null-terminated response string
 * @return true if sent successfully, false on error
 */
bool usb_protocol_send_response(const char* response);

/**
 * Send log message via USB (for debugging)
 * @param log_msg Null-terminated log message
 * @return true if sent successfully, false on error
 */
bool usb_protocol_send_log(const char* log_msg);

/**
 * Process pending USB commands from queue
 * @return Number of commands processed
 */
int usb_protocol_process_commands(void);

/**
 * Get statistics about USB communication
 * @param rx_count Pointer to receive count variable (can be NULL)
 * @param tx_count Pointer to transmit count variable (can be NULL)
 * @param error_count Pointer to error count variable (can be NULL)
 */
void usb_protocol_get_stats(uint32_t* rx_count, uint32_t* tx_count, uint32_t* error_count);

/**
 * Reset USB communication subsystem
 * Clears buffers and resets state
 */
void usb_protocol_reset(void);

/**
 * Trigger DFU mode for firmware updates
 * Sets magic value and resets system to bootloader
 * Preserves firmware update capability
 */
void usb_protocol_enter_dfu_mode(void);

/**
 * Enable/disable automatic status reports
 * @param enable true to enable periodic status reports, false to disable
 * @param interval_ms Interval between status reports (only used if enable=true)
 */
void usb_protocol_set_auto_status(bool enable, uint32_t interval_ms);

#ifdef __cplusplus
}
#endif