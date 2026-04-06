#include "app_display.h"
#include "dispDirver.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOP_COL1    2
#define TOP_COL2    44
#define TOP_COL3    87
#define TOP_LABEL_Y 10
#define TOP_VALUE_Y 22

#define BOT_COL1    2
#define BOT_COL2    66
#define BOT_LABEL_Y 38
#define BOT_VALUE_Y 50

#define DIV_H_Y     26
#define DIV_TOP_V1  42
#define DIV_TOP_V2  85
#define DIV_BOT_V   63
#define DIV_H2_Y    54

#define STATUS_Y    63
#define OLED_TIMEOUT_MS  30000

static uint32_t s_last_activity_tick = 0;
static bool     s_oled_off = false;

/* ---- 弹窗状态 ---- */
#define ALERT_DURATION_MS  3000
#define ALERT_X            10
#define ALERT_Y            12
#define ALERT_W            108
#define ALERT_H            38
#define ALERT_CORNER_R     4

static char     s_alert_title[16] = {0};
static char     s_alert_msg[16]   = {0};
static uint32_t s_alert_off_tick  = 0;   /* 0 = 无弹窗 */

static void draw_alert_overlay(void)
{
    /* 背景白色实心圆角矩形（覆盖内容）*/
    OLED_SetDrawColor(1);
    OLED_DrawRBox(ALERT_X, ALERT_Y, ALERT_W, ALERT_H, ALERT_CORNER_R);

    /* 黑色边框 */
    OLED_SetDrawColor(0);
    OLED_DrawRFrame(ALERT_X, ALERT_Y, ALERT_W, ALERT_H, ALERT_CORNER_R);

    /* 标题行（黑字）*/
    OLED_DrawStr(ALERT_X + 6, ALERT_Y + 13, s_alert_title);

    /* 分割线 */
    OLED_DrawLine(ALERT_X + 4, ALERT_Y + 16, ALERT_X + ALERT_W - 5, ALERT_Y + 16);

    /* 内容行 */
    OLED_DrawStr(ALERT_X + 6, ALERT_Y + 30, s_alert_msg);

    OLED_SetDrawColor(1);  /* 恢复默认颜色 */
}

static uint16_t soil_estimate_percent_x10(uint16_t soil_raw)
{
    if (soil_raw >= 4095U)
        return 0;
    if (soil_raw <= 1000U)
        return 1000;

    return (uint16_t)(((uint32_t)(4095U - soil_raw) * 1000U) /
                      (4095U - 1000U));
}

void app_display_init(void)
{
    Disp_Init();
    s_last_activity_tick = HAL_GetTick();
    s_oled_off = false;
}

void app_display_activity(void)
{
    s_last_activity_tick = HAL_GetTick();
    if (s_oled_off)
    {
        OLED_SetPowerSave(0);
        s_oled_off = false;
    }
}

void app_display_timeout_check(void)
{
    if (!s_oled_off && (HAL_GetTick() - s_last_activity_tick >= OLED_TIMEOUT_MS))
    {
        OLED_SetPowerSave(1);
        s_oled_off = true;
    }
}

bool app_display_is_off(void)
{
    return s_oled_off;
}

void app_display_sensor(int16_t temp, uint16_t humi,
                        uint16_t light, uint16_t soil,
                        uint16_t co2, const char *dev_status)
{
    char buf[16];
    uint16_t soil_percent_x10;

    if (s_oled_off)
        return;

    soil_percent_x10 = soil_estimate_percent_x10(soil);

    OLED_ClearBuffer();
    OLED_DrawLine(0, DIV_H_Y, 127, DIV_H_Y);
    OLED_DrawLine(DIV_TOP_V1, 0, DIV_TOP_V1, DIV_H_Y);
    OLED_DrawLine(DIV_TOP_V2, 0, DIV_TOP_V2, DIV_H_Y);
    OLED_DrawLine(DIV_BOT_V, DIV_H_Y, DIV_BOT_V, DIV_H2_Y);

    OLED_DrawStr(TOP_COL1, TOP_LABEL_Y, "Temp");
    sprintf(buf, "%d.%dC", temp / 10, abs(temp % 10));
    OLED_DrawStr(TOP_COL1, TOP_VALUE_Y, buf);

    OLED_DrawStr(TOP_COL2, TOP_LABEL_Y, "Humi");
    sprintf(buf, "%d.%d%%", humi / 10, humi % 10);
    OLED_DrawStr(TOP_COL2, TOP_VALUE_Y, buf);

    OLED_DrawStr(TOP_COL3, TOP_LABEL_Y, "Light");
    sprintf(buf, "%u", light);
    OLED_DrawStr(TOP_COL3, TOP_VALUE_Y, buf);

    OLED_DrawStr(BOT_COL1, BOT_LABEL_Y, "Soil%");
    sprintf(buf, "%u.%u%%", soil_percent_x10 / 10, soil_percent_x10 % 10);
    OLED_DrawStr(BOT_COL1, BOT_VALUE_Y, buf);

    OLED_DrawStr(BOT_COL2, BOT_LABEL_Y, "CO2");
    sprintf(buf, "%uppm", co2);
    OLED_DrawStr(BOT_COL2, BOT_VALUE_Y, buf);

    if (dev_status)
    {
        OLED_DrawLine(0, DIV_H2_Y, 127, DIV_H2_Y);
        OLED_DrawStr(2, STATUS_Y, dev_status);
    }

    /* 弹窗叠层（有激活弹窗时覆盖在传感器数据上方） */
    if (s_alert_off_tick != 0)
        draw_alert_overlay();

    OLED_SendBuffer();
}

void app_display_alert(const char *title, const char *msg)
{
    if (title) { strncpy(s_alert_title, title, sizeof(s_alert_title) - 1); s_alert_title[sizeof(s_alert_title) - 1] = '\0'; }
    if (msg)   { strncpy(s_alert_msg,   msg,   sizeof(s_alert_msg)   - 1); s_alert_msg[sizeof(s_alert_msg)   - 1] = '\0'; }
    s_alert_off_tick = HAL_GetTick() + ALERT_DURATION_MS;
    if (s_alert_off_tick == 0) s_alert_off_tick = 1;  /* 避免 0 = 禁用 */
    /* 唤醒屏幕 */
    app_display_activity();
}

void app_display_alert_dismiss(void)
{
    s_alert_off_tick = 0;
}

bool app_display_alert_is_active(void)
{
    return (s_alert_off_tick != 0);
}

void app_display_alert_tick(void)
{
    if (s_alert_off_tick != 0 && HAL_GetTick() >= s_alert_off_tick)
        s_alert_off_tick = 0;
}
