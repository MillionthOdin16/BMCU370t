#include "Motion_control.h"
#include "config.h"
#include <string.h>  // For memset, memcpy

AS5600_soft_IIC_many MC_AS5600;

// AS5600 Hall sensor I2C pin configurations (defined in config.h)
// Note: Removing const to match function signature requirements
uint32_t AS5600_SCL[] = AS5600_SCL_PINS;
uint32_t AS5600_SDA[] = AS5600_SDA_PINS;

// Speed calculation and filtering
float speed_as5600[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};

/**
 * Initialize motion control pull-online detection system
 */
void MC_PULL_ONLINE_init()
{
    ADC_DMA_init();
}

// Motion control state variables
float MC_PULL_stu_raw[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};      ///< Raw pull sensor readings
int MC_PULL_stu[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};            ///< Processed pull status
float MC_ONLINE_key_stu_raw[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0}; ///< Raw online key sensor readings

// Channel status: 0=offline, 1=both micro-switches triggered, 2=outer triggered, 3=inner triggered  
int MC_ONLINE_key_stu[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};
int MC_ONLINE_key_stu_prev[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0}; ///< Previous presence sensor state for edge detection

// Voltage control constants (defined in config.h)
const float PULL_voltage_up = PULL_VOLTAGE_HIGH;     ///< High pressure threshold - red LED
const float PULL_voltage_down = PULL_VOLTAGE_LOW;    ///< Low pressure threshold - blue LED

// Motion assist variables
bool Assist_send_filament[MAX_FILAMENT_CHANNELS] = {false, false, false, false};
bool pull_state_old = false; ///< Previous trigger state - True: not triggered, False: feeding complete
bool is_backing_out = false;  ///< Currently backing out filament

uint64_t Assist_filament_time[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};
const uint64_t Assist_send_time = ASSIST_SEND_TIME_MS; ///< Feed assist duration after outer trigger

// Retraction distances (in millimeters) - defined in config.h
const float_t P1X_OUT_filament_meters = P1X_OUT_FILAMENT_MM;        ///< Internal retraction distance
const float_t P1X_OUT_filament_ext_meters = P1X_OUT_FILAMENT_EXT_MM; ///< External retraction distance
float_t last_total_distance[MAX_FILAMENT_CHANNELS] = {0.0f, 0.0f, 0.0f, 0.0f}; ///< Initial distance when retraction started

// Dual micro-switch configuration
const bool is_two = false; ///< Use dual micro-switches

/**
 * Read ADC values for all channels and update sensor states
 */
void MC_PULL_ONLINE_read()
{
    float *data = ADC_DMA_get_value();
    
    // Store previous presence sensor states for edge detection
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        MC_ONLINE_key_stu_prev[i] = MC_ONLINE_key_stu[i];
    }
    
    // Map ADC channels to sensor readings (channels 3,2,1,0 in reverse order)
    MC_PULL_stu_raw[3] = data[0];
    MC_ONLINE_key_stu_raw[3] = data[1];
    MC_PULL_stu_raw[2] = data[2];
    MC_ONLINE_key_stu_raw[2] = data[3];
    MC_PULL_stu_raw[1] = data[4];
    MC_ONLINE_key_stu_raw[1] = data[5];
    MC_PULL_stu_raw[0] = data[6];
    MC_ONLINE_key_stu_raw[0] = data[7];

    // Process sensor readings for each channel
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++)
    {
        /*
        if (i == 0){
            DEBUG_MY("MC_PULL_stu_raw = ");
            DEBUG_float(MC_PULL_stu_raw[i],3);
            DEBUG_MY("  MC_ONLINE_key_stu_raw = ");
            DEBUG_float(MC_ONLINE_key_stu_raw[i],3);
            DEBUG_MY("  通道：");
            DEBUG_float(i,1);
            DEBUG_MY("   \n");
        }
        */
        // Use adaptive pressure thresholds if available, otherwise fall back to static values
        float pressure_high_threshold = get_dynamic_pressure_threshold_high(i);
        float pressure_low_threshold = get_dynamic_pressure_threshold_low(i);
        
        if (MC_PULL_stu_raw[i] > pressure_high_threshold) // 压力过高
        {
            MC_PULL_stu[i] = 1;
        }
        else if (MC_PULL_stu_raw[i] < pressure_low_threshold) // 压力过低
        {
            MC_PULL_stu[i] = -1;
        }
        else // 在正常误差范围内，无需动作
        {
            MC_PULL_stu[i] = 0;
        }
        /*在线状态*/

        // 耗材在线判断
        if (is_two == false)
        {
            // 大于1.65V，为耗材在线，高电平.
            if (MC_ONLINE_key_stu_raw[i] > 1.65)
            {
                MC_ONLINE_key_stu[i] = 1;
            }
            else
            {
                MC_ONLINE_key_stu[i] = 0;
            }
        }
        else
        {
            // DEBUG_MY(MC_ONLINE_key_stu_raw);
            // 双微动
            if (MC_ONLINE_key_stu_raw[i] < 0.6f)
            { // 小于则离线.
                MC_ONLINE_key_stu[i] = 0;
            }
            else if ((MC_ONLINE_key_stu_raw[i] < 1.7f) & (MC_ONLINE_key_stu_raw[i] > 1.4f))
            { // 仅触发外侧微动，需辅助进料
                MC_ONLINE_key_stu[i] = 2;
            }
            else if (MC_ONLINE_key_stu_raw[i] > 1.7f)
            { // 双微动同时触发, 在线状态
                MC_ONLINE_key_stu[i] = 1;
            }
            else if (MC_ONLINE_key_stu_raw[i] < 1.4f)
            { // 仅触发内侧微动 , 需确认是缺料还是抖动.
                MC_ONLINE_key_stu[i] = 3;
            }
        }
        
        // Detect presence sensor rising edge (filament insertion) and automatically start feeding
        if (MC_ONLINE_key_stu_prev[i] == 0 && MC_ONLINE_key_stu[i] == 1) {
            // Filament presence detected for the first time - automatically start feeding
            if (get_filament_motion(i) == AMS_filament_motion::idle) {
                DEBUG_MY("Auto-start feeding for channel ");
                DEBUG_float(i, 0);
                DEBUG_MY(" - presence detected\n");
                set_filament_motion(i, AMS_filament_motion::need_send_out);
            }
        }
    }
}

#define PWM_lim 1000

// Adaptive pressure sensor calibration data for each channel
struct alignas(4) PressureSensorCalibration
{
    float zero_point;           ///< Sensor voltage when no pressure is applied (neutral position)
    float positive_range;       ///< Maximum positive pressure voltage range from zero point
    float negative_range;       ///< Maximum negative pressure voltage range from zero point
    float deadband_low;         ///< Dynamic low pressure threshold
    float deadband_high;        ///< Dynamic high pressure threshold
    uint16_t calibration_samples; ///< Number of samples used for calibration
    bool is_calibrated;         ///< Whether this channel has been calibrated
    uint64_t last_calibration_time; ///< Last time calibration was performed
} pressure_calibration[MAX_FILAMENT_CHANNELS];

// Active pressure correction state for each channel
struct PressureCorrectionState
{
    bool correction_active;     ///< Whether active correction is currently running
    uint64_t correction_start_time; ///< When active correction started
    float target_pressure;     ///< Target pressure for correction
    float initial_error;       ///< Initial pressure error when correction started
} pressure_correction[MAX_FILAMENT_CHANNELS];

// Advanced signal filtering state for each channel
struct PressureFilterState
{
    float filter_buffer[PRESSURE_FILTER_WINDOW_SIZE]; ///< Circular buffer for moving average
    uint8_t buffer_index;           ///< Current index in circular buffer
    uint8_t sample_count;           ///< Number of samples in buffer (for initialization)
    float filtered_value;           ///< Current filtered pressure value
    float lowpass_value;            ///< Low-pass filtered value
    float last_raw_value;           ///< Previous raw reading for change detection
    bool is_stable;                 ///< Whether pressure reading is stable
    uint16_t stability_counter;     ///< Counter for stability detection
} pressure_filter[MAX_FILAMENT_CHANNELS];

// Dynamic update rate control state
struct PressureUpdateRateState
{
    bool active_feeding;            ///< Currently in active feeding mode
    bool critical_pressure;         ///< Critical pressure deviation detected
    uint64_t last_update_time;      ///< Last pressure update timestamp
    uint32_t current_interval_ms;   ///< Current update interval
    uint16_t idle_counter;          ///< Counter for idle period detection
} pressure_update_rate;

// Pressure diagnostics and health monitoring state for each channel
struct PressureDiagnosticsState
{
    bool sensor_healthy;            ///< Overall sensor health status
    float initial_zero_point;       ///< Initial zero point for drift calculation
    float current_drift;            ///< Current sensor drift from initial calibration
    uint64_t last_diagnostic_time;  ///< Last diagnostic check timestamp
    
    // Stuck sensor detection
    float last_readings[5];         ///< Last few readings for stuck detection
    uint8_t reading_index;          ///< Index for circular buffer
    uint16_t identical_count;       ///< Count of identical consecutive readings
    
    // Performance metrics
    float total_error;              ///< Cumulative pressure error
    float max_error;                ///< Maximum pressure error observed
    uint32_t measurement_count;     ///< Total number of measurements
    uint32_t correction_count;      ///< Number of pressure corrections applied
    
    // Fault detection
    bool drift_warning;             ///< Drift warning flag
    bool stuck_warning;             ///< Stuck sensor warning flag
    uint64_t last_fault_time;       ///< Last time a fault was detected
} pressure_diagnostics[MAX_FILAMENT_CHANNELS];

struct alignas(4) Motion_control_save_struct
{
    int Motion_control_dir[4];
    bool auto_learned[4];  ///< Whether direction was learned automatically vs static correction
    PressureSensorCalibration pressure_cal[4]; ///< Per-channel pressure sensor calibration
    int check = 0x40614061;
} Motion_control_data_save;

// Automatic direction learning state
struct DirectionLearningState
{
    bool learning_active;          ///< Currently learning direction for this channel
    bool learning_complete;        ///< Direction learning completed successfully
    uint64_t learning_start_time;  ///< When learning started (ms)
    uint64_t last_sample_time;     ///< Last time a sample was taken (ms)
    float initial_position;        ///< Initial filament position when learning started
    float total_movement;          ///< Total measured movement during learning
    float accumulated_noise;       ///< Accumulated sensor noise for validation
    int command_direction;         ///< Direction commanded to motor (+1 or -1)
    int sample_count;              ///< Number of valid direction samples collected
    int positive_samples;          ///< Samples showing positive correlation
    int negative_samples;          ///< Samples showing negative correlation
    float confidence_score;        ///< Learning confidence (0.0-1.0)
    bool has_valid_data;          ///< Whether any valid sensor data was received
    int error_count;              ///< Number of invalid/noisy samples rejected
} direction_learning[MAX_FILAMENT_CHANNELS];

