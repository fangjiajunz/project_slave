/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Adapted for STM32 bare-metal: output via byte_queue + USB CDC,
 * removed time.h/FILE* dependencies.
 */

#include <stdio.h>
#include <string.h>
#include "log.h"
#include "usb_uart.h"

#define MAX_CALLBACKS 32
#define LOG_BUF_SIZE  128

typedef struct {
  log_LogFn fn;
  void *udata;
  int level;
} Callback;

static struct {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
  Callback callbacks[MAX_CALLBACKS];
} L;

static const char *level_strings[] = {
    "[TRACE]", "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]", "[FATAL]"};


const char* log_level_string(int level) {
  return level_strings[level];
}


static void lock(void) {
  if (L.lock) { L.lock(true, L.udata); }
}


static void unlock(void) {
  if (L.lock) { L.lock(false, L.udata); }
}


void log_set_lock(log_LockFn fn, void *udata) {
  L.lock = fn;
  L.udata = udata;
}


void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(bool enable) {
  L.quiet = enable;
}


int log_add_callback(log_LogFn fn, void *udata, int level) {
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!L.callbacks[i].fn) {
      L.callbacks[i] = (Callback) { fn, udata, level };
      return 0;
    }
  }
  return -1;
}


/**
 * @brief  默认输出回调 — 格式化后写入 USB CDC 发送队列
 *
 * 输出格式 (LOG_SHOW_FILE=0): "LEVEL TAG: message\r\n"
 * 输出格式 (LOG_SHOW_FILE=1): "LEVEL TAG file:line: message\r\n"
 */
static void usb_callback(log_Event *ev) {
  char buf[LOG_BUF_SIZE];
  int len;

  if (ev->file) {
    /* 只取文件名，去掉路径前缀 */
    const char *p = ev->file;
    const char *s;
    for (s = p; *s; s++) {
      if (*s == '/' || *s == '\\') {
        p = s + 1;
      }
    }
    len = snprintf(buf, sizeof(buf), "%-5s %s %s:%d: ",
                   level_strings[ev->level], ev->tag, p, ev->line);
  } else {
    len = snprintf(buf, sizeof(buf), "%-5s %s: ",
                   level_strings[ev->level], ev->tag);
  }
  len += vsnprintf(buf + len, sizeof(buf) - len, ev->fmt, ev->ap);

  /* 确保 buf 中有空间放 \r\n */
  if (len > (int)sizeof(buf) - 3) {
    len = (int)sizeof(buf) - 3;
  }
  buf[len++] = '\r';
  buf[len++] = '\n';

  byte_queue_write(&uart_tx_queue, (uint8_t *)buf, len);
}


void log_log(int level, const char *tag, const char *file, int line, const char *fmt, ...) {
  log_Event ev = {
    .fmt   = fmt,
    .tag   = tag,
    .file  = file,
    .line  = line,
    .level = level,
  };

  lock();

  /* 默认输出 */
  if (!L.quiet && level >= L.level) {
    va_start(ev.ap, fmt);
    usb_callback(&ev);
    va_end(ev.ap);
  }

  /* 用户注册的额外回调 */
  for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
    Callback *cb = &L.callbacks[i];
    if (level >= cb->level) {
      ev.udata = cb->udata;
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock();
}
