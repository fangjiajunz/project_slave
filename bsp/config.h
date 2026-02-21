

#ifndef CONFIG_H
#define CONFIG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "nvm.h"

/* ---- NVM Flash 地址 (STM32F103C8T6 末尾两个 1KB 页) ---- */
#define NVM_CONFIG_ADDR_A     0x0800F800
#define NVM_CONFIG_ADDR_B     0x0800FC00
#define NVM_CONFIG_PARTITION  CONFIG_NVM_PICES_0
#define NVM_CONFIG_FLASH_SIZE 1024

/* ---- LoRa 参数默认值 ---- */
#define LORA_DEFAULT_SLAVE_ADDR    1
#define LORA_DEFAULT_FREQUENCY_MHZ 915
#define LORA_DEFAULT_TX_POWER      22
#define LORA_DEFAULT_SF            11
#define LORA_DEFAULT_BW            500

/* ---- LoRa 配置子结构体 ---- */
typedef struct {
    uint8_t slave_addr;      /* 从机地址 1-14 */
    int     frequency_mhz;   /* 频率 850-930 MHz */
    int     tx_power;        /* 功率 -9~22 dBm */
    int     lora_sf;         /* 扩频因子 5-12 */
    int     lora_bw;         /* 带宽 125/250/500 KHz */
} lora_config_t;

/* ---- 系统配置（后续可扩展其他组） ---- */
typedef struct {
    lora_config_t lora;
} sys_config_t;

#endif
