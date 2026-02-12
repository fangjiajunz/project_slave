#include <stdint.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define NELEM ARRAY_SIZE

//#define CONFIG_USE_CRC_TABLE

#ifdef CONFIG_USE_CRC_TABLE
static const uint16_t _crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    /* ... 省略中间数据，实际要完整256项 ... */
};
#else
static uint16_t _crc16_table[256];
#endif

// 生成 YMODEM 的 CRC16 表
void crc16_table_init(void)
{
#ifndef CONFIG_USE_CRC_TABLE
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i << 8; // 对应XMODEM, 高位在前
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        _crc16_table[i] = crc;
    }
#endif
}

// 更新单字节 CRC
uint16_t crc16_update_byte(uint16_t crc, uint8_t data)
{
    return (_crc16_table[((crc >> 8) ^ data) & 0xFF] ^ (crc << 8));
}

// 更新一段数据 CRC
uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint16_t size)
{
#ifndef CONFIG_USE_CRC_TABLE
    if (_crc16_table[1] == 0) {
        crc16_table_init();
    }
#endif
    for (uint16_t i = 0; i < size; i++) {
        crc = crc16_update_byte(crc, data[i]);
    }
    return crc;
}

// 获取数据 CRC
uint16_t crc16_get(const uint8_t *data, uint16_t size)
{
    return crc16_update(0, data, size); // YMODEM 初值 0x0000
}
