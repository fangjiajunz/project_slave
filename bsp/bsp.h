

#ifndef BSP_H
#define BSP_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp_uart_fifo.h"
#define ENABLE_INT() __set_PRIMASK(0)  /* 允许中断*/
#define DISABLE_INT() __set_PRIMASK(1) /* 关中断*/

#endif
