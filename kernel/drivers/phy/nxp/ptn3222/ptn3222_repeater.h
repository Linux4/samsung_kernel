/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/phy/nxp/ptn3222_repeater.h
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung PTN3222 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PTN3222_H__
#define __PTN3222_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/version.h>
#include <linux/phy/repeater_core.h>

#define PTN3222_REV_ID_REG		0x13

#define TUNE_NAME_SIZE		32
#define I2C_RETRY_CNT		3

struct ptn3222_tune {
	char name[TUNE_NAME_SIZE];
	u8 reg;
	u8 value;
};

struct ptn3222_pdata {
	struct ptn3222_tune *tune;
	u8 tune_cnt;
};

struct ptn3222_ddata {
	struct i2c_client *client;
	struct mutex i2c_mutex;
	struct ptn3222_pdata *pdata;
	struct repeater_data rdata;
};

#endif
