/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/reboot.h>
#include "sec_key_notifier.h"

static const unsigned int hard_reset_keys[] = { KEY_POWER, KEY_VOLUMEDOWN };

static int hold_keys;
static int all_pressed;
static ktime_t hold_time;
static struct hrtimer hard_reset_hook_timer;

/* Proc node to enable hard reset */
static bool hard_reset_hook_enable = 1;
module_param_named(hard_reset_hook_enable, hard_reset_hook_enable, bool, 0664);
MODULE_PARM_DESC(hard_reset_hook_enable, "1: Enabled, 0: Disabled");

int hard_reset_delay(struct notifier_block *nb,
		unsigned long action, void *data)
{
	/* HQE team request hard reset key should guarantee 7 seconds.
	 * To get proper stack, hard reset hook starts after 6 seconds.
	 * And it will reboot before 7 seconds.
	 * Add delay to keep the 7 seconds
	 */
	pr_err("wait until warm or manual reset is triggered\n");
	mdelay(2000); // TODO: change to dev_mdelay

	return NOTIFY_DONE;
}

static struct notifier_block sec_hard_reset_delay = {
	.notifier_call = hard_reset_delay,
	.priority = 131,
};

static enum hrtimer_restart hard_reset_hook_callback(struct hrtimer *hrtimer)
{
	int err;

	pr_err("Hard Reset\n");

	err = register_restart_handler(&sec_hard_reset_delay);
	if (err) {
		pr_err("cannot register restart handler (err=%d)\n", err);
	}

	panic("Hard Reset Hook");
	return HRTIMER_RESTART;
}

static ssize_t hard_reset_key_idx(unsigned int code)
{
	ssize_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			return i;

	return -1;
}

static inline void hard_reset_key_set(int idx)
{
	hold_keys |= (0x1 << idx);
}

static inline void hard_reset_key_unset(int idx)
{
	hold_keys &= ~(0x1 << idx);
}

static inline bool hard_reset_key_all_pressed(void)
{
	return (hold_keys == all_pressed);
}

static int hard_reset_hook(struct notifier_block *nb,
			   unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int pressed = param->down;
	ssize_t idx;

	if (unlikely(!hard_reset_hook_enable))
		return NOTIFY_DONE;

	idx = hard_reset_key_idx(code);
	if (idx == -1)
		return NOTIFY_DONE;

	if (pressed)
		hard_reset_key_set(idx);
	else
		hard_reset_key_unset(idx);

	if (hard_reset_key_all_pressed()) {
		hrtimer_start(&hard_reset_hook_timer,
			hold_time, HRTIMER_MODE_REL);
		pr_info("%s : hrtimer_start\n", __func__);
	} else {
		hrtimer_try_to_cancel(&hard_reset_hook_timer);
	}

	return NOTIFY_OK;
}

static struct notifier_block seccmn_hard_reset_notifier = {
	.notifier_call = hard_reset_hook
};

static int __init hard_reset_hook_init(void)
{
	int ret = 0;

	hrtimer_init(&hard_reset_hook_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hard_reset_hook_timer.function = hard_reset_hook_callback;
	hold_time = ktime_set(6, 0); /* 6 seconds */
	all_pressed = (0x1 << ARRAY_SIZE(hard_reset_keys)) - 1;
	ret = sec_kn_register_notifier(&seccmn_hard_reset_notifier);

	return ret;
}

static void hard_reset_hook_exit(void)
{
	sec_kn_unregister_notifier(&seccmn_hard_reset_notifier);
}

module_init(hard_reset_hook_init);
module_exit(hard_reset_hook_exit);
#if IS_ENABLED(CONFIG_PINCTRL_SAMSUNG)
MODULE_SOFTDEP("pre: pinctrl-samsung-core");
#endif

MODULE_DESCRIPTION("Samsung Hard_reset_hook driver");
MODULE_LICENSE("GPL v2");
