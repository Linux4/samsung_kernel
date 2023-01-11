/*
 * extcon-sec.h - Samsung logical extcon drvier to support USB switches
 *
 * This code support for multi MUIC at one device(project).
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_EXTCON_SAMSUNG_H__
#define __LINUX_EXTCON_SAMSUNG_H__

#include <linux/extcon.h>

#define EXTCON_SEC_NAME			"extcon-sec"

enum extsec_cable_type {
	CABLE_USB,
	CABLE_USB_HOST,
	CABLE_TA,
	CABLE_JIG_USB,
	CABLE_JIG_UART,
	CABLE_DESKTOP_DOCK,
	CABLE_PPD,

	CABLE_UNKNOWN,
	CABLE_UNKNOWN_VB,

	CABLE_END,
};

enum extsec_vbus_type {
	CABLE_VBUS_NONE,
	CABLE_VBUS_OCCURRENCE,
};

extern const char *extsec_cable[CABLE_NAME_MAX + 1];

#define SET_PHY_EXTCON(_extsec, _extphy_name)				\
do {									\
	strncpy((_extsec)->pdata->phy_edev_name, #_extphy_name,		\
				strlen(#_extphy_name));			\
	(_extsec)->extsec_set_path_fn = _extphy_name##_set_path;	\
	(_extsec)->extsec_detect_cable_fn = _extphy_name##_detect_cable_fn; \
} while(0)

extern int vbus_register_notify(struct notifier_block *nb);
extern int vbus_unregister_notify(struct notifier_block *nb);

#endif
