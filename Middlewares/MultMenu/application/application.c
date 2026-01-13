#include "application.h"

#include "dispDirver.h"
#include "main.h"

/**
 * 菜单 Setting > 参数默认显示值
 */
menu_config_t user_config =
    {
        .lora_sf = 11,
        .lora_bw = 500,
        .lora_cr = 5,
        .frequency_mhz = 915,
        .tx_power = 22,
        .tx_count = 10,
};

/* 注意为xbm格式数据
   可以使用网络在线工具转换 https://convertio.co/zh/xbm-converter/ */
// clang-format off
static const unsigned char ebyte_logo[861] = {
       0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 
       0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 
       0x70, 0x00, 0x00, 0x00, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 
       0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x80, 0x9C, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x06, 0x18, 0x00, 0x00, 0x60, 0xC0, 0x01, 0x00, 0xC0, 0x26, 0x01, 
       0x00, 0x00, 0x00, 0x00, 0x03, 0x0C, 0x00, 0x00, 0x60, 0x80, 0x01, 0x00, 
       0x40, 0x26, 0x01, 0x00, 0x00, 0x00, 0x80, 0x01, 0x06, 0x00, 0x00, 0xC0, 
       0x00, 0x03, 0x00, 0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x01, 0x03, 
       0x03, 0x80, 0x81, 0x01, 0x07, 0x00, 0x40, 0x16, 0x01, 0x00, 0x00, 0x00, 
       0xC0, 0x80, 0x83, 0x03, 0x80, 0x03, 0x03, 0x06, 0x00, 0xC0, 0x36, 0x01, 
       0x00, 0x00, 0x00, 0x60, 0xC0, 0xC1, 0x01, 0x00, 0x07, 0x07, 0x0C, 0x00, 
       0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0xC0, 0xC0, 0x00, 0x00, 0x06, 
       0x06, 0x0C, 0x00, 0x00, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x60, 0xC0, 0x60, 
       0xC0, 0x07, 0x0C, 0x06, 0x0C, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 
       0x60, 0xC0, 0x60, 0xC0, 0x0F, 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x30, 0x60, 0x60, 0xE0, 0x0F, 0x08, 0x0C, 0x18, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0x20, 0xE0, 0x0F, 0x18, 
       0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0x20, 
       0xE0, 0x0F, 0x18, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x30, 0x60, 0x60, 0xC0, 0x0F, 0x18, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x20, 0x40, 0x60, 0x80, 0x03, 0x0C, 0x0C, 0x18, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xC0, 0x60, 0x00, 0x00, 0x0C, 
       0x06, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xC0, 0xC0, 
       0x00, 0x00, 0x06, 0x06, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x60, 0x80, 0xC1, 0x01, 0x00, 0x06, 0x07, 0x0C, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0xC0, 0x80, 0x83, 0x03, 0x80, 0x03, 0x03, 0x0E, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x01, 0x03, 0x03, 0x80, 0x81, 
       0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x06, 
       0xC0, 0x07, 0xC0, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x03, 0x0C, 0xC0, 0x07, 0xE0, 0x80, 0x03, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0xC0, 0x07, 0x00, 0x80, 0x01, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0xC0, 0x07, 0x00, 
       0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 
       0xC0, 0x07, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0xC0, 0x0F, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x0F, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x0F, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0xC0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0xFF, 0xFF, 0xE7, 0xFF, 0x3F, 0xF0, 0x03, 0xE0, 0xCF, 0xFF, 0xFF, 0xCF, 
       0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xE0, 0x07, 0xE0, 0xC7, 
       0xFF, 0xFF, 0xCF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xC7, 0xFF, 0xFF, 0xC1, 
       0x0F, 0xF0, 0xC3, 0xFF, 0xFF, 0xCF, 0xFF, 0xFF, 0x01, 0x3F, 0x00, 0xC0, 
       0x07, 0xF8, 0xC3, 0x0F, 0xF8, 0x01, 0xC0, 0x0F, 0xC0, 0x0F, 0x00, 0x00, 
       0x1F, 0x00, 0xC0, 0x07, 0xF0, 0x83, 0x1F, 0xFC, 0x00, 0xC0, 0x0F, 0xC0, 
       0x07, 0x00, 0x00, 0x3F, 0x00, 0xC0, 0x07, 0xF0, 0x01, 0x3F, 0x7E, 0x00, 
       0xC0, 0x0F, 0xC0, 0x07, 0x00, 0x00, 0x3F, 0x00, 0xE0, 0x07, 0xFC, 0x00, 
       0x7E, 0x3E, 0x00, 0xC0, 0x0F, 0xC0, 0x0F, 0x00, 0x00, 0xFF, 0xFF, 0xC3, 
       0xFF, 0x3F, 0x00, 0xFC, 0x1F, 0x00, 0xC0, 0x0F, 0xC0, 0xFF, 0xFF, 0x00, 
       0xFF, 0xFF, 0xC3, 0xFF, 0x3F, 0x00, 0xF8, 0x0F, 0x00, 0xC0, 0x0F, 0xC0, 
       0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0x01, 0xF0, 0x07, 0x00, 
       0xC0, 0x0F, 0xC0, 0xFF, 0xFF, 0x00, 0x3F, 0x00, 0xC0, 0x07, 0xF0, 0x03, 
       0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 0x07, 0x00, 0x00, 0x3F, 0x00, 0xC0, 
       0x07, 0xE0, 0x03, 0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 0x07, 0x00, 0x00, 
       0x3F, 0x00, 0xE0, 0x07, 0xE0, 0x07, 0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 
       0x07, 0x00, 0x00, 0x3F, 0x00, 0xC0, 0x07, 0xF0, 0x03, 0xE0, 0x03, 0x00, 
       0xC0, 0x0F, 0xC0, 0x07, 0x00, 0x00, 0xFF, 0xFF, 0xC7, 0xFF, 0xFF, 0x03, 
       0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xC7, 
       0xFF, 0xFF, 0x01, 0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 0xFF, 0xFF, 0x01, 
       0xFF, 0xFF, 0xC7, 0xFF, 0x7F, 0x00, 0xE0, 0x03, 0x00, 0xC0, 0x0F, 0xC0, 
       0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, };

