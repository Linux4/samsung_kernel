/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/leds-s2mu005.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef S2MU005_DTNAME
#define S2MU005_DTNAME "mediatek,flashlights_s2mu005"
#endif
#if 0
#ifndef LM3643_DTNAME_I2C
#define LM3643_DTNAME_I2C "mediatek,flashlights_s2mu005_i2c"
#endif
#endif
#define S2MU005_NAME "flashlights-s2mu005"

#define CONFIG_LEDS_S2MU005_FLASH 1
#ifndef CONFIG_LEDS_S2MU005_FLASH
static int g_bLtVersion;
#endif

/* define channel, level */
#define S2MU005_CHANNEL_NUM 2
#define S2MU005_CHANNEL_CH1 0
#define S2MU005_CHANNEL_CH2 1
/* define mutex and work queue */
static DEFINE_MUTEX(s2mu005_mutex);
static struct work_struct s2mu005_work_ch1;
static struct work_struct s2mu005_work_ch2;
/* define usage count */
static int use_count;
static int s2mu005_level_ch1 = -1;
/* static int s2mu005_level_ch2 = -1; */

#if 0
static int s2mu005_is_torch_Ch1(int level)
{
	if (level == 0)
		return -1;
	return 0;
}
#endif

static int s2mu005_verify_level_Ch1(int level)
{
#if 0
	pr_err("s2mu005_verify_level_Ch1. %d\n", level);
	if (level < 0)
		level = 0;
	else if (level >= 3)
		level = 2;
#endif
	return level;
}

/* flashlight enable function */
static int s2mu005_enable_ch1(void)
{
#if 0
	pr_err("s2mu005_enable_ch1. %d\n", s2mu005_level_ch1);
	if (!s2mu005_is_torch_Ch1(s2mu005_level_ch1)) {
	/* torch mode */
		pr_err("ss_rear_flash_led_torch_on\n");
		ss_rear_flash_led_torch_on();
	} else {
	/* flash mode */
		pr_err("ss_rear_flash_led_flash_on\n");
		ss_rear_flash_led_flash_on();
	}
	return 1;
#endif
	pr_err("s2mu005_enable_ch1. %d\n", s2mu005_level_ch1);
	if (s2mu005_level_ch1 == 1)
		ss_rear_flash_led_flash_on();
	else
		ss_rear_flash_led_torch_on();

	return 1;
}

static int s2mu005_enable_ch2(void)
{
	pr_err("s2mu005_enable_ch2.\n");
	//ss_front_flash_led_turn_on();
	return 1;
}

