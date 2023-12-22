/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>

#include "cam_ois_ctrl.h"

int cam_ois_open(enum IMGSENSOR_SENSOR_IDX idx)
{
	struct cam_ois_info *ois_info;
	struct i2c_client *ois_i2c_client;

	ois_info = get_cam_ois_info(idx);
	if (ois_info == NULL) {
		pr_err("cam ois info is NULL\n");
		return -1;
	}

	pr_info("Sensor Idx: %d, OIS I2C: %d\n", idx, ois_info->i2c_chanel);
	if (ois_info->i2c_chanel == OIS_I2C_DEV_NONE) {
		pr_debug("not supported OIS\n");
		return 0;
	}

	ois_i2c_client = get_ois_i2c_client(ois_info->i2c_chanel);
	if (ois_i2c_client == NULL) {
		pr_err("OIS I2C client is NULL\n");
		return -1;
	}

	// set I2C client
	set_global_ois_i2c_client(ois_i2c_client);

	if (ois_info->ois_func == NULL) {
		pr_err("ois func is NULL\n");
		return -1;
	}
	ois_info->ois_func->ois_open();

	return 0;
}


int cam_ois_close(enum IMGSENSOR_SENSOR_IDX idx)
{
	struct cam_ois_info *ois_info;

	ois_info = get_cam_ois_info(idx);
	if (ois_info == NULL) {
		pr_err("cam ois info is NULL\n");
		return -1;
	}

	if (ois_info->ois_func == NULL) {
		pr_err("ois func is NULL\n");
		return -1;
	}
	ois_info->ois_func->ois_close();

	set_global_ois_i2c_client(NULL);

	return 0;
}

int cam_ois_control(enum IMGSENSOR_SENSOR_IDX idx, struct ois_ctrl_info *crtl_info)
{
	struct cam_ois_info *ois_info;

	ois_info = get_cam_ois_info(idx);
	if (ois_info == NULL) {
		pr_err("cam ois info is NULL\n");
		return -1;
	}

	if (ois_info->ois_func == NULL) {
		pr_err("ois func is NULL\n");
		return -1;
	}
	ois_info->ois_func->ois_control(crtl_info);

	return 0;
}
