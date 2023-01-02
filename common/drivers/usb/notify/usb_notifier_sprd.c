/*
 * Copyright (C) 2011 Samsung Electronics Co. Ltd.
 *  Inchul Im <inchul.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define pr_fmt(fmt) "usb_notifier: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/usb_notifier.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/power_supply.h>

struct otg_func {
	int (*start_host) (void *, bool);
	int (*start_peripheral) (void *, bool);
	void *data;
};

static struct otg_func otg_f;

#ifdef CONFIG_OF
static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	int gpio = 0;

	if (!np) {
		ret = -EINVAL;
		goto err;
	}

	ret = of_property_read_string(np, "booster-name",
			 (const char **)&pdata->booster_name);
	if (ret) {
		pr_err("%s, Not found booster name\n", __func__);
		goto err;
	}

	gpio = of_get_named_gpio(np,
			"booster-gpio", 0);
	pdata->gpio_booster = gpio;
	if (!gpio_is_valid(gpio))
			pr_err("%s, cannot get booster-gpio, ret=%d\n",
					__func__, gpio);
	else {
		pr_info("%s, booster-gpio=%d\n", __func__, gpio);
		ret = gpio_request(gpio, "gpio_booster");
		if (ret < 0) {
			pr_err("%s, gpio_booster gpio_request failed, ret=%d\n",
					__func__, gpio);
			goto err;
		}
		gpio_direction_output(gpio, 0);
	}
err:
	return ret;
}
#endif

static int dwc3_pregpio(int gpio, int use)
{
	pr_info("%s use=%s\n", __func__, use ? "REDRIVER" : "VBUS");
	return 0;
}

static int dwc3_vbus_drive(bool enable)
{
	struct otg_booster *o_b;
	o_b = find_get_booster();
	if (!o_b || !o_b->booster)
		return -EFAULT;
	pr_info("booster-%s %s\n", o_b->name, enable ? "ON" : "OFF");
	return o_b->booster(enable);
}

static int dwc3_set_host(bool enable)
{
	pr_info("%s+ enable=%d\n", __func__, enable);
	if (otg_f.start_host && otg_f.data)
		otg_f.start_host(otg_f.data, enable);
	else
		pr_err("%s start_host or data is null\n", __func__);
	pr_info("%s-\n", __func__);
	return 0;
}

static int dwc3_set_peripheral(bool enable)
{
	pr_info("%s+ enable=%d\n", __func__, enable);
	if (otg_f.start_peripheral && otg_f.data)
		otg_f.start_peripheral(otg_f.data, enable);
	else
		pr_err("%s start_peripheral or data is null\n", __func__);
	pr_info("%s-\n", __func__);
	return 0;
}

static struct otg_notify dwc3_otg_notify = {
	.pre_gpio = dwc3_pregpio,
	.vbus_drive = dwc3_vbus_drive,
	.set_host	= dwc3_set_host,
	.set_peripheral = dwc3_set_peripheral,
	.vbus_detect_gpio = -1,
	.redriver_en_gpio = -1,
	.is_wakelock = 1,
	.auto_drive_vbus = 1,
};

int tps61256_booster_enable(bool enable)
{
	struct otg_notify *notify = get_otg_notify();
	struct usb_notifier_platform_data *pdata
			= get_notify_data(notify);
	int ret = 0;
	int gpio = 0;

	if (!pdata) {
		pr_err("%s, pdata is NULL\n", __func__);
		ret = -ENOENT;
		goto err;
	}

	gpio = pdata->gpio_booster;

	pr_info("%s, enable=%d\n", __func__, enable);
	if (gpio_is_valid(gpio))
		gpio_set_value(gpio, enable);
	else
		pr_err("%s, gpio %d is invalid\n", __func__, gpio);
err:
	return ret;
}

int smb328_booster_enable(bool enable)
{
	struct power_supply *psy
		= power_supply_get_by_name("sec-charger");
	union power_supply_propval value;
	int ret = 0;

	if (!psy) {
		pr_err("%s, psy is NULL\n", __func__);
		ret = -ENOENT;
		goto err;
	}
	if (enable)
		value.intval = POWER_SUPPLY_TYPE_OTG;
	else
		value.intval = POWER_SUPPLY_TYPE_BATTERY;

	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
err:
	return ret;
}

static struct otg_booster otg_boosters[] = {
	{
		.name = "tps61256_booster",
		.booster = tps61256_booster_enable,
	},
	{
		.name = "smb328_booster",
		.booster = smb328_booster_enable,
	},
};

static void find_and_register_boosters(char *b_name)
{
	int ret = 0;
	int i = 0;

	pr_info("%s, booster-name=%s\n", __func__, b_name);

	for (i = 0; i < ARRAY_SIZE(otg_boosters); i++) {
		if (!strcmp(otg_boosters[i].name, b_name))
			break;
	}

	if (i == ARRAY_SIZE(otg_boosters)) {
		pr_err("%s, No matching booster\n", __func__);
		return;
	}

	ret = register_booster(&otg_boosters[i]);
	if (ret)
		pr_err("%s, failed. ret=%d\n", __func__, ret);
}

/*
 *	The register_otg_func function is called by dwc driver.
 */
void register_otg_func(int (*host)(void *, bool),
			int (*peripheral)(void *, bool), void *data)
{
	pr_info("%s\n", __func__);
	if (host)
		otg_f.start_host = host;
	if (peripheral)
		otg_f.start_peripheral = peripheral;
	if (data)
		otg_f.data = data;
}
EXPORT_SYMBOL(register_otg_func);

static int usb_notifier_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct usb_notifier_platform_data *pdata = NULL;

	dev_info(&pdev->dev, "%s +\n", __func__);
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct usb_notifier_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
		}

		ret = of_usb_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;
#else
	pdata = pdev->dev.platform_data;
#endif

	if (!pdata) {
		dev_err(&pdev->dev, "Failed to get platfom_data\n");
		goto err;
	}
	set_otg_notify(&dwc3_otg_notify);
	set_notify_data(&dwc3_otg_notify, pdata);
	find_and_register_boosters(pdata->booster_name);
err:
	dev_info(&pdev->dev, "%s -\n", __func__);
	return 0;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id usb_notifier_dt_ids[] = {
	{ .compatible = "samsung,usb-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_notifier_dt_ids);
#endif

static struct platform_driver usb_notifier_driver = {
	.probe		= usb_notifier_probe,
	.remove		= usb_notifier_remove,
	.driver		= {
		.name	= "usb_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(usb_notifier_dt_ids),
#endif
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

module_init(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_AUTHOR("Samsung usb team");
MODULE_DESCRIPTION("USB notifier");
MODULE_LICENSE("GPL");
