/**
 * @file    tf_master.c
 * @brief   TinyFrame 主机端模块实现
 *
 * 主机端通信模块，提供三种发送路径：
 *
 *   SendTo()    — 可靠单播：发送后等待从机 ACK，超时自动重试 (最多 TF_MASTER_MAX_RETRIES 次)
 *   QueryTo()   — 查询单播：发送后等待从机自定义响应，超时由调用方处理，不自动重试
 *   Broadcast() — 广播：发后即忘，从机不回复
 *
 * 调用链：
 *   SendLedCmd()      → SendTo()    → TF_QuerySimple() + 重试  (可靠)
 *   SendHeartbeat()   → QueryTo()   → TF_QuerySimple()          (调用方控制)
 *   BroadcastLedCmd() → Broadcast() → TF_SendSimple()           (不可靠)
 */

#include "tf_master.h"
#include "usbd_cdc_if.h"
#include <string.h>

/* ========================== 内部变量 ========================== */

/**
 * 重试上下文 — 保存当前待确认命令的参数，供超时重发使用。
 *
 * 单例设计：同一时间只能有一个待确认的 SendTo 命令。
 * LoRa 半双工特性决定了不可能同时收发，因此单例足够。
 * 如果在前一条命令未完成时再次调用 SendTo，新命令会覆盖旧上下文。
 */
typedef struct {
    uint8_t     slave_addr;                     /* 目标从机地址 */
    TF_MsgType  msg_type;                       /* 消息类型 */
    uint8_t     payload[TF_MASTER_MAX_PAYLOAD]; /* 负载缓存 (超时重发时从这里取数据) */
    TF_LEN      payload_len;                    /* 负载长度 */
    uint8_t     retries_left;                   /* 剩余重试次数 */
} RetryContext;

static RetryContext s_retry_ctx;

/* 从机数据接收回调 */
static TF_Master_DataCallback s_data_callback = NULL;

/* 去重缓冲区 — 检测从机重传导致的重复上报 */
static TF_DedupBuf s_dedup;

/* ========================== 内部函数 ========================== */

void TF_Master_SetDataCallback(TF_Master_DataCallback callback)
{
    s_data_callback = callback;
}

/**
 * @brief  通用监听器 — 处理不携带 ID 监听器的从机消息
 *
 * TinyFrame 消息派发优先级：ID 监听器 > Type 监听器 > Generic 监听器
 * 因此 SendTo 的 ACK 会被 Master_AckListener (ID 监听器) 优先捕获，
 * 不会到达这里。这里主要处理从机主动上报的 DATA、非预期的消息等。
 */
static TF_Result Master_GenericListener(TinyFrame *tf, TF_Msg *msg)
{
    uint8_t addr = TF_GET_ADDR(msg->type);
    uint8_t msg_type = TF_GET_MSG(msg->type);

    usb_printf("[Master] Recv from Slave %d, MsgType=0x%02X, Len=%d\r\n",
           addr, msg_type, msg->len);

    switch (msg_type) {
        case TF_MSG_ACK:
            usb_printf("[Master] ACK from Slave %d\r\n", addr);
            break;
        case TF_MSG_NACK:
            usb_printf("[Master] NACK from Slave %d\r\n", addr);
            break;
        case TF_MSG_HEARTBEAT:
            usb_printf("[Master] Heartbeat from Slave %d\r\n", addr);
            break;
        case TF_MSG_STATUS_RSP:
            if (msg->len >= sizeof(TF_StatusData)) {
                TF_StatusData *status = (TF_StatusData *)msg->data;
                usb_printf("[Master] Status: Addr=%d, LED=%d, Err=%d\r\n",
                       status->node_addr, status->led_state, status->error_code);
            }
            break;
        case TF_MSG_DATA:
            usb_printf("[Master] Data from Slave %d, Len=%d\r\n", addr, msg->len);
            /* 去重：重复帧只回 ACK，不重复调用用户回调 */
            if (!TF_Dedup_IsDuplicate(&s_dedup, addr, msg->frame_id)) {
                if (s_data_callback != NULL) {
                    s_data_callback(addr, msg->data, msg->len);
                }
            } else {
                usb_printf("[Master] Duplicate frame %d from Slave %d, ACK only\r\n",
                           msg->frame_id, addr);
            }
            /* 无论新帧还是重复帧，都回 ACK 让从机停止重试 */
            {
                TF_Msg response;
                TF_ClearMsg(&response);
                response.frame_id    = msg->frame_id;
                response.is_response = true;
                response.type = TF_MAKE_TYPE(TF_ADDR_MASTER, TF_MSG_ACK);
                response.data = NULL;
                response.len  = 0;
                TF_Respond(tf, &response);
            }
            break;
        default:
            break;
    }

    return TF_STAY;
}

