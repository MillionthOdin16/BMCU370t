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
 * Read ADC values for all channels and update sensor states with enhanced robustness
 */
void MC_PULL_ONLINE_read()
{
    float *data = ADC_DMA_get_value();
    bool *adc_health = ADC_DMA_get_health_status();
    uint32_t *fault_counts = ADC_DMA_get_fault_counts();
    
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

    // Process sensor readings for each channel with fault tolerance
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++)
    {
        // Check sensor health for pull and presence sensors
        int pull_adc_index = (3 - i) * 2;      // ADC channel for pull sensor
        int presence_adc_index = pull_adc_index + 1; // ADC channel for presence sensor
        
        bool pull_sensor_healthy = (pull_adc_index < 8) ? adc_health[pull_adc_index] : false;
        bool presence_sensor_healthy = (presence_adc_index < 8) ? adc_health[presence_adc_index] : false;
        
        // Enhanced pressure sensing with hysteresis for noise immunity
        if (pull_sensor_healthy) {
            // Use hysteresis to prevent oscillation at thresholds
            static int MC_PULL_stu_prev[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};
            
            if (MC_PULL_stu_raw[i] > PULL_voltage_up + 0.05f) { // Add 50mV hysteresis
                MC_PULL_stu[i] = 1;  // High pressure
            } else if (MC_PULL_stu_raw[i] < PULL_voltage_down - 0.05f) { // Add 50mV hysteresis  
                MC_PULL_stu[i] = -1; // Low pressure
            } else if (MC_PULL_stu_raw[i] > PULL_voltage_up - 0.05f && MC_PULL_stu_prev[i] == 1) {
                MC_PULL_stu[i] = 1;  // Stay high due to hysteresis
            } else if (MC_PULL_stu_raw[i] < PULL_voltage_down + 0.05f && MC_PULL_stu_prev[i] == -1) {
                MC_PULL_stu[i] = -1; // Stay low due to hysteresis
            } else {
                MC_PULL_stu[i] = 0;  // Normal range
            }
            
            MC_PULL_stu_prev[i] = MC_PULL_stu[i];
        } else {
            // Sensor faulty - maintain previous state or set to safe default
            DEBUG_MY("Pull sensor channel ");
            DEBUG_num("", i);
            DEBUG_MY(" faulty, faults: ");
            DEBUG_num("", fault_counts[pull_adc_index]);
            DEBUG_MY("\n");
            
            // Set to safe state when sensor is faulty
            MC_PULL_stu[i] = 0;  // Normal pressure when sensor unavailable
        }

        // Enhanced presence detection with fault tolerance
        if (presence_sensor_healthy) {
            if (is_two == false)
            {
                // Single micro-switch mode with improved thresholds
                if (MC_ONLINE_key_stu_raw[i] > 1.7f) { // Increased threshold for better noise immunity
                    MC_ONLINE_key_stu[i] = 1;
                } else if (MC_ONLINE_key_stu_raw[i] < 1.6f) { // Hysteresis
                    MC_ONLINE_key_stu[i] = 0;
                }
                // Maintain current state if in hysteresis zone
            }
            else
            {
                // Dual micro-switch mode with enhanced logic
                if (MC_ONLINE_key_stu_raw[i] < 0.5f) { // Lower threshold for better detection
                    MC_ONLINE_key_stu[i] = 0; // Offline
                } else if ((MC_ONLINE_key_stu_raw[i] < 1.75f) && (MC_ONLINE_key_stu_raw[i] > 1.35f)) {
                    MC_ONLINE_key_stu[i] = 2; // Outer switch only - needs assist
                } else if (MC_ONLINE_key_stu_raw[i] > 1.75f) {
                    MC_ONLINE_key_stu[i] = 1; // Both switches - fully online
                } else if (MC_ONLINE_key_stu_raw[i] < 1.35f) {
                    MC_ONLINE_key_stu[i] = 3; // Inner switch only - needs verification
                }
            }
        } else {
            // Presence sensor faulty - use fallback logic
            DEBUG_MY("Presence sensor channel ");
            DEBUG_num("", i);
            DEBUG_MY(" faulty, maintaining previous state\n");
            
            // Keep previous state when sensor is faulty to avoid false triggers
            // This prevents incorrect feeding operations due to sensor failures
        }
        
        // Enhanced auto-start feeding with debouncing and validation
        static uint8_t detection_debounce[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};
        static uint64_t last_detection_time[MAX_FILAMENT_CHANNELS] = {0, 0, 0, 0};
        uint64_t current_time = millis();
        
        if (MC_ONLINE_key_stu_prev[i] == 0 && MC_ONLINE_key_stu[i] == 1) {
            detection_debounce[i]++;
            
            // Require multiple consistent detections to reduce false positives
            if (detection_debounce[i] >= 3 && 
                (current_time - last_detection_time[i]) > 100) { // 100ms debounce
                
                // Only auto-start if sensor is healthy and conditions are right
                if (presence_sensor_healthy && 
                    get_filament_motion(i) == AMS_filament_motion::idle) {
                    
                    DEBUG_MY("Auto-start feeding for channel ");
                    DEBUG_float(i, 0);
                    DEBUG_MY(" - validated presence detected\n");
                    set_filament_motion(i, AMS_filament_motion::need_send_out);
                    
                    last_detection_time[i] = current_time;
                }
                detection_debounce[i] = 0;
            }
        } else if (MC_ONLINE_key_stu[i] == 0) {
            // Reset debounce counter when filament not present
            detection_debounce[i] = 0;
        }
    }
}

#define PWM_lim 1000

struct alignas(4) Motion_control_save_struct
{
    int Motion_control_dir[4];
    bool auto_learned[4];  ///< Whether direction was learned automatically vs static correction
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
        switch (control_type)
        {
        case pressure_control_enum::all: // 全范围控制
        {
            x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            break;
        }
        case pressure_control_enum::less_pressure: // 仅低压控制
        {
            if (pressure_voltage < control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            }
            break;
        }
        case pressure_control_enum::over_pressure: // 仅高压控制
        {
            if (pressure_voltage > control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            }
            break;
        }
        }
        if (x > 0) // 将控制力转为平方增强，平方会消掉正负，需要判断
            x = x * x / 250;
        else
            x = -x * x / 250;
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
                    x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - 1.65, time_E);
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
                    if (MC_PULL_stu_raw[CHx] < 1.65)
                    {
                        x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], 1.65, time_E, pressure_control_enum::less_pressure);
                    }
                    else if (MC_PULL_stu_raw[CHx] > 1.7)
                    {
                        x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], 1.7, time_E, pressure_control_enum::over_pressure);
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
    MC_PULL_ONLINE_read();

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
 * Enhanced direction learning with improved statistical analysis
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
    if ((current_time - state.learning_start_time) > (AUTO_DIRECTION_TIMEOUT_MS * 1000ULL)) {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning timeout for channel ");
        DEBUG_float(channel, 0);
        DEBUG_MY("\n");
        #endif
        complete_direction_learning(channel);
        return;
    }
    
    // Enforce minimum sample interval to avoid noise
    if ((current_time - state.last_sample_time) < (AUTO_DIRECTION_SAMPLE_INTERVAL_MS * 1000ULL)) {
        return;
    }
    
    // Validate movement data quality
    float abs_movement = fabsf(movement_delta);
    
    // Skip samples that are too small (likely noise)
    if (abs_movement < (AUTO_DIRECTION_MIN_MOVEMENT_MM / AUTO_DIRECTION_MIN_SAMPLES)) {
        return;
    }
    
    // Check for excessive noise in this sample
    if (abs_movement > AUTO_DIRECTION_MAX_NOISE_MM) {
        state.error_count++;
        state.accumulated_noise += abs_movement;
        
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Noisy sample rejected: ");
        DEBUG_float(abs_movement, 3);
        DEBUG_MY("mm\n");
        #endif
        
        // Too many noisy samples indicates poor sensor quality
        if (state.error_count > (AUTO_DIRECTION_MIN_SAMPLES * 2)) {
            #if AUTO_DIRECTION_DEBUG_ENABLED
            DEBUG_MY("Too many noisy samples, aborting learning\n");
            #endif
            complete_direction_learning(channel);
        }
        return;
    }
    
    // Record valid sample
    state.has_valid_data = true;
    state.last_sample_time = current_time;
    state.total_movement += movement_delta;
    state.sample_count++;
    
    // Determine direction correlation
    bool positive_correlation = (state.command_direction > 0 && movement_delta > 0) ||
                               (state.command_direction < 0 && movement_delta < 0);
    
    if (positive_correlation) {
        state.positive_samples++;
    } else {
        state.negative_samples++;
    }
    
    // Calculate confidence based on consistency
    if (state.sample_count > 0) {
        float positive_ratio = (float)state.positive_samples / state.sample_count;
        float negative_ratio = (float)state.negative_samples / state.sample_count;
        state.confidence_score = fmaxf(positive_ratio, negative_ratio);
    }
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Learning sample ");
    DEBUG_float(state.sample_count, 0);
    DEBUG_MY(": movement=");
    DEBUG_float(movement_delta, 3);
    DEBUG_MY(", confidence=");
    DEBUG_float(state.confidence_score, 3);
    DEBUG_MY("\n");
    #endif
    
    // Check if we have enough data for a decision
    if (state.sample_count >= AUTO_DIRECTION_MIN_SAMPLES) {
        // Sufficient movement accumulated?
        if (fabsf(state.total_movement) >= AUTO_DIRECTION_MIN_MOVEMENT_MM) {
            // High enough confidence?
            if (state.confidence_score >= AUTO_DIRECTION_CONFIDENCE_THRESHOLD) {
                complete_direction_learning(channel);
                return;
            }
        }
    }
    
    // Safety limit - don't run learning forever
    if (state.sample_count >= (AUTO_DIRECTION_MIN_SAMPLES * 5)) {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning sample limit reached\n");
        #endif
        complete_direction_learning(channel);
    }
}

