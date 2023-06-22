/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "../utility/shub_utility.h"
#include "../comm/shub_comm.h"
#include "../sensor/light.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_file_manager.h"
#include "shub_panel.h"

#include <linux/notifier.h>

#define LCD_PANEL_LCD_TYPE "/sys/class/lcd/panel/lcd_type"
static u8 lcd_type_flag;

static void get_panel_lcd_type(void)
{
	char lcd_type_data[256];
	int ret;

	ret = shub_file_read(LCD_PANEL_LCD_TYPE, lcd_type_data, sizeof(lcd_type_data), 0);
	if (ret < 0) {
		shub_errf("file read error %d", ret);
		return;
	}

	/*
	 * lcd_type_data[ret - 2], which type have different transmission ratio.
	 * [0 ~ 2] : 0.7%, [3] : 15%, [4] : 40%
	 */
	if (lcd_type_data[ret - 2] >= '0' && lcd_type_data[ret - 2] <= '2')
		lcd_type_flag = 0;
	else if (lcd_type_data[ret - 2] == '3')
		lcd_type_flag = 1;
	else
		lcd_type_flag = 2;

	shub_infof("lcd_type_flag : %d", lcd_type_flag);
}

static int fm_ready_panel(struct notifier_block *this, unsigned long event, void *ptr)
{
	shub_infof("notify event %d", event);
	return NOTIFY_OK;
}

static struct notifier_block fm_notifier = {
    .notifier_call = fm_ready_panel,
};

void init_shub_panel(void)
{
	shub_infof();
	register_file_manager_ready_callback(&fm_notifier);
}

void remove_shub_panel(void)
{
	shub_infof();
}

int save_panel_lcd_type(void)
{
	int ret = 0;

	get_panel_lcd_type();

	ret = shub_file_write_no_wait(UID_FILE_PATH, (char *)&lcd_type_flag, sizeof(lcd_type_flag), 0);
	if (ret != sizeof(lcd_type_flag)) {
		shub_errf("failed");
		return -EIO;
	}

	shub_infof("save lcd_type_flag %d", lcd_type_flag);

	return ret;
}

inline bool is_lcd_changed(void)
{
	int ret = 0;
	u8 curr_lcd_type_flag = 0;

	get_panel_lcd_type();
	ret = shub_file_read(UID_FILE_PATH, &curr_lcd_type_flag, sizeof(curr_lcd_type_flag), 0);
	if (ret != sizeof(curr_lcd_type_flag)) {
		shub_errf("saved lcd type read failed %d", ret);
		return false;
	}
	shub_infof("%d -> %d", lcd_type_flag, curr_lcd_type_flag);
	return (lcd_type_flag != curr_lcd_type_flag);
}
