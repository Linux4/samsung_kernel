/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SENSOR_POWER_H_
#define _SENSOR_POWER_H_

#include <linux/types.h>

int sensor_power_on(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2);
int sensor_power_off(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2);

#if defined(CONFIG_MACH_J1MINI3G)
int sensor_power_init(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2);
#endif

#endif
