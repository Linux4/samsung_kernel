/*
 *  usb notify header
 *
 * Copyright (C) 2011-2013 Samsung, Inc.
 * Author: Dongrak Shin <dongrak.shin@samsung.com>
 *
*/

#ifndef __LINUX_USB_NOTIFIER_H__
#define __LINUX_USB_NOTIFIER_H__

#include <linux/usb_notify.h>

struct usb_notifier_platform_data {
	int	gpio_booster;
	char *booster_name;
};

#ifdef CONFIG_USB_NOTIFIER
extern void register_otg_func(int (*host)(void *, bool),
			int (*peripheral)(void *, bool), void *data);
#else
static inline void register_otg_func(int (*host)(void *, bool),
			int (*peripheral)(void *, bool), void *data) {}
#endif
#endif /* __LINUX_USB_NOTIFIER_H__ */
