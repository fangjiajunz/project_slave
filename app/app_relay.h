#ifndef APP_RELAY_H
#define APP_RELAY_H

#include <stdint.h>

#include "main.h"
#include "tf_multinode.h"

/* ========================== 继电器引脚配置 ==========================
 * 根据实际硬件修改以下宏定义
 * ================================================================== */

#define RELAY_FAN_PORT GPIOB
#define RELAY_FAN_PIN GPIO_PIN_2

#define RELAY_HEATER_PORT GPIOA
#define RELAY_HEATER_PIN GPIO_PIN_2

#define RELAY_PUMP_PORT GPIOB
#define RELAY_PUMP_PIN GPIO_PIN_14

#define RELAY_LED_PORT LED_TX_GPIO_Port
#define RELAY_LED_PIN LED_TX_Pin

/* 继电器有效电平: 1 = 高电平驱动, 0 = 低电平驱动 */
#define RELAY_ACTIVE_HIGH 1

/* ========================== API ========================== */

/**
 * @brief  初始化所有继电器 GPIO 为输出，默认关闭
 */
void app_relay_init(void);

/**
 * @brief  控制指定继电器
 * @param  dev_id: 设备 ID (TF_CTRL_DEV_FAN / HEATER / PUMP / LED)
 * @param  action: 0=关, 1=开
 */
void app_relay_set(uint8_t dev_id, uint8_t action);

/**
 * @brief  获取指定继电器当前状态
 * @param  dev_id: 设备 ID
 * @return 0=关, 1=开
 */
uint8_t app_relay_get(uint8_t dev_id);

#endif /* APP_RELAY_H */
