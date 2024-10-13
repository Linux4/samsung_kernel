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
#include <linux/atomic.h>
#include <linux/sec_hard_reset_hook.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/moduleparam.h>
#ifndef CONFIG_SEC_KEY_NOTIFIER
#include <linux/gpio_keys.h>
#else
#include "sec_key_notifier.h"
#endif
#include <linux/sec_debug.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

struct gpio_key_info {
	unsigned int keycode;
	int gpio;
	bool active_low;
};

static unsigned int hard_reset_keys[] = { KEY_POWER, KEY_VOLUMEDOWN };

static atomic_t hold_keys = ATOMIC_INIT(0);
static ktime_t hold_time;
static struct hrtimer hard_reset_hook_timer;
static bool hard_reset_occurred;
static int all_pressed;
static struct workqueue_struct *blkdev_flush_wq;
static struct delayed_work blkdev_flush_work;
int hard_reset_key_pressed = 0;

struct super_block *keypress_callback_sb = NULL;
int (*keypress_callback_fn)(struct super_block *sb) = NULL;

extern int mtk_pmic_pwrkey_status(void);
extern int mtk_pmic_homekey_status(void);

/* Proc node to enable hard reset */
static bool hard_reset_hook_enable = 1;
module_param_named(hard_reset_hook_enable, hard_reset_hook_enable, bool, 0664);
MODULE_PARM_DESC(hard_reset_hook_enable, "1: Enabled, 0: Disabled");

static bool is_hard_reset_key(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			return true;
	return false;
}

static int hard_reset_key_set(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			atomic_or(0x1 << i, &hold_keys);
	return atomic_read(&hold_keys);
}

static int hard_reset_key_unset(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			atomic_and(~(0x1) << i, &hold_keys);

	return atomic_read(&hold_keys);
}

static int hard_reset_key_all_pressed(void)
{
	return (atomic_read(&hold_keys) == all_pressed);
}

static bool is_all_key_pressed(void)
{
	unsigned short pressed;

	pressed = mtk_pmic_pwrkey_status();
	if (!pressed) {
		pr_info("%s: PowerButton was released(%d)\n", __func__, pressed);
		return false;
	}
	
	pressed = mtk_pmic_homekey_status();
	if (!pressed) {
		pr_info("%s: Vol DN was released(%d)\n", __func__, pressed);
		return false;
	}

	return true;
}

static void blkdev_flush_work_fn(struct work_struct *work) {
	int ret = 0;
	if (keypress_callback_fn && keypress_callback_sb)
		ret = keypress_callback_fn(keypress_callback_sb);
}

static enum hrtimer_restart hard_reset_hook_callback(struct hrtimer *hrtimer)
{
	if (!is_all_key_pressed()) {
		hard_reset_key_pressed = 0;
		pr_warn("All keys are not pressed\n");
		return HRTIMER_NORESTART;
	}

	pr_err("Hard Reset\n");
	hard_reset_occurred = true;
	BUG();
	return HRTIMER_RESTART;
}

static int hard_reset_hook(struct notifier_block *nb,
			   unsigned long type, void *data)
{
#ifndef CONFIG_SEC_KEY_NOTIFIER
	unsigned int code = (unsigned int)type;
	int pressed = *(int *)data;
#else
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int pressed = param->down;
#endif
	if (unlikely(!hard_reset_hook_enable))
		return NOTIFY_DONE;

	if (!is_hard_reset_key(code))
		return NOTIFY_DONE;

	if (pressed)
		hard_reset_key_set(code);
	else
		hard_reset_key_unset(code);

	if (hard_reset_key_all_pressed()) {
		hard_reset_key_pressed = 1;
		if (blkdev_flush_wq)
			queue_delayed_work(blkdev_flush_wq, &blkdev_flush_work, 0);
		hrtimer_start(&hard_reset_hook_timer,
			      hold_time, HRTIMER_MODE_REL);		
		pr_info("%s : hrtimer_start\n", __func__);
	}
	else {
		hard_reset_key_pressed = 0;
		hrtimer_try_to_cancel(&hard_reset_hook_timer);
	}

	return NOTIFY_OK;
}

#ifndef CONFIG_SEC_KEY_NOTIFIER
static struct notifier_block nb_gpio_keys = {
	.notifier_call = hard_reset_hook
};
#else
static struct notifier_block seccmn_hard_reset_notifier = {
	.notifier_call = hard_reset_hook
};
#endif

bool is_hard_reset_occurred(void)
{
	return hard_reset_occurred;
}

void hard_reset_delay(void)
{
	/* HQE team request hard reset key should guarantee 7 seconds.
	 * To get proper stack, hard reset hook starts after 6 seconds.
	 * And it will reboot before 7 seconds.
	 * Add delay to keep the 7 seconds
	 */
	if (hard_reset_occurred) {
		pr_err("wait until warm or manual reset is triggered\n");
		mdelay(3000);
	}
}

int __init hard_reset_hook_init(void)
{
	size_t i;

	hrtimer_init(&hard_reset_hook_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hard_reset_hook_timer.function = hard_reset_hook_callback;
	hold_time = ktime_set(6, 0); /* 6 seconds */

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		all_pressed |= 0x1 << i;

#ifndef CONFIG_SEC_KEY_NOTIFIER
	register_gpio_keys_notifier(&nb_gpio_keys);
#else
	sec_kn_register_notifier(&seccmn_hard_reset_notifier);
#endif
	INIT_DELAYED_WORK(&blkdev_flush_work, blkdev_flush_work_fn);
	blkdev_flush_wq = create_singlethread_workqueue("blkdev_flush_wq");
	if (!blkdev_flush_wq) {
		pr_err("fail to create blkdev_flush_wq!\n");
	}

	return 0;
}

late_initcall(hard_reset_hook_init);
