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

#include "gpio.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "TinyFrame.h"
#include "tf_multinode.h"
#include "usb_uart.h"

/* 根据角色包含对应模块 */
#define TF_NODE_IS_MASTER 1 /* 1=主机, 0=从机 */

#define TF_SLAVE_ADDRESS 1 /* 从机地址 (1-14) */

#define TF_TARGET_SLAVE_0 1 /* 主机发送目标从机地址 */
#define TF_TARGET_SLAVE_1 2 /* 主机发送目标从机地址 */

#if TF_NODE_IS_MASTER
#include "tf_master.h"
#else
#include "tf_slave.h"
#endif
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

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
static bool s_led_state = false;
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

#if TF_NODE_IS_MASTER
/* ========================== 主机回调函数 ========================== */

/**
 * @brief  主机收到从机数据的回调
 */
static void Master_DataCallback(uint8_t slave_addr, const uint8_t *data, TF_LEN len)
{
    usb_printf("[Master] Slave %d pressed key! Data=0x%02X\r\n", slave_addr, data[0]);

    /* 翻转 LED */
    s_led_state = !s_led_state;
    if (s_led_state) {
        gpio_led_rx_on();
    } else {
        gpio_led_rx_off();
    }
    usb_printf("[Master] LED -> %d\r\n", s_led_state);
}

#else
/* ========================== 从机回调函数 ========================== */

/**
 * @brief  从机 LED 控制回调
 */
static void Slave_LedCallback(TF_LedCmd led_cmd)
{
    switch (led_cmd)
    {
        case LED_CMD_OFF:
            s_led_state = false;
            gpio_led_rx_off();
            usb_printf("[Slave] LED OFF\r\n");
            break;
        case LED_CMD_ON:
            s_led_state = true;
            gpio_led_rx_on();
            usb_printf("[Slave] LED ON\r\n");
            break;
        case LED_CMD_TOGGLE:
            s_led_state = !s_led_state;
            if (s_led_state)
            {
                gpio_led_rx_on();
            }
            else
            {
                gpio_led_rx_off();
            }
            usb_printf("[Slave] LED TOGGLE -> %d\r\n", s_led_state);
            break;
        default:
            usb_printf("[Slave] Unknown LED cmd: %d\r\n", led_cmd);
            break;
    }
}

#endif /* !TF_NODE_IS_MASTER */

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
    // MX_USB_DEVICE_Init();  // 由uart_init内部调用
    MX_TIM2_Init();
    /* USER CODE BEGIN 2 */
    HAL_TIM_Base_Start_IT(&htim2);  /* 启动 TIM2 1ms 定时中断 */
    uart_init();  // 初始化USB串口

    /* 等待 USB 枚举完成 */
    HAL_Delay(500);

    /* 初始化 LoRa 模组 */
    e22_demo_init();

    /* 初始化 TinyFrame */
#if TF_NODE_IS_MASTER
    usb_printf("\r\n=== MASTER MODE ===\r\n");
    TF_Master_Init(&tf);
    TF_Master_SetDataCallback(Master_DataCallback);  /* 设置数据回调 */
    usb_printf("UP=Slave1, DOWN=Slave2, ENTER=Broadcast\r\n");
#else
    usb_printf("\r\n=== SLAVE MODE (addr=%d) ===\r\n", TF_SLAVE_ADDRESS);
    TF_Slave_Init(&tf, TF_SLAVE_ADDRESS);
    TF_Slave_SetLedCallback(Slave_LedCallback);
    usb_printf("Press ENTER to send key event to Master\r\n");
#endif

    /* 进入 LoRa 接收模式 */
    e22_demo_receive();

    usb_printf("System Ready!\r\n");
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* 0. TF_Tick 超时驱动 — 在主循环中调用，避免中断上下文阻塞 */
        {
            static uint32_t last_tick = 0;
            uint32_t now = HAL_GetTick();
            while (last_tick < now) {
                TF_Tick(&tf);
                last_tick++;
            }
        }

        /* 1. USB 轮询 */
        uart_tx_poll();
        uart_rx_poll();

        /* 2. LoRa 接收处理 */
        uint8_t rx_buf[255];
        uint8_t rx_len;
        int8_t rssi;
        if (e22_demo_check_rx_done(rx_buf, &rx_len, &rssi))
        {
            usb_printf("[LoRa] RX %d bytes, RSSI=%d\r\n", rx_len, rssi);
            TF_Accept(&tf, rx_buf, rx_len);
        }

#if TF_NODE_IS_MASTER
        /* 3. 主机: 按键检测发送命令 */
        if (key_check_press(KEY_NAME_UP))
        {
            usb_printf("[Master] Key UP -> LED ON\r\n");
            TF_Master_SendLedCmd(&tf, TF_TARGET_SLAVE_0, LED_CMD_TOGGLE);
        }
        if (key_check_press(KEY_NAME_DOWN))
        {
            usb_printf("[Master] Key DOWN -> LED OFF\r\n");
            TF_Master_SendLedCmd(&tf, TF_TARGET_SLAVE_1, LED_CMD_TOGGLE);
        }
        if (key_check_press(KEY_NAME_ENTER))
        {
            usb_printf("[Master] Key ENTER -> LED TOGGLE\r\n");
            TF_Master_BroadcastLedCmd(&tf, LED_CMD_TOGGLE);
        }
#else
        /* 3. 从机: 按键检测，发送数据给主机 */
        if (key_check_press(KEY_NAME_ENTER))
        {
            uint8_t key_event = 0x01;  /* 按键事件代码 */
            usb_printf("[Slave %d] Key ENTER -> Send to Master\r\n", TF_SLAVE_ADDRESS);
            TF_Slave_ReportEvent(&tf, key_event);
        }
#endif

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
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
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
