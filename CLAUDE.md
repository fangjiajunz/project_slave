# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 STM32F103C8T6 的 LoRa 无线主从通信系统，使用 E22-900M22S (SX126X) 模组进行 LoRa 通信，通过 TinyFrame 帧协议实现多节点主从通信。配合 SSD1306 OLED 显示屏、三按键输入和 USB CDC 虚拟串口调试输出。

通过编译宏 `TF_NODE_IS_MASTER` 切换主机/从机模式：
- **主机**: 通过按键向指定从机或广播发送 LED 控制命令，接收从机上报的事件/数据
- **从机**: 接收主机命令控制 LED，按键触发事件上报给主机，支持时间槽防冲突

## 构建命令

使用 Keil MDK-ARM V5.32 进行编译:
- 项目文件: `MDK-ARM/project.uvprojx`
- 命令行构建 (需要配置 Keil 路径): `"C:\Keil_v5\UV4\UV4.exe" -b MDK-ARM\project.uvprojx -o build.log`

## 代码架构

### 目录结构
```
Core/
├── Src/           - 应用层源码 (main.c, e22_hal.c, e22_demo.c, key.c, gpio.c, u8g2_hal.c)
├── Inc/           - 应用层头文件 (main.h 中定义 context_e22_t, key_name_t)
Drivers/
├── sx126x_driver/ - SX126X LoRa 芯片驱动 (Semtech 官方驱动)
├── STM32F1xx_HAL_Driver/ - STM32 HAL 库
├── CMSIS/         - ARM CMSIS 库
user/              - 用户工具库
├── usb_uart.c/h   - USB CDC 串口抽象层 (基于 byte_queue 缓冲)
├── byte_queue.c/h - 环形字节队列实现
├── crc16.c/h      - CRC16 校验算法
├── heap_4.c/h     - 内存分配器
TinyFrame/         - TinyFrame 帧协议库 (v2.3.0)
├── TF_Config.h    - 帧配置 (ID 1B, LEN 2B, TYPE 1B, CRC16)
├── TF_Integration.c - TF_WriteImpl 通过 LoRa 发送
├── ex/
│   ├── tf_multinode.h - 多节点协议定义 (TYPE = ADDR[7:4] + MSG[3:0])
│   ├── tf_master.h/c  - 主机端模块
│   └── tf_slave.h/c   - 从机端模块
Middlewares/
├── MultMenu/      - OLED 菜单系统
│   ├── menu/      - 菜单核心逻辑
│   ├── application/ - 菜单回调、AirPlane 小游戏、DinoGame 小游戏
│   ├── disp/      - 显示驱动封装
├── u8g2Lib/       - u8g2 图形库
├── ST/STM32_USB_Device_Library/ - USB CDC 类库
```

### 核心模块

**TinyFrame 多节点通信层 (`TinyFrame/`)**
- TYPE 字段拆分: 高 4 位为地址 (0x0=广播, 0x1-0xE=从机), 低 4 位为消息类型
- 消息类型: ACK, NACK, HEARTBEAT, LED_CTRL, STATUS_REQ/RSP, DATA, CONFIG
- `TF_Integration.c`: `TF_WriteImpl()` 内部调用 `e22_demo_transmit()` 通过 LoRa 发送帧，发送完成后自动恢复接收模式
- `tf_master.h/c`: 主机端 API — `TF_Master_Init()`, `TF_Master_SendLedCmd()`, `TF_Master_BroadcastLedCmd()`, `TF_Master_SendHeartbeat()`
- `tf_slave.h/c`: 从机端 API — `TF_Slave_Init()`, `TF_Slave_ReportEvent()`, `TF_Slave_ReportData()`, `TF_Slave_SendInSlot()` (时间槽防冲突)

**LoRa 通信层 (`Core/Src/e22_*.c`)**
- `e22_hal.c`: SX126X HAL 实现 (SPI 读写、GPIO 控制、RF 开关切换)
- `e22_demo.c`: LoRa 初始化、发送、接收、DIO1 中断处理
- `context_e22_t` 结构体定义在 `Core/Inc/main.h`，包含 `is_tx`, `is_rx`, `rx_buffer[255]`, `rx_length`, `rx_rssi`
- 关键函数: `e22_demo_init()`, `e22_demo_transmit()`, `e22_demo_receive()`, `e22_demo_check_rx_done()`, `e22_demo_dio1_interrupt_callback()`

**USB 串口抽象层 (`user/usb_uart.c`)**
- `uart_init()`: 初始化 byte_queue 收发缓冲区并调用 `MX_USB_DEVICE_Init()`
- `uart_tx_poll()`: 从 `uart_tx_queue` 读取数据，通过 `CDC_Transmit_FS()` 发送到 USB 主机
- `uart_rx_poll()`: 从 `uart_rx_queue` 读取数据，调用 `serial_on_data_received()` 回调
- `usb_uart_rx_handler()`: USB CDC 接收中断中调用，将数据写入 `uart_rx_queue`
- 缓冲区大小: TX/RX 各 512 字节

