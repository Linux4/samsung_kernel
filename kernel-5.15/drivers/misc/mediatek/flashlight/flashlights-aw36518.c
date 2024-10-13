/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include "flashlight.h"
#include "flashlight-dt.h"
#include "flashlight-core.h"
#include <linux/leds-aw36518.h>

/* device tree should be defined in flashlight-dt.h */
#define AW36518_FLASH_NAME "flashlights-aw36518"

/* define channel, level */
#define AW36518_CHANNEL_NUM          1

/* define level */
#define AW36518_LEVEL_TORCH          7
#define AW36518_LEVEL_MAX            60
#define AW36518_HW_TIMEOUT 1600 /* ms */
#define AW36518_DUTY_STEP 25

#define AW_I2C_RETRIES			5
#define AW_I2C_RETRY_DELAY		2

#define SGM3785_GPIO_NAME "flashlights-sgm3785-gpio"

/* define mutex and work queue */
static DEFINE_MUTEX(aw36518_mutex);
static struct work_struct aw36518_work;
static struct work_struct aw36518_prepare_flash_work;
static int aw36518_prepare_flashmode_state = AW36518_FLED_MODE_CLOSE_FLASH;

struct i2c_client *aw36518_flashlight_client;

/* define usage count */
static int use_count;


/* platform data */
struct aw36518_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

static int aw36518_current_level;

static int aw36518_is_torch(int level)
{
	if (level > AW36518_LEVEL_TORCH)
		return 0;

	return 1;
}

static int aw36518_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= AW36518_LEVEL_MAX)
		level = AW36518_LEVEL_MAX - 1;

	return level;
}

/* set flashlight level */
static void aw36518_set_level(int level)
{
	level = aw36518_verify_level(level);
	aw36518_current_level = level;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer aw36518_timer;
static unsigned int aw36518_timeout_ms;

static void aw36518_work_disable(struct work_struct *data)
{
	pr_info("work queue callback\n");
	aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
}

static enum hrtimer_restart aw36518_timer_func(struct hrtimer *timer)
{
	schedule_work(&aw36518_work);
	return HRTIMER_NORESTART;
}

static void aw36518_trigger_preflash(struct work_struct *data)
{
	aw36518_fled_mode_ctrl(aw36518_prepare_flashmode_state, 0);
	if (aw36518_prepare_flashmode_state == AW36518_FLED_MODE_PREPARE_FLASH) {
		pr_info("[%s]%d the current state is : AW36518_FLED_MODE_PREPARE_FLASH \n", __func__, __LINE__);
	} else if (aw36518_prepare_flashmode_state == AW36518_FLED_MODE_CLOSE_FLASH) {
		pr_info("[%s]%d the current state is AW36518_FLED_MODE_CLOSE_FLASH \n", __func__, __LINE__);
	}
}

/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw36518_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;


	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw36518_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw36518_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (aw36518_timeout_ms) {
				s = aw36518_timeout_ms / 1000;
				ns = aw36518_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&aw36518_timer, ktime,
						HRTIMER_MODE_REL);
			}
			//0 based indexing for level
			if (aw36518_is_torch(aw36518_current_level)) {
				pr_debug("TORCH MODE");
				aw36518_fled_mode_ctrl(AW36518_FLED_MODE_TORCH_FLASH
						, (aw36518_current_level + 1) * AW36518_DUTY_STEP);
			} else {
				pr_debug("FLASH MODE");
				aw36518_fled_mode_ctrl(AW36518_FLED_MODE_MAIN_FLASH
						, (aw36518_current_level + 1) * AW36518_DUTY_STEP);
			}
		} else {
			aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
			hrtimer_cancel(&aw36518_timer);
		}
		break;

	case FLASH_IOC_PRE_ON:
		pr_debug("FLASH_IOC_PRE_ON(%d)\n", channel);
		if (aw36518_prepare_flashmode_state == AW36518_FLED_MODE_CLOSE_FLASH) {
			aw36518_prepare_flashmode_state = AW36518_FLED_MODE_PREPARE_FLASH;
		} else if (aw36518_prepare_flashmode_state == AW36518_FLED_MODE_PREPARE_FLASH) {
			aw36518_prepare_flashmode_state = AW36518_FLED_MODE_CLOSE_FLASH;
		}
		schedule_work(&aw36518_prepare_flash_work);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = AW36518_LEVEL_MAX;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = AW36518_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = aw36518_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = ((fl_arg->arg)+1) * AW36518_DUTY_STEP;
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = AW36518_HW_TIMEOUT;
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d)\n", channel);
		break;

	default:
		pr_err("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}
	return 0;
}

