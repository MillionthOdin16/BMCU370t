#include <Arduino.h>
#include "main.h"

#include "BambuBus.h"
#include "Adafruit_NeoPixel.h"

extern void debug_send_run();

// RGB LED configuration - defined in config.h
// Testing configuration: Use 8 LEDs by changing LED_PA11_NUM in config.h

// RGB strip objects for channels and main board
// Channel RGB objects: strip_channel[Chx], channels 0-3 correspond to PA11/PA8/PB1/PB0
Adafruit_NeoPixel strip_channel[MAX_FILAMENT_CHANNELS] = {
    Adafruit_NeoPixel(LED_PA11_NUM, PA11, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_PA8_NUM, PA8, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_PB1_NUM, PB1, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_PB0_NUM, PB0, NEO_GRB + NEO_KHZ800)
};

// Main board 5050 RGB LED
Adafruit_NeoPixel strip_PD1(LED_PD1_NUM, PD1, NEO_GRB + NEO_KHZ800);

/**
 * Set RGB brightness for all LED strips
 * Values range from 0-255, configured in config.h
 */
void RGB_Set_Brightness() {
    // Main board brightness
    strip_PD1.setBrightness(BRIGHTNESS_MAIN_BOARD);
    
    // Set brightness for all channels
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        strip_channel[i].setBrightness(BRIGHTNESS_CHANNEL);
    }
}

/**
 * Initialize all RGB LED strips
 */
void RGB_init() {
    strip_PD1.begin();
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        strip_channel[i].begin();
    }
}

/**
 * Update all RGB LED strips with current data
 */
void RGB_show_data() {
    strip_PD1.show();
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        strip_channel[i].show();
    }
}

// Global variables for channel color storage
uint8_t channel_colors[MAX_FILAMENT_CHANNELS][4] = {
    {DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A},
    {DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A},
    {DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A},
    {DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A}
};

// Runtime RGB color storage to avoid frequent color updates causing communication failures
uint8_t channel_runs_colors[MAX_FILAMENT_CHANNELS][2][3] = {
    // R,G,B  ,, R,G,B
    {{1, 2, 3}, {1, 2, 3}}, // Channel 0
    {{3, 2, 1}, {3, 2, 1}}, // Channel 1
    {{1, 2, 3}, {1, 2, 3}}, // Channel 2
    {{3, 2, 1}, {3, 2, 1}}  // Channel 3
};

extern void BambuBUS_UART_Init();
extern void send_uart(const unsigned char *data, uint16_t length);

void setup()
{
    WWDG_DeInit();
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, DISABLE); // Disable watchdog
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);
    // Initialize RGB lights
    RGB_init();
    // Update RGB display
    RGB_show_data();
    // Set RGB brightness - this maintains color proportions while limiting maximum values
    RGB_Set_Brightness();

    BambuBus_init();
    DEBUG_init();
    Motion_control_init();
    delay(1);
}

/**
 * Set RGB color for a specific channel LED with bounds checking
 * @param channel Channel number (0-3) 
 * @param num LED index within the channel
 * @param R Red component (0-255)
 * @param G Green component (0-255)
 * @param B Blue component (0-255)
 * @return true if color was set successfully, false if parameters were invalid
 */
void Set_MC_RGB(uint8_t channel, int num, uint8_t R, uint8_t G, uint8_t B)
{
    // Input validation - bounds checking
    if (channel >= MAX_FILAMENT_CHANNELS) {
        DEBUG_MY("ERROR: Invalid channel in Set_MC_RGB\n");
        return;
    }
    
    // Check LED count per channel based on configuration
    uint8_t led_counts[] = {LED_PA11_NUM, LED_PA8_NUM, LED_PB1_NUM, LED_PB0_NUM};
    if (num < 0 || num >= led_counts[channel]) {
        DEBUG_MY("ERROR: Invalid LED num in Set_MC_RGB\n");
        return;
    }
    
    const int set_colors[3] = {R, G, B};
    bool is_new_colors = false;

    // Check if any color component has changed
    // Safety check: ensure we don't exceed runtime color storage array bounds
    if (num < 2) { // Runtime color storage is currently hardcoded to [4][2][3]
        for (int colors = 0; colors < 3; colors++) {
            if (channel_runs_colors[channel][num][colors] != set_colors[colors]) {
                channel_runs_colors[channel][num][colors] = set_colors[colors]; // Record new color
                is_new_colors = true; // Color has been updated
            }
        }
    } else {
        // For LED indices beyond 2, we still need to update the LED but skip color tracking
        is_new_colors = true;
    }
    
    // Update LED only if color has changed (reduces unnecessary updates)
    if (is_new_colors) {
        strip_channel[channel].setPixelColor(num, strip_channel[channel].Color(R, G, B));
        strip_channel[channel].show(); // Display new color
    }
}

// Channel error status flags - initialize to no errors
bool MC_STU_ERROR[MAX_FILAMENT_CHANNELS] = {false, false, false, false};
void Show_SYS_RGB(int BambuBUS_status)
{
    // Update main board RGB light
    if (BambuBUS_status == -1) // Offline
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 0, 0)); // Red
        strip_PD1.show();
    }
    else if (BambuBUS_status == 0) // Online
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 9, 9)); // White
        strip_PD1.show();
    }
    // Update error channels, light up red LEDs
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++)
    {
        if (MC_STU_ERROR[i])
        {
            // Red color
            strip_channel[i].setPixelColor(0, strip_channel[i].Color(255, 0, 0));
            strip_channel[i].show(); // Display new color
        }
    }
}

BambuBus_package_type is_first_run = BambuBus_package_type::NONE;
void loop()
{
    while (1)
    {
        BambuBus_package_type stu = BambuBus_run();
        // int stu =-1;
        static int error = 0;
        bool motion_can_run = false;
        uint16_t device_type = get_now_BambuBus_device_type();
        if (stu != BambuBus_package_type::NONE) // have data/offline
        {
            motion_can_run = true;
            if (stu == BambuBus_package_type::ERROR) // offline
            {
                error = -1;
                // Offline - red light
            }
            else // have data
            {
                error = 0;
                // if (stu == BambuBus_package_type::heartbeat)
                // {
                // Normal operation - white light
            }
            
            // Update RGB every 3 seconds (defined in config.h)
            static unsigned long last_sys_rgb_time = 0;
            unsigned long now = get_time64();
            if (now - last_sys_rgb_time >= RGB_UPDATE_INTERVAL_MS) {
                Show_SYS_RGB(error);
                last_sys_rgb_time = now;
            }
        }
        else
        {
        } // wait for data
        // Log output
        if (is_first_run != stu)
        {
            is_first_run = stu;
            if (stu == BambuBus_package_type::ERROR)
            {                                   // offline
                DEBUG_MY("BambuBus_offline\n"); // Offline
            }
            else if (stu == BambuBus_package_type::heartbeat)
            {
                DEBUG_MY("BambuBus_online\n"); // Online
            }
            else if (device_type == BambuBus_AMS_lite)
            {
                DEBUG_MY("Run_To_AMS_lite\n"); // Online as AMS Lite
            }
            else if (device_type == BambuBus_AMS)
            {
                DEBUG_MY("Run_To_AMS\n"); // Online as AMS
            }
            else
            {
                DEBUG_MY("Running Unknown ???\n");
            }
        }

        if (motion_can_run)
        {
            Motion_control_run(error);
        }
    }
}
