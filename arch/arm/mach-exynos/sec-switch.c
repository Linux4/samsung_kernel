/*
 * arch/arm/mach-exynos/sec-switch.c
 *
 * c source file supporting MUIC common platform device register
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seung-Jin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>

/* MUIC header */
#if defined(CONFIG_MUIC_FSA9480)
#include <linux/fsa9480.h>
#endif /* CONFIG_MUIC_FSA9480 */
#if defined(CONFIG_MUIC_TSU6721)
#include <linux/tsu6721.h>
#endif /* CONFIG_MUIC_TSU6721 */
#if defined(CONFIG_MUIC_SM5502)
#include <linux/sm5502.h>
#endif /* CONFIG_MUIC_SM5502 */

#include <linux/muic.h>

/* switch device header */
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

/* USB header */
#include <plat/udc-hs.h>
#include <plat/devs.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#endif

/* #include "vps_table.h" */

#include <linux/power_supply.h>

/* touch callback */
#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MXT224E)
#include <linux/i2c/mxt224e.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MXT224S)
#include <linux/i2c/mxt224s.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)
#include <linux/i2c/zinitix_bt532_ts.h>
#endif

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};
#endif

extern struct class *sec_class;
extern void ehci_power(struct device*, int);
extern void ohci_power(struct device*, int);

struct device *switch_device;
bool is_cable_attached;

static void muic_init_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("%s:%s Failed to register dock switch(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	pr_info("%s:%s switch_dock switch_dev_register\n", MUIC_DEV_NAME,
			__func__);
#endif
}

static int muic_filter_vps_cb(muic_data_t *muic_data)
{
#if 0
	struct muic_platform_data *pdata = muic_data->pdata;
	struct i2c_client *i2c = muic_data->i2c;
#endif
	int intr = MUIC_INTR_DETACH;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	return intr;
}

/* usb cable call back function */
static void muic_usb_cb(u8 usb_mode)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usb_hsotg);
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;
#endif
	pr_info("%s:%s MUIC usb_cb:%d\n", MUIC_DEV_NAME, __func__, usb_mode);

	if (gadget) {
		switch (usb_mode) {
		case USB_CABLE_DETACHED:
			pr_info("usb: muic: USB_CABLE_DETACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_disconnect(gadget);
			break;
		case USB_CABLE_ATTACHED:
			pr_info("usb: muic: USB_CABLE_ATTACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_connect(gadget);
			break;
		}
	}
#ifdef CONFIG_USB_HOST_NOTIFY
	if (usb_mode == USB_OTGHOST_ATTACHED) {
		host_noti_pdata->booster(1);
		host_noti_pdata->ndev.mode = NOTIFY_HOST_MODE;
		if (host_noti_pdata->usbhostd_start)
			host_noti_pdata->usbhostd_start();
#ifdef CONFIG_USB_EHCI_S5P
		ehci_power(&s5p_device_ehci.dev, 1);
#endif
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 1);
#endif
	} else if (usb_mode == USB_OTGHOST_DETACHED) {
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 0);
#endif
#ifdef CONFIG_USB_EHCI_S5P
		ehci_power(&s5p_device_ehci.dev, 0);
#endif
		host_noti_pdata->ndev.mode = NOTIFY_NONE_MODE;
		if (host_noti_pdata->usbhostd_stop)
			host_noti_pdata->usbhostd_stop();
		host_noti_pdata->booster(0);
	} else if (usb_mode == USB_POWERED_HOST_ATTACHED) {
		host_noti_pdata->powered_booster(1);
		start_usbhostd_wakelock();
#ifdef CONFIG_USB_EHCI_S5P
		ehci_power(&s5p_device_ehci.dev, 1);
#endif
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 1);
#endif
		pr_info("%s - USB_POWERED_HOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_DETACHED) {
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 0);
#endif
#ifdef CONFIG_USB_EHCI_S5P
		ehci_power(&s5p_device_ehci.dev, 0);
