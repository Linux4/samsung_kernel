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

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/sec_debug.h>
#ifndef CONFIG_SEC_KEY_NOTIFIER
#include <linux/gpio_keys.h>
#else
#include <linux/input.h>
#include "sec_key_notifier.h"
#endif

static int sec_crash_key_check_keys(struct notifier_block *nb,
				unsigned long type, void *data)
{
#ifndef CONFIG_SEC_KEY_NOTIFIER
	unsigned int code = (unsigned int)type;
	int state = *(int *)data;
#else
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int state = param->down;
#endif

#ifdef CONFIG_SEC_DEBUG
	if (code == KEY_VOLUMEDOWN || code == KEY_VOLUMEUP || code == KEY_POWER)
		sec_debug_check_crash_key(code, state);
#endif

	return NOTIFY_DONE;
}

#ifndef CONFIG_SEC_KEY_NOTIFIER
static struct notifier_block nb_gpio_keys = {
	.notifier_call = sec_crash_key_check_keys
};
#else
static struct notifier_block seccmn_crash_key_notifier = {
	.notifier_call = sec_crash_key_check_keys
};
#endif

int __init sec_crash_key_init(void)
{
	//spin_lock_init(&key_crash_lock);
	/* only work when upload enabled*/
	if (SEC_DEBUG_LEVEL(kernel)) {
#ifndef CONFIG_SEC_KEY_NOTIFIER
		register_gpio_keys_notifier(&nb_gpio_keys);
#else
		sec_kn_register_notifier(&seccmn_crash_key_notifier);
#endif
		pr_info("%s: force upload key registered\n", __func__);
	}

	return 0;
}

early_initcall(sec_crash_key_init);
