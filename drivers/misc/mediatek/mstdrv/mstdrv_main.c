/*
 * @file mstdrv.c
 * @brief MST drv Support
 * Copyright (c) 2015, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/kern_levels.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/percpu-defs.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/threads.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include "mstdrv_main.h"

/* defines */
#define	ON				1	// On state
#define	OFF				0	// Off state
#define	TRACK1			1	// Track1 data
#define	TRACK2			2	// Track2 data
#define mode_set_wait   3

#define CMD_MST_LDO_OFF			'0'	// MST LDO off
#define CMD_MST_LDO_ON			'1'	// MST LDO on
#define CMD_SEND_TRACK1_DATA		'2'	// Send track1 test data
#define CMD_SEND_TRACK2_DATA		'3'	// send track2 test data
#define ERROR_VALUE			-1	// Error value

/* global variables */
static struct class *mst_drv_class;
struct device *mst_drv_dev;
static int escape_loop = 1;
static int mst_pwr_en;
static int mst_vcc;
static spinlock_t event_lock;

/* function */
/**
 * mst_printk - print with mst tag
 */
int mst_log_level = MST_LOG_LEVEL;
void mst_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	//if (mst_log_level < level)
	//	return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", TAG, &vaf);

	va_end(args);
}

/**
 * of_mst_hw_onoff - Enable/Disable MST LDO GPIO pin (or Regulator)
 * @on: on/off value
 */
static void of_mst_hw_onoff(bool on)
{
	mst_info("%s: on = %d\n", __func__, on);
	if (on) {
		gpio_set_value(mst_vcc, 1);
		gpio_set_value(mst_pwr_en, 1);
		mst_info("%s: mst_pwr_en HIGH\n", __func__);

		cancel_delayed_work_sync(&dwork);
		queue_delayed_work(cluster_freq_ctrl_wq, &dwork, 90 * HZ);

		msleep(mode_set_wait);

	} else {
		gpio_set_value(mst_vcc, 0);
		gpio_set_value(mst_pwr_en, 0);
		mst_info("%s: mst_pwr_en LOW\n", __func__);
		usleep_range(800, 1000);
	}
}

uint32_t transmit_mst_teegris(uint32_t cmd)
{
	TEEC_Context context;
	TEEC_Session session_ta;
	TEEC_Operation operation;
	TEEC_Result ret;

	uint32_t origin;

	origin = 0x0;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
						TEEC_NONE, TEEC_NONE);

	mst_info("%s: TEEC_InitializeContext\n", __func__);
	ret = TEEC_InitializeContext(NULL, &context);
	if (ret != TEEC_SUCCESS) {
		mst_err("%s: InitializeContext failed, %d\n", __func__, ret);
		goto exit;
	}

	mst_info("%s: TEEC_OpenSession\n", __func__);
	ret = TEEC_OpenSession(&context, &session_ta, &uuid_ta, 0,
			       NULL, &operation, &origin);
	if (ret != TEEC_SUCCESS) {
		mst_err("%s: OpenSession(ta) failed, %d\n", __func__, ret);
		goto finalize_context;
	}

	mst_info("%s: TEEC_InvokeCommand (CMD_OPEN)\n", __func__);
	ret = TEEC_InvokeCommand(&session_ta, CMD_OPEN, &operation, &origin);
	if (ret != TEEC_SUCCESS) {
		mst_err("%s: InvokeCommand(OPEN) failed, %d\n", __func__, ret);
		goto ta_close_session;
	}

	/* MST IOCTL - transmit track data */
	mst_info("tracing mark write: MST transmission Start\n");
	mst_info("%s: TEEC_InvokeCommand (TRACK1 or TRACK2)\n", __func__);
	ret = TEEC_InvokeCommand(&session_ta, cmd, &operation, &origin);
	if (ret != TEEC_SUCCESS) {
		mst_err("%s: InvokeCommand failed, %d\n", __func__, ret);
		goto ta_close_session;
	}
	mst_info("tracing mark write: MST transmission End\n");

	if (ret) {
		mst_info("%s: Send track%d data --> failed\n", __func__, cmd-2);
	} else {
		mst_info("%s: Send track%d data --> success\n", __func__, cmd-2);
	}

	mst_info("%s: TEEC_InvokeCommand (CMD_CLOSE)\n", __func__);
	ret = TEEC_InvokeCommand(&session_ta, CMD_CLOSE, &operation, &origin);
	if (ret != TEEC_SUCCESS) {
		mst_err("%s: InvokeCommand(CLOSE) failed, %d\n", __func__, ret);
		goto ta_close_session;
	}

