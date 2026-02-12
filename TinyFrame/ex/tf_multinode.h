/**
 * @file    tf_multinode.h
 * @brief   TinyFrame 多节点通信协议定义
 *
 * TYPE 字段拆分方案 (8位):
 * +--------+--------+
 * | ADDR   | MSG    |
 * | [7:4]  | [3:0]  |
 * +--------+--------+
 *   4位      4位
 *
 * 地址范围:
 *   0x0 = 广播地址 (所有从机接收, 不响应)
 *   0x1-0xE = 从机地址 1-14
 *   0xF = 保留
 */

#ifndef TF_MULTINODE_H
#define TF_MULTINODE_H

#include <stdint.h>
#include "TinyFrame.h"

/* ========================== 地址定义 ========================== */

#define TF_ADDR_BROADCAST       0x00    /* 广播地址 */
#define TF_ADDR_SLAVE_MIN       0x01    /* 从机地址最小值 */
#define TF_ADDR_SLAVE_MAX       0x0E    /* 从机地址最大值 */
#define TF_ADDR_RESERVED        0x0F    /* 保留地址 */
#define TF_ADDR_MASTER          0x00    /* 主机响应时使用的地址 (与广播相同) */

/* ========================== 消息类型枚举 ========================== */

typedef enum {
    TF_MSG_ACK          = 0x00,     /* 确认响应 */
    TF_MSG_NACK         = 0x01,     /* 否定响应 */
    TF_MSG_HEARTBEAT    = 0x02,     /* 心跳包 */
    TF_MSG_LED_CTRL     = 0x03,     /* LED 控制命令 */
    TF_MSG_STATUS_REQ   = 0x04,     /* 状态请求 */
    TF_MSG_STATUS_RSP   = 0x05,     /* 状态响应 */
    TF_MSG_DATA         = 0x06,     /* 数据传输 */
    TF_MSG_CONFIG       = 0x07,     /* 配置命令 */
    /* 0x08 - 0x0F 预留给用户自定义 */
    TF_MSG_USER_BASE    = 0x08,     /* 用户自定义消息起始 */
    TF_MSG_MAX          = 0x0F      /* 最大消息类型 */
} TF_MsgType;

/* ========================== TYPE 字段操作宏 ========================== */

/**
 * @brief  组合地址和消息类型为 TYPE 字段
 * @param  addr: 目标地址 (0x0-0xF)
 * @param  msg:  消息类型 (0x0-0xF)
 * @return TYPE 字段值
 */
#define TF_MAKE_TYPE(addr, msg)     ((TF_TYPE)(((addr) << 4) | ((msg) & 0x0F)))

/**
 * @brief  从 TYPE 字段提取地址
 * @param  type: TYPE 字段值
 * @return 地址 (0x0-0xF)
 */
#define TF_GET_ADDR(type)           ((uint8_t)(((type) & 0xF0) >> 4))

/**
 * @brief  从 TYPE 字段提取消息类型
 * @param  type: TYPE 字段值
 * @return 消息类型 (0x0-0xF)
 */
#define TF_GET_MSG(type)            ((uint8_t)((type) & 0x0F))

/**
 * @brief  检查地址是否匹配 (本机地址或广播)
 * @param  type:    TYPE 字段值
 * @param  my_addr: 本机地址
 * @return 1 = 匹配, 0 = 不匹配
 */
#define TF_ADDR_MATCH(type, my_addr) \
    ((TF_GET_ADDR(type) == (my_addr)) || (TF_GET_ADDR(type) == TF_ADDR_BROADCAST))

/**
 * @brief  检查是否为广播消息
 * @param  type: TYPE 字段值
 * @return 1 = 广播, 0 = 单播
 */
#define TF_IS_BROADCAST(type)       (TF_GET_ADDR(type) == TF_ADDR_BROADCAST)

/* ========================== LED 控制命令定义 ========================== */

typedef enum {
    LED_CMD_OFF     = 0x00,     /* LED 关闭 */
    LED_CMD_ON      = 0x01,     /* LED 开启 */
    LED_CMD_TOGGLE  = 0x02,     /* LED 翻转 */
} TF_LedCmd;

/* ========================== 状态响应结构 ========================== */

typedef struct {
    uint8_t node_addr;          /* 节点地址 */
    uint8_t led_state;          /* LED 状态 */
    uint8_t error_code;         /* 错误代码 */
} TF_StatusData;

/* ========================== 时间槽防冲突配置 ========================== */

/* 时间槽周期 (ms) - 所有从机共享一个周期 */
#define TF_SLOT_PERIOD_MS       100

/* 每个从机的时间槽宽度 (ms) */
#define TF_SLOT_WIDTH_MS        20

/* 计算从机的发送时间窗口起点 */
#define TF_SLOT_START(addr)     (((addr) - 1) * TF_SLOT_WIDTH_MS)

/* 检查当前是否在本机时间槽内 */
#define TF_IN_MY_SLOT(tick_ms, addr) \
    (((tick_ms) % TF_SLOT_PERIOD_MS) >= TF_SLOT_START(addr) && \
     ((tick_ms) % TF_SLOT_PERIOD_MS) < (TF_SLOT_START(addr) + TF_SLOT_WIDTH_MS))

#endif /* TF_MULTINODE_H */
