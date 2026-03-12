#include "app_dht11.h"

#include "driver_dht11.h"
#include "driver_dht11_interface.h"
#include "main.h"

#define LOG_TAG "DHT11"
#include "log.h"

#define APP_DHT11_READ_INTERVAL_MS 1200U
#define APP_DHT11_DEFAULT_TEMP_X10 250
#define APP_DHT11_DEFAULT_HUMI_X10 550

static dht11_handle_t s_dht11_handle;
static int16_t s_temp_x10 = APP_DHT11_DEFAULT_TEMP_X10;
static uint16_t s_humi_x10 = APP_DHT11_DEFAULT_HUMI_X10;
static uint32_t s_last_read_tick = 0;
static bool s_inited = false;
static bool s_ready = false;

static int16_t app_dht11_convert_temp_x10(float temp_c)
{
    if (temp_c >= 0.0f)
    {
        return (int16_t)(temp_c * 10.0f + 0.5f);
    }

    return (int16_t)(temp_c * 10.0f - 0.5f);
}

static void app_dht11_link_interface(void)
{
    DRIVER_DHT11_LINK_INIT(&s_dht11_handle, dht11_handle_t);
    DRIVER_DHT11_LINK_BUS_INIT(&s_dht11_handle, dht11_interface_init);
    DRIVER_DHT11_LINK_BUS_DEINIT(&s_dht11_handle, dht11_interface_deinit);
    DRIVER_DHT11_LINK_BUS_READ(&s_dht11_handle, dht11_interface_read);
    DRIVER_DHT11_LINK_BUS_WRITE(&s_dht11_handle, dht11_interface_write);
    DRIVER_DHT11_LINK_DELAY_MS(&s_dht11_handle, dht11_interface_delay_ms);
    DRIVER_DHT11_LINK_DELAY_US(&s_dht11_handle, dht11_interface_delay_us);
    DRIVER_DHT11_LINK_ENABLE_IRQ(&s_dht11_handle, dht11_interface_enable_irq);
    DRIVER_DHT11_LINK_DISABLE_IRQ(&s_dht11_handle, dht11_interface_disable_irq);
    DRIVER_DHT11_LINK_DEBUG_PRINT(&s_dht11_handle, dht11_interface_debug_print);
}

static void app_dht11_try_init(void)
{
    uint8_t res;

    app_dht11_link_interface();

    res = dht11_init(&s_dht11_handle);
    if (res == 0)
    {
        s_inited = true;
        s_ready = true;
        s_last_read_tick = HAL_GetTick() - APP_DHT11_READ_INTERVAL_MS;
        log_info("DHT11 init ok");
    }
    else
    {
        s_inited = false;
        s_ready = false;
        log_error("DHT11 init failed: %u", res);
    }
}

void app_dht11_init(void)
{
    app_dht11_try_init();
}

void app_dht11_poll(void)
{
    uint32_t now;
    uint8_t humidity_s;
    uint16_t temperature_raw;
    uint16_t humidity_raw;
    float temperature_s;

    now = HAL_GetTick();

    if (!s_inited)
    {
        if ((now - s_last_read_tick) >= APP_DHT11_READ_INTERVAL_MS)
        {
            s_last_read_tick = now;
            app_dht11_try_init();
        }
        return;
    }

    if ((now - s_last_read_tick) < APP_DHT11_READ_INTERVAL_MS)
    {
        return;
    }

    s_last_read_tick = now;

    if (dht11_read_temperature_humidity(&s_dht11_handle,
                                        &temperature_raw,
                                        &temperature_s,
                                        &humidity_raw,
                                        &humidity_s) == 0)
    {
        s_temp_x10 = app_dht11_convert_temp_x10(temperature_s);
        s_humi_x10 = (uint16_t)(((humidity_raw >> 8) & 0xFFU) * 10U + (humidity_raw & 0xFFU));
        s_ready = true;
//        log_debug("DHT11: T=%d.%d C, H=%d.%d %%",
//                  s_temp_x10 / 10, s_temp_x10 % 10,
//                  s_humi_x10 / 10, s_humi_x10 % 10);
    }
    else
    {
        s_ready = false;
        log_warn("DHT11 read failed");
    }
}

bool app_dht11_get(int16_t *temp_x10, uint16_t *humi_x10)
{
    if (temp_x10 != NULL)
    {
        *temp_x10 = s_temp_x10;
    }
    if (humi_x10 != NULL)
    {
        *humi_x10 = s_humi_x10;
    }

    return s_ready;
}

bool app_dht11_is_ready(void)
{
    return s_ready;
}
