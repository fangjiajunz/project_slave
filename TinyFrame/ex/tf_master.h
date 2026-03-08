/**
 * @file    tf_master.h
 * @brief   TinyFrame 主机端模块
 *
 * 提供三种发送路径：
 *   SendTo()    — 可靠单播：内置 ACK 等待 + 超时自动重试
 *   QueryTo()   — 查询单播：等待自定义响应，超时由调用方处理
 *   Broadcast() — 广播：发后即忘，无 ACK
 *
 * 所有经过 SendTo() 的便捷 API (SendLedCmd 等) 自动获得可靠性。
 */

#ifndef TF_MASTER_H
#define TF_MASTER_H

#include "tf_multinode.h"
#include "TinyFrame.h"

/* ========================== 配置 ========================== */

/* 等待从机响应的超时时间 (ticks, 1tick = 1ms)
 * SF11 BW500 下往返约 120-140ms + 主循环延迟，需留足裕量 */
#define TF_MASTER_RESPONSE_TIMEOUT  500

/* 最大重试次数 (不含首次发送) */
#define TF_MASTER_MAX_RETRIES       3

/* 重试缓存最大负载字节数 */
#define TF_MASTER_MAX_PAYLOAD       32

/* 轮询最大从机数 */
#define TF_MASTER_MAX_POLL_SLAVES   8

/* 轮询等待响应超时 (ms) */
#define TF_MASTER_POLL_TIMEOUT      500

/* ========================== 回调函数类型 ========================== */

/**
 * @brief  从机数据接收回调函数类型
 * @param  slave_addr: 发送数据的从机地址
 * @param  data:       数据指针
 * @param  len:        数据长度
 */
typedef void (*TF_Master_DataCallback)(uint8_t slave_addr, const uint8_t *data, TF_LEN len);

/* ========================== API 函数 ========================== */

/**
 * @brief  设置从机数据接收回调
 * @param  callback: 回调函数
 */
void TF_Master_SetDataCallback(TF_Master_DataCallback callback);

/**
 * @brief  初始化主机端 TinyFrame
 *
 * 初始化 TF 实例为主机模式，注册通用监听器，初始化去重缓冲区。
 *
 * @param  tf: TinyFrame 实例
 * @return 成功返回 true
 */
bool TF_Master_Init(TinyFrame *tf);

/**
 * @brief  可靠发送消息到指定从机
 *
 * 内部使用 TF_QuerySimple 注册 ID 监听器等待从机 ACK。
 * 超时后自动重发，最多 TF_MASTER_MAX_RETRIES 次。
 * 所有经过此函数的命令自动获得可靠性。
 * 负载会被拷贝到内部缓存，data 指针在返回后可释放。
 *
 * @param  tf:        TinyFrame 实例
 * @param  slave_addr: 目标从机地址 (1-14)
 * @param  msg_type:  消息类型
 * @param  data:      数据负载 (可为 NULL)
 * @param  len:       数据长度 (不超过 TF_MASTER_MAX_PAYLOAD)
 * @return 成功返回 true
 */
bool TF_Master_SendTo(TinyFrame *tf, uint8_t slave_addr, TF_MsgType msg_type,
                      const uint8_t *data, TF_LEN len);

/**
 * @brief  查询发送 — 等待从机自定义响应，不自动重试
 *
 * listener 由调用方传入，用于处理自定义响应数据（如状态查询）。
 * 超时仅打印日志，不重发。如需重试由调用方自行处理。
 *
 * @param  tf:        TinyFrame 实例
 * @param  slave_addr: 目标从机地址 (1-14)
 * @param  msg_type:  消息类型
 * @param  data:      数据负载 (可为 NULL)
 * @param  len:       数据长度
 * @param  listener:  响应回调函数
 * @return 成功返回 true
 */
bool TF_Master_QueryTo(TinyFrame *tf, uint8_t slave_addr, TF_MsgType msg_type,
                       const uint8_t *data, TF_LEN len, TF_Listener listener);

/**
 * @brief  广播消息到所有从机 — 发后即忘，从机不回复
 *
 * @param  tf:       TinyFrame 实例
 * @param  msg_type: 消息类型
 * @param  data:     数据负载 (可为 NULL)
 * @param  len:      数据长度
 * @return 成功返回 true
 */
bool TF_Master_Broadcast(TinyFrame *tf, TF_MsgType msg_type,
                         const uint8_t *data, TF_LEN len);

/**
 * @brief  发送 LED 控制命令到指定从机 (经 SendTo，可靠)
 * @param  tf:        TinyFrame 实例
 * @param  slave_addr: 目标从机地址
 * @param  led_cmd:   LED 命令 (LED_CMD_OFF/ON/TOGGLE)
 * @return 成功返回 true
 */
bool TF_Master_SendLedCmd(TinyFrame *tf, uint8_t slave_addr, TF_LedCmd led_cmd);

/**
 * @brief  广播 LED 控制命令到所有从机 (经 Broadcast，发后即忘)
 * @param  tf:      TinyFrame 实例
 * @param  led_cmd: LED 命令
 * @return 成功返回 true
 */
bool TF_Master_BroadcastLedCmd(TinyFrame *tf, TF_LedCmd led_cmd);

/**
 * @brief  发送心跳查询到指定从机 (经 QueryTo，调用方处理超时)
 * @param  tf:        TinyFrame 实例
 * @param  slave_addr: 目标从机地址
 * @param  listener:  响应回调函数
 * @return 成功返回 true
 */
bool TF_Master_SendHeartbeat(TinyFrame *tf, uint8_t slave_addr, TF_Listener listener);

/**
 * @brief  发送 CONFIG 控制命令到指定从机 (经 SendTo，可靠)
 *
 * 通过 TF_MSG_CONFIG 发送统一控制命令，payload = [设备ID, 动作值]。
 * 经过 SendTo 自动获得 ACK + 超时重试的可靠性。
 *
 * @param  tf:         TinyFrame 实例
 * @param  slave_addr: 目标从机地址
 * @param  dev_id:     设备 ID (TF_CTRL_DEV_FAN / HEATER / PUMP)
 * @param  action:     动作值 (TF_CTRL_ACT_OFF / ON)
 * @return 成功返回 true
 */
bool TF_Master_SendCtrlCmd(TinyFrame *tf, uint8_t slave_addr,
                           uint8_t dev_id, uint8_t action);

/* ========================== 轮询 API ========================== */

/**
 * @brief  轮询回调 — 每轮询一个从机后调用
 * @param  slave_addr: 从机地址
 * @param  online:     true = 收到响应, false = 超时
 * @param  data:       响应数据指针 (离线时为 NULL)
 * @param  len:        响应数据长度
 */
typedef void (*TF_Master_PollCallback)(uint8_t slave_addr, bool online,
                                       const uint8_t *data, TF_LEN len);

/**
 * @brief  配置轮询从机列表和轮次间隔
 * @param  slave_addrs:       从机地址数组
 * @param  count:             从机数量 (不超过 TF_MASTER_MAX_POLL_SLAVES)
 * @param  round_interval_ms: 两轮轮询之间的间隔 (ms)
 */
void TF_Master_PollSetup(const uint8_t *slave_addrs, uint8_t count,
                          uint32_t round_interval_ms);

/**
 * @brief  设置轮询响应回调
 * @param  callback: 回调函数
 */
void TF_Master_SetPollCallback(TF_Master_PollCallback callback);

/**
 * @brief  主循环调用 — 驱动轮询状态机
 * @param  tf: TinyFrame 实例
 */
void TF_Master_PollTick(TinyFrame *tf);

#endif /* TF_MASTER_H */
