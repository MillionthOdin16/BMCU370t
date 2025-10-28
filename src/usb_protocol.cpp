#include "usb_protocol.h"
#include "usb_cdc_device.h"
#include "Debug_log.h"
#include <string.h>
#include <stdio.h>

// USB communication state and buffers
static usb_comm_state_t usb_state = USB_STATE_DISCONNECTED;
static char usb_rx_buffer[USB_RX_BUFFER_SIZE];
static char usb_tx_buffer[USB_TX_BUFFER_SIZE];
static int usb_rx_pos = 0;

// Command queue for processing commands
static usb_command_entry_t command_queue[USB_COMMAND_QUEUE_SIZE];
static int command_queue_head = 0;
static int command_queue_tail = 0;
static int command_queue_count = 0;

// Statistics
static uint32_t rx_count = 0;
static uint32_t tx_count = 0;
static uint32_t error_count = 0;

// Auto status reporting
static bool auto_status_enabled = false;
static uint32_t auto_status_interval = 5000; // 5 seconds default
static uint32_t last_auto_status_time = 0;

void usb_protocol_init(void) {
    // Initialize state
    usb_state = USB_STATE_DISCONNECTED;
    usb_rx_pos = 0;
    
    // Clear buffers
    memset(usb_rx_buffer, 0, sizeof(usb_rx_buffer));
    memset(usb_tx_buffer, 0, sizeof(usb_tx_buffer));
    
    // Initialize command queue
    memset(command_queue, 0, sizeof(command_queue));
    command_queue_head = 0;
    command_queue_tail = 0;
    command_queue_count = 0;
    
    // Reset statistics
    rx_count = 0;
    tx_count = 0;
    error_count = 0;
    
    usb_cdc_init();
    
    DEBUG_MY("USB Protocol: Initialized\n");
}

usb_comm_state_t usb_protocol_get_state(void) {
    return usb_state;
}

bool usb_protocol_is_ready(void) {
    return (usb_state == USB_STATE_READY);
}

static bool add_command_to_queue(const char* command) {
    if (command_queue_count >= USB_COMMAND_QUEUE_SIZE) {
        DEBUG_MY("USB Protocol: Command queue full\n");
        error_count++;
        return false;
    }
    
    // Add command to queue
    strncpy(command_queue[command_queue_tail].command, command, USB_COMMAND_BUFFER_SIZE - 1);
    command_queue[command_queue_tail].command[USB_COMMAND_BUFFER_SIZE - 1] = '\0';
    command_queue[command_queue_tail].timestamp = get_time64();
    command_queue[command_queue_tail].processed = false;
    
    command_queue_tail = (command_queue_tail + 1) % USB_COMMAND_QUEUE_SIZE;
    command_queue_count++;
    
    return true;
}

static void process_received_data(void) {
    // Look for complete commands (terminated by newline)
    char* line_start = usb_rx_buffer;
    char* line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        // Null-terminate the command
        *line_end = '\0';
        
        // Add command to processing queue
        if (strlen(line_start) > 0) {
            add_command_to_queue(line_start);
            rx_count++;
        }
        
        // Move to next line
        line_start = line_end + 1;
    }
    
    // Move remaining data to start of buffer
    if (line_start != usb_rx_buffer) {
        int remaining = strlen(line_start);
        memmove(usb_rx_buffer, line_start, remaining + 1);
        usb_rx_pos = remaining;
    }
}

int usb_protocol_process_commands(void) {
    int processed_count = 0;
    
    while (command_queue_count > 0) {
        // Get command from head of queue
        usb_command_entry_t* entry = &command_queue[command_queue_head];
        
        if (!entry->processed) {
            // Parse and process command
            usb_command_t cmd;
            if (usb_parse_command(entry->command, &cmd)) {
                // Process command and generate response
                int response_len = usb_process_command(&cmd, usb_tx_buffer, USB_TX_BUFFER_SIZE);
                
                if (response_len > 0) {
                    if (usb_protocol_send_response(usb_tx_buffer)) {
                        processed_count++;
                    } else {
                        error_count++;
                    }
                }
                
                // Handle special commands that require post-response actions
                if (cmd.type == USB_CMD_RESET) {
                    // Schedule system reset after response is sent
                    DEBUG_MY("USB Protocol: System reset requested\n");
                    // In a real implementation, this would set a flag for delayed reset
                    // delay(100); // Allow response to be sent
                    // NVIC_SystemReset();
                }
                else if (cmd.type == USB_CMD_DFU) {
                    // Schedule DFU mode entry after response is sent
                    DEBUG_MY("USB Protocol: DFU mode requested\n");
                    // usb_protocol_enter_dfu_mode(); // Would be called after delay
                }
            } else {
                // Send error response for invalid commands
                usb_format_error_response("Invalid command format", usb_tx_buffer, USB_TX_BUFFER_SIZE);
                usb_protocol_send_response(usb_tx_buffer);
                error_count++;
            }
            
            entry->processed = true;
        }
        
        // Remove command from queue
        command_queue_head = (command_queue_head + 1) % USB_COMMAND_QUEUE_SIZE;
        command_queue_count--;
    }
    
    return processed_count;
}

