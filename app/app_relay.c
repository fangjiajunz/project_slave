#include "app_relay.h"

#define LOG_TAG "Relay"
#include "log.h"

/* ========================== 内部结构 ========================== */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} relay_pin_t;

static const relay_pin_t s_relays[] = {
    [TF_CTRL_DEV_FAN]    = { RELAY_FAN_PORT,    RELAY_FAN_PIN },
    [TF_CTRL_DEV_HEATER] = { RELAY_HEATER_PORT, RELAY_HEATER_PIN },
    [TF_CTRL_DEV_PUMP]   = { RELAY_PUMP_PORT,   RELAY_PUMP_PIN },
    [TF_CTRL_DEV_LED]    = { RELAY_LED_PORT,    RELAY_LED_PIN },
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
    /* 全部关闭 */
    for (uint8_t i = 1; i < RELAY_COUNT; i++)
    {
        if (s_relays[i].port != NULL)
        {
            HAL_GPIO_WritePin(s_relays[i].port, s_relays[i].pin,
                              action_to_level(0));
        }
    }
    log_info("Relay init done");
}

void app_relay_set(uint8_t dev_id, uint8_t action)
{
    if (dev_id == 0 || dev_id >= RELAY_COUNT || s_relays[dev_id].port == NULL)
    {
        log_warn("Invalid relay dev_id: 0x%02X", dev_id);
        return;
    }

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