static int aw36518_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw36518_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw36518_set_driver(int set)
{
	/* init chip and set usage count */
	/* set chip and usage count */
	mutex_lock(&aw36518_mutex);
	if (set) {
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw36518_mutex);
	return 0;
}

static ssize_t aw36518_strobe_store(struct flashlight_arg arg)
{
	aw36518_set_driver(1);
	aw36518_set_level(arg.level);
	aw36518_timeout_ms = 0;
	aw36518_set_driver(0);

	return 0;
}

static struct flashlight_operations aw36518_ops = {
	aw36518_open,
	aw36518_release,
	aw36518_ioctl,
	aw36518_strobe_store,
	aw36518_set_driver
};

static int aw36518_parse_dt(struct device *dev,
		struct aw36518_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				AW36518_FLASH_NAME);
		pdata->dev_id[i].channel = i + 1;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}
	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/

static int
aw36518_probe(struct platform_device *pdev)
{
	struct aw36518_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err = 0;
	int i;

	pr_debug("Probe start.\n");

	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pdev->dev.platform_data = pdata;
		err = aw36518_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err_free;
	}

	/* init work queue */
	INIT_WORK(&aw36518_work, aw36518_work_disable);
	INIT_WORK(&aw36518_prepare_flash_work, aw36518_trigger_preflash);

	/* init timer */
	hrtimer_init(&aw36518_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw36518_timer.function = aw36518_timer_func;
	aw36518_timeout_ms = AW36518_HW_TIMEOUT;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&aw36518_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(AW36518_FLASH_NAME, &aw36518_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_info("%s Probe done.\n", __func__);
	return 0;
err_free:
	return err;
}

static int aw36518_remove(struct platform_device *pdev)
{
	struct aw36518_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(AW36518_FLASH_NAME);

	/* flush work queue */
	flush_work(&aw36518_work);
	flush_work(&aw36518_prepare_flash_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw36518_of_match[] = {
	{.compatible = AW36518_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, aw36518_of_match);
#else
static struct platform_device aw36518_platform_device[] = {
	{
		.name = AW36518_FLASH_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, aw36518_platform_device);
#endif

static struct platform_driver aw36518_platform_driver = {
	.probe = aw36518_probe,
	.remove = aw36518_remove,
	.driver = {
		.name = AW36518_FLASH_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw36518_of_match,
#endif
	},
};

static int __init flashlight_aw36518_init(void)
{
	int ret;

	pr_info("%s driver version %s.\n", __func__, AW36518_DRIVER_VERSION);

#ifndef CONFIG_OF
	ret = platform_device_register(&aw36518_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&aw36518_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_info("flashlight_aw36518 Init done.\n");

	return 0;
}

static void __exit flashlight_aw36518_exit(void)
{
	pr_info("flashlight_aw36518-Exit start.\n");

	platform_driver_unregister(&aw36518_platform_driver);

	pr_info("flashlight_aw36518 Exit done.\n");
}

module_init(flashlight_aw36518_init);
module_exit(flashlight_aw36518_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<safy.odedara@samsung.com>,<sk.pankaj@samsung.com>");
MODULE_DESCRIPTION("AW Flashlight AW36518 Driver");

