# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 STM32F103C8T6 的 LoRa 无线智慧农业监控系统（**从机端**），使用 E22-900M22S (SX126X) 模组进行 LoRa 通信，通过 TinyFrame 帧协议实现多节点主从通信。配合 SSD1306 OLED 显示屏、三按键输入和 USB CDC 虚拟串口调试输出。

**从机**功能：
- 采集本地传感器数据（DHT11 温湿度、ADC 光照/土壤湿度、UART CO2）
- 响应主机轮询，上报传感器数据和设备状态
- 接收主机下发的设备控制命令（风扇、加热器、水泵、LED）
- 接收主机下发的阈值配置更新
- 本地按键控制设备 + 阈值编辑 UI
- 阈值自动控制 + 蜂鸣器报警（含 CO2 检测）
- OLED 显示传感器数据（上3下2 网格布局 + 底部状态行）

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
app/               - 应用层模块
├── app.c/h            - 系统初始化 (DWT, NVM, 各模块init)
├── app_display.c/h    - OLED 显示 (上3下2网格 + 自动关屏)
├── app_threshold.c/h  - 阈值监控 + 继电器自动控制 (含CO2)
├── app_threshold_ui.c/h - 阈值编辑 UI (OLED + 按键)
├── app_devctrl_ui.c/h - 设备手动控制 UI
├── app_relay.c/h      - 继电器控制
├── app_dht11.c/h      - DHT11 温湿度传感器
├── app_adc.c/h        - ADC 采集 (光照/土壤)
├── app_co2.c/h        - CO2 传感器 (UART 串口通信)
user/              - 用户工具库
├── usb_uart.c/h   - USB CDC 串口抽象层 (基于 byte_queue 缓冲, 受 USB_DEBUG_ENABLE 控制)
├── log/log.c/h    - rxi/log 日志库 (受 USB_DEBUG_ENABLE 控制)
├── byte_queue.c/h - 环形字节队列实现
├── crc16.c/h      - CRC16 校验算法
├── heap_4.c/h     - 内存分配器
bsp/               - 板级支持
├── config.h       - NVM 地址、threshold_config_t、sys_config_t 定义
├── nvm.c/h        - Flash NVM 存储 (双页 ping-pong)
TinyFrame/         - TinyFrame 帧协议库 (v2.3.0)
├── TF_Config.h    - 帧配置 (ID 1B, LEN 2B, TYPE 1B, CRC16)
├── TF_Integration.c - TF_WriteImpl 通过 LoRa 发送
├── ex/
│   ├── tf_multinode.h - 多节点协议定义 (TYPE = ADDR[7:4] + MSG[3:0])
│   ├── tf_master.h/c  - 主机端模块
│   └── tf_slave.h/c   - 从机端模块
Middlewares/
├── MultMenu/      - OLED 菜单系统
│   ├── disp/      - 显示驱动封装 (dispDirver.h: OLED_SetPowerSave 等)
├── u8g2Lib/       - u8g2 图形库
├── ST/STM32_USB_Device_Library/ - USB CDC 类库
```

### 核心模块

**TinyFrame 从机通信层 (`TinyFrame/`)**
- TYPE 字段拆分: 高 4 位为地址 (0x0=广播, 0x1-0xE=从机), 低 4 位为消息类型
- 消息类型: ACK, NACK, HEARTBEAT, LED_CTRL, STATUS_REQ/RSP, DATA, CONFIG, THRESHOLD
- `TF_Integration.c`: `TF_WriteImpl()` 内部调用 `e22_demo_transmit()` 通过 LoRa 发送帧
- `tf_slave.h/c`: 从机端 API — `TF_Slave_Init()`, 通过回调响应主机请求
- 从机地址: `TF_SLAVE_ADDRESS` 定义在 `main.c` (当前值 = 1)

**从机回调函数 (main.c 中定义):**
- `Slave_StatusCallback()`: 填充 `TF_StatusData` 结构体响应主机轮询
- `Slave_ConfigCallback()`: 接收设备控制命令 (dev_id + action)
- `Slave_ThresholdCallback()`: 接收阈值字段更新 (field_id + value)

**LoRa 通信层 (`Core/Src/e22_*.c`)**
- `e22_hal.c`: SX126X HAL 实现 (SPI 读写、GPIO 控制、RF 开关切换)
- `e22_demo.c`: LoRa 初始化、发送、接收、DIO1 中断处理
- LoRa 参数: SF11, BW500, CR4/5, 915MHz, 22dBm, DCDC, TCXO 3.3V
- 关键函数: `e22_demo_init()`, `e22_demo_transmit()`, `e22_demo_receive()`, `e22_demo_check_rx_done()`

**USB 串口抽象层 (`user/usb_uart.c`)**
- `uart_init()`: 初始化 byte_queue 收发缓冲区，`USB_DEBUG_ENABLE` 时调用 `MX_USB_DEVICE_Init()`
- `uart_tx_poll()` / `uart_rx_poll()`: `USB_DEBUG_ENABLE=0` 时为空操作
- 缓冲区大小: TX/RX 各 512 字节

**按键驱动 (`Core/Src/key.c`)**
- 三按键: KEY_UP (PB4), KEY_DOWN (PB9), KEY_ENTER (PB7)
- 1ms 定时器中断扫描，支持短按、长按和连按模式
- 函数: `key_check_press()`, `key_check_long_press()`, `key_set_continue()`, `key_any_activity()`
- `key_any_activity()`: 非破坏性活动检测，用于 OLED 自动关屏/唤醒

**OLED 显示模块 (`app/app_display.c`)**
- 上3下2 网格布局: 上排(温度/湿度/光照) + 下排(土壤湿度/CO2) + 底部设备状态行
- 自动关屏: 30 秒无按键操作后调用 `OLED_SetPowerSave(1)` 进入 SSD1306 省电模式
- 按键唤醒: `key_any_activity()` 检测到按键后 `app_display_activity()` 点亮屏幕
- 关屏时第一次按键仅唤醒屏幕，不执行功能操作
- 关键函数: `app_display_init()`, `app_display_sensor()`, `app_display_activity()`, `app_display_timeout_check()`, `app_display_is_off()`
- 注意: 从机 `app_display_sensor()` 比主机多一个 `co2` 参数

**阈值监控系统 (`app/app_threshold.c`)**
- 实时监控传感器数据，自动控制继电器设备
- 支持温度、湿度、土壤湿度、光照、CO2 阈值检测
- 回差控制防止震荡（温度±2.0°C，土壤/光照±200，CO2±50ppm）
- 手动控制超时机制（5分钟后恢复自动控制）
- 阈值触发时蜂鸣器报警（响铃5秒自动关闭）
- 触发条件：
  - 温度过高（>35°C）→ 开风扇 + 蜂鸣器
  - 温度过低（<10°C）→ 开加热器 + 蜂鸣器
  - CO2过高（>1000ppm）→ 开风扇 + 蜂鸣器
  - 土壤干燥 → 开水泵 + 蜂鸣器
  - 光照不足 → 开LED + 蜂鸣器
- 风扇 OR 逻辑：温度或 CO2 任一触发即开启

**CO2 传感器模块 (`app/app_co2.c`)**
- 通过 UART 串口读取 CO2 浓度 (ppm)
- `app_co2_poll()`: 主循环中调用，解析串口数据
- `app_co2_get()`: 获取当前 CO2 值

### 硬件引脚映射 (定义在 `Core/Inc/main.h`)
```
SPI1: E22 模组通信
  - PA4: SPI_CS, PA5/PA6/PA7: SCK/MISO/MOSI
