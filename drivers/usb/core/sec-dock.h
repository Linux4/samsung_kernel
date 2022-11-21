/*
 * drivers/usb/core/sec-dock.h
 *
 * Copyright (C) 2013 Samsung Electronics
 * Author: Woo-kwang Lee <wookwang.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#if 0
static int call_battery_notify(struct usb_device *dev, bool bOnOff)
{
	return 0;
}
#else
#include <linux/usb_notify.h>

#define SMARTDOCK_INDEX	1
#define MMDOCK_INDEX		2

struct dev_table {
	struct usb_device_id dev;
	int index;
};

static struct dev_table enable_notify_hub_table[] = {
	{ .dev = { USB_DEVICE(0x0424, 0x2514), },
	   .index = SMARTDOCK_INDEX,
	}, /* SMART DOCK HUB 1 */
	{ .dev = { USB_DEVICE(0x1a40, 0x0101), },
	   .index = SMARTDOCK_INDEX,
	}, /* SMART DOCK HUB 2 */
	{ .dev = { USB_DEVICE(0x0424, 0x9512), },
	   .index = MMDOCK_INDEX,
	}, /* SMSC USB LAN HUB 9512 */
	{}
};

static struct dev_table essential_device_table[] = {
	{ .dev = { USB_DEVICE(0x08bb, 0x2704), },
	   .index = SMARTDOCK_INDEX,
	}, /* TI USB Audio DAC 1 */
	{ .dev = { USB_DEVICE(0x08bb, 0x27c4), },
	   .index = SMARTDOCK_INDEX,
	}, /* TI USB Audio DAC 2 */
	{ .dev = { USB_DEVICE(0x0424, 0xec00), },
	   .index = MMDOCK_INDEX,
	}, /* SMSC LAN Driver */
	{}
};

static int check_essential_device(struct usb_device *dev, int index)
{
	struct dev_table *id;
	int ret = 0;

	/* check VID, PID */
	for (id = essential_device_table; id->dev.match_flags; id++) {
		if ((id->dev.match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
			(id->dev.match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
			id->dev.idVendor == le16_to_cpu(dev->descriptor.idVendor) &&
			id->dev.idProduct == le16_to_cpu(dev->descriptor.idProduct) &&
			id->index == index) {
			ret = 1;
			break;
		}
	}
	return ret;
}

static int is_notify_hub(struct usb_device *dev)
{
	struct dev_table *id;
	struct usb_device *hdev;
	int ret = 0;

	hdev = dev->parent;
	if (!hdev)
		goto skip;

	/* check VID, PID */
	for (id = enable_notify_hub_table; id->dev.match_flags; id++) {
		if ((id->dev.match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
			(id->dev.match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
			id->dev.idVendor == le16_to_cpu(hdev->descriptor.idVendor) &&
			id->dev.idProduct == le16_to_cpu(hdev->descriptor.idProduct)) {

			ret = (hdev->parent &&
				(hdev->parent == dev->bus->root_hub)) ? id->index : 0;
			break;
		}
	}
skip:
	return ret;
}

static int call_battery_notify(struct usb_device *dev, bool bOnOff)
{
	struct usb_device *hdev;
	struct usb_device *udev;
	struct usb_hub *hub = usb_hub_to_struct_hub(dev->parent);
	struct otg_notify *o_notify = get_otg_notify();
	int index = 0;
	int count = 0;
	int port;

	if (dev->bus->root_hub != dev && bOnOff) {
		pr_info("%s device\n", __func__);
		send_otg_notify(o_notify, NOTIFY_EVENT_DEVICE_CONNECT, 1);
	}
	index = is_notify_hub(dev);
	if (!index)
		goto skip;
	if (check_essential_device(dev, index))
		goto skip;

	hdev = dev->parent;
	for (port = 1; port <= hdev->maxchild; port++) {

		udev = hub->ports[port-1]->child;

		if (udev) {
			if (!check_essential_device(udev, index)) {
				if (!bOnOff && (udev == dev))
					continue;
				else
					count++;
			}
		}
	}

	pr_info("%s : VID : 0x%x, PID : 0x%x, bOnOff=%d, count=%d\n", __func__,
		dev->descriptor.idVendor, dev->descriptor.idProduct,
			bOnOff, count);
	if (bOnOff) {
		if (count == 1) {
			if (index == SMARTDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_SMTD_EXT_CURRENT, 1);
			else if (index == MMDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_MMD_EXT_CURRENT, 1);
		}
	} else {
		if (!count) {
			if (index == SMARTDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_SMTD_EXT_CURRENT, 0);
			else if (index == MMDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_MMD_EXT_CURRENT, 0);
		}
	}
skip:
	return 0;
}
#endif
