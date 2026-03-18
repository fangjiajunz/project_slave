

#ifndef CONFIG_H
#define CONFIG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "nvm.h"

/* ---- NVM Flash 地址 (STM32F103C8T6 末尾两个 1KB 页) ---- */
#define NVM_CONFIG_ADDR_A 0x0800F800
#define NVM_CONFIG_ADDR_B 0x0800FC00
#define NVM_CONFIG_PARTITION CONFIG_NVM_PICES_0
#define NVM_CONFIG_FLASH_SIZE 1024

/* ---- LoRa 参数默认值 ---- */
#define LORA_DEFAULT_SLAVE_ADDR 1
#define LORA_DEFAULT_FREQUENCY_MHZ 915
#define LORA_DEFAULT_TX_POWER 22
#define LORA_DEFAULT_SF 11
#define LORA_DEFAULT_BW 500

/* ---- LoRa 配置子结构体 ---- */
typedef struct
{
    uint8_t slave_addr; /* 从机地址 1-14 */
    int frequency_mhz;  /* 频率 850-930 MHz */
    int tx_power;       /* 功率 -9~22 dBm */
    int lora_sf;        /* 扩频因子 5-12 */
    int lora_bw;        /* 带宽 125/250/500 KHz */
} lora_config_t;

/* ---- 阈值控制配置 ---- */
typedef struct {
    int16_t  temp_high;      /* 温度上限 x10 (默认 350 = 35.0°C) → 开风扇 */
    int16_t  temp_low;       /* 温度下限 x10 (默认 100 = 10.0°C) → 开加热器 */
    uint16_t humi_high;      /* 湿度上限 x10 (默认 800 = 80.0%) */
    uint16_t humi_low;       /* 湿度下限 x10 (默认 300 = 30.0%) */
    uint16_t soil_dry;       /* 土壤湿度阈值 x10 (默认 300 = 30%) → 开水泵 */
    uint16_t light_low;      /* 光照不足阈值 ADC (默认 500) → 开LED */
    uint16_t co2_high;       /* CO2上限 ppm (默认 1000) → 开风扇 */
    uint8_t  enable;         /* 自动控制使能位掩码 bit0=温度 bit1=土壤湿度 bit2=光照 bit3=CO2 bit4=湿度 */
} threshold_config_t;

/* ---- 设备数量 ---- */
#define APP_DEV_COUNT 1  /* 从机只管自己 */

/* ---- 系统配置（后续可扩展其他组） ---- */
typedef struct
{
    // lora_config_t      lora;
    threshold_config_t threshold[APP_DEV_COUNT]; /* 从机只用 [0] */
} sys_config_t;

#endif