// clang-format on
/**
 * Tx Mode 数据发送测试 过程状态
 */
typedef enum
{
    TX_MODE_INIT = 0,
    TX_MODE_SEND,
    TX_MODE_WAIT,
    TX_MODE_END,
} tx_state_t;

/**
 * Rx Mode 数据接收测试 过程状态
 */
typedef enum
{
    RX_MODE_INIT = 0,
    RX_MODE_RECV,
    RX_MODE_WAIT,
    RX_MODE_END,
} rx_state_t;

/**
 * Rx Mode 数据接收测试 提示光标状态
 */
typedef enum
{
    HINT_WAIT = 0,
    HINT_DONE,
} hint_state_t;

/**
 * @brief  菜单 进入首页
 *
 * @param  item 指向项目状态的指针
 */
void logo_callback(xpItem item)
{
    OLED_ClearBuffer();
    OLED_DrawXBMP(4, 6, 118, 57, ebyte_logo);
    OLED_SendBuffer();
    switch (item->state)
    {
        case MENU_UP:
        case MENU_DOWN:
            item->state = MENU_ENTER;
            break;
        default:
            break;
    }
}

/**
 * @brief  菜单三级页面 扩频因子设定 Setting > LoRa SF
 *
 * @param  item 指向项目状态的指针
 */
void lora_sf_callback(xpItem item)
{
    char value[20] = {0};
    int scrollbar_min = 5;
    int scrollbar_max = 12;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 1, (*item->data.ptr));
                break;
            case MENU_DOWN:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -1, (*item->data.ptr));
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, (*item->data.ptr));
                break;
        }

        sprintf(value, "SF: %d ", (*item->data.ptr));
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 载波带宽设定 Setting > LoRa BW
 *
 * @param  item 指向项目状态的指针
 */
