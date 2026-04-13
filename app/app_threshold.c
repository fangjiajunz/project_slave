#include "app_threshold.h"

#include "app_display.h"
#include "app_relay.h"
#include "config.h"
#include "main.h"
#include "tf_multinode.h"

#define LOG_TAG "Threshold"
#include "log.h"

extern sys_config_t g_sys_config;

#define THRESH_EN_TEMP   (1 << 0)
#define THRESH_EN_SOIL   (1 << 1)
#define THRESH_EN_LIGHT  (1 << 2)
#define THRESH_EN_CO2    (1 << 3)
#define THRESH_EN_HUMI   (1 << 4)

#define HYST_TEMP   20
#define HYST_HUMI   50
#define HYST_SOIL   200
#define HYST_LIGHT  200
#define HYST_CO2    50

#define MANUAL_OVERRIDE_TIMEOUT_MS  (5UL * 60 * 1000)
#define OVERRIDE_SLOT_COUNT  5

static uint32_t s_override_tick[OVERRIDE_SLOT_COUNT] = {0};

static uint8_t s_fan_by_temp;
static uint8_t s_fan_by_humi;
static uint8_t s_fan_by_co2;
static uint8_t s_heater_auto;
static uint8_t s_pump_auto;
static uint8_t s_led_auto;
static uint8_t s_humi_low_alarm;

#define BUZZER_DURATION_MS  5000
static uint32_t s_buzzer_off_tick = 0;

static bool is_manual_active(uint8_t dev_id)
{
    if (dev_id == 0 || dev_id >= OVERRIDE_SLOT_COUNT) return false;
    if (s_override_tick[dev_id] == 0) return false;
    if ((HAL_GetTick() - s_override_tick[dev_id]) >= MANUAL_OVERRIDE_TIMEOUT_MS)
    {
        s_override_tick[dev_id] = 0;
        log_info("Manual override expired: dev 0x%02X, auto resumed", dev_id);
        return false;
    }
    return true;
}

static void buzzer_trigger(void)
{
    buzzer_on();
    s_buzzer_off_tick = HAL_GetTick() + BUZZER_DURATION_MS;
}

static void buzzer_update(void)
{
    if (s_buzzer_off_tick != 0 && HAL_GetTick() >= s_buzzer_off_tick)
    {
        buzzer_off();
        s_buzzer_off_tick = 0;
    }
}

void app_threshold_manual_override(uint8_t dev_id)
{
    if (dev_id >= 1 && dev_id < OVERRIDE_SLOT_COUNT)
    {
        s_override_tick[dev_id] = HAL_GetTick();
        if (s_override_tick[dev_id] == 0) s_override_tick[dev_id] = 1;
        log_info("Manual override: dev 0x%02X, %lus", dev_id,
                 (unsigned long)(MANUAL_OVERRIDE_TIMEOUT_MS / 1000));
    }
}

void app_threshold_buzzer_dismiss(void)
{
    buzzer_off();
    s_buzzer_off_tick = 0;
}

void app_threshold_init(void)
{
    s_fan_by_temp = 0;
    s_fan_by_humi = 0;
    s_fan_by_co2 = 0;
    s_heater_auto = 0;
    s_pump_auto = 0;
    s_led_auto = 0;
    s_humi_low_alarm = 0;
    for (int i = 0; i < OVERRIDE_SLOT_COUNT; i++)
        s_override_tick[i] = 0;
    log_info("Threshold init (enable=0x%02X)", g_sys_config.threshold[0].enable);
}