// Presence-based loading direction detection
struct LoadingDirectionState
{
    bool detection_active;         ///< Currently detecting loading direction
    bool detection_complete;       ///< Loading direction detection completed
    uint64_t detection_start_time; ///< When detection started (ms)
    uint64_t stable_time;          ///< Time when presence became stable (ms)
    bool initial_presence;         ///< Initial presence sensor state
    bool presence_lost;            ///< Whether presence was lost during test
    int test_direction;            ///< Direction being tested (+1 or -1)
    int confirmed_loading_direction; ///< Confirmed direction that loads filament (0=unknown)
    bool presence_stable_phase;    ///< Whether we're in the stable monitoring phase
} loading_detection[MAX_FILAMENT_CHANNELS];

#define Motion_control_save_flash_addr ((uint32_t)0x0800E000)
bool Motion_control_read()
{
    Motion_control_save_struct *ptr = (Motion_control_save_struct *)(Motion_control_save_flash_addr);
    if (ptr->check == 0x40614061)
    {
        memcpy(&Motion_control_data_save, ptr, sizeof(Motion_control_save_struct));
        return true;
    }
    return false;
}
void Motion_control_save()
{
    // Copy current pressure calibration data to save structure
    if (ADAPTIVE_PRESSURE_ENABLED) {
        for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
            Motion_control_data_save.pressure_cal[i] = pressure_calibration[i];
        }
    }
    Flash_saves(&Motion_control_data_save, sizeof(Motion_control_save_struct), Motion_control_save_flash_addr);
}

class MOTOR_PID
{

    float P = 0;
    float I = 0;
    float D = 0;
    float I_save = 0;
    float E_last = 0;
    float pid_MAX = PWM_lim;
    float pid_MIN = -PWM_lim;
    float pid_range = (pid_MAX - pid_MIN) / 2;

public:
    MOTOR_PID()
    {
        pid_MAX = PWM_lim;
        pid_MIN = -PWM_lim;
        pid_range = (pid_MAX - pid_MIN) / 2;
    }
    MOTOR_PID(float P_set, float I_set, float D_set)
    {
        init_PID(P_set, I_set, D_set);
        pid_MAX = PWM_lim;
        pid_MIN = -PWM_lim;
        pid_range = (pid_MAX - pid_MIN) / 2;
    }
    void init_PID(float P_set, float I_set, float D_set) // 注意，采用了PID独立的计算方法，I和D默认已乘P
    {
        P = P_set;
        I = I_set;
        D = D_set;
        I_save = 0;
        E_last = 0;
    }
    float caculate(float E, float time_E)
    {
        I_save += I * E * time_E;
        if (I_save > pid_range) // 对I限幅
            I_save = pid_range;
        if (I_save < -pid_range)
            I_save = -pid_range;

        float ouput_buf;
        if (time_E != 0) // 防止快速调用
            ouput_buf = P * E + I_save + D * (E - E_last) / time_E;
        else
            ouput_buf = P * E + I_save;

        if (ouput_buf > pid_MAX)
            ouput_buf = pid_MAX;
        if (ouput_buf < pid_MIN)
            ouput_buf = pid_MIN;

        E_last = E;
        return ouput_buf;
    }
    void clear()
    {
        I_save = 0;
        E_last = 0;
    }
};

enum class filament_motion_enum
{
    filament_motion_send,
    filament_motion_redetect,
    filament_motion_slow_send,
    filament_motion_pull,
    filament_motion_stop,
    filament_motion_pressure_ctrl_on_use,
    filament_motion_pressure_ctrl_idle,
};
enum class pressure_control_enum
{
    less_pressure,
    all,
    over_pressure
};

class _MOTOR_CONTROL
{
public:
    filament_motion_enum motion = filament_motion_enum::filament_motion_stop;
    int CHx = 0;
    uint64_t motor_stop_time = 0;
    MOTOR_PID PID_speed = MOTOR_PID(2, 20, 0);
    MOTOR_PID PID_pressure = MOTOR_PID(1500, 0, 0);
    float pwm_zero = 500;
    float dir = 0;
    int x1 = 0;
    _MOTOR_CONTROL(int _CHx)
    {
        CHx = _CHx;
        motor_stop_time = 0;
        motion = filament_motion_enum::filament_motion_stop;
    }

    void set_pwm_zero(float _pwm_zero)
    {
        pwm_zero = _pwm_zero;
    }
    void set_motion(filament_motion_enum _motion, uint64_t over_time)
    {
        uint64_t time_now = get_time64();
        motor_stop_time = time_now + over_time;
        if (motion != _motion)
        {
            motion = _motion;
            PID_speed.clear();
        }
    }
    filament_motion_enum get_motion()
    {
        return motion;
    }
    float _get_x_by_pressure(float pressure_voltage, float control_voltage, float time_E, pressure_control_enum control_type)
    {
        float x=0;
        
        // Get adaptive control voltage (zero point) if available
        float adaptive_control_voltage = control_voltage;
        if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[CHx].is_calibrated) {
            adaptive_control_voltage = pressure_calibration[CHx].zero_point;
        }
        
        switch (control_type)
        {
        case pressure_control_enum::all: // 全范围控制
        {
            x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - adaptive_control_voltage, time_E);
            break;
        }
        case pressure_control_enum::less_pressure: // 仅低压控制
        {
            if (pressure_voltage < adaptive_control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - adaptive_control_voltage, time_E);
            }
            break;
        }
        case pressure_control_enum::over_pressure: // 仅高压控制
        {
            if (pressure_voltage > adaptive_control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - adaptive_control_voltage, time_E);
            }
            break;
        }
        }
        
        // Enhanced pressure control responsiveness when enabled
        if (PRESSURE_CONTROL_RESPONSIVE) {
            // Scale PID output for more responsive control
            x *= PRESSURE_CONTROL_PID_P_SCALE;
            
            // Limit maximum correction to prevent excessive force
            if (x > PRESSURE_CONTROL_MAX_CORRECTION) {
                x = PRESSURE_CONTROL_MAX_CORRECTION;
            } else if (x < -PRESSURE_CONTROL_MAX_CORRECTION) {
                x = -PRESSURE_CONTROL_MAX_CORRECTION;
            }
        } else {
            // Original control scaling
            if (x > 0) // 将控制力转为平方增强，平方会消掉正负，需要判断
                x = x * x / 250;
            else
                x = -x * x / 250;
        }
        
        return x;
    }
    void run(float time_E)
    {
        // 当处于退料状态，并且需要退料时，开始记录里程。
        if (is_backing_out){
            last_total_distance[CHx] += fabs(speed_as5600[CHx] * time_E);
        }
        float speed_set = 0;
        float now_speed = speed_as5600[CHx];
        float x=0;

        uint16_t device_type = get_now_BambuBus_device_type();
        static uint64_t countdownStart[4] = {0};          // 辅助进料倒计时
        if (motion == filament_motion_enum::filament_motion_pressure_ctrl_idle) // 在空闲状态
        {
            // 当 两个微动都被释放
            if (MC_ONLINE_key_stu[CHx] == 0)
            {
                Assist_send_filament[CHx] = true; // 某通道离线后才可触发辅助进料一次
                countdownStart[CHx] = 0;          // 清空倒计时
            }

            if (Assist_send_filament[CHx] && is_two)
            { // 允许状态，尝试辅助进料
                if (MC_ONLINE_key_stu[CHx] == 2)
                {                   // 触发外侧微动
                    x = -dir * 666; // 驱动送料
                }
                if (MC_ONLINE_key_stu[CHx] == 1)
                { // 同时触发双微动，准备停机
                    if (countdownStart[CHx] == 0)
                    { // 启动倒计时
                        countdownStart[CHx] = get_time64();
                    }
                    uint64_t now = get_time64();
                    if (now - countdownStart[CHx] >= Assist_send_time) // 倒计时
                    {
                        x = 0;                             // 停止电机
                        Assist_send_filament[CHx] = false; // 达成条件，完成一轮辅助进料。
                    }
                    else
                    {
                        // 驱动送料
                        x = -dir * 666;
                    }
                }
            }
            else
            {
                // 已经触发过，或微动触发在其他状态
                if (MC_ONLINE_key_stu[CHx] != 0 && MC_PULL_stu[CHx] != 0)
                { // 如果滑块被人为拉动，做出对应响应
                    float target_pressure = 1.65f; // Default target
                    if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[CHx].is_calibrated) {
                        target_pressure = pressure_calibration[CHx].zero_point;
                    }
                    x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - target_pressure, time_E);
                }
                else
                { // 否则，保持停机
                    x = 0;
                    PID_pressure.clear();
                }
            }
        }
        else if (MC_ONLINE_key_stu[CHx] != 0) // 通道在运行状态，并且有耗材
        {
            if (motion == filament_motion_enum::filament_motion_pressure_ctrl_on_use) // 在使用状态
            {
                if (pull_state_old) { // 首次进入使用中，不触发后退，冲刷会让缓冲归位.
                    if (MC_PULL_stu_raw[CHx] < 1.55){
                        pull_state_old = false; // 检测到耗材已处于低压力。
                    }
                } else {
                    // Use adaptive pressure control with active correction if available
                    float target_pressure = 1.65f; // Default target
                    float pressure_tolerance = 0.05f; // Default tolerance
                    
                    if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[CHx].is_calibrated) {
                        target_pressure = pressure_calibration[CHx].zero_point;
                        pressure_tolerance = PRESSURE_CONTROL_DEADBAND_SMALL;
                    }
                    
                    float pressure_error = MC_PULL_stu_raw[CHx] - target_pressure;
                    float abs_pressure_error = fabs(pressure_error);
                    
                    // Check if we should start active pressure correction
                    if (PRESSURE_ACTIVE_CORRECTION && 
                        abs_pressure_error > PRESSURE_DRIFT_THRESHOLD &&
                        !pressure_correction[CHx].correction_active) {
                        
                        // Start active correction
                        pressure_correction[CHx].correction_active = true;
                        pressure_correction[CHx].correction_start_time = get_time64();
                        pressure_correction[CHx].target_pressure = target_pressure;
                        pressure_correction[CHx].initial_error = pressure_error;
                    }
                    
                    // Handle active pressure correction
                    if (pressure_correction[CHx].correction_active) {
                        uint64_t correction_time = get_time64() - pressure_correction[CHx].correction_start_time;
                        
                        // Check if correction should be stopped
                        if (correction_time > PRESSURE_CORRECTION_TIMEOUT_MS ||
                            abs_pressure_error < pressure_tolerance) {
                            pressure_correction[CHx].correction_active = false;
                        } else {
                            // Apply active correction - more aggressive than normal
                            x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], target_pressure, time_E, pressure_control_enum::all);
                            x *= 1.5f; // More aggressive correction during active mode
                        }
                    } else {
                        // Normal pressure control
                        if (pressure_error < -pressure_tolerance) // Too low pressure
                        {
                            x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], target_pressure, time_E, pressure_control_enum::less_pressure);
                        }
                        else if (pressure_error > pressure_tolerance) // Too high pressure
                        {
                            x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], target_pressure, time_E, pressure_control_enum::over_pressure);
                        }
                    }
                }
            }
            else
            {
                if (motion == filament_motion_enum::filament_motion_stop) // 要求停止
                {
                    PID_speed.clear();
                    Motion_control_set_PWM(CHx, 0);
                    return;
                }
                if (motion == filament_motion_enum::filament_motion_send) // 送料中
                {
                    if (device_type == BambuBus_AMS_lite)
                    {
                        if (MC_PULL_stu_raw[CHx] < PULL_VOLTAGE_SEND_MAX) // 压力主动到这个位置
                        {
                            speed_set = 30;
                        }
                        else
                        {
                            speed_set = 10; // 原版这里是 10 - restored original working value
                        }
                    }
                    else
                    {
                        speed_set = 50; // P系全力以赴
                    }
                }
                if (motion == filament_motion_enum::filament_motion_slow_send) // 要求缓慢送料
                {
                    speed_set = 3;
                }
                if (motion == filament_motion_enum::filament_motion_pull) // 回抽
                {
                    speed_set = -50;
                }
                x = dir * PID_speed.caculate(now_speed - speed_set, time_E);
            }
        }
        else // 运行过程中耗材用完，需要停止电机控制
        {
            x = 0;
        }

        if (x > 10)
            x += pwm_zero;
        else if (x < -10)
            x -= pwm_zero;
        else
            x = 0;

        if (x > PWM_lim)
        {
            x = PWM_lim;
        }
        if (x < -PWM_lim)
        {
            x = -PWM_lim;
        }

        Motion_control_set_PWM(CHx, x);
    }
};
_MOTOR_CONTROL MOTOR_CONTROL[4] = {_MOTOR_CONTROL(0), _MOTOR_CONTROL(1), _MOTOR_CONTROL(2), _MOTOR_CONTROL(3)};

