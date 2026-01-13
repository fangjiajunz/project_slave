# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 STM32F103C8T6 的 LoRa 无线通信测试项目，使用 E22-900M22S (SX126X) 模组进行 LoRa 通信，配合 SSD1306 OLED 显示屏和三按键菜单系统。

## 构建命令

使用 Keil MDK-ARM V5.32 进行编译:
- 项目文件: `MDK-ARM/project.uvprojx`
- 命令行构建 (需要配置 Keil 路径): `"C:\Keil_v5\UV4\UV4.exe" -b MDK-ARM\project.uvprojx -o build.log`

## 代码架构

### 目录结构
```
Core/
├── Src/           - 应用层源码 (main.c, e22_hal.c, e22_demo.c, key.c, u8g2_hal.c)
├── Inc/           - 应用层头文件
Drivers/
├── sx126x_driver/ - SX126X LoRa 芯片驱动 (Semtech 官方驱动)
├── STM32F1xx_HAL_Driver/ - STM32 HAL 库
├── CMSIS/         - ARM CMSIS 库
Middlewares/
├── MultMenu/      - OLED 菜单系统
│   ├── menu/      - 菜单核心逻辑
│   ├── application/ - 菜单回调与业务逻辑
│   ├── disp/      - 显示驱动封装
├── u8g2Lib/       - u8g2 图形库
├── ST/STM32_USB_Device_Library/ - USB CDC 类库
TinyFrame/         - 串口帧协议库 (TinyFrame v2.3.0)
```

### 核心模块

**LoRa 通信层 (`Core/Src/e22_*.c`)**
- `e22_hal.c`: SX126X HAL 实现 (SPI 读写、GPIO 控制、RF 开关切换)
- `e22_demo.c`: LoRa 演示应用 (初始化、发送、接收、中断处理)
- 关键函数: `e22_demo_init()`, `e22_demo_transmit()`, `e22_demo_receive()`, `e22_demo_dio1_interrupt_callback()`

**菜单系统 (`Middlewares/MultMenu/`)**
- `menu.c`: 菜单状态机与动画渲染 (PID 光标动画)
- `application.c`: 菜单回调实现 (LoRa 参数配置、Tx/Rx 测试模式)
- 菜单入口: `Menu_Init()`, `Menu_Task()`
- 配置结构体: `menu_config_t` (定义在 `application.h`)

**按键驱动 (`Core/Src/key.c`)**
- 三按键: UP, DOWN, ENTER
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
按键: PB4(UP), PB7(ENTER), PB9(DOWN)
LED: PA15(TX), PB6(RX)
蜂鸣器: PB3 (TIM2 CH2 PWM)
```

### 主循环流程
```c
main() {
    HAL_Init();
    SystemClock_Config();  // 72MHz HSE + PLL
    MX_*_Init();           // 外设初始化
    e22_demo_init();       // LoRa 模组初始化
    Menu_Init();           // 菜单系统初始化
    while(1) {
        Menu_Task();       // 菜单任务循环
    }
}
```

## 开发注意事项

- STM32CubeMX 配置文件: `project.ioc`
- 用户代码需写在 `USER CODE BEGIN/END` 注释块内以避免被 CubeMX 覆盖
- SX126X 驱动需要实现 HAL 层 (`sx126x_hal.h` 中声明的函数)
- LoRa 默认参数: SF11, BW500, CR4/5, 915MHz, 22dBm
- OLED 使用硬件 I2C (Fast Mode), 地址 0x78
