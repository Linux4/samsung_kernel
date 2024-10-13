/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_POWER_RAIL_H
#define __EXYNOS_POWER_RAIL_H __FILE__

#define EXYNOS_POWER_RAIL_DBG_PREFIX	"EXYNOS-POWER-RAIL-DBG: "

#define INFO_SIZE	(SZ_64K)
#define RGT_STEP_SIZE	(6250)
#define POWER_RAIL_SIZE	12

int exynos_power_rail_dbg_init(void);

typedef struct max_volt {
	unsigned char req_id;
	unsigned char volt;
} IDLE_DATA;

struct acpm_idle_data {
	IDLE_DATA max_voltage[POWER_RAIL_SIZE];
};

struct exynos_power_rail_info {
	unsigned int* domain_list;
	unsigned int size;
};

struct exynos_power_rail_dbg_info {
	struct device *dev;
	struct acpm_idle_data* idle_data;
	struct exynos_power_rail_info* power_rail_info;
	struct dentry *d;
	struct file_operations fops;
};
#endif
