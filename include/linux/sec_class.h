/*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifdef CONFIG_DRV_SAMSUNG
extern struct device *sec_dev_get_by_name(char *name);
extern struct device *sec_device_create(void *drvdata, const char *fmt);
extern void sec_device_destroy(dev_t devt);
extern struct device *sec_device_find(const char *name);

#else
#define sec_dev_get_by_name(name)		NULL
#define sec_device_create(drvdata, fmt)	NULL
#define sec_device_destroy(devt)		NULL
#endif /* CONFIG_DRV_SAMSUNG */