void Motion_control_set_PWM(uint8_t CHx, int PWM)//传递到硬件层控制电机的PWM
{
    uint16_t set1 = 0, set2 = 0;
    if (PWM > 0)
    {
        set1 = PWM;
    }
    else if (PWM < 0)
    {
        set2 = -PWM;
    }
    else // PWM==0
    {
        set1 = 1000;
        set2 = 1000;
    }
    switch (CHx)
    {
    case 3:
        TIM_SetCompare1(TIM2, set1);
        TIM_SetCompare2(TIM2, set2);
        break;
    case 2:
        TIM_SetCompare1(TIM3, set1);
        TIM_SetCompare2(TIM3, set2);
        break;
    case 1:
        TIM_SetCompare1(TIM4, set1);
        TIM_SetCompare2(TIM4, set2);
        break;
    case 0:
        TIM_SetCompare3(TIM4, set1);
        TIM_SetCompare4(TIM4, set2);
        break;
    }
}

int32_t as5600_distance_save[4] = {0, 0, 0, 0};
void AS5600_distance_updata()//读取as5600，更新相关的数据
{
    static uint64_t time_last = 0;
    uint64_t time_now;
    float T;
    do
    {
        time_now = get_time64();
    } while (time_now <= time_last); // T!=0
    T = (float)(time_now - time_last);
    MC_AS5600.updata_angle();
    for (int i = 0; i < 4; i++)
    {
        if ((MC_AS5600.online[i] == false))
        {
            as5600_distance_save[i] = 0;
            speed_as5600[i] = 0;
            continue;
        }

        int32_t cir_E = 0;
        int32_t last_distance = as5600_distance_save[i];
        int32_t now_distance = MC_AS5600.raw_angle[i];
        float distance_E;
        if ((now_distance > 3072) && (last_distance <= 1024))
        {
            cir_E = -4096;
        }
        else if ((now_distance <= 1024) && (last_distance > 3072))
        {
            cir_E = 4096;
        }

        distance_E = -(float)(now_distance - last_distance + cir_E) * AS5600_PI * 7.5 / 4096; // D=7.5mm，加负号是因为AS5600正对磁铁
        as5600_distance_save[i] = now_distance;

        float speedx = distance_E / T * 1000;
        // T = speed_filter_k / (T + speed_filter_k);
        speed_as5600[i] = speedx; // * (1 - T) + speed_as5600[i] * T; // mm/s
        add_filament_meters(i, distance_E / 1000);
        
        // Update automatic direction learning with movement data
        if (AUTO_DIRECTION_LEARNING_ENABLED && fabs(distance_E) > 0.1) { // Only for significant movement
            update_direction_learning(i, distance_E);
        }
    }
    time_last = time_now;
}

enum filament_now_position_enum
{
    filament_idle,
    filament_sending_out,
    filament_using,
    filament_pulling_back,
    filament_redetect,
};
int filament_now_position[4];
bool wait = false;

