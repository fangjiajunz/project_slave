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

#include <stdbool.h>
#include <stdint.h>

#include "TinyFrame.h"

/* ========================== 地址定义 ========================== */

#define TF_ADDR_BROADCAST 0x00 /* 广播地址 */
#define TF_ADDR_SLAVE_MIN 0x01 /* 从机地址最小值 */
#define TF_ADDR_SLAVE_MAX 0x0E /* 从机地址最大值 */
#define TF_ADDR_RESERVED 0x0F  /* 保留地址 */
#define TF_ADDR_MASTER 0x00    /* 主机响应时使用的地址 (与广播相同) */

/* ========================== 消息类型枚举 ========================== */

typedef enum
{
    TF_MSG_ACK = 0x00,        /* 确认响应 */
    TF_MSG_NACK = 0x01,       /* 否定响应 */
    TF_MSG_HEARTBEAT = 0x02,  /* 心跳包 */
    TF_MSG_LED_CTRL = 0x03,   /* LED 控制命令 */
    TF_MSG_STATUS_REQ = 0x04, /* 状态请求 */
    TF_MSG_STATUS_RSP = 0x05, /* 状态响应 */
    TF_MSG_DATA = 0x06,       /* 数据传输 */
    TF_MSG_CONFIG = 0x07,     /* 配置命令 */
    /* 0x08 - 0x0F 用户自定义 */
    TF_MSG_THRESHOLD = 0x08, /* threshold config distribution */
    TF_MSG_MAX = 0x0F        /* 最大消息类型 */
} TF_MsgType;

/* ========================== TYPE 字段操作宏 ========================== */

/**
 * @brief  组合地址和消息类型为 TYPE 字段
 * @param  addr: 目标地址 (0x0-0xF)
 * @param  msg:  消息类型 (0x0-0xF)
 * @return TYPE 字段值
 */
#define TF_MAKE_TYPE(addr, msg) ((TF_TYPE)(((addr) << 4) | ((msg) & 0x0F)))

/**
 * @brief  从 TYPE 字段提取地址
 * @param  type: TYPE 字段值
 * @return 地址 (0x0-0xF)
 */
#define TF_GET_ADDR(type) ((uint8_t)(((type) & 0xF0) >> 4))

/**
 * @brief  从 TYPE 字段提取消息类型
 * @param  type: TYPE 字段值
 * @return 消息类型 (0x0-0xF)
 */
#define TF_GET_MSG(type) ((uint8_t)((type) & 0x0F))

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
#define TF_IS_BROADCAST(type) (TF_GET_ADDR(type) == TF_ADDR_BROADCAST)

/* ========================== LED 控制命令定义 ========================== */

typedef enum
{
    LED_CMD_OFF = 0x00,    /* LED 关闭 */
    LED_CMD_ON = 0x01,     /* LED 开启 */
    LED_CMD_TOGGLE = 0x02, /* LED 翻转 */
} TF_LedCmd;

/* ========================== CONFIG 控制命令定义 ========================== */

/**
 * TF_MSG_CONFIG 统一控制命令格式:
 *   payload[0] = 设备 ID (TF_CTRL_DEV_xxx)
 *   payload[1] = 动作值 (TF_CTRL_ACT_xxx)
 *
 * 主机通过 TF_Master_SendCtrlCmd() 发送，从机在 CONFIG 回调中解析。
 * 所有外设控制统一走此通道，避免消息类型不够用。
 */

/* 设备 ID 定义 */
#define TF_CTRL_DEV_FAN     0x01    /* 风扇 */
#define TF_CTRL_DEV_HEATER  0x02    /* 加热器 */
#define TF_CTRL_DEV_PUMP    0x03    /* 水泵 */
#define TF_CTRL_DEV_LED     0x04    /* LED */

/* 动作值定义 */
#define TF_CTRL_ACT_OFF     0x00    /* 关闭 */
#define TF_CTRL_ACT_ON      0x01    /* 开启 */

/* ========================== 阈值字段 ID (按字段下发) ========================== */

/*
 * TF_MSG_THRESHOLD payload 格式 (3 字节):
 *   payload[0] = field_id (TF_THRESH_FIELD_xxx)
 *   payload[1..2] = value (int16_t, little-endian)
 */
#define TF_THRESH_FIELD_TEMP_HIGH  0
#define TF_THRESH_FIELD_TEMP_LOW   1
#define TF_THRESH_FIELD_HUMI_HIGH  2
#define TF_THRESH_FIELD_HUMI_LOW   3
#define TF_THRESH_FIELD_SOIL_DRY   4
#define TF_THRESH_FIELD_LIGHT_LOW  5
#define TF_THRESH_FIELD_CO2_HIGH   6
#define TF_THRESH_FIELD_ENABLE     7

