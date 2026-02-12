#ifndef HEAP4__H
#define HEAP4__H

#include <stddef.h>

void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
void *pvPortMallocAddr(void);

#endif