E22 控制:
  - PB0: E22_RESET, PB1: E22_BUSY
  - PA3: E22_DIO1 (EXTI3 中断)
  - PB12/PB13: E22_TXEN/E22_RXEN (RF 开关)
I2C2: OLED (PB10/PB11)
USB: CDC 虚拟串口 (PA11/PA12), PB5: USB_CTRL
按键: PB4(UP), PB7(ENTER), PB9(DOWN)
LED: PA15(TX), PB6(RX)
蜂鸣器: PB3 (GPIO 直接控制)
继电器: 风扇/水泵/加热器/LED (通过 app_relay 模块控制)
  - LED 初始状态: 开启（其他继电器初始关闭）
ADC: PA0(光照, CH0), PA1(土壤湿度, CH1)
USART1: CO2 传感器 / BSP 串口
```

### 主循环流程
```c
main() {
    // 外设初始化
    HAL_Init(); SystemClock_Config();
    MX_GPIO/I2C2/SPI1/USB/TIM2/ADC1_Init();
    uart_init();  bsp_InitUart();
    HAL_Delay(2000);  // USB 枚举等待 (USB_DEBUG_ENABLE 时)
    app_start();      // NVM + ADC + Relay + DHT11 + CO2 + OLED + Threshold

    // LoRa + TinyFrame 初始化
    e22_demo_init();
    TF_Slave_Init(&tf, TF_SLAVE_ADDRESS);  // 从机地址 = 1
    TF_Slave_SetStatusCallback(...);
    TF_Slave_SetConfigCallback(...);
    TF_Slave_SetThresholdCallback(...);
    e22_demo_receive();

    while(1) {
        TF_Tick();                        // 0. 超时驱动
        uart_tx/rx_poll();                // 1. USB 轮询
        app_co2_poll();                   // 1.5 CO2 串口解析
        app_dht11_poll();                 // DHT11 轮询
        LoRa RX → TF_Accept();           // 2. LoRa 接收
        key_any_activity() → OLED wake;   // 3. 按键唤醒 + 超时检查
        app_threshold_ui_poll();          // 3.5 阈值编辑 UI
        app_devctrl_ui_poll();            // 3.6 设备控制 UI
        ADC + OLED refresh (500ms);       // 4. 传感器采集 + 显示 + 阈值检查
        __WFI();                          // 低功耗等待中断
    }
}
```

## 编译开关

### USB 调试输出开关 (`USB_DEBUG_ENABLE`)

在 `user/log/log.h` 中定义，默认值为 `1`（启用）。

关闭方法（二选一）：
1. **Keil 工程选项**: C/C++ → Preprocessor Symbols → Define 中添加 `USB_DEBUG_ENABLE=0`
2. **直接修改**: 将 `log.h` 中 `#define USB_DEBUG_ENABLE 1` 改为 `0`

