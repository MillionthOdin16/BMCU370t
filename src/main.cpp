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

// System health monitoring variables
bool MC_STU_ERROR[4] = {false, false, false, false};
uint64_t last_health_check_time = 0;
uint64_t system_uptime_start = 0;
uint32_t watchdog_refresh_count = 0;
bool system_healthy = true;

// External communication health variables (defined in BambuBus.cpp)
extern uint64_t last_heartbeat_time;
extern uint64_t last_communication_time;

// Task scheduling for improved responsiveness
struct TaskScheduler {
    uint64_t last_sensor_update;
    uint64_t last_communication_poll;
    uint64_t last_rgb_update;
    uint64_t last_health_check;
    uint64_t last_watchdog_refresh;
    bool sensor_update_pending;
    bool communication_pending;
    bool rgb_update_pending;
};
TaskScheduler scheduler = {0, 0, 0, 0, 0, false, false, false};

/**
 * Initialize system health monitoring and watchdog
 */
void system_health_init()
{
    system_uptime_start = millis();
    last_health_check_time = system_uptime_start;
    
    if (WATCHDOG_ENABLED) {
        // Initialize Independent Watchdog (IWDG)
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(IWDG_Prescaler_256);  // 40kHz / 256 = 156.25Hz
        IWDG_SetReload(WATCHDOG_TIMEOUT_MS * 156 / 1000);  // Convert to timer ticks
        IWDG_Enable();
        
        DEBUG_MY("Watchdog enabled with ");
        DEBUG_num("", WATCHDOG_TIMEOUT_MS);
        DEBUG_MY("ms timeout\n");
    }
}

/**
 * Refresh watchdog timer to prevent system reset
 */
void refresh_watchdog()
{
    if (WATCHDOG_ENABLED) {
        IWDG_ReloadCounter();
        watchdog_refresh_count++;
    }
}

/**
 * Perform comprehensive system health check
 */
void perform_system_health_check()
{
    uint64_t current_time = millis();
    
    if ((current_time - last_health_check_time) < SYSTEM_HEALTH_CHECK_INTERVAL_MS) {
        return;  // Not time for health check yet
    }
    
    bool previous_health = system_healthy;
    system_healthy = true;
    
    // Check ADC sensor health
    bool* adc_health = ADC_DMA_get_health_status();
    uint32_t* fault_counts = ADC_DMA_get_fault_counts();
    
    for (int i = 0; i < 8; i++) {
        if (!adc_health[i]) {
            system_healthy = false;
            DEBUG_MY("ADC channel ");
            DEBUG_num("", i);
            DEBUG_MY(" unhealthy, faults: ");
            DEBUG_num("", fault_counts[i]);
            DEBUG_MY("\n");
        }
    }
    
    // Check communication health
    uint32_t comm_errors, comm_timeouts, comm_retries;
    bool comm_healthy;
    get_communication_stats(&comm_errors, &comm_timeouts, &comm_retries, &comm_healthy);
    
    if (!comm_healthy) {
        system_healthy = false;
        DEBUG_MY("Communication unhealthy - errors: ");
        DEBUG_num("", comm_errors);
        DEBUG_MY(", timeouts: ");
        DEBUG_num("", comm_timeouts);
        DEBUG_MY(", retries: ");
        DEBUG_num("", comm_retries);
        DEBUG_MY("\n");
    }
    
    // Check voltage levels (if monitoring enabled)
    if (VOLTAGE_MONITORING_ENABLED) {
        // Use ADC to check supply voltage - typically on a dedicated channel
        // This would need hardware-specific implementation
        // For now, we'll assume voltage is OK if ADC is working
    }
    
    // Report health status change
    if (previous_health != system_healthy) {
        if (system_healthy) {
            DEBUG_MY("System health restored\n");
        } else {
            DEBUG_MY("System health degraded\n");
        }
    }
    
    // Update error flags for motion control
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        // Check channel-specific health
        int pull_adc_idx = (3 - i) * 2;
        int presence_adc_idx = pull_adc_idx + 1;
        
        bool channel_healthy = true;
        if (pull_adc_idx < 8 && !adc_health[pull_adc_idx]) channel_healthy = false;
        if (presence_adc_idx < 8 && !adc_health[presence_adc_idx]) channel_healthy = false;
        
        MC_STU_ERROR[i] = !channel_healthy;
    }
    
    last_health_check_time = current_time;
}

/**
 * High-frequency task scheduler for improved responsiveness
 */
void update_task_scheduler()
{
    uint64_t current_time = get_time64();  // Use microsecond timing
    
    // Check which tasks need to be executed
    if ((current_time - scheduler.last_sensor_update) >= (SENSOR_UPDATE_INTERVAL_US * 1000)) {
        scheduler.sensor_update_pending = true;
    }
    
    if ((current_time - scheduler.last_communication_poll) >= (COMMUNICATION_POLL_INTERVAL_US * 1000)) {
        scheduler.communication_pending = true;
    }
    
    if ((current_time - scheduler.last_rgb_update) >= (RGB_UPDATE_FAST_INTERVAL_MS * 1000000ULL)) {
        scheduler.rgb_update_pending = true;
    }
}

