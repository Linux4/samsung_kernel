/*
 * Copyright (C) 2023 Maximintegrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX77775_LIMITER_H_
#define __MAX77775_LIMITER_H_
#include <linux/mfd/core.h>
#include <linux/mfd/max77775.h>
#include <linux/mfd/max77775-private.h>
#include <linux/regulator/machine.h>
#include <linux/pm_wakeup.h>
#include "../../common/sec_charging_common.h"

struct max77775_limiter_platform_data {
	char *charger_name;
	char *fuelgauge_name;
	unsigned int battery_type;
};

struct max77775_limiter_info {
	struct device *dev;
	struct max77775_limiter_platform_data *pdata;
	struct power_supply     *psy_sw;
};

#endif // __MAX77775_LIMITER_H_
