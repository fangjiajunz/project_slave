#include "app_threshold.h"

#include "app_relay.h"
#include "config.h"
#include "main.h"
#include "tf_multinode.h"

#define LOG_TAG "Threshold"
#include "log.h"

/* ========================== 外部引用 ========================== */

extern sys_config_t g_sys_config;

/* ========================== 使能位定义 ========================== */

#define THRESH_EN_TEMP   (1 << 0)
#define THRESH_EN_SOIL   (1 << 1)
#define THRESH_EN_LIGHT  (1 << 2)
#define THRESH_EN_CO2    (1 << 3)

/* ========================== 回差常量 ========================== */

#define HYST_TEMP   20    /* 2.0°C (x10) */
#define HYST_SOIL   200   /* ADC 单位 */
#define HYST_LIGHT  200   /* ADC 单位 */
#define HYST_CO2    50    /* ppm */

/* ========================== 手动控制超时 ========================== */

#define MANUAL_OVERRIDE_TIMEOUT_MS  (5UL * 60 * 1000)  /* 5 分钟 */
#define OVERRIDE_SLOT_COUNT  5  /* index 1-4 对应 TF_CTRL_DEV_xxx */

static uint32_t s_override_tick[OVERRIDE_SLOT_COUNT] = {0}; /* 0 = 无手动控制 */

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

/* ========================== 自动状态跟踪 ========================== */

static uint8_t s_fan_by_temp;    /* 温度触发风扇: 0=关, 1=开 */
static uint8_t s_fan_by_co2;     /* CO2触发风扇: 0=关, 1=开 */
static uint8_t s_heater_auto;    /* 自动加热器: 0=关, 1=开 */
static uint8_t s_pump_auto;      /* 自动水泵: 0=关, 1=开 */
static uint8_t s_led_auto;       /* 自动LED: 0=关, 1=开 */

/* ========================== 蜂鸣器控制 ========================== */

#define BUZZER_DURATION_MS  1000  /* 蜂鸣器响铃时长 1秒 */
static uint32_t s_buzzer_off_tick = 0;  /* 蜂鸣器关闭时刻，0表示未激活 */

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

/* ========================== API 实现 ========================== */

void app_threshold_manual_override(uint8_t dev_id)
{
    if (dev_id >= 1 && dev_id < OVERRIDE_SLOT_COUNT)
    {
        s_override_tick[dev_id] = HAL_GetTick();
        if (s_override_tick[dev_id] == 0) s_override_tick[dev_id] = 1; /* 避免 0 == 禁用 */
        log_info("Manual override: dev 0x%02X, %lus", dev_id,
                 (unsigned long)(MANUAL_OVERRIDE_TIMEOUT_MS / 1000));
    }
}

void app_threshold_init(void)
{
    s_fan_by_temp = 0;
    s_fan_by_co2  = 0;
    s_heater_auto = 0;
    s_pump_auto   = 0;
    s_led_auto    = 0;
    for (int i = 0; i < OVERRIDE_SLOT_COUNT; i++)
        s_override_tick[i] = 0;
    log_info("Threshold init (enable=0x%02X)", g_sys_config.threshold[0].enable);
}

