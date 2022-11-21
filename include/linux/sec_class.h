/*
 * include/linux/sec_class.h
 *
 * COPYRIGHT(C) 2011-2017 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_DRV_SAMSUNG
extern struct device *sec_dev_get_by_name(char *name);
extern struct device *sec_device_create(dev_t devt,
			void *drvdata, const char *fmt);
extern void sec_device_destroy(dev_t devt);
#else
#define sec_dev_get_by_name(name)		NULL
#define sec_device_create(devt, drvdata, fmt)	NULL
#define sec_device_destroy(devt)		NULL
#endif /* CONFIG_DRV_SAMSUNG */
