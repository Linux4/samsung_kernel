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

extern int sensors_create_symlink(struct kobject *, const char *);
extern void sensors_remove_symlink(struct kobject *, const char *);

extern int sensors_register(struct device *, void *,
	struct device_attribute *[], char *);
extern void sensors_unregister(struct device *,
	struct device_attribute *[]);

void remap_sensor_data(s16 *, u32);


#endif	/* __LINUX_SENSORS_CORE_H_INCLUDED */
