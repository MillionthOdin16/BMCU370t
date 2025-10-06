#include "ADC_DMA.h"
#include <math.h>  // For statistical calculations

int16_t ADC_Calibrattion_Val = 0;

#define ADC_filter_n_pow 8        // 滑动窗口滤波的窗口长度(为2的几次方)
constexpr int mypow(int a, int b) // 定义一个函数用于简单计算次方
{
    int x = 1;
    while (b--)
        x *= a;
    return x;
}
constexpr const int ADC_filter_n = mypow(2, ADC_filter_n_pow); // 滑动窗口滤波的窗口长度n
// 说明：滑动滤波后信号的频率变为f/(2n),f约为8000,n=256,则滤波后变为15hz
uint16_t ADC_data[ADC_filter_n][8];
float ADC_V[8];

// Enhanced robustness and accuracy features
float ADC_previous[8] = {0};           ///< Previous readings for stability detection
uint32_t ADC_fault_count[8] = {0};     ///< Fault counter for each channel
uint32_t ADC_stable_count[8] = {0};    ///< Stability counter for each channel
bool ADC_channel_healthy[8] = {true, true, true, true, true, true, true, true}; ///< Health status
float ADC_noise_history[8][SENSOR_NOISE_FILTER_SAMPLES]; ///< Noise history for adaptive filtering
uint8_t ADC_noise_index[8] = {0};      ///< Circular buffer index for noise history
uint64_t ADC_last_calibration = 0;     ///< Last calibration timestamp

/**
 * Calculate standard deviation for outlier detection
 */
float calculate_std_dev(uint16_t* data, int count, float mean)
{
    float variance = 0;
    for (int i = 0; i < count; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    return sqrtf(variance / count);
}

/**
 * Detect and remove outliers from ADC data
 */
bool is_outlier(uint16_t value, uint16_t* data, int count)
{
    if (!ADC_OUTLIER_DETECTION_ENABLED) return false;
    
    // Calculate mean
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += data[i];
    }
    float mean = sum / count;
    
    // Calculate standard deviation
    float std_dev = calculate_std_dev(data, count, mean);
    
    // Check if value is outside threshold
    float deviation = fabsf(value - mean);
    return (deviation > ADC_OUTLIER_THRESHOLD * std_dev);
}

/**
 * Update noise history for adaptive filtering
 */
void update_noise_history(int channel, float noise_level)
{
    if (channel < 0 || channel >= 8) return;
    
    ADC_noise_history[channel][ADC_noise_index[channel]] = noise_level;
    ADC_noise_index[channel] = (ADC_noise_index[channel] + 1) % SENSOR_NOISE_FILTER_SAMPLES;
}

/**
 * Calculate average noise level for adaptive filtering
 */
float get_average_noise_level(int channel)
{
    if (channel < 0 || channel >= 8) return 1.0f;
    
    float sum = 0;
    for (int i = 0; i < SENSOR_NOISE_FILTER_SAMPLES; i++) {
        sum += ADC_noise_history[channel][i];
    }
    return sum / SENSOR_NOISE_FILTER_SAMPLES;
}

void ADC_DMA_init()
{
    // 设置IO模式
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
    }

    // 初始化DMA
    {
        DMA_InitTypeDef DMA_InitStructure;

        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

        DMA_DeInit(DMA1_Channel1);
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_data; // 数组名代表的是首项地址
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
        DMA_InitStructure.DMA_BufferSize = ADC_filter_n * 8;
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
        DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
        DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init(DMA1_Channel1, &DMA_InitStructure);

        DMA_Cmd(DMA1_Channel1, ENABLE); // 打开DMA
    }

    // 初始化ADC
    {
        ADC_DeInit(ADC1);
        RCC_ADCCLKConfig(RCC_PCLK2_Div8);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
        ADC_InitTypeDef ADC_InitStructure;
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_InitStructure.ADC_ScanConvMode = ENABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 8;
        ADC_Init(ADC1, &ADC_InitStructure);

        ADC_Cmd(ADC1, ENABLE);
        ADC_BufferCmd(ADC1, DISABLE); // 关闭buff

        ADC_ResetCalibration(ADC1); // 重置ADC校准
        while (ADC_GetResetCalibrationStatus(ADC1))
            ;
        ADC_StartCalibration(ADC1); // 开始ADC校准
        while (ADC_GetCalibrationStatus(ADC1))
            ;
        ADC_Calibrattion_Val = Get_CalibrationValue(ADC1); // 保存ADC校准值
        for (int i = 0; i < 8; i++)
            ADC_RegularChannelConfig(ADC1, i, i + 1, ADC_SampleTime_239Cycles5); // 设置8个通道为规则通道,约72KHz单通道,8K总速率
        ADC_DMACmd(ADC1, ENABLE);                                                // 打开ADC的DMA模式
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);                                  // 开启ADC转换
    }

    delay(ADC_filter_n); // 等待8次缓冲区域满(每ms可以转换8组数据)
}

