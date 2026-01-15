#include <string.h>

#include "application.h"
#include "main.h"
#include "sx126x.h"
#include "sx126x_hal.h"

/**
 * 模组基本参数定义
 */
// typedef struct
// {
// 	bool    is_tx;
// 	bool    is_rx;
// 	uint8_t rx_buffer[255];
// 	uint8_t rx_length;
// 	int8_t  rx_rssi;
// }context_e22_t;

/**
 * 模组默认配置
 */
context_e22_t context_e22 = {
    .is_tx = false,
    .is_rx = false,
    .rx_length = 0,
};

/**
 * 模组PA默认配置
 */
static sx126x_pa_cfg_params_t pa_cfg = {
    .pa_duty_cycle = 0x04,
    .hp_max = 0x07,
    .device_sel = 0x00,
    .pa_lut = 0x01,
};

/**
 * 模组LoRa调制默认配置
 */
static sx126x_mod_params_lora_t mod_cfg = {
    .sf = SX126X_LORA_SF11,    //!< LoRa Spreading Factor
    .bw = SX126X_LORA_BW_500,  //!< LoRa Bandwidth
    .cr = SX126X_LORA_CR_4_5,  //!< LoRa Coding Rate
    .ldro = true,              //!< Low DataRate Optimization configuration
};

/**
 * 模组数据包结构默认配置
 */
static sx126x_pkt_params_lora_t pkt_cfg = {
    .preamble_len_in_symb = 8,                //!< Preamble length in symbols
    .header_type = SX126X_LORA_PKT_EXPLICIT,  //!< Header type
    .pld_len_in_bytes = 255,                  //!< Payload length in bytes
    .crc_is_on = true,                        //!< CRC activation
    .invert_iq_is_on = false,                 //!< IQ polarity setup
};

/**
 * @brief  (可选的)模组SPI读写测试
 *
 * @note 如果失败了，请检查电气链接。也可以先短接SPI的 MISO MOSI，先测试自发自收是否正确
 */
static void e22_spi_check(void)
{
    uint8_t temp = 0xA5;

    /* 写入一个标记
     0x06BB 寄存器为可读可写类型
     SX126X_REG_RXTX_PAYLOAD_LEN  */
    sx126x_write_register(&context_e22, 0x06BB, &temp, 1);

    /* 读回 */
    temp = 0;
    sx126x_read_register(&context_e22, 0x06BB, &temp, 1);

    /* 比较 */
    if (0xA5 != temp)
    {
        /* 一般是SPI通信异常 请检查接线 */
        while (1);
    }
}

/**
 * @brief  (可选的)模组晶振启动测试
 */
static void e22_xosc_check(void)
{
    sx126x_errors_mask_t error_info;

    sx126x_clear_device_errors(&context_e22);

    sx126x_set_standby(&context_e22, SX126X_STANDBY_CFG_XOSC);

    sx126x_get_device_errors(&context_e22, &error_info);

    if (error_info & SX126X_ERRORS_XOSC_START)
    {
        /* 晶振问题请检查模组型号或与销售联系
         不要搞混无源、有源晶振模组 */
        while (1);
    }
}

/**
 * @brief  模组初始化
 */
