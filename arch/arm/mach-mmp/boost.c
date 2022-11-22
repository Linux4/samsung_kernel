/*
 *  Input touch boost feature support
 *
 *  Copyright (c) 2012 Marvell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif

/* boost cpu to max frequency 200ms by default */
#define CPU_BOOST_TIME		100000
static struct pm_qos_request touchboost_cpu_qos_min = {
	.name = "touch_boost",
};
static struct pm_qos_request touchboost_ddr_qos_min = {
	.name = "touch_boost",
};
static struct pm_qos_request touchboost_gpu2d_qos_min = {
	.name = "touch_boost",
};
static struct pm_qos_request touchboost_gpu3d_qos_min = {
	.name = "touch_boost",
};

static unsigned int touch_boost_enabled = 1;
static struct work_struct touchboost_wk;

/*
 * The reason why we use workqueue here is input event report handler
 * is called in irq disable content, but core freq-chg related function
 * cpufreq_notify_transition request irq is enabled.
 * Call chain as below:
 * input_event(irq disable here) -> input_handle_event-> input_pass_event->
 * handler->event(handle, type, code, value) = touchboost_event->
 * touchboost_work
 */
static void touchboost_work(struct work_struct *w)
{

	pm_qos_update_request_timeout(&touchboost_cpu_qos_min,
			LONG_MAX, CPU_BOOST_TIME);
	pm_qos_update_request_timeout(&touchboost_ddr_qos_min,
			DDR_CONSTRAINT_LVL2, CPU_BOOST_TIME);
	/* boost gpu0(3D) to max frequency 624000KHz*/
	pm_qos_update_request_timeout(&touchboost_gpu3d_qos_min,
			624000, CPU_BOOST_TIME);
	/* boost gpu1(2D) to max frequency 416000KHz*/
	pm_qos_update_request_timeout(&touchboost_gpu2d_qos_min,
			416000, CPU_BOOST_TIME);

}

static void keyboost_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if (touch_boost_enabled && type == EV_KEY && value == 1)
		schedule_work(&touchboost_wk);
}

static void touchboost_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if (touch_boost_enabled && code == ABS_MT_TRACKING_ID && value != 0xffffffff)
		schedule_work(&touchboost_wk);
}

static int touchboost_connect(struct input_handler *handler,
				  struct input_dev *dev,
				  const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	pr_info("%s: connect to %s\n", __func__, dev->name);
	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "touch_boost";

	error = input_register_handle(handle);
	if (error) {
		pr_err("Failed to register touch boost handler, error %d\n",
		       error);
		goto err;
	}

	error = input_open_device(handle);
	if (error) {
		pr_err("Failed to open touch boost device, error %d\n", error);
		input_unregister_handle(handle);
		goto err;
	}

	return 0;
err:
	kfree(handle);
	return error;
}

static void touchboost_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id touchboost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
		       BIT_MASK(ABS_MT_POSITION_X) |
		       BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
		       BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	}, /* touchpad */
	{ },
};

static const struct input_device_id keyboost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY)},
	}, /* keyboard */
};

static struct input_handler touchboost_handler = {
	.event =	touchboost_event,
	.connect =	touchboost_connect,
	.disconnect =	touchboost_disconnect,
	.name =		"touch_boost",
	.id_table =	touchboost_ids,
};

static struct input_handler keyboost_handler = {
	.event =	keyboost_event,
	.connect =	touchboost_connect,
	.disconnect =	touchboost_disconnect,
	.name =		"key_boost",
	.id_table =	keyboost_ids,
};

static int __init boost_init(void)
{
	int ret;
	pm_qos_add_request(&touchboost_cpu_qos_min,
		PM_QOS_CPUFREQ_MIN, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&touchboost_ddr_qos_min,
		   PM_QOS_DDR_DEVFREQ_MIN, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&touchboost_gpu3d_qos_min,
		   PM_QOS_GPUFREQ_3D_MIN, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&touchboost_gpu2d_qos_min,
		   PM_QOS_GPUFREQ_2D_MIN, PM_QOS_DEFAULT_VALUE);
	INIT_WORK(&touchboost_wk, touchboost_work);
#ifdef CONFIG_DEBUG_FS
	debugfs_create_u32("touchbst_enable", 0644, NULL,
		&touch_boost_enabled);
#endif
	ret = input_register_handler(&touchboost_handler);
	if (ret)
		pr_err("touchboost_handler register failed");
	return input_register_handler(&keyboost_handler);
}

static void __exit boost_exit(void)
{
	flush_work(&touchboost_wk);
	input_unregister_handler(&touchboost_handler);
	pm_qos_remove_request(&touchboost_cpu_qos_min);
}

module_init(boost_init);
module_exit(boost_exit);

MODULE_DESCRIPTION("Input touch boost feature support for Marvell MMP");
MODULE_LICENSE("GPL");
