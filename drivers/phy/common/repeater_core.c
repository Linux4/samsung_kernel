// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung USB repeater driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/phy/repeater_core.h>

static struct repeater_data *gdata;

int repeater_enable(bool en)
{
	struct repeater_data *data = gdata;

	if (!data) {
		pr_err("%s no repeater data\n", __func__);
		return -ENODATA;
	}

	if (data->enable)
		return data->enable(data->private_data, en);
	else
		pr_err("%s no enable function\n", __func__);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(repeater_enable);

int repeater_core_register(struct repeater_data *_data)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!_data)
		return -ENODATA;

	gdata = _data;

	return ret;
}
EXPORT_SYMBOL_GPL(repeater_core_register);

void repeater_core_unregister(void)
{
	pr_info("%s\n", __func__);

	gdata = NULL;
}
EXPORT_SYMBOL_GPL(repeater_core_unregister);

MODULE_DESCRIPTION("Samsung USB REPEATER driver");
MODULE_LICENSE("GPL");
