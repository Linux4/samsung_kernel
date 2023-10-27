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

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

#undef LOG_INF
#define FLASHLIGHTS_OCP8132A_DEBUG 1
#if FLASHLIGHTS_OCP8132A_DEBUG
#define LOG_INF(format, args...) pr_info("flashLight-ocp8132a" "[%s]"  format, __func__,  ##args)
#else
#define LOG_INF(format, args...)
#endif
#define LOG_ERR(format, args...) pr_err("flashLight-ocp8132a" "[%s]" format, __func__, ##args)

/* define device tree */
/* TODO: modify temp device tree name */
#ifndef OCP8132A_GPIO_DTNAME
#define OCP8132A_GPIO_DTNAME "mediatek,flashlights_ocp8132a"
#endif

/* TODO: define driver name */
#define OCP8132A_NAME "flashlights-ocp8132a"

/* define registers */
/* TODO: define register */

/* define mutex and work queue */
static DEFINE_MUTEX(ocp8132a_mutex);
static struct work_struct ocp8132a_work;
static int mainFlashFlag  = 0;

/* define pinctrl */
/* TODO: define pinctrl */
#define OCP8132A_PINCTRL_PIN_XXX 0
#define OCP8132A_PINCTRL_PINSTATE_LOW 0
#define OCP8132A_PINCTRL_PINSTATE_HIGH 1
#define OCP8132A_PINCTRL_STATE_XXX_HIGH "flashlights_on"
#define OCP8132A_PINCTRL_STATE_XXX_LOW  "flashlights_off"
static struct pinctrl *ocp8132a_pinctrl;
static struct pinctrl_state *ocp8132a_xxx_high;
static struct pinctrl_state *ocp8132a_xxx_low;

#define OCP8132A_PINCTRL_PIN_MAIN 1
#define OCP8132A_PINCTRL_MAIN_PINSTATE_LOW 0
#define OCP8132A_PINCTRL_MAIN_PINSTATE_HIGH 1
#define OCP8132A_PINCTRL_STATE_MAIN_HIGH "flashlights_on_main"
#define OCP8132A_PINCTRL_STATE_MAIN_LOW  "flashlights_off_main"
static struct pinctrl_state *ocp8132a_main_high;
static struct pinctrl_state *ocp8132a_main_low;

/* define usage count */
static int use_count;

/* platform data */
struct ocp8132a_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int ocp8132a_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	ocp8132a_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ocp8132a_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(ocp8132a_pinctrl);
		return ret;
	}

	/* TODO: Flashlight XXX pin initialization */
	ocp8132a_xxx_high = pinctrl_lookup_state(
			ocp8132a_pinctrl, OCP8132A_PINCTRL_STATE_XXX_HIGH);
	if (IS_ERR(ocp8132a_xxx_high)) {
		pr_err("Failed to init (%s)\n", OCP8132A_PINCTRL_STATE_XXX_HIGH);
		ret = PTR_ERR(ocp8132a_xxx_high);
	}
	ocp8132a_xxx_low = pinctrl_lookup_state(
			ocp8132a_pinctrl, OCP8132A_PINCTRL_STATE_XXX_LOW);
	if (IS_ERR(ocp8132a_xxx_low)) {
		pr_err("Failed to init (%s)\n", OCP8132A_PINCTRL_STATE_XXX_LOW);
		ret = PTR_ERR(ocp8132a_xxx_low);
	}

	ocp8132a_main_high = pinctrl_lookup_state(
			ocp8132a_pinctrl, OCP8132A_PINCTRL_STATE_MAIN_HIGH);
	if (IS_ERR(ocp8132a_main_high)) {
		pr_err("Failed to init (%s)\n", OCP8132A_PINCTRL_STATE_MAIN_HIGH);
		ret = PTR_ERR(ocp8132a_xxx_high);
	}
	ocp8132a_main_low = pinctrl_lookup_state(
			ocp8132a_pinctrl, OCP8132A_PINCTRL_STATE_MAIN_LOW);
	if (IS_ERR(ocp8132a_main_low)) {
		pr_err("Failed to init (%s)\n", OCP8132A_PINCTRL_STATE_MAIN_LOW);
		ret = PTR_ERR(ocp8132a_xxx_low);
	}

	return ret;
}