**按键驱动 (`Core/Src/key.c`)**
- 三按键: KEY_UP (PB4), KEY_DOWN (PB9), KEY_ENTER (PB7)
- 1ms 定时器中断扫描，支持短按和连按模式
- 函数: `key_check_press()`, `key_set_continue()`

### 硬件引脚映射 (定义在 `Core/Inc/main.h`)
```
SPI1: E22 模组通信
  - PA4: SPI_CS
  - PA5/PA6/PA7: SCK/MISO/MOSI
E22 控制:
  - PB0: E22_RESET
  - PB1: E22_BUSY
  - PA3: E22_DIO1 (EXTI3 中断)
  - PB12/PB13: E22_TXEN/E22_RXEN (RF 开关)
I2C2: OLED (PB10/PB11)
USB: CDC 虚拟串口 (PA11/PA12)
  - PB5: USB_CTRL (USB 控制引脚)
按键: PB4(UP), PB7(ENTER), PB9(DOWN)
LED: PA15(TX), PB6(RX)
蜂鸣器: PB3 (TIM2 CH2 PWM)
```

### 主循环流程
```c
main() {
    HAL_Init();
    SystemClock_Config();  // 72MHz HSE + PLL
    MX_GPIO/I2C2/SPI1/TIM2_Init();
    uart_init();           // USB CDC 初始化 (内部调用 MX_USB_DEVICE_Init)
    HAL_Delay(500);        // 等待 USB 枚举
    e22_demo_init();       // LoRa 模组初始化

#if TF_NODE_IS_MASTER
    TF_Master_Init(&tf);
    TF_Master_SetDataCallback(Master_DataCallback);
#else
    TF_Slave_Init(&tf, TF_SLAVE_ADDRESS);
    TF_Slave_SetLedCallback(Slave_LedCallback);
#endif

    e22_demo_receive();    // 进入 LoRa 接收模式

    while(1) {
        uart_tx_poll();    // USB 发送轮询
        uart_rx_poll();    // USB 接收轮询

        // LoRa 接收处理 → TF_Accept() 解析帧
        if (e22_demo_check_rx_done(...)) {
            TF_Accept(&tf, rx_buf, rx_len);
        }

#if TF_NODE_IS_MASTER
        // 按键 → 发送 LED 命令给从机
        // UP → Slave1, DOWN → Slave2, ENTER → 广播
#else
        // 按键 ENTER → TF_Slave_ReportEvent() 上报主机
#endif
    }
}
```

## 开发注意事项

- STM32CubeMX 配置文件: `project.ioc`
- 用户代码需写在 `USER CODE BEGIN/END` 注释块内以避免被 CubeMX 覆盖
- SX126X 驱动需要实现 HAL 层 (`sx126x_hal.h` 中声明的函数)
- LoRa 默认参数: SF11, BW500, CR4/5, 915MHz, 22dBm
- OLED 使用硬件 I2C (Fast Mode), 地址 0x78

### 主从模式编译切换

在 `Core/Src/main.c` 中通过宏控制编译模式:
```c
#define TF_NODE_IS_MASTER 1  /* 1=主机, 0=从机 */
#define TF_SLAVE_ADDRESS  2  /* 从机地址 (1-14), 仅从机模式有效 */
```

### TinyFrame 配置要求

- 通信双方必须使用相同的 `TF_Config.h` 配置
- 当前配置: ID 1字节, LEN 2字节, TYPE 1字节, CRC16 校验
- `TF_WriteImpl()` 通过 LoRa 发送，发送前等待上次完成 (超时 1000ms)
- `TF_Error()` 宏重定向到 `usb_printf()` 输出调试信息

### 消息可靠性机制

**已实现的机制：**
- CRC16 双校验（帧头 + 数据），校验失败帧被丢弃（`TF_Config.h` 配置 `TF_CKSUM_CRC16`）
- SOF 字节 (0x01) 帧同步（`TF_Config.h`）
- 从机对单播消息回复 ACK/NACK，广播消息不响应（`tf_slave.c` `Slave_AddressFilter()`）
- `TF_Master_QueryTo()` 支持带超时的请求-响应模式（超时 100 ticks，`tf_master.h`）
- Frame ID 匹配请求与响应（`tf_slave.c` 中 `response.frame_id = msg->frame_id`）
- TX 超时保护 1000ms，防止发送卡死（`TF_Integration.c`）
- TDMA 时间槽防冲突：周期 100ms，每从机 20ms 窗口（`tf_multinode.h`）

**当前不足：**
- 无自动重传：超时/NACK 后只打印日志，不重发（`tf_master.c` `Master_TimeoutHandler()`）
- LED 命令使用 `TF_SendSimple` 发后即忘，不监听 ACK（`tf_master.c` `TF_Master_SendLedCmd()`）
- 从机主动上报（`TF_Slave_ReportEvent/ReportData`）无确认机制
- 无消息去重/乱序检测
