/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 *		http://www.samsung.com/
 *
 * Defines phy types for samsung usb phy controllers - HOST or DEIVCE.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/usb/phy.h>

enum samsung_usb_phy_type {
	USB_PHY_TYPE_DEVICE,
	USB_PHY_TYPE_HOST,
};

/* Samsung USB LPA notifier chain */
#define USB_LPA_PREPARE	0
#define USB_LPA_RESUME	1

extern int register_samsung_usb_lpa_notifier(struct notifier_block *nb);
extern int unregister_samsung_usb_lpa_notifier(struct notifier_block *nb);

#ifdef CONFIG_USB_ANDROID_SAMSUNG_USBTUNE
extern void samsung_usb3phy_tune_read(struct usb_phy *phy);
extern void samsung_usb3phy_tune_write(struct usb_phy *phy);
#endif