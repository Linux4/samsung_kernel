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

int ois_drv1_open(void)
{
	pr_info("%s\n", __func__);
	return 0;
}

int ois_drv1_close(void)
{
	pr_info("%s\n", __func__);
	return 0;
}

int ois_drv1_control(struct ois_ctrl_info *ctrl_info)
{
	char read_data;
	int *param_data;

	pr_info("%s\n", __func__);
	if (ctrl_info == NULL) {
		pr_err("%s, invalid parameter\n", __func__);
		return -1;
	}
	switch (ctrl_info->cmd) {
	case OIS_CMD_MODE:
		break;
	case OIS_CMD_DATA_READ:
		//for test
		param_data = ctrl_info->param;
		ois_cam_i2c_read((u16)param_data[0], 1, &read_data, 400);
		param_data[1] = (int)read_data;
		break;
	default:
		break;
	}

	return 0;
}

struct cam_ois_func ois_drv1_func = {
	.ois_open    = ois_drv1_open,
	.ois_close   = ois_drv1_close,
	.ois_control = ois_drv1_control
};