#endif
		host_noti_pdata->powered_booster(0);
		stop_usbhostd_wakelock();
		pr_info("%s - USB_POWERED_HOST_DETACHED\n", __func__);
	}
#endif

}

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

#if defined(CONFIG_MUIC_SM5502)/*Degas LTE, */
static int muic_charger_cb(enum muic_attached_dev cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct power_supply *psy_p = power_supply_get_by_name("ps");
	union power_supply_propval value;
	static enum muic_attached_dev previous_cable_type = ATTACHED_DEV_NONE_MUIC;

	pr_info("%s:%s CB enabled(%d), prev_cable(%d)\n", MUIC_DEV_NAME, __func__,
		cable_type, previous_cable_type);

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_PS_CABLE_MUIC:
		is_cable_attached = false;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		is_cable_attached = true;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNKNOWN_VB_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_AUDIODOCK_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_LANHUB_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

	/*  charger setting */
	if (previous_cable_type == cable_type) {
		pr_info("%s: SKIP cable setting\n", __func__);
		goto skip_cable_setting;
	}

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_OTG_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_AUDIODOCK_MUIC:
	case ATTACHED_DEV_UNKNOWN_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case ATTACHED_DEV_LANHUB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_LAN_HUB;
		break;
	case ATTACHED_DEV_PS_CABLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_POWER_SHARING;
		break;
	default:
		pr_err("%s:%s invalid type:%d\n", MUIC_DEV_NAME, __func__,
				cable_type);
		goto skip;
	}

	if (!psy || !psy->set_property || !psy_p || !psy_p->set_property) {
		pr_err("%s: fail to get battery/ps psy\n", __func__);
	} else {
		if (current_cable_type == POWER_SUPPLY_TYPE_POWER_SHARING) {
			value.intval = current_cable_type;
			psy_p->set_property(psy_p, POWER_SUPPLY_PROP_ONLINE, &value);
		} else {
			if (previous_cable_type == ATTACHED_DEV_PS_CABLE_MUIC) {
				value.intval = current_cable_type;
				psy_p->set_property(psy_p, POWER_SUPPLY_PROP_ONLINE, &value);
			}
			value.intval = current_cable_type;
			psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		}
	}
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)	//DegasLTE
	tsp_charger_infom(is_cable_attached);
#endif

skip:
	previous_cable_type = cable_type;
skip_cable_setting:
	return 0;
}
#else
static int muic_charger_cb(enum muic_attached_dev cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	pr_info("%s:%s %d\n", MUIC_DEV_NAME, __func__, cable_type);

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		is_cable_attached = false;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		is_cable_attached = true;
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	#if defined(CONFIG_MACH_GARDA) /*garda*/
		is_cable_attached = false;
		break;
	#endif
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_AUDIODOCK_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_OTG_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		goto skip;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_AUDIODOCK_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	default:
		pr_err("%s:%s invalid type:%d\n", MUIC_DEV_NAME, __func__,
				cable_type);
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

#if defined(CONFIG_TOUCHSCREEN_MMS134S) ||\
	defined(CONFIG_TOUCHSCREEN_MXT224E) ||\
	defined(CONFIG_TOUCHSCREEN_MXT224S)
	tsp_charger_infom(is_cable_attached);
#endif
skip:
	return 0;
}
#endif

static void muic_dock_cb(int type)
{
	pr_info("%s:%s MUIC dock type=%d\n", MUIC_DEV_NAME, __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

struct sec_switch_data sec_switch_data = {
	.init_cb = muic_init_cb,
	.filter_vps_cb = muic_filter_vps_cb,
	.usb_cb = muic_usb_cb,
	.charger_cb = muic_charger_cb,
	.dock_cb = muic_dock_cb,
};

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_device = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_device)) {
		pr_err("%s:%s Failed to create device(switch)!\n",
				MUIC_DEV_NAME, __func__);
		return -ENODEV;
	}

	return ret;
};

device_initcall(sec_switch_init);
