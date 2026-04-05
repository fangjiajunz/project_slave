#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdbool.h>
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

/**
 * @brief  标记用户活动 (按键按下时调用)，如果屏幕已关闭则点亮
 */
void app_display_activity(void);

/**
 * @brief  检查超时并自动关屏 (主循环中调用)
 */
void app_display_timeout_check(void);

/**
 * @brief  屏幕是否处于关闭状态
 */
bool app_display_is_off(void);

/**
 * @brief  显示报警弹窗 (叠加在传感器页上方，持续 3 秒后自动消失)
 *
 * @param  title  弹窗标题，如 "! TEMP HIGH"
 * @param  msg    弹窗内容，如 "Fan ON"
 */
void app_display_alert(const char *title, const char *msg);

/**
 * @brief  立即关闭弹窗
 */
void app_display_alert_dismiss(void);

/**
 * @brief  弹窗是否正在显示
 */
bool app_display_alert_is_active(void);

/**
 * @brief  弹窗刷新 (主循环中调用，处理自动消失逻辑)
 */
void app_display_alert_tick(void);

#endif /* APP_DISPLAY_H */