static int ocp8132a_pinctrl_set(int pin, int state)
{
	int ret = 0;

	LOG_INF("+++pin:%d, state:%d. \n", pin, state);
	if (IS_ERR(ocp8132a_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case OCP8132A_PINCTRL_PIN_XXX:
		if (state == OCP8132A_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(ocp8132a_xxx_low))
			pinctrl_select_state(ocp8132a_pinctrl, ocp8132a_xxx_low);
		else if (state == OCP8132A_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8132a_xxx_high))
			pinctrl_select_state(ocp8132a_pinctrl, ocp8132a_xxx_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	case OCP8132A_PINCTRL_PIN_MAIN:
		if (state == OCP8132A_PINCTRL_MAIN_PINSTATE_LOW &&
				!IS_ERR(ocp8132a_main_low))
			pinctrl_select_state(ocp8132a_pinctrl, ocp8132a_main_low);
		else if (state == OCP8132A_PINCTRL_MAIN_PINSTATE_HIGH &&
				!IS_ERR(ocp8132a_main_high))
			pinctrl_select_state(ocp8132a_pinctrl, ocp8132a_main_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_debug("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * dummy operations
 *****************************************************************************/
/* flashlight enable function */
static int ocp8132a_enable(void)
{
	//int pin = 0, state = 1;

	/* TODO: wrap enable function */
	LOG_ERR("mainFlashFlag:%d.\n", mainFlashFlag);
	if (mainFlashFlag) {
		ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_MAIN, OCP8132A_PINCTRL_MAIN_PINSTATE_HIGH);
		//+bug687909, liuxiangyin.wt, ADD, 2021/8/31, fix the main flash current problem of the flash unit
		ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_XXX, OCP8132A_PINCTRL_PINSTATE_LOW);
		//-bug687909, liuxiangyin.wt, ADD, 2021/8/31, fix the main flash current problem of the flash unit
	} else {
		ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_MAIN, OCP8132A_PINCTRL_MAIN_PINSTATE_LOW);
		ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_XXX, OCP8132A_PINCTRL_PINSTATE_HIGH);
	}
	return 0;
}

/* flashlight disable function */
static int ocp8132a_disable(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap disable function */
	ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_MAIN, OCP8132A_PINCTRL_MAIN_PINSTATE_LOW);
	ocp8132a_pinctrl_set(OCP8132A_PINCTRL_PIN_XXX, OCP8132A_PINCTRL_PINSTATE_LOW);
	return 0;
}

/* set flashlight level */
static int ocp8132a_set_level(int level)
{
	//int pin = 0, state = 0;

	/* TODO: wrap set level function */

	//return ocp8132a_pinctrl_set(pin, state);
        if (level == 1)
            mainFlashFlag = 1;
        else if (level == 0)
            mainFlashFlag  = 0;

    LOG_INF("level:%d, mainFlashFlag:%d.\n", level, mainFlashFlag);
	return 0;
}

/* flashlight init */
static int ocp8132a_init(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap init function */

	//return ocp8132a_pinctrl_set(pin, state);
	return 0;
}

/* flashlight uninit */
static int ocp8132a_uninit(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap uninit function */

	//return ocp8132a_pinctrl_set(pin, state);
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer ocp8132a_timer;
static unsigned int ocp8132a_timeout_ms;

static void ocp8132a_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	ocp8132a_disable();
}

static enum hrtimer_restart ocp8132a_timer_func(struct hrtimer *timer)
{
	schedule_work(&ocp8132a_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int ocp8132a_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	//ktime_t ktime;
	//unsigned int s;
	//unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		LOG_INF("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		ocp8132a_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		LOG_INF("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		ocp8132a_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		LOG_INF("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			/*
			if (ocp8132a_timeout_ms) {
				s = ocp8132a_timeout_ms / 1000;
				ns = ocp8132a_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&ocp8132a_timer, ktime,
						HRTIMER_MODE_REL);
			}*/
			ocp8132a_enable();
		} else {
			ocp8132a_disable();
			//hrtimer_cancel(&ocp8132a_timer);
		}
		break;
	default:
		LOG_INF("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ocp8132a_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8132a_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8132a_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&ocp8132a_mutex);
	if (set) {
		if (!use_count)
			ret = ocp8132a_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = ocp8132a_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&ocp8132a_mutex);

	return ret;
}