/* ========================== 状态响应结构 ========================== */

/* 传感器数据 */
typedef struct
{
    int16_t  temperature;    /* 温度 (x10, 如 251 = 25.1°C) */
    uint16_t humidity;       /* 湿度 (x10, 如 655 = 65.5%) */
    uint16_t illuminance;    /* 光照强度 (lux) */
    uint16_t co2;            /* CO2浓度 (ppm) */
    uint16_t soil_moisture;  /* 土壤湿度 (x10, 0-1000 对应 0-100%) */
} sensor_cb;

/* 控制器状态 */
typedef struct
{
    uint8_t fan;             /* 风扇 (0=关, 1=开) */
    uint8_t heater;          /* 加热 (0=关, 1=开) */
    uint8_t pump;            /* 水泵 (0=关, 1=开) */
    uint8_t led;             /* LED (0=关, 1=开) */
} control_cb;

typedef struct
{
    uint8_t    node_addr;    /* 节点地址 */
    uint8_t    error_code;   /* 错误代码 */
    int8_t     rssi;         /* 信号强度 */
    sensor_cb  sensor;       /* 传感器数据 */
    control_cb ctrl;         /* 控制器状态 */
} TF_StatusData;

/* ========================== 时间槽防冲突配置 ========================== */

/* 时间槽周期 (ms) - 所有从机共享一个周期 */
#define TF_SLOT_PERIOD_MS 100

/* 每个从机的时间槽宽度 (ms) */
#define TF_SLOT_WIDTH_MS 20

/* 计算从机的发送时间窗口起点 */
#define TF_SLOT_START(addr) (((addr) - 1) * TF_SLOT_WIDTH_MS)

/* 检查当前是否在本机时间槽内 */
#define TF_IN_MY_SLOT(tick_ms, addr)                           \
    (((tick_ms) % TF_SLOT_PERIOD_MS) >= TF_SLOT_START(addr) && \
     ((tick_ms) % TF_SLOT_PERIOD_MS) < (TF_SLOT_START(addr) + TF_SLOT_WIDTH_MS))

/* ========================== 消息去重 ========================== */

/**
 * 环形去重缓冲区 — 记录最近处理过的 (来源地址 + Frame ID) 对，
 * 用于检测重传导致的重复帧。
 *
 * 存储 (addr, frame_id) 对而非单独 frame_id，
 * 避免不同从机的 ID 计数器重叠导致误判。
 *
 * TF_ID 当前为 uint8_t (TF_ID_BYTES=1)，范围 0-255，
 * 缓冲区大小 16 足以覆盖短期内的所有活跃记录。
 */
#define TF_DEDUP_BUF_SIZE 16

typedef struct
{
    struct
    {
        uint8_t addr;             /* 来源地址 */
        TF_ID id;                 /* Frame ID */
    } entries[TF_DEDUP_BUF_SIZE]; /* 环形数组 */
    uint8_t write_idx;            /* 下一个写入位置 */
    uint8_t count;                /* 已记录数量 (最大 = TF_DEDUP_BUF_SIZE) */
} TF_DedupBuf;

/**
 * @brief  初始化去重缓冲区
 */
static inline void TF_Dedup_Init(TF_DedupBuf *buf)
{
    buf->write_idx = 0;
    buf->count = 0;
}

/**
 * @brief  检查 (来源地址 + Frame ID) 是否重复，并记录新条目
 * @param  buf:  去重缓冲区
 * @param  addr: 来源地址 (区分不同发送方)
 * @param  id:   待检查的 Frame ID
 * @return true = 重复帧 (已存在), false = 新帧 (已记录)
 */
static inline bool TF_Dedup_IsDuplicate(TF_DedupBuf *buf, uint8_t addr, TF_ID id)
{
    /* 在已有记录中查找 (addr + id) 对 */
    uint8_t n = (buf->count < TF_DEDUP_BUF_SIZE) ? buf->count : TF_DEDUP_BUF_SIZE;
    for (uint8_t i = 0; i < n; i++)
    {
        if (buf->entries[i].addr == addr && buf->entries[i].id == id)
        {
            return true; /* 重复 */
        }
    }
    /* 新条目，记录到环形缓冲区 */
    buf->entries[buf->write_idx].addr = addr;
    buf->entries[buf->write_idx].id = id;
    buf->write_idx = (buf->write_idx + 1) % TF_DEDUP_BUF_SIZE;
    if (buf->count < TF_DEDUP_BUF_SIZE)
    {
        buf->count++;
    }
    return false; /* 新帧 */
}

#endif /* TF_MULTINODE_H */
