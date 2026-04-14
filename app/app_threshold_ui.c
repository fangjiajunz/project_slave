#include "app_threshold_ui.h"

#include "app.h"
#include "config.h"
#include "dispDirver.h"
#include "main.h"
#include "tf_slave.h"

#include <stddef.h>
#include <stdio.h>

#define LOG_TAG "ThreshUI"
#include "log.h"

extern TinyFrame tf;

typedef struct {
    const char *name;
    uint8_t    offset;
    int16_t    step;
    int16_t    min_val;
    int16_t    max_val;
    bool       div10;
    const char *unit;
} edit_item_t;

#define FOFF(f) ((uint8_t)offsetof(threshold_config_t, f))

static const edit_item_t s_items[] = {
    {"Temp High", FOFF(temp_high), 10,  -200, 600,  true,  "C"},
    {"Temp Low",  FOFF(temp_low),  10,  -200, 600,  true,  "C"},
    {"Humi High", FOFF(humi_high), 10,  0,    1000, true,  "%"},
    {"Humi Low",  FOFF(humi_low),  10,  0,    1000, true,  "%"},
    {"Soil Dry",  FOFF(soil_dry),  10,  0,    1000, true,  "%"},
    {"Light High", FOFF(light_low), 100, 0,    4095, false, ""},
    {"CO2 High",  FOFF(co2_high),  50,  0,    5000, false, "ppm"},
};

#define ITEM_COUNT (sizeof(s_items) / sizeof(s_items[0]))
#define THRESH_SNAPSHOT_PAYLOAD_SIZE (1 + 8 * 3)

static bool     s_active          = false;
static uint8_t  s_index           = 0;
static bool     s_dirty           = false;
static bool     s_edit_mode       = false;  /* false=导航模式, true=编辑模式 */
static int16_t  s_saved_val       = 0;     /* 进入编辑前的备份值 */
static uint32_t s_long_press_tick = 0;     /* 上次长按被消费的时刻，防止穿透 */

#define THRESH_LONG_PRESS_COOLDOWN_MS 1000U

#define long_press_ready() \
    (key_check_long_press(KEY_NAME_ENTER) && \
     (HAL_GetTick() - s_long_press_tick >= THRESH_LONG_PRESS_COOLDOWN_MS))

static int16_t *field_ptr(uint8_t idx)
{
    return (int16_t *)((uint8_t *)&g_sys_config.threshold[0] + s_items[idx].offset);
}

static void append_threshold_field(uint8_t *payload, uint8_t *len, uint8_t field_id, int16_t value)
{
    payload[(*len)++] = field_id;
    payload[(*len)++] = (uint8_t)(value & 0xFF);
    payload[(*len)++] = (uint8_t)((value >> 8) & 0xFF);
}

static void report_threshold_snapshot(void)
{
    const threshold_config_t *th = &g_sys_config.threshold[0];
    uint8_t payload[THRESH_SNAPSHOT_PAYLOAD_SIZE];
    uint8_t len = 0;

    payload[len++] = TF_DATA_KIND_THRESHOLD_SNAPSHOT;
    append_threshold_field(payload, &len, TF_THRESH_FIELD_TEMP_HIGH, th->temp_high);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_TEMP_LOW, th->temp_low);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_HUMI_HIGH, (int16_t)th->humi_high);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_HUMI_LOW, (int16_t)th->humi_low);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_SOIL_DRY, (int16_t)th->soil_dry);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_LIGHT_LOW, (int16_t)th->light_low);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_CO2_HIGH, (int16_t)th->co2_high);
    append_threshold_field(payload, &len, TF_THRESH_FIELD_ENABLE, (int16_t)th->enable);

    if (TF_Slave_ReportData(&tf, payload, len))
    {
        log_info("Threshold snapshot reported (%d bytes)", len);
    }
    else
    {
        log_warn("Threshold snapshot report failed");
    }
}

static void fmt_val(char *buf, uint8_t idx)
{
    const edit_item_t *item = &s_items[idx];
    int16_t val = *field_ptr(idx);
    if (item->div10)
    {
        int16_t a = (int16_t)(val < 0 ? -val : val);
        sprintf(buf, "%s%d.%d%s",
                val < 0 ? "-" : "",
                a / 10, a % 10,
                item->unit);
    }
    else
    {
        sprintf(buf, "%d%s", val, item->unit);
    }
}

/*
 * 三行滚动列表:  上一项 / 当前项(光标) / 下一项
 *
 * 导航模式  当前行: [Name:value]   — 方括号包住整行
 * 编辑模式  当前行: >Name:[value]  — > 前缀 + 方括号包住值
 * 其余行:          " Name:value"  — 空格对齐
 */
