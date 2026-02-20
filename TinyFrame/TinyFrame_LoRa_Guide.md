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
- 可靠传输：单播命令自动重试 + ACK 确认 + 消息去重

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

**主机 → 从机（可靠单播，带重试 + 去重）：**

```
主机                                    从机
┌──────────────┐                      ┌──────────────┐
│ 按键检测      │                      │              │
│     ↓        │                      │              │
│ TF_Master_   │    LoRa 无线         │ TF_Accept()  │
│ SendLedCmd() │ ─────────────────→   │     ↓        │
│     ↓        │                      │ 地址过滤     │
│ SendTo()     │                      │     ↓        │
│     ↓        │                      │ 去重检查     │
│ QuerySimple()│                      │     ↓        │
│  + 注册ID    │                      │ LedCallback()│
│    监听器    │                      │     ↓        │
│     ↓        │                      │ LED 控制     │
│ e22_transmit │                      │     ↓        │
│     ↓        │    LoRa 无线         │ SendAck()    │
│ AckListener  │ ←─────────────────   │              │
│ 收到ACK,完成 │                      │              │
│              │                      │              │
│ (超时则自动  │                      │              │
│  重发,最多3次)│                      │              │
└──────────────┘                      └──────────────┘
```

**从机 → 主机（可靠上报，带确认 + 去重）：**