ta_close_session:
	TEEC_CloseSession(&session_ta);
finalize_context:
	TEEC_FinalizeContext(&context);
exit:
	return ret;
}

/**
 * show_mst_drv - device attribute show sysfs operation
 */
static ssize_t show_mst_drv(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	if (!dev)
		return -ENODEV;

	if (escape_loop == 0) {
		return sprintf(buf, "%s\n", "activating");
	} else {
		return sprintf(buf, "%s\n", "waiting");
	}
}

/**
 * store_mst_drv - device attribute store sysfs operation
 */
static ssize_t store_mst_drv(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	char in = 0;
	int ret = 0;

	sscanf(buf, "%c\n", &in);
	mst_info("%s: in = %c\n", __func__, in);

	switch (in) {
	case CMD_MST_LDO_OFF:
		of_mst_hw_onoff(OFF);
		break;

	case CMD_MST_LDO_ON:
		of_mst_hw_onoff(ON);
		break;

	case CMD_SEND_TRACK1_DATA:
		of_mst_hw_onoff(ON);

		mst_info("%s: send track1 data\n", __func__);

		ret = transmit_mst_teegris(CMD_TRACK1);

		of_mst_hw_onoff(0);

		break;

	case CMD_SEND_TRACK2_DATA:
		of_mst_hw_onoff(1);

		mst_info("%s: send track2 data\n", __func__);

		ret = transmit_mst_teegris(CMD_TRACK2);

		of_mst_hw_onoff(0);

		break;

	default:
		mst_err("%s: invalid value : %c\n", __func__, in);
		break;
	}
	return count;
}
static DEVICE_ATTR(transmit, 0770, show_mst_drv, store_mst_drv);

/* support node */
static ssize_t show_support(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	if (!dev)
		return -ENODEV;

	mst_info("%s: MST_LDO enabled, support MST\n", __func__);
	return sprintf(buf, "%d\n", 1);
}

static ssize_t store_support(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	return count;
}
static DEVICE_ATTR(support, 0444, show_support, store_support);

/**
 * sec_mst_gpio_init - Initialize GPIO pins used by driver
 * @dev: driver handle
 */
static int sec_mst_gpio_init(struct device *dev)
{
	int ret = 0;

	mst_pwr_en = of_get_named_gpio(dev->of_node, "sec-mst,mst-pwr-gpio", 0);

	mst_info("%s: Data Value : %d\n", __func__, mst_pwr_en);

	/* check if gpio pin is inited */
	if (mst_pwr_en < 0) {
		mst_err("%s: fail to get pwr gpio, %d\n", __func__, mst_pwr_en);
		return 1;
	}
	mst_info("%s: gpio pwr inited\n", __func__);

	/* gpio request */
	ret = gpio_request(mst_pwr_en, "sec-mst,mst-pwr-gpio");
	if (ret) {
		mst_err("%s: failed to request pwr gpio, %d, %d\n",
			__func__, ret, mst_pwr_en);
	}

	/* set gpio direction */
	if (!(ret < 0) && (mst_pwr_en > 0)) {
		gpio_direction_output(mst_pwr_en, 0);
		mst_info("%s: mst_pwr_en output\n", __func__);
	}
	mst_vcc = of_get_named_gpio(dev->of_node, "sec-mst,mst-vcc-gpio", 0);
	mst_info("%s: Data Value mst_vcc : %d\n", __func__, mst_vcc);

	/* check if gpio pin is available */
	if (mst_vcc < 0) {
		mst_err("%s: fail to get vcc gpio, %d\n", __func__, mst_vcc);
		return 1;
	}
	mst_info("%s: gpio vcc inited\n", __func__);

	/* gpio request */
	ret = gpio_request(mst_vcc, "sec-mst,mst-vcc-gpio");
	if (ret) {
		mst_err("%s: failed to request data gpio, %d, %d\n",
			__func__, ret, mst_vcc);
	}
	/* set gpio direction */
	if (!(ret < 0)  && (mst_vcc > 0)) {
		gpio_direction_output(mst_vcc, 0);
		mst_info("%s: mst_vcc output\n", __func__);
	}

	return 0;
}

