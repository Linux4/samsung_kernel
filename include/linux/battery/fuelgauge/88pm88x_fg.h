/*
 * 88pm88x_fg.h
 * 88PM88X PMIC Fuelgauge Header
 *
 * Copyright (C) 2015 Marvell Technology Ltd.
 *
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

#ifndef __88PM88X_FG_H
#define __88PM88X_FG_H

#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm88x.h>

#define PM88X_VBAT_MEAS_EN		(1 << 1)
#define PM88X_GPADC_BD_PREBIAS		(1 << 4)
#define PM88X_GPADC_BD_EN		(1 << 5)
#define PM88X_BD_GP_SEL			(1 << 6)
#define PM88X_BATT_TEMP_SEL		(1 << 7)
#define PM88X_BATT_TEMP_SEL_GP1		(0 << 7)
#define PM88X_BATT_TEMP_SEL_GP3		(1 << 7)

#define PM88X_VBAT_LOW_TH		(0x18)

#define PM88X_CC_CONFIG1		(0x01)
#define PM88X_CC_EN			(1 << 0)
#define PM88X_CC_CLR_ON_RD		(1 << 2)
#define PM88X_SD_PWRUP			(1 << 3)

#define PM88X_CC_CONFIG2		(0x02)
#define PM88X_CC_READ_REQ		(1 << 0)
#define PM88X_OFFCOMP_EN		(1 << 1)

#define PM88X_CC_VAL1			(0x03)
#define PM88X_CC_VAL2			(0x04)
#define PM88X_CC_VAL3			(0x05)
#define PM88X_CC_VAL4			(0x06)
#define PM88X_CC_VAL5			(0x07)

#define PM88X_IBAT_VAL1			(0x08)		/* LSB */
#define PM88X_IBAT_VAL2			(0x09)
#define PM88X_IBAT_VAL3			(0x0a)

#define PM88X_IBAT_EOC_CONFIG		(0x0f)
#define PM88X_IBAT_MEAS_EN		(1 << 0)

#define PM88X_IBAT_EOC_MEAS1		(0x10)		/* bit [7 : 0] */
#define PM88X_IBAT_EOC_MEAS2		(0x11)		/* bit [15: 8] */

#define PM88X_CC_LOW_TH1		(0x12)		/* bit [7 : 0] */
#define PM88X_CC_LOW_TH2		(0x13)		/* bit [15 : 8] */

#define PM88X_VBAT_AVG_MSB		(0xa0)
#define PM88X_VBAT_AVG_LSB		(0xa1)
#define PM88X_BATTEMP_MON_EN		(1 << 4)
#define PM88X_BATTEMP_MON2_DIS		(1 << 5)

#define PM88X_VBAT_SLP_MSB		(0xb0)
#define PM88X_VBAT_SLP_LSB		(0xb1)

#define PM88X_SLP_CNT1			(0xce)

#define PM88X_SLP_CNT2			(0xcf)
#define PM88X_SLP_CNT_HOLD		(1 << 6)
#define PM88X_SLP_CNT_RST		(1 << 7)

#define MONITOR_INTERVAL		(HZ * 30)
#define LOW_BAT_INTERVAL		(HZ * 5)
#define LOW_TEMP_MONITOR_INTERVAL		(HZ * 10)
#define LOW_TEMP_LOW_BAT_INTERVAL		(HZ * 2)
#define LOW_BAT_CAP			(15)

#define PM88X_GPADC_RTC_SPARE0	(0xc0)
#define PM88X_RTC_SPARE0_BIT_1	(1 << 1)
#define PM88X_RTC_SPARE0_BIT_2	(1 << 2)
#define PM88X_RTC_SPARE0_BIT_3	(1 << 3)

#define ROUND_SOC(_x)			(((_x) + 5) / 10 * 10)

#ifdef CONFIG_USB_MV_UDC
#include <linux/platform_data/mv_usb.h>
#endif

/* this flag is used to decide whether the ocv_flag needs update */
static atomic_t in_resume = ATOMIC_INIT(0);
/*
 * this flag shows that the voltage with load is too low:
 * if it is lower than safe_power_off_th
 * or lower than 3.0V, this flag is true;
 */
static bool is_extreme_low;

enum {
	ALL_SAVED_DATA,
	SLEEP_COUNT,
};

enum {
	VBATT_CHAN,
	VBATT_SLP_CHAN,
	TEMP_CHAN,
	MAX_CHAN = 3,
};

struct pm88x_bat_irq {
	int		irq;
	unsigned long disabled;
};

struct pm88x_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int ibat;	/* mA */
	int soc;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;

	/* battery cycle */
	int  cycle_nums;
};

struct temp_vs_ohm {
	unsigned int ohm;
	int temp;
};

struct pm88x_battery_info {
	struct mutex		cycle_lock;
	struct mutex		volt_lock;
	struct mutex		update_lock;
	struct pm88x_chip	*chip;
	struct device	*dev;
	struct notifier_block		nb;
	struct pm88x_battery_params	bat_params;

	struct power_supply	battery;
	struct delayed_work	monitor_work;
	struct delayed_work	charged_work;
	struct delayed_work	cc_work; /* sigma-delta offset compensation */
	struct workqueue_struct *bat_wqueue;

	int			total_capacity;
	int			fixup_init_soc;

	bool		use_ntc;
	bool		bat_temp_monitor_en;
	bool		use_sw_bd;
	int			gpadc_det_no;
	int			gpadc_temp_no;

	int			ocv_is_realiable;
	int			range_low_th;
	int			range_high_th;
	int			sleep_counter_th;

	int			power_off_th;
	int			safe_power_off_th;

	int			alart_percent;

	int			irq_nums;
	struct pm88x_bat_irq	irqs[7];

	struct iio_channel	*chan[MAX_CHAN];
	struct temp_vs_ohm	*temp_ohm_table;
	int			temp_ohm_table_size;
	int			zero_degree_ohm;
	int			default_temp_ohm;

	int			abs_lowest_temp;
	int			t1;
	int			t2;
	int			t3;
	int			t4;

	int times_in_minus_ten;
	int times_in_zero;

	int offset_in_minus_ten;
	int offset_in_zero;

	int  soc_low_th_cycle;
	int  soc_high_th_cycle;

	int cc_fixup;
	bool valid_cycle;
};

static int ocv_table[100];

struct ccnt {
	int soc;	/* mC, 1mAH = 1mA * 3600 = 3600 mC */
	int max_cc;
	int last_cc;
	int cc_offs;
	int alart_cc;
	int real_last_cc;
	int real_soc;
	int previous_soc;
	bool bypass_cc;
};
static struct ccnt ccnt_data;

/*
 * the save_buffer mapping:
 *
 * | o o o o   o o o | o ||         o      | o o o   o o o o |
 * |<--- cycles ---->|   ||ocv_is_realiable|      SoC        |
 * |---RTC_SPARE6(0xef)--||-----------RTC_SPARE5(0xee)-------|
 */
struct save_buffer {
	int soc;
	int ocv_is_realiable;
	int padding;
	int cycles;
};
static struct save_buffer extern_data;

#endif /* __88PM88X_FG_H */
