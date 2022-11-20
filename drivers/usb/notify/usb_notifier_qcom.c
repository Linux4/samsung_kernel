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

struct usb_notifier_platform_data {
	struct	notifier_block usb_nb;
	int	gpio_redriver_en;
	bool unsupport_host;
};

enum {
	GADGET_NOTIFIER_DETACH,
	GADGET_NOTIFIER_ATTACH,
	GADGET_NOTIFIER_DEFAULT,
};

extern void sec_otg_set_vbus_state(int online);
extern int sec_set_host(int enable);

static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	pdata->unsupport_host = of_property_read_bool(np, "qcom,unsupport_host");

	pr_info("usb: %s: qcom,un-support-host is %d\n", __func__, pdata->unsupport_host);

	return 0;
}

static int otg_accessory_power(bool enable)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int on = !!enable;
	int current_cable_type;
	int ret = 0;
	/* otg psy test */
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: Fail to get psy battery\n", __func__);
		return -1;
	}

#if defined(CONFIG_MUIC_SM5502_SUPPORT_LANHUB_TA)
	if (enable) {
			current_cable_type = lanhub_ta_case ? POWER_SUPPLY_TYPE_LAN_HUB : POWER_SUPPLY_TYPE_OTG;
			pr_info("%s: LANHUB+TA Case cable type change for the (%d) \n",
									__func__,current_cable_type);
	}
#else
	if (enable)
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
#endif
	else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

	val.intval = current_cable_type;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);

	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		pr_info("otg accessory power = %d\n", on);
	}

	return ret;
}

extern void set_ncm_ready(bool ready);
static int sec_set_peripheral(int enable)
{
	if(!enable)
		set_ncm_ready(false);
	sec_otg_set_vbus_state(enable);

	return 0;
}

static struct otg_notify sec_otg_notify = {
	.vbus_drive	= otg_accessory_power,
	.set_peripheral	= sec_set_peripheral,
	.set_host = sec_set_host,
	.vbus_detect_gpio = -1,
	.is_wakelock = 1,
	.redriver_en_gpio = -1,
	.unsupport_host = 0,
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

	if(pdata->unsupport_host)
		sec_otg_notify.unsupport_host = 1;
	set_otg_notify(&sec_otg_notify);
	set_notify_data(&sec_otg_notify, pdata);

	dev_info(&pdev->dev, "usb notifier probe\n");
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
MODULE_DESCRIPTION("USB notifier");
MODULE_LICENSE("GPL");
