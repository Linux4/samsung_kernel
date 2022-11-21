/*
 * lm36274-backlight.h - Platform data for lm36274 backlight driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
//#include <asm/mach-types.h>
#include <linux/device.h>
#include <linux/of_gpio.h>

#include "ss_dsi_panel_common.h"

struct blic_message {
	char*address;
	char *data;
	int size;
};

struct lm36274_backlight_platform_data {
	int gpio_hwen;
	u32 gpio_hwen_flags;

	int gpio_enn;
	u32 gpio_enn_flags;

	int gpio_enp;
	u32 gpio_enp_flags;

	struct blic_message init_data;
	struct blic_message enable_data;
};

struct lm36274_backlight_info {
	struct i2c_client			*client;
	struct lm36274_backlight_platform_data	*pdata;
};

int lm36274_pwm_en(int enable);
void lm36274_bl_ic_en(int enable);
