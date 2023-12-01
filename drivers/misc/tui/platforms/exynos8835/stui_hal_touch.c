/*
 * Copyright (c) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../../stui_core.h"
#include "../../stui_hal.h"
#include <linux/input/stui_inf.h>

#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
extern int stui_tsp_enter(void);
extern int stui_tsp_exit(void);
extern int stui_tsp_type(void);
#endif

static int touch_requested;

static int request_touch(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
	int tsp_type = 0;

	tsp_type = stui_tsp_type();
	if (tsp_type)
		stui_set_touch_type(tsp_type);

	if (touch_requested == 1)
		return -EALREADY;

	ret = stui_tsp_enter();
	if (ret) {
		pr_err(TUIHW_LOG_TAG " stui_tsp_enter failed:%d\n", ret);
		return ret;
	}

	touch_requested = 1;
#endif
	pr_debug(TUIHW_LOG_TAG " Touch requested\n");

	return ret;
}

static int release_touch(void)
{
	int ret = 0;

	if (touch_requested != 1)
		return -EALREADY;

#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
	ret = stui_tsp_exit();
	if (ret) {
		pr_err(TUIHW_LOG_TAG " stui_tsp_exit failed : %d\n", ret);
		return ret;
	}

	touch_requested = 0;
#endif
	pr_debug(TUIHW_LOG_TAG " Touch release\n");

	return ret;
}

int stui_i2c_protect(bool is_protect)
{
	int ret;

	pr_info(TUIHW_LOG_TAG " %s(%s) called\n",
			__func__, is_protect ? "true" : "false");

	if (is_protect)
		ret = request_touch();
	else
		ret = release_touch();

	return ret;
}
