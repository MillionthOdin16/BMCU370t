#pragma once
#include "ch32v20x.h"
#include <Arduino.h>
#include "stdlib.h"
#include "Debug_log.h"
#include "Flash_saves.h"
#include "Motion_control.h"
#include "BambuBus.h"
#include "time64.h"
#include "many_soft_AS5600.h"
#include "ADC_DMA.h"
#include "config.h"

/**
 * Microsecond delay using SysTick timer
 * @param time Delay time in microseconds (must be > 0)
 * @warning This is a blocking delay - use carefully in interrupt contexts
 */
#define delay_any_us(time)                                                    \
    do {                                                                      \
        if ((time) <= 0) break;                                              \
        const uint64_t _delay_div = DELAY_US_DIVISOR(time);                  \
        if (_delay_div == 0) break; /* Prevent division by zero */           \
        const uint32_t _compare_val = (uint32_t)(SystemCoreClock / _delay_div); \
        if (_compare_val == 0) break; /* Prevent zero comparison value */     \
        SysTick->SR &= ~(1 << 0);                                            \
        SysTick->CMP = _compare_val;                                          \
        SysTick->CTLR |= (1 << 5) | (1 << 4) | (1 << 0);                     \
        while (!(SysTick->SR & 1));                                          \
        SysTick->CTLR &= ~(1 << 0);                                          \
    } while(0)

/**
 * Millisecond delay using SysTick timer  
 * @param time Delay time in milliseconds (must be > 0)
 * @warning This is a blocking delay - use carefully in interrupt contexts
 */
#define delay_any_ms(time)                                                    \
    do {                                                                      \
        if ((time) <= 0) break;                                              \
        const uint64_t _delay_div = DELAY_MS_DIVISOR(time);                  \
        if (_delay_div == 0) break; /* Prevent division by zero */           \
        const uint32_t _compare_val = (uint32_t)(SystemCoreClock / _delay_div); \
        if (_compare_val == 0) break; /* Prevent zero comparison value */     \
        SysTick->SR &= ~(1 << 0);                                            \
        SysTick->CMP = _compare_val;                                          \
        SysTick->CTLR |= (1 << 5) | (1 << 4) | (1 << 0);                     \
        while (!(SysTick->SR & 1));                                          \
        SysTick->CTLR &= ~(1 << 0);                                          \
    } while(0)
/**
 * Set RGB color for a specific channel and LED index
 * @param channel Channel number (0-3)
 * @param num LED index within the channel
 * @param R Red component (0-255)
 * @param G Green component (0-255) 
 * @param B Blue component (0-255)
 */
extern void Set_MC_RGB(uint8_t channel, int num, uint8_t R, uint8_t G, uint8_t B);

/**
 * Set RGB color for channel status LED (convenience macro)
 */
#define MC_STU_RGB_set(channel, R, G, B) Set_MC_RGB(channel, 0, R, G, B)

/**
 * Set RGB color for channel pull-online LED (convenience macro)  
 */
#define MC_PULL_ONLINE_RGB_set(channel, R, G, B) Set_MC_RGB(channel, 1, R, G, B)

/**
 * Global variables
 */
extern uint8_t channel_colors[4][4];  ///< Channel color storage array [channel][RGBA]
extern bool MC_STU_ERROR[4];          ///< Channel error status flags

// System health monitoring functions
extern void system_health_init();
extern void refresh_watchdog();
extern void perform_system_health_check();

// Communication health monitoring functions (from BambuBus.cpp)
extern void update_communication_health();
extern void get_communication_stats(uint32_t* errors, uint32_t* timeouts, uint32_t* retries, bool* healthy);
extern void get_rx_stats(uint32_t* rx_errors, uint32_t* crc_errors, uint32_t* length_errors);
