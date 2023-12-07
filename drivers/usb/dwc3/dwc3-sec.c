/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/usb_notify.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_EXTCON
#include <linux/extcon.h>
#include <linux/power_supply.h>
#endif

struct dwc3_msm;
struct dwc3_sec {
	struct notifier_block nb;
	struct dwc3_msm *dwcm;
};

static struct dwc3_sec sec_noti;

void sec_usb_work(int usb_mode)
{
	struct power_supply *psy;

#if defined(CONFIG_BATTERY_SAMSUNG)
	psy = power_supply_get_by_name("dwc-usb");
#else
	psy = power_supply_get_by_name("usb");
#endif
	pr_info("usb: dwc3 power supply set(%d)", usb_mode);
	if (psy)
		power_supply_set_present(psy, usb_mode);
	else
		pr_err("usb: dwc-usb power supply is null!\n");
}
EXPORT_SYMBOL(sec_usb_work);

static void dwc3_resume_work(struct work_struct *w);
static int sec_otg_ext_notify(struct dwc3_msm *mdwc, int enable)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	mdwc->ext_xceiv.id = enable ? DWC3_ID_GROUND : DWC3_ID_FLOAT;

	if (atomic_read(&dwc->in_lpm)) {
		dev_info(mdwc->dev, "%s: calling resume_work\n", __func__);
		dwc3_resume_work(&mdwc->resume_work.work);
	} else {
		dev_info(mdwc->dev, "%s: notifying xceiv event\n", __func__);
		if (mdwc->otg_xceiv)
			mdwc->ext_xceiv.notify_ext_events(mdwc->otg_xceiv->otg);
	}
	return 0;
}

int sec_handle_event(bool enable)
{
	struct dwc3_sec *snoti = &sec_noti;
	struct dwc3_msm *dwcm;

	pr_info("%s: event %d\n", __func__, enable);

	if (!snoti) {
		pr_err("%s: dwc3_otg (snoti) is null\n", __func__);
		return NOTIFY_BAD;
	}

	dwcm = snoti->dwcm;
	if (!dwcm) {
		pr_err("%s: dwc3_otg (dwcm) is null\n", __func__);
		return NOTIFY_BAD;
	}

	if (enable) {
		pr_info("USB OTG START : ID clear\n");
		sec_otg_ext_notify(dwcm, 1);
	} else {
		pr_info("USB OTG STOP : ID set\n");
		sec_otg_ext_notify(dwcm, 0);
	}

	return 0;
}
EXPORT_SYMBOL(sec_handle_event);

#ifdef CONFIG_EXTCON
struct sec_cable {
	struct work_struct work;
	struct notifier_block nb;
	struct extcon_specific_cable_nb extcon_nb;
	struct extcon_dev *edev;
	enum extcon_cable_name cable_type;
	int cable_state;
};

static struct sec_cable support_cable_list[] = {
	{ .cable_type = EXTCON_USB, },
	{ .cable_type = EXTCON_USB_HOST, },
	{ .cable_type = EXTCON_USB_HOST_5V, },
	{ .cable_type = EXTCON_TA, },
	{ .cable_type = EXTCON_AUDIODOCK, },
	{ .cable_type = EXTCON_SMARTDOCK_TA, },
	{ .cable_type = EXTCON_SMARTDOCK_USB, },
	{ .cable_type = EXTCON_JIG_USBON, },
	{ .cable_type = EXTCON_CHARGE_DOWNSTREAM, },
};

static void sec_cable_event_worker(struct work_struct *work)
{
	struct sec_cable *cable =
			    container_of(work, struct sec_cable, work);
	struct otg_notify *n = get_otg_notify();

	pr_info("%s: %s(%d) is %s\n", __func__,
		extcon_cable_name[cable->cable_type],
		cable->cable_type,
		cable->cable_state ? "attached" : "detached");

	switch (cable->cable_type) {
	case EXTCON_USB:
	case EXTCON_SMARTDOCK_USB:
	case EXTCON_JIG_USBON:
	case EXTCON_CHARGE_DOWNSTREAM:
		if (cable->cable_state)
			send_otg_notify(n, NOTIFY_EVENT_VBUS, 1);
		else
			send_otg_notify(n, NOTIFY_EVENT_VBUS, 0);
		break;
	case EXTCON_USB_HOST:
		if (cable->cable_state) {
			send_otg_notify(n, NOTIFY_EVENT_DRIVE_VBUS, 1);
			send_otg_notify(n, NOTIFY_EVENT_HOST, 1);
		} else {
			send_otg_notify(n, NOTIFY_EVENT_HOST, 0);
			send_otg_notify(n, NOTIFY_EVENT_DRIVE_VBUS, 0);
		}
		break;
	case EXTCON_AUDIODOCK:
		if (cable->cable_state)
			send_otg_notify(n, NOTIFY_EVENT_AUDIODOCK, 1);
		else
			send_otg_notify(n, NOTIFY_EVENT_AUDIODOCK, 0);
		break;
	case EXTCON_SMARTDOCK_TA:
		if (cable->cable_state)
			send_otg_notify(n, NOTIFY_EVENT_SMARTDOCK_TA, 1);
		else
			send_otg_notify(n, NOTIFY_EVENT_SMARTDOCK_TA, 0);
		break;
	case EXTCON_USB_HOST_5V:
		if (cable->cable_state)
			send_otg_notify(n, NOTIFY_EVENT_VBUSPOWER, 1);
		else
			send_otg_notify(n, NOTIFY_EVENT_VBUSPOWER, 0);
	default:
		break;
	}
}

static int sec_cable_notifier(struct notifier_block *nb,
					unsigned long stat, void *ptr)
{
	struct sec_cable *cable =
			container_of(nb, struct sec_cable, nb);

	cable->cable_state = stat;
	pr_info("sec_cable_notifier: state %lu\n", stat);

	queue_work(system_nrt_wq, &cable->work);

	return NOTIFY_DONE;
}

static int __init sec_otg_init_cable_notify(void)
{
	struct sec_cable *cable;
	int i;
	int ret;

	pr_info("%s register extcon notifier for usb and ta\n", __func__);
	for (i = 0; i < ARRAY_SIZE(support_cable_list); i++) {
		cable = &support_cable_list[i];

		INIT_WORK(&cable->work, sec_cable_event_worker);
		cable->nb.notifier_call = sec_cable_notifier;

		ret = extcon_register_interest(&cable->extcon_nb,
				EXTCON_DEV_NAME,
				extcon_cable_name[cable->cable_type],
				&cable->nb);
		if (ret)
			pr_err("%s: fail to register extcon notifier(%s, %d)\n",
				__func__, extcon_cable_name[cable->cable_type],
				ret);

		cable->edev = cable->extcon_nb.edev;
		if (!cable->edev)
			pr_err("%s: fail to get extcon device\n", __func__);
	}
	return 0;
}
device_initcall_sync(sec_otg_init_cable_notify);
#endif

static int sec_otg_init(struct dwc3_msm *dwcm, struct usb_phy *phy)
{
	int ret = 0;

	pr_info("%s: register notifier\n", __func__);

	sec_noti.dwcm = dwcm;
	return ret;
}
