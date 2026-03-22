#include "main.h"

#define KEY_SHORT_PRESS_TIME_MS  100
#define KEY_LONG_PRESS_TIME_MS   1000

typedef struct
{
		uint32_t count ;
		bool continue_enable;
		bool is_press;
		bool is_continue;
		bool is_long_press;
}key_config_t;

static volatile bool s_any_key_activity = false;

bool key_any_activity(void)
{
	bool ret = s_any_key_activity;
	s_any_key_activity = false;
	return ret;
}

static key_config_t key_group[3] =
{
		[ KEY_NAME_UP ] = {
			.count = 0,
			.is_press = 0,
			.continue_enable = false,
			.is_continue =  false,
			.is_long_press = false,
		},

		[ KEY_NAME_DOWN ] = {
			.count = 0,
			.is_press = 0,
			.continue_enable = false,
			.is_continue =  false,
			.is_long_press = false,
		},

		[ KEY_NAME_ENTER ] = {
			.count = 0,
			.is_press = 0,
			.continue_enable = false,
			.is_continue =  false,
			.is_long_press = false,
		}
};


bool key_check_press( key_name_t name )
{
	bool ret = false;

	/* 上层会持续查询状态，给出当前状态 */
	ret = key_group[name].is_press;

	/* 重置flash，禁止二次触发 */
	key_group[name].is_press = false;

	return ret;
}

bool key_check_long_press( key_name_t name )
{
	bool ret = key_group[name].is_long_press;
	key_group[name].is_long_press = false;
	return ret;
}

uint32_t key_get_hold_time( key_name_t name )
{
	return key_group[name].count;
}

void key_set_continue( key_name_t name , bool enable )
{
	key_group[name].continue_enable = enable;
}

static inline void key_press( key_name_t name )
{
	key_group[name].count ++;

	/* 如果短按超过判定时间 */
	if( key_group[name].count > KEY_SHORT_PRESS_TIME_MS )
	{
		s_any_key_activity = true;
		/* 长按检测: 超过 1000ms 触发长按 */
		if( key_group[name].count > KEY_LONG_PRESS_TIME_MS )
		{
			if( key_group[name].is_long_press == false && key_group[name].is_continue == true )
			{
				key_group[name].is_long_press = true;
			}
			return;
		}

		/* 如果禁止连续按下判定 */
		if( key_group[name].continue_enable == false )
		{
				/* 标记已过消抖阈值，短按在释放时触发 */
				key_group[name].is_continue = true;
		}
		/* 如果允许连续按下判定 */
		else
		{
				/* 连续给出按下标识 */
				key_group[name].is_press = true;
				/* 重新计时 */
				key_group[name].count = 0;
		}
	}
}

static inline void key_release( key_name_t name )
{
	/* 非连续模式: 释放时判定短按 (仅在消抖通过且长按未触发时) */
	if( key_group[name].continue_enable == false &&
	    key_group[name].is_continue == true &&
	    key_group[name].is_long_press == false )
	{
		key_group[name].is_press = true;
	}
	key_group[ name ].count = 0;
	key_group[ name ].is_continue = false;
	key_group[ name ].is_long_press = false;
}


void key_timer_1ms_interrupt_callback(void)
{
	if( HAL_GPIO_ReadPin( KEY_UP_GPIO_Port, KEY_UP_Pin ) == RESET )
	{
			key_press( KEY_NAME_UP );
	}
	else
	{
			key_release( KEY_NAME_UP );
	}
	
	if( HAL_GPIO_ReadPin( KEY_DOWN_GPIO_Port, KEY_DOWN_Pin ) == RESET )
	{
			key_press( KEY_NAME_DOWN );
	}
	else
	{
			key_release( KEY_NAME_DOWN );
	}

	if( HAL_GPIO_ReadPin( KEY_ENTER_GPIO_Port, KEY_ENTER_Pin ) == RESET )
	{
			key_press( KEY_NAME_ENTER );
	}
	else
	{
			key_release( KEY_NAME_ENTER );
	}	
	
}