void app_threshold_check(int16_t temp_x10, uint16_t humi_x10,
                          uint16_t light_raw, uint16_t soil_raw,
                          uint16_t co2_ppm)
{
    (void)humi_x10; /* 预留 */
    const threshold_config_t *cfg = &g_sys_config.threshold[0];

    /* ---- 温度 → 风扇 (过热) ---- */
    if (cfg->enable & THRESH_EN_TEMP)
    {
        if (!s_fan_by_temp && temp_x10 > cfg->temp_high)
        {
            s_fan_by_temp = 1;
            log_info("TEMP HIGH: %d.%d > %d.%d -> FAN ON",
                     temp_x10 / 10, temp_x10 % 10,
                     cfg->temp_high / 10, cfg->temp_high % 10);
            buzzer_trigger();  /* 温度过高报警 */
        }
        else if (s_fan_by_temp && temp_x10 < (cfg->temp_high - HYST_TEMP))
        {
            s_fan_by_temp = 0;
            log_info("TEMP OK: %d.%d < %d.%d -> FAN(temp) OFF",
                     temp_x10 / 10, temp_x10 % 10,
                     (cfg->temp_high - HYST_TEMP) / 10,
                     (cfg->temp_high - HYST_TEMP) % 10);
        }

        /* 温度 → 加热器 (过低) */
        if (!s_heater_auto && temp_x10 < cfg->temp_low)
        {
            s_heater_auto = 1;
            log_info("TEMP LOW: %d.%d < %d.%d -> HEATER ON",
                     temp_x10 / 10, temp_x10 % 10,
                     cfg->temp_low / 10, cfg->temp_low % 10);
            buzzer_trigger();  /* 温度过低报警 */
        }
        else if (s_heater_auto && temp_x10 > (cfg->temp_low + HYST_TEMP))
        {
            s_heater_auto = 0;
            log_info("TEMP OK: %d.%d > %d.%d -> HEATER OFF",
                     temp_x10 / 10, temp_x10 % 10,
                     (cfg->temp_low + HYST_TEMP) / 10,
                     (cfg->temp_low + HYST_TEMP) % 10);
        }

        if (!is_manual_active(TF_CTRL_DEV_HEATER))
            app_relay_set(TF_CTRL_DEV_HEATER, s_heater_auto);
    }

    /* ---- 土壤湿度 → 水泵 ---- */
    if (cfg->enable & THRESH_EN_SOIL)
    {
        if (!s_pump_auto && soil_raw > cfg->soil_dry)
        {
            s_pump_auto = 1;
            log_info("SOIL DRY: %u > %u -> PUMP ON", soil_raw, cfg->soil_dry);
            buzzer_trigger();  /* 土壤干燥报警 */
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

    /* ---- 光照 → LED ---- */
    if (cfg->enable & THRESH_EN_LIGHT)
    {
        if (!s_led_auto && light_raw < cfg->light_low)
        {
            s_led_auto = 1;
            log_info("LIGHT LOW: %u < %u -> LED ON", light_raw, cfg->light_low);
            buzzer_trigger();  /* 光照不足报警 */
        }
        else if (s_led_auto && light_raw > (cfg->light_low + HYST_LIGHT))
        {
            s_led_auto = 0;
            log_info("LIGHT OK: %u > %u -> LED OFF",
                     light_raw, cfg->light_low + HYST_LIGHT);
        }

        if (!is_manual_active(TF_CTRL_DEV_LED))
            app_relay_set(TF_CTRL_DEV_LED, s_led_auto);
    }

    /* ---- CO2 → 风扇 ---- */
    if (cfg->enable & THRESH_EN_CO2)
    {
        if (!s_fan_by_co2 && co2_ppm > cfg->co2_high)
        {
            s_fan_by_co2 = 1;
            log_info("CO2 HIGH: %u > %u -> FAN ON", co2_ppm, cfg->co2_high);
            buzzer_trigger();  /* CO2过高报警 */
        }
        else if (s_fan_by_co2 && co2_ppm < (cfg->co2_high - HYST_CO2))
        {
            s_fan_by_co2 = 0;
            log_info("CO2 OK: %u < %u -> FAN(co2) OFF",
                     co2_ppm, cfg->co2_high - HYST_CO2);
        }
    }

    /* ---- 风扇 OR 合并: 温度 || CO2 任一触发即开 ---- */
    if ((cfg->enable & THRESH_EN_TEMP) || (cfg->enable & THRESH_EN_CO2))
    {
        if (!is_manual_active(TF_CTRL_DEV_FAN))
            app_relay_set(TF_CTRL_DEV_FAN, (s_fan_by_temp || s_fan_by_co2) ? 1 : 0);
    }

    /* 更新蜂鸣器状态 (自动关闭) */
    buzzer_update();
}