static void draw_edit_screen(void)
{
    char buf[24];
    char line[28];

    OLED_ClearBuffer();

    static const uint8_t ROW_Y[3] = {13, 30, 47};

    for (int8_t rel = -1; rel <= 1; rel++)
    {
        int8_t idx = (int8_t)s_index + rel;
        if (idx < 0 || idx >= (int8_t)ITEM_COUNT)
            continue;

        uint8_t row = (uint8_t)(rel + 1);
        fmt_val(buf, (uint8_t)idx);

        if (rel == 0)
        {
            if (s_edit_mode)
                snprintf(line, sizeof(line), ">%s:[%s]", s_items[idx].name, buf);
            else
                snprintf(line, sizeof(line), "[%s:%s]", s_items[idx].name, buf);
        }
        else
        {
            snprintf(line, sizeof(line), " %s:%s", s_items[idx].name, buf);
        }
        OLED_DrawStr(0, ROW_Y[row], line);
    }

    OLED_DrawLine(0, 52, 127, 52);

    if (s_edit_mode)
        snprintf(buf, sizeof(buf), "UD:+/- OK:ok LO:cxl");
    else
        snprintf(buf, sizeof(buf), "UD:sel OK:edt LO:sv");
    OLED_DrawStr(0, 63, buf);

    OLED_SendBuffer();
}

void app_threshold_ui_init(void)
{
    s_active          = false;
    s_index           = 0;
    s_dirty           = false;
    s_edit_mode       = false;
    s_long_press_tick = 0;
}

bool app_threshold_ui_is_active(void)
{
    return s_active;
}

void app_threshold_ui_poll(void)
{
    if (!s_active)
    {
        if (long_press_ready())
        {
            s_long_press_tick = HAL_GetTick();
            s_active          = true;
            s_index           = 0;
            s_edit_mode       = false;
            s_dirty           = true;
            log_info("Threshold edit: enter");
        }
        return;
    }

    bool changed = false;

    if (!s_edit_mode)
    {
        /* ======== 导航模式 ======== */

        if (key_check_press(KEY_NAME_UP))
        {
            if (s_index > 0)
            {
                s_index--;
                changed = true;
            }
        }

        if (key_check_press(KEY_NAME_DOWN))
        {
            if (s_index < ITEM_COUNT - 1)
            {
                s_index++;
                changed = true;
            }
        }

        /* 短按 ENTER: 进入编辑模式，备份当前值 */
        if (key_check_press(KEY_NAME_ENTER))
        {
            s_saved_val = *field_ptr(s_index);
            s_edit_mode = true;
            key_set_continue(KEY_NAME_UP, true);
            key_set_continue(KEY_NAME_DOWN, true);
            changed = true;
        }

        /* 长按 ENTER: 保存并上报，退出（冷却期内不响应，防止穿透）*/
        if (long_press_ready())
        {
            s_long_press_tick = HAL_GetTick();
            app_config_save();
            report_threshold_snapshot();
            s_active    = false;
            s_edit_mode = false;
            log_info("Threshold edit: saved, reported & exit");
            return;
        }
    }
    else
    {
        /* ======== 编辑模式 ======== */

        if (key_check_press(KEY_NAME_UP))
        {
            int16_t val = *field_ptr(s_index);
            val += s_items[s_index].step;
            if (val > s_items[s_index].max_val)
                val = s_items[s_index].max_val;
            *field_ptr(s_index) = val;
            changed = true;
        }

        if (key_check_press(KEY_NAME_DOWN))
        {
            int16_t val = *field_ptr(s_index);
            val -= s_items[s_index].step;
            if (val < s_items[s_index].min_val)
                val = s_items[s_index].min_val;
            *field_ptr(s_index) = val;
            changed = true;
        }

        /* 短按 ENTER: 确认，返回导航模式 */
        if (key_check_press(KEY_NAME_ENTER))
        {
            s_edit_mode = false;
            key_set_continue(KEY_NAME_UP, false);
            key_set_continue(KEY_NAME_DOWN, false);
            changed = true;
        }

        /* 长按 ENTER: 取消，还原备份值，返回导航模式 */
        if (long_press_ready())
        {
            s_long_press_tick = HAL_GetTick();
            *field_ptr(s_index) = s_saved_val;
            s_edit_mode = false;
            key_set_continue(KEY_NAME_UP, false);
            key_set_continue(KEY_NAME_DOWN, false);
            changed = true;
        }
    }

    if (changed || s_dirty)
    {
        s_dirty = false;
        draw_edit_screen();
    }
}
