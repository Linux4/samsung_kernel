/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_I2C_H__
#define __PANEL_I2C_H__
#include <linux/i2c.h>
#include <linux/of_device.h>
#include "panel.h"

struct i2c_data {
	char *vendor;
	char *model;
	int addr_len;
	int data_len;

	struct seqinfo *seqtbl;
	int nr_seqtbl;
};

struct panel_i2c_dev {
	struct i2c_client *client;
	struct i2c_drv_ops *ops;

	struct mutex lock;

	int addr_len;
	int data_len;
	int tx_len; /* addr_len + data_len */
};

struct i2c_drv_ops {
	int (*tx)(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);
	int (*rx)(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);
};


int panel_i2c_drv_probe(struct panel_device *panel, struct i2c_data *i2c_data);
#endif /* __PANEL_I2C_H__ */