/**
 * Execute pending tasks based on priority
 */
void execute_scheduled_tasks()
{
    uint64_t current_time = get_time64();
    
    // Highest priority: Sensor updates
    if (scheduler.sensor_update_pending) {
        MC_PULL_ONLINE_read();
        scheduler.last_sensor_update = current_time;
        scheduler.sensor_update_pending = false;
    }
    
    // Medium priority: Communication
    if (scheduler.communication_pending) {
        update_communication_health();
        scheduler.last_communication_poll = current_time;
        scheduler.communication_pending = false;
    }
    
    // Lower priority: RGB updates (only if no higher priority tasks)
    if (scheduler.rgb_update_pending && !scheduler.sensor_update_pending && !scheduler.communication_pending) {
        // RGB updates are handled in main loop to avoid conflicts
        scheduler.last_rgb_update = current_time;
        scheduler.rgb_update_pending = false;
    }
}

extern void BambuBUS_UART_Init();
extern void send_uart(const unsigned char *data, uint16_t length);

void setup()
{
    WWDG_DeInit();
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, DISABLE); // Disable windowed watchdog
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);
    
    // Initialize system health monitoring and watchdog
    system_health_init();
    
    // Initialize RGB lights
    RGB_init();
    // Update RGB display
    RGB_show_data();
    // Set RGB brightness - this maintains color proportions while limiting maximum values
    RGB_Set_Brightness();

    BambuBus_init();
    DEBUG_init();
    Motion_control_init();
    
    DEBUG_MY("BMCU370 firmware initialized\n");
    DEBUG_MY("System health monitoring: ");
    DEBUG_MY(WATCHDOG_ENABLED ? "enabled" : "disabled");
    DEBUG_MY("\n");
    
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
    
    if (num < 0 || num >= 2) { // Each channel has max 2 LEDs (status and pull-online)
        DEBUG_MY("ERROR: Invalid LED num in Set_MC_RGB\n");
        return;
    }
    
    const int set_colors[3] = {R, G, B};
    bool is_new_colors = false;

    // Check if any color component has changed
    for (int colors = 0; colors < 3; colors++) {
        if (channel_runs_colors[channel][num][colors] != set_colors[colors]) {
            channel_runs_colors[channel][num][colors] = set_colors[colors]; // Record new color
            is_new_colors = true; // Color has been updated
        }
    }
    
    // Update LED only if color has changed (reduces unnecessary updates)
    if (is_new_colors) {
        strip_channel[channel].setPixelColor(num, strip_channel[channel].Color(R, G, B));
        strip_channel[channel].show(); // Display new color
    }
}

/**
 * Enhanced RGB status indication with breathing effects and health status
 */
void Show_SYS_RGB(int BambuBUS_status)
{
    static uint64_t last_breath_time = 0;
    static int breath_direction = 1;
    static int breath_intensity = 0;
    uint64_t current_time = millis();
    
    // Breathing effect timing (2-second cycle)
    if ((current_time - last_breath_time) > 20) { // 50Hz update rate for smooth breathing
        breath_intensity += breath_direction * 2;
        if (breath_intensity >= 40) {
            breath_direction = -1;
        } else if (breath_intensity <= 5) {
            breath_direction = 1;
        }
        last_breath_time = current_time;
    }
    
    // Main board status with enhanced patterns
    if (BambuBUS_status == -1) // Offline - red breathing
    {
        int red_intensity = breath_intensity;
        strip_PD1.setPixelColor(0, strip_PD1.Color(red_intensity, 0, 0));
        strip_PD1.show();
    }
    else if (BambuBUS_status == -2) // System health issues - yellow/orange breathing
    {
        int yellow_intensity = breath_intensity;
        strip_PD1.setPixelColor(0, strip_PD1.Color(yellow_intensity, yellow_intensity/2, 0));
        strip_PD1.show();
    }
    else if (BambuBUS_status == 0) // Online - white breathing
    {
        int white_intensity = breath_intensity / 4 + 5; // Dimmer for normal operation
        strip_PD1.setPixelColor(0, strip_PD1.Color(white_intensity, white_intensity, white_intensity));
        strip_PD1.show();
    }
    
    // Enhanced channel status indication
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++)
    {
        if (MC_STU_ERROR[i])
        {
            // Flashing red for error
            int flash_rate = (current_time / 500) % 2; // 1Hz flash
            if (flash_rate) {
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(255, 0, 0));
            } else {
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(0, 0, 0));
            }
            strip_channel[i].show();
        }
        else
        {
            // Check filament status for normal indication
            bool filament_online = get_filament_online(i);
            AMS_filament_motion motion = get_filament_motion(i);
            
            if (!filament_online) {
                // No filament - dim blue breathing
                int blue_intensity = breath_intensity / 6 + 2;
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(0, 0, blue_intensity));
            }
            else if (motion == AMS_filament_motion::on_use) {
                // Active feeding - solid green
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(0, 30, 0));
            }
            else if (motion == AMS_filament_motion::need_send_out || 
                     motion == AMS_filament_motion::need_pull_back) {
                // Motion pending - pulsing yellow
                int yellow_pulse = (current_time / 200) % 2 ? 20 : 5; // 2.5Hz pulse
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(yellow_pulse, yellow_pulse, 0));
            }
            else {
                // Idle with filament - soft green breathing  
                int green_intensity = breath_intensity / 8 + 3;
                strip_channel[i].setPixelColor(0, strip_channel[i].Color(0, green_intensity, 0));
            }
            strip_channel[i].show();
        }
    }
}

