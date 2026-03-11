/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "adc.h"
#include "gpio.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#define LOG_TAG "App"
#include "TinyFrame.h"
#include "app.h"
#include "app_adc.h"
#include "app_relay.h"
#include "bsp.h"
#include "log.h"
#include "tf_multinode.h"
#include "usb_uart.h"
#define TF_SLAVE_ADDRESS 1 /* 从机地址 (1-14) */

#include "tf_slave.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "dispDirver.h"
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
TinyFrame tf;

static int8_t s_last_rssi = 0;          /* 最近一次接收 RSSI */
static uint16_t s_light_raw = 0;        /* 光照 ADC 缓存 (CH0, PA0) */
static uint16_t s_soil_raw  = 0;        /* 土壤湿度 ADC 缓存 (CH1, PA1) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USB 接收数据回调 */
void serial_on_data_received(uint8_t *buffer, uint16_t len)
{
    byte_queue_write(&uart_tx_queue, buffer, len);
}

/* ========================== 从机回调函数 ========================== */

/**
 * @brief  从机状态查询回调 — 主机轮询时调用，填充当前状态
 */
static void Slave_StatusCallback(TF_StatusData *status)
{
    static uint16_t num = 0;
    status->node_addr = TF_SLAVE_ADDRESS;
    status->error_code = 0;
    status->rssi = s_last_rssi;

    /* 传感器数据 (测试值，接入实际传感器后替换) */
    status->sensor.temperature = 250 + (num % 20);   /* 25.0~26.9 °C */
    status->sensor.humidity = 550 + (num % 30);      /* 55.0~57.9 % */
    status->sensor.illuminance = 1000 + num;         /* lux */
    status->sensor.co2 = 400 + (num % 100);          /* ppm */
    status->sensor.soil_moisture = 450 + (num % 25); /* 45.0~47.4 % */
    num++;

    /* 控制器状态 */
    status->ctrl.fan = 0;
    status->ctrl.heater = 0;
    status->ctrl.pump = 0;
    status->ctrl.led = 0;
}

/**
 * @brief  从机 CONFIG 控制回调 — 主机发送设备控制命令时调用
 *
 * payload 格式: [设备ID, 动作值]
 * 当前为测试阶段，仅输出日志，后续替换为实际 GPIO 控制。
 *
 * @param  dev_id: 设备 ID (TF_CTRL_DEV_FAN / HEATER / PUMP)
 * @param  action: 动作值 (0=关, 1=开)
 */
static void Slave_ConfigCallback(uint8_t dev_id, uint8_t action)
{
    log_info("Config: dev=0x%02X, act=%d", dev_id, action);
    /* TODO: 确定继电器引脚后取消注释 */
    app_relay_set(dev_id, action);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C2_Init();
    MX_SPI1_Init();
    MX_USB_DEVICE_Init();
    MX_TIM2_Init();
    MX_ADC1_Init();
    //  MX_USART1_UART_Init();
    /* USER CODE BEGIN 2 */
    HAL_TIM_Base_Start_IT(&htim2); /* 启动 TIM2 1ms 定时中断 */
    uart_init();                   // 初始化USB串口
    bsp_InitUart();
    /* 等待 USB 枚举完成 */
    HAL_Delay(5000);

    /* 设置日志级别 */
    log_set_level(LOG_DEBUG);

    /* 初始化 NVM 并加载配置 */
    app_start();

    /* 初始化 LoRa 模组 */
    e22_demo_init();

    /* 初始化 TinyFrame */
    log_info("=== SLAVE MODE (addr=%d) ===", TF_SLAVE_ADDRESS);
    TF_Slave_Init(&tf, TF_SLAVE_ADDRESS);
    TF_Slave_SetStatusCallback(Slave_StatusCallback);
    TF_Slave_SetConfigCallback(Slave_ConfigCallback);
    log_info("Press ENTER to send key event to Master");

    /* 进入 LoRa 接收模式 */
    e22_demo_receive();
    // Disp_Init();
    // OLED_ClearBuffer();
    // OLED_DrawStr(10, 10, "const char ");
    // OLED_SendBuffer();
    log_info("System Ready!");
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* 0. TF_Tick 超时驱动 — 在主循环中调用，避免中断上下文阻塞 */
        {
            static uint32_t last_tick = 0;
            uint32_t now = HAL_GetTick();
            while (last_tick < now)
            {
                TF_Tick(&tf);
                last_tick++;
            }
        }

        /* 1. USB 轮询 (暂时注释，排查卡死问题) */
        uart_tx_poll();
        uart_rx_poll();

        /* 2. LoRa 接收处理 */
        uint8_t rx_buf[255];
        uint8_t rx_len;
        int8_t rssi;
        if (e22_demo_check_rx_done(rx_buf, &rx_len, &rssi))
        {
            s_last_rssi = rssi;
            log_debug("RX %d bytes, RSSI=%d", rx_len, rssi);
            TF_Accept(&tf, rx_buf, rx_len);
        }

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_USB;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