void usb_protocol_run(void) {
    // Update connection state
    if (usb_cdc_is_connected()) {
        if (usb_state == USB_STATE_DISCONNECTED) {
            usb_state = USB_STATE_CONNECTED;
            DEBUG_MY("USB Protocol: Connected\n");
            
            // Send welcome message
            usb_protocol_send_response("{\"status\":\"BMCU370 USB Interface Ready\"}\n");
            usb_state = USB_STATE_READY;
        }
    } else {
        if (usb_state != USB_STATE_DISCONNECTED) {
            usb_state = USB_STATE_DISCONNECTED;
            DEBUG_MY("USB Protocol: Disconnected\n");
        }
        return; // No USB connection, nothing to do
    }
    
    // Read incoming data
    if (usb_state == USB_STATE_READY) {
        int available_space = USB_RX_BUFFER_SIZE - usb_rx_pos - 1;
        if (available_space > 0) {
            int bytes_read = usb_cdc_receive_data((uint8_t*)usb_rx_buffer + usb_rx_pos, available_space);
            if (bytes_read > 0) {
                usb_rx_pos += bytes_read;
                usb_rx_buffer[usb_rx_pos] = '\0'; // Ensure null termination
                
                // Process received data
                process_received_data();
            }
        } else {
            // Buffer full - try to preserve partial commands by shifting buffer
            DEBUG_MY("USB Protocol: RX buffer overflow, attempting recovery\n");
            
            // Look for the last complete line/command separator
            int last_newline = -1;
            for (int i = usb_rx_pos - 1; i >= 0; i--) {
                if (usb_rx_buffer[i] == '\n' || usb_rx_buffer[i] == '\r') {
                    last_newline = i;
                    break;
                }
            }
            
            if (last_newline > 0) {
                // Preserve data after the last complete command
                int preserve_len = usb_rx_pos - last_newline - 1;
                if (preserve_len > 0 && preserve_len < USB_RX_BUFFER_SIZE / 2) {
                    memmove(usb_rx_buffer, &usb_rx_buffer[last_newline + 1], preserve_len);
                    usb_rx_pos = preserve_len;
                    usb_rx_buffer[usb_rx_pos] = '\0';
                    DEBUG_MY("USB Protocol: Buffer recovered, preserved bytes\n");
                } else {
                    usb_rx_pos = 0;
                    usb_rx_buffer[0] = '\0';
                }
            } else {
                // No recoverable data, clear buffer
                usb_rx_pos = 0;
                usb_rx_buffer[0] = '\0';
            }
            error_count++;
        }
        
        // Process pending commands
        usb_protocol_process_commands();
        
        // Handle automatic status reporting
        if (auto_status_enabled) {
            uint32_t current_time = get_time64();
            if (current_time - last_auto_status_time >= auto_status_interval) {
                usb_get_full_status(usb_tx_buffer, USB_TX_BUFFER_SIZE);
                usb_protocol_send_response(usb_tx_buffer);
                last_auto_status_time = current_time;
            }
        }
    }
}

bool usb_protocol_send_response(const char* response) {
    if (!response || usb_state != USB_STATE_READY) {
        return false;
    }
    
    int length = strlen(response);
    if (usb_cdc_send_data((const uint8_t*)response, length)) {
        tx_count++;
        return true;
    } else {
        error_count++;
        return false;
    }
}

bool usb_protocol_send_log(const char* log_msg) {
    if (!log_msg || usb_state != USB_STATE_READY) {
        return false;
    }
    
    // Format log message as JSON
    snprintf(usb_tx_buffer, USB_TX_BUFFER_SIZE, 
             "{\"log\":{\"timestamp\":%llu,\"message\":\"%s\"}}\n",
             get_time64(), log_msg);
             
    return usb_protocol_send_response(usb_tx_buffer);
}

void usb_protocol_get_stats(uint32_t* rx_count_out, uint32_t* tx_count_out, uint32_t* error_count_out) {
    if (rx_count_out) *rx_count_out = rx_count;
    if (tx_count_out) *tx_count_out = tx_count;
    if (error_count_out) *error_count_out = error_count;
}

void usb_protocol_reset(void) {
    usb_rx_pos = 0;
    memset(usb_rx_buffer, 0, sizeof(usb_rx_buffer));
    
    // Clear command queue
    command_queue_head = 0;
    command_queue_tail = 0;
    command_queue_count = 0;
    
    DEBUG_MY("USB Protocol: Reset\n");
}

void usb_protocol_enter_dfu_mode(void) {
#ifdef USB_DFU_DUAL_MODE
    DEBUG_MY("USB Protocol: Entering DFU mode for firmware update\n");
    
    // Set magic value in backup register to indicate DFU request
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    BKP_WriteBackupRegister(BKP_DR1, 0xDF00); // DFU magic value
    
    // Reset system - will enter DFU mode on next boot
    NVIC_SystemReset();
#else
    DEBUG_MY("USB Protocol: DFU mode not supported in this build\n");
#endif
}

void usb_protocol_set_auto_status(bool enable, uint32_t interval_ms) {
    auto_status_enabled = enable;
    auto_status_interval = interval_ms;
    if (enable) {
        last_auto_status_time = get_time64();
        DEBUG_MY("USB Protocol: Auto status enabled\n");
    } else {
        DEBUG_MY("USB Protocol: Auto status disabled\n");
    }
}