/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 *
 * Adapted for STM32 bare-metal: removed time.h/FILE* dependencies,
 * output via byte_queue + USB CDC.
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define LOG_VERSION "0.2.0"

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

/* LOG_TAG should be defined before including log.h in each source file */
#ifndef LOG_TAG
#define LOG_TAG "LOG"
#endif

/* Set to 1 to include __FILE__:__LINE__ in log output */
#ifndef LOG_SHOW_FILE
#define LOG_SHOW_FILE 0
#endif

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

#endif
