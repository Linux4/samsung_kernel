/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SAMSUNG_USBPD_H_
#define _SAMSUNG_USBPD_H_

#include <linux/usb/usbpd.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/typec/pm6150/pm6150_typec.h>

/* USBPD-TypeC specific Macros */
#define VDM_VERSION		0x0
#define USB_C_SAMSUNG_SID		0x04E8

enum samsung_usbpd_events {
	SAMSUNG_USBPD_EVT_DISCOVER,
	SAMSUNG_USBPD_EVT_ENTER,
	SAMSUNG_USBPD_EVT_EXIT,
};

enum samsung_usbpd_alt_mode {
	SAMSUNG_USBPD_ALT_MODE_NONE	    = 0,
	SAMSUNG_USBPD_ALT_MODE_INIT	    = BIT(0),
	SAMSUNG_USBPD_ALT_MODE_DISCOVER  = BIT(1),
	SAMSUNG_USBPD_ALT_MODE_ENTER	    = BIT(2),
};

struct samsung_usbpd_private {
	char *name;
	bool forced_disconnect;
	u32 vdo;
	struct platform_device *pdev;
	struct device *dev;
	struct usbpd *pd;
	struct usbpd_svid_handler svid_handler;
	enum samsung_usbpd_alt_mode alt_mode;
	struct pm6150_phydrv_data *phy_data;
};

extern int samsung_usbpd_uvdm_ready(void);
extern void samsung_usbpd_uvdm_close(void);

#endif /* _SAMSUNG_USBPD_H_ */
