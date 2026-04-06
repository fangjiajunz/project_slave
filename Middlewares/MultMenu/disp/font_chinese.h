#ifndef _FONT_CHINESE_H_
#define _FONT_CHINESE_H_

#include <stdint.h>

/* 汉字索引枚举（42个汉字） */
typedef enum {
    CH_WEN = 0,      /* 温 */
    CH_DU,           /* 度 */
    CH_SHI,          /* 湿 */
    CH_GUANG,        /* 光 */
    CH_ZHAO,         /* 照 */
    CH_TU,           /* 土 */
    CH_RANG,         /* 壤 */
    CH_FENG,         /* 风 */
    CH_SHAN,         /* 扇 */
    CH_JIA,          /* 加 */
    CH_RE,           /* 热 */
    CH_SHUI,         /* 水 */
    CH_BANG,         /* 泵 */
    CH_KAI,          /* 开 */
    CH_GUAN,         /* 关 */
    CH_SHE,          /* 设 */
    CH_BEI,          /* 备 */
    CH_LI,           /* 离 */
    CH_XIAN,         /* 线 */
    CH_XI,           /* 系 */
    CH_TONG,         /* 统 */
    CH_XIN,          /* 信 */
    CH_XI2,          /* 息 */
    CH_YI,           /* 已 */
    CH_LIAN,         /* 连 */
    CH_JIE,          /* 接 */
    CH_YUN,          /* 云 */
    CH_DUAN,         /* 端 */
    CH_ZAI,          /* 在 */
    CH_WANG,         /* 网 */
    CH_GAO,          /* 高 */
    CH_DI,           /* 低 */
    CH_GAN,          /* 干 */
    CH_ZAOIMG,       /* 燥 */
    CH_BU,           /* 不 */
    CH_ZU,           /* 足 */
    CH_GUO,          /* 过 */
    CH_CONG,         /* 从 */
    CH_JI,           /* 机 */
    CH_ZHONG,        /* 中 */
    CH_YUN2,         /* 运 */
    CH_XING           /* 行 */
} Chinese_Char_Index;

/* 42个汉字的字模数据（16×16点阵，每个32字节，阳码LSB格式） */
extern const uint8_t chinese_font[42][32];

#endif