bool Prepare_For_filament_Pull_Back(float_t OUT_filament_meters)
{
    bool wait = false;
    for (int i = 0; i < 4; i++)
    {
        if (filament_now_position[i] == filament_pulling_back)
        {
            // DEBUG_MY("last_total_distance: "); // 输出调试信息
            // Debug_log_write_float(last_total_distance[i], 5);
            if (last_total_distance[i] < OUT_filament_meters)
            {
                // 未到达时进行退料
                MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_pull, 100); // 驱动电机退料
                // 渐变灯效
                float npercent = (last_total_distance[i] / OUT_filament_meters) * 100.0f;
                MC_STU_RGB_set(i, 255 - ((255 / 100) * npercent), 125 - ((125 / 100) * npercent), (255 / 100) * npercent);
                // 退料未完成需要优先处理
            }
            else
            {
                // 到达停止距离
                is_backing_out = false; // 无需继续记录距离
                MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_stop, 100); // 停止电机
                filament_now_position[i] = filament_idle;               // 设置当前位置为空闲
                set_filament_motion(i, AMS_filament_motion::idle);      // 强制进入空闲
                last_total_distance[i] = 0;                             // 重置退料距离
                // 退料完成
            }
            // 只要在退料状态就必须等待，直到不在退料中，下次循环后才不需要等待。
            wait = true;
        }
    }
    return wait;
}
void motor_motion_switch() // 通道状态切换函数，只控制当前在使用的通道，其他通道设置为停止
{
    int num = get_now_filament_num();                      // 当前通道号
    uint16_t device_type = get_now_BambuBus_device_type(); // 设备类型
    for (int i = 0; i < 4; i++)
    {
        if (i != num)
        {
            filament_now_position[i] = filament_idle;
            MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 1000);
        }
        else if (MC_ONLINE_key_stu[num] == 1 || MC_ONLINE_key_stu[num] == 3) // 通道有耗材丝
        {
            switch (get_filament_motion(num)) // 判断模拟器状态
            {
            case AMS_filament_motion::need_send_out: // 需要进料
                MC_STU_RGB_set(num, 00, 255, 00);
                filament_now_position[num] = filament_sending_out;
                MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_send, 100);
                
                // Start loading direction detection if we don't know the loading direction yet
                if (loading_detection[num].confirmed_loading_direction == 0 && 
                    !loading_detection[num].detection_active) {
                    start_loading_direction_detection(num);
                }
                
                // Start automatic direction learning if enabled and needed
                if (AUTO_DIRECTION_LEARNING_ENABLED && 
                    !Motion_control_data_save.auto_learned[num]) {
                    start_direction_learning(num, -1); // Feeding direction is typically negative
                }
                break;
            case AMS_filament_motion::need_pull_back:
                pull_state_old = false; // 重置标记
                is_backing_out = true; // 标记正在回退
                filament_now_position[num] = filament_pulling_back;
                if (device_type == BambuBus_AMS_lite)
                {
                    MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pull, 100);
                }
                // Prepare_For_filament_Pull_Back(OUT_filament_meters); // 通过距离控制退料是否完成
                break;
            case AMS_filament_motion::before_pull_back:
            case AMS_filament_motion::on_use:
            {
                static uint64_t time_end = 0;
                uint64_t time_now = get_time64();
                if (filament_now_position[num] == filament_sending_out) // 如果通道刚开始进料
                {
                    is_backing_out = false; // 设置无需记录距离
                    pull_state_old = true; // 首次不会往后拽，会等待触发低电压位，避免刚进入料就被拉出。
                    filament_now_position[num] = filament_using; // 标记为使用中
                    time_end = time_now + 1500;                  // 防止未被咬合, 持续进1.5秒
                }
                else if (filament_now_position[num] == filament_using) // 已经触发且处于使用中
                {
                    last_total_distance[i] = 0; // 重置退料距离
                    if (time_now > time_end)
                    {                                          // 已超1.5秒，进入通道使用 进行续料
                        MC_STU_RGB_set(num, 255, 255, 255); // 白色
                        MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_on_use, 20);
                    }
                    else
                    {                                                                  // 是否刚被检测到耗材丝
                        MC_STU_RGB_set(num, 128, 192, 128);                         // 淡绿色
                        MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_slow_send, 100); // 前1.5秒 慢速进料，辅助工具头咬合。
                    }
                }
                break;
            }
            case AMS_filament_motion::idle:
                filament_now_position[num] = filament_idle;
                MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 100);
                for (int i = 0; i < 4; i++)
                {
                    // 硬件正常
                    if (MC_ONLINE_key_stu[i] == 1 || MC_ONLINE_key_stu[i] == 0)
                    {   // 同时触发或者无耗材丝
                        MC_STU_RGB_set(i, 0, 0, 255); // 蓝色
                    }
                    else if (MC_ONLINE_key_stu[i] == 2)
                    {   // 仅外部触发
                        MC_STU_RGB_set(i, 255, 144, 0); // 橙 /像金色
                    }
                    else if (MC_ONLINE_key_stu[i] == 3)
                    {   // 仅内部触发
                        MC_STU_RGB_set(i, 0, 255, 255); // 青色
                    }
                }
                break;
            }
        }
        else if (MC_ONLINE_key_stu[num] == 0) // 0:一定没有耗材丝，1:同时触发一定有耗材丝 2:仅外部触发 3:仅内部触发，这里有防掉线功能
        {
            filament_now_position[num] = filament_idle;
            MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 100);
            // MC_STU_RGB_set(num, 0, 0, 255);
        }
    }
}
// 根据AMS模拟器的信息，来调度电机
void motor_motion_run(int error)
{
    uint64_t time_now = get_time64();
    static uint64_t time_last = 0;
    float time_E = time_now - time_last; // 获取时间差（ms）
    time_E = time_E / 1000;              // 切换到单位s
    uint16_t device_type = get_now_BambuBus_device_type();
    if (!error) // 正常模式
    {
        // 根据设备类型执行不同的电机控制逻辑
        if (device_type == BambuBus_AMS_lite)
        {
            motor_motion_switch(); // 调度电机
        }
        else if (device_type == BambuBus_AMS)
        {
            if (!Prepare_For_filament_Pull_Back(P1X_OUT_filament_meters)) // 取反(返回true)，则代表不需要优先考虑退料，并继续调度电机。
            {
                motor_motion_switch(); // 调度电机
            }
        }
    }
    else // error模式
    {
        for (int i = 0; i < 4; i++)
            MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_stop, 100); // 关闭电机
    }

    for (int i = 0; i < 4; i++)
    {
        /*if (!get_filament_online(i)) // 通道不在线则电机不允许工作
            MOTOR_CONTROL[i].set_motion(filament_motion_stop, 100);*/
        MOTOR_CONTROL[i].run(time_E); // 根据状态信息来驱动电机
        
        // Update loading direction detection
        if (loading_detection[i].detection_active) {
            update_loading_direction_detection(i);
        }

        if (MC_PULL_stu[i] == 1)
        {
            MC_PULL_ONLINE_RGB_set(i, 255, 0, 0); // 压力过大，红灯
        }
        else if (MC_PULL_stu[i] == 0)
        { // 正常压力
            if (MC_ONLINE_key_stu[i] == 1)
            { // 在线，并且双微动触发
                int filament_colors_R = channel_colors[i][0];
                int filament_colors_G = channel_colors[i][1];
                int filament_colors_B = channel_colors[i][2];
                // 根据储存的耗材丝颜色
                MC_PULL_ONLINE_RGB_set(i, filament_colors_R, filament_colors_G, filament_colors_B);
                // 亮白灯
                // MC_STU_RGB_set(i, 255, 255, 255);
            }
            else
            {
                MC_PULL_ONLINE_RGB_set(i, 0, 0, 0); // 无耗材，不亮灯
            }
        }
        else if (MC_PULL_stu[i] == -1)
        {
            MC_PULL_ONLINE_RGB_set(i, 0, 0, 255); // 压力过小，蓝灯
        }
    }
    time_last = time_now;
}
// 运动控制函数
void Motion_control_run(int error)
{
    // Enhanced pressure control timing with adaptive update rate
    bool should_update_pressure = true;
    
    if (PRESSURE_TIMING_CONTROL_ENABLED || PRESSURE_ADAPTIVE_UPDATE_RATE) {
        should_update_pressure = should_update_pressure_now();
    }
    
    // Determine system activity for adaptive update rate
    if (PRESSURE_ADAPTIVE_UPDATE_RATE) {
        bool active_feeding = false;
        bool critical_pressure = false;
        
        // Check if any channel is actively feeding or has critical pressure
        for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
            if (get_filament_motion(i) != AMS_filament_motion::idle) {
                active_feeding = true;
            }
            
            // Check for critical pressure deviation
            if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[i].is_calibrated) {
                float pressure = get_filtered_pressure(i);
                float deviation = fabsf(pressure - pressure_calibration[i].zero_point);
                if (deviation > PRESSURE_DRIFT_THRESHOLD * 2.0f) { // Critical threshold
                    critical_pressure = true;
                }
            }
        }
        
        update_system_activity_state(active_feeding, critical_pressure);
    }
    
    // Update pressure readings at controlled rate
    if (should_update_pressure) {
        MC_PULL_ONLINE_read();
        
        // Apply signal filtering to pressure readings
        if (PRESSURE_FILTERING_ENABLED) {
            for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
                float raw_pressure = MC_PULL_stu_raw[i];
                float filtered_pressure = pressure_filter_update(i, raw_pressure);
                
                // Update diagnostics with raw and filtered values
                if (PRESSURE_DIAGNOSTICS_ENABLED) {
                    pressure_diagnostics_update(i, raw_pressure, filtered_pressure);
                }
                
                // Store filtered value back for use by other systems
                MC_PULL_stu_raw[i] = filtered_pressure;
            }
        } else if (PRESSURE_DIAGNOSTICS_ENABLED) {
            // Update diagnostics even if filtering is disabled
            for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
                pressure_diagnostics_update(i, MC_PULL_stu_raw[i], MC_PULL_stu_raw[i]);
            }
        }

        // Run automatic pressure sensor calibration if enabled
        if (!error && ADAPTIVE_PRESSURE_ENABLED) {
            pressure_sensor_auto_calibrate();
        }
        
        // Run periodic diagnostics checks
        if (PRESSURE_DIAGNOSTICS_ENABLED) {
            pressure_diagnostics_check_all_channels();
        }
    }

    AS5600_distance_updata();
    for (int i = 0; i < 4; i++)
    {
        if (MC_ONLINE_key_stu[i] == 0) {
            set_filament_online(i, false);
        } else if (MC_ONLINE_key_stu[i] == 1) {
            set_filament_online(i, true);
        } else if (MC_ONLINE_key_stu[i] == 3 && filament_now_position[i] == filament_using) {
            // 如果 仅内侧触发且在使用中，先不离线
            set_filament_online(i, true);
        } else if (filament_now_position[i] == filament_redetect || (filament_now_position[i] == filament_pulling_back)) {
            // 如果 处于退料返回，或退料中，先不离线
            set_filament_online(i, true);
        } else {
            set_filament_online(i, false);
        }
    }
    /*
        如果外侧微动触发，橙/ 像金色
        如果仅内测微动触发，// 青色
        如果同时触发，空闲 = 蓝色，同时代表耗材丝上线，蓝 + 白色/通道保存色
    */

    if (error) // Error != 0
    {
        for (int i = 0; i < 4; i++)
        {
            set_filament_online(i, false);
            // filament_channel_inserted[i] = true; // 用于测试
            if (MC_ONLINE_key_stu[i] == 1)
            {                                        // 同时触发
                MC_STU_RGB_set(i, 0, 0, 255); // 蓝色
            }
            else if (MC_ONLINE_key_stu[i] == 2)
            {                                        // 仅外部触发
                MC_STU_RGB_set(i, 255, 144, 0); // 橙/ 像金色
            }
            else if (MC_ONLINE_key_stu[i] == 3)
            {                                        // 仅内部触发
                MC_STU_RGB_set(i, 0, 255, 255); // 青色
            } else if (MC_ONLINE_key_stu[i] == 0)
            {   // 未连接打印机且没有耗材丝
                MC_STU_RGB_set(i, 0, 0, 0); // 黑色
            }
        }
    } else { // 正常连接到打印机
        // 在这里设置颜色会重复修改。
        for (int i = 0; i < 4; i++)
        {
            if ((MC_AS5600.online[i] == false) || (MC_AS5600.magnet_stu[i] == -1)) // AS5600 error
            {
                set_filament_online(i, false);
                MC_STU_ERROR[i] = true;
            }
        }
    }
    motor_motion_run(error);
}
// 设置PWM驱动电机
void MC_PWM_init()
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 |
                                  GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE); // 开启复用时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // 开启TIM2时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 开启TIM3时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); // 开启TIM4时钟

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 定时器基础配置
    TIM_TimeBaseStructure.TIM_Period = 999;  // 周期（x+1）
    TIM_TimeBaseStructure.TIM_Prescaler = 1; // 预分频（x+1）
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // PWM模式配置
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; // 占空比
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure); // PA15
    TIM_OC2Init(TIM2, &TIM_OCInitStructure); // PB3
    TIM_OC1Init(TIM3, &TIM_OCInitStructure); // PB4
    TIM_OC2Init(TIM3, &TIM_OCInitStructure); // PB5
    TIM_OC1Init(TIM4, &TIM_OCInitStructure); // PB6
    TIM_OC2Init(TIM4, &TIM_OCInitStructure); // PB7
    TIM_OC3Init(TIM4, &TIM_OCInitStructure); // PB8
    TIM_OC4Init(TIM4, &TIM_OCInitStructure); // PB9

    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE);    // TIM2完全映射-CH1-PA15/CH2-PB3
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE); // TIM3部分映射-CH1-PB4/CH2-PB5
    GPIO_PinRemapConfig(GPIO_Remap_TIM4, DISABLE);       // TIM4不映射-CH1-PB6/CH2-PB7/CH3-PB8/CH4-PB9

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    TIM_CtrlPWMOutputs(TIM3, ENABLE);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
    TIM_CtrlPWMOutputs(TIM4, ENABLE);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}
// 获取PWM摩擦力零点（弃用，假设为50%占空比）
void MOTOR_get_pwm_zero()
{
    float pwm_zero[4] = {0, 0, 0, 0};
    MC_AS5600.updata_angle();

    int16_t last_angle[4];
    for (int index = 0; index < 4; index++)
    {
        last_angle[index] = MC_AS5600.raw_angle[index];
    }
    for (int pwm = 300; pwm < 1000; pwm += 10)
    {
        MC_AS5600.updata_angle();
        for (int index = 0; index < 4; index++)
        {

            if (pwm_zero[index] == 0)
            {
                if (abs(MC_AS5600.raw_angle[index] - last_angle[index]) > 50)
                {
                    pwm_zero[index] = pwm;
                    pwm_zero[index] *= 0.90;
                    Motion_control_set_PWM(index, 0);
                }
                else if ((MC_AS5600.online[index] == true))
                {
                    Motion_control_set_PWM(index, -pwm);
                }
            }
            else
            {
                Motion_control_set_PWM(index, 0);
            }
        }
        delay(100);
    }
    for (int index = 0; index < 4; index++)
    {
        Motion_control_set_PWM(index, 0);
        MOTOR_CONTROL[index].set_pwm_zero(pwm_zero[index]);
    }
}
// 将角度数值转化为角度差数值
/**
 * Initialize loading direction detection for a channel
 * Called when filament presence is first detected
 */
void start_loading_direction_detection(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    // Only start detection if we don't already know the loading direction
    if (loading_detection[channel].confirmed_loading_direction != 0) {
        return; // Already know loading direction
    }
    
    // Verify presence sensor is currently triggered
    if (MC_ONLINE_key_stu[channel] == 0) {
        return; // No filament detected, can't start detection
    }
    
    LoadingDirectionState& state = loading_detection[channel];
    
    // Initialize detection state
    memset(&state, 0, sizeof(LoadingDirectionState));
    state.detection_active = true;
    state.detection_complete = false;
    state.detection_start_time = get_time64();
    state.initial_presence = true; // We know filament is present
    state.presence_lost = false;
    state.presence_stable_phase = false;
    
    // Test current motor direction first
    state.test_direction = MOTOR_CONTROL[channel].dir;
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Starting loading direction detection for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY(" testing direction ");
    DEBUG_float(state.test_direction, 0);
    DEBUG_MY("\n");
    #else
    DEBUG_MY("Loading direction detection started for CH");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
    #endif
}

/**
 * Update loading direction detection with presence sensor feedback
 * Called during filament feeding operations
 */
void update_loading_direction_detection(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    LoadingDirectionState& state = loading_detection[channel];
    
    if (!state.detection_active || state.detection_complete) {
        return;
    }
    
    uint64_t current_time = get_time64();
    
    // Check for timeout
    if (current_time - state.detection_start_time > 3000) { // 3 second timeout
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Loading direction detection timeout for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY("\n");
        #endif
        state.detection_active = false;
        return;
    }
    
    bool current_presence = (MC_ONLINE_key_stu[channel] != 0);
    
    if (!state.presence_stable_phase) {
        // Initial phase: wait for motor to start moving and presence to stabilize
        if (current_time - state.detection_start_time > 500) { // Wait 500ms for motor to engage
            state.presence_stable_phase = true;
            state.stable_time = current_time;
        }
        return;
    }
    
    // Stable phase: monitor presence sensor for direction feedback
    if (!current_presence && state.initial_presence) {
        // Filament presence lost - this direction is UNLOADING
        state.presence_lost = true;
        
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(" direction ");
        DEBUG_float(state.test_direction, 0);
        DEBUG_MY(" is UNLOADING (presence lost)\n");
        #endif
        
        // The opposite direction should be for loading
        state.confirmed_loading_direction = -state.test_direction;
        complete_loading_direction_detection(channel);
        
    } else if (current_presence && (current_time - state.stable_time > 2000)) {
        // Filament presence maintained for 2+ seconds - this direction is LOADING
        
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(" direction ");
        DEBUG_float(state.test_direction, 0);
        DEBUG_MY(" is LOADING (presence maintained)\n");
        #endif
        
        state.confirmed_loading_direction = state.test_direction;
        complete_loading_direction_detection(channel);
    }
}

