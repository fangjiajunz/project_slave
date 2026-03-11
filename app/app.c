#include "app.h"

#include <string.h>

#include "app_relay.h"
#include "application.h"
#include "nvm.h"
#define LOG_TAG "APP"
#include "log.h"

/* ---- 全局配置，赋默认值 ---- */
sys_config_t g_sys_config = {
    .lora = {
        .slave_addr = LORA_DEFAULT_SLAVE_ADDR,
        .frequency_mhz = LORA_DEFAULT_FREQUENCY_MHZ,
        .tx_power = LORA_DEFAULT_TX_POWER,
        .lora_sf = LORA_DEFAULT_SF,
        .lora_bw = LORA_DEFAULT_BW,
    },
};

/* ---- g_sys_config.lora → user_config ---- */
static void sync_to_user_config(void)
{
    user_config.lora_sf = g_sys_config.lora.lora_sf;
    user_config.lora_bw = g_sys_config.lora.lora_bw;
    user_config.frequency_mhz = g_sys_config.lora.frequency_mhz;
    user_config.tx_power = g_sys_config.lora.tx_power;
}

/* ---- user_config → g_sys_config.lora ---- */
static void sync_from_user_config(void)
{
    g_sys_config.lora.lora_sf = user_config.lora_sf;
    g_sys_config.lora.lora_bw = user_config.lora_bw;
    g_sys_config.lora.frequency_mhz = user_config.frequency_mhz;
    g_sys_config.lora.tx_power = user_config.tx_power;
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
        log_info("NVM: config loaded (addr=%d, freq=%d, pwr=%d, sf=%d, bw=%d)",
                 g_sys_config.lora.slave_addr,
                 g_sys_config.lora.frequency_mhz,
                 g_sys_config.lora.tx_power,
                 g_sys_config.lora.lora_sf,
                 g_sys_config.lora.lora_bw);
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
    nvm_sys_init();
    sync_to_user_config();
    app_relay_init();
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
