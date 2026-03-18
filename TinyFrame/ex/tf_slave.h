/**
 * @file    tf_slave.h
 * @brief   TinyFrame 从机端模块
 *
 * 从机端负责两类通信：
 *   接收处理 — 地址过滤 + 去重 + 业务回调 + ACK 响应
 *   主动上报 — 可靠发送到主机：内置 ACK 等待 + 超时自动重试
 *
 * 接收路径：
 *   TF_Accept() → Slave_AddressFilter() → 去重检查 → switch(msg_type)
 *     → 新帧：执行回调 + 回 ACK
 *     → 重复帧：只回 ACK，不执行业务逻辑
 *
 * 上报路径：
 *   ReportEvent() / ReportData() → SendToMaster() → TF_QuerySimple() + 重试
 *     → 收到主机 ACK → 完成
 *     → 超时 → 自动重发 (最多 TF_SLAVE_MAX_RETRIES 次)
 */

#ifndef TF_SLAVE_H
#define TF_SLAVE_H

#include "tf_multinode.h"
#include "TinyFrame.h"
#include "config.h"

/* ========================== 配置 ========================== */

/* 从机上报等待主机 ACK 的超时时间 (ticks, 1tick = 1ms)
 * SF11 BW500 下单帧空中时间约 50-70ms，往返需 120-140ms + 主循环延迟，
 * 因此超时需留足够裕量 */
#define TF_SLAVE_RESPONSE_TIMEOUT   500

/* 从机上报最大重试次数 (不含首次发送) */
#define TF_SLAVE_MAX_RETRIES        2

/* 从机上报重试缓存最大负载字节数 */
#define TF_SLAVE_MAX_PAYLOAD        32

/* ========================== 回调函数类型定义 ========================== */

/**
 * @brief  LED 控制回调函数类型
 * @param  led_cmd: LED 命令 (LED_CMD_OFF/ON/TOGGLE)
 */
typedef void (*TF_Slave_LedCallback)(TF_LedCmd led_cmd);

/**
 * @brief  状态请求回调函数类型
 * @param  status: 状态数据指针 (由回调填充 led_state, error_code 等)
 */
typedef void (*TF_Slave_StatusCallback)(TF_StatusData *status);

/**
 * @brief  CONFIG 控制命令回调函数类型
 *
 * 主机通过 TF_MSG_CONFIG 发送控制命令，payload 格式:
 *   payload[0] = 设备 ID (TF_CTRL_DEV_FAN / HEATER / PUMP)
 *   payload[1] = 动作值 (TF_CTRL_ACT_OFF / ON)
 *
 * @param  dev_id: 设备 ID
 * @param  action: 动作值 (0=关, 1=开)
 */
typedef void (*TF_Slave_ConfigCallback)(uint8_t dev_id, uint8_t action);

/**
 * @brief  threshold field update callback type
 * @param  field_id: TF_THRESH_FIELD_xxx
 * @param  value:    field value (int16_t)
 */
typedef void (*TF_Slave_ThresholdCallback)(uint8_t field_id, int16_t value);

/* ========================== API 函数 ========================== */

/**
 * @brief  初始化从机端 TinyFrame
 *
 * 初始化 TF 实例为从机模式，注册地址过滤监听器，初始化去重缓冲区。
 *
 * @param  tf:   TinyFrame 实例
 * @param  addr: 本机地址 (1-14)
 * @return 成功返回 true
 */
bool TF_Slave_Init(TinyFrame *tf, uint8_t addr);

/**
 * @brief  获取从机地址
 * @return 本机地址
 */
uint8_t TF_Slave_GetAddr(void);

/**
 * @brief  设置 LED 控制回调函数
 * @param  callback: 回调函数，收到 TF_MSG_LED_CTRL 时调用
 */
void TF_Slave_SetLedCallback(TF_Slave_LedCallback callback);

/**
 * @brief  设置状态请求回调函数
 * @param  callback: 回调函数，收到 TF_MSG_STATUS_REQ 时调用
 */
void TF_Slave_SetStatusCallback(TF_Slave_StatusCallback callback);

/**
 * @brief  设置 CONFIG 控制命令回调函数
 * @param  callback: 回调函数，收到 TF_MSG_CONFIG 时调用
 */
