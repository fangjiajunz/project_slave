#pragma once
#include "stdint.h"
#include "stdbool.h"
//#include "os/types.h"

uint16_t crc16_update_byte(uint16_t crc, uint8_t value);
uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint16_t size);
uint16_t crc16_get(const uint8_t *data, uint16_t size);


