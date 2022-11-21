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
#include <linux/interrupt.h>
#define THM_TEST
struct sprd_thermal_zone {
	struct thermal_zone_device *therm_dev;
	struct mutex th_lock;
	struct work_struct therm_work;
	struct delayed_work resume_delay_work;
    struct delayed_work thm_read_work;
	struct sprd_thm_platform_data *trip_tab;
    struct sprd_board_sensor_config *sensor_config;
	struct thm_handle_ops *ops;
	struct delayed_work thm_logtime_work;
	enum thermal_device_mode mode;
	int sensor_id;
	int thm_cal;
	int off_set;
	int temp_inteval;
	int logtime;
	int thmlog_switch;
	struct regmap * thm_glb;
    struct spi_device *spi_dev;
	unsigned long reg_base;
	unsigned long cur_temp;
	unsigned long last_temp;
	int current_trip_num;
	int sen_cal_offset;
	char thermal_zone_name[30];
	enum thermal_trend trend_val;
};

enum thermal_log_switch{
	THERMAL_LOG_DISABLED = 0,
	THERMAL_LOG_ENABLED,
};
struct thm_handle_ops{
	int (*hw_init)(struct sprd_thermal_zone*);
	int (*get_reg_base)(struct sprd_thermal_zone*,struct resource *regs);
	unsigned long (*read_temp)(struct sprd_thermal_zone*);
	int (*get_trend)(struct sprd_thermal_zone*,int, enum thermal_trend *);
	int (*get_hyst)(struct sprd_thermal_zone*,int,unsigned long *);
	int (*irq_handle)(struct sprd_thermal_zone*);
	int (*suspend)(struct sprd_thermal_zone*);
	int (*resume)(struct sprd_thermal_zone*);
	int (*reg_debug_set)(struct sprd_thermal_zone*,  unsigned  int*);
	int (*reg_debug_get)(struct sprd_thermal_zone*,  unsigned  int*);
	int (*trip_debug_set)(struct sprd_thermal_zone*,int );
};
extern int  sprd_thm_add(struct thm_handle_ops* ops, char *p,int id);
extern void sprd_thm_delete(int id);
extern int  sci_efuse_thermal_cal_get(int *cal);
extern int  sci_efuse_arm_thm_cal_get(int *cal,int *offset);
extern int  sci_efuse_bcore_thm_cal_get(int *cal,int *offset);
extern int  sprd_boardthm_add(struct thm_handle_ops* ops, char *p,int id);
extern void sprd_boardthm_delete(int id);
#endif
