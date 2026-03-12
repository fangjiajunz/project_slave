/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      driver_dht11_interface_template.c
 * @brief     driver dht11 interface template source file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-03-12
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/03/12  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/11/19  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_dht11_interface.h"

#include <stdarg.h>
#include <stdio.h>

#include "app.h"
#include "main.h"

#define DHT11_GPIO_PORT GPIOB
#define DHT11_GPIO_PIN GPIO_PIN_8
#define DHT11_CPU_FREQ_MHZ 72U
#define DHT11_GPIO_CRH_SHIFT 0U
#define DHT11_GPIO_MODE_INPUT_PULLUP 0x8U
#define DHT11_GPIO_MODE_OUTPUT_OD_50M 0x7U

static void dht11_gpio_set_input(void)
{
    uint32_t reg;

    reg = DHT11_GPIO_PORT->CRH;
    reg &= ~(0xFU << DHT11_GPIO_CRH_SHIFT);
    reg |= (DHT11_GPIO_MODE_INPUT_PULLUP << DHT11_GPIO_CRH_SHIFT);
    DHT11_GPIO_PORT->CRH = reg;
    DHT11_GPIO_PORT->BSRR = DHT11_GPIO_PIN;
}

static void dht11_gpio_set_output(void)
{
    uint32_t reg;

    reg = DHT11_GPIO_PORT->CRH;
    reg &= ~(0xFU << DHT11_GPIO_CRH_SHIFT);
    reg |= (DHT11_GPIO_MODE_OUTPUT_OD_50M << DHT11_GPIO_CRH_SHIFT);
    DHT11_GPIO_PORT->CRH = reg;
}

uint8_t dht11_interface_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    dht11_gpio_set_input();

    return 0;
}

/**
 * @brief  interface bus deinit
 * @return status code
 *         - 0 success
 *         - 1 bus deinit failed
 * @note   none
 */
uint8_t dht11_interface_deinit(void)
{
    dht11_gpio_set_input();

    return 0;
}

/**
 * @brief      interface bus read
 * @param[out] *value pointer to a value buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t dht11_interface_read(uint8_t *value)
{
    *value = (uint8_t)((DHT11_GPIO_PORT->IDR & DHT11_GPIO_PIN) ? 1U : 0U);

    return 0;
}

/**
 * @brief     interface bus write
 * @param[in] value written value
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t dht11_interface_write(uint8_t value)
{
    if (value == 0)
    {
        dht11_gpio_set_output();
        DHT11_GPIO_PORT->BRR = DHT11_GPIO_PIN;
    }
    else
    {
        dht11_gpio_set_input();
    }

    return 0;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void dht11_interface_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief     interface delay us (hardware timer based, accurate)
 * @param[in] us time
 * @note      falls back to NOP loop if DWT is unavailable
 */
void dht11_interface_delay_us(uint32_t us)
{
    app_delay_us(us);
}

/**
 * @brief interface enable the interrupt
 * @note  none
 */
void dht11_interface_enable_irq(void)
{
    __enable_irq();
}

/**
 * @brief interface disable the interrupt
 * @note  none
 */
void dht11_interface_disable_irq(void)
{
    __disable_irq();
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void dht11_interface_debug_print(const char *const fmt, ...)
{
    char buffer[128];
    va_list args;

    va_start(args, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    usb_printf("%s", buffer);
}
