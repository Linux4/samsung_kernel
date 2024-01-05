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

/* #include "vps_table.h" */

#include <linux/power_supply.h>

/* touch callback */
#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#endif

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};
#endif

extern struct class *sec_class;

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
#if 1
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usb_hsotg);
#endif
	pr_info("%s:%s MUIC usb_cb:%d\n", MUIC_DEV_NAME, __func__, usb_mode);
#if 1
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
		default:
			pr_info("usb: muic: invalid mode%d\n", usb_mode);
		}
	}
#endif
}

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

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
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
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
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
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

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
	tsp_charger_infom(is_cable_attached);
#endif
skip:
	return 0;
}

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
