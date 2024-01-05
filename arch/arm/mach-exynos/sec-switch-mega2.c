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
#if defined(CONFIG_USB_SWITCH_RT8973)
#include <linux/rt8973.h>
#endif /* CONFIG_MUIC_RT8973 */

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
extern void ohci_power(struct device*, int);

struct device *switch_device;
bool is_cable_attached;
bool is_jig_on;
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

static void muic_init_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		muic_err("%s Failed to register dock switch(%d)\n",
				__func__, ret);

	muic_dbg("%s switch_dock switch_dev_register\n", __func__);
#endif
}

static int muic_filter_vps_cb(muic_data_t *muic_data)
{
#if 0
	struct muic_platform_data *pdata = muic_data->pdata;
	struct i2c_client *i2c = muic_data->i2c;
#endif
	int intr = MUIC_INTR_DETACH;

	muic_dbg("%s\n", __func__);

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
	muic_dbg("%s MUIC usb_cb:%d\n", __func__, usb_mode);

	if (gadget) {
		switch (usb_mode) {
		case USB_CABLE_DETACHED:
			muic_dbg("usb: muic: USB_CABLE_DETACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_disconnect(gadget);
			break;
		case USB_CABLE_ATTACHED:
			muic_dbg("usb: muic: USB_CABLE_ATTACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_connect(gadget);
			break;
		default:
			muic_dbg("usb: muic: invalid mode%d\n", usb_mode);
		}
	}
#ifdef CONFIG_USB_HOST_NOTIFY
	if (usb_mode == USB_OTGHOST_ATTACHED) {
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_get_sync(&s5p_device_ehci.dev);
		pm_runtime_forbid(&s5p_device_ehci.dev);
#endif
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 1);
#endif
		host_noti_pdata->booster(1);
		host_noti_pdata->ndev.mode = NOTIFY_HOST_MODE;
		if (host_noti_pdata->usbhostd_start)
			host_noti_pdata->usbhostd_start();
	} else if (usb_mode == USB_OTGHOST_DETACHED) {
		host_noti_pdata->ndev.mode = NOTIFY_NONE_MODE;
		if (host_noti_pdata->usbhostd_stop)
			host_noti_pdata->usbhostd_stop();
		host_noti_pdata->booster(0);
#ifdef CONFIG_USB_OHCI_EXYNOS
		ohci_power(&exynos4_device_ohci.dev, 0);
#endif
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_allow(&s5p_device_ehci.dev);
		pm_runtime_put_sync(&s5p_device_ehci.dev);
#endif
	} else if (usb_mode == USB_POWERED_HOST_ATTACHED) {
		host_noti_pdata->powered_booster(1);
		start_usbhostd_wakelock();
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_get_sync(&s5p_device_ehci.dev);
#endif
#ifdef CONFIG_USB_OHCI_EXYNOS
		pm_runtime_get_sync(&exynos4_device_ohci.dev);
#endif
		muic_dbg("%s - USB_POWERED_HOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_DETACHED) {
#ifdef CONFIG_USB_OHCI_EXYNOS
		pm_runtime_put_sync(&exynos4_device_ohci.dev);
#endif
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_put_sync(&s5p_device_ehci.dev);
#endif
		host_noti_pdata->powered_booster(0);
		stop_usbhostd_wakelock();
		muic_dbg("%s - USB_POWERED_HOST_DETACHED\n", __func__);
	}
#endif
}

static int muic_charger_cb(enum muic_attached_dev cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");

	muic_dbg("%s %d\n", __func__, cable_type);

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
		is_jig_on = false;
		is_cable_attached = false;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;

	case ATTACHED_DEV_USB_MUIC:
		is_jig_on = false;
		is_cable_attached = true;
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;

	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNKNOWN_VB_MUIC:
		is_jig_on = false;
		is_cable_attached = true;
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;

	case ATTACHED_DEV_OTG_MUIC:
		is_jig_on = false;
		is_cable_attached = false;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;

	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
#if !defined(CONFIG_USB_SWITCH_RT8973)
	case ATTACHED_DEV_AUDIODOCK_MUIC:
#endif	/* CONFIG_USB_SWITCH_RT8973 */
#if 0
		is_jig_on = false;
		is_cable_attached = true;
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
#else
		muic_dbg("%s: the dock is not work in this model(%d)\n",
			__func__, cable_type);
		return 0;
#endif

	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		is_jig_on = true;
		is_cable_attached = false;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;

	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		is_jig_on = true;
		is_cable_attached = true;
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;

	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		is_jig_on = true;
		is_cable_attached = true;
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;

	default:
		muic_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

	if (!psy || !psy->set_property)
		muic_err("%s: fail to get battery psy\n", __func__);
	else {
		union power_supply_propval value;

		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

#if defined(CONFIG_TOUCHSCREEN_MMS134S) ||\
	defined(CONFIG_TOUCHSCREEN_MXT224E) ||\
	defined(CONFIG_TOUCHSCREEN_MXT224S) ||\
	defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)
	tsp_charger_infom(is_cable_attached);
#endif
	return 0;
}

static void muic_dock_cb(int type)
{
	muic_dbg("%s : %d\n", __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

#ifdef CONFIG_USB_HOST_NOTIFY
int muic_host_notify_cb(int enable)
{
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;

	struct host_notify_dev *ndev = &host_noti_pdata->ndev;

	if (!ndev) {
		muic_err("%s: ndev is null.\n", __func__);
		return -1;
	}

	ndev->booster = enable ? NOTIFY_POWER_ON : NOTIFY_POWER_OFF;
	return ndev->mode;
}
#endif

struct sec_switch_data sec_switch_data = {
	.init_cb = muic_init_cb,
	.filter_vps_cb = muic_filter_vps_cb,
	.usb_cb = muic_usb_cb,
	.charger_cb = muic_charger_cb,
	.dock_cb = muic_dock_cb,
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify_cb = muic_host_notify_cb,
#else
	.host_notify_cb = NULL,
#endif
};

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_device = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_device)) {
		muic_err("%s Failed to create device(switch)!\n", __func__);
		return -ENODEV;
	}

	return ret;
};

device_initcall(sec_switch_init);
