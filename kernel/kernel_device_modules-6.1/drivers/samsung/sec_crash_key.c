/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/sec_debug.h>
#include <linux/input.h>
#include "sec_key_notifier.h"

void check_crash_key_panic(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	if (is_debug_level_low()) {
		printk("%s: returnd due to debug_level is low", __func__);	
		return;
	}

	printk("%s: code is %d, value is %d\n", __func__, code, value);

	/* Enter Force Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = true;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info
				    ("%s: count for enter forced upload : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2)
					panic("Crash Key");
			}
		}
	} else {
		if (code == KEY_VOLUMEUP) {
			volup_p = false;
		}
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static int sec_crash_key_check_keys(struct notifier_block *nb,
				unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int state = param->down;

	if (code == KEY_VOLUMEDOWN || code == KEY_VOLUMEUP || code == KEY_POWER)
		check_crash_key_panic(code, state);

	return NOTIFY_DONE;
}

static struct notifier_block seccmn_crash_key_notifier = {
	.notifier_call = sec_crash_key_check_keys
};

int sec_crash_key_init(void)
{
	pr_info("%s\n", __func__);

	/* only work when upload enabled*/
	if(is_debug_level_low()) 
		return 0;

	sec_kn_register_notifier(&seccmn_crash_key_notifier);
	pr_info("%s: force upload key registered\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sec_crash_key_init);



