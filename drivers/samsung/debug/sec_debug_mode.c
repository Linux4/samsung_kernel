/*
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/sec_debug.h>

/* secure jtag lock */
static int __debug_sj_lock;

int secdbg_mode_check_sj(void)
{
	if (__debug_sj_lock == 1)
		/* Locked */
		return 1;
	else if (__debug_sj_lock == 0)
		/* Unlocked */
		return 0;

	return -1;
}

static int __init secdbg_mode_get_sj_status(char *str)
{
	unsigned long val = memparse(str, &str);

	pr_err("%s: start %lx\n", __func__, val);

	if (!val) {
		pr_err("%s: UNLOCKED (%lx)\n", __func__, val);
		__debug_sj_lock = 0;
		/* Unlocked or Disabled */
		return 1;
	} else {
		pr_err("%s: LOCKED (%lx)\n", __func__, val);
		__debug_sj_lock = 1;
		/* Locked */
		return 1;
	}
}
__setup("sec_debug.sjl=", secdbg_mode_get_sj_status);

/* upload mode en/disable */
static int __force_upload;

int secdbg_mode_enter_upload(void)
{
	return __force_upload;
}

static int __init secdbg_mode_force_upload(char *str)
{
	unsigned long val = memparse(str, &str);

	if (!val) {
		pr_err("%s: disabled (%lx)\n", __func__, val);
		__force_upload = 0;
		/* Unlocked or Disabled */
		return 1;
	} else {
		pr_err("%s: enabled (%lx)\n", __func__, val);
		__force_upload = 1;
		/* Locked */
		return 1;
	}
}
__setup("androidboot.force_upload=", secdbg_mode_force_upload);