/**
 * Complete loading direction detection and update motor direction
 */
void complete_loading_direction_detection(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    LoadingDirectionState& state = loading_detection[channel];
    
    if (state.confirmed_loading_direction == 0) {
        state.detection_active = false;
        return;
    }
    
    // Update motor direction for loading operations
    int loading_dir = state.confirmed_loading_direction;
    
    // Save the loading direction (we store the loading direction, not the motor direction)
    // When we need to load: use loading_dir
    // When we need to unload: use -loading_dir
    Motion_control_data_save.Motion_control_dir[channel] = loading_dir;
    Motion_control_data_save.auto_learned[channel] = true;
    MOTOR_CONTROL[channel].dir = loading_dir;
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Loading direction detection completed for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY(": loading direction=");
    DEBUG_float(loading_dir, 0);
    DEBUG_MY("\n");
    #else
    DEBUG_MY("Loading direction learned: CH");
    DEBUG_float(channel, 0);
    DEBUG_MY(" dir=");
    DEBUG_float(loading_dir, 0);
    DEBUG_MY("\n");
    #endif
    
    // Save to flash
    Motion_control_save();
    
    state.detection_complete = true;
    state.detection_active = false;
}

/**
 * Initialize direction learning for a channel
 * Called when filament feeding begins
 */
void start_direction_learning(int channel, int commanded_direction)
{
    if (!AUTO_DIRECTION_LEARNING_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    // Validate parameters
    if (AUTO_DIRECTION_MIN_SAMPLES < 1 || AUTO_DIRECTION_MIN_SAMPLES > 50) {
        return; // Invalid configuration
    }
    if (AUTO_DIRECTION_MIN_MOVEMENT_MM < 0.1f || AUTO_DIRECTION_MIN_MOVEMENT_MM > 20.0f) {
        return; // Invalid configuration
    }
    if (AUTO_DIRECTION_CONFIDENCE_THRESHOLD < 0.5f || AUTO_DIRECTION_CONFIDENCE_THRESHOLD > 1.0f) {
        return; // Invalid configuration
    }
    
    // Only start learning if direction is unknown or was using static correction
    if (Motion_control_data_save.Motion_control_dir[channel] != 0 && 
        Motion_control_data_save.auto_learned[channel]) {
        return; // Already have a good learned direction
    }
    
    // Verify AS5600 sensor is online for this channel
    if (!MC_AS5600.online[channel]) {
        return; // Cannot learn without sensor
    }
    
    DirectionLearningState& state = direction_learning[channel];
    
    // Initialize learning state
    memset(&state, 0, sizeof(DirectionLearningState));
    state.learning_active = true;
    state.learning_complete = false;
    state.learning_start_time = get_time64();
    state.last_sample_time = state.learning_start_time;
    state.initial_position = get_filament_meters(channel);
    state.command_direction = commanded_direction;
    state.confidence_score = 0.0f;
    state.has_valid_data = false;
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Starting direction learning for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY(" with command direction ");
    DEBUG_float(commanded_direction, 0);
    DEBUG_MY("\n");
    #endif
}

/**
 * Update direction learning with movement data
 * Called during filament feeding operations
 */
void update_direction_learning(int channel, float movement_delta)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    DirectionLearningState& state = direction_learning[channel];
    
    if (!state.learning_active || state.learning_complete) {
        return;
    }
    
    uint64_t current_time = get_time64();
    
    // Check for timeout
    if (current_time - state.learning_start_time > AUTO_DIRECTION_TIMEOUT_MS) {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning timeout for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY("\n");
        #endif
        state.learning_active = false;
        return;
    }
    
    // Validate movement delta for noise
    float abs_movement = fabs(movement_delta);
    if (abs_movement > AUTO_DIRECTION_MAX_NOISE_MM * 10) {
        // Likely sensor error or mechanical issue
        state.error_count++;
        if (state.error_count > AUTO_DIRECTION_MIN_SAMPLES) {
            // Too many errors, abort learning
            state.learning_active = false;
        }
        return;
    }
    
    // Mark that we have received some sensor data
    if (abs_movement > 0.01f) {
        state.has_valid_data = true;
    }
    
    // Accumulate movement
    state.total_movement += abs_movement;
    state.accumulated_noise += (abs_movement < AUTO_DIRECTION_MAX_NOISE_MM) ? abs_movement : AUTO_DIRECTION_MAX_NOISE_MM;
    
    // Check if enough time has passed since last sample (prevent rapid sampling)
    if (current_time - state.last_sample_time < AUTO_DIRECTION_SAMPLE_INTERVAL_MS) {
        return;
    }
    
    // Check if we have sufficient movement for a sample
    if (state.total_movement >= AUTO_DIRECTION_MIN_MOVEMENT_MM) {
        
        // Validate sample quality based on noise ratio
        float noise_ratio = state.accumulated_noise / state.total_movement;
        if (noise_ratio > 0.3f) {
            // Sample too noisy, reset and try again
            state.total_movement = 0.0f;
            state.accumulated_noise = 0.0f;
            state.error_count++;
            return;
        }
        
        // Determine actual movement direction from sensor
        int actual_direction = (movement_delta > 0) ? 1 : -1;
        
        // Compare with commanded direction
        bool directions_match = (state.command_direction == actual_direction);
        
        state.sample_count++;
        state.last_sample_time = current_time;
        
        if (directions_match) {
            state.positive_samples++;
        } else {
            state.negative_samples++;
        }
        
        // Calculate confidence score
        int total_directional_samples = state.positive_samples + state.negative_samples;
        if (total_directional_samples > 0) {
            float max_samples = (float)max(state.positive_samples, state.negative_samples);
            state.confidence_score = max_samples / (float)total_directional_samples;
        }
        
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(" sample ");
        DEBUG_float(state.sample_count, 0);
        DEBUG_MY(": movement=");
        DEBUG_float(movement_delta, 3);
        DEBUG_MY(" commanded=");
        DEBUG_float(state.command_direction, 0);
        DEBUG_MY(" actual=");
        DEBUG_float(actual_direction, 0);
        DEBUG_MY(" match=");
        DEBUG_MY(directions_match ? "Y" : "N");
        DEBUG_MY(" confidence=");
        DEBUG_float(state.confidence_score, 3);
        DEBUG_MY("\n");
        #endif
        
        // Reset movement accumulator for next sample
        state.total_movement = 0.0f;
        state.accumulated_noise = 0.0f;
        
        // Check if we have enough samples and sufficient confidence to make a decision
        if (state.sample_count >= AUTO_DIRECTION_MIN_SAMPLES && 
            state.confidence_score >= AUTO_DIRECTION_CONFIDENCE_THRESHOLD) {
            complete_direction_learning(channel);
        }
    }
}

/**
 * Complete direction learning and save results
 */
void complete_direction_learning(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    DirectionLearningState& state = direction_learning[channel];
    
    if (!state.learning_active || state.sample_count < AUTO_DIRECTION_MIN_SAMPLES) {
        state.learning_active = false;
        return;
    }
    
    // Validate that we received meaningful sensor data
    if (!state.has_valid_data) {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning failed for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(": no valid sensor data\n");
        #endif
        state.learning_active = false;
        return;
    }
    
    // Require a minimum confidence level
    if (state.confidence_score < AUTO_DIRECTION_CONFIDENCE_THRESHOLD) {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning failed for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(": confidence too low: ");
        DEBUG_float(state.confidence_score, 3);
        DEBUG_MY("\n");
        #endif
        state.learning_active = false;
        return;
    }
    
    // Determine learned direction based on sample correlation
    int learned_direction = 0;
    if (state.positive_samples > state.negative_samples) {
        // Commands and actual movement correlate positively
        learned_direction = 1;
    } else if (state.negative_samples > state.positive_samples) {
        // Commands and actual movement are inverted
        learned_direction = -1;
    } else {
        // Inconclusive results
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning inconclusive for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(": equal pos/neg samples\n");
        #endif
        state.learning_active = false;
        return;
    }
    
    if (learned_direction != 0) {
        // Successfully learned direction
        Motion_control_data_save.Motion_control_dir[channel] = learned_direction;
        Motion_control_data_save.auto_learned[channel] = true;
        MOTOR_CONTROL[channel].dir = learned_direction;
        
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning completed for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY(": direction=");
        DEBUG_float(learned_direction, 0);
        DEBUG_MY(" confidence=");
        DEBUG_float(state.confidence_score, 3);
        DEBUG_MY(" samples=");
        DEBUG_float(state.sample_count, 0);
        DEBUG_MY(" pos=");
        DEBUG_float(state.positive_samples, 0);
        DEBUG_MY(" neg=");
        DEBUG_float(state.negative_samples, 0);
        DEBUG_MY("\n");
        #else
        DEBUG_MY("Auto direction learned: CH");
        DEBUG_float(channel, 0);
        DEBUG_MY(" dir=");
        DEBUG_float(learned_direction, 0);
        DEBUG_MY(" confidence=");
        DEBUG_float(state.confidence_score, 2);
        DEBUG_MY("\n");
        #endif
        
        // Save to flash
        Motion_control_save();
        
        state.learning_complete = true;
    }
    
    state.learning_active = false;
}

/**
 * Get direction learning status for diagnostics
 * Returns learning state and progress for a channel
 */
bool get_direction_learning_status(int channel, float* confidence, int* samples, bool* complete)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return false;
    }
    
    const DirectionLearningState& state = direction_learning[channel];
    
    if (confidence) *confidence = state.confidence_score;
    if (samples) *samples = state.sample_count;
    if (complete) *complete = state.learning_complete;
    
    return state.learning_active || state.learning_complete;
}

/**
 * Reset direction learning state for a specific channel
 * Useful for troubleshooting or when mechanical changes are made
 */
void reset_direction_learning(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    // Clear learning state
    memset(&direction_learning[channel], 0, sizeof(DirectionLearningState));
    
    // Reset saved direction to unknown
    Motion_control_data_save.Motion_control_dir[channel] = 0;
    Motion_control_data_save.auto_learned[channel] = false;
    
    // Update motor control
    MOTOR_CONTROL[channel].dir = 1; // Default to positive until learned
    
    // Save to flash
    Motion_control_save();
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Direction learning reset for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
    #endif
}

/**
 * Reset all learned directions - useful for complete recalibration
 */
