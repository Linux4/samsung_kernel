/*
 * Zinitix zt7554 touchscreen driver - Platform Data
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * Default path : linux/input/zt7554_platform_data.h
 *
 */

#ifndef _LINUX_ZT7554_TOUCH_H
#define _LINUX_ZT7554_TOUCH_H

struct zt7554_ts_platform_data {
	int		gpio_int;
	int		gpio_scl;
	int		gpio_sda;
	int		gpio_ldo_en;
	int             (*tsp_power)(struct i2c_client *client, int on);
	u32		x_resolution;
	u32		y_resolution;
	const char	*fw_name;
	const char	*ext_fw_name;
	const char	*model_name;
	u32		page_size;
	u32		orientation;
	u32		tsp_supply_type;
	u32		core_num;
};

#endif