void lora_bw_callback(xpItem item)
{
    char value[20] = {0};
    int sf_convert[3] = {125, 250, 500};
    /* 对应 sf_convert 范围 */
    int scrollbar_min = 0;
    int scrollbar_max = 2;
    /* 默认对应sf_convert中的 500  */
    static int scrollbar_value = 2;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                scrollbar_value = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 1, scrollbar_value);
                (*item->data.ptr) = sf_convert[scrollbar_value];
                break;
            case MENU_DOWN:
                scrollbar_value = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -1, scrollbar_value);
                (*item->data.ptr) = sf_convert[scrollbar_value];
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, scrollbar_value);
                break;
        }

        sprintf(value, "BW: %d %s", (*item->data.ptr), "KHz");
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 编码率设定 Setting > LoRa CR
 *
 * @param  item 指向项目状态的指针
 */
void lora_cr_callback(xpItem item)
{
    char value[20] = {0};
    int scrollbar_min = 5;
    int scrollbar_max = 8;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 1, (*item->data.ptr));
                break;
            case MENU_DOWN:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -1, (*item->data.ptr));
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, (*item->data.ptr));
                break;
        }

        sprintf(value, "CR: 4/%d ", (*item->data.ptr));
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 通信载波频率设定 Setting > Frequency
 *
 * @param  item 指向项目状态的指针
 */
void lora_frequency_callback(xpItem item)
{
    char value[20] = {0};
    int scrollbar_min = 850;
    int scrollbar_max = 930;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 1, (*item->data.ptr));
                break;
            case MENU_DOWN:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -1, (*item->data.ptr));
                break;
            case MENU_ENTER:
                key_set_continue(KEY_NAME_UP, false);
                key_set_continue(KEY_NAME_DOWN, false);
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, (*item->data.ptr));
                key_set_continue(KEY_NAME_UP, true);
                key_set_continue(KEY_NAME_DOWN, true);
                break;
        }

        sprintf(value, "Freq: %d %s", (*item->data.ptr), "MHz");
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 发射功率设定 Setting > TX Power
 *
 * @param  item 指向项目状态的指针
 */
void tx_power_callback(xpItem item)
{
    char value[20] = {0};
    int scrollbar_min = -9;
    int scrollbar_max = 22;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 1, (*item->data.ptr));
                break;
            case MENU_DOWN:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -1, (*item->data.ptr));
                break;
            case MENU_ENTER:
                key_set_continue(KEY_NAME_UP, false);
                key_set_continue(KEY_NAME_DOWN, false);
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, (*item->data.ptr));
                key_set_continue(KEY_NAME_UP, true);
                key_set_continue(KEY_NAME_DOWN, true);
                break;
        }

        sprintf(value, "Power: %d %s", (*item->data.ptr), "dBm");
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 模组通信测试数据发射次数设定 Setting > TX Count
 *
 * @param  item 指向项目状态的指针
 */
void tx_count_callback(xpItem item)
{
    char value[20] = {0};
    int scrollbar_min = 10;
    int scrollbar_max = 100;

    if (DialogScale_Show(0, 0, 127, 63))
    {
        switch (item->state)
        {
            case MENU_UP:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 10, (*item->data.ptr));
                break;
            case MENU_DOWN:
                (*item->data.ptr) = Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, -10, (*item->data.ptr));
                break;
            default:
                Draw_Scrollbar(8, 30, 111, 6, 0, scrollbar_min, scrollbar_max, 0, (*item->data.ptr));
                break;
        }

        sprintf(value, "Count: %d ", (*item->data.ptr));
        OLED_DrawStr(8, 25, value);
        OLED_SendBuffer();
    }
}

/**
 * @brief  菜单三级页面 显示背景色设定 Setting > Back Color
 *
 * @param  item 指向项目状态的指针
 */