关闭后的效果：
- 所有 `log_trace/debug/info/warn/error/fatal` 宏变为 `((void)0)`，不产生代码
- `uart_init()` 跳过 `MX_USB_DEVICE_Init()` 调用
- `uart_tx_poll()` / `uart_rx_poll()` 变为空函数
- `HAL_Delay(2000)` USB 枚举等待被跳过
- 节省 Flash（去除字符串常量）和 CPU（去除格式化开销）

### 从机地址配置

在 `Core/Src/main.c` 中:
```c
#define TF_SLAVE_ADDRESS 1  /* 从机地址 (1-14) */
```

## 低功耗机制

- **WFI 睡眠**: 主循环末尾 `__WFI()` 等待中断，SysTick/UART/DIO1 唤醒
- **OLED 自动关屏**: 30 秒无按键操作后 `OLED_SetPowerSave(1)` 进入 SSD1306 省电模式
- **按键唤醒**: `key_any_activity()` 检测通过消抖的按键事件，唤醒 OLED
- 关屏期间传感器采集、LoRa 通信、阈值检测等功能正常运行
- 关屏状态下第一次按键仅唤醒屏幕，不执行 UI 操作

## 开发注意事项

- STM32CubeMX 配置文件: `project.ioc`
- 用户代码需写在 `USER CODE BEGIN/END` 注释块内以避免被 CubeMX 覆盖
- SX126X 驱动需要实现 HAL 层 (`sx126x_hal.h` 中声明的函数)
- OLED 使用硬件 I2C (Fast Mode), 地址 0x78
- NVM 存储: Flash 最后两个 1KB 页面 (0x0800F800 / 0x0800FC00), 双页 ping-pong

### TinyFrame 配置要求

- 通信双方必须使用相同的 `TF_Config.h` 配置
- 当前配置: ID 1字节, LEN 2字节, TYPE 1字节, CRC16 校验
- `TF_WriteImpl()` 通过 LoRa 发送，发送前等待上次完成 (超时 1000ms)
- `TF_Error()` 宏重定向到 `usb_printf()` 输出调试信息

### 消息可靠性机制

**基础机制：**
- CRC16 双校验（帧头 + 数据），校验失败帧被丢弃
- SOF 字节 (0x01) 帧同步
- Frame ID 匹配请求与响应
- TX 超时保护 1000ms，防止发送卡死
- TDMA 时间槽防冲突：周期 100ms，每从机 20ms 窗口
- `TF_Tick()` 在主循环中逐 ms 调用，驱动所有超时计数

**从机 → 主机（可靠上报）：**
- `TF_Slave_SendToMaster()` 内置 ACK 等待 + 超时自动重试（最多 2 次，超时 150ms）
- ReportEvent / ReportData 经过 SendToMaster，自动获得可靠性

**消息去重：**
- 环形 ID 缓冲区（16 条记录），重复帧只回 ACK 不执行业务逻辑
- `Slave_AddressFilter()` 地址匹配后、业务处理前做去重
