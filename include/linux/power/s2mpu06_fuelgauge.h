/*
 * s2mpu06_fuelgauge.h
 * Samsung S2MPU06 Fuel Gauge Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
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
 */

#ifndef __S2MPU06_FUELGAUGE_H
#define __S2MPU06_FUELGAUGE_H __FILE__
#include <linux/mfd/samsung/s2mpu06.h>
#include <linux/mfd/samsung/s2mpu06-private.h>

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif


/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define S2MPU06_FG_REG_STATUS		0x00
#define S2MPU06_FG_REG_IRQ		0x02
#define S2MPU06_FG_REG_RVBAT		0x04
#define S2MPU06_FG_REG_ROCV		0x06
#define S2MPU06_FG_REG_RCUR		0x08
#define S2MPU06_FG_REG_RSOC_SYS		0x0A
#define S2MPU06_FG_REG_RSOC		0x0C
#define S2MPU06_FG_REG_RTEMP		0x0E
#define S2MPU06_FG_REG_RBATCAP		0x10
#define S2MPU06_FG_REG_RZADJ		0x12
#define S2MPU06_FG_REG_RBATZ0		0x14
#define S2MPU06_FG_REG_RBATZ1		0x16
#define S2MPU06_FG_REG_IRQ_LVL		0x18
#define S2MPU06_FG_REG_START		0x1A
#define S2MPU06_FG_REG_MONSEL		0x1E
#define S2MPU06_FG_REG_MONOUT_SEL	0x1F
#define S2MPU06_FG_REG_MONOUT		0x20
#if defined(CONFIG_BATTERY_AGE_FORECAST)
#define S2MPU06_FG_REG_CELL_CHAR0	0x34
#define S2MPU06_FG_REG_CELL_CHAR0_NUM	164
#define S2MPU06_FG_REG_CELL_CHAR1	0x0F
#define S2MPU06_FG_REG_CELL_CHAR1_NUM	5
#endif

struct sec_fg_info {
	/* test print count */
	int pr_cnt;
	/* full charge comp */
	u32 previous_fullcap;
	u32 previous_vffullcap;
	/* low battery comp */
	int low_batt_comp_flag;
	/* low battery boot */
	int low_batt_boot_flag;
	bool is_low_batt_alarm;

	/* battery info */
	u32 soc;

	/* miscellaneous */
	unsigned long fullcap_check_interval;
	int full_check_flag;
	bool is_first_check;
	int data_ver;
};

#endif /* __S2MPU06_FUELGAUGE_H */
