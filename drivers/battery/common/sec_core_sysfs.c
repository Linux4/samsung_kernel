/*
 *  sec_core_sysfs.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/power_supply.h>
#include "sec_battery_sysfs.h"

static LIST_HEAD(sysfs_list);
int sec_bat_create_attrs(struct device *dev)
{
	struct sec_sysfs *psysfs;
	unsigned long i = 0;
	int rc = 0;

	list_for_each_entry(psysfs, &sysfs_list, list) {
		for (i = 0; i < psysfs->size; i++) {
			rc = device_create_file(dev, &psysfs->attr[i]);
			if (rc) {
				while (i--)
					device_remove_file(dev, &psysfs->attr[i]);
				break;
			}
		}
	}

	INIT_LIST_HEAD(&sysfs_list);
	return 0;
}

void add_sec_sysfs(struct list_head *plist)
{
	struct power_supply *psy = NULL;

	list_add(plist, &sysfs_list);

	psy = power_supply_get_by_name("battery");
	if (!IS_ERR_OR_NULL(psy)) {
		sec_bat_create_attrs(&psy->dev);
		power_supply_put(psy);
	}
}