void background_color_callback(xpItem item)
{
    Set_BgColor(item->switchState);
}

/**
 * @brief  Tx Mode 数据发送测试 构造发送数据
 *
 * @param  send_count 当前发送计数
 */
static void tx_e22_send(int send_count)
{
    char value[30] = {0};
    uint8_t tx_length = 0;

    /* e22 发送 */
    tx_length = sprintf(value, "TX.%03d.%03d", user_config.tx_count, send_count);
    e22_demo_transmit((uint8_t *)value, tx_length + 1);
}

/**
 * @brief  Tx Mode 数据发送测试 发送过程显示
 *
 * @param  send_count 当前发送计数
 */
static void tx_display_count(int send_count)
{
    char value[30] = {0};
    uint8_t bg_color;
    uint8_t tx_length = 0;

    /* e22 发送 */
    tx_length = sprintf(value, "TX.%03d.%03d", user_config.tx_count, send_count);
    e22_demo_transmit((uint8_t *)value, tx_length + 1);

    /* 第一次则刷新全部显示信息 */
    if (send_count == 0)
    {
        /* display refresh */
        bg_color = Get_BgColor();

        OLED_ClearBuffer();
        OLED_SetDrawColor(bg_color);
        OLED_DrawBox(0, 0, 128, 64);
        OLED_SetDrawColor(bg_color ^ 0x01);

        sprintf(value, "LoRa:SF%d BW%d CR4/%d", user_config.lora_sf, user_config.lora_bw, user_config.lora_cr);
        OLED_DrawStr(0, Font_Size * 1, value);
        OLED_DrawLine(0, Font_Size * 1 + 3, 127, Font_Size * 1 + 3);

        sprintf(value, "Freq:%dMHz Pwr:%ddBm", user_config.frequency_mhz, user_config.tx_power);
        OLED_DrawStr(0, Font_Size * 2 + 3, value);
        OLED_DrawLine(0, Font_Size * 2 + 6, 127, Font_Size * 2 + 6);

        sprintf(value, "Tx Total :%d", user_config.tx_count);
        OLED_DrawStr(0, Font_Size * 3 + 9, value);
    }

    /* 如果是最后一次发送 */
    if (send_count == user_config.tx_count)
    {
        OLED_DrawStr(0, Font_Size * 3 + 9, "Tx Done !     ");
        OLED_DrawStr(0, Font_Size * 4 + 9, "Press DOWN button");
    }
    else
    {
        sprintf(value, "Tx Number:%d", send_count);
        OLED_DrawStr(0, Font_Size * 4 + 9, value);
    }

    OLED_SendBuffer();
}

/**
 * @brief  菜单二级页面 模组发送测试 Tx Mode
 *
 * @param  item 指向项目状态的指针
 */
void tx_mode_callback(xpItem item)
{
    static int send_count = 0;
    static uint32_t tick_start;
    static tx_state_t tx_state = TX_MODE_END;

    switch (item->state)
    {
        case MENU_UP:
            break;
        case MENU_DOWN:
            /* 再次开始 */
            if (tx_state == TX_MODE_WAIT)
            {
                tx_state = TX_MODE_INIT;
            }
            break;
        case MENU_ENTER:
            /* 结束 */
            tx_state = TX_MODE_END;

            break;
        default:
            /* 开始 */
            if (tx_state == TX_MODE_END)
            {
                tx_state = TX_MODE_INIT;
            }
            break;
    }

    switch (tx_state)
    {
        case TX_MODE_INIT:
            tx_display_count(0);
            e22_demo_menu_config(&user_config);
            tick_start = HAL_GetTick();
            send_count = 1;
            tx_display_count(send_count);
            tx_e22_send(send_count++);
            tx_state = TX_MODE_SEND;
            break;

        case TX_MODE_SEND:
            if ((HAL_GetTick() - tick_start) > 1000)
            {
                tick_start = HAL_GetTick();
                tx_display_count(send_count);
                tx_e22_send(send_count++);

                if (send_count > user_config.tx_count)
                {
                    tx_state = TX_MODE_WAIT;
                }
            }
            break;

        case TX_MODE_WAIT:
            /* 发送完成，等待用户按键 */
            break;

        case TX_MODE_END:
        default:
            tx_state = TX_MODE_END;
            break;
    }
}

