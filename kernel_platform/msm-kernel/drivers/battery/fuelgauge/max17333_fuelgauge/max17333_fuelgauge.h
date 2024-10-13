/*
 * Copyright (C) 2022 Maximintegrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17333_BATTERY_H_
#define __MAX17333_BATTERY_H_

#include "../../common/sec_charging_common.h"

#define MAX17333_BATTERY_FULL	100
#define MAX17333_BATTERY_LOW	15

/* Need to be increased if there are more than 2 BAT ID GPIOs */
#define BAT_GPIO_NO	1

enum {
	MAX17333_FG_VCELL = 0,
	MAX17333_FG_AVGVCELL,
	MAX17333_FG_CURRENT,
	MAX17333_FG_AVGCURRENT,
	MAX17333_FG_AVGCELL,
	MAX17333_FG_QH,
	MAX17333_FG_DESIGNCAP,
	MAX17333_FG_FULLCAPREP,
	MAX17333_FG_FULLCAPNOM,
	MAX17333_FG_AVCAP,
	MAX17333_FG_REPCAP,
	MAX17333_FG_MIXCAP,
	MAX17333_FG_AVSOC,
	MAX17333_FG_REPSOC,
	MAX17333_FG_MIXSOC,
	MAX17333_FG_AGE,
	MAX17333_FG_AGEFORECAST,
	MAX17333_FG_CYCLES,
	MAX17333_FG_TEMP,
	MAX17333_FG_PORCMD,
};

ssize_t max17333_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max17333_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define MAX17333_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = max17333_fg_show_attrs,			\
	.store = max17333_fg_store_attrs,			\
}

struct max17333_fg_platform_data {
	int volt_min; /* in mV */
	int volt_max; /* in mV */
	int temp_min; /* in DegreC */
	int temp_max; /* in DegreeC */
	int soc_max;  /* in percent */
	int soc_min;  /* in percent */
	int curr_max; /* in mA */
	int curr_min; /* in mA */
	int bat_id_gpio[BAT_GPIO_NO];
	int bat_gpio_cnt;
};

struct max17333_fg_data {
	struct device           *dev;
	struct max17333_dev     *max17333;
	struct regmap			*regmap;
	struct regmap			*regmap_shdw;

	int						fg_irq;
	struct power_supply		*psy_fg;
	struct power_supply_desc		psy_batt_d;

	/* mutex */
	struct mutex			lock;

	/* rsense */
	unsigned int rsense;
	unsigned int battery_type;

	struct max17333_fg_platform_data	*pdata;
	u8 battery_id;
};
#endif // __MAX17333_BATTERY_H_
