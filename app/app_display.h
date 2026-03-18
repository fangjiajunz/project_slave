#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdint.h>

/**
 * @brief  初始化 OLED 显示
 */
void app_display_init(void);

/**
 * @brief  刷新传感器数据到 OLED (上3下2 网格布局 + 底部设备状态)
 *
 * @param  temp       温度 (x10, 如 251 = 25.1°C)
 * @param  humi       湿度 (x10, 如 655 = 65.5%)
 * @param  light      光照 ADC 原始值 (0-4095)
 * @param  soil       土壤湿度 (x10, 0-1000 对应 0-100%)
 * @param  co2        CO2 浓度 (ppm)
 * @param  dev_status 底部设备状态字符串 (如 ">Fan:OFF"), NULL 则不显示
 */
void app_display_sensor(int16_t temp, uint16_t humi,
                        uint16_t light, uint16_t soil,
                        uint16_t co2, const char *dev_status);

#endif /* APP_DISPLAY_H */
