#include "byte_queue.h"

static uint16_t byte_queue_add(const byte_queue_t *queue, uint16_t value1, uint16_t value2)
{
	return (value1 + value2) % queue->size;
}

static uint16_t byte_queue_tail_add(const byte_queue_t *queue, uint16_t value)
{
	return byte_queue_add(queue, queue->tail, value);
}

static uint16_t byte_queue_head_add(const byte_queue_t *queue, uint16_t value)
{
	return byte_queue_add(queue, queue->head, value);
}

uint8_t *byte_queue_head(byte_queue_t *queue)
{
	return queue->buff + queue->head;
}

bool byte_queue_empty(const byte_queue_t *queue)
{
	return (bool) (queue->head == queue->tail);
}

bool byte_queue_readable(const byte_queue_t *queue)
{
	return (bool) (queue->head != queue->tail);
}

bool byte_queue_full(const byte_queue_t *queue)
{
	return (bool) (byte_queue_tail_add(queue, 1) == queue->head);
}

bool byte_queue_writeable(const byte_queue_t *queue)
{
	return (bool) (byte_queue_tail_add(queue, 1) != queue->head);
}

uint16_t byte_queue_get_used(const byte_queue_t *queue)
{
	if (queue->head <= queue->tail) {
		return queue->tail - queue->head;
	}

	return queue->size - (queue->head - queue->tail) - 1;
}

uint16_t byte_queue_get_free(const byte_queue_t *queue)
{
	if (queue->tail < queue->head) {
		return queue->head - queue->tail;
	}

	return queue->size - (queue->tail - queue->head) - 1;
}

void byte_queue_reset(byte_queue_t *queue)
{
	queue->head = queue->tail = 0;
}

void byte_queue_init(byte_queue_t *queue, uint8_t *buff, uint16_t size)
{
	queue->buff = buff;
	queue->size = size;
	queue->head = queue->tail = 0;
}

uint16_t byte_queue_write(byte_queue_t *queue, const uint8_t *buff, uint16_t size)
{
	const uint8_t *buff_bak = buff;
	const uint8_t *buff_end;

	for (buff_end = buff + size; buff < buff_end; buff++) {
		uint16_t tail = byte_queue_tail_add(queue, 1);

		if (tail == queue->head) {
			return buff - buff_bak;
		}

		queue->buff[queue->tail] = *buff;
		queue->tail = tail;
	}

	return size;
}

uint16_t byte_queue_write_byte(byte_queue_t *queue, uint8_t byte)
{
	return byte_queue_write(queue, &byte, 1);
}

uint16_t byte_queue_read(byte_queue_t *queue, uint8_t *buff, uint16_t size)
{
	uint8_t *buff_bak = buff;
	uint8_t *buff_end;

	for (buff_end = buff + size; buff < buff_end; buff++) {
		if (queue->head == queue->tail) {
			return buff - buff_bak;
		}

		*buff = queue->buff[queue->head];
		queue->head = byte_queue_head_add(queue, 1);
	}

	return size;
}

void byte_queue_fill(byte_queue_t *queue, uint8_t *buff, uint16_t size)
{
	while (size > 0) {
		uint16_t length = byte_queue_read(queue, buff, size);
		size -= length;
		buff += size;
	}
}

void byte_queue_skip(byte_queue_t *queue, uint16_t length)
{
	queue->head = byte_queue_head_add(queue, length);
}

uint16_t byte_queue_peek(byte_queue_t *queue)
{
	if (queue->tail < queue->head) {
		return queue->size - queue->head;
	} else {
		return queue->tail - queue->head;
	}
}

// ================================================================================

void byte_queue_alloc_init(byte_queue_t *queue, uint8_t *buff, uint8_t size)
{
	queue->buff = buff;
	queue->size = size;
	byte_queue_alloc_reset(queue);
}

void byte_queue_alloc_reset(byte_queue_t *queue)
{
	uint8_t *buff = queue->buff;
	uint8_t size = queue->size;
	uint8_t index;

	for (index = 0; index < size; index++) {
		buff[index] = index;
	}

	queue->tail = size - 1;
	queue->head = 0;
}

uint8_t byte_queue_alloc(byte_queue_t *queue)
{
	uint8_t index;

	if (byte_queue_read(queue, &index, 1) > 0) {
		return index;
	}

	return 0xFF;
}

void byte_queue_free(byte_queue_t *queue, uint8_t index)
{
	byte_queue_write(queue, &index, 1);
}