BambuBus_package_type is_first_run = BambuBus_package_type::NONE;
void loop()
{
    while (1)
    {
        // High-frequency task scheduling for responsiveness
        update_task_scheduler();
        execute_scheduled_tasks();
        
        // Refresh watchdog periodically (not every loop to reduce overhead)
        static uint64_t last_watchdog_time = 0;
        uint64_t current_time = millis();
        if ((current_time - last_watchdog_time) > 100) { // Refresh every 100ms
            refresh_watchdog();
            last_watchdog_time = current_time;
        }
        
        // Perform system health checks less frequently
        static uint64_t last_health_time = 0;
        if ((current_time - last_health_time) > SYSTEM_HEALTH_CHECK_INTERVAL_MS) {
            perform_system_health_check();
            last_health_time = current_time;
        }
        
        BambuBus_package_type stu = BambuBus_run();
        static int error = 0;
        bool motion_can_run = false;
        uint16_t device_type = get_now_BambuBus_device_type();
        
        if (stu != BambuBus_package_type::NONE) // have data/offline
        {
            motion_can_run = true;
            if (stu == BambuBus_package_type::ERROR) // offline
            {
                error = -1;
            }
            else // have data
            {
                error = 0;
                // Update heartbeat time for communication monitoring
                if (stu == BambuBus_package_type::heartbeat) {
                    last_heartbeat_time = millis();
                }
            }
            
            // Update RGB at high frequency for smooth animations
            static uint64_t last_sys_rgb_time = 0;
            if ((current_time - last_sys_rgb_time) >= RGB_UPDATE_FAST_INTERVAL_MS) {
                // Modify error status based on system health
                int display_error = error;
                if (!system_healthy) {
                    display_error = -2;  // Special health error code
                }
                Show_SYS_RGB(display_error);
                last_sys_rgb_time = current_time;
            }
        }
        else
        {
            // No data - check for communication timeout less frequently
            static uint64_t last_no_data_check = 0;
            
            if ((current_time - last_no_data_check) > 1000) { // Check every second
                if ((current_time - last_communication_time) > COMMUNICATION_TIMEOUT_MS) {
                    motion_can_run = true;  // Allow motion control to handle timeout
                    error = -1;  // Communication timeout
                }
                last_no_data_check = current_time;
            }
        }
        
        // Log output with enhanced status information (reduced frequency)
        static uint64_t last_log_time = 0;
        if (is_first_run != stu || (current_time - last_log_time) > 5000) // Log every 5 seconds or on change
        {
            is_first_run = stu;
            if (stu == BambuBus_package_type::ERROR)
            {
                DEBUG_MY("BambuBus_offline\n");
            }
            else if (stu == BambuBus_package_type::heartbeat)
            {
                DEBUG_MY("BambuBus_online - heartbeat received\n");
            }
            else if (device_type == BambuBus_AMS_lite)
            {
                DEBUG_MY("Run_To_AMS_lite\n");
            }
            else if (device_type == BambuBus_AMS)
            {
                DEBUG_MY("Run_To_AMS\n");
            }
            else if (stu != BambuBus_package_type::NONE)
            {
                DEBUG_MY("Running Unknown ???\n");
            }
            
            // Log system health status less frequently
            if ((current_time - last_log_time) > 5000) {
                DEBUG_MY("System health: ");
                DEBUG_MY(system_healthy ? "OK" : "DEGRADED");
                DEBUG_MY(", Watchdog refreshes: ");
                DEBUG_num("", watchdog_refresh_count);
                DEBUG_MY("\n");
                last_log_time = current_time;
            }
        }

        if (motion_can_run)
        {
            Motion_control_run(error);
        }
        
        // Minimal delay for maximum responsiveness while preventing CPU overload
        delay_any_us(FAST_LOOP_INTERVAL_US);  // 100μs = 10kHz main loop frequency
    }
}
