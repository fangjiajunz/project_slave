#include "app_threshold_ui.h"

#include "app.h"
#include "config.h"
#include "dispDirver.h"
#include "main.h"

#include <stddef.h>
#include <stdio.h>

#define LOG_TAG "ThreshUI"
#include "log.h"

/* ========================== 编辑项描述表 ========================== */

typedef struct {
    const char *name;       /* 显示名称 */
    uint8_t    offset;      /* offsetof(threshold_config_t, field) */
    int16_t    step;        /* UP/DOWN 步长 */
    int16_t    min_val;     /* 最小值 */
    int16_t    max_val;     /* 最大值 */
    bool       div10;       /* 是否 /10 显示 */
    const char *unit;       /* 单位字符串 ("" 表示无单位) */
} edit_item_t;

#define FOFF(f) ((uint8_t)offsetof(threshold_config_t, f))

static const edit_item_t s_items[] = {
    {"Temp Hi",  FOFF(temp_high), 10,  -200, 600,  true,  "C"},
    {"Temp Lo",  FOFF(temp_low),  10,  -200, 600,  true,  "C"},
    {"Humi Hi",  FOFF(humi_high), 10,  0,    1000, true,  "%"},
    {"Humi Lo",  FOFF(humi_low),  10,  0,    1000, true,  "%"},
    {"PH Thrs", FOFF(soil_dry),  100, 0,    4095, false, ""},
    {"Light Lo", FOFF(light_low), 100, 0,    4095, false, ""},
    {"CO2 Hi",   FOFF(co2_high),  50,  0,    5000, false, "ppm"},
};

#define ITEM_COUNT (sizeof(s_items) / sizeof(s_items[0]))

/* ========================== 状态变量 ========================== */

static bool    s_active = false;   /* 是否在编辑模式 */
static uint8_t s_index  = 0;      /* 当前编辑项 0~6 */
static bool    s_dirty  = false;   /* 需要刷新屏幕 */

/* ========================== 字段访问 ========================== */

/* 所有编辑字段均为 2 字节，值范围 -200~5000，int16_t 可覆盖 */
static int16_t *field_ptr(uint8_t idx)
{
    return (int16_t *)((uint8_t *)&g_sys_config.threshold[0] + s_items[idx].offset);
}

/* ========================== OLED 绘制 ========================== */

static void draw_edit_screen(void)
{
    const edit_item_t *item = &s_items[s_index];
    int16_t val = *field_ptr(s_index);
    char buf[24];

    OLED_ClearBuffer();

    /* 行1 y=12: "[n/7] Name" */
    sprintf(buf, "[%d/%d] %s", s_index + 1, (int)ITEM_COUNT, item->name);
    OLED_DrawStr(0, 12, buf);

    /* 行2 y=38: 当前值 (居中) */
    if (item->div10)
    {
        int16_t abs_val = (int16_t)(val < 0 ? -val : val);
        sprintf(buf, "%s%d.%d%s%s",
                val < 0 ? "-" : "",
                abs_val / 10, abs_val % 10,
                item->unit[0] ? " " : "",
                item->unit);
    }
    else
    {
        sprintf(buf, "%d%s%s", val,
                item->unit[0] ? " " : "",
                item->unit);
    }
    uint16_t w = OLED_GetStrWidth(buf);
    OLED_DrawStr((128 - w) / 2, 38, buf);

    /* 行3 y=60: 操作提示 */
    OLED_DrawStr(0, 60, "UP:+ DOWN:- OK:>");

    OLED_SendBuffer();
}

/* ========================== 公共接口 ========================== */

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
        /* 非编辑模式: ENTER 长按进入编辑 */
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

    /* ---- 编辑模式 ---- */
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
            /* 全部编辑完成，保存 NVM 并退出 */
            app_config_save();
            s_active = false;
            key_set_continue(KEY_NAME_UP, false);
            key_set_continue(KEY_NAME_DOWN, false);
            log_info("Threshold edit: saved & exit");
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
