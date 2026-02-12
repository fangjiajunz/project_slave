#pragma once

//#include "os/types.h"
#include "stdint.h"
#include "stdbool.h"
typedef struct {
	uint8_t *buff;
	uint16_t size;
	uint16_t head;
	uint16_t tail;
} byte_queue_t;

uint8_t *byte_queue_head(byte_queue_t *queue);
bool byte_queue_empty(const byte_queue_t *queue);
bool byte_queue_readable(const byte_queue_t *queue);
bool byte_queue_full(const byte_queue_t *queue);
bool byte_queue_writeable(const byte_queue_t *queue);
uint16_t byte_queue_get_used(const byte_queue_t *queue);
uint16_t byte_queue_get_free(const byte_queue_t *queue);

void byte_queue_reset(byte_queue_t *queue);
void byte_queue_init(byte_queue_t *queue, uint8_t *buff, uint16_t size);
uint16_t byte_queue_write(byte_queue_t *queue, const uint8_t *buff, uint16_t size);
uint16_t byte_queue_write_byte(byte_queue_t *queue, uint8_t byte);
uint16_t byte_queue_read(byte_queue_t *queue, uint8_t *buff, uint16_t size);
void byte_queue_fill(byte_queue_t *queue, uint8_t *buff, uint16_t size);
void byte_queue_skip(byte_queue_t *queue, uint16_t length);
uint16_t byte_queue_peek(byte_queue_t *queue);

void byte_queue_alloc_init(byte_queue_t *queue, uint8_t *buff, uint8_t size);
void byte_queue_alloc_reset(byte_queue_t *queue);
uint8_t byte_queue_alloc(byte_queue_t *queue);
void byte_queue_free(byte_queue_t *queue, uint8_t index);