/**
 * @brief  Rx Mode 数据接收测试 显示页面初始化
 */
static void rx_init_display(void)
{
    char value[30] = {0};
    uint8_t bg_color;

    bg_color = Get_BgColor();

    OLED_ClearBuffer();
    OLED_SetDrawColor(bg_color);
    OLED_DrawBox(0, 0, 128, 64);
    OLED_SetDrawColor(bg_color ^ 0x01);

    sprintf(value, "LoRa:SF%d BW%d CR4/%d", user_config.lora_sf, user_config.lora_bw, user_config.lora_cr);
    OLED_DrawStr(0, Font_Size * 1, value);

    OLED_DrawLine(0, Font_Size * 1 + 3, 127, Font_Size * 1 + 3);

    sprintf(value, "Freq:%dMHz ", user_config.frequency_mhz);
    OLED_DrawStr(0, Font_Size * 2 + 3, value);

    OLED_DrawLine(0, Font_Size * 2 + 6, 127, Font_Size * 2 + 6);

    OLED_SendBuffer();
}

/**
 * @brief  Rx Mode 数据接收测试 提示光标显示
 *
 * @param  state 光标状态
 */
static void rx_hint(hint_state_t state)
{
    static uint8_t hint_select = 0;
    static const char *hint[3] = {"=  ", "== ", "==="};

    switch (state)
    {
        case HINT_WAIT:
            OLED_DrawStr(100, Font_Size * 2 + 3, hint[hint_select++]);
            if (hint_select > 2)
            {
                hint_select = 0;
            }
            break;

        case HINT_DONE:
            OLED_DrawStr(100, Font_Size * 2 + 3, "DONE");
            break;
    }

    OLED_SendBuffer();
}

/**
 * @brief  Rx Mode 数据接收测试 无线数据包校验与数据解析
 *
 * @param  buffer 执行接收数据缓存
 * @param  length 接收数据长度
 * @param  total_number 发送总数
 * @param  current_number 当前包序号
 */
static bool rx_analysis(uint8_t *buffer, uint8_t length, uint32_t *tx_count, uint32_t *send_count)
{
    /* 长度至少10字节 */
    if (length < 10)
        return false;

    /* 帧头字符判断TX */
    if (buffer[0] != 'T' || buffer[1] != 'X')
        return false;

    /* 拆分出发送总计数与当前序号 */
    sscanf((char *)(buffer + 3), "%d.%d", tx_count, send_count);

    return true;
}

/**
 * @brief  Rx Mode 数据接收测试 接收过程显示
 *
 * @param  total_number 发送总数
 * @param  current_number 但钱
 * @param  rssi 数据包RSSI
 * @param  lost_nubmer 已丢失数据包计数
 * @param  lost_percent 已丢失数据包百分比
 */
static void rx_mode_display(uint32_t total_number, uint32_t rx_count, int8_t rssi, uint32_t lost_nubmer, uint32_t lost_percent)
{
    char value[30] = {0};

    sprintf(value, "Total:%d  ", total_number);
    OLED_DrawStr(0, Font_Size * 3 + 5, value);

    sprintf(value, "Current:%d  ", rx_count);
    OLED_DrawStr(60, Font_Size * 3 + 5, value);

    sprintf(value, "Lost :%d  ", lost_nubmer);
    OLED_DrawStr(0, Font_Size * 4 + 4, value);

    sprintf(value, "Lost P%%:%d%% ", lost_percent);
    OLED_DrawStr(60, Font_Size * 4 + 4, value);

    sprintf(value, "RSSI :%ddBm  ", rssi);
    OLED_DrawStr(0, Font_Size * 5 + 4, value);

    OLED_SendBuffer();
}

