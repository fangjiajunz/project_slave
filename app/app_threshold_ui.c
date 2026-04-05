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
    {"Soil Dry",  FOFF(soil_dry), 100,  0,    4095, false, ""},
    {"Light Low", FOFF(light_low), 100, 0,    4095, false, ""},
    {"CO2 High",  FOFF(co2_high),  50,  0,    5000, false, "ppm"},
};

#define ITEM_COUNT (sizeof(s_items) / sizeof(s_items[0]))
#define THRESH_SNAPSHOT_PAYLOAD_SIZE (1 + 8 * 3)

static bool    s_active = false;
static uint8_t s_index  = 0;
static bool    s_dirty  = false;

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

static void draw_edit_screen(void)
{
    char buf[24];

    OLED_ClearBuffer();

    static const uint8_t ROW_Y[3] = {13, 30, 47};

    for (int8_t rel = -1; rel <= 1; rel++)
    {
        int8_t idx = (int8_t)s_index + rel;
        if (idx < 0 || idx >= (int8_t)ITEM_COUNT)
            continue;

        uint8_t row = (uint8_t)(rel + 1);

        OLED_DrawStr(0, ROW_Y[row], rel == 0 ? ">" : " ");

        fmt_val(buf, (uint8_t)idx);
        char line[24];
        snprintf(line, sizeof(line), "%s:%s", s_items[idx].name, buf);
        OLED_DrawStr(8, ROW_Y[row], line);
    }

    OLED_DrawLine(0, 52, 127, 52);
    snprintf(buf, sizeof(buf), "U:+ D:- [%d/%d] OK:>",
             s_index + 1, (int)ITEM_COUNT);
    OLED_DrawStr(0, 63, buf);

    OLED_SendBuffer();
}

void app_threshold_ui_init(void)
{
    s_active = false;
    s_index  = 0;
    s_dirty  = false;
}

bool app_threshold_ui_is_active(void)
{
    return s_active;
}

void app_threshold_ui_poll(void)
{
    if (!s_active)
    {
        if (key_check_long_press(KEY_NAME_ENTER))
        {
            s_active = true;
            s_index  = 0;
            s_dirty  = true;
            key_set_continue(KEY_NAME_UP, true);
            key_set_continue(KEY_NAME_DOWN, true);
            log_info("Threshold edit: enter");
        }
        return;
    }

    bool changed = false;

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

    if (key_check_press(KEY_NAME_ENTER))
    {
        s_index++;
        if (s_index >= ITEM_COUNT)
        {
            app_config_save();
            report_threshold_snapshot();
            s_active = false;
            key_set_continue(KEY_NAME_UP, false);
            key_set_continue(KEY_NAME_DOWN, false);
            log_info("Threshold edit: saved, reported & exit");
            return;
        }
        changed = true;
    }

    if (changed || s_dirty)
    {
        s_dirty = false;
        draw_edit_screen();
    }
}
