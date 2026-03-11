#ifndef __APP_ADC_H__
#define __APP_ADC_H__

#include <stdint.h>

/**
 * @brief ADC 通道索引
 */
typedef enum {
    APP_ADC_CH0 = 0,    // 对应 PA0 (ADC_CHANNEL_0)
    APP_ADC_CH1,        // 对应 PA1 (ADC_CHANNEL_1)
    APP_ADC_CH_MAX
} app_adc_ch_t;

/**
 * @brief 初始化 ADC 应用层模块（执行校准）
 */
void app_adc_init(void);

/**
 * @brief 获取指定通道的原始 ADC 值 (0-4095)
 * 
 * @param ch 通道索引
 * @return uint16_t ADC 原始值
 */
uint16_t app_adc_get_raw(app_adc_ch_t ch);

/**
 * @brief 获取指定通道的电压值 (单位: V)
 * 
 * @param ch 通道索引
 * @return float 电压值
 */
float app_adc_get_voltage(app_adc_ch_t ch);

#endif /* __APP_ADC_H__ */
