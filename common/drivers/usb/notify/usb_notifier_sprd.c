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
#if defined (CONFIG_EXTCON_SAMSUNG)
#include <linux/extcon/extcon-samsung.h>
#endif

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
	if ( ret < 0 ) {
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
	.booting_delay_sec = 16,
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

int sm5701_booster_enable(bool enable)
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
	{
		.name = "sm5701_booster",
		.booster = sm5701_booster_enable,
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

#ifdef CONFIG_EXTCON_SAMSUNG
extern int get_epmic_event_state(void);
extern int epmic_event_handler(int level);

struct usb_state_t {
	int pre_cable;
	bool b_host;
	bool b_gadget;
} usb_state;

static int sec_cable_notifier(struct notifier_block *nb,
		unsigned long cable_type, void *v)
{
	bool cable_state = 0;
	struct otg_notify *o_notify;
	int epmic_event_state;

	o_notify = get_otg_notify();


	if ((cable_type == CABLE_NONE)
		||(cable_type == CABLE_VBUSTPOWER_OFF_TEST))
		cable_state = 0;
	else
		cable_state = 1;

	pr_info("%s %d is %s\n", __func__, (u8)cable_type,
			cable_state ? "attached" : "detached");

	switch (cable_type) {
	case CABLE_USB:
	case CABLE_JIG_USB_ON:
	case CABLE_JIG_USB_OFF:
	case CABLE_CDP:
#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
		epmic_event_state = get_epmic_event_state();
		if (!epmic_event_state) {
			epmic_event_handler(1);
			pr_info("%s: trigger usb intr 1\n", __func__);
		}
#else
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUS,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_VBUS;
#endif
		break;
	case CABLE_OTG:
		send_otg_notify(o_notify, NOTIFY_EVENT_HOST,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_HOST;
		break;
#if 0
	case CABLE_SMARTDOCK_TA:
		send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_TA,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_SMARTDOCK_TA;
		break;
	case CABLE_SMARTDOCK_USB:
		send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_USB,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_SMARTDOCK_USB;
		break;
	case CABLE_AUDIODOCK:
		send_otg_notify(o_notify, NOTIFY_EVENT_AUDIODOCK,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_AUDIODOCK;
		break;
#endif
	case CABLE_VBUSTPOWER_ON_TEST:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER,
		cable_state);
		break;
	case CABLE_VBUSTPOWER_OFF_TEST:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER,
		cable_state);
		break;
	case CABLE_NONE:
#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
		epmic_event_state = get_epmic_event_state();
		if (epmic_event_state) {
			epmic_event_handler(0);
			pr_info("%s: trigger usb intr 0\n", __func__);
		}
#else
		send_otg_notify(o_notify, usb_state.pre_cable,
			cable_state);
		usb_state.pre_cable = NOTIFY_EVENT_NONE;
#endif
		break;
	default:
		break;
	}
	return 1;
}
#endif


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
			goto end;
		}

		ret = of_usb_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			goto end;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;
#else
	pdata = pdev->dev.platform_data;
#endif

	if (!pdata) {
		dev_err(&pdev->dev, "Failed to get platfom_data\n");
		ret = -EINVAL;
		goto end;
	}
	set_otg_notify(&dwc3_otg_notify);
	set_notify_data(&dwc3_otg_notify, pdata);
	find_and_register_boosters(pdata->booster_name);

#ifdef CONFIG_EXTCON_SAMSUNG
	pdata->usb_nb.notifier_call = sec_cable_notifier;
	usb_switch_register_notify(&pdata->usb_nb);
#endif
end:
	dev_info(&pdev->dev, "%s -\n", __func__);
	return ret;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
#ifdef CONFIG_EXTCON_SAMSUNG
	struct usb_notifier_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	usb_switch_unregister_notify(&pdata->usb_nb);
	devm_kfree(&pdev->dev, (struct usb_notifier_platform_data *) pdata);
#endif
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
