#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_init(void);
bool usb_cdc_is_connected(void);
int usb_cdc_send_data(const uint8_t* data, int length);
int usb_cdc_receive_data(uint8_t* buffer, int max_length);
bool usb_cdc_tx_ready(void);
int usb_cdc_rx_available(void);
void usb_cdc_interrupt_handler(void);

#ifdef __cplusplus
}
#endif