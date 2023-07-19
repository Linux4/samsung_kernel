/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "usb_notifier: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/usb_notify.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/battery/sec_charging_common.h>

struct gadget_notify_dev {
	struct device	*dev;
	int	gadget_state;
	bool	is_ready;
	struct delayed_work	notify_ready_work;
};

struct usb_notifier_platform_data {
	struct	gadget_notify_dev g_ndev;
	struct	notifier_block usb_nb;
	int	gpio_redriver_en;
	int   disable_control_en;
};

enum {
	GADGET_NOTIFIER_DETACH,
	GADGET_NOTIFIER_ATTACH,
	GADGET_NOTIFIER_DEFAULT,
};

extern int sec_set_host(bool enable);

static void of_get_usb_redriver_dt(struct device_node *np,
		struct usb_notifier_platform_data *pdata)
{
	pdata->gpio_redriver_en = of_get_named_gpio(np, "qcom,gpios_redriver_en", 0);
	if (pdata->gpio_redriver_en < 0)
		pr_info("There is no USB 3.0 redriver pin\n");

	if (!gpio_is_valid(pdata->gpio_redriver_en))
		pr_err("%s: usb30_redriver_en: Invalied gpio pins\n", __func__);

	pr_info("redriver_en : %d\n", pdata->gpio_redriver_en);
}

static void of_get_disable_control_en_dt(struct device_node *np,
		struct usb_notifier_platform_data *pdata)
{
	of_property_read_u32(np, "qcom,disable_control_en", &pdata->disable_control_en);

	pr_info("disable_control_en : %d\n", pdata->disable_control_en);
}

static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	of_get_usb_redriver_dt(np, pdata);
	of_get_disable_control_en_dt(np, pdata);
	return 0;
}

static struct usb_notifier_platform_data *of_get_usb_notifier_pdata(void)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct usb_notifier_platform_data *pdata = NULL;

	np = of_find_compatible_node(NULL, NULL, "samsung,usb-notifier");
	if (!np) {
		pr_err("%s: failed to get the usb-notifier device node\n",
				__func__);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: failed to get the usb-notifier platform_device\n",
				__func__);
		return NULL;
	}
	pdata = pdev->dev.platform_data;
	of_node_put(np);

	return pdata;
}

static void check_usb_vbus_state(unsigned long state)
{
}

static void usbgadget_ready(struct work_struct *work)
{
	struct usb_notifier_platform_data *pdata = of_get_usb_notifier_pdata();

	pr_info("usb: %s,gadget_state:%d\n", __func__,
			pdata->g_ndev.gadget_state);
	pdata->g_ndev.is_ready = true;
}

static int otg_accessory_power(bool enable)
{
	struct power_supply *psy_otg, *psy_battery;
	union power_supply_propval val;
	int on = !!enable;
	int current_cable_type;
	int ret = 0;
	pr_info("%s %d, enable=%d\n", __func__, __LINE__, enable);
	/* otg psy test */
	psy_otg = power_supply_get_by_name("otg");
	psy_battery = power_supply_get_by_name("battery");

	if (psy_otg) {
		val.intval = enable;
		ret = psy_otg->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
	} else if (psy_battery) {
		if (enable)
			current_cable_type = POWER_SUPPLY_TYPE_OTG;
		else
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

		val.intval = current_cable_type;
		ret = psy_battery->set_property(psy_battery, POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);
		return -1;
	}
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		pr_info("otg accessory power = %d\n", on);
	}

	return ret;
}

extern void set_ncm_ready(bool ready);
static int qcom_set_peripheral(bool enable)
{
	struct power_supply *psy;
	struct otg_notify *o_notify;
	struct usb_notifier_platform_data *pdata;
	o_notify = get_otg_notify();
	pdata = get_notify_data(o_notify);

	if (!enable) {
		pdata->g_ndev.gadget_state = GADGET_NOTIFIER_DETACH;
		check_usb_vbus_state(pdata->g_ndev.gadget_state);
	} else {
		pdata->g_ndev.gadget_state = GADGET_NOTIFIER_ATTACH;
		check_usb_vbus_state(pdata->g_ndev.gadget_state);
	}

	if(!enable)
		set_ncm_ready(false);

	pr_info("%s %d, enable=%d\n", __func__, __LINE__, enable);
	psy = power_supply_get_by_name("msm-usb");
	power_supply_set_present(psy, enable);

	return 0;
}

static int set_online(int event, int state)
{
	union power_supply_propval value;
	struct power_supply *psy;

	pr_info("set_online: %d, %d\n", event, state);

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: fail to get battery power_supply\n", __func__);
		return -1;
	}

	if (state)
		value.intval = POWER_SUPPLY_TYPE_SMART_OTG;
	else
		value.intval = POWER_SUPPLY_TYPE_SMART_NOTG;

	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	return 0;
}

static struct otg_notify sec_otg_notify = {
	.vbus_drive	= otg_accessory_power,
	.set_peripheral	= qcom_set_peripheral,
	.set_host = sec_set_host,
	.vbus_detect_gpio = -1,
	.is_wakelock = 1,
	.set_battcall = set_online,
};

static int usb_notifier_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct usb_notifier_platform_data *pdata = NULL;

	pr_info("notifier_probe\n");

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct usb_notifier_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = of_usb_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;

	pdata->g_ndev.gadget_state = GADGET_NOTIFIER_DEFAULT;

	/*
	When a device is booted up with usb cable,
	Sometimes you can't show usb icon on device.
	if MUIC notify is called before usb composite is up,
	usb state UEVENT is not happened.
	*/
	INIT_DELAYED_WORK(&pdata->g_ndev.notify_ready_work, usbgadget_ready);
	schedule_delayed_work(&pdata->g_ndev.notify_ready_work,
			msecs_to_jiffies(15000));

	sec_otg_notify.redriver_en_gpio = pdata->gpio_redriver_en;
	if (pdata->disable_control_en == 1)
		sec_otg_notify.disable_control = 1;
	set_otg_notify(&sec_otg_notify);
	set_notify_data(&sec_otg_notify, pdata);

	dev_info(&pdev->dev, "usb notifier probe\n");
	return 0;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
	struct usb_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);
	cancel_delayed_work_sync(&pdata->g_ndev.notify_ready_work);
	return 0;
}

static const struct of_device_id usb_notifier_dt_ids[] = {
	{ .compatible = "samsung,usb-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_notifier_dt_ids);

static struct platform_driver usb_notifier_driver = {
	.probe		= usb_notifier_probe,
	.remove		= usb_notifier_remove,
	.driver		= {
		.name	= "usb_notifier",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(usb_notifier_dt_ids),
	},
};

static int __init usb_notifier_init(void)
{
	return platform_driver_register(&usb_notifier_driver);
}

static void __init usb_notifier_exit(void)
{
	platform_driver_unregister(&usb_notifier_driver);
}

late_initcall(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("USB Notifier");
MODULE_LICENSE("GPL");
