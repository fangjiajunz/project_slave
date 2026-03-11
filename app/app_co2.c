#include "app_co2.h"
#include "bsp.h"

/*
 * 串口数据流格式 (9 字节固定帧):
 *
 *  B1    B2    B3      B4      B5      B6      B7     B8     B9
 *  0x2C  0xE4  TVOC_H  TVOC_L  CH2O_H  CH2O_L  CO2_H  CO2_L  校验和
 *
 * 校验和 = (uint8_t)(B1 + B2 + B3 + B4 + B5 + B6 + B7 + B8)
 * CO2 (ppm) = B7 * 256 + B8
 */

#define FRAME_LEN    9
#define FRAME_HEAD1  0x2C
#define FRAME_HEAD2  0xE4
#define CO2_HIGH_IDX 6   /* B7 在 buf 中的下标 */
#define CO2_LOW_IDX  7   /* B8 在 buf 中的下标 */

static uint16_t s_co2_ppm = 0;

void app_co2_poll(void)
{
    static uint8_t buf[FRAME_LEN];
    static uint8_t idx = 0;

    uint8_t byte;
    while (comGetChar(COM1, &byte))
    {
        switch (idx)
        {
        case 0: /* 等待帧头 B1 */
            if (byte == FRAME_HEAD1)
                buf[idx++] = byte;
            break;

        case 1: /* 等待帧头 B2 */
            if (byte == FRAME_HEAD2)
                buf[idx++] = byte;
            else
                idx = 0; /* 不匹配，重新找帧头 */
            break;

        default: /* B3 ~ B9 */
            buf[idx++] = byte;
            if (idx >= FRAME_LEN)
            {
                /* 校验和验证 */
                uint8_t sum = 0;
                for (uint8_t i = 0; i < FRAME_LEN - 1; i++)
                    sum += buf[i];

                if (sum == buf[FRAME_LEN - 1])
                {
                    s_co2_ppm = ((uint16_t)buf[CO2_HIGH_IDX] << 8)
                              | buf[CO2_LOW_IDX];
                }
                idx = 0;
            }
            break;
        }
    }
}

uint16_t app_co2_get(void)
{
    return s_co2_ppm;
}