```
从机                                    主机
┌──────────────┐                      ┌──────────────┐
│ 按键/传感器   │                      │              │
│     ↓        │                      │              │
│ ReportEvent()│    LoRa 无线         │ TF_Accept()  │
│     ↓        │ ─────────────────→   │     ↓        │
│ SendToMaster │                      │ Generic      │
│     ↓        │                      │ Listener     │
│ QuerySimple()│                      │     ↓        │
│  + 注册ID    │                      │ 去重检查     │
│    监听器    │                      │     ↓        │
│              │    LoRa 无线         │ DataCallback │
│ MasterAck    │ ←─────────────────   │     ↓        │
│ Listener     │                      │ TF_Respond   │
│ 收到ACK,完成 │                      │ (回复 ACK)   │
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

### 4.3 超时与重试配置

**主机端（`tf_master.h`）：**

```c
#define TF_MASTER_RESPONSE_TIMEOUT  100   // 等待从机 ACK 超时 (ms)
#define TF_MASTER_MAX_RETRIES       3     // 最大重试次数 (不含首次)
#define TF_MASTER_MAX_PAYLOAD       32    // 重试缓存最大负载 (字节)
```

**从机端（`tf_slave.h`）：**

```c
#define TF_SLAVE_RESPONSE_TIMEOUT   150   // 等待主机 ACK 超时 (ms)
#define TF_SLAVE_MAX_RETRIES        2     // 最大重试次数 (不含首次)
#define TF_SLAVE_MAX_PAYLOAD        32    // 重试缓存最大负载 (字节)
```

**去重（`tf_multinode.h`）：**

```c
#define TF_DEDUP_BUF_SIZE           16    // 去重缓冲区大小 (记录最近 16 个 Frame ID)
```

**发送层（`TF_Integration.c`）：**

```c
#define TF_TX_TIMEOUT_MS            1000  // LoRa 硬件发送超时 (ms)
```

---

## 5. API 参考

### 5.1 主机 API (tf_master.h)

| 函数 | 说明 | 可靠性 |
|------|------|--------|
| `TF_Master_Init(tf)` | 初始化主机（含去重缓冲区） | — |
| `TF_Master_SendTo(tf, addr, type, data, len)` | 可靠发送到指定从机 | ACK + 重试 |
| `TF_Master_QueryTo(tf, addr, type, data, len, cb)` | 查询发送，等待自定义响应 | 超时由调用方处理 |
| `TF_Master_Broadcast(tf, type, data, len)` | 广播到所有从机 | 发后即忘 |
| `TF_Master_SendLedCmd(tf, addr, cmd)` | LED 控制（经 SendTo） | ACK + 重试 |
| `TF_Master_BroadcastLedCmd(tf, cmd)` | 广播 LED（经 Broadcast） | 发后即忘 |
| `TF_Master_SendHeartbeat(tf, addr, cb)` | 心跳查询（经 QueryTo） | 超时由调用方处理 |
| `TF_Master_SetDataCallback(cb)` | 注册从机上报数据回调 | — |

### 5.2 从机 API (tf_slave.h)

| 函数 | 说明 | 可靠性 |
|------|------|--------|
| `TF_Slave_Init(tf, addr)` | 初始化从机（含去重缓冲区） | — |
| `TF_Slave_SetLedCallback(cb)` | 注册 LED 控制回调 | — |
| `TF_Slave_SetStatusCallback(cb)` | 注册状态查询回调 | — |
| `TF_Slave_SendAck(tf, msg)` | 回复 ACK（响应主机命令） | — |
| `TF_Slave_SendNack(tf, msg)` | 回复 NACK | — |
| `TF_Slave_SendToMaster(tf, type, data, len)` | 可靠上报数据到主机 | ACK + 重试 |
| `TF_Slave_ReportEvent(tf, code)` | 上报事件（经 SendToMaster） | ACK + 重试 |
| `TF_Slave_ReportData(tf, data, len)` | 上报数据（经 SendToMaster） | ACK + 重试 |
| `TF_Slave_SendInSlot(tf, tick, type, data, len)` | 在时间槽内上报（经 SendToMaster） | ACK + 重试 |

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

## 8. 可靠传输机制

本系统已实现完整的消息可靠传输：自动重试、应答确认、消息去重。

### 8.1 机制总览

| 机制 | 位置 | 说明 |
|------|------|------|
| CRC16 校验 | `TF_Config.h` | 帧头 + 数据各一次校验，损坏帧直接丢弃 |
| SOF 帧同步 | `TF_Config.h` | 0x01 起始字节，防止对齐错误 |
| ACK/NACK 响应 | `tf_slave.c` | 从机收到单播后回复 ACK，广播不响应 |
| Frame ID 匹配 | `tf_slave.c` | `response.frame_id = msg->frame_id`，响应与请求配对 |
| TF_Tick 超时驱动 | `stm32f1xx_it.c` | TIM2 1ms 中断中调用 `TF_Tick(&tf)`，驱动所有超时计数 |
| 主机自动重试 | `tf_master.c` | `SendTo()` 内置 ACK 等待 + 超时重发，最多 3 次 |
| 从机上报确认 | `tf_slave.c` + `tf_master.c` | 从机 `SendToMaster()` 等主机 ACK，主机收到 DATA 后回 ACK |
| 消息去重 | `tf_multinode.h` | 环形 ID 缓冲区，重复帧只回 ACK 不执行业务逻辑 |
| TX 超时保护 | `TF_Integration.c` | 等待硬件发送完成最多 1000ms |
| TDMA 时间槽 | `tf_multinode.h` | 周期 100ms，每从机 20ms 窗口 |

### 8.2 三种发送路径

主机端提供三种发送路径，可靠性策略各不相同：

```
SendTo()    — 可靠单播：等待 ACK + 超时自动重试 (最多 TF_MASTER_MAX_RETRIES 次)
QueryTo()   — 查询单播：等待自定义响应，超时由调用方处理，不自动重试
Broadcast() — 广播：发后即忘，从机不回复
```

调用链：

```
SendLedCmd()      → SendTo()    → TF_QuerySimple() + 重试  (可靠)
SendHeartbeat()   → QueryTo()   → TF_QuerySimple()          (调用方控制)
BroadcastLedCmd() → Broadcast() → TF_SendSimple()           (发后即忘)
```

所有经过 `SendTo()` 的命令自动获得可靠性，上层 API 无需感知。

### 8.3 TF_SendSimple 与 TF_QuerySimple 的区别

两者最终都调用 `TF_SendFrame()`，差异在于 listener 参数：

```c
// TF_SendSimple: 不注册监听器，发完即忘
TF_SendFrame(tf, &msg, NULL, NULL, 0);

// TF_QuerySimple: 注册 ID 监听器，等待对方用相同 Frame ID 回复
TF_SendFrame(tf, &msg, listener, ftimeout, timeout);
```

在 `TF_SendFrame_Begin()` 内部，如果 listener 非空，调用 `TF_AddIdListener()` 绑定监听器：

```
TF_SendSimple:  发帧 → 结束
TF_QuerySimple: 发帧 → 注册 ID 监听器 → 等对方 TF_Respond() → 触发 listener
                                       → 超时 (由 TF_Tick 驱动) → 触发 ftimeout
