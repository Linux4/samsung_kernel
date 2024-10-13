/*
 * max77816_i2c.h - Platform data for max77816 buck booster hw i2c driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "../ss_dsi_panel_common.h"

struct ss_boost_max77816_info {
	struct i2c_client *client;
	int boost_en_gpio;
};

int ss_boost_i2c_write_ex(u8 addr,  u8 value);
int ss_boost_i2c_read_ex(u8 addr,  u8 *value);
int ss_boost_i2c_en_control(bool enable);
int ss_boost_max77816_init(void);
