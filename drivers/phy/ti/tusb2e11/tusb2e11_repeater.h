/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/phy/ti/tusb2e11_repeater.h
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung TUSB2E11 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TUSB2E11_H__
#define __TUSB2E11_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/phy/repeater_core.h>

#define TUSB2E11_UART_PORT1_REG	0x50
#define TUSB2E11_UART_PORT1_INIT	0x0A

#define TUSB2E11_REV_ID_REG	0xB0

#define TUNE_NAME_SIZE		32
#define I2C_RETRY_CNT		3

struct tusb2e11_tune {
	char name[TUNE_NAME_SIZE];
	u8 reg;
	u8 value;
};

struct tusb2e11_pdata {
	struct tusb2e11_tune *tune;
	u8 tune_cnt;
	int gpio_eusb_ctrl_sel;
};

struct tusb2e11_ddata {
	struct i2c_client *client;
	struct mutex i2c_mutex;
	struct tusb2e11_pdata *pdata;
	struct repeater_data rdata;
	int ctrl_sel_status;
};

#endif
