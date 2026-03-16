#include "app_devctrl_ui.h"

#include "app_relay.h"
#include "app_threshold.h"
#include "app_threshold_ui.h"
#include "main.h"
#include "tf_multinode.h"

#include <stdio.h>

#define LOG_TAG "DevCtrl"
#include "log.h"

/* ========================== 设备表 ========================== */

typedef struct {
    const char *name;
    uint8_t     dev_id;
} dev_entry_t;

static const dev_entry_t s_devs[] = {
    {"Fan",    TF_CTRL_DEV_FAN},
    {"Heat",   TF_CTRL_DEV_HEATER},
    {"Pump",   TF_CTRL_DEV_PUMP},
    {"LED",    TF_CTRL_DEV_LED},
};

#define DEV_COUNT (sizeof(s_devs) / sizeof(s_devs[0]))

/* ========================== 状态变量 ========================== */

static uint8_t s_dev_index = 0;
static char    s_status_buf[16];

/* ========================== 公共接口 ========================== */

void app_devctrl_ui_init(void)
{
    s_dev_index = 0;
}

void app_devctrl_ui_poll(void)
{
    /* 阈值编辑模式时不响应 */
    if (app_threshold_ui_is_active())
        return;

    /* UP: 上一个设备 */
    if (key_check_press(KEY_NAME_UP))
    {
        if (s_dev_index == 0)
            s_dev_index = DEV_COUNT - 1;
        else
            s_dev_index--;
    }

    /* DOWN: 下一个设备 */
    if (key_check_press(KEY_NAME_DOWN))
    {
        s_dev_index++;
        if (s_dev_index >= DEV_COUNT)
            s_dev_index = 0;
    }

    /* ENTER 短按: toggle 设备开关 */
    if (key_check_press(KEY_NAME_ENTER))
    {
        uint8_t dev_id = s_devs[s_dev_index].dev_id;
        uint8_t cur = app_relay_get(dev_id);
        app_relay_set(dev_id, !cur);
        app_threshold_manual_override(dev_id);
        log_info("DevCtrl: %s -> %s", s_devs[s_dev_index].name, cur ? "OFF" : "ON");
    }
}

const char *app_devctrl_ui_status_str(void)
{
    uint8_t dev_id = s_devs[s_dev_index].dev_id;
    uint8_t state = app_relay_get(dev_id);
    sprintf(s_status_buf, ">%s:%s", s_devs[s_dev_index].name, state ? "ON" : "OFF");
    return s_status_buf;
}
