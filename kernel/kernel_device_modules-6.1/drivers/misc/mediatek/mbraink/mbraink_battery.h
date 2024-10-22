/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_BATTERY_H
#define MBRAINK_BATTERY_H
#include <linux/power_supply.h>
#include "mbraink_ioctl_struct_def.h"
void mbraink_get_battery_info(struct mbraink_battery_data *battery_buffer,
			      long long timestamp);
extern int power_supply_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val);

#endif
