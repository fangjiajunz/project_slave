#ifndef APP_H
#define APP_H

#include "config.h"

/* 全局系统配置 */
extern sys_config_t g_sys_config;

/* 初始化 NVM 并加载配置，在 e22_demo_init() 之前调用 */
void app_start(void);

/* 将当前配置保存到 Flash (先从 user_config 同步再写入) */
void app_config_save(void);

#endif