void reset_all_learned_directions()
{
    for (int channel = 0; channel < MAX_FILAMENT_CHANNELS; channel++) {
        // Clear learning state but don't save yet
        memset(&direction_learning[channel], 0, sizeof(DirectionLearningState));
        
        // Reset saved direction to unknown
        Motion_control_data_save.Motion_control_dir[channel] = 0;
        Motion_control_data_save.auto_learned[channel] = false;
        
        // Update motor control
        MOTOR_CONTROL[channel].dir = 1; // Default to positive until learned
    }
    
    // Save all changes to flash at once
    Motion_control_save();
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("All direction learning data reset\n");
    #endif
}

// =============================================================================
// Adaptive Pressure Control Implementation
// =============================================================================

/**
 * Calibrate pressure sensor for a specific channel
 * Learns the zero point and range characteristics of the sensor
 */
void pressure_sensor_calibrate_channel(int channel)
{
    if (!ADAPTIVE_PRESSURE_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    // Only calibrate if no filament is present to get accurate zero reading
    if (MC_ONLINE_key_stu[channel] != 0) {
        return; // Filament present, cannot calibrate
    }
    
    PressureSensorCalibration& cal = pressure_calibration[channel];
    
    // Initialize calibration
    float voltage_sum = 0.0f;
    float voltage_min = 5.0f;
    float voltage_max = 0.0f;
    uint16_t samples = 0;
    uint64_t calibration_start = get_time64();
    
    DEBUG_MY("Starting pressure sensor calibration for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
    
    // Collect samples for calibration
    while (samples < PRESSURE_CALIBRATION_SAMPLES && 
           (get_time64() - calibration_start) < PRESSURE_CALIBRATION_TIME_MS) {
        
        // Ensure we still have no filament during calibration
        if (MC_ONLINE_key_stu[channel] != 0) {
            DEBUG_MY("Calibration aborted - filament detected\n");
            return;
        }
        
        float voltage = MC_PULL_stu_raw[channel];
        
        // Validate reading is within reasonable bounds
        if (voltage >= PRESSURE_RANGE_MIN_VOLTAGE && voltage <= PRESSURE_RANGE_MAX_VOLTAGE) {
            voltage_sum += voltage;
            voltage_min = min(voltage_min, voltage);
            voltage_max = max(voltage_max, voltage);
            samples++;
        }
        
        delay(50); // Sample every 50ms
    }
    
    if (samples < (PRESSURE_CALIBRATION_SAMPLES / 2)) {
        DEBUG_MY("Calibration failed - insufficient samples\n");
        return;
    }
    
    // Calculate zero point (average of no-load readings)
    cal.zero_point = voltage_sum / samples;
    
    // Estimate range based on sensor type and observed variation
    float observed_variation = voltage_max - voltage_min;
    
    // Use a conservative estimate for range if we only saw small variation
    if (observed_variation < PRESSURE_ZERO_TOLERANCE) {
        // Assume typical sensor range around zero point
        cal.positive_range = 0.8f; // Typical range above zero
        cal.negative_range = 0.8f; // Typical range below zero
    } else {
        // Use larger range based on observed variation
        cal.positive_range = max(0.5f, observed_variation * 4.0f);
        cal.negative_range = max(0.5f, observed_variation * 4.0f);
    }
    
    // Calculate dynamic thresholds based on learned characteristics
    float deadband_range = min(cal.positive_range, cal.negative_range) * PRESSURE_DEADBAND_SCALE;
    cal.deadband_low = cal.zero_point - deadband_range;
    cal.deadband_high = cal.zero_point + deadband_range;
    
    // Mark as calibrated
    cal.calibration_samples = samples;
    cal.is_calibrated = true;
    cal.last_calibration_time = get_time64();
    
    // Copy to save structure
    Motion_control_data_save.pressure_cal[channel] = cal;
    
    DEBUG_MY("Pressure calibration complete for CH");
    DEBUG_float(channel, 0);
    DEBUG_MY(": zero=");
    DEBUG_float(cal.zero_point, 3);
    DEBUG_MY("V, range=");
    DEBUG_float(cal.positive_range, 3);
    DEBUG_MY("V, deadband=");
    DEBUG_float(cal.deadband_low, 3);
    DEBUG_MY("-");
    DEBUG_float(cal.deadband_high, 3);
    DEBUG_MY("V\n");
}

// =============================================================================
// Advanced Signal Filtering Functions
// =============================================================================

/**
 * Initialize pressure signal filtering system
 */
void pressure_filter_init()
{
    if (!PRESSURE_FILTERING_ENABLED) {
        return;
    }
    
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        memset(&pressure_filter[i], 0, sizeof(PressureFilterState));
        pressure_filter[i].filtered_value = 0.0f;
        pressure_filter[i].lowpass_value = 0.0f;
        pressure_filter[i].is_stable = false;
    }
}

/**
 * Update pressure filter with new raw reading
 * @param channel Channel number (0-3)
 * @param raw_value Raw pressure reading in volts
 * @return Filtered pressure value
 */
float pressure_filter_update(int channel, float raw_value)
{
    if (!PRESSURE_FILTERING_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return raw_value;
    }
    
    PressureFilterState* filter = &pressure_filter[channel];
    
    // Outlier rejection - compare against current filtered value
    if (PRESSURE_OUTLIER_REJECTION && filter->sample_count >= 3) {
        float deviation = fabsf(raw_value - filter->filtered_value);
        float sensor_range = 3.0f; // Default range, use calibrated range if available
        
        if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[channel].is_calibrated) {
            sensor_range = pressure_calibration[channel].positive_range + pressure_calibration[channel].negative_range;
        }
        
        float outlier_threshold = sensor_range * PRESSURE_OUTLIER_THRESHOLD;
        
        if (deviation > outlier_threshold) {
            // Reject outlier, return previous filtered value
            return filter->filtered_value;
        }
    }
    
    // Add to moving average buffer
    filter->filter_buffer[filter->buffer_index] = raw_value;
    filter->buffer_index = (filter->buffer_index + 1) % PRESSURE_FILTER_WINDOW_SIZE;
    
    if (filter->sample_count < PRESSURE_FILTER_WINDOW_SIZE) {
        filter->sample_count++;
    }
    
    // Calculate moving average
    float sum = 0.0f;
    for (int i = 0; i < filter->sample_count; i++) {
        sum += filter->filter_buffer[i];
    }
    float moving_average = sum / filter->sample_count;
    
    // Apply low-pass filter
    if (filter->sample_count == 1) {
        filter->lowpass_value = moving_average;
    } else {
        filter->lowpass_value = PRESSURE_LOWPASS_FILTER_ALPHA * moving_average + 
                               (1.0f - PRESSURE_LOWPASS_FILTER_ALPHA) * filter->lowpass_value;
    }
    
    filter->filtered_value = filter->lowpass_value;
    
    // Update stability detection
    float change = fabsf(raw_value - filter->last_raw_value);
    if (change < 0.01f) { // Less than 10mV change
        filter->stability_counter++;
        if (filter->stability_counter >= 5) {
            filter->is_stable = true;
        }
    } else {
        filter->stability_counter = 0;
        filter->is_stable = false;
    }
    
    filter->last_raw_value = raw_value;
    return filter->filtered_value;
}

/**
 * Reset pressure filter for specific channel
 */
void pressure_filter_reset_channel(int channel)
{
    if (!PRESSURE_FILTERING_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    memset(&pressure_filter[channel], 0, sizeof(PressureFilterState));
}

/**
 * Reset all pressure filters
 */
void pressure_filter_reset_all()
{
    if (!PRESSURE_FILTERING_ENABLED) {
        return;
    }
    
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        pressure_filter_reset_channel(i);
    }
}

/**
 * Get current filtered pressure value
 */
float get_filtered_pressure(int channel)
{
    if (!PRESSURE_FILTERING_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return MC_PULL_stu_raw[channel];
    }
    
    return pressure_filter[channel].filtered_value;
}

/**
 * Check if pressure reading is stable
 */
bool is_pressure_reading_stable(int channel)
{
    if (!PRESSURE_FILTERING_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return false;
    }
    
    return pressure_filter[channel].is_stable;
}

// =============================================================================
// Dynamic Update Rate Control Functions
// =============================================================================

/**
 * Initialize dynamic update rate control system
 */
void pressure_update_rate_init()
{
    if (!PRESSURE_ADAPTIVE_UPDATE_RATE) {
        return;
    }
    
    memset(&pressure_update_rate, 0, sizeof(PressureUpdateRateState));
    pressure_update_rate.current_interval_ms = PRESSURE_UPDATE_INTERVAL_MS;
    pressure_update_rate.last_update_time = get_time64();
}

/**
 * Get current adaptive update interval
 */
uint32_t get_adaptive_update_interval_ms()
{
    if (!PRESSURE_ADAPTIVE_UPDATE_RATE) {
        return PRESSURE_UPDATE_INTERVAL_MS;
    }
    
    return pressure_update_rate.current_interval_ms;
}

/**
 * Update system activity state for adaptive update rate
 */
void update_system_activity_state(bool active_feeding, bool critical_pressure)
{
    if (!PRESSURE_ADAPTIVE_UPDATE_RATE) {
        return;
    }
    
    pressure_update_rate.active_feeding = active_feeding;
    pressure_update_rate.critical_pressure = critical_pressure;
    
    // Determine appropriate update interval
    if (critical_pressure) {
        pressure_update_rate.current_interval_ms = PRESSURE_UPDATE_RATE_CRITICAL_MS;
        pressure_update_rate.idle_counter = 0;
    } else if (active_feeding) {
        pressure_update_rate.current_interval_ms = PRESSURE_UPDATE_RATE_ACTIVE_MS;
        pressure_update_rate.idle_counter = 0;
    } else {
        // Gradually transition to idle rate
        pressure_update_rate.idle_counter++;
        if (pressure_update_rate.idle_counter >= 10) { // After 10 inactive cycles
            pressure_update_rate.current_interval_ms = PRESSURE_UPDATE_RATE_IDLE_MS;
        }
    }
}

/**
 * Check if pressure should be updated now
 */
bool should_update_pressure_now()
{
    if (!PRESSURE_TIMING_CONTROL_ENABLED && !PRESSURE_ADAPTIVE_UPDATE_RATE) {
        return true;
    }
    
    uint64_t current_time = get_time64();
    uint32_t interval_ms = PRESSURE_ADAPTIVE_UPDATE_RATE ? 
                          get_adaptive_update_interval_ms() : 
                          PRESSURE_UPDATE_INTERVAL_MS;
    
    bool should_update = (current_time - pressure_update_rate.last_update_time) >= interval_ms;
    
    if (should_update) {
        pressure_update_rate.last_update_time = current_time;
    }
    
    return should_update;
}

// =============================================================================
// Pressure Diagnostics and Health Monitoring Functions
// =============================================================================

/**
 * Initialize pressure diagnostics and health monitoring system
 */
void pressure_diagnostics_init()
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED) {
        return;
    }
    
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        memset(&pressure_diagnostics[i], 0, sizeof(PressureDiagnosticsState));
        pressure_diagnostics[i].sensor_healthy = true;
        pressure_diagnostics[i].last_diagnostic_time = get_time64();
        
        // Initialize with current calibrated zero point if available
        if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[i].is_calibrated) {
            pressure_diagnostics[i].initial_zero_point = pressure_calibration[i].zero_point;
        }
    }
}

