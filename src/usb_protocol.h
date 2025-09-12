#pragma once

#include "usb_status_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Buffer sizes for USB communication
#define USB_RX_BUFFER_SIZE          512     ///< USB receive buffer size
#define USB_TX_BUFFER_SIZE          2048    ///< USB transmit buffer size
#define USB_COMMAND_QUEUE_SIZE      8       ///< Command queue size

/**
 * USB communication states
 */
typedef enum {
    USB_STATE_DISCONNECTED = 0,     ///< USB disconnected
    USB_STATE_CONNECTED,            ///< USB connected but not ready
    USB_STATE_READY                 ///< USB ready for communication
} usb_comm_state_t;

/**
 * Command queue entry
 */
typedef struct {
    char command[USB_COMMAND_BUFFER_SIZE];  ///< Command string
    uint64_t timestamp;                     ///< Command timestamp
    bool processed;                         ///< Processing status
} usb_command_entry_t;

/**
 * Initialize USB protocol handler
 */
void usb_protocol_init(void);

/**
 * Get current USB communication state
 * @return Current communication state
 */
usb_comm_state_t usb_protocol_get_state(void);

/**
 * Check if USB is ready for communication
 * @return true if ready, false otherwise
 */
bool usb_protocol_is_ready(void);

/**
 * Main USB protocol processing function
 * Call this regularly from main loop
 */
void usb_protocol_run(void);

/**
 * Process pending commands from command queue
 * @return Number of commands processed
 */
int usb_protocol_process_commands(void);

/**
 * Send response via USB
 * @param response Response string to send
 * @return true if sent successfully, false otherwise
 */
bool usb_protocol_send_response(const char* response);

/**
 * Send log message via USB (formatted as JSON)
 * @param log_msg Log message to send
 * @return true if sent successfully, false otherwise
 */
bool usb_protocol_send_log(const char* log_msg);

/**
 * Get USB communication statistics
 * @param rx_count_out Pointer to store RX count (can be NULL)
 * @param tx_count_out Pointer to store TX count (can be NULL)
 * @param error_count_out Pointer to store error count (can be NULL)
 */
void usb_protocol_get_stats(uint32_t* rx_count_out, uint32_t* tx_count_out, uint32_t* error_count_out);

/**
 * Reset USB protocol state
 */
void usb_protocol_reset(void);

/**
 * Enable/disable automatic status reporting
 * @param enable true to enable, false to disable
 * @param interval_ms Interval between status reports in milliseconds
 */
void usb_protocol_set_auto_status(bool enable, uint32_t interval_ms);

#ifdef __cplusplus
}
#endif