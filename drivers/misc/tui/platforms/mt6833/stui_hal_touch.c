/*
 * Copyright (c) 2015-2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stui_core.h"
#include "stui_hal.h"
#include <linux/spinlock.h>
#include <linux/input/stui_inf.h>

static int touch_requested;
static DEFINE_SPINLOCK(stui_touch_lock);
static int (*stui_tsp_enter_cb)(void);
static int (*stui_tsp_exit_cb)(void);

int stui_set_info(int (*tsp_enter_cb)(void), int (*tsp_exit_cb)(void), uint32_t touch_type)
{
	spin_lock(&stui_touch_lock);
	if (stui_tsp_enter_cb || stui_tsp_exit_cb) {
		spin_unlock(&stui_touch_lock);
		pr_err("[STUI] tsp handlers already set\n");
		return -EBUSY;
	}

	stui_tsp_enter_cb = tsp_enter_cb;
	stui_tsp_exit_cb = tsp_exit_cb;
	stui_set_touch_type(touch_type);
	spin_unlock(&stui_touch_lock);

	return 0;
}

static int request_touch(void)
{
	int ret = 0;

	if (touch_requested == 1)
		return -EALREADY;

	ret = stui_tsp_enter_cb();
	if (ret) {
		pr_err("[STUI] stui_tsp_enter failed:%d\n", ret);
		return ret;
	}

	touch_requested = 1;
	pr_info(KERN_DEBUG "[STUI] Touch requested\n");

	return ret;
}

static int release_touch(void)
{
	int ret = 0;

	if (touch_requested != 1)
		return -EALREADY;

	ret = stui_tsp_exit_cb();
	if (ret) {
		pr_err("[STUI] stui_tsp_exit failed : %d\n", ret);
		return ret;
	}

	touch_requested = 0;
	pr_info(KERN_DEBUG "[STUI] Touch release\n");

	return ret;
}

int stui_i2c_protect(bool is_protect)
{
	int ret;

	pr_info(KERN_DEBUG "[STUI] %s(%s) called\n",
			__func__, is_protect ? "true" : "false");

	if (is_protect)
		ret = request_touch();
	else
		ret = release_touch();

	return ret;
}
