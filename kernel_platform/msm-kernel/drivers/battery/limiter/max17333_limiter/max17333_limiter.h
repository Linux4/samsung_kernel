/*
 * Copyright (C) 2023 Maximintegrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17333_CHARGER_H_
#define __MAX17333_CHARGER_H_

#include "../../common/sec_charging_common.h"

enum {
	MAX17333_CHG_CURRENT = 0,
	MAX17333_CHG_EN,
	MAX17333_CHG_VOLTAGE,
	MAX17333_PRECHG_CURRENT,
	MAX17333_PREQUAL_VOLTAGE,
	MAX17333_SHIP_EN,
	MAX17333_COMMSH_EN,
};

ssize_t max17333_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max17333_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define MAX17333_CHG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = max17333_chg_show_attrs,			\
	.store = max17333_chg_store_attrs,			\
}

struct max17333_charger_data {
	struct device           *dev;
	struct max17333_dev     *max17333;
	struct regmap			*regmap;
	struct regmap			*regmap_shdw;

	int						prot_irq;
	struct power_supply		*psy_chg;
	struct power_supply_desc		psy_chg_d;

	/* mutex */
	struct mutex			lock;

	int cycles_reg_lsb_percent;
	/* rsense */
	unsigned int rsense;
	unsigned int battery_type;
};

#endif // __MAX17333_CHARGER_H_
