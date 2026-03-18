#include "app_relay.h"

#define LOG_TAG "Relay"
#include "log.h"

/* ========================== 内部结构 ========================== */

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} relay_pin_t;

static const relay_pin_t s_relays[] = {
    [TF_CTRL_DEV_FAN] = {RELAY_FAN_PORT, RELAY_FAN_PIN},
    [TF_CTRL_DEV_HEATER] = {RELAY_HEATER_PORT, RELAY_HEATER_PIN},
    [TF_CTRL_DEV_PUMP] = {RELAY_PUMP_PORT, RELAY_PUMP_PIN},
    [TF_CTRL_DEV_LED] = {RELAY_LED_PORT, RELAY_LED_PIN},
};

#define RELAY_COUNT (sizeof(s_relays) / sizeof(s_relays[0]))

/* ========================== 内部工具 ========================== */

static inline GPIO_PinState action_to_level(uint8_t action)
{
#if RELAY_ACTIVE_HIGH
    return action ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
    return action ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
}

static inline uint8_t level_to_action(GPIO_PinState level)
{
#if RELAY_ACTIVE_HIGH
    return (level == GPIO_PIN_SET) ? 1 : 0;
#else
    return (level == GPIO_PIN_RESET) ? 1 : 0;
#endif
}

/* ========================== API 实现 ========================== */

void app_relay_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    for (uint8_t i = 0; i < RELAY_COUNT; i++)
    {
        if (s_relays[i].port != NULL)
        {
            /* 1. 先开启该 GPIO 端口的时钟 (极其关键!) */

            /* 2. 写入默认电平，LED 默认开启，其他关闭 */
            uint8_t default_state = (i == TF_CTRL_DEV_LED) ? 1 : 0;
            HAL_GPIO_WritePin(s_relays[i].port, s_relays[i].pin, action_to_level(default_state));

            /* 3. 配置 GPIO 为推挽输出 */
            GPIO_InitStruct.Pin = s_relays[i].pin;
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // 推挽输出
            GPIO_InitStruct.Pull = GPIO_NOPULL;           // 无需上下拉
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // 继电器低速即可
            HAL_GPIO_Init(s_relays[i].port, &GPIO_InitStruct);
        }
    }

    log_info("Relay init done. Count: %zu", RELAY_COUNT);
}

void app_relay_set(uint8_t dev_id, uint8_t action)
{
    if (dev_id == 0 || dev_id >= RELAY_COUNT || s_relays[dev_id].port == NULL)
    {
        log_warn("Invalid relay dev_id: 0x%02X", dev_id);
        return;
    }

    /* 状态未变化则跳过，避免重复日志 */
    if (app_relay_get(dev_id) == (action ? 1 : 0))
        return;

    HAL_GPIO_WritePin(s_relays[dev_id].port, s_relays[dev_id].pin,
                      action_to_level(action));
    log_info("Relay 0x%02X -> %s", dev_id, action ? "ON" : "OFF");
}

uint8_t app_relay_get(uint8_t dev_id)
{
    if (dev_id == 0 || dev_id >= RELAY_COUNT || s_relays[dev_id].port == NULL)
        return 0;

    GPIO_PinState level = HAL_GPIO_ReadPin(s_relays[dev_id].port,
                                           s_relays[dev_id].pin);
    return level_to_action(level);
}
