/*
 * include/linux/cable_type_notifier.h
 *
 * header file supporting cable type notifier call chain information
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

#ifndef __CABLE_TYPE_NOTIFIER_H__
#define __CABLE_TYPE_NOTIFIER_H__

typedef enum {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_USB_SDP,
	CABLE_TYPE_USB_CDP,
	CABLE_TYPE_OTG,
	CABLE_TYPE_TA,
	CABLE_TYPE_AFC,
	CABLE_TYPE_NONSTANDARD,
	CABLE_TYPE_UNKNOWN,
} cable_type_attached_dev_t;

static const char *cable_type_names[] = {
	"NONE",
	"USB",
	"USB SDP",
	"USB CDP",
	"OTG",
	"TA",
	"AFC",
	"NON-STANDARD Charger",
	"UNKNOWN",
};

/* CABLE_TYPE notifier call chain command */
typedef enum {
	CABLE_TYPE_NOTIFY_CMD_DETACH	= 0,
	CABLE_TYPE_NOTIFY_CMD_ATTACH,
} cable_type_notifier_cmd_t;

/* CABLE_TYPE notifier call sequence,
 * largest priority number device will be called first. */
typedef enum {
	CABLE_TYPE_NOTIFY_DEV_CHARGER = 0,
	CABLE_TYPE_NOTIFY_DEV_USB,
} cable_type_notifier_device_t;

struct cable_type_notifier_struct {
	cable_type_attached_dev_t attached_dev;
	cable_type_notifier_cmd_t cmd;
	struct blocking_notifier_head notifier_call_chain;
};

#define CABLE_TYPE_NOTIFIER_BLOCK(name)	\
	struct notifier_block (name)

/* cable_type notifier init/notify function */
extern void cable_type_notifier_set_attached_dev(cable_type_attached_dev_t new_dev);

/* cable_type notifier register/unregister API
 * for used any where want to receive cable_type attached device attach/detach. */
extern int cable_type_notifier_register(struct notifier_block *nb,
		notifier_fn_t notifier, cable_type_notifier_device_t listener);
extern int cable_type_notifier_unregister(struct notifier_block *nb);

#endif /* __CABLE_TYPE_NOTIFIER_H__ */
