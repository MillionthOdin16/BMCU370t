#pragma once

#include "main.h"

extern void Motion_control_init();
extern void Motion_control_set_PWM(uint8_t CHx, int PWM);
extern void Motion_control_run(int error);

// Adaptive pressure control functions
extern void pressure_sensor_calibrate_channel(int channel);
extern void pressure_sensor_auto_calibrate();
extern void pressure_sensor_reset_calibration(int channel);
extern void pressure_sensor_reset_all_calibration();
extern void pressure_sensor_force_calibrate_all();
extern float get_dynamic_pressure_threshold_high(int channel);
extern float get_dynamic_pressure_threshold_low(int channel);
extern bool is_pressure_in_deadband(int channel, float pressure);

// Advanced pressure signal filtering functions
extern void pressure_filter_init();
extern float pressure_filter_update(int channel, float raw_value);
extern void pressure_filter_reset_channel(int channel);
extern void pressure_filter_reset_all();
extern float get_filtered_pressure(int channel);
extern bool is_pressure_reading_stable(int channel);

// Dynamic update rate control functions
extern void pressure_update_rate_init();
extern uint32_t get_adaptive_update_interval_ms();
extern void update_system_activity_state(bool active_feeding, bool critical_pressure);
extern bool should_update_pressure_now();

// Pressure diagnostics and health monitoring functions
extern void pressure_diagnostics_init();
extern void pressure_diagnostics_update(int channel, float raw_value, float filtered_value);
extern bool is_pressure_sensor_healthy(int channel);
extern bool is_pressure_sensor_stuck(int channel);
extern float get_pressure_sensor_drift(int channel);
extern void get_pressure_performance_metrics(int channel, float* avg_error, float* max_error, uint32_t* correction_count);
extern void reset_pressure_diagnostics(int channel);
extern void pressure_diagnostics_check_all_channels();

// Automatic direction learning functions
extern void start_direction_learning(int channel, int commanded_direction);
extern void update_direction_learning(int channel, float movement_delta);
extern void complete_direction_learning(int channel);
extern bool get_direction_learning_status(int channel, float* confidence, int* samples, bool* complete);
extern void reset_direction_learning(int channel);
extern void reset_all_learned_directions();

// Loading direction detection functions
extern void start_loading_direction_detection(int channel);
extern void update_loading_direction_detection(int channel);
extern void complete_loading_direction_detection(int channel);