/**
 * Complete direction learning and apply results
 */
void complete_direction_learning(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    DirectionLearningState& state = direction_learning[channel];
    
    if (!state.learning_active) {
        return; // Already completed or not started
    }
    
    state.learning_active = false;
    state.learning_complete = true;
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Completing direction learning for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
    #endif
    
    // Apply learning results if we have valid data
    if (state.has_valid_data && state.sample_count >= AUTO_DIRECTION_MIN_SAMPLES) {
        // Determine the correct direction based on sample analysis
        int learned_direction = 0;
        
        if (state.confidence_score >= AUTO_DIRECTION_CONFIDENCE_THRESHOLD) {
            // High confidence - use the majority vote
            if (state.positive_samples > state.negative_samples) {
                learned_direction = state.command_direction;
            } else {
                learned_direction = -state.command_direction;
            }
            
            // Apply the learned direction
            Motion_control_data_save.Motion_control_dir[channel] = learned_direction;
            Motion_control_data_save.auto_learned[channel] = true;
            
            // Save to flash
            Motion_control_save();
            
            #if AUTO_DIRECTION_DEBUG_ENABLED
            DEBUG_MY("Direction learned successfully: ");
            DEBUG_float(learned_direction, 0);
            DEBUG_MY(" with confidence ");
            DEBUG_float(state.confidence_score, 3);
            DEBUG_MY("\n");
            #endif
        } else {
            #if AUTO_DIRECTION_DEBUG_ENABLED
            DEBUG_MY("Direction learning failed - low confidence: ");
            DEBUG_float(state.confidence_score, 3);
            DEBUG_MY("\n");
            #endif
        }
    } else {
        #if AUTO_DIRECTION_DEBUG_ENABLED
        DEBUG_MY("Direction learning incomplete - insufficient data\n");
        #endif
    }
}

/**
 * Get current status of direction learning for a channel
 */
bool get_direction_learning_status(int channel, float* confidence, int* samples, bool* complete)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return false;
    }
    
    DirectionLearningState& state = direction_learning[channel];
    
    if (confidence) *confidence = state.confidence_score;
    if (samples) *samples = state.sample_count;
    if (complete) *complete = state.learning_complete;
    
    return state.learning_active || state.learning_complete;
}

/**
 * Reset direction learning for a specific channel
 */
void reset_direction_learning(int channel)
{
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        return;
    }
    
    DirectionLearningState& state = direction_learning[channel];
    memset(&state, 0, sizeof(DirectionLearningState));
    
    // Reset saved direction data
    Motion_control_data_save.Motion_control_dir[channel] = 0;
    Motion_control_data_save.auto_learned[channel] = false;
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("Direction learning reset for channel ");
    DEBUG_float(channel, 0);
    DEBUG_MY("\n");
    #endif
}

/**
 * Reset all learned directions
 */
void reset_all_learned_directions()
{
    for (int i = 0; i < MAX_FILAMENT_CHANNELS; i++) {
        reset_direction_learning(i);
    }
    
    // Save to flash
    Motion_control_save();
    
    #if AUTO_DIRECTION_DEBUG_ENABLED
    DEBUG_MY("All direction learning data reset\n");
    #endif
}

// Existing loading direction detection functions are implemented above

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
