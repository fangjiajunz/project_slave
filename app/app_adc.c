#include "app_adc.h"
#include "adc.h"

/**
 * @brief 初始化 ADC 应用层
 * @note  STM32F1 系列启动后必须进行校准，否则采样值会有较大偏差
 */
void app_adc_init(void)
{
    // 执行 ADC 校准
    HAL_ADCEx_Calibration_Start(&hadc1);
}

/**
 * @brief 获取原始 ADC 数据（阻塞式轮询采样）
 */
uint16_t app_adc_get_raw(app_adc_ch_t ch)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t adc_val = 0;

    // 1. 根据传入参数配置通道
    if (ch == APP_ADC_CH0) {
        sConfig.Channel = ADC_CHANNEL_0;
    } else if (ch == APP_ADC_CH1) {
        sConfig.Channel = ADC_CHANNEL_1;
    } else {
        return 0;
    }

    sConfig.Rank = ADC_REGULAR_RANK_1;
    // 适当增加采样时间以提高稳定性，此处改为 7.5 个周期或更多
    sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5; 

    // 配置通道
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    // 2. 启动 ADC 并等待转换完成
    HAL_ADC_Start(&hadc1);
    
    // 等待转换完成，超时时间设为 10ms
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);

    return adc_val;
}

/**
 * @brief 将原始值转换为电压值 (假设参考电压为 3.3V)
 */
float app_adc_get_voltage(app_adc_ch_t ch)
{
    uint16_t raw = app_adc_get_raw(ch);
    // 12位 ADC: 0-4095 对应 0-3.3V
    return (float)raw * 3.3f / 4095.0f;
}