static int s2mu005_enable(int channel)
{
	if (channel == S2MU005_CHANNEL_CH1)
		s2mu005_enable_ch1();
	else if (channel == S2MU005_CHANNEL_CH2)
		s2mu005_enable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

/* flashlight disable function */
static int s2mu005_disable_ch1(void)
{
	pr_err("s2mu005_disable_ch1\n");
	ss_rear_flash_led_turn_off();
	return 1;
}

static int s2mu005_disable_ch2(void)
{
	pr_err("s2mu005_disable_ch2\n");
	//ss_front_flash_led_turn_off();
	return 1;
}

static int s2mu005_disable(int channel)
{
	if (channel == S2MU005_CHANNEL_CH1)
		s2mu005_disable_ch1();
	else if (channel == S2MU005_CHANNEL_CH2)
		s2mu005_disable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

/* set flashlight level */
static int s2mu005_set_level_ch1(int level)
{
#if 0
	int ret = 1;

	pr_err("s2mu005_set_level_ch1. %d\n", level);
	level = s2mu005_verify_level_Ch1(level);
	s2mu005_level_ch1 = level;
	ss_rear_torch_set_flashlight(level);
	return ret;
#endif
	int ret = 1;

	pr_err("s2mu005_set_level_ch1. %d\n", level);
	level = s2mu005_verify_level_Ch1(level);
	s2mu005_level_ch1 = level;
	if (level == 0)
		ss_rear_torch_set_flashlight(1);

	return ret;
}

int s2mu005_set_level_ch2(int level)
{
	int ret = 1;

	pr_err("s2mu005_set_level_ch2. %d\n", level);
	return ret;
}

static int s2mu005_set_level(int channel, int level)
{
	if (channel == S2MU005_CHANNEL_CH1)
		s2mu005_set_level_ch1(level);
	else if (channel == S2MU005_CHANNEL_CH2)
		s2mu005_set_level_ch2(level);
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

/* flashlight init */
int s2mu005_init(void)
{
	pr_debug("lt work queue callback\n");
	return 1;
}

/* flashlight uninit */
int s2mu005_uninit(void)
{
	s2mu005_disable(S2MU005_CHANNEL_CH1);
	s2mu005_disable(S2MU005_CHANNEL_CH2);
#if 0
	s2mu005_pinctrl_set(S2MU005_PINCTRL_PIN_HWEN, S2MU005_PINCTRL_PINSTATE_LOW);
#endif
	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer s2mu005_timer_ch1;
static struct hrtimer s2mu005_timer_ch2;
static unsigned int s2mu005_timeout_ms[S2MU005_CHANNEL_NUM];

static void s2mu005_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	s2mu005_disable_ch1();
}

static void s2mu005_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	s2mu005_disable_ch2();
}

static enum hrtimer_restart s2mu005_timer_func_ch1(struct hrtimer *timer)
{
	
	schedule_work(&s2mu005_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart s2mu005_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&s2mu005_work_ch2);
	return HRTIMER_NORESTART;
}

int s2mu005_timer_start(int channel, ktime_t ktime)
{
	if (channel == S2MU005_CHANNEL_CH1)
		hrtimer_start(&s2mu005_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == S2MU005_CHANNEL_CH2)
		hrtimer_start(&s2mu005_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

int s2mu005_timer_cancel(int channel)
{
	if (channel == S2MU005_CHANNEL_CH1)
		hrtimer_cancel(&s2mu005_timer_ch1);
	else if (channel == S2MU005_CHANNEL_CH2)
		hrtimer_cancel(&s2mu005_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int s2mu005_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;
	/* verify channel */
	if (channel < 0 || channel >= S2MU005_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}
	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		s2mu005_timeout_ms[channel] = fl_arg->arg;
		break;
	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		s2mu005_set_level(channel, fl_arg->arg);
		break;
	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (s2mu005_timeout_ms[channel]) {
				ktime = ktime_set(s2mu005_timeout_ms[channel] / 1000,
						(s2mu005_timeout_ms[channel] % 1000) * 1000000);
				s2mu005_timer_start(channel, ktime);
			}
			s2mu005_enable(channel);
		} else {
			s2mu005_disable(channel);
			s2mu005_timer_cancel(channel);
		}
		break;
	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}
	return 0;
}

static int s2mu005_open(void)
{
	/* Actual behavior move to set driver function since power saving issue */
	return 0;
}

static int s2mu005_release(void)
{
	/* uninit chip and clear usage count */
	mutex_lock(&s2mu005_mutex);
	use_count--;
	if (!use_count)
		s2mu005_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&s2mu005_mutex);
	pr_debug("Release: %d\n", use_count);
	return 0;
}

static int s2mu005_set_driver(int set)
{
	/* init chip and set usage count */
	mutex_lock(&s2mu005_mutex);
	if (!use_count)
		s2mu005_init();
	use_count++;
	mutex_unlock(&s2mu005_mutex);
	pr_debug("Set driver: %d\n", use_count);
	return 0;
}

static ssize_t s2mu005_strobe_store(struct flashlight_arg arg)
{
	s2mu005_set_driver(1);
	s2mu005_set_level(arg.ct, arg.level);
	s2mu005_enable(arg.ct);
	if (arg.dur > 20)
		msleep(arg.dur);
	else
		usleep_range(arg.dur*1000,arg.dur*1000);
	s2mu005_disable(arg.ct);
	s2mu005_release();
	return 0;
}

static struct flashlight_operations s2mu005_ops = {
	s2mu005_open,
	s2mu005_release,
	s2mu005_ioctl,
	s2mu005_strobe_store,
	s2mu005_set_driver
};




/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int s2mu005_probe(struct platform_device *dev)
{
	int err;

	pr_debug("Probe start.\n");
	/* init work queue */
	INIT_WORK(&s2mu005_work_ch1, s2mu005_work_disable_ch1);
	INIT_WORK(&s2mu005_work_ch2, s2mu005_work_disable_ch2);

	/* init timer */
	hrtimer_init(&s2mu005_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	s2mu005_timer_ch1.function = s2mu005_timer_func_ch1;
	hrtimer_init(&s2mu005_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	s2mu005_timer_ch2.function = s2mu005_timer_func_ch2;
	s2mu005_timeout_ms[S2MU005_CHANNEL_CH1] = 100;
	s2mu005_timeout_ms[S2MU005_CHANNEL_CH2] = 100;

	/* register flashlight operations */
	if (flashlight_dev_register(S2MU005_NAME, &s2mu005_ops)) {
		pr_err("Failed to register flashlight device.\n");
		err = -EFAULT;
		/* goto err_free; */
	}

	/* clear usage count */
	use_count = 0;
	pr_debug("Probe done.\n");
	return 0;
}

static int s2mu005_remove(struct platform_device *dev)
{
	pr_debug("Remove start.\n");
	/* flush work queue */
	flush_work(&s2mu005_work_ch1);
	flush_work(&s2mu005_work_ch2);
	/* unregister flashlight operations */
	flashlight_dev_unregister(S2MU005_NAME);
	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id s2mu005_of_match[] = {
	{.compatible = S2MU005_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, s2mu005_of_match);
#else
static struct platform_device s2mu005_platform_device[] = {
	{
		.name = S2MU005_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, s2mu005_platform_device);
#endif

static struct platform_driver s2mu005_platform_driver = {
	.probe = s2mu005_probe,
	.remove = s2mu005_remove,
	.driver = {
		.name = S2MU005_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = s2mu005_of_match,
#endif
	},
};

static int __init flashlight_s2mu005_init(void)
{
	int ret;

	pr_debug("Init start.\n");
#ifndef CONFIG_OF
	ret = platform_device_register(&s2mu005_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif
	ret = platform_driver_register(&s2mu005_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}
	pr_debug("Init done.\n");
	return 0;
}

static void __exit flashlight_s2mu005_exit(void)
{
	pr_debug("Exit start.\n");
	platform_driver_unregister(&s2mu005_platform_driver);
	pr_debug("Exit done.\n");
}

module_init(flashlight_s2mu005_init);
module_exit(flashlight_s2mu005_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight S2MU005 Driver");