void e22_demo_init(void)
{
    /* 硬件复位 */
    sx126x_reset(&context_e22);

    /* 唤醒 */
    sx126x_wakeup(&context_e22);

    /* 切换工作状态 */
    sx126x_set_standby(&context_e22, SX126X_STANDBY_CFG_RC);

    /* (可选)基础SPI检查 */
    e22_spi_check();

    /* 保留寄存器设置 (减少唤醒恢复时间) */
    sx126x_init_retention_list(&context_e22);

    /* 内部电源模式 (DCDC功耗更小)*/
    sx126x_set_reg_mode(&context_e22, SX126X_REG_MODE_DCDC);

    /* 禁止DIO2切换射频开关 (该评估板硬件没有将E22模组的DIO2与RXEN连接)	*/
    sx126x_set_dio2_as_rf_sw_ctrl(&context_e22, false);

    /* 开启晶振 E22系列为有源温补晶振(TCXO) */
    sx126x_set_dio3_as_tcxo_ctrl(&context_e22, SX126X_TCXO_CTRL_3_3V, 320);

    /* 修正内部状态 */
    sx126x_cal(&context_e22, SX126X_CAL_ALL);

    /* (可选)基础晶振检查 */
    e22_xosc_check();

    /* 数据包类型 */
    sx126x_set_pkt_type(&context_e22, SX126X_PKT_TYPE_LORA);

    /* 载波频率 (915000000Hz = 915000KHz = 915MHz)*/
    sx126x_set_rf_freq(&context_e22, 915000000);

    /* 内部PA参数      (请参考sx126x Datasheet中的 13.1.14 SetPaConfig)*/
    sx126x_set_pa_cfg(&context_e22, &pa_cfg);

    /* 发射功率 22dBm  (请参考sx126x Datasheet中的 13.4.4 SetTxParams)*/
    sx126x_set_tx_params(&context_e22, 22, SX126X_RAMP_40_US);

    /* 完成TX/RX后的工作状态 */
    sx126x_set_rx_tx_fallback_mode(&context_e22, SX126X_FALLBACK_STDBY_RC);

    /* 关闭增强接收 (开启会增加接收灵敏度，但功耗会增加) */
    sx126x_cfg_rx_boosted(&context_e22, false);

    /* LORA调制参数 (与空中速率、接收灵敏度有关联)
     可以使用计算器	https://www.semtech.com/design-support/lora-calculator */
    sx126x_set_lora_mod_params(&context_e22, &mod_cfg);

    /* LORA数据包格式 */
    sx126x_set_lora_pkt_params(&context_e22, &pkt_cfg);

    /* LORA同步字 */
    sx126x_set_lora_sync_word(&context_e22, 0x14);
}

/**
 * @brief 使用显示菜单用户配置参数重新配置模组
 *
 * @param config 菜单配置参数信息
 */
void e22_demo_menu_config(menu_config_t *config)
{
    /* 切换工作状态 */
    sx126x_set_standby(&context_e22, SX126X_STANDBY_CFG_RC);

    /* 载波频率 HZ */
    sx126x_set_rf_freq(&context_e22, config->frequency_mhz * 1000000);

    /* 发射功率        (请参考sx126x Datasheet中的 13.4.4 SetTxParams)*/
    sx126x_set_tx_params(&context_e22, (int8_t)(config->tx_power & 0xFF), SX126X_RAMP_40_US);

    /* LORA调制参数 (与空中速率、接收灵敏度有关联)
     可以使用计算器	https://www.semtech.com/design-support/lora-calculator */
    mod_cfg.sf = (sx126x_lora_sf_t)config->lora_sf;
    switch (config->lora_bw)
    {
        case 500:
            mod_cfg.bw = SX126X_LORA_BW_500;
            break;
        case 250:
            mod_cfg.bw = SX126X_LORA_BW_250;
            break;
        case 125:
            mod_cfg.bw = SX126X_LORA_BW_125;
            break;
        default:
            /* 需要修改 增加 */
            while (1);
    }
    switch (config->lora_cr)
    {
        case 5:
            mod_cfg.cr = SX126X_LORA_CR_4_5;
            break;
        case 6:
            mod_cfg.cr = SX126X_LORA_CR_4_6;
            break;
        case 7:
            mod_cfg.cr = SX126X_LORA_CR_4_7;
            break;
        case 8:
            mod_cfg.cr = SX126X_LORA_CR_4_8;
            break;
        default:
            /* 不支持 */
            while (1);
    }

    sx126x_set_lora_mod_params(&context_e22, &mod_cfg);
}

/**
 * @brief 向模组写入数据并开始发射
 *
 * @param buffer 指向数据缓存
 * @param length 写入长度
 */
void e22_demo_transmit(uint8_t *buffer, uint8_t length)
{
    /* 发送长度 */
    pkt_cfg.pld_len_in_bytes = length;
    sx126x_set_lora_pkt_params(&context_e22, &pkt_cfg);

    /* 写入内部缓存 */
    sx126x_write_buffer(&context_e22, 0x00, buffer, length);

    /* 允许发送完成中断，并映射到引脚DIO1上 */
    sx126x_set_dio_irq_params(&context_e22,
                              SX126X_IRQ_TX_DONE, /* irq_mask */
                              SX126X_IRQ_TX_DONE, /* dio1_mask */
                              SX126X_IRQ_NONE,    /* dio2_mask */
                              SX126X_IRQ_NONE);   /* dio3_mask */

    /* 清中断状态 */
    sx126x_clear_irq_status(&context_e22, SX126X_IRQ_ALL);

    /* 模组内部射频开关切换到发送状态 */
    sx126x_rf_switch_tx();

    /* 开始发送数据 */
    sx126x_set_tx(&context_e22, 0x00);

    /* 设置发送标记 */
    context_e22.is_tx = true;

    /* (可选) LED TX指示 */
    gpio_led_tx_on();
}

