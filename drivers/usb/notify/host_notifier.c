/*
 * Copyright (C) 2011-2013 Samsung Electronics Co. Ltd.
 *  Hyuk Kang <hyuk78.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>
#include <linux/host_notify.h>
#ifdef CONFIG_MFD_MAX77693
#include <linux/mfd/max77693.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "notifier %s %d: " fmt, __func__, __LINE__

struct  hnotifier_info {
	struct usb_phy *phy;
	struct host_notify_dev ndev;
	struct work_struct noti_work;

	int event;
	int prev_event;
};

static struct hnotifier_info ninfo = {
	.ndev.name = "usb_otg",
};

extern int sec_handle_event(int enable);
#ifdef CONFIG_CHARGER_BQ24260
extern void bq24260_otg_control(int enable);
#endif
static int host_notifier_booster(int enable, struct host_notify_dev *ndev)
{
	int ret = 0;
	pr_info("booster %s\n", enable ? "ON" : "OFF");

#ifdef CONFIG_MFD_MAX77693
	muic_otg_control(enable);
#endif
#ifdef CONFIG_CHARGER_BQ24260
	bq24260_otg_control(enable);
#endif

	return ret;
}

static void hnotifier_work(struct work_struct *w)
{
	struct hnotifier_info *pinfo;
	int event;

	pinfo = container_of(w, struct hnotifier_info, noti_work);
	event = pinfo->event;
	pr_info("hnotifier_work : event %d\n", pinfo->event);

	switch (event) {
	case HNOTIFY_NONE:
	case HNOTIFY_VBUS: break;
	case HNOTIFY_ID:
		pr_info("!ID\n");
		host_state_notify(&pinfo->ndev,	NOTIFY_HOST_ADD);
	#ifdef CONFIG_MFD_MAX77693
		muic_otg_control(1);
	#endif
	#ifdef CONFIG_CHARGER_BQ24260
		bq24260_otg_control(1);
	#endif
		sec_handle_event(1);
		break;
	case HNOTIFY_ENUMERATED:
	case HNOTIFY_CHARGER: break;
	case HNOTIFY_ID_PULL:
		pr_info("ID\n");
		host_state_notify(&pinfo->ndev,	NOTIFY_HOST_REMOVE);
		sec_handle_event(0);
	#ifdef CONFIG_MFD_MAX77693
		muic_otg_control(0);
	#endif
	#ifdef CONFIG_CHARGER_BQ24260
		bq24260_otg_control(0);
	#endif
		break;
	case HNOTIFY_OVERCURRENT:
		pr_info("OVP\n");
		host_state_notify(&pinfo->ndev,	NOTIFY_HOST_OVERCURRENT);
		break;
	case HNOTIFY_OTG_POWER_ON:
		pinfo->ndev.booster = NOTIFY_POWER_ON;
		break;
	case HNOTIFY_OTG_POWER_OFF:
		pinfo->ndev.booster = NOTIFY_POWER_OFF;
		break;
	case HNOTIFY_SMARTDOCK_ON:
		sec_handle_event(1);
		break;
	case HNOTIFY_SMARTDOCK_OFF:
		sec_handle_event(0);
		break;
	case HNOTIFY_AUDIODOCK_ON:
	case HNOTIFY_AUDIODOCK_OFF:
	default:
		break;
	}

#if 0
	atomic_notifier_call_chain(&pinfo->phy->notifier,
		event, pinfo);
#endif
}

int sec_otg_notify(int event)
{
	pr_info("sec_otg_notify : %d\n", event);
	ninfo.prev_event = ninfo.event;
	ninfo.event = event;

	if (ninfo.phy)
		schedule_work(&ninfo.noti_work);
	return 0;
}
EXPORT_SYMBOL(sec_otg_notify);

int sec_otg_set_booster(int (*f)(int, struct host_notify_dev*))
{
	ninfo.ndev.set_booster = f;
	return 0;
}
EXPORT_SYMBOL(sec_otg_set_booster);

static int host_notifier_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_info(&pdev->dev, "notifier_probe\n");

	ninfo.ndev.set_booster = host_notifier_booster;
	ninfo.phy = usb_get_transceiver();
	ATOMIC_INIT_NOTIFIER_HEAD(&ninfo.phy->notifier);

	ret = host_notify_dev_register(&ninfo.ndev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to host_notify_dev_register\n");
		return ret;
	}
	INIT_WORK(&ninfo.noti_work, hnotifier_work);

	return 0;
}

static int host_notifier_remove(struct platform_device *pdev)
{
	cancel_work_sync(&ninfo.noti_work);
	host_notify_dev_unregister(&ninfo.ndev);
	return 0;
}

static struct of_device_id host_notifier_of_match[] = {
	{	.compatible = "sec,host-notifier", },
	{}
};

static struct platform_driver host_notifier_driver = {
	.probe		= host_notifier_probe,
	.remove		= host_notifier_remove,
	.driver		= {
		.name	= "host_notifier",
		.owner	= THIS_MODULE,
		.of_match_table = host_notifier_of_match,
	},
};
module_platform_driver(host_notifier_driver);

MODULE_AUTHOR("Hyuk Kang <hyuk78.kang@samsung.com>");
MODULE_DESCRIPTION("USB Host notifier");
MODULE_LICENSE("GPL");