```

TinyFrame 消息派发优先级：**ID 监听器 > Type 监听器 > Generic 监听器**。因此 `SendTo` 注册的 `Master_AckListener`（ID 监听器）会优先捕获 ACK，不会传到 `Master_GenericListener`。

### 8.4 TF_Tick 超时驱动

`TF_Tick()` 在 TIM2 1ms 周期中断中调用，驱动 TinyFrame 内部所有超时：

```c
// Core/Src/stm32f1xx_it.c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        TF_Tick(&tf);   // 每 1ms 驱动一次超时
    }
}
```

因此所有 timeout 配置的单位为 ms（1 tick = 1ms）。

### 8.5 主机命令重试机制

`TF_Master_SendTo()` 内部实现：

1. 将命令参数（地址、类型、负载）保存到静态 `RetryContext`
2. 通过 `TF_QuerySimple()` 发送并注册 ID 监听器
3. 从机回复 ACK → `Master_AckListener` 触发 → 返回 `TF_CLOSE`，完成
4. 超时无响应 → `Master_RetryTimeoutHandler` 触发 → 从 `RetryContext` 取参数重发
5. 重试耗尽 → 打印 `"All retries exhausted"`，放弃

```
主机                              从机
  │                                 │
  ├── 任意单播命令 ──────────────→  │  第 1 次
  │          (等 100ms)             ├── ACK ──→ 收到，完成
  │                                 │
  ├── 重发 ──────────────────────→  │  第 2 次 (超时)
  │          (等 100ms)             │  ...
  ├── 重发 ──────────────────────→  │  第 3 次
  │          (等 100ms)             │  ...
  ├── 重发 ──────────────────────→  │  第 4 次 (最后一次)
  │          (等 100ms)             │
  └── "All retries exhausted"      │
```

**配置参数（`tf_master.h`）：**

```c
#define TF_MASTER_RESPONSE_TIMEOUT  100   // 单次超时 100ms
#define TF_MASTER_MAX_RETRIES       3     // 最多重试 3 次 (不含首次)
#define TF_MASTER_MAX_PAYLOAD       32    // 重试缓存最大负载
```

**局限性：** `RetryContext` 是单例，同一时间只能有一个待确认的 `SendTo` 命令。LoRa 半双工特性决定了单例足够。

### 8.6 从机上报确认机制

从机主动上报（`ReportEvent`/`ReportData`）经过 `TF_Slave_SendToMaster()` 发送，机制与主机侧对称：

**从机侧（`tf_slave.c`）：**
- `SendToMaster()` 通过 `TF_QuerySimple()` 发送并等待主机 ACK
- 超时后 `Slave_ReportTimeoutHandler` 自动重发
- 收到 ACK 后 `Slave_MasterAckListener` 停止重试

**主机侧（`tf_master.c`）：**
- `Master_GenericListener` 收到 `TF_MSG_DATA` 后回复 ACK（`TF_Respond`）
- 先做去重检查，重复帧只回 ACK 不调用用户回调

**配置参数（`tf_slave.h`）：**

```c
#define TF_SLAVE_RESPONSE_TIMEOUT   150   // 单次超时 150ms (稍大于主机处理时间)
#define TF_SLAVE_MAX_RETRIES        2     // 最多重试 2 次
#define TF_SLAVE_MAX_PAYLOAD        32    // 重试缓存最大负载
```

### 8.7 消息去重机制

重传会导致接收方收到重复帧。例如：主机发 `LED_CMD_TOGGLE`，从机执行并回 ACK，但 ACK 丢失，主机重发，没有去重的话 LED 会翻转两次。

**去重缓冲区（`tf_multinode.h`）：**

```c
#define TF_DEDUP_BUF_SIZE   16

typedef struct {
    TF_ID   ids[TF_DEDUP_BUF_SIZE];  // 环形 Frame ID 数组
    uint8_t write_idx;                 // 写入位置
    uint8_t count;                     // 已记录数量
} TF_DedupBuf;

// 检查并记录：返回 true = 重复帧, false = 新帧
bool TF_Dedup_IsDuplicate(TF_DedupBuf *buf, TF_ID id);
```

**从机侧集成（`tf_slave.c`）：**
- `Slave_AddressFilter()` 地址匹配后、`switch` 之前做去重检查
- 重复帧：只回 ACK，不执行业务逻辑（不调用 `LedCallback` 等）
- 新帧：正常处理 + 回 ACK

**主机侧集成（`tf_master.c`）：**
- `Master_GenericListener()` 的 `TF_MSG_DATA` 分支做去重检查
- 重复帧：只回 ACK，不调用 `s_data_callback`
- 新帧：调用用户回调 + 回 ACK

去重缓冲区在 `TF_Master_Init()` 和 `TF_Slave_Init()` 中初始化。

### 8.8 验证方法

| 测试场景 | 方法 | 预期结果 |
|----------|------|----------|
| 正常通信 | 主机发 LED 命令 | 从机执行 + ACK，主机收到 ACK 后不重试 |
| ACK 丢失 | 从机 `TF_Slave_SendAck()` 前加 `return TF_STAY` 模拟丢失 | 主机超时重发，从机去重后只执行一次 |
| 完全无响应 | 关闭从机电源 | 主机重试 3 次后打印 "All retries exhausted" |
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
