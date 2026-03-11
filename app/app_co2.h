#ifndef APP_CO2_H
#define APP_CO2_H

#include <stdint.h>

/**
 * @brief  在主循环中调用，从 UART1 FIFO 读取字节并解析 CO2 帧
 */
void app_co2_poll(void);

/**
 * @brief  获取最近一次解析成功的 CO2 浓度
 * @return CO2 浓度 (ppm), 未收到有效数据时返回 0
 */
uint16_t app_co2_get(void);

#endif /* APP_CO2_H */