/**
 * @brief  模组切换到持续接收状态
 */
void e22_demo_receive(void)
{
    /* 接收长度 最大255 */
    pkt_cfg.pld_len_in_bytes = 255;
    sx126x_set_lora_pkt_params(&context_e22, &pkt_cfg);

    /* 允许接收完成中断与前导码检测中断，并映射到引脚DIO1上 */
    sx126x_set_dio_irq_params(&context_e22,
                              SX126X_IRQ_RX_DONE | SX126X_IRQ_PREAMBLE_DETECTED,
                              SX126X_IRQ_RX_DONE | SX126X_IRQ_PREAMBLE_DETECTED,
                              SX126X_IRQ_NONE,
                              SX126X_IRQ_NONE);
    /* 清中断状态 */
    sx126x_clear_irq_status(&context_e22, SX126X_IRQ_ALL);

    /* 模组内部射频开关切换到接收状态 */
    sx126x_rf_switch_rx();

    /* 进入接收状态，等待数据 */
    sx126x_set_rx(&context_e22, 0);
}

/**
 * @brief  查询是否有接收数据
 *
 * @param   buffer 指向待拷贝数据缓存
 * @param   length 接收数据长度
 * @return  bool 有新接收数据则返回true; 否则返回false。
 */
bool e22_demo_check_rx_done(uint8_t *buffer, uint8_t *length, int8_t *rssi)
{
    bool ret = false;

    if (context_e22.is_rx == true)
    {
        context_e22.is_rx = false;

        memcpy(buffer, context_e22.rx_buffer, context_e22.rx_length);

        *length = context_e22.rx_length;
        context_e22.rx_length = 0;

        *rssi = context_e22.rx_rssi;
        context_e22.rx_rssi = 0;

        ret = true;
    }

    return ret;
}

/**
 * @brief  模组引脚中断响应
 *
 * @note 与收发函数内的中断配置关联，默认仅实现了发送完成与接收完成
 */
void e22_demo_dio1_interrupt_callback(void)
{
    sx126x_irq_mask_t irq_mask;
    sx126x_rx_buffer_status_t buffer_status;
    sx126x_pkt_status_lora_t pkt_status;

    /* 读取中断状态 */
    sx126x_get_irq_status(&context_e22, &irq_mask);

    /* 如果有中断标识 */
    if (irq_mask != SX126X_IRQ_NONE)
    {
        /* 清除中断标识 */
        sx126x_clear_irq_status(&context_e22, irq_mask);

        //================================================================================
        /* 中断：检查到了前导码 */
        if (irq_mask & SX126X_IRQ_PREAMBLE_DETECTED)
        {
        }

        //================================================================================
        /* 中断：接收完成 */
        if (irq_mask & SX126X_IRQ_RX_DONE)
        {
            sx126x_get_rx_buffer_status(&context_e22, &buffer_status);

            if (buffer_status.pld_len_in_bytes != 0)
            {
                /* 先获取接收数据长度与缓存偏移位置 */
                sx126x_get_lora_pkt_status(&context_e22, &pkt_status);

                /* 将SX126X内部缓存数据读出 */
                sx126x_read_buffer(&context_e22, buffer_status.buffer_start_pointer, context_e22.rx_buffer, buffer_status.pld_len_in_bytes);

                /* 记录接收长度 */
                context_e22.rx_length = buffer_status.pld_len_in_bytes;

                /* RSSI */
                context_e22.rx_rssi = pkt_status.rssi_pkt_in_dbm;

                /* 标记接收 */
                context_e22.is_rx = true;

                /* (可选) LED RX指示 */
                gpio_led_rx_on();
            }

            /* 重新进入接收状态 */
            e22_demo_receive();
        }
        //================================================================================
        /* 中断：发送完成 */
        if (irq_mask & SX126X_IRQ_TX_DONE)
        {
            /* 清除发送标记 */
            context_e22.is_tx = false;

            /* (可选) LED TX指示 */
            gpio_led_tx_off();
        }
    }
}
