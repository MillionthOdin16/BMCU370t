#include "usb_cdc_device.h"
#include "Adafruit_TinyUSB.h"
#include "Debug_log.h"

// TinyUSB CDC device
Adafruit_USBD_CDC USB_Serial;

void usb_cdc_init(void) {
    // Initialize USB Serial
    USB_Serial.begin(115200);
    DEBUG_MY("USB CDC: TinyUSB CDC initialized\n");
}

bool usb_cdc_is_connected(void) {
    return (bool)USB_Serial;
}

int usb_cdc_send_data(const uint8_t* data, int length) {
    if (!usb_cdc_is_connected() || !data || length <= 0) {
        return 0;
    }
    return USB_Serial.write(data, length);
}

int usb_cdc_receive_data(uint8_t* buffer, int max_length) {
    if (!usb_cdc_is_connected() || !buffer || max_length <= 0) {
        return 0;
    }
    return USB_Serial.read(buffer, max_length);
}

bool usb_cdc_tx_ready(void) {
    if (!usb_cdc_is_connected()) {
        return false;
    }
    return USB_Serial.availableForWrite() > 0;
}

int usb_cdc_rx_available(void) {
    if (!usb_cdc_is_connected()) {
        return 0;
    }
    return USB_Serial.available();
}

void usb_cdc_interrupt_handler(void) {
    // TinyUSB handles interrupts internally
}