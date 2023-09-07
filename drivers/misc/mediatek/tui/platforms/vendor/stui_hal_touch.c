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

#include <linux/console.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/stui_inf.h>

#include "stui_core.h"
#include "stui_hal.h"
#include "tui_platform.h"


static const char i2c_dev_name[] = "6-0049";	/* TODO: Set appropriate touch device driver name*/
static struct device *local_dev;
static void *local_int_data;
static bool local_irq_enabled;
static atomic_t touch_requested;
static struct notifier_block pm_notifier;
static bool pm_get_parent_ok;



static int do_pm_notify(struct notifier_block *b, unsigned long ev, void *p)
{
	(void)b;
	(void)p;
	pr_notice("[STUI] Device power event occurred. ev=%lu\n", ev);
	if (ev == PM_SUSPEND_PREPARE ||
	    ev == PM_HIBERNATION_PREPARE) {
		/* release_touch(); */
	}
	return NOTIFY_DONE;
}



/* Get pointer to the touch driver on i2c bus */
static struct device *get_touch_device(const char *name)
{
	struct device *dev = NULL;

	dev = bus_find_device_by_name(&i2c_bus_type, NULL, name);
	return dev;
}

/* Get pointer to i2c adapter connected to touch controller */
static struct i2c_adapter *get_i2c_adapter(void *touch_int_data)
{
	struct i2c_adapter *i2c_adapt = (void *)0xDEADBEAF; /* TODO: NULL;*/

	/* TODO:
	 * Place your custom way to get i2c adapter from the touch
	 * internal data structure
	if (touch_int_data && touch_int_data->client)
		i2c_adapt = touch_int_data->client->adapter;
	 */

	if (!i2c_adapt)
		pr_notice("[STUI] I2C adapter is NULL.\n");

	return i2c_adapt;
}

/* Find touch driver by name and lock its using on the linux */
static int __maybe_unused request_touch(void)
{
	int retval = -EFAULT, ret;
	struct device *dev = NULL;
	void *touch_int_data;
	struct i2c_adapter *adapt = NULL;
	(void)local_irq_enabled;

	pr_debug("[STUI] %s() : Enter\n", __func__);
	if (atomic_read(&touch_requested) == 1)
		return -EALREADY;

	dev = get_touch_device(i2c_dev_name);
	if (!dev) {
		pr_notice("[STUI] Touch device %s not found.\n", i2c_dev_name);
		return -ENXIO;
	}
	touch_int_data = dev_get_drvdata(dev);
	if (!touch_int_data) {
		pr_notice("[STUI] Touchscreen driver internal data is empty.\n");
		return -EFAULT;
	}

	adapt = get_i2c_adapter(touch_int_data);
	if (adapt)
		i2c_lock_adapter(adapt);

	pm_get_parent_ok = true;
	ret = pm_runtime_get_sync(to_i2c_client(dev)->adapter->dev.parent);
	if (ret < 0) {
		pr_notice("[STUI] pm_runtime_get parent error: %d\n", ret);
		pm_get_parent_ok = false;
	}

	/* TODO:
	 * Perform all necessary steps to lock touch driver
	 * and appropriate i2c channel from the linux side, for example:

	mutex_lock(&touch_int_data->device_mutex);
	disable_irq(touch_int_data->irq);
	local_irq_enabled = touch_int_data->irq_enabled;
	local_dev = dev;
	local_int_data = touch_int_data;

	 */

	memset(&pm_notifier, 0, sizeof(pm_notifier));
	pm_notifier.notifier_call = do_pm_notify;
	pm_notifier.priority = 0;
	retval = register_pm_notifier(&pm_notifier);

	if (retval < 0) {
		pr_notice("[STUI] register_pm_notifier: retval=%d\n", retval);
		goto exit_err2;
	}

	atomic_set(&touch_requested, 1);
	pr_debug("[STUI] Touch requested\n");

	/* TODO: mutex_unlock(&touch_int_data->device_mutex);*/
	return 0;

exit_err2:
	if (pm_get_parent_ok) {
		pm_runtime_put_sync(to_i2c_client(dev)->adapter->dev.parent);
		pm_get_parent_ok = false;
	}

/* TODO:
*	if (local_irq_enabled) {
*		enable_irq(touch_int_data->irq);
*		touch_int_data->irq_enabled = true;
*	}
*/

	if (adapt)
		i2c_unlock_adapter(adapt);

	/* TODO: mutex_unlock(&touch_int_data->device_mutex);*/
	local_dev = NULL;
	local_int_data = NULL;
	return retval;
}

/* Release touch driver and prepare it to normal work */
static int __maybe_unused release_touch(void)
{
	int retval = -EFAULT;
	void *touch_int_data = local_int_data;
	struct i2c_adapter *adapt = NULL;

	pr_debug("[STUI] %s() : Enter\n", __func__);
	if (atomic_read(&touch_requested) != 1)
		return -EALREADY;

	adapt = get_i2c_adapter(touch_int_data);

	/* TODO:
	 * Perform all necessary steps to release touch driver
	 * and appropriate i2c channel and prepare to normal work, for example:

	mutex_lock(&touch_int_data->device_mutex);

	if (local_irq_enabled) {
		enable_irq(touch_int_data->irq);
		touch_int_data->irq_enabled = true;
		msleep(10);
		pr_notice("[STUI] touch irq enabled\n");
	}

	*/

	if (pm_get_parent_ok) {
		retval = pm_runtime_put_sync(to_i2c_client(local_dev)->adapter->dev.parent);
		if (retval < 0)
			pr_notice("[STUI] pm_runtime_put parent failed:%d\n", retval);

		pm_get_parent_ok = false;
	}

	retval = unregister_pm_notifier(&pm_notifier);
	if (adapt)
		i2c_unlock_adapter(adapt);

	atomic_set(&touch_requested, 0);
	/* TODO: mutex_unlock(&touch_int_data->device_mutex);*/
	return retval;
}


int stui_i2c_protect(bool is_protect)
{
	int ret = 0;

	pr_debug("[STUI] stui_i2c_protect(%s) called\n", is_protect?"true":"false");

#ifdef TUI_ENABLE_TOUCH
	if (is_protect) {
		ret = tpd_enter_tui();
		ret = i2c_tui_enable_clock(0);
	} else {
		ret = tpd_exit_tui();
		ret = i2c_tui_disable_clock(0);
	}
#else
	if (is_protect)
		ret = request_touch();
	else
		ret = release_touch();
#endif
	return ret;
}

