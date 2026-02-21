#pragma once
#include "stdbool.h"
#include "stdint.h"

uint16_t crc16_update_byte(uint16_t crc, uint8_t value);
uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint16_t size);
uint16_t crc16_get(const uint8_t *data, uint16_t size);
uint32_t crc32_get(const uint8_t *buff, uint32_t length);
uint32_t crc32_update(uint32_t crc, const uint8_t *buff, uint32_t length);
uint32_t crc32_finish(uint32_t crc);