/**
 * @brief  SendTo 的 ID 监听器 — 收到从机 ACK/NACK 时触发
 *
 * 从机用 TF_Respond() 回复时携带相同的 frame_id，TinyFrame 据此
 * 匹配到这里。无论 ACK 还是 NACK 都视为"收到响应"，停止重试。
 *
 * @return TF_CLOSE 移除此 ID 监听器
 */
static TF_Result Master_AckListener(TinyFrame *tf, TF_Msg *msg)
{
    uint8_t addr = TF_GET_ADDR(msg->type);
    uint8_t msg_type = TF_GET_MSG(msg->type);

    if (msg_type == TF_MSG_ACK) {
        usb_printf("[Master] ACK from Slave %d\r\n", addr);
    } else if (msg_type == TF_MSG_NACK) {
        usb_printf("[Master] NACK from Slave %d\r\n", addr);
    }
    s_retry_ctx.retries_left = 0;   /* 收到响应，停止重试 */
    return TF_CLOSE;                /* 移除此 ID 监听器 */
}

/**
 * @brief  SendTo 的超时回调 — 未收到响应时触发
 *
 * 如果还有剩余重试次数，从 s_retry_ctx 取出保存的参数重新发送，
 * 并注册新的 ID 监听器继续等待。否则放弃。
 *
 * 注意：每次重发会产生新的 frame_id，从机会视为新消息。
 * 如果从机已执行过但 ACK 丢失，会导致重复执行。
 * 需配合去重机制（步骤 3）解决。
 *
 * @return TF_CLOSE 移除当前已超时的 ID 监听器
 */
static TF_Result Master_RetryTimeoutHandler(TinyFrame *tf)
{
    if (s_retry_ctx.retries_left > 0) {
        s_retry_ctx.retries_left--;
        usb_printf("[Master] Timeout, retry (%d left)\r\n", s_retry_ctx.retries_left);

        /* 从重试上下文恢复参数，重新发送 */
        TF_TYPE type = TF_MAKE_TYPE(s_retry_ctx.slave_addr, s_retry_ctx.msg_type);
        TF_QuerySimple(tf, type,
                        s_retry_ctx.payload, s_retry_ctx.payload_len,
                        Master_AckListener, Master_RetryTimeoutHandler,
                        TF_MASTER_RESPONSE_TIMEOUT);
    } else {
        usb_printf("[Master] All retries exhausted\r\n");
    }
    return TF_CLOSE;
}

/**
 * @brief  QueryTo 专用超时处理 — 不自动重试，由调用方决定后续动作
 */
static TF_Result Master_TimeoutHandler(TinyFrame *tf)
{
    usb_printf("[Master] Response timeout!\r\n");
    return TF_CLOSE;
}

/* ========================== API 实现 ========================== */

bool TF_Master_Init(TinyFrame *tf)
{
    if (tf == NULL) {
        return false;
    }

    if (!TF_InitStatic(tf, TF_MASTER)) {
        return false;
    }

    /* 初始化去重缓冲区 */
    TF_Dedup_Init(&s_dedup);

    TF_AddGenericListener(tf, Master_GenericListener);

    usb_printf("[Master] Initialized\r\n");
    return true;
}

/**
 * @brief  可靠发送消息到指定从机
 *
 * 内部使用 TF_QuerySimple 注册 ID 监听器等待从机 ACK。
 * 超时后自动重发，最多重试 TF_MASTER_MAX_RETRIES 次。
 * 所有经过此函数的命令（LED、CONFIG 等）自动获得可靠性。
 *
 * 注意：负载会被拷贝到 s_retry_ctx.payload 中缓存，
 * 因此调用方传入的 data 指针在函数返回后可以释放。
 */
