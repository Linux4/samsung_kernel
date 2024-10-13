/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _BATTERY_CHARGER_H
#define _BATTERY_CHARGER_H

#include <linux/notifier.h>

enum battery_charger_prop {
	BATTERY_RESISTANCE,
	FLASH_ACTIVE,
	BATTERY_CHARGER_PROP_MAX,
};

enum bc_hboost_event {
	VMAX_CLAMP,
};

#if IS_ENABLED(CONFIG_QTI_BATTERY_CHARGER)
int qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val);
int qti_battery_charger_set_prop(const char *name,
				enum battery_charger_prop prop_id, int val);
int register_hboost_event_notifier(struct notifier_block *nb);
int unregister_hboost_event_notifier(struct notifier_block *nb);
#else
static inline int
qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val)
{
	return -EINVAL;
}

static inline int
qti_battery_charger_set_prop(const char *name,
				enum battery_charger_prop prop_id, int val)
{
	return -EINVAL;
}

static inline int register_hboost_event_notifier(struct notifier_block *nb)
{
	return -EOPNOTSUPP;
}

static inline int unregister_hboost_event_notifier(struct notifier_block *nb)
{
	return -EOPNOTSUPP;
}
#endif

/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
typedef enum charger_notify {
	CHARGER_NOTIFY_UNKNOWN,
	CHARGER_NOTIFY_STOP_CHARGING,
	CHARGER_NOTIFY_START_CHARGING,
} charger_notify_t;
extern int register_usb_check_notifier(struct notifier_block *nb);
extern int unregister_usb_check_notifier(struct notifier_block *nb);
/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/

/*M55 code for QN6887A-400 by shanxinkai at 20231103 start*/
extern bool g_charge_bright_screen;
/*M55 code for QN6887A-400 by shanxinkai at 20231103 end*/

#endif
