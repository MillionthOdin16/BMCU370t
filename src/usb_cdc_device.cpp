#include "usb_cdc_device.h"
#include "Debug_log.h"
#include <string.h>

// Simple time function for USB module
uint32_t usb_get_time(void) {
    return ::get_time64(); // Use existing time function from global scope
}

/**
 * Simplified USB CDC Device Implementation for CH32V203
 * Framework implementation for BMCU370 - focuses on interface compatibility
 * Full hardware implementation would require detailed USB register programming
 */

// USB Device Variables
volatile uint8_t  USBFS_DevConfig = 0;
volatile uint8_t  USBFS_DevAddr = 0;
volatile uint8_t  USBFS_DevEnumStatus = 0;

// CDC Line Coding (for future use)
static usb_cdc_line_coding_t cdc_line_coding __attribute__((unused)) = {
    .baudrate = 115200,
    .format = 0,      // 1 stop bit
    .paritytype = 0,  // No parity
    .datatype = 8     // 8 data bits
};

// USB CDC State
static bool usb_cdc_connected = false;
static bool usb_cdc_enumerated = false;

// Circular buffers for CDC data (for future use)
static uint8_t usb_rx_ring_buffer[256] __attribute__((unused));
static volatile int usb_rx_head __attribute__((unused)) = 0;
static volatile int usb_rx_tail __attribute__((unused)) = 0;

static uint8_t usb_tx_ring_buffer[256] __attribute__((unused)); 
static volatile int usb_tx_head __attribute__((unused)) = 0;
static volatile int usb_tx_tail __attribute__((unused)) = 0;

/**
 * USB Clock Configuration for CH32V203
 */
static void usb_rcc_init(void) {
    // USB clock configuration for 144MHz system clock
    if (SystemCoreClock == 144000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div3); // 144MHz/3 = 48MHz
    } else if (SystemCoreClock == 96000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2); // 96MHz/2 = 48MHz  
    } else if (SystemCoreClock == 48000000) {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1); // 48MHz/1 = 48MHz
    }
    
    // Enable USB peripheral clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
    
    DEBUG_MY("USB: Clock configured for system clock\n");
}

/**
 * USB GPIO Configuration (PA11=USB_DM, PA12=USB_DP)
 */
static void usb_gpio_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    
    // Enable GPIOA clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // Configure USB pins PA11 and PA12
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    DEBUG_MY("USB: GPIO PA11/PA12 configured for USB_DM/USB_DP\n");
    DEBUG_MY("USB: WARNING - PA11 pin conflict: Channel 0 RGB LEDs disabled\n");
}

/**
 * USB Device Initialization - Framework version
 */
void usb_cdc_init(void) {
    DEBUG_MY("USB CDC: Initializing USB CDC device framework...\n");
    
    // Configure clocks and GPIO
    usb_rcc_init();
    usb_gpio_init();
    
    // Initialize state variables
    USBFS_DevConfig = 0;
    USBFS_DevAddr = 0;
    USBFS_DevEnumStatus = 0;
    usb_cdc_connected = false;
    usb_cdc_enumerated = false;
    
    // Clear ring buffers
    usb_rx_head = usb_rx_tail = 0;
    usb_tx_head = usb_tx_tail = 0;
    
    // For framework implementation, simulate connection after delay
    // Real implementation would involve full USB CDC stack
    static uint32_t init_timestamp __attribute__((unused)) = 0;
    init_timestamp = usb_get_time();
    
    DEBUG_MY("USB CDC: Framework initialization complete\n");
    DEBUG_MY("USB CDC: Note - This is a framework implementation\n");
    DEBUG_MY("USB CDC: Full USB stack implementation would require additional work\n");
}

/**
 * Check if USB CDC is connected - Framework version
 */
bool usb_cdc_is_connected(void) {
    // For framework implementation, simulate connection after 3 seconds
    static uint32_t init_time = 0;
    if (init_time == 0) {
        init_time = usb_get_time();
    }
    
    // Simulate USB enumeration after 3 seconds
    if (!usb_cdc_enumerated && (usb_get_time() - init_time > 3000)) {
        usb_cdc_enumerated = true;
        usb_cdc_connected = true;
        USBFS_DevConfig = 1;
        DEBUG_MY("USB CDC: Framework - simulated connection established\n");
    }
    
    return usb_cdc_connected && usb_cdc_enumerated && (USBFS_DevConfig != 0);
}

/**
 * Send data via USB CDC - Framework version
 */
int usb_cdc_send_data(const uint8_t* data, int length) {
    if (!usb_cdc_is_connected() || !data || length <= 0) {
        return 0;
    }
    
    // Framework implementation - simulate sending by logging
    DEBUG_MY("USB CDC TX: Data prepared for transmission\n");
    
    // In a real implementation, data would be sent via USB hardware
    // For framework, we'll just return the length to indicate "success"
    return length;
}

/**
 * Receive data from USB CDC - Framework version  
 */
int usb_cdc_receive_data(uint8_t* buffer, int max_length) {
    if (!buffer || max_length <= 0) {
        return 0;
    }
    
    // Framework implementation - no real data reception
    // Real implementation would read from USB RX buffers
    return 0; // No data available in framework mode
}

/**
 * Check if transmit is ready - Framework version
 */
bool usb_cdc_tx_ready(void) {
    return usb_cdc_is_connected(); // Always ready in framework mode
}

/**
 * Get available receive bytes - Framework version
 */
int usb_cdc_rx_available(void) {
    return 0; // No data available in framework mode
}

/**
 * USB Interrupt Handler - Framework version
 */
void usb_cdc_interrupt_handler(void) {
    // Framework implementation - no actual interrupt handling
    // Real implementation would handle USB interrupts
}

/**
 * USB Interrupt Service Routine - Framework version
 */
void USB_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USB_IRQHandler(void) {
    // Framework implementation - minimal interrupt handler
    usb_cdc_interrupt_handler();
}