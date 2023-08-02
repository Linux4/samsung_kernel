/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DISPC_H_
#define _DISPC_H_

typedef enum{
	DISPLAY_TYPE_MCU = 0,
	DISPLAY_TYPE_RGB,
	DISPLAY_TYPE_MIPI,
}E_DISPLAY_TYPE;

enum DISPC_PIN_FUNC {
	DISPC_PIN_FUNC0 = 0,
	DISPC_PIN_FUNC1,
	DISPC_PIN_FUNC2,
	DISPC_PIN_FUNC3,
};

int32_t autotst_dispc_mcu_send_cmd(uint32_t cmd);
int32_t autotst_dispc_mcu_send_cmd_data(uint32_t cmd, uint32_t data);
int32_t autotst_dispc_mcu_send_data(uint32_t data);
uint32_t autotst_dispc_mcu_read_data(void);

int32_t autotst_dispc_init(int display_type);
int32_t autotst_dispc_uninit(int display_type);
int32_t autotst_dispc_refresh (void);

int autotst_dispc_pin_ctrl(int type);

#endif