bool TF_Master_SendTo(TinyFrame *tf, uint8_t slave_addr, TF_MsgType msg_type,
                      const uint8_t *data, TF_LEN len)
{
    if (tf == NULL || slave_addr < TF_ADDR_SLAVE_MIN || slave_addr > TF_ADDR_SLAVE_MAX) {
        return false;
    }
    if (len > TF_MASTER_MAX_PAYLOAD) {
        usb_printf("[Master] Payload too large for retry buffer\r\n");
        return false;
    }

    /* 保存命令参数到重试上下文，超时重发时使用 */
    s_retry_ctx.slave_addr   = slave_addr;
    s_retry_ctx.msg_type     = msg_type;
    if (data != NULL && len > 0) {
        memcpy(s_retry_ctx.payload, data, len);
    }
    s_retry_ctx.payload_len  = len;
    s_retry_ctx.retries_left = TF_MASTER_MAX_RETRIES;

    TF_TYPE type = TF_MAKE_TYPE(slave_addr, msg_type);

    usb_printf("[Master] Send to Slave %d, MsgType=0x%02X\r\n", slave_addr, msg_type);

    /*
     * TF_QuerySimple 与 TF_SendSimple 的区别：
     * - TF_SendSimple:  发帧 → 结束 (发后即忘)
     * - TF_QuerySimple: 发帧 → 注册 ID 监听器 → 等从机 TF_Respond → 触发 AckListener
     *                                          → 超时 → 触发 RetryTimeoutHandler
     */
    return TF_QuerySimple(tf, type, data, len,
                           Master_AckListener, Master_RetryTimeoutHandler,
                           TF_MASTER_RESPONSE_TIMEOUT);
}

/**
 * @brief  查询发送 — 等待从机自定义响应，不自动重试
 *
 * 与 SendTo 的区别：listener 由调用方传入，用于处理自定义响应数据
 * （如状态查询返回的 StatusData）。超时仅打印日志，不重发。
 * 如需重试，由调用方在自己的超时回调中处理。
 */
bool TF_Master_QueryTo(TinyFrame *tf, uint8_t slave_addr, TF_MsgType msg_type,
                       const uint8_t *data, TF_LEN len, TF_Listener listener)
{
    if (tf == NULL || slave_addr < TF_ADDR_SLAVE_MIN || slave_addr > TF_ADDR_SLAVE_MAX) {
        return false;
    }

    TF_TYPE type = TF_MAKE_TYPE(slave_addr, msg_type);

    usb_printf("[Master] Query to Slave %d, MsgType=0x%02X\r\n", slave_addr, msg_type);

    return TF_QuerySimple(tf, type, data, len, listener, Master_TimeoutHandler,
                          TF_MASTER_RESPONSE_TIMEOUT);
}

/**
 * @brief  广播消息 — 发后即忘，从机不回复
 */
bool TF_Master_Broadcast(TinyFrame *tf, TF_MsgType msg_type,
                         const uint8_t *data, TF_LEN len)
{
    if (tf == NULL) {
        return false;
    }

    TF_TYPE type = TF_MAKE_TYPE(TF_ADDR_BROADCAST, msg_type);

    usb_printf("[Master] Broadcast MsgType=0x%02X\r\n", msg_type);

    /* 广播无人 ACK，使用 TF_SendSimple 发后即忘 */
    return TF_SendSimple(tf, type, data, len);
}

/* ---- 便捷 API：内部调用 SendTo/Broadcast/QueryTo，自动继承对应的可靠性策略 ---- */

bool TF_Master_SendLedCmd(TinyFrame *tf, uint8_t slave_addr, TF_LedCmd led_cmd)
{
    uint8_t data = (uint8_t)led_cmd;
    return TF_Master_SendTo(tf, slave_addr, TF_MSG_LED_CTRL, &data, 1);
}

bool TF_Master_BroadcastLedCmd(TinyFrame *tf, TF_LedCmd led_cmd)
{
    uint8_t data = (uint8_t)led_cmd;
    return TF_Master_Broadcast(tf, TF_MSG_LED_CTRL, &data, 1);
}

bool TF_Master_SendHeartbeat(TinyFrame *tf, uint8_t slave_addr, TF_Listener listener)
{
    return TF_Master_QueryTo(tf, slave_addr, TF_MSG_HEARTBEAT, NULL, 0, listener);
}
