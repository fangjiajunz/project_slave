#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define LOG_VERSION "0.2.0"

/*
 * USB 调试输出总开关 (编译时):
 *   设为 0 可彻底关闭所有日志输出，节省 Flash (字符串常量) 和 CPU 开销。
 *   可通过 Keil C/C++ Preprocessor Defines 添加 USB_DEBUG_ENABLE=0 来关闭，
 *   或直接修改此处的默认值。
 */
#ifndef USB_DEBUG_ENABLE
#define USB_DEBUG_ENABLE 1
#endif

/* LOG_TAG should be defined before including log.h in each source file */
#ifndef LOG_TAG
#define LOG_TAG "LOG"
#endif

/* Set to 1 to include __FILE__:__LINE__ in log output */
#ifndef LOG_SHOW_FILE
#define LOG_SHOW_FILE 0
#endif

#if !USB_DEBUG_ENABLE
/* ---- 调试关闭: 所有日志宏变为空操作 ---- */
#define log_trace(...) ((void)0)
#define log_debug(...) ((void)0)
#define log_info(...)  ((void)0)
#define log_warn(...)  ((void)0)
#define log_error(...) ((void)0)
#define log_fatal(...) ((void)0)

#define log_set_level(l) ((void)0)
#define log_set_quiet(e) ((void)0)

#else /* USB_DEBUG_ENABLE */

typedef struct {
  va_list ap;
  const char *fmt;
  const char *tag;
  const char *file;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#if LOG_SHOW_FILE
#define log_trace(...) log_log(LOG_TRACE, LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, LOG_TAG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define log_trace(...) log_log(LOG_TRACE, LOG_TAG, NULL, 0, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, LOG_TAG, NULL, 0, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  LOG_TAG, NULL, 0, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  LOG_TAG, NULL, 0, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, LOG_TAG, NULL, 0, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, LOG_TAG, NULL, 0, __VA_ARGS__)
#endif

const char* log_level_string(int level);
void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void *udata, int level);

void log_log(int level, const char *tag, const char *file, int line, const char *fmt, ...);

#endif /* USB_DEBUG_ENABLE */

#endif /* LOG_H */
