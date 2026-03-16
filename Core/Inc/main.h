/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdio.h>

#include "application.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
    typedef enum
    {
        KEY_NAME_UP = 0,
        KEY_NAME_DOWN,
        KEY_NAME_ENTER,
    } key_name_t;

    typedef struct
    {
        bool is_tx;
        bool is_rx;
        uint8_t rx_buffer[255];
        uint8_t rx_length;
        int8_t rx_rssi;
    } context_e22_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define E22_DIO1_Pin GPIO_PIN_3
#define E22_DIO1_GPIO_Port GPIOA
#define E22_DIO1_EXTI_IRQn EXTI3_IRQn
#define SPI_CS_Pin GPIO_PIN_4
#define SPI_CS_GPIO_Port GPIOA
#define E22_RESET_Pin GPIO_PIN_0
#define E22_RESET_GPIO_Port GPIOB
#define E22_BUSY_Pin GPIO_PIN_1
#define E22_BUSY_GPIO_Port GPIOB
#define E22_TXEN_Pin GPIO_PIN_12
#define E22_TXEN_GPIO_Port GPIOB
#define E22_RXEN_Pin GPIO_PIN_13
#define E22_RXEN_GPIO_Port GPIOB
#define LED_TX_Pin GPIO_PIN_15
#define LED_TX_GPIO_Port GPIOA
#define BUZZER_PWM_Pin GPIO_PIN_3
#define BUZZER_PWM_GPIO_Port GPIOB
#define KEY_UP_Pin GPIO_PIN_4
#define KEY_UP_GPIO_Port GPIOB
#define USB_CTRL_Pin GPIO_PIN_5
#define USB_CTRL_GPIO_Port GPIOB
#define LED_RX_Pin GPIO_PIN_6
#define LED_RX_GPIO_Port GPIOB
#define KEY_ENTER_Pin GPIO_PIN_7
#define KEY_ENTER_GPIO_Port GPIOB
#define KEY_DOWN_Pin GPIO_PIN_9
#define KEY_DOWN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
    void usb_printf(const char *format, ...);

    void gpio_usb_ctrl_on(void);
    void gpio_usb_ctrl_off(void);
    void gpio_led_tx_on(void);
    void gpio_led_tx_off(void);
    void gpio_led_rx_on(void);
    void gpio_led_rx_off(void);
    void buzzer_on(void);
    void buzzer_off(void);
    void buzzer_button_push(void);

    bool key_check_press(key_name_t name);
    bool key_check_long_press(key_name_t name);
    uint32_t key_get_hold_time(key_name_t name);
    void key_set_continue(key_name_t name, bool enable);
    void key_timer_1ms_interrupt_callback(void);

    void e22_demo_init(void);
    void e22_demo_menu_config(menu_config_t *config);
    void e22_demo_transmit(uint8_t *buffer, uint8_t length);
    void e22_demo_receive(void);
    void e22_demo_dio1_interrupt_callback(void);
    bool e22_demo_check_rx_done(uint8_t *buffer, uint8_t *length, int8_t *rssi);
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