/**
 * Update diagnostics with new pressure reading
 * @param channel Channel number (0-3)
 * @param raw_value Raw pressure reading
 * @param filtered_value Filtered pressure reading
 */
void pressure_diagnostics_update(int channel, float raw_value, float filtered_value)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    PressureDiagnosticsState* diag = &pressure_diagnostics[channel];
    uint64_t current_time = get_time64();
    
    // Update measurement count
    diag->measurement_count++;
    
    // Check for stuck sensor - compare with recent readings
    bool is_stuck = true;
    diag->last_readings[diag->reading_index] = raw_value;
    
    for (int i = 1; i < 5; i++) {
        int idx = (diag->reading_index - i + 5) % 5;
        if (fabsf(raw_value - diag->last_readings[idx]) > 0.005f) { // 5mV tolerance
            is_stuck = false;
            break;
        }
    }
    
    if (is_stuck && diag->measurement_count >= 5) {
        diag->identical_count++;
        if (diag->identical_count >= PRESSURE_STUCK_READING_THRESHOLD) {
            diag->stuck_warning = true;
        }
    } else {
        diag->identical_count = 0;
        diag->stuck_warning = false;
    }
    
    diag->reading_index = (diag->reading_index + 1) % 5;
    
    // Calculate pressure error if calibrated
    if (ADAPTIVE_PRESSURE_ENABLED && pressure_calibration[channel].is_calibrated) {
        float error = fabsf(filtered_value - pressure_calibration[channel].zero_point);
        diag->total_error += error;
        
        if (error > diag->max_error) {
            diag->max_error = error;
        }
        
        // Monitor sensor drift
        if (PRESSURE_DRIFT_MONITORING) {
            diag->current_drift = fabsf(pressure_calibration[channel].zero_point - diag->initial_zero_point);
            
            if (diag->current_drift > PRESSURE_DRIFT_WARNING_THRESHOLD) {
                diag->drift_warning = true;
            }
        }
    }
    
    // Update overall health status
    diag->sensor_healthy = !diag->stuck_warning && !diag->drift_warning;
    
    // Perform periodic diagnostic checks (every 5 seconds)
    if (current_time - diag->last_diagnostic_time >= 5000) {
        diag->last_diagnostic_time = current_time;
        
        if (!diag->sensor_healthy && PRESSURE_FAULT_DETECTION) {
            DEBUG_MY("Pressure sensor fault detected on channel ");
            DEBUG_float(channel, 0);
            if (diag->stuck_warning) DEBUG_MY(" (stuck)");
            if (diag->drift_warning) DEBUG_MY(" (drift)");
            DEBUG_MY("\n");
            
            diag->last_fault_time = current_time;
        }
    }
}

/**
 * Check if pressure sensor is healthy
 */
bool is_pressure_sensor_healthy(int channel)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return true; // Default to healthy if diagnostics disabled
    }
    
    return pressure_diagnostics[channel].sensor_healthy;
}

/**
 * Check if pressure sensor appears stuck
 */
bool is_pressure_sensor_stuck(int channel)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return false;
    }
    
    return pressure_diagnostics[channel].stuck_warning;
}

/**
 * Get current sensor drift amount
 */
float get_pressure_sensor_drift(int channel)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || !PRESSURE_DRIFT_MONITORING || 
        channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return 0.0f;
    }
    
    return pressure_diagnostics[channel].current_drift;
}

/**
 * Get performance metrics for a channel
 */
void get_pressure_performance_metrics(int channel, float* avg_error, float* max_error, uint32_t* correction_count)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || !PRESSURE_PERFORMANCE_METRICS || 
        channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        if (avg_error) *avg_error = 0.0f;
        if (max_error) *max_error = 0.0f;
        if (correction_count) *correction_count = 0;
        return;
    }
    
    PressureDiagnosticsState* diag = &pressure_diagnostics[channel];
    
    if (avg_error) {
        *avg_error = diag->measurement_count > 0 ? diag->total_error / diag->measurement_count : 0.0f;
    }
    if (max_error) {
        *max_error = diag->max_error;
    }
    if (correction_count) {
        *correction_count = diag->correction_count;
    }
}

/**
 * Reset diagnostics for a specific channel
 */
void reset_pressure_diagnostics(int channel)
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    memset(&pressure_diagnostics[channel], 0, sizeof(PressureDiagnosticsState));
    pressure_diagnostics[channel].sensor_healthy = true;
    pressure_diagnostics[channel].last_diagnostic_time = get_time64();
}

/**
 * Check diagnostics for all channels and log any issues
 */
void pressure_diagnostics_check_all_channels()
{
    if (!PRESSURE_DIAGNOSTICS_ENABLED) {
        return;
    }
    
    static uint64_t last_check = 0;
    uint64_t current_time = get_time64();
    
    // Check every 10 seconds
    if (current_time - last_check < 10000) {
        return;
    }
    last_check = current_time;
    
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        if (!is_pressure_sensor_healthy(i)) {
            float avg_error, max_error;
            uint32_t corrections;
            get_pressure_performance_metrics(i, &avg_error, &max_error, &corrections);
            
            DEBUG_MY("CH");
            DEBUG_float(i, 0);
            DEBUG_MY(" health: avg_err=");
            DEBUG_float(avg_error, 3);
            DEBUG_MY("V, max_err=");
            DEBUG_float(max_error, 3);
            DEBUG_MY("V, drift=");
            DEBUG_float(get_pressure_sensor_drift(i), 3);
            DEBUG_MY("V\n");
        }
    }
}

/**
 * Automatically calibrate pressure sensors during idle periods
 * Called periodically during system operation
 */
void pressure_sensor_auto_calibrate()
{
    if (!ADAPTIVE_PRESSURE_ENABLED || !PRESSURE_AUTO_RECALIBRATION) {
        return;
    }
    
    static uint64_t last_auto_calibration = 0;
    uint64_t current_time = get_time64();
    
    // Only attempt auto-calibration every 30 seconds
    if (current_time - last_auto_calibration < 30000) {
        return;
    }
    
    last_auto_calibration = current_time;
    
    // Check each channel for calibration needs
    for (int channel = 0; channel < MAX_FILAMENT_CHANNELS; channel++) {
        PressureSensorCalibration& cal = pressure_calibration[channel];
        
        // Skip if recently calibrated (within 5 minutes)
        if (cal.is_calibrated && 
            (current_time - cal.last_calibration_time) < 300000) {
            continue;
        }
        
        // Only calibrate if channel is idle and no filament present
        if (filament_now_position[channel] == filament_idle && 
            MC_ONLINE_key_stu[channel] == 0) {
            pressure_sensor_calibrate_channel(channel);
        }
    }
}

/**
 * Reset calibration data for a specific channel
 */