/**
 * @brief  菜单二级页面 模组通信测试数据发射次数设定 Rx Mode
 *
 * @param  item 指向项目状态的指针
 */
void rx_mode_callback(xpItem item)
{
    static rx_state_t rx_state = RX_MODE_INIT;
    static uint8_t rx_mode_buffer[255];
    static uint8_t rx_mode_length = 0;
    static uint32_t tick_start;
    static uint32_t tx_number_record = 0;
    static uint32_t tx_total_number = 0;
    static uint32_t rx_count = 0;
    uint32_t tx_current_number = 0;
    int8_t rx_rssi = 0;

    switch (item->state)
    {
        case MENU_UP:
            break;
        case MENU_DOWN:
            /* 再次开始 */
            if (rx_state == RX_MODE_WAIT)
            {
                rx_state = RX_MODE_INIT;
            }
            break;
        case MENU_ENTER:
            /* 结束 */
            rx_state = RX_MODE_END;

            break;
        default:
            /* 开始 */
            if (rx_state == TX_MODE_END)
            {
                rx_state = RX_MODE_INIT;
            }
            break;
    }

    switch (rx_state)
    {
        case RX_MODE_INIT:
            rx_init_display();
            e22_demo_menu_config(&user_config);
            e22_demo_receive();
            tick_start = HAL_GetTick();
            rx_count = 0;
            tx_number_record = 0;
            rx_state = RX_MODE_RECV;
            break;

        case RX_MODE_RECV:
            if (e22_demo_check_rx_done(rx_mode_buffer, &rx_mode_length, &rx_rssi) == true)
            {
                if (rx_analysis(rx_mode_buffer, rx_mode_length, &tx_total_number, &tx_current_number) == true)
                {
                    /* 其他情况，可能是发送端重新开始了，需要重新计数 */
                    if (tx_number_record >= tx_current_number)
                    {
                        rx_count = 0;
                    }

                    tx_number_record = tx_current_number;

                    /* 有效接收计数 */
                    rx_count++;

                    rx_mode_display(tx_total_number, tx_current_number, rx_rssi, (tx_number_record - rx_count), ((tx_number_record - rx_count) * 100 / tx_number_record));

                    if (tx_current_number == tx_total_number)
                    {
                        rx_state = RX_MODE_WAIT;
                        rx_hint(HINT_DONE);
                    }

                    gpio_led_rx_off();
                }
            }
            else
            {
                if ((HAL_GetTick() - tick_start) > 1000)
                {
                    tick_start = HAL_GetTick();
                    rx_hint(HINT_WAIT);
                }
            }

            break;

        case RX_MODE_WAIT:
            /* 接收完成，等待用户按键 */
            break;

        case RX_MODE_END:
        default:
            rx_state = RX_MODE_END;
            break;
    }
}

/**
 * @brief  菜单二级页面 版本信息显示 Version
 *
 * @param  item 指向项目状态的指针
 */
void version_callback(xpItem item)
{
    OLED_ClearBuffer();

    OLED_DrawStr(0, Font_Size * 1, "Model Type:");
    OLED_DrawStr(8, Font_Size * 2, "E22-900M22S");
    OLED_DrawLine(0, Font_Size * 2 + 3, 127, Font_Size * 2 + 3);

    OLED_DrawStr(0, Font_Size * 3 + 3, "Model Fireware:");
    OLED_DrawStr(8, Font_Size * 4 + 3, "NONE");
    OLED_DrawLine(0, Font_Size * 4 + 6, 127, Font_Size * 4 + 6);

    OLED_SendBuffer();
}

/**
 * @brief  菜单二级页面 重启复位
 *
 * @param  item 指向项目状态的指针
 */
void reset_callback(xpItem item)
{
    NVIC_SystemReset();
}
