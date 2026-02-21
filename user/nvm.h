/**
 * @file   nvm.h
 * @brief  NVM (Non-Volatile Memory) 非易失性存储模块
 *
 * 基于 STM32 内部 Flash 实现掉电不丢失的数据存储。
 * 采用双 Flash 页交替写入（ping-pong）机制，带 CRC32 校验，
 * 保证写入过程中断电也不会丢失上一次的有效数据。
 *
 * 支持最多 CONFIG_NVM_PICES_NUM 个独立分区，各分区互不影响。
 *
 * 使用流程:
 *   1. nvm2_init()  — 初始化指定分区，传入两个 Flash 页地址和页大小
 *   2. nvm_read()   — 从分区读取数据到 RAM 缓冲区
 *   3. nvm_write()  — 将 RAM 数据写入分区（自动交替页、计算 CRC）
 */
#ifndef _NVM_H__
#define _NVM_H__

#include "stdint.h"
#include "stdbool.h"

/* ---- 错误码 ---- */
#define NVM_ERR_NO     0   /* 成功 */
#define NVM_PARAM_ERR -1   /* 参数错误 */
#define NVM_ERR_CRC   -2   /* CRC 校验失败 */
#define NVM_ERR_LEN   -3   /* 读写长度与存储长度不匹配 */
#define NVM_ERR_EMPTY -4   /* 分区为空（从未写入过有效数据） */

/* ---- 分区编号 ---- */
#define CONFIG_NVM_PICES_0   0
#define CONFIG_NVM_PICES_1   1
#define CONFIG_NVM_PICES_2   2

#define CONFIG_NVM_PICES_NUM 3  /* 最大分区数 */

/**
 * @brief  初始化所有分区（旧接口，供 simulink 使用）
 * @param  flash_addr  各分区 Flash 地址数组
 * @param  sizes       各分区大小数组
 */
void nvm_init(uint32_t flash_addr[], uint32_t sizes[]);

/**
 * @brief  初始化单个 NVM 分区
 * @param  nvm_nr      分区编号 (CONFIG_NVM_PICES_0/1/2)
 * @param  flash_addr  两个 Flash 页地址 (flash_addr[0], flash_addr[1])
 * @param  flash_size  单个 Flash 页大小（字节）
 */
void nvm2_init(int nvm_nr, uint32_t flash_addr[], uint32_t flash_size);

/**
 * @brief  从 NVM 分区读取数据
 * @param  nvm_nr  分区编号
 * @param  buffer  接收缓冲区
 * @param  len     期望读取的字节数（必须与写入时的长度一致）
 * @retval NVM_ERR_NO 成功，其他见错误码定义
 */
int32_t nvm_read(uint32_t nvm_nr, uint8_t *buffer, uint32_t len);

/**
 * @brief  向 NVM 分区写入数据
 * @param  nvm_nr  分区编号
 * @param  buffer  待写入数据
 * @param  len     数据长度（字节，不得超过 flash_size）
 * @retval NVM_ERR_NO 成功，其他见错误码定义
 */
int32_t nvm_write(uint32_t nvm_nr, uint8_t *buffer, uint32_t len);

/**
 * @brief  NVM 读写自测函数
 */
void nvm_test(void);

#endif /* _NVM_H__ */
