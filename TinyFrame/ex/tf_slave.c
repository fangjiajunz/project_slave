/**
 * @file    tf_slave.c
 * @brief   TinyFrame 从机端模块实现
 */

#include "tf_slave.h"
#include "usbd_cdc_if.h"
#include <string.h>

/* ========================== 内部变量 ========================== */

static uint8_t s_slave_addr = 0;                    /* 本机地址 */
static TF_Slave_LedCallback s_led_callback = NULL;  /* LED 回调 */
static TF_Slave_StatusCallback s_status_callback = NULL;  /* 状态回调 */

/**
 * 从机上报重试上下文 — 保存当前待确认的上报消息参数。
 * 单例设计，同一时间只能有一个待确认的上报。
 */
static struct {
    TF_MsgType  msg_type;                           /* 消息类型 */
    uint8_t     payload[TF_SLAVE_MAX_PAYLOAD];      /* 负载缓存 */
    TF_LEN      payload_len;                        /* 负载长度 */
    uint8_t     retries_left;                       /* 剩余重试次数 */
} s_report_retry;

/* 去重缓冲区 — 检测主机重传导致的重复命令 */
static TF_DedupBuf s_dedup;

/* ========================== 内部函数 ========================== */

/**
 * @brief  上报的 ID 监听器 — 收到主机 ACK 时触发
 *
 * 主机在 Master_GenericListener 中收到 DATA 后，
 * 用 TF_Respond() 回复 ACK（携带相同 frame_id），匹配到这里。
 *
 * @return TF_CLOSE 移除此 ID 监听器
 */
static TF_Result Slave_MasterAckListener(TinyFrame *tf, TF_Msg *msg)
{
    usb_printf("[Slave %d] Master ACK received\r\n", s_slave_addr);
    s_report_retry.retries_left = 0;    /* 收到确认，停止重试 */
    return TF_CLOSE;
}

/**
 * @brief  上报超时回调 — 未收到主机 ACK 时触发
 *
 * 从 s_report_retry 取出保存的参数重新发送，或放弃。
 *
 * @return TF_CLOSE 移除当前已超时的 ID 监听器
 */
static TF_Result Slave_ReportTimeoutHandler(TinyFrame *tf)
{
    if (s_report_retry.retries_left > 0) {
        s_report_retry.retries_left--;
        usb_printf("[Slave %d] Report timeout, retry (%d left)\r\n",
                   s_slave_addr, s_report_retry.retries_left);

        /* 从重试上下文恢复参数，重新发送 */
        TF_TYPE type = TF_MAKE_TYPE(s_slave_addr, s_report_retry.msg_type);
        TF_QuerySimple(tf, type,
                        s_report_retry.payload, s_report_retry.payload_len,
                        Slave_MasterAckListener, Slave_ReportTimeoutHandler,
                        TF_SLAVE_RESPONSE_TIMEOUT);
    } else {
        usb_printf("[Slave %d] Report failed, master unreachable\r\n", s_slave_addr);
    }
    return TF_CLOSE;
}

/**
 * @brief  地址过滤监听器 - 处理发送给本机的消息
 */
static TF_Result Slave_AddressFilter(TinyFrame *tf, TF_Msg *msg)
{
    uint8_t addr = TF_GET_ADDR(msg->type);
    uint8_t msg_type = TF_GET_MSG(msg->type);
    bool is_broadcast = TF_IS_BROADCAST(msg->type);

    /* 地址过滤: 只处理发给本机或广播的消息 */
    if (!TF_ADDR_MATCH(msg->type, s_slave_addr)) {
        /* 地址不匹配, 静默忽略 */
        return TF_STAY;
    }

    usb_printf("[Slave %d] Recv Addr=%d, MsgType=0x%02X, Len=%d, Broadcast=%d\r\n",
           s_slave_addr, addr, msg_type, msg->len, is_broadcast);

    /* 去重检查：如果是重复帧，只回 ACK 不执行业务逻辑。
     * 场景：主机发了 LED_TOGGLE，从机执行并回 ACK，但 ACK 丢失，
     * 主机重发 LED_TOGGLE，没有去重的话 LED 会翻转两次。 */
    if (TF_Dedup_IsDuplicate(&s_dedup, TF_ADDR_MASTER, msg->frame_id)) {
        usb_printf("[Slave %d] Duplicate frame %d, ACK only\r\n",
                   s_slave_addr, msg->frame_id);
        if (!is_broadcast) {
            TF_Slave_SendAck(tf, msg);
        }
        return TF_STAY;
    }

    /* 根据消息类型处理 */
    switch (msg_type) {
        case TF_MSG_HEARTBEAT:
            usb_printf("[Slave %d] Heartbeat request\r\n", s_slave_addr);
            if (!is_broadcast) {
                /* 单播消息需要响应 */
                TF_Slave_SendAck(tf, msg);
            }
            break;

        case TF_MSG_LED_CTRL:
            usb_printf("[Slave %d] LED control command\r\n", s_slave_addr);
            if (msg->len > 0 && s_led_callback != NULL) {
                TF_LedCmd cmd = (TF_LedCmd)msg->data[0];
                s_led_callback(cmd);
            }
            if (!is_broadcast) {
                TF_Slave_SendAck(tf, msg);
            }
            break;

        case TF_MSG_STATUS_REQ:
            usb_printf("[Slave %d] Status request\r\n", s_slave_addr);
            if (!is_broadcast && s_status_callback != NULL) {
                TF_StatusData status;
                status.node_addr = s_slave_addr;
                s_status_callback(&status);
                TF_Slave_Respond(tf, msg, TF_MSG_STATUS_RSP,
                                (uint8_t *)&status, sizeof(status));
            }
            break;

        case TF_MSG_CONFIG:
            usb_printf("[Slave %d] Config command\r\n", s_slave_addr);
            if (!is_broadcast) {
                TF_Slave_SendAck(tf, msg);
            }
            break;

        default:
            usb_printf("[Slave %d] Unknown message type: 0x%02X\r\n",
                   s_slave_addr, msg_type);
            if (!is_broadcast) {
                TF_Slave_SendNack(tf, msg);
            }
            break;
    }

    return TF_STAY;
}

/* ========================== API 实现 ========================== */

bool TF_Slave_Init(TinyFrame *tf, uint8_t addr)
{
    if (tf == NULL) {
        return false;
    }

    /* 验证地址范围 */
    if (addr < TF_ADDR_SLAVE_MIN || addr > TF_ADDR_SLAVE_MAX) {
        usb_printf("[Slave] Invalid address: %d\r\n", addr);
        return false;
    }

    /* 保存本机地址 */
    s_slave_addr = addr;

    /* 初始化去重缓冲区 */
    TF_Dedup_Init(&s_dedup);

    /* 初始化为从机模式 */
    if (!TF_InitStatic(tf, TF_SLAVE)) {
        return false;
    }

    /* 注册通用监听器进行地址过滤 */
    TF_AddGenericListener(tf, Slave_AddressFilter);

    usb_printf("[Slave %d] Initialized\r\n", s_slave_addr);
    return true;
}

uint8_t TF_Slave_GetAddr(void)
{
    return s_slave_addr;
}

void TF_Slave_SetLedCallback(TF_Slave_LedCallback callback)
{
    s_led_callback = callback;
}

void TF_Slave_SetStatusCallback(TF_Slave_StatusCallback callback)
{
    s_status_callback = callback;
}

bool TF_Slave_SendAck(TinyFrame *tf, TF_Msg *msg)
{
    TF_Msg response;
    TF_ClearMsg(&response);

    response.frame_id = msg->frame_id;
    response.is_response = true;
    response.type = TF_MAKE_TYPE(s_slave_addr, TF_MSG_ACK);
    response.data = NULL;
    response.len = 0;

    usb_printf("[Slave %d] Send ACK\r\n", s_slave_addr);

    return TF_Respond(tf, &response);
}

bool TF_Slave_SendNack(TinyFrame *tf, TF_Msg *msg)
{
    TF_Msg response;
    TF_ClearMsg(&response);

    response.frame_id = msg->frame_id;
    response.is_response = true;
    response.type = TF_MAKE_TYPE(s_slave_addr, TF_MSG_NACK);
    response.data = NULL;
    response.len = 0;

    usb_printf("[Slave %d] Send NACK\r\n", s_slave_addr);

    return TF_Respond(tf, &response);
}

bool TF_Slave_Respond(TinyFrame *tf, TF_Msg *msg, TF_MsgType msg_type,
                      const uint8_t *data, TF_LEN len)
{
    TF_Msg response;
    TF_ClearMsg(&response);

    response.frame_id = msg->frame_id;
    response.is_response = true;
    response.type = TF_MAKE_TYPE(s_slave_addr, msg_type);
    response.data = data;
    response.len = len;

    usb_printf("[Slave %d] Send Response, MsgType=0x%02X\r\n", s_slave_addr, msg_type);

    return TF_Respond(tf, &response);
}

/**
 * @brief  可靠发送消息到主机 (主动上报)
 *
 * 内部使用 TF_QuerySimple 注册 ID 监听器等待主机 ACK。
 * 超时后自动重发，最多重试 TF_SLAVE_MAX_RETRIES 次。
 * ReportEvent 和 ReportData 都经过此函数，自动获得可靠性。
 *
 * 注意：负载会被拷贝到 s_report_retry.payload 中缓存，
 * 调用方传入的 data 指针在函数返回后可以释放。
 */
bool TF_Slave_SendToMaster(TinyFrame *tf, TF_MsgType msg_type,
                           const uint8_t *data, TF_LEN len)
{
    if (tf == NULL) {
        return false;
    }
    if (len > TF_SLAVE_MAX_PAYLOAD) {
        usb_printf("[Slave %d] Payload too large for retry buffer\r\n", s_slave_addr);
        return false;
    }

    /* 保存上报参数到重试上下文，超时重发时使用 */
    s_report_retry.msg_type    = msg_type;
    if (data != NULL && len > 0) {
        memcpy(s_report_retry.payload, data, len);
    }
    s_report_retry.payload_len = len;
    s_report_retry.retries_left = TF_SLAVE_MAX_RETRIES;

    TF_TYPE type = TF_MAKE_TYPE(s_slave_addr, msg_type);

    usb_printf("[Slave %d] Send to Master, MsgType=0x%02X\r\n", s_slave_addr, msg_type);

    return TF_QuerySimple(tf, type, data, len,
                           Slave_MasterAckListener, Slave_ReportTimeoutHandler,
                           TF_SLAVE_RESPONSE_TIMEOUT);
}

bool TF_Slave_ReportEvent(TinyFrame *tf, uint8_t event_code)
{
    return TF_Slave_SendToMaster(tf, TF_MSG_DATA, &event_code, 1);
}

bool TF_Slave_ReportData(TinyFrame *tf, const uint8_t *data, TF_LEN len)
{
    return TF_Slave_SendToMaster(tf, TF_MSG_DATA, data, len);
}

bool TF_Slave_CanSendNow(uint32_t tick_ms)
{
    return TF_IN_MY_SLOT(tick_ms, s_slave_addr);
}

bool TF_Slave_SendInSlot(TinyFrame *tf, uint32_t tick_ms, TF_MsgType msg_type,
                         const uint8_t *data, TF_LEN len)
{
    if (!TF_Slave_CanSendNow(tick_ms)) {
        return false;
    }
    return TF_Slave_SendToMaster(tf, msg_type, data, len);
}
