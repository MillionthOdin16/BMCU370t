#pragma once

#include "main.h"
#define Debug_log_on
#define Debug_log_baudrate 115200

#ifdef __cplusplus
extern "C"
{
#endif
    extern void Debug_log_init();
    extern uint64_t Debug_log_count64();
    extern void Debug_log_time();
    extern void Debug_log_write(const void *data);
    extern void Debug_log_write_num(const void *data, int num);
    extern void Debug_log_write_float(float value, int precision);
    extern void Debug_log_sensor_monitor();
#define DEBUG_time_log() ;
#ifdef Debug_log_on
#define DEBUG_init() Debug_log_init()
#define DEBUG_MY(logs) Debug_log_write(logs)
#define DEBUG_num(logs, num) Debug_log_write_num(logs, num)
#define DEBUG_float(value, precision) Debug_log_write_float(value, precision)
#define DEBUG_sensors() Debug_log_sensor_monitor()
#define DEBUG_time() Debug_log_time()
#define DEBUG_get_time() Debug_log_count64()
#else
#define DEBUG_init() ;
#define DEBUG(logs) \
    ;               \
    ;
#define DEBUG_num(logs, num) \
    ;                        \
    ;
#define DEBUG_float(value, precision) \
    ;                                 \
    ;
#define DEBUG_sensors() \
    ;                   \
    ;
#define DEBUG_time() \
    ;                \
    ;
#endif
#ifdef __cplusplus
}
#endif
