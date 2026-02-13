/**
 * @file    tf_master.c
 * @brief   TinyFrame 主机端模块实现
 */

#include "tf_master.h"
#include "usbd_cdc_if.h"

/* ========================== 内部变量 ========================== */

/* ========================== 内部函数 ========================== */

/**
 * @brief  通用响应处理监听器
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
        default:
            break;
    }

    return TF_STAY;
}

/**
 * @brief  ID 监听器超时处理
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

    /* 初始化为主机模式 */
    if (!TF_InitStatic(tf, TF_MASTER)) {
        return false;
    }

    /* 注册通用监听器处理从机响应 */
    TF_AddGenericListener(tf, Master_GenericListener);

    usb_printf("[Master] Initialized\r\n");
    return true;
}

bool TF_Master_SendTo(TinyFrame *tf, uint8_t slave_addr, TF_MsgType msg_type,
                      const uint8_t *data, TF_LEN len)
{
    if (tf == NULL || slave_addr < TF_ADDR_SLAVE_MIN || slave_addr > TF_ADDR_SLAVE_MAX) {
        return false;
    }

    TF_TYPE type = TF_MAKE_TYPE(slave_addr, msg_type);

    usb_printf("[Master] Send to Slave %d, MsgType=0x%02X\r\n", slave_addr, msg_type);

    return TF_SendSimple(tf, type, data, len);
}

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

bool TF_Master_Broadcast(TinyFrame *tf, TF_MsgType msg_type,
                         const uint8_t *data, TF_LEN len)
{
    if (tf == NULL) {
        return false;
    }

    TF_TYPE type = TF_MAKE_TYPE(TF_ADDR_BROADCAST, msg_type);

    usb_printf("[Master] Broadcast MsgType=0x%02X\r\n", msg_type);

    return TF_SendSimple(tf, type, data, len);
}

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