static int mst_ldo_device_probe(struct platform_device *pdev)
{
	int retval = 0;

	mst_info("%s: probe start\n", __func__);
	spin_lock_init(&event_lock);

	if (sec_mst_gpio_init(&pdev->dev))
		return -1;

	mst_info("%s: create sysfs node\n", __func__);
	mst_drv_class = class_create(THIS_MODULE, "mstldo");
	if (IS_ERR(mst_drv_class)) {
		retval = PTR_ERR(mst_drv_class);
		goto error;
	}

	mst_drv_dev = device_create(mst_drv_class,
				    NULL /* parent */, 0 /* dev_t */,
				    NULL /* drvdata */,
				    MST_DRV_DEV);
	if (IS_ERR(mst_drv_dev)) {
		retval = PTR_ERR(mst_drv_dev);
		goto error_destroy;
	}

	/* register this mst device with the driver core */
	retval = device_create_file(mst_drv_dev, &dev_attr_transmit);
	if (retval)
		goto error_destroy;

	retval = device_create_file(mst_drv_dev, &dev_attr_support);
	if (retval)
		goto error_destroy;

	mst_info("%s: MST driver(%s) initialized\n", __func__, MST_DRV_DEV);
	return 0;

error_destroy:
	kfree(mst_drv_dev);
	device_destroy(mst_drv_class, 0);
error:
	mst_info("%s: MST driver(%s) init failed\n", __func__, MST_DRV_DEV);
	return retval;
}
EXPORT_SYMBOL_GPL(mst_drv_dev);

/**
 * device suspend - function called device goes suspend
 */
static int mst_ldo_device_suspend(struct platform_device *dev,
				  pm_message_t state)
{
	uint8_t is_mst_pwr_on;

	is_mst_pwr_on = gpio_get_value(mst_pwr_en);
	if (is_mst_pwr_on == 1) {
		mst_info("%s: mst power is on, %d\n", __func__, is_mst_pwr_on);
		of_mst_hw_onoff(0);
		mst_info("%s: mst power off\n", __func__);
	} else {
		mst_info("%s: mst power is off, %d\n", __func__, is_mst_pwr_on);
	}
	return 0;
}

static struct of_device_id mst_match_ldo_table[] = {
	{.compatible = "sec-mst",},
	{},
};

static struct platform_driver sec_mst_ldo_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mstldo",
		.of_match_table = mst_match_ldo_table,
	},
	.probe = mst_ldo_device_probe,
	.suspend = mst_ldo_device_suspend,
};

/**
 * mst delayed work
 */
static void mst_cluster_freq_ctrl_worker(struct work_struct *work)
{
	uint8_t is_mst_pwr_on;

	is_mst_pwr_on = gpio_get_value(mst_pwr_en);
	if (is_mst_pwr_on == 1) {
		mst_info("%s: mst power is on, %d\n", __func__, is_mst_pwr_on);
		of_mst_hw_onoff(0);
		mst_info("%s: mst power off\n", __func__);
	} else {
		mst_info("%s: mst power is off, %d\n", __func__, is_mst_pwr_on);
	}
	return;
}

/**
 * mst_drv_init - Driver init function
 */
static int __init mst_drv_init(void)
{
	int ret = 0;

	mst_info("%s\n", __func__);
	ret = platform_driver_register(&sec_mst_ldo_driver);
	mst_info("%s: init , ret : %d\n", __func__, ret);

	cluster_freq_ctrl_wq =
		create_singlethread_workqueue("mst_cluster_freq_ctrl_wq");
	INIT_DELAYED_WORK(&dwork, mst_cluster_freq_ctrl_worker);

	return ret;
}

/**
 * mst_drv_exit - Driver exit function
 */
static void __exit mst_drv_exit(void)
{
	class_destroy(mst_drv_class);

	mst_info("%s\n", __func__);
}

MODULE_AUTHOR("yurak.choe@samsung.com");
MODULE_DESCRIPTION("MST QC/LSI combined driver");
MODULE_VERSION("1.0");
late_initcall(mst_drv_init);
module_exit(mst_drv_exit);
MODULE_LICENSE("GPL v2");
