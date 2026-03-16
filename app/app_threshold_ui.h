#ifndef APP_THRESHOLD_UI_H
#define APP_THRESHOLD_UI_H

#include <stdbool.h>

/**
 * @brief  初始化阈值编辑 UI 模块
 */
void app_threshold_ui_init(void);

/**
 * @brief  是否正在编辑模式中
 * @retval true = 编辑模式, OLED 由本模块控制; false = 正常显示
 */
bool app_threshold_ui_is_active(void);

/**
 * @brief  主循环轮询: 按键检测 + 刷新 OLED 编辑画面
 */
void app_threshold_ui_poll(void);

#endif /* APP_THRESHOLD_UI_H */