static ssize_t ocp8132a_strobe_store(struct flashlight_arg arg)
{
	ocp8132a_set_driver(1);
	ocp8132a_set_level(arg.level);
	ocp8132a_timeout_ms = 0;
	ocp8132a_enable();
	msleep(arg.dur);
	ocp8132a_disable();
	ocp8132a_set_driver(0);

	return 0;
}

static struct flashlight_operations ocp8132a_ops = {
	ocp8132a_open,
	ocp8132a_release,
	ocp8132a_ioctl,
	ocp8132a_strobe_store,
	ocp8132a_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int ocp8132a_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * ocp8132a_init();
	 */

	return 0;
}

static int ocp8132a_parse_dt(struct device *dev,
		struct ocp8132a_platform_data *pdata)
{
	return 0;
}

static ssize_t led_flash_show(struct device *dev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "%d\n", 0);
}
static ssize_t led_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size){
	unsigned long value;
	int err;
	err = kstrtoul(buf, 10, &value);
	if(err != 0){
		return err;
	}
        LOG_INF("value:%ld. \n", value);
	switch(value){
		case 0: //off
                        err = ocp8132a_disable();
			if(err < 0)
				LOG_ERR("AAA - error1 - AAA\n");
			break;
		case 1: //on
                        err = ocp8132a_enable();
			if(err < 0)
				LOG_ERR("AAA - error2 - AAA\n");
			break;
		default :
			LOG_ERR("AAA - error3 - AAA\n");
			break;
	}
	return 1;
}
static DEVICE_ATTR(led_flash, 0664, led_flash_show, led_flash_store);


static int ocp8132a_probe(struct platform_device *pdev)
{
	struct ocp8132a_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err, ret;
	int i;

	LOG_INF("Probe start.\n");

	/* init pinctrl */
	if (ocp8132a_pinctrl_init(pdev)) {
		pr_debug("Failed to init pinctrl.\n");
		err = -EFAULT;
		goto err;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err;
		}
		pdev->dev.platform_data = pdata;
		err = ocp8132a_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&ocp8132a_work, ocp8132a_work_disable);

	/* init timer */
	hrtimer_init(&ocp8132a_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp8132a_timer.function = ocp8132a_timer_func;
	ocp8132a_timeout_ms = 100;

	/* init chip hw */
	ocp8132a_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&ocp8132a_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(OCP8132A_NAME, &ocp8132a_ops)) {
			err = -EFAULT;
			goto err;
		}
	}
	//add file node.
	ret = device_create_file(&pdev->dev, &dev_attr_led_flash);
	if(ret < 0){
		pr_err("=== create led_flash_node file failed ===\n");
	}

	LOG_INF("Probe done.\n");

	return 0;
err:
	return err;
}

static int ocp8132a_remove(struct platform_device *pdev)
{
	struct ocp8132a_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(OCP8132A_NAME);

	/* flush work queue */
	flush_work(&ocp8132a_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ocp8132a_gpio_of_match[] = {
	{.compatible = OCP8132A_GPIO_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp8132a_gpio_of_match);
#else
static struct platform_device ocp8132a_gpio_platform_device[] = {
	{
		.name = OCP8132A_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ocp8132a_gpio_platform_device);
#endif

static struct platform_driver ocp8132a_platform_driver = {
	.probe = ocp8132a_probe,
	.remove = ocp8132a_remove,
	.driver = {
		.name = OCP8132A_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ocp8132a_gpio_of_match,
#endif
	},
};

static int __init flashlight_ocp8132a_init(void)
{
	int ret;

	LOG_INF("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&ocp8132a_gpio_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&ocp8132a_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	LOG_INF("Init done.\n");

	return 0;
}

static void __exit flashlight_ocp8132a_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&ocp8132a_platform_driver);

	pr_debug("Exit done.\n");
}

late_initcall(flashlight_ocp8132a_init);
module_exit(flashlight_ocp8132a_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris<chenshiqiang@wingtechcom>");
MODULE_DESCRIPTION("MTK Flashlight OCP8132A Driver");
