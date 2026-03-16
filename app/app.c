#include "app.h"

#include <string.h>

#include "app_dht11.h"
#include "app_relay.h"
#include "app_threshold.h"
#include "application.h"
#include "main.h"
#include "nvm.h"
#define LOG_TAG "APP"
#include "log.h"

/* ========================== DWT 微秒延时 ========================== */

static volatile uint8_t s_dwt_available = 0;

void app_dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* verify DWT CYCCNT is actually counting */
    volatile uint32_t start = DWT->CYCCNT;
    __NOP();
    __NOP();
    __NOP();
    s_dwt_available = (DWT->CYCCNT != start) ? 1U : 0U;
}

void app_delay_us(uint32_t us)
{
    if (s_dwt_available)
    {
        uint32_t start = DWT->CYCCNT;
        uint32_t ticks = us * (SystemCoreClock / 1000000U);
        while ((DWT->CYCCNT - start) < ticks)
        {
        }
    }
    else
    {
        volatile uint32_t count = us * 18U;
        while (count-- != 0U)
        {
            __NOP();
        }
    }
}

/* ---- 全局配置，赋默认值 (从机只用 [0]) ---- */
sys_config_t g_sys_config = {
    .threshold = {
        [0] = {
            .temp_high = 350,     /* 35.0°C */
            .temp_low  = 100,     /* 10.0°C */
            .humi_high = 800,     /* 80.0% */
            .humi_low  = 300,     /* 30.0% */
            .soil_dry  = 3000,
            .light_low = 500,
            .co2_high  = 1000,
            .enable    = 0x1F,    /* 全部使能 */
        },
    },
};

/* ---- g_sys_config.lora → user_config ---- */
static void sync_to_user_config(void)
{

}

/* ---- user_config → g_sys_config.lora ---- */
static void sync_from_user_config(void)
{

}

/* ---- NVM 初始化并加载 ---- */
static void nvm_sys_init(void)
{
    uint32_t addrs[2] = {NVM_CONFIG_ADDR_A, NVM_CONFIG_ADDR_B};

    nvm2_init(NVM_CONFIG_PARTITION, addrs, NVM_CONFIG_FLASH_SIZE);

    int32_t ret = nvm_read(NVM_CONFIG_PARTITION,
                           (uint8_t *)&g_sys_config,
                           sizeof(g_sys_config));
    if (ret == NVM_ERR_NO)
    {

    }
    else
    {
        log_warn("NVM: using defaults, saving to flash");
        nvm_write(NVM_CONFIG_PARTITION, (uint8_t *)&g_sys_config, sizeof(g_sys_config));
    }
}

/* ---- 公共接口 ---- */

void app_start(void)
{
    app_dwt_init();
    nvm_sys_init();
    sync_to_user_config();
    app_relay_init();
    app_dht11_init();
    app_display_init();
    app_adc_init();
    app_threshold_init();
}

void app_config_save(void)
{
    sync_from_user_config();

    int32_t ret = nvm_write(NVM_CONFIG_PARTITION,
                            (uint8_t *)&g_sys_config,
                            sizeof(g_sys_config));
    if (ret == NVM_ERR_NO)
    {
        log_info("NVM: config saved");
    }
    else
    {
        log_error("NVM: save failed (ret=%d)", ret);
    }
}
