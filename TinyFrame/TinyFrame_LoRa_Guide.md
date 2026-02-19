# TinyFrame LoRa 主从通信指南

## 目录

1. [系统概述](#1-系统概述)
2. [快速开始](#2-快速开始)
3. [架构说明](#3-架构说明)
4. [配置说明](#4-配置说明)
5. [API 参考](#5-api-参考)
6. [扩展开发](#6-扩展开发)
7. [调试技巧](#7-调试技巧)
8. [可靠传输实现指南](#8-可靠传输实现指南)

---

## 1. 系统概述

基于 TinyFrame 协议的 LoRa 无线主从通信系统。

### 特性

- 支持 1 主机 + 14 从机
- 地址过滤，从机只响应发给自己的消息
- 支持广播（所有从机收，不响应）
- 回调机制，业务逻辑与协议分离
- 条件编译，主从代码完全分离

### 文件结构

```
TinyFrame/
├── TinyFrame.c/h        # TinyFrame 核心库
├── TF_Config.h          # TinyFrame 配置
├── TF_Integration.c     # HAL 实现 (LoRa 发送)
└── ex/
    ├── tf_multinode.h   # 协议定义 (消息类型、地址规则)
    ├── tf_master.c/h    # 主机模块
    └── tf_slave.c/h     # 从机模块
```

---

## 2. 快速开始

### 2.1 编译从机固件

`main.c` 中设置：

```c
#define TF_NODE_IS_MASTER   0       // 0 = 从机
#define TF_SLAVE_ADDRESS    1       // 从机地址 (1-14)
```

### 2.2 编译主机固件

```c
#define TF_NODE_IS_MASTER   1       // 1 = 主机
#define TF_TARGET_SLAVE     1       // 目标从机地址
```

### 2.3 烧录测试

1. 烧录从机固件到板子 A
2. 烧录主机固件到板子 B
3. 主机按 UP 键 → 从机 LED 亮
4. 主机按 DOWN 键 → 从机 LED 灭
5. 主机按 ENTER 键 → 从机 LED 翻转

---

## 3. 架构说明

### 3.1 通信流程

```
主机                                    从机
┌──────────────┐                      ┌──────────────┐
│ 按键检测      │                      │              │
│     ↓        │                      │              │
│ TF_Master_   │    LoRa 无线         │ TF_Accept()  │
│ SendLedCmd() │ ─────────────────→   │     ↓        │
│     ↓        │                      │ 地址过滤     │
│ TF_WriteImpl │                      │     ↓        │
│     ↓        │                      │ LedCallback()│
│ e22_transmit │                      │     ↓        │
│              │                      │ LED 控制     │
│              │                      │     ↓        │
│              │    LoRa 无线         │ SendAck()    │
│ 收到 ACK     │ ←─────────────────   │              │
└──────────────┘                      └──────────────┘
```

### 3.2 帧格式

TinyFrame 帧结构：

```
| SOF | ID | LEN | TYPE | HEAD_CKSUM | DATA | DATA_CKSUM |
|  1  |  1 |  1  |  1   |     1      |  N   |     1      |
```

TYPE 字段复用为地址+消息类型：

```
TYPE (8 bits):
┌────────┬────────┐
│ ADDR   │ MSG    │
│ [7:4]  │ [3:0]  │
└────────┴────────┘

ADDR: 0x0 = 广播, 0x1-0xE = 从机地址
MSG:  消息类型 (LED_CTRL, HEARTBEAT, etc.)
```

### 3.3 主循环结构

```c
while (1) {
    // 1. USB 轮询
    uart_tx_poll();
    uart_rx_poll();

    // 2. LoRa 接收 → TinyFrame 解析
    if (e22_demo_check_rx_done(rx_buf, &rx_len, &rssi)) {
        TF_Accept(&tf, rx_buf, rx_len);
    }

#if TF_NODE_IS_MASTER
    // 3. 主机：按键触发发送
    if (key_check_press(KEY_NAME_UP)) {
        TF_Master_SendLedCmd(&tf, target, LED_CMD_ON);
    }
#else
    // 3. 从机：无需额外逻辑，回调自动处理
#endif
}
```

---

## 4. 配置说明

### 4.1 角色配置 (main.c)

```c
/* ============ 节点角色配置 ============ */
#define TF_NODE_IS_MASTER   0       // 1=主机, 0=从机
#define TF_SLAVE_ADDRESS    1       // 从机地址 (1-14)
#define TF_TARGET_SLAVE     1       // 主机发送目标
```

### 4.2 TinyFrame 配置 (TF_Config.h)

```c
#define TF_ID_BYTES     1       // 帧ID字节数
#define TF_LEN_BYTES    1       // 长度字节数
#define TF_TYPE_BYTES   1       // 类型字节数
#define TF_CKSUM_TYPE   TF_CKSUM_XOR  // 校验和类型
#define TF_MAX_PAYLOAD  256     // 最大负载
```

### 4.3 超时配置 (tf_master.h)

```c
#define TF_MASTER_RESPONSE_TIMEOUT  100  // 响应超时 (ticks)
```

---

## 5. API 参考

### 5.1 主机 API (tf_master.h)

| 函数 | 说明 |
|------|------|
| `TF_Master_Init(tf)` | 初始化主机 |
| `TF_Master_SendTo(tf, addr, type, data, len)` | 发送到指定从机 |
| `TF_Master_Broadcast(tf, type, data, len)` | 广播到所有从机 |
| `TF_Master_SendLedCmd(tf, addr, cmd)` | 发送 LED 控制命令 |
| `TF_Master_BroadcastLedCmd(tf, cmd)` | 广播 LED 控制命令 |
| `TF_Master_SendHeartbeat(tf, addr, listener)` | 发送心跳查询 |

### 5.2 从机 API (tf_slave.h)

| 函数 | 说明 |
|------|------|
| `TF_Slave_Init(tf, addr)` | 初始化从机，设置地址 |
| `TF_Slave_SetLedCallback(cb)` | 注册 LED 控制回调 |
| `TF_Slave_SetStatusCallback(cb)` | 注册状态查询回调 |
| `TF_Slave_SendAck(tf, msg)` | 发送 ACK 响应 |
| `TF_Slave_SendNack(tf, msg)` | 发送 NACK 响应 |
| `TF_Slave_SendToMaster(tf, type, data, len)` | 主动上报数据 |

### 5.3 消息类型 (tf_multinode.h)

```c
TF_MSG_ACK          = 0x00    // 确认
TF_MSG_NACK         = 0x01    // 否定
TF_MSG_HEARTBEAT    = 0x02    // 心跳
TF_MSG_LED_CTRL     = 0x03    // LED 控制
TF_MSG_STATUS_REQ   = 0x04    // 状态请求
TF_MSG_STATUS_RSP   = 0x05    // 状态响应
TF_MSG_DATA         = 0x06    // 数据传输
TF_MSG_CONFIG       = 0x07    // 配置命令
```

### 5.4 LED 命令 (tf_multinode.h)

```c
LED_CMD_OFF    = 0x00    // LED 关闭
LED_CMD_ON     = 0x01    // LED 开启
LED_CMD_TOGGLE = 0x02    // LED 翻转
```

---

## 6. 扩展开发

### 6.1 添加新消息类型

**步骤 1: tf_multinode.h - 定义消息类型**

```c
typedef enum {
    // ... 现有类型 ...
    TF_MSG_MOTOR_CTRL   = 0x08,   // 新增：电机控制
    TF_MSG_SENSOR_REQ   = 0x09,   // 新增：传感器请求
    TF_MSG_SENSOR_RSP   = 0x0A,   // 新增：传感器响应
} TF_MsgType;

/* 定义数据结构 */
typedef struct {
    uint8_t speed;      // 速度 0-100
    uint8_t direction;  // 方向 0=停, 1=正, 2=反
} TF_MotorCmd;
```

**步骤 2: tf_slave.h - 添加回调类型**

```c
/* 电机控制回调 */
typedef void (*TF_Slave_MotorCallback)(uint8_t speed, uint8_t direction);

/* API */
void TF_Slave_SetMotorCallback(TF_Slave_MotorCallback callback);
```

**步骤 3: tf_slave.c - 实现处理逻辑**

```c
/* 内部变量 */
static TF_Slave_MotorCallback s_motor_callback = NULL;

/* 注册函数 */
void TF_Slave_SetMotorCallback(TF_Slave_MotorCallback callback) {
    s_motor_callback = callback;
}

/* 在 Slave_AddressFilter 的 switch 中添加 case */
case TF_MSG_MOTOR_CTRL:
    usb_printf("[Slave %d] Motor control\r\n", s_slave_addr);
    if (msg->len >= 2 && s_motor_callback != NULL) {
        s_motor_callback(msg->data[0], msg->data[1]);
    }
    if (!is_broadcast) {
        TF_Slave_SendAck(tf, msg);
    }
    break;
```

**步骤 4: tf_master.h - 添加发送函数声明**

```c
bool TF_Master_SendMotorCmd(TinyFrame *tf, uint8_t slave_addr,
                            uint8_t speed, uint8_t direction);
```

**步骤 5: tf_master.c - 实现发送函数**

```c
bool TF_Master_SendMotorCmd(TinyFrame *tf, uint8_t slave_addr,
                            uint8_t speed, uint8_t direction) {
    uint8_t data[2] = {speed, direction};
    return TF_Master_SendTo(tf, slave_addr, TF_MSG_MOTOR_CTRL, data, 2);
}
```

**步骤 6: main.c - 使用新功能**

```c
/* 从机端：注册回调 */
#if !TF_NODE_IS_MASTER
static void Slave_MotorCallback(uint8_t speed, uint8_t direction) {
    usb_printf("[Slave] Motor: speed=%d, dir=%d\r\n", speed, direction);
    // TODO: 控制电机硬件
}

// 在初始化中注册
TF_Slave_SetMotorCallback(Slave_MotorCallback);
#endif

/* 主机端：发送命令 */
#if TF_NODE_IS_MASTER
if (key_check_press(KEY_NAME_ENTER)) {
    TF_Master_SendMotorCmd(&tf, TF_TARGET_SLAVE, 80, 1);  // 速度80, 正转
}
#endif
```

### 6.2 添加从机主动上报

从机可以主动向主机发送数据：

```c
/* 从机端：主动上报传感器数据 */
uint8_t sensor_data[4] = {temp, humidity, pressure_h, pressure_l};
TF_Slave_SendToMaster(&tf, TF_MSG_SENSOR_RSP, sensor_data, 4);
```

```c
/* 主机端：在 Master_GenericListener 中处理 */
case TF_MSG_SENSOR_RSP:
    usb_printf("[Master] Sensor data from Slave %d\r\n", addr);
    // 处理传感器数据
    break;
```

### 6.3 扩展清单

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `tf_multinode.h` | 添加消息类型枚举、数据结构 |
| 2 | `tf_slave.h` | 添加回调类型、注册函数声明 |
| 3 | `tf_slave.c` | 实现回调注册 + switch case |
| 4 | `tf_master.h` | 添加发送函数声明 |
| 5 | `tf_master.c` | 实现发送函数 |
| 6 | `main.c` | 主机调用发送 / 从机注册回调 |

---

## 7. 调试技巧

### 7.1 USB 串口输出

所有模块使用 `usb_printf()` 输出调试信息：

```
[Master] Send to Slave 1, MsgType=0x03
[LoRa] RX 10 bytes, RSSI=-45
[Slave 1] Recv Addr=1, MsgType=0x03, Len=1, Broadcast=0
[Slave 1] LED control command
[Slave] LED ON
[Slave 1] Send ACK
```

### 7.2 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 从机无响应 | 地址不匹配 | 检查 `TF_SLAVE_ADDRESS` 和 `TF_TARGET_SLAVE` |
| 只响应一次 | 发送后未重回接收 | 确认 `TF_WriteImpl` 末尾调用 `e22_demo_receive()` |
| 程序卡死 | 使用了 `printf` | 改为 `usb_printf` |
| LED 闪烁 | 中断中自动点亮 | 注释 `e22_demo.c` 中的 `gpio_led_rx_on()` |
| 收不到数据 | LoRa 参数不一致 | 确保主从 SF/BW/CR/频率一致 |

### 7.3 LoRa 参数确认

主从机必须使用相同的 LoRa 参数：

```c
// e22_demo.c 默认配置
SF:   11
BW:   500 kHz
CR:   4/5
Freq: 915 MHz
Power: 22 dBm
```

---

## 8. 可靠传输实现指南

当前系统消息发出后不跟踪结果，丢失即丢失。本章说明如何逐步实现可靠传输。

### 8.1 现状分析

**已有的基础设施：**

| 机制 | 位置 | 说明 |
|------|------|------|
| CRC16 校验 | `TF_Config.h` | 帧头 + 数据各一次校验，损坏帧直接丢弃 |
| SOF 帧同步 | `TF_Config.h` | 0x01 起始字节，防止对齐错误 |
| ACK/NACK 响应 | `tf_slave.c` | 从机收到单播后回复 ACK，广播不响应 |
| Frame ID 匹配 | `tf_slave.c` | `response.frame_id = msg->frame_id`，响应与请求配对 |
| ID 监听器 + 超时 | `TinyFrame.c` | `TF_QuerySimple()` 注册 ID 监听器，超时触发回调 |
| TX 超时保护 | `TF_Integration.c` | 等待硬件发送完成最多 1000ms |
| TDMA 时间槽 | `tf_multinode.h` | 周期 100ms，每从机 20ms 窗口 |

**当前的问题：**

| 问题 | 说明 |
|------|------|
| `TF_Tick()` 未调用 | ID 监听器超时机制完全无效，`Master_TimeoutHandler` 永远不会被触发 |
| 主机命令无重试 | `TF_Master_SendLedCmd()` 经由 `TF_Master_SendTo()` → `TF_SendSimple()` 发后即忘 |
| 从机上报无确认 | `TF_Slave_ReportEvent/ReportData()` → `TF_SendSimple()` 发后即忘 |
| 无消息去重 | 重传后接收方会重复执行业务逻辑 |

### 8.2 TF_SendSimple 与 TF_QuerySimple 的区别

理解这两个函数的区别是实现可靠传输的前提。

两者最终都调用 `TF_SendFrame()`，差异在于 listener 参数：

```c
// TinyFrame.c:1087-1111

// TF_SendSimple: 不注册监听器，发完即忘
bool TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len)
{
    // ... 填充 msg ...
    return TF_Send(tf, &msg);  // → TF_SendFrame(tf, &msg, NULL, NULL, 0)
}

// TF_QuerySimple: 注册 ID 监听器，等待对方用相同 Frame ID 回复
bool TF_QuerySimple(TinyFrame *tf, TF_TYPE type,
                    const uint8_t *data, TF_LEN len,
                    TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    // ... 填充 msg ...
    return TF_SendFrame(tf, &msg, listener, ftimeout, timeout);
}
```

在 `TF_SendFrame_Begin()` 内部（第 988 行），如果 listener 非空，就调用 `TF_AddIdListener()` 把监听器绑定到这一帧的 Frame ID 上：

```
TF_SendSimple:  发帧 → 结束
TF_QuerySimple: 发帧 → 注册 ID 监听器 → 等对方 TF_Respond() → 触发 listener
                                       → 超时 → 触发 ftimeout
```

**从机侧 `TF_Respond()` 能匹配到的原因：** 从机在 `TF_Slave_SendAck()` 中设置了 `response.frame_id = msg->frame_id` 和 `response.is_response = true`，TinyFrame 内部据此匹配到主机注册的 ID 监听器。

### 8.3 前置条件：启用 TF_Tick()

**这是第一步，不做这一步后面的超时/重试机制全部无效。**

`TF_Tick()` 驱动 TinyFrame 内部的超时计数器，包括：
- ID 监听器超时（`TF_Master_QueryTo` 注册的回调）
- 解析器超时（`TF_PARSER_TIMEOUT_TICKS`，防止收到半帧后状态机卡住）

需要在 SysTick 中断（1ms 周期）中调用：

```c
// Core/Src/stm32f1xx_it.c

extern TinyFrame tf;   // main.c 中定义的 TF 实例

void SysTick_Handler(void)
{
    /* USER CODE BEGIN SysTick_IRQn 0 */
    /* USER CODE END SysTick_IRQn 0 */
    HAL_IncTick();
    /* USER CODE BEGIN SysTick_IRQn 1 */
    TF_Tick(&tf);       // ← 新增：每 1ms 驱动一次 TF 超时
    key_scan();          // 按键扫描（如果已有）
    /* USER CODE END SysTick_IRQn 1 */
}
```

加上这行之后，`TF_MASTER_RESPONSE_TIMEOUT` 的单位就是 ms（1 tick = 1ms），当前值 100 即 100ms 超时。

### 8.4 第一步：主机所有单播命令加重试

**目标：** 把重试逻辑内置到 `TF_Master_SendTo()` 中，所有经过它的单播命令（LED、CONFIG、STATUS_REQ、未来新增的任何命令）自动获得可靠性，上层 API 不需要任何修改。

#### 8.4.1 关键认识：调用链分析

```
TF_Master_SendLedCmd(tf, addr, cmd)      ─┐
TF_Master_SendMotorCmd(tf, addr, ...)     ─┤  所有单播
TF_Master_SendXxx(tf, addr, ...)          ─┤  都汇聚到
    └──→ TF_Master_SendTo(tf, addr, type, data, len)  ← 在这里改
              └──→ TF_SendSimple()   ← 当前：发后即忘
              └──→ TF_QuerySimple()  ← 改成这个：等 ACK + 重试

TF_Master_BroadcastLedCmd(tf, cmd)        ─┐
    └──→ TF_Master_Broadcast()             ─┤  广播无人 ACK
              └──→ TF_SendSimple()          ─┘  保持不变
```

只改 `TF_Master_SendTo()` 一个函数，所有现有和未来的单播命令都自动可靠。上层的 `SendLedCmd`、`SendHeartbeat`、扩展的 `SendMotorCmd` 等无需感知。

#### 8.4.2 设计

```
主机                              从机
  │                                 │
  ├── 任意单播命令 ──────────────→  │  第 1 次
  │          (等 100ms)             ├── ACK ──→ 收到，完成
  │                                 │
  ├── 同一命令重发 ──────────────→  │  第 2 次 (超时重发)
  │          (等 100ms)             ├── ACK ──→ 收到，完成
  │                                 │
  ├── 同一命令重发 ──────────────→  │  第 3 次
  │          (等 100ms)             │  (无响应)
  │                                 │
  └── 放弃，打印错误日志            │
```

#### 8.4.3 修改 tf_master.h — 添加重试配置

```c
/* 重试配置 */
#define TF_MASTER_MAX_RETRIES       3     /* 最大重试次数 (不含首次) */
#define TF_MASTER_RESPONSE_TIMEOUT  100   /* 单次超时 (ticks/ms) */
#define TF_MASTER_MAX_PAYLOAD       32    /* 重试缓存最大负载 */
```

不需要添加新的 API 函数，原有的 `TF_Master_SendLedCmd`、`TF_Master_SendTo` 签名不变。

#### 8.4.4 修改 tf_master.c — 在 SendTo 内部实现重试

```c
/* ---- 重试上下文 (通用，不绑定具体消息类型) ---- */
typedef struct {
    uint8_t     slave_addr;
    TF_MsgType  msg_type;
    uint8_t     payload[TF_MASTER_MAX_PAYLOAD];
    TF_LEN      payload_len;
    uint8_t     retries_left;
} RetryContext;

static RetryContext s_retry_ctx;

/* ACK/NACK 响应监听器 */
static TF_Result Master_AckListener(TinyFrame *tf, TF_Msg *msg)
{
    uint8_t msg_type = TF_GET_MSG(msg->type);
    if (msg_type == TF_MSG_ACK) {
        usb_printf("[Master] ACK received\r\n");
    } else if (msg_type == TF_MSG_NACK) {
        usb_printf("[Master] NACK received\r\n");
    }
    s_retry_ctx.retries_left = 0;
    return TF_CLOSE;
}

/* 超时回调 — 重发或放弃 */
static TF_Result Master_RetryTimeoutHandler(TinyFrame *tf)
{
    if (s_retry_ctx.retries_left > 0) {
        s_retry_ctx.retries_left--;
        usb_printf("[Master] Timeout, retry (%d left)\r\n", s_retry_ctx.retries_left);

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

/* ---- 修改后的 TF_Master_SendTo (替换原实现) ---- */
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

    /* 保存重试上下文 */
    s_retry_ctx.slave_addr   = slave_addr;
    s_retry_ctx.msg_type     = msg_type;
    if (data != NULL && len > 0) {
        memcpy(s_retry_ctx.payload, data, len);
    }
    s_retry_ctx.payload_len  = len;
    s_retry_ctx.retries_left = TF_MASTER_MAX_RETRIES;

    TF_TYPE type = TF_MAKE_TYPE(slave_addr, msg_type);
    usb_printf("[Master] Send to Slave %d, MsgType=0x%02X\r\n", slave_addr, msg_type);

    /* 走 QuerySimple 路径：发送 + 注册 ID 监听器等 ACK */
    return TF_QuerySimple(tf, type, data, len,
                           Master_AckListener, Master_RetryTimeoutHandler,
                           TF_MASTER_RESPONSE_TIMEOUT);
}
```

#### 8.4.5 上层 API 无需修改

由于重试逻辑在 `SendTo` 内部，所有通过它的命令自动具备可靠性：

```c
// 这些调用全部自动获得 ACK 等待 + 重试，无需任何改动
TF_Master_SendLedCmd(&tf, addr, LED_CMD_ON);         // LED 控制
TF_Master_SendTo(&tf, addr, TF_MSG_CONFIG, cfg, 4);  // 配置命令
TF_Master_SendTo(&tf, addr, TF_MSG_MOTOR_CTRL, m, 2); // 未来的电机命令
// ... 任何新增的单播命令
```

广播走 `TF_Master_Broadcast()` 独立路径，不经过 `SendTo`，维持发后即忘。

#### 8.4.6 QueryTo 的处理

原有的 `TF_Master_QueryTo()` 已经走 `TF_QuerySimple` 路径，但它的 listener 是调用方传入的自定义回调（如心跳响应处理），不应被重试逻辑覆盖。保持 `QueryTo` 不变——它适用于需要自定义响应处理的场景（如请求状态数据）。如果 `QueryTo` 也需要重试，由调用方在自己的超时回调中处理。

```
SendTo   → 通用可靠发送 (内置 ACK 等待 + 自动重试)
QueryTo  → 自定义响应处理 (调用方控制超时逻辑)
Broadcast → 发后即忘 (无 ACK)
```

#### 8.4.7 局限性

`s_retry_ctx` 是单例——同一时间只能有一个待确认的单播命令。对于按键触发的场景足够（LoRa 半双工，本身也无法同时收发）。如果需要快速连续发送多条命令，应在上层排队，等前一条 ACK 或超时后再发下一条。

### 8.5 第二步：从机上报加确认

**目标：** 从机 `ReportEvent/ReportData` 发出后等主机 ACK，超时重发。

#### 8.5.1 主机侧 — 收到 DATA 后回 ACK

修改 `tf_master.c` 中 `Master_GenericListener()` 的 `TF_MSG_DATA` 分支：

```c
case TF_MSG_DATA:
    usb_printf("[Master] Data from Slave %d, Len=%d\r\n", addr, msg->len);
    if (s_data_callback != NULL) {
        s_data_callback(addr, msg->data, msg->len);
    }
    /* ---- 新增：回复 ACK ---- */
    {
        TF_Msg response;
        TF_ClearMsg(&response);
        response.frame_id   = msg->frame_id;
        response.is_response = true;
        response.type = TF_MAKE_TYPE(TF_ADDR_MASTER, TF_MSG_ACK);
        response.data = NULL;
        response.len  = 0;
        TF_Respond(tf, &response);
    }
    break;
```

#### 8.5.2 从机侧 — SendToMaster 改用 QuerySimple + 重试

修改 `tf_slave.c`，把 `TF_Slave_SendToMaster()` 从 `TF_SendSimple` 改为 `TF_QuerySimple` 加重试：

```c
#define TF_SLAVE_MAX_RETRIES        2
#define TF_SLAVE_RESPONSE_TIMEOUT   150    /* 稍大于主机处理时间 */

static struct {
    TF_MsgType msg_type;
    uint8_t    payload[32];
    TF_LEN     payload_len;
    uint8_t    retries_left;
} s_slave_retry;

static TF_Result Slave_MasterAckListener(TinyFrame *tf, TF_Msg *msg)
{
    usb_printf("[Slave %d] Master ACK received\r\n", s_slave_addr);
    s_slave_retry.retries_left = 0;
    return TF_CLOSE;
}

static TF_Result Slave_ReportTimeoutHandler(TinyFrame *tf)
{
    if (s_slave_retry.retries_left > 0) {
        s_slave_retry.retries_left--;
        usb_printf("[Slave %d] Report timeout, retry (%d left)\r\n",
                   s_slave_addr, s_slave_retry.retries_left);

        TF_TYPE type = TF_MAKE_TYPE(s_slave_addr, s_slave_retry.msg_type);
        TF_QuerySimple(tf, type,
                        s_slave_retry.payload, s_slave_retry.payload_len,
                        Slave_MasterAckListener, Slave_ReportTimeoutHandler,
                        TF_SLAVE_RESPONSE_TIMEOUT);
    } else {
        usb_printf("[Slave %d] Report failed, master unreachable\r\n", s_slave_addr);
    }
    return TF_CLOSE;
}

bool TF_Slave_SendToMaster(TinyFrame *tf, TF_MsgType msg_type,
                            const uint8_t *data, TF_LEN len)
{
    if (tf == NULL || len > sizeof(s_slave_retry.payload)) return false;

    /* 保存重试上下文 */
    s_slave_retry.msg_type    = msg_type;
    memcpy(s_slave_retry.payload, data, len);
    s_slave_retry.payload_len = len;
    s_slave_retry.retries_left = TF_SLAVE_MAX_RETRIES;

    TF_TYPE type = TF_MAKE_TYPE(s_slave_addr, msg_type);
    usb_printf("[Slave %d] Send to Master, MsgType=0x%02X\r\n", s_slave_addr, msg_type);

    return TF_QuerySimple(tf, type, data, len,
                           Slave_MasterAckListener, Slave_ReportTimeoutHandler,
                           TF_SLAVE_RESPONSE_TIMEOUT);
}
```

### 8.6 第三步：消息去重

重传会导致接收方收到重复帧。例如：主机发了 LED_ON，从机执行并回 ACK，但 ACK 丢失，主机重发 LED_ON，从机收到后又执行一次（虽然结果相同，但如果是 TOGGLE 命令就会出问题）。

#### 8.6.1 去重缓冲区

```c
/* 在 tf_multinode.h 或新头文件中定义 */
#define TF_DEDUP_BUF_SIZE   16

typedef struct {
    TF_ID   ids[TF_DEDUP_BUF_SIZE];
    uint8_t idx;        /* 环形写入位置 */
    uint8_t count;      /* 已记录数量 */
} TF_DedupBuf;

static inline void TF_Dedup_Init(TF_DedupBuf *buf) {
    buf->idx = 0;
    buf->count = 0;
}

/* 检查并记录 Frame ID。返回 true = 重复，false = 新帧 */
static inline bool TF_Dedup_Check(TF_DedupBuf *buf, TF_ID id) {
    /* 搜索已有记录 */
    uint8_t n = (buf->count < TF_DEDUP_BUF_SIZE) ? buf->count : TF_DEDUP_BUF_SIZE;
    for (uint8_t i = 0; i < n; i++) {
        if (buf->ids[i] == id) return true;   /* 重复 */
    }
    /* 记录新 ID */
    buf->ids[buf->idx] = id;
    buf->idx = (buf->idx + 1) % TF_DEDUP_BUF_SIZE;
    if (buf->count < TF_DEDUP_BUF_SIZE) buf->count++;
    return false;   /* 新帧 */
}
```

#### 8.6.2 从机侧集成

在 `tf_slave.c` 的 `Slave_AddressFilter()` 入口处：

```c
static TF_DedupBuf s_dedup;   /* 初始化在 TF_Slave_Init() 中调用 TF_Dedup_Init() */

static TF_Result Slave_AddressFilter(TinyFrame *tf, TF_Msg *msg)
{
    /* 地址过滤 ... (不变) */

    /* ---- 新增：去重检查 ---- */
    if (TF_Dedup_Check(&s_dedup, msg->frame_id)) {
        usb_printf("[Slave %d] Duplicate frame %d, ACK only\r\n",
                   s_slave_addr, msg->frame_id);
        /* 重复帧：只回 ACK，不执行业务逻辑 */
        if (!is_broadcast) {
            TF_Slave_SendAck(tf, msg);
        }
        return TF_STAY;
    }

    /* 正常消息处理 switch ... (不变) */
}
```

#### 8.6.3 主机侧集成

同理在 `Master_GenericListener()` 入口对从机主动上报的 DATA 消息做去重。

### 8.7 实施顺序总结

```
步骤 0: 在 SysTick 中调用 TF_Tick()          ← 前置条件，否则后面全部无效
        ↓
步骤 1: TF_Master_SendTo() 内置重试           ← 改一个函数，所有单播命令自动可靠
        修改: tf_master.h (添加重试配置宏)
              tf_master.c (SendTo 改用 QuerySimple + 重试上下文)
        上层 API (SendLedCmd 等) 无需改动
        ↓
步骤 2: 从机上报改 QuerySimple + 重试         ← 主机和从机两端配合
        修改: tf_master.c (GenericListener 回 ACK)
              tf_slave.c  (SendToMaster 改 QuerySimple)
        ↓
步骤 3: 添加去重缓冲区                        ← 防止重传副作用
        修改: tf_multinode.h (去重结构定义)
              tf_slave.c    (AddressFilter 入口检查)
              tf_master.c   (GenericListener 入口检查)
```

### 8.8 验证方法

| 测试场景 | 方法 | 预期结果 |
|----------|------|----------|
| 正常通信 | 主机发 LED 命令 | 从机执行 + ACK，主机收到 ACK 后不重试 |
| ACK 丢失 | 从机 `TF_Slave_SendAck()` 前加 `return TF_STAY` 模拟丢失 | 主机超时重发，从机去重后只执行一次 |
| 完全无响应 | 关闭从机电源 | 主机重试 3 次后打印 "give up" |
| 从机上报 | 从机按键触发 ReportEvent | 主机收到后回 ACK，从机不重发 |
| 从机上报丢失 | 主机 GenericListener 中 DATA 分支 return 前注释掉 ACK | 从机超时重发，主机去重 |
| TOGGLE 去重 | 主机发 LED_CMD_TOGGLE，模拟 ACK 丢失 | LED 只翻转一次，不因重传翻转两次 |

---

## 附录：完整示例

### main.c 模板

```c
#include "main.h"
#include "usb_uart.h"
#include "TinyFrame.h"
#include "tf_multinode.h"

/* ============ 配置 ============ */
#define TF_NODE_IS_MASTER   0
#define TF_SLAVE_ADDRESS    1
#define TF_TARGET_SLAVE     1

#if TF_NODE_IS_MASTER
#include "tf_master.h"
#else
#include "tf_slave.h"
#endif

TinyFrame tf;

#if !TF_NODE_IS_MASTER
/* 从机回调 */
static void Slave_LedCallback(TF_LedCmd cmd) {
    switch (cmd) {
        case LED_CMD_OFF:    gpio_led_rx_off(); break;
        case LED_CMD_ON:     gpio_led_rx_on();  break;
        case LED_CMD_TOGGLE: /* toggle */       break;
    }
}
#endif

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_TIM2_Init();

    uart_init();
    HAL_Delay(500);
    e22_demo_init();

#if TF_NODE_IS_MASTER
    TF_Master_Init(&tf);
#else
    TF_Slave_Init(&tf, TF_SLAVE_ADDRESS);
    TF_Slave_SetLedCallback(Slave_LedCallback);
#endif

    e22_demo_receive();

    while (1) {
        uart_tx_poll();
        uart_rx_poll();

        uint8_t rx_buf[255];
        uint8_t rx_len;
        int8_t rssi;
        if (e22_demo_check_rx_done(rx_buf, &rx_len, &rssi)) {
            TF_Accept(&tf, rx_buf, rx_len);
        }

#if TF_NODE_IS_MASTER
        if (key_check_press(KEY_NAME_UP)) {
            TF_Master_SendLedCmd(&tf, TF_TARGET_SLAVE, LED_CMD_ON);
        }
        if (key_check_press(KEY_NAME_DOWN)) {
            TF_Master_SendLedCmd(&tf, TF_TARGET_SLAVE, LED_CMD_OFF);
        }
        if (key_check_press(KEY_NAME_ENTER)) {
            TF_Master_SendLedCmd(&tf, TF_TARGET_SLAVE, LED_CMD_TOGGLE);
        }
#endif
    }
}
```
