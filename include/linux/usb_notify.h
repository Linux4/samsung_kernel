
/*
 * Copyright (C) 2015-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

  /* usb notify layer v3.1 */

#ifndef __LINUX_USB_NOTIFY_SYSFS_H__
#define __LINUX_USB_NOTIFY_SYSFS_H__

#include <linux/usb.h>

#define CMD_STATE_LEN 32
#define UDC_CMD_LEN 16
#define MAX_WHITELIST_STR_LEN 256
#define MAX_CLASS_TYPE_NUM	USB_CLASS_VENDOR_SPEC

enum notify_block_type {
	NOTIFY_INVALID = 0,
	NOTIFY_OFF,
	NOTIFY_BLOCK_HOST,
	NOTIFY_BLOCK_ALL,
};

enum otg_notify_mdm_type {
	NOTIFY_MDM_TYPE_OFF,
	NOTIFY_MDM_TYPE_ON,
};

enum u_interface_class_type {
	U_CLASS_PER_INTERFACE = 1,
	U_CLASS_AUDIO,
	U_CLASS_COMM,
	U_CLASS_HID,
	U_CLASS_PHYSICAL,
	U_CLASS_STILL_IMAGE,
	U_CLASS_PRINTER,
	U_CLASS_MASS_STORAGE,
	U_CLASS_HUB,
	U_CLASS_CDC_DATA,
	U_CLASS_CSCID,
	U_CLASS_CONTENT_SEC,
	U_CLASS_VIDEO,
	U_CLASS_WIRELESS_CONTROLLER,
	U_CLASS_MISC,
	U_CLASS_APP_SPEC,
	U_CLASS_VENDOR_SPEC,
};

struct usb_notify_dev {
	const char *name;
	struct device *dev;
	unsigned long usb_data_enabled;
	char state_cmd[CMD_STATE_LEN];
	char whitelist_str[MAX_WHITELIST_STR_LEN];
	int whitelist_array_for_mdm[MAX_CLASS_TYPE_NUM+1];
};

struct usb_device_dev {
	const char *name;
	struct device *dev;
};

struct usb_notify {
	struct class *usb_notify_class;
	struct class *usb_device_class;
	struct raw_notifier_head	otg_notifier;
	struct notifier_block otg_nb;
	struct blocking_notifier_head extra_notifier;
	struct usb_notify_dev notify_dev;
	struct usb_device_dev device_dev;
	struct gadget_info *usb_gi;
	char *udc_name;
	char gi_name[UDC_CMD_LEN];
	int usb_host_flag;
	int usb_device_flag;
	int sec_whitelist_enable;
};


extern int usb_notify_dev_register(struct usb_notify_dev *ndev);
extern void usb_notify_dev_unregister(struct usb_notify_dev *ndev);
extern int usb_device_dev_register(struct usb_device_dev *ndev);
extern void usb_device_dev_unregister(struct usb_device_dev *ndev);
extern int usb_notify_class_init(struct usb_notify* u_notify);
extern void usb_notify_class_exit(struct usb_notify* u_notify);

extern int usb_otg_notifier_register(struct notifier_block *nb);
extern void usb_otg_notifier_unregister(struct notifier_block *nb);
extern struct usb_notify *usb_get_notify(struct gadget_info *gi, char *name , int len);
extern int usb_check_whitelist_for_mdm(struct usb_device *dev);

extern struct usb_notify *u_notify;
extern struct raw_notifier_head usb_otg_notifier;

#endif

