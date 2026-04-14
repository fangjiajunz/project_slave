#include "app_relay.h"

#define LOG_TAG "Relay"
#include "log.h"

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

static inline GPIO_PinState action_to_level(uint8_t dev_id, uint8_t action)
{
    if (dev_id == TF_CTRL_DEV_LED)
    {
        return action ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

#if RELAY_ACTIVE_HIGH
    return action ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
    return action ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
}

static inline uint8_t level_to_action(uint8_t dev_id, GPIO_PinState level)
{
    if (dev_id == TF_CTRL_DEV_LED)
    {
        return (level == GPIO_PIN_RESET) ? 1U : 0U;
    }

#if RELAY_ACTIVE_HIGH
    return (level == GPIO_PIN_SET) ? 1U : 0U;
#else
    return (level == GPIO_PIN_RESET) ? 1U : 0U;
#endif
}

void app_relay_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    for (uint8_t i = 0; i < RELAY_COUNT; i++)
    {
        if (s_relays[i].port != NULL)
        {
            HAL_GPIO_WritePin(s_relays[i].port, s_relays[i].pin, action_to_level(i, 0));

            GPIO_InitStruct.Pin = s_relays[i].pin;
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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

    if (app_relay_get(dev_id) == (action ? 1U : 0U))
        return;

    HAL_GPIO_WritePin(s_relays[dev_id].port, s_relays[dev_id].pin,
                      action_to_level(dev_id, action));
    log_info("Relay 0x%02X -> %s", dev_id, action ? "ON" : "OFF");
}

uint8_t app_relay_get(uint8_t dev_id)
{
    if (dev_id == 0 || dev_id >= RELAY_COUNT || s_relays[dev_id].port == NULL)
        return 0;

    GPIO_PinState level = HAL_GPIO_ReadPin(s_relays[dev_id].port,
                                           s_relays[dev_id].pin);
    return level_to_action(dev_id, level);
}
