/*
 * Copyright (C) 2015-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

 /* usb_power_notify.c */

#define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/usb.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/usb_notify.h>
#include "usb_power_notify.h"
#include "../core/hub.h"
#include "core.h"

enum usb_port_state {
	PORT_EMPTY = 0,		/* OTG only */
	PORT_USB2,		/* usb 2.0 device only */
	PORT_USB3,		/* usb 3.0 device only */
	PORT_HUB,		/* usb hub single */
	PORT_DP			/* DP device */
};

#define USB_CLASS_BILLBOARD	0x11

static enum usb_port_state port_state;
static int phy_power_control;
int is_otg_only;

extern struct dwc3 *g_dwc;
extern int exynos_usbdrd_pipe3_enable(struct phy *phy);
extern int exynos_usbdrd_pipe3_disable(struct phy *phy);

static void usb_power_phy_control(int on)
{
	pr_info("%s, pre = %d, on = %d\n",
			__func__, phy_power_control, on);

	if (phy_power_control != on) {
		phy_power_control = on;
		if (on) {
			dwc3_core_susphy_set(g_dwc, 0);
			exynos_usbdrd_pipe3_enable(g_dwc->usb3_generic_phy);
			dwc3_core_susphy_set(g_dwc, 1);
			pr_info("%s, USB3.0 PHY reinit done\n", __func__);
			xhci_port_power_set(1, 1);
		} else {
			xhci_port_power_set(0, 1);
			dwc3_core_susphy_set(g_dwc, 0);
			exynos_usbdrd_pipe3_disable(g_dwc->usb3_generic_phy);
			dwc3_core_susphy_set(g_dwc, 1);
		}
	} else
		pr_info("%s, phy control is same as pre state(%d)\n",
				__func__, on);
}

static int check_usb3_hub(struct usb_device *dev, bool on)
{
	struct usb_device *hdev;
	struct usb_device *udev = dev;
	struct usb_hub *hub;
	enum usb_port_state pre_state;
	int usb3_hub_detect = 0;
	int usb2_detect = 0;
	int port;
	int bInterfaceClass = 0;

	if (udev->bus->root_hub == udev) {
		pr_info("this dev is a root hub\n");
		goto skip;
	}

	pre_state = port_state;

	/* Find root hub */
	hdev = udev->parent;
	if (!hdev)
		goto skip;

	for (port = 0; port < HUB_MAX_DEPTH; port++) {
		if (hdev == udev->bus->root_hub)
			break;
		hdev = hdev->parent;
	}
	pr_info("root hub depth = %d\n", port);

	hub = usb_hub_to_struct_hub(hdev);
	if (!hub)
		goto skip;

	/* check all ports */
	for (port = 1; port <= hdev->maxchild; port++) {
		udev = hub->ports[port-1]->child;
		if (udev && udev->state != USB_STATE_NOTATTACHED) {
			bInterfaceClass	= udev->config->interface[0]
					->cur_altsetting->desc.bInterfaceClass;
			if (on) {
				if ((bInterfaceClass == USB_CLASS_HID) ||
						(bInterfaceClass == USB_CLASS_AUDIO)) {
					udev->do_remote_wakeup =
						(udev->config->desc.bmAttributes &
						 USB_CONFIG_ATT_WAKEUP) ? 1 : 0;
					if (udev->do_remote_wakeup == 1) {
						device_init_wakeup(&udev->dev, 1);
						usb_enable_autosuspend(dev);
					}
				}
			}
			if (udev->descriptor.bDeviceClass == USB_CLASS_HUB) {
				port_state = PORT_HUB;
				usb3_hub_detect = 1;
				break;
			} else if(udev->descriptor.bDeviceClass ==
							USB_CLASS_BILLBOARD) {
				port_state = PORT_DP;
				usb3_hub_detect = 1;
				break;
			}

			if (udev->speed >= USB_SPEED_SUPER) {
				port_state = PORT_USB3;
				usb3_hub_detect = 1;
				break;
			} else {
				port_state = PORT_USB2;
				usb2_detect = 1;
			}
		}
	}

	if (!usb3_hub_detect && !usb2_detect)
		port_state = PORT_EMPTY;

	pr_info("%s %s state pre=%d now=%d\n", __func__,
		on ? "on" : "off", pre_state, port_state);

	return port_state;

skip:
	return -EINVAL;
}

static void set_usb3_port_power(struct usb_device *dev, bool on)
{
	switch (check_usb3_hub(dev, on)) {
	case PORT_EMPTY:
		pr_info("Port check empty\n");
		is_otg_only = 1;
		usb_power_phy_control(1);
		break;
	case PORT_USB2:
		pr_info("Port check usb2\n");
		is_otg_only = 0;
		usb_power_phy_control(0);
		break;
	case PORT_USB3:
		/* xhci_port_power_set(1, 1); */
		is_otg_only = 0;
		pr_info("Port check usb3\n");
		break;
	case PORT_HUB:
		/*xhci_port_power_set(1, 1);*/
		pr_info("Port check hub\n");
		is_otg_only = 0;
		break;
	case PORT_DP:
		/*xhci_port_power_set(1, 1);*/
		pr_info("Port check DP\n");
		is_otg_only = 0;
		break;
	default:
		break;
	}
}

static int usb_power_notify(struct notifier_block *self,
			       unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		set_usb3_port_power(dev, 1);
		break;
	case USB_DEVICE_REMOVE:
		set_usb3_port_power(dev, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block dev_nb = {
	.notifier_call = usb_power_notify,
};

void register_usb_power_notify(void)
{
	is_otg_only = 1;
	port_state = PORT_EMPTY;
	phy_power_control = 1;
	usb_register_notify(&dev_nb);
}
EXPORT_SYMBOL(register_usb_power_notify);

void unregister_usb_power_notify(void)
{
	usb_unregister_notify(&dev_nb);
}
EXPORT_SYMBOL(unregister_usb_power_notify);


