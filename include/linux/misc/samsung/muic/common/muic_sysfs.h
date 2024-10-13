/*
 * Copyright (C) 2019 Samsung Electronics
 * Sejong Park <sejong123.park@samsung.com>
 * Taejung Kim <tj.kim@samsung.com>
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

#ifndef MUIC_SYSFS_H
#define MUIC_SYSFS_H

#include <linux/misc/samsung/muic/common/muic.h>

#if IS_ENABLED(CONFIG_ERD_MUIC_SYSFS)
extern struct device *muic_device_create(void *drvdata, const char *fmt);
extern void muic_device_destroy(dev_t devt);
int muic_sysfs_init(struct muic_platform_data *muic_pdata);
void muic_sysfs_deinit(struct muic_platform_data *muic_pdata);
#else
static inline struct device *muic_device_create(void *drvdata, const char *fmt)
{
	pr_err("No rule to make muic sysfs\n");
	return NULL;
}
static inline void muic_device_destroy(dev_t devt)
{
	return;
}
#endif

#endif /* MUIC_SYSFS_H */
