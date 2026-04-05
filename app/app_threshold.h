#ifndef APP_THRESHOLD_H
#define APP_THRESHOLD_H

#include <stdint.h>

/**
 * @brief  初始化阈值控制模块 (NVM 加载后调用)
 */
void app_threshold_init(void);

/**
 * @brief  阈值检测，根据传感器值自动控制继电器 (500ms 周期调用)
 * @param  temp_x10:  温度 x10 (如 250 = 25.0°C)
 * @param  humi_x10:  湿度 x10 (未使用，预留)
 * @param  light_raw: 光照 ADC 原始值 0-4095
 * @param  soil_raw:  土壤湿度 x10 (0-1000 对应 0-100%)
 * @param  co2_ppm:   CO2 浓度 ppm
 */
void app_threshold_check(int16_t temp_x10, uint16_t humi_x10,
                          uint16_t light_raw, uint16_t soil_raw,
                          uint16_t co2_ppm);

/**
 * @brief  标记设备进入手动控制模式，暂停自动控制
 * @param  dev_id: TF_CTRL_DEV_FAN / HEATER / PUMP / LED
 */
void app_threshold_manual_override(uint8_t dev_id);

/**
 * @brief  立即关闭蜂鸣器（按键消警时调用）
 */
void app_threshold_buzzer_dismiss(void);

#endif /* APP_THRESHOLD_H */
