/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ************************************************
 * Automatically generated C config: don't edit *
 ************************************************
 */

#ifndef __CTL_THM_H__
#define __CTL_THM_H__
#include <linux/types.h>
#include <linux/sprd_thm.h>

struct sprd_thermal_zone {
	struct thermal_zone_device *therm_dev;
	struct mutex th_lock;
	struct work_struct therm_work;
	struct delayed_work resume_delay_work;
	struct sprd_thm_platform_data *trip_tab;
	enum thermal_device_mode mode;
	int sensor_id;
	void __iomem *reg_base;
	char thermal_zone_name[30];
};

extern int sprd_thm_chip_id_check(void);
extern int sprd_thm_hw_init(struct sprd_thermal_zone *pzone);
extern int sprd_thm_hw_irq_handle(struct sprd_thermal_zone *pzone);
extern int sprd_thm_hw_suspend(struct sprd_thermal_zone *pzone);
extern int sprd_thm_hw_resume(struct sprd_thermal_zone *pzone);
extern int sprd_thm_temp_read(struct sprd_thermal_zone *pzone);

#endif
