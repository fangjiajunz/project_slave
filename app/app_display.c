#include "app_display.h"
#include "dispDirver.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * 布局: 上 3 列 + 下 2 列 + 底部设备状态行
 *
 * +----------+----------+----------+
 * | Temp     | Humi     | Light    |  上半区 0~26
 * | 25.0C    | 55.0%    | 3200     |
 * +----------+-----+----+----------+
 * | PH            | CO2            |  下半区 27~52
 * | 2048          | 400ppm         |
 * +---------------+----------------+
 * | >Fan:OFF                       |  底部状态行 53~63
 * +--------------------------------+
 */

/* ---- 上半区: 3 列, 每列 ~42px ---- */
#define TOP_COL1    2       /* Temp  x 起点 */
#define TOP_COL2    44      /* Humi  x 起点 */
#define TOP_COL3    87      /* Light x 起点 */
#define TOP_LABEL_Y 10      /* 标签 baseline */
#define TOP_VALUE_Y 22      /* 数值 baseline */

/* ---- 下半区: 2 列, 每列 64px ---- */
#define BOT_COL1    2       /* PH x 起点 */
#define BOT_COL2    66      /* CO2  x 起点 */
#define BOT_LABEL_Y 38      /* 标签 baseline */
#define BOT_VALUE_Y 50      /* 数值 baseline */

/* ---- 分割线 ---- */
#define DIV_H_Y     26      /* 水平分割线 */
#define DIV_TOP_V1  42      /* 上半区竖线 1 */
#define DIV_TOP_V2  85      /* 上半区竖线 2 */
#define DIV_BOT_V   63      /* 下半区竖线 */
#define DIV_H2_Y    54      /* 底部分割线 */

/* ---- 底部状态行 ---- */
#define STATUS_Y    63

void app_display_init(void)
{
    Disp_Init();
}

void app_display_sensor(int16_t temp, uint16_t humi,
                        uint16_t light, uint16_t soil,
                        uint16_t co2, const char *dev_status)
{
    char buf[16];

    OLED_ClearBuffer();

    /* ---- 分割线 ---- */
    OLED_DrawLine(0, DIV_H_Y, 127, DIV_H_Y);          /* 水平 */
    OLED_DrawLine(DIV_TOP_V1, 0, DIV_TOP_V1, DIV_H_Y); /* 上竖线 1 */
    OLED_DrawLine(DIV_TOP_V2, 0, DIV_TOP_V2, DIV_H_Y); /* 上竖线 2 */
    OLED_DrawLine(DIV_BOT_V, DIV_H_Y, DIV_BOT_V, DIV_H2_Y); /* 下竖线 */

    /* ---- 上左: 温度 ---- */
    OLED_DrawStr(TOP_COL1, TOP_LABEL_Y, "Temp");
    sprintf(buf, "%d.%dC", temp / 10, abs(temp % 10));
    OLED_DrawStr(TOP_COL1, TOP_VALUE_Y, buf);

    /* ---- 上中: 湿度 ---- */
    OLED_DrawStr(TOP_COL2, TOP_LABEL_Y, "Humi");
    sprintf(buf, "%d.%d%%", humi / 10, humi % 10);
    OLED_DrawStr(TOP_COL2, TOP_VALUE_Y, buf);

    /* ---- 上右: 光照 ---- */
    OLED_DrawStr(TOP_COL3, TOP_LABEL_Y, "Light");
    sprintf(buf, "%u", light);
    OLED_DrawStr(TOP_COL3, TOP_VALUE_Y, buf);

    /* ---- 下左: 土壤PH ---- */
    OLED_DrawStr(BOT_COL1, BOT_LABEL_Y, "PH");
    sprintf(buf, "%u", soil);
    OLED_DrawStr(BOT_COL1, BOT_VALUE_Y, buf);

    /* ---- 下右: CO2 ---- */
    OLED_DrawStr(BOT_COL2, BOT_LABEL_Y, "CO2");
    sprintf(buf, "%uppm", co2);
    OLED_DrawStr(BOT_COL2, BOT_VALUE_Y, buf);

    /* ---- 底部: 设备状态行 ---- */
    if (dev_status)
    {
        OLED_DrawLine(0, DIV_H2_Y, 127, DIV_H2_Y);  /* 底部分割线 */
        OLED_DrawStr(2, STATUS_Y, dev_status);
    }

    OLED_SendBuffer();
}
