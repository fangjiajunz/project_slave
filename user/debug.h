#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define PRINT_DEBUG_ENABLE  0   /* 打印调试信息 */
#define PRINT_ERR_ENABLE    0   /* 打印错误信息 */
#define PRINT_INFO_ENABLE   0   /* 打印信息 */

/* === 修改后的输出格式: [TAG] [file:line] <func> 内容 === */

#if PRINT_DEBUG_ENABLE
#define PRINT_DEBUG(fmt, ...) \
    do { printf("[DEBUG] [%s:%d] <%s> " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#else
#define PRINT_DEBUG(fmt, ...)
#endif

#if PRINT_ERR_ENABLE
#define PRINT_ERR(fmt, ...) \
    do { printf("[ERR] [%s:%d] <%s> " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#else
#define PRINT_ERR(fmt, ...)
#endif

#if PRINT_INFO_ENABLE
#define PRINT_INFO(fmt, ...) \
    do { printf("[INFO] [%s:%d] <%s> " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#else
#define PRINT_INFO(fmt, ...)
#endif

#endif /* _DEBUG_H */