void TF_Slave_SetConfigCallback(TF_Slave_ConfigCallback callback);

/**
 * @brief  Set threshold config callback
 * @param  callback: called when TF_MSG_THRESHOLD is received from master
 */
void TF_Slave_SetThresholdCallback(TF_Slave_ThresholdCallback callback);

/**
 * @brief  发送 ACK 响应
 *
 * 使用 TF_Respond() 回复，携带原始 frame_id 供主机 ID 监听器匹配。
 *
 * @param  tf:  TinyFrame 实例
 * @param  msg: 原始消息 (用于获取 frame_id)
 * @return 成功返回 true
 */
bool TF_Slave_SendAck(TinyFrame *tf, TF_Msg *msg);

/**
 * @brief  发送 NACK 响应
 *
 * 使用 TF_Respond() 回复，携带原始 frame_id 供主机 ID 监听器匹配。
 *
 * @param  tf:  TinyFrame 实例
 * @param  msg: 原始消息 (用于获取 frame_id)
 * @return 成功返回 true
 */
bool TF_Slave_SendNack(TinyFrame *tf, TF_Msg *msg);

/**
 * @brief  发送自定义数据响应 (如状态查询的回复)
 *
 * 使用 TF_Respond() 回复，携带原始 frame_id。
 *
 * @param  tf:       TinyFrame 实例
 * @param  msg:      原始消息 (用于获取 frame_id)
 * @param  msg_type: 响应消息类型
 * @param  data:     数据负载
 * @param  len:      数据长度
 * @return 成功返回 true
 */
bool TF_Slave_Respond(TinyFrame *tf, TF_Msg *msg, TF_MsgType msg_type,
                      const uint8_t *data, TF_LEN len);

/**
 * @brief  可靠发送消息到主机 (主动上报)
 *
 * 内部使用 TF_QuerySimple 注册 ID 监听器等待主机 ACK。
 * 超时后自动重发，最多 TF_SLAVE_MAX_RETRIES 次。
 * ReportEvent() 和 ReportData() 都经过此函数，自动获得可靠性。
 * 负载会被拷贝到内部缓存，data 指针在返回后可释放。
 *
 * @param  tf:       TinyFrame 实例
 * @param  msg_type: 消息类型
 * @param  data:     数据负载 (可为 NULL)
 * @param  len:      数据长度 (不超过 TF_SLAVE_MAX_PAYLOAD)
 * @return 成功返回 true
 */
bool TF_Slave_SendToMaster(TinyFrame *tf, TF_MsgType msg_type,
                           const uint8_t *data, TF_LEN len);

/**
 * @brief  上报事件到主机 (经 SendToMaster，可靠)
 * @param  tf:         TinyFrame 实例
 * @param  event_code: 事件代码 (1 字节)
 * @return 成功返回 true
 */
bool TF_Slave_ReportEvent(TinyFrame *tf, uint8_t event_code);

/**
 * @brief  上报数据到主机 (经 SendToMaster，可靠)
 * @param  tf:   TinyFrame 实例
 * @param  data: 数据负载
 * @param  len:  数据长度 (不超过 TF_SLAVE_MAX_PAYLOAD)
 * @return 成功返回 true
 */
bool TF_Slave_ReportData(TinyFrame *tf, const uint8_t *data, TF_LEN len);

/**
 * @brief  检查当前是否在本机 TDMA 时间槽内
 * @param  tick_ms: 当前系统时间 (ms)
 * @return true = 可以发送, false = 等待
 */
bool TF_Slave_CanSendNow(uint32_t tick_ms);

/**
 * @brief  在时间槽内发送数据 (经 SendToMaster，可靠 + 防冲突)
 *
 * 先检查 TDMA 时间槽，在窗口内才通过 SendToMaster 发送。
 *
 * @param  tf:       TinyFrame 实例
 * @param  tick_ms:  当前系统时间 (ms)
 * @param  msg_type: 消息类型
 * @param  data:     数据负载
 * @param  len:      数据长度
 * @return 成功返回 true, 不在时间槽内返回 false
 */
bool TF_Slave_SendInSlot(TinyFrame *tf, uint32_t tick_ms, TF_MsgType msg_type,
                         const uint8_t *data, TF_LEN len);

#endif /* TF_SLAVE_H */
