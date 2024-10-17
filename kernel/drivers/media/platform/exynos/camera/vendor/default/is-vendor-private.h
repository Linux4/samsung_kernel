/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vendor functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_PRIVATE_H
#define IS_VENDOR_PRIVATE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

struct is_vendor_rom {
	void *client;
	bool valid;
};

struct is_vendor_private {
	struct is_vendor_rom	rom[ROM_ID_MAX];

	u32			sensor_id[SENSOR_POSITION_MAX];
	const char		*sensor_name[SENSOR_POSITION_MAX];

	u32			aperture_sensor_index;
	u32			mcu_sensor_index;
	bool			zoom_running;
	int			is_dualized[SENSOR_POSITION_MAX];
};

#endif
