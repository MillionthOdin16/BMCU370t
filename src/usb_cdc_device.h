#pragma once

#include "ch32v20x.h"
#include "config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB CDC Device Implementation for CH32V203
 * Adapted for BMCU370 USB interface
 */

// USB Endpoint Configuration
#define DEF_UEP_IN                    0x80
#define DEF_UEP_OUT                   0x00
#define DEF_UEP0                      0x00
#define DEF_UEP1                      0x01
#define DEF_UEP2                      0x02
#define DEF_UEP3                      0x03

// USB Buffer Sizes  
#define DEF_USBD_UEP0_SIZE            64
#define DEF_USBD_ENDP1_SIZE           64
#define DEF_USBD_ENDP2_SIZE           64
#define DEF_USBD_ENDP3_SIZE           64

// USB Device Status
extern volatile uint8_t  USBFS_DevConfig;
extern volatile uint8_t  USBFS_DevAddr;
extern volatile uint8_t  USBFS_DevEnumStatus;

// USB CDC Line Coding
typedef struct {
    uint32_t baudrate;
    uint8_t  format;
    uint8_t  paritytype;
    uint8_t  datatype;
} usb_cdc_line_coding_t;

// USB CDC Control Commands
#define CDC_SET_LINE_CODING           0x20
#define CDC_GET_LINE_CODING           0x21
#define CDC_SET_CONTROL_LINE_STATE    0x22

/**
 * Initialize USB CDC device
 */
void usb_cdc_init(void);

/**
 * Check if USB CDC is connected and enumerated
 */
bool usb_cdc_is_connected(void);

/**
 * Send data via USB CDC
 * @param data Pointer to data buffer
 * @param length Number of bytes to send
 * @return Number of bytes actually sent
 */
int usb_cdc_send_data(const uint8_t* data, int length);

/**
 * Receive data from USB CDC
 * @param buffer Buffer to store received data
 * @param max_length Maximum bytes to receive
 * @return Number of bytes actually received
 */
int usb_cdc_receive_data(uint8_t* buffer, int max_length);

/**
 * Check if transmit buffer is ready for more data
 */
bool usb_cdc_tx_ready(void);

/**
 * Get number of bytes available in receive buffer
 */
int usb_cdc_rx_available(void);

/**
 * USB interrupt handler (called from main interrupt vector)
 */
void usb_cdc_interrupt_handler(void);

#ifdef __cplusplus
}
#endif