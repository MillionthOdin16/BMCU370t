#pragma once
#include "main.h"
#include "config.h"

extern void ADC_DMA_init();
extern float *ADC_DMA_get_value();

// Enhanced robustness functions
extern bool* ADC_DMA_get_health_status();
extern uint32_t* ADC_DMA_get_fault_counts();