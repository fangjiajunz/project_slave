/**
 * @file    tf_slave.c
 * @brief   TinyFrame 从机端模块实现
 */

#include "tf_slave.h"
#include "usbd_cdc_if.h"

/* ========================== 内部变量 ========================== */

static uint8_t s_slave_addr = 0;                    /* 本机地址 */
static TF_Slave_LedCallback s_led_callback = NULL;  /* LED 回调 */
static TF_Slave_StatusCallback s_status_callback = NULL;  /* 状态回调 */

/* ========================== 内部函数 ========================== */

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
        /* 地址不匹配, 忽略 */
        return TF_NEXT;
    }

    usb_printf("[Slave %d] Recv Addr=%d, MsgType=0x%02X, Len=%d, Broadcast=%d\r\n",
           s_slave_addr, addr, msg_type, msg->len, is_broadcast);

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

bool TF_Slave_SendToMaster(TinyFrame *tf, TF_MsgType msg_type,
                           const uint8_t *data, TF_LEN len)
{
    if (tf == NULL) {
        return false;
    }

    /* 主动发送用 TF_SendSimple，TYPE 带上本机地址 */
    TF_TYPE type = TF_MAKE_TYPE(s_slave_addr, msg_type);

    usb_printf("[Slave %d] Send to Master, MsgType=0x%02X\r\n", s_slave_addr, msg_type);

    return TF_SendSimple(tf, type, data, len);
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