float *ADC_DMA_get_value()
{
    uint64_t current_time = millis();
    
    // Check if calibration is needed
    if (ADC_last_calibration == 0 || 
        (current_time - ADC_last_calibration) > SENSOR_CALIBRATION_INTERVAL_MS) {
        
        // Perform periodic calibration
        ADC_ResetCalibration(ADC1);
        while (ADC_GetResetCalibrationStatus(ADC1));
        ADC_StartCalibration(ADC1);
        while (ADC_GetCalibrationStatus(ADC1));
        ADC_Calibrattion_Val = Get_CalibrationValue(ADC1);
        
        ADC_last_calibration = current_time;
        DEBUG_MY("ADC recalibrated at ");
        DEBUG_time();
    }
    
    for (int i = 0; i < 8; i++)
    {
        int data_sum = 0;
        int valid_count = 0;
        uint16_t channel_data[ADC_filter_n];
        
        // Collect data for this channel
        for (int j = 0; j < ADC_filter_n; j++) {
            channel_data[j] = ADC_data[j][i];
        }
        
        // Process data with outlier detection and improved filtering
        for (int j = 0; j < ADC_filter_n; j++)
        {
            uint16_t val = channel_data[j];
            
            // Skip obvious invalid readings
            if (val == 0 || val >= 4095) continue;
            
            // Outlier detection (if enabled)
            if (ADC_OUTLIER_DETECTION_ENABLED && is_outlier(val, channel_data, ADC_filter_n)) {
                continue;  // Skip outlier
            }
            
            // Apply calibration correction
            int sum = val + ADC_Calibrattion_Val;
            if (sum < 0) {
                sum = 0;
            } else if (sum > 4095) {
                sum = 4095;
            }
            
            data_sum += sum;
            valid_count++;
        }
        
        // Calculate average from valid samples
        float raw_voltage;
        if (valid_count > ADC_filter_n / 4) {  // Need at least 25% valid samples
            data_sum /= valid_count;
            raw_voltage = ((float)data_sum) / 4096.0f * 3.3f;
            
            // Reset fault counter on good reading
            if (ADC_fault_count[i] > 0) {
                ADC_fault_count[i]--;
            }
        } else {
            // Too many invalid samples - use previous value and increment fault counter
            raw_voltage = ADC_previous[i];
            ADC_fault_count[i]++;
            
            DEBUG_MY("ADC channel ");
            DEBUG_num("", i);
            DEBUG_MY(" low valid sample count: ");
            DEBUG_num("", valid_count);
            DEBUG_MY("/");
            DEBUG_num("", ADC_filter_n);
            DEBUG_MY("\n");
        }
        
        // Stability detection
        float voltage_diff = fabsf(raw_voltage - ADC_previous[i]);
        if (voltage_diff < ADC_STABILITY_THRESHOLD) {
            ADC_stable_count[i]++;
        } else {
            ADC_stable_count[i] = 0;  // Reset stability counter
        }
        
        // Adaptive filtering based on signal stability
        if (ADC_ADAPTIVE_FILTER_ENABLED) {
            float noise_level = voltage_diff;
            update_noise_history(i, noise_level);
            
            float avg_noise = get_average_noise_level(i);
            if (avg_noise > SENSOR_MAX_NOISE_RATIO) {
                // Apply stronger filtering for noisy signals
                ADC_V[i] = ADC_previous[i] * 0.8f + raw_voltage * 0.2f;
            } else if (ADC_stable_count[i] > 10) {
                // Signal is stable, allow faster response
                ADC_V[i] = ADC_previous[i] * 0.3f + raw_voltage * 0.7f;
            } else {
                // Normal filtering
                ADC_V[i] = ADC_previous[i] * 0.5f + raw_voltage * 0.5f;
            }
        } else {
            ADC_V[i] = raw_voltage;
        }
        
        // Update health status
        if (ADC_fault_count[i] >= SENSOR_FAULT_THRESHOLD) {
            if (ADC_channel_healthy[i]) {
                ADC_channel_healthy[i] = false;
                DEBUG_MY("ADC channel ");
                DEBUG_num("", i);
                DEBUG_MY(" marked as faulty\n");
            }
        } else if (ADC_fault_count[i] == 0 && ADC_stable_count[i] >= SENSOR_RECOVERY_THRESHOLD) {
            if (!ADC_channel_healthy[i]) {
                ADC_channel_healthy[i] = true;
                DEBUG_MY("ADC channel ");
                DEBUG_num("", i);
                DEBUG_MY(" recovered\n");
            }
        }
        
        ADC_previous[i] = ADC_V[i];
    }

    return ADC_V;
}

/**
 * Get health status of ADC channels
 * @return Pointer to array of 8 health status flags
 */
bool* ADC_DMA_get_health_status()
{
    return ADC_channel_healthy;
}

/**
 * Get fault counts for ADC channels
 * @return Pointer to array of 8 fault counters
 */
uint32_t* ADC_DMA_get_fault_counts()
{
    return ADC_fault_count;
}