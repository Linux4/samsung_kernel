/*
 * Driver model for sensor
 *
 * Copyright (C) 2008 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_SENSORS_CORE_H_INCLUDED
#define __LINUX_SENSORS_CORE_H_INCLUDED
#include <linux/alarmtimer.h>
/* report timestamp from kernel (for Android L) */
#define TIME_LO_MASK 0x00000000FFFFFFFF
#define TIME_HI_MASK 0xFFFFFFFF00000000
#define TIME_HI_SHIFT 32
extern int sensors_create_symlink(struct kobject *, const char *);
extern void sensors_remove_symlink(struct kobject *, const char *);

extern int sensors_register(struct device *, void *,
	struct device_attribute *[], char *);
extern void sensors_unregister(struct device *,
	struct device_attribute *[]);

void remap_sensor_data(s16 *, u32);


#endif	/* __LINUX_SENSORS_CORE_H_INCLUDED */
