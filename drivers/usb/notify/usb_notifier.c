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
#include <linux/delay.h>
#include <linux/usb_notify.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
#include <linux/cable_type_notifier.h>
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#include "usb_notifier.h"

#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#if defined(CONFIG_USB_MTK_HDRC)
#include "../../misc/mediatek/usb20/mtk_musb.h"
#include "../../misc/mediatek/usb20/mt6765/usb20.h"
#endif

#if defined(CONFIG_EXTCON_MTK_USB)
#include "../../misc/mediatek/extcon/extcon-mtk-usb.h"
#endif

#if IS_MODULE(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery_common.h>
#elif defined(CONFIG_BATTERY_SAMSUNG)
#include "../../../battery/common/sec_charging_common.h"
#endif

struct usb_notifier_platform_data {
#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
	struct	notifier_block cable_type_nb;
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	struct	notifier_block vbus_nb;
#endif
	int	gpio_redriver_en;
	int can_disable_usb;

	int  host_wake_lock_enable;
	int  device_wake_lock_enable;
};

#ifdef CONFIG_OF
static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	pdata->can_disable_usb =
		!(of_property_read_bool(np, "samsung,unsupport-disable-usb"));
	pr_info("%s, can_disable_usb %d\n", __func__, pdata->can_disable_usb);

	pdata->host_wake_lock_enable =
		!(of_property_read_bool(np, "disable_host_wakelock"));

	pdata->device_wake_lock_enable =
		!(of_property_read_bool(np, "disable_device_wakelock"));

	pr_info("%s, host_wake_lock_enable %d, device_wake_lock_enable %d\n",
		__func__, pdata->host_wake_lock_enable, pdata->device_wake_lock_enable);

	return 0;
}
#endif

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
static int cable_type_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	cable_type_attached_dev_t attached_dev = *(cable_type_attached_dev_t *)data;
	struct otg_notify *o_notify = get_otg_notify();

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case CABLE_TYPE_USB_SDP:
	case CABLE_TYPE_USB_CDP:
		if (action == CABLE_TYPE_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		else if (action == CABLE_TYPE_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case CABLE_TYPE_OTG:
		if (action == CABLE_TYPE_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
		else if (action == CABLE_TYPE_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	return 0;
}
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
static int vbus_handle_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *)data;
	struct otg_notify *o_notify;

	o_notify = get_otg_notify();

	pr_info("%s cmd=%lu, vbus_type=%s\n",
		__func__, cmd, vbus_type == STATUS_VBUS_HIGH ? "HIGH" : "LOW");

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 1);
		break;
	case STATUS_VBUS_LOW:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 0);
		break;
	default:
		break;
	}
	return 0;
}
#endif

static int otg_accessory_power(bool enable)
{
	bool onoff = !!enable;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;

	val.intval = enable;
	psy_do_property("otg", set,
			POWER_SUPPLY_PROP_ONLINE, val);
#elif defined(CONFIG_USB_MTK_HDRC)
	mt_otg_accessory_power(onoff);
#endif
	pr_info("%s : enable=%d\n", __func__, enable);

	return 0;
}

static int mtk_set_host(bool enable)
{
#if defined(CONFIG_EXTCON_MTK_USB) && defined(CONFIG_CABLE_TYPE_NOTIFIER)
	if (enable) {
		pr_info("%s USB_HOST_ATTACHED\n", __func__);
		mtk_usb_notify_set_mode(DUAL_PROP_DR_HOST);
	} else {
		pr_info("%s USB_HOST_DETACHED\n", __func__);
		mtk_usb_notify_set_mode(DUAL_PROP_DR_NONE);
	}
#elif defined(CONFIG_USB_MTK_HDRC)
	if (enable) {
		pr_info("%s USB_HOST_ATTACHED\n", __func__);
		mtk_usb_host_connect();
	} else {
		pr_info("%s USB_HOST_DETACHED\n", __func__);
		mtk_usb_host_disconnect();
	}
#endif
	return 0;
}

static int mtk_set_peripheral(bool enable)
{
#if defined(CONFIG_EXTCON_MTK_USB) && defined(CONFIG_CABLE_TYPE_NOTIFIER)
	if (enable)
		mtk_usb_notify_set_mode(DUAL_PROP_DR_DEVICE);
	else
		mtk_usb_notify_set_mode(DUAL_PROP_DR_NONE);
#elif defined(CONFIG_USB_MTK_HDRC)

	if (enable) {
		pr_info("%s usb attached\n", __func__);
		mtk_usb_connect();
	} else {
		pr_info("%s usb detached\n", __func__);
		mtk_usb_disconnect();
	}
#endif
	return 0;
}

static struct otg_notify dwc_mtk_notify = {
	.vbus_drive	= otg_accessory_power,
	.set_host = mtk_set_host,
	.set_peripheral	= mtk_set_peripheral,
	.vbus_detect_gpio = -1,
	.redriver_en_gpio = -1,
	.is_host_wakelock = 1,
	.is_wakelock = 1,
	.booting_delay_sec = 10,
#if defined(CONFIG_TCPC_CLASS)
	.auto_drive_vbus = NOTIFY_OP_OFF,
#else
	.auto_drive_vbus = NOTIFY_OP_POST,
#endif
	.disable_control = 1,
	.device_check_sec = 3,
	.pre_peri_delay_us = 6,
};

static int usb_notifier_probe(struct platform_device *pdev)
{
	struct usb_notifier_platform_data *pdata = NULL;
	int ret = 0;

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

	dwc_mtk_notify.disable_control = pdata->can_disable_usb;
	dwc_mtk_notify.is_host_wakelock = pdata->host_wake_lock_enable;
	dwc_mtk_notify.is_wakelock = pdata->device_wake_lock_enable;
	set_otg_notify(&dwc_mtk_notify);
	set_notify_data(&dwc_mtk_notify, pdata);

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
	cable_type_notifier_register(&pdata->cable_type_nb, cable_type_handle_notification,
			       CABLE_TYPE_NOTIFY_DEV_USB);
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&pdata->vbus_nb, vbus_handle_notification,
			       VBUS_NOTIFY_DEV_USB);
#endif
	dev_info(&pdev->dev, "usb notifier probe\n");
	return 0;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
	struct usb_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);
#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
	cable_type_notifier_unregister(&pdata->cable_type_nb);
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&pdata->vbus_nb);
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

static void __exit usb_notifier_exit(void)
{
	platform_driver_unregister(&usb_notifier_driver);
}

late_initcall(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_DESCRIPTION("USB notifier");
MODULE_LICENSE("GPL");
