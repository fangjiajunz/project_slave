#ifndef APP_DEVCTRL_UI_H
#define APP_DEVCTRL_UI_H

#include <stdint.h>

/**
 * @brief  初始化设备控制 UI 模块
 */
void app_devctrl_ui_init(void);

/**
 * @brief  主循环轮询: UP/DOWN 切换设备, ENTER 短按 toggle 开关
 *         仅在阈值编辑模式非激活时响应
 */
void app_devctrl_ui_poll(void);

/**
 * @brief  获取当前选中设备的状态字符串 (用于 OLED 底部显示)
 * @return 格式如 ">Fan:OFF" 或 ">LED:ON"
 */
const char *app_devctrl_ui_status_str(void);

#endif /* APP_DEVCTRL_UI_H */
