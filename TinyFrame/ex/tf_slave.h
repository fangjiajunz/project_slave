/**
 * @file    tf_slave.h
 * @brief   TinyFrame 从机端模块
 */

#ifndef TF_SLAVE_H
#define TF_SLAVE_H

#include "tf_multinode.h"
#include "TinyFrame.h"

/* ========================== 回调函数类型定义 ========================== */

/**
 * @brief  LED 控制回调函数类型
 * @param  led_cmd: LED 命令
 */
typedef void (*TF_Slave_LedCallback)(TF_LedCmd led_cmd);

/**
 * @brief  状态请求回调函数类型
 * @param  status: 状态数据指针 (由回调填充)
 */
typedef void (*TF_Slave_StatusCallback)(TF_StatusData *status);

/* ========================== API 函数 ========================== */

/**
 * @brief  初始化从机端 TinyFrame
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
 * @param  callback: 回调函数
 */
void TF_Slave_SetLedCallback(TF_Slave_LedCallback callback);

/**
 * @brief  设置状态请求回调函数
 * @param  callback: 回调函数
 */
void TF_Slave_SetStatusCallback(TF_Slave_StatusCallback callback);

/**
 * @brief  发送 ACK 响应
 * @param  tf:  TinyFrame 实例
 * @param  msg: 原始消息 (用于获取 frame_id)
 * @return 成功返回 true
 */
bool TF_Slave_SendAck(TinyFrame *tf, TF_Msg *msg);

/**
 * @brief  发送 NACK 响应
 * @param  tf:  TinyFrame 实例
 * @param  msg: 原始消息 (用于获取 frame_id)
 * @return 成功返回 true
 */
bool TF_Slave_SendNack(TinyFrame *tf, TF_Msg *msg);

/**
 * @brief  发送数据响应
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
 * @brief  从机主动发送消息到主机
 * @param  tf:       TinyFrame 实例
 * @param  msg_type: 消息类型
 * @param  data:     数据负载 (可为 NULL)
 * @param  len:      数据长度
 * @return 成功返回 true
 */
bool TF_Slave_SendToMaster(TinyFrame *tf, TF_MsgType msg_type,
                           const uint8_t *data, TF_LEN len);

/**
 * @brief  从机主动上报事件到主机
 * @param  tf:         TinyFrame 实例
 * @param  event_code: 事件代码
 * @return 成功返回 true
 */
bool TF_Slave_ReportEvent(TinyFrame *tf, uint8_t event_code);

/**
 * @brief  从机主动上报传感器数据到主机
 * @param  tf:   TinyFrame 实例
 * @param  data: 数据负载
 * @param  len:  数据长度
 * @return 成功返回 true
 */
bool TF_Slave_ReportData(TinyFrame *tf, const uint8_t *data, TF_LEN len);

/**
 * @brief  检查当前是否在本机时间槽内
 * @param  tick_ms: 当前系统时间 (ms)
 * @return true = 可以发送, false = 等待
 */
bool TF_Slave_CanSendNow(uint32_t tick_ms);

/**
 * @brief  在时间槽内发送数据 (带冲突避免)
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
