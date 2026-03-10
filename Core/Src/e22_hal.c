#include "sx126x_hal.h"

/**
 * 与单片机平台有关
 */
#include "gpio.h"
#include "spi.h"

/**
 * @brief 模组复位
 *
 * @param context 模组上下文。仅适配单个模组时可以不处理
 */
sx126x_hal_status_t sx126x_hal_reset( const void* context )
{
		/* E22 RESET 引脚先拉低 触发复位*/
		HAL_GPIO_WritePin( E22_RESET_GPIO_Port, E22_RESET_Pin, GPIO_PIN_RESET );
	
		/* 延迟1ms */
		HAL_Delay(1);
	
		/* E22 RESET 引脚再拉高 恢复正常*/
		HAL_GPIO_WritePin( E22_RESET_GPIO_Port, E22_RESET_Pin, GPIO_PIN_SET );

    return SX126X_HAL_STATUS_OK;
}

/**
 * @brief 忙状态等待
 *
 * @param radio 模组上下文。仅适配单个模组时可以不处理
 */
void sx126x_hal_wait_on_busy( const void* radio )
{
		/* E22 BUSY 引脚高电平表示忙 需要等待，超时 100ms 防死锁 */
		uint32_t start = HAL_GetTick();
		while( GPIO_PIN_SET == HAL_GPIO_ReadPin( E22_BUSY_GPIO_Port , E22_BUSY_Pin ) )
		{
				if( HAL_GetTick() - start > 100 )
						break;
		}
}

/**
 * @brief 模组唤醒
 *
 * @param context 模组上下文。仅适配单个模组时可以不处理
 * @return 返回 SX126X_HAL_STATUS_OK
 */
sx126x_hal_status_t sx126x_hal_wakeup( const void* context )
{
		/* E22 SPI CS(NSS) 引脚先拉低 触发唤醒*/
    HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);

		/* 延迟1ms */
		HAL_Delay(1);
	
		/* E22 SPI CS(NSS) 引脚再拉高 恢复正常*/
		HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET );

    return SX126X_HAL_STATUS_OK;
}

/**
 * @brief 寄存器写入
 *
 * @param context 模组上下文。仅适配单个模组时可以不处理
 * @param command 指向指令内容，一般为寄存器地址
 * @param command_length 指令长度
 * @param data 指向数据内容，一般为目标寄存器连续写入数据
 * @param data_length 数据长度
 * @return 返回 SX126X_HAL_STATUS_OK
 */
sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length, const uint8_t* data, const uint16_t data_length )
{
		/* E22 等待空闲 */
		sx126x_hal_wait_on_busy( context );
	
		/* E22 SPI CS(NSS) 引脚先拉低 选中 */
    HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);	
	
	  /* SPI 先发送命令 */
		HAL_SPI_Transmit( &hspi1 , (uint8_t*)command, command_length, 0xFFFF );
	
	  /* SPI 再发送数据 */
		HAL_SPI_Transmit( &hspi1 , (uint8_t*)data, data_length, 0xFFFF );		

		/* E22 SPI CS(NSS) 引脚再拉高 结束 */
		HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET );	
	
    return SX126X_HAL_STATUS_OK;
}

/**
 * @brief 寄存器读取
 *
 * @param context 模组上下文。仅适配单个模组时可以不处理
 * @param command 指向指令内容，一般为寄存器地址
 * @param command_length 指令长度
 * @param data 指向待读取数据缓存
 * @param data_length 预期读取长度
 * @return 返回 SX126X_HAL_STATUS_OK
 */
sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length, uint8_t* data, const uint16_t data_length )
{
		/* E22 等待空闲 */
		sx126x_hal_wait_on_busy( context );
	
		/* E22 SPI CS(NSS) 引脚先拉低 选中 */
		HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);	
	
		/* SPI 先发送命令 */
		HAL_SPI_Transmit( &hspi1 , (uint8_t*)command, command_length, 0xFFFF );
	
		/* SPI 再读取响应数据 */
		HAL_SPI_Receive( &hspi1 , (uint8_t*)data, data_length, 0xFFFF );		

		/* E22 SPI CS(NSS) 引脚再拉高 结束 */
		HAL_GPIO_WritePin( SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET );		

    return SX126X_HAL_STATUS_OK;
}

/**
 * @brief 射频开关切换到发送线路
 *
 * @return 返回 SX126X_HAL_STATUS_OK
 */
sx126x_hal_status_t sx126x_rf_switch_tx(void)
{
		HAL_GPIO_WritePin( E22_RXEN_GPIO_Port, E22_RXEN_Pin, GPIO_PIN_RESET );	
		HAL_GPIO_WritePin( E22_TXEN_GPIO_Port, E22_TXEN_Pin, GPIO_PIN_SET );
	
		return SX126X_HAL_STATUS_OK;
}

/**
 * @brief 射频开关切换到接收线路
 *
 * @return 返回 SX126X_HAL_STATUS_OK
 */
sx126x_hal_status_t sx126x_rf_switch_rx(void)
{
		HAL_GPIO_WritePin( E22_TXEN_GPIO_Port, E22_TXEN_Pin, GPIO_PIN_RESET );
		HAL_GPIO_WritePin( E22_RXEN_GPIO_Port, E22_RXEN_Pin, GPIO_PIN_SET );	
	
		return SX126X_HAL_STATUS_OK;
}
/* ====================================================================== */