void app_threshold_check(int16_t temp_x10, uint16_t humi_x10,
                         uint16_t light_raw, uint16_t soil_raw,
                         uint16_t co2_ppm)
{
    const threshold_config_t *cfg = &g_sys_config.threshold[0];
    uint8_t fan_request;
    uint8_t heater_request;

    if (cfg->enable & THRESH_EN_TEMP)
    {
        if (!s_fan_by_temp && temp_x10 > cfg->temp_high)
        {
            s_fan_by_temp = 1;
            log_info("TEMP HIGH: %d.%d > %d.%d -> FAN ON",
                     temp_x10 / 10, temp_x10 % 10,
                     cfg->temp_high / 10, cfg->temp_high % 10);
            app_display_alert("! TEMP HIGH", "Fan ON");
            buzzer_trigger();
        }
        else if (s_fan_by_temp && temp_x10 < (cfg->temp_high - HYST_TEMP))
        {
            s_fan_by_temp = 0;
            log_info("TEMP OK: %d.%d < %d.%d -> FAN(temp) OFF",
                     temp_x10 / 10, temp_x10 % 10,
                     (cfg->temp_high - HYST_TEMP) / 10,
                     (cfg->temp_high - HYST_TEMP) % 10);
        }

        if (!s_heater_auto && temp_x10 < cfg->temp_low)
        {
            s_heater_auto = 1;
            log_info("TEMP LOW: %d.%d < %d.%d -> HEATER ON",
                     temp_x10 / 10, temp_x10 % 10,
                     cfg->temp_low / 10, cfg->temp_low % 10);
            app_display_alert("! TEMP LOW", "Heater ON");
            buzzer_trigger();
        }
        else if (s_heater_auto && temp_x10 > (cfg->temp_low + HYST_TEMP))
        {
            s_heater_auto = 0;
            log_info("TEMP OK: %d.%d > %d.%d -> HEATER OFF",
                     temp_x10 / 10, temp_x10 % 10,
                     (cfg->temp_low + HYST_TEMP) / 10,
                     (cfg->temp_low + HYST_TEMP) % 10);
        }
    }

    if (cfg->enable & THRESH_EN_HUMI)
    {
        if (!s_fan_by_humi && humi_x10 > cfg->humi_high)
        {
            s_fan_by_humi = 1;
            log_info("HUMI HIGH: %u.%u > %u.%u -> FAN ON",
                     humi_x10 / 10, humi_x10 % 10,
                     cfg->humi_high / 10, cfg->humi_high % 10);
            app_display_alert("! HUMI HIGH", "Fan ON");
            buzzer_trigger();
        }
        else if (s_fan_by_humi && humi_x10 < (cfg->humi_high - HYST_HUMI))
        {
            s_fan_by_humi = 0;
            log_info("HUMI OK: %u.%u < %u.%u -> FAN(humi) OFF",
                     humi_x10 / 10, humi_x10 % 10,
                     (cfg->humi_high - HYST_HUMI) / 10,
                     (cfg->humi_high - HYST_HUMI) % 10);
        }

        if (!s_humi_low_alarm && humi_x10 < cfg->humi_low)
        {
            s_humi_low_alarm = 1;
            log_info("HUMI LOW: %u.%u < %u.%u -> ALARM",
                     humi_x10 / 10, humi_x10 % 10,
                     cfg->humi_low / 10, cfg->humi_low % 10);
            app_display_alert("! HUMI LOW", "Check Water");
            buzzer_trigger();
        }
        else if (s_humi_low_alarm && humi_x10 > (cfg->humi_low + HYST_HUMI))
        {
            s_humi_low_alarm = 0;
            log_info("HUMI LOW CLEARED: %u.%u > %u.%u",
                     humi_x10 / 10, humi_x10 % 10,
                     (cfg->humi_low + HYST_HUMI) / 10,
                     (cfg->humi_low + HYST_HUMI) % 10);
        }
    }

    if (cfg->enable & THRESH_EN_SOIL)
    {
        if (!s_pump_auto && soil_raw > cfg->soil_dry)
        {
            s_pump_auto = 1;
            log_info("SOIL DRY: %u > %u -> PUMP ON", soil_raw, cfg->soil_dry);
            app_display_alert("! SOIL DRY", "Pump ON");
            buzzer_trigger();
        }
        else if (s_pump_auto && soil_raw < (cfg->soil_dry - HYST_SOIL))
        {
            s_pump_auto = 0;
            log_info("SOIL OK: %u < %u -> PUMP OFF",
                     soil_raw, cfg->soil_dry - HYST_SOIL);
        }

        if (!is_manual_active(TF_CTRL_DEV_PUMP))
            app_relay_set(TF_CTRL_DEV_PUMP, s_pump_auto);
    }

    if (cfg->enable & THRESH_EN_LIGHT)
    {
        if (!s_led_auto && light_raw > cfg->light_low)
        {
            s_led_auto = 1;
            log_info("LIGHT LOW: %u > %u -> LED ON", light_raw, cfg->light_low);
            app_display_alert("! LIGHT LOW", "LED ON");
            buzzer_trigger();
        }
        else if (s_led_auto && light_raw < (cfg->light_low - HYST_LIGHT))
        {
            s_led_auto = 0;
            log_info("LIGHT OK: %u < %u -> LED OFF",
                     light_raw, cfg->light_low - HYST_LIGHT);
        }

        if (!is_manual_active(TF_CTRL_DEV_LED))
            app_relay_set(TF_CTRL_DEV_LED, s_led_auto);
    }

    if (cfg->enable & THRESH_EN_CO2)
    {
        if (!s_fan_by_co2 && co2_ppm > cfg->co2_high)
        {
            s_fan_by_co2 = 1;
            log_info("CO2 HIGH: %u > %u -> FAN ON", co2_ppm, cfg->co2_high);
            app_display_alert("! CO2 HIGH", "Fan ON");
            buzzer_trigger();
        }
        else if (s_fan_by_co2 && co2_ppm < (cfg->co2_high - HYST_CO2))
        {
            s_fan_by_co2 = 0;
            log_info("CO2 OK: %u < %u -> FAN(co2) OFF",
                     co2_ppm, cfg->co2_high - HYST_CO2);
        }
    }

    fan_request = (s_fan_by_temp || s_fan_by_humi || s_fan_by_co2) ? 1U : 0U;
    heater_request = fan_request ? 0U : s_heater_auto;

    if ((cfg->enable & THRESH_EN_TEMP) ||
        (cfg->enable & THRESH_EN_HUMI) ||
        (cfg->enable & THRESH_EN_CO2))
    {
        if (!is_manual_active(TF_CTRL_DEV_FAN))
            app_relay_set(TF_CTRL_DEV_FAN, fan_request);
    }

    if (cfg->enable & THRESH_EN_TEMP)
    {
        if (!is_manual_active(TF_CTRL_DEV_HEATER))
            app_relay_set(TF_CTRL_DEV_HEATER, heater_request);
    }

    buzzer_update();
}