void pressure_sensor_reset_calibration(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    PressureSensorCalibration& cal = pressure_calibration[channel];
    memset(&cal, 0, sizeof(PressureSensorCalibration));
    
    // Set defaults
    cal.zero_point = 1.65f; // Conservative default around middle voltage
    cal.positive_range = 0.6f;
    cal.negative_range = 0.6f;
    cal.deadband_low = PULL_VOLTAGE_LOW;   // Fall back to static values
    cal.deadband_high = PULL_VOLTAGE_HIGH;
    cal.is_calibrated = false;
    
    Motion_control_data_save.pressure_cal[channel] = cal;
    
    DEBUG_MY("Pressure calibration reset for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
}

/**
 * Get dynamic high pressure threshold for a channel
 */
float get_dynamic_pressure_threshold_high(int channel)
{
    if (!ADAPTIVE_PRESSURE_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return PULL_VOLTAGE_HIGH; // Fall back to static threshold
    }
    
    // Check if adaptive control is disabled for this channel
    if (PRESSURE_ADAPTIVE_DISABLE_MASK & (1 << channel)) {
        return PULL_VOLTAGE_HIGH; // Fall back to static threshold
    }
    
    const PressureSensorCalibration& cal = pressure_calibration[channel];
    
    if (!cal.is_calibrated) {
        return PULL_VOLTAGE_HIGH; // Fall back to static threshold
    }
    
    return cal.zero_point + (cal.positive_range * PRESSURE_HIGH_THRESHOLD_SCALE);
}

/**
 * Get dynamic low pressure threshold for a channel
 */
float get_dynamic_pressure_threshold_low(int channel)
{
    if (!ADAPTIVE_PRESSURE_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return PULL_VOLTAGE_LOW; // Fall back to static threshold
    }
    
    // Check if adaptive control is disabled for this channel
    if (PRESSURE_ADAPTIVE_DISABLE_MASK & (1 << channel)) {
        return PULL_VOLTAGE_LOW; // Fall back to static threshold
    }
    
    const PressureSensorCalibration& cal = pressure_calibration[channel];
    
    if (!cal.is_calibrated) {
        return PULL_VOLTAGE_LOW; // Fall back to static threshold
    }
    
    return cal.zero_point - (cal.negative_range * PRESSURE_LOW_THRESHOLD_SCALE);
}

/**
 * Check if pressure reading is within acceptable deadband around zero
 */
bool is_pressure_in_deadband(int channel, float pressure)
{
    if (!ADAPTIVE_PRESSURE_ENABLED || channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        // Fall back to static deadband check
        return (pressure >= PULL_VOLTAGE_LOW && pressure <= PULL_VOLTAGE_HIGH);
    }
    
    const PressureSensorCalibration& cal = pressure_calibration[channel];
    
    if (!cal.is_calibrated) {
        // Fall back to static deadband check
        return (pressure >= PULL_VOLTAGE_LOW && pressure <= PULL_VOLTAGE_HIGH);
    }
    
    return (pressure >= cal.deadband_low && pressure <= cal.deadband_high);
}

/**
 * Reset calibration for all channels - useful for complete recalibration
 */
void pressure_sensor_reset_all_calibration()
{
    if (!ADAPTIVE_PRESSURE_ENABLED) {
        return;
    }
    
    for (int channel = 0; channel < MAX_FILAMENT_CHANNELS; channel++) {
        pressure_sensor_reset_calibration(channel);
    }
    
    // Save all changes to flash
    Motion_control_save();
    
    DEBUG_MY("All pressure sensor calibrations reset\n");
}

/**
 * Force immediate calibration of all channels (for testing/debugging)
 * Note: This will only calibrate channels with no filament present
 */
void pressure_sensor_force_calibrate_all()
{
    if (!ADAPTIVE_PRESSURE_ENABLED) {
        return;
    }
    
    DEBUG_MY("Force calibrating all pressure sensors...\n");
    
    for (int channel = 0; channel < MAX_FILAMENT_CHANNELS; channel++) {
        // Only calibrate if no filament is present
        if (MC_ONLINE_key_stu[channel] == 0) {
            pressure_sensor_calibrate_channel(channel);
            delay(100); // Small delay between calibrations
        } else {
            DEBUG_MY("Skipping CH");
            DEBUG_float(channel, 0);
            DEBUG_MY(" - filament present\n");
        }
    }
    
    // Save all calibrations to flash
    Motion_control_save();
    
    DEBUG_MY("Force calibration complete\n");
}

// Convert angle difference, handling wraparound
int M5600_angle_dis(int16_t angle1, int16_t angle2)
{
    int cir_E = angle1 - angle2;
    if ((angle1 > 3072) && (angle2 <= 1024))
    {
        cir_E = -4096;
    }
    else if ((angle1 <= 1024) && (angle2 > 3072))
    {
        cir_E = 4096;
    }
    return cir_E;
}

// 测试电机运动方向（启动时调用，现在作为自动学习的备用方案）
void MOTOR_get_dir()
{
    int dir[4] = {0, 0, 0, 0};
    bool done = false;
    bool have_data = Motion_control_read();
    if (!have_data)
    {
        for (int index = 0; index < 4; index++)
        {
            Motion_control_data_save.Motion_control_dir[index] = 0;
            Motion_control_data_save.auto_learned[index] = false;
        }
    }
    
    #if AUTO_DIRECTION_LEARNING_ENABLED
    // Initialize direction learning states
    for (int index = 0; index < 4; index++)
    {
        memset(&direction_learning[index], 0, sizeof(DirectionLearningState)); // Properly initialize to zero
        memset(&loading_detection[index], 0, sizeof(LoadingDirectionState)); // Initialize loading detection
    }
    #endif
    
    MC_AS5600.updata_angle(); //读取5600的初始角度值

    int16_t last_angle[4];
    for (int index = 0; index < 4; index++)
    {
        last_angle[index] = MC_AS5600.raw_angle[index];                  //将初始角度值记录下来
        dir[index] = Motion_control_data_save.Motion_control_dir[index]; //记录flash中的dir数据
    }
    
    // If automatic learning is enabled, skip startup calibration for channels that don't have learned directions
    bool need_startup_calibration = false;
    if (AUTO_DIRECTION_LEARNING_ENABLED) {
        for (int index = 0; index < 4; index++) {
            if ((MC_AS5600.online[index] == true) && 
                (Motion_control_data_save.Motion_control_dir[index] == 0 || 
                 !Motion_control_data_save.auto_learned[index])) {
                // This channel needs direction detection but will use automatic learning
                // Set a temporary direction for now
                dir[index] = 1; // Default positive direction, will be corrected during operation
            }
        }
    } else {
        // Auto-learning disabled, perform traditional startup calibration
        need_startup_calibration = true;
    }
    
    bool need_save = false; // 是否需要更新状态
    
    if (need_startup_calibration) {
        for (int index = 0; index < 4; index++)
        {
            if ((MC_AS5600.online[index] == true)) // 有5600，说明通道在线
            {
                if (Motion_control_data_save.Motion_control_dir[index] == 0) // 之前测试结果为0，需要测试
                {
                    Motion_control_set_PWM(index, 1000); // 打开电机
                    need_save = true;                    // 有状态更新
                }
            }
            else
            {
                dir[index] = 0;   // 通道不在线，清空它的方向数据
                need_save = true; // 有状态更新
            }
        }
        
        int i = 0;
        while (done == false)
        {
            done = true;

            delay(10);                // 间隔10ms检测一次
            MC_AS5600.updata_angle(); // 更新角度数据

            if (i++ > 200) // 超过2s无响应
            {
                for (int index = 0; index < 4; index++)
                {
                    Motion_control_set_PWM(index, 0);                       // 停止
                    Motion_control_data_save.Motion_control_dir[index] = 0; // 方向设为0
                }
                break; // 跳出循环
            }
            for (int index = 0; index < 4; index++) // 遍历
            {
                if ((MC_AS5600.online[index] == true) && (Motion_control_data_save.Motion_control_dir[index] == 0)) // 对于新的通道
                {
                    int angle_dis = M5600_angle_dis(MC_AS5600.raw_angle[index], last_angle[index]);
                    if (abs(angle_dis) > 163) // 移动超过1mm
                    {
                        Motion_control_set_PWM(index, 0); // 停止
                        if (angle_dis > 0)                // 这里AS600正对着磁铁，和背贴方向是反的
                        {
                            dir[index] = 1;
                        }
                        else
                        {
                            dir[index] = -1;
                        }
                    }
                    else
                    {
                        done = false; // 没有移动。继续等待
                    }
                }
            }
        }
        
        // Apply static motor direction corrections when auto-learning is disabled
        static const bool motor_dir_correction[4] = {
            MOTOR_DIR_CORRECTION_CH0,  // Channel 0
            MOTOR_DIR_CORRECTION_CH1,  // Channel 1 (commonly reversed)
            MOTOR_DIR_CORRECTION_CH2,  // Channel 2 (commonly reversed)
            MOTOR_DIR_CORRECTION_CH3   // Channel 3 (sometimes reversed)
        };
        
        for (int index = 0; index < 4; index++) // 遍历四个电机
        {
            // Apply direction correction if needed for this channel
            if (motor_dir_correction[index] && dir[index] != 0) {
                dir[index] = -dir[index]; // Invert the detected direction
            }
            Motion_control_data_save.Motion_control_dir[index] = dir[index]; // 数据复制
            Motion_control_data_save.auto_learned[index] = false; // Mark as static correction
        }
    }
    
    if (need_save) // 如果需要保存数据
    {
        Motion_control_save(); // 数据保存
    }
}
// 初始化电机
// 指定方向
int first_boot = 1; // 1 表示第一次启动，用于执行仅启动时.
void set_motor_directions(int dir0, int dir1, int dir2, int dir3)
{
    Motion_control_data_save.Motion_control_dir[0] = dir0;
    Motion_control_data_save.Motion_control_dir[1] = dir1;
    Motion_control_data_save.Motion_control_dir[2] = dir2;
    Motion_control_data_save.Motion_control_dir[3] = dir3;

    Motion_control_save(); // 保存到闪存
}
void MOTOR_init()
{

    MC_PWM_init();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    MC_AS5600.init(AS5600_SCL, AS5600_SDA, MAX_FILAMENT_CHANNELS);
    // MOTOR_get_pwm_zero();
    // 自动方向检测（包含硬件差异修正）
    MOTOR_get_dir();

    // 固定电机方向用 - 仅在需要覆盖自动检测时使用
    // 注意：现在自动检测已包含对通道1和2的方向修正
    if (first_boot == 1)
    { // 首次启动
        // set_motor_directions(1 , 1 , 1 , 1 ); // 1为正转 -1为反转
        // 如果自动检测+修正仍有问题，可取消注释上行并调整方向值
        
        // To reset all learned directions, uncomment the next line:
        // reset_all_learned_directions(); // Clears all saved directions and forces relearning
        
        first_boot = 0;
    }
    for (int index = 0; index < 4; index++)
    {
        Motion_control_set_PWM(index, 0);
        MOTOR_CONTROL[index].set_pwm_zero(500);
        // Ensure motor direction is never zero to prevent complete motor failure
        int motor_dir = Motion_control_data_save.Motion_control_dir[index];
        if (motor_dir == 0) {
            // If no direction is saved, use default direction for all channels
            motor_dir = 1; // Default positive direction for ALL channels
            // Save the default direction for future use
            Motion_control_data_save.Motion_control_dir[index] = motor_dir;
            Motion_control_data_save.auto_learned[index] = false;
        }
        MOTOR_CONTROL[index].dir = motor_dir;
    }
    
    // Debug: Print motor directions being initialized
    DEBUG_MY("Motor directions set: CH0=");
    DEBUG_MY(MOTOR_CONTROL[0].dir > 0 ? "+" : "-");
    DEBUG_MY(" CH1=");  
    DEBUG_MY(MOTOR_CONTROL[1].dir > 0 ? "+" : "-");
    DEBUG_MY(" CH2=");
    DEBUG_MY(MOTOR_CONTROL[2].dir > 0 ? "+" : "-");
    DEBUG_MY(" CH3=");
    DEBUG_MY(MOTOR_CONTROL[3].dir > 0 ? "+" : "-");
    DEBUG_MY("\n");
    
    // Save any updated motor directions to flash
    Motion_control_save();
    
    // Validate that all motor directions are properly set (never zero)
    for (int i = 0; i < 4; i++)
    {
        if (MOTOR_CONTROL[i].dir == 0) {
            // This should never happen after our fix, but add safety check
            MOTOR_CONTROL[i].dir = 1; // Use same default for all channels
            Motion_control_data_save.Motion_control_dir[i] = MOTOR_CONTROL[i].dir;
        }
    }
    
    MC_AS5600.updata_angle();
    for (int i = 0; i < 4; i++)
    {
        as5600_distance_save[i] = MC_AS5600.raw_angle[i];
    }
}
extern void RGB_update();
void Motion_control_init() // 初始化所有运动和传感器
{
    MC_PULL_ONLINE_init();
    MC_PULL_ONLINE_read();
    MOTOR_init();
    
    // Initialize pressure calibration system
    if (ADAPTIVE_PRESSURE_ENABLED) {
        // Load calibration data from flash or initialize defaults
        for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
            if (Motion_control_data_save.pressure_cal[i].is_calibrated) {
                pressure_calibration[i] = Motion_control_data_save.pressure_cal[i];
                DEBUG_MY("Loaded pressure calibration for CH");
                DEBUG_float(i, 0);
                DEBUG_MY(": zero=");
                DEBUG_float(pressure_calibration[i].zero_point, 3);
                DEBUG_MY("V\n");
            } else {
                pressure_sensor_reset_calibration(i);
            }
            
            // Initialize pressure correction state
            memset(&pressure_correction[i], 0, sizeof(PressureCorrectionState));
        }
        
        // Initialize advanced signal filtering
        if (PRESSURE_FILTERING_ENABLED) {
            pressure_filter_init();
        }
        
        // Initialize dynamic update rate control
        if (PRESSURE_ADAPTIVE_UPDATE_RATE) {
            pressure_update_rate_init();
        }
        
        // Initialize diagnostics and health monitoring
        if (PRESSURE_DIAGNOSTICS_ENABLED) {
            pressure_diagnostics_init();
        }
    }
    
    /*
    //这是一段阻塞的DEBUG代码
    while (1)
    {
        delay(10);
        MC_PULL_ONLINE_read();

        for (int i = 0; i < 4; i++)
        {
            MOTOR_CONTROL[i].set_motion(filament_motion_pressure_ctrl_on_use, 100);
            if (!get_filament_online(i)) // 通道不在线则电机不允许工作
                MOTOR_CONTROL[i].set_motion(filament_motion_stop, 100);
            MOTOR_CONTROL[i].run(0); // 根据状态信息来驱动电机
        }
        char s[100];
        int n = sprintf(s, "%d\n", (int)(MC_PULL_stu_raw[3] * 1000));
        DEBUG_num(s, n);
    }*/

    for (int i = 0; i < 4; i++)
    {
        // if(MC_AS5600.online[i])//用AS5600是否有信号来判断通道是否插入
        // {
        //     filament_channel_inserted[i]=true;
        // }
        // else
        // {
        //     filament_channel_inserted[i]=false;
        // }
        filament_now_position[i] = filament_idle;//将通道初始状态设置为空闲
    }
}
