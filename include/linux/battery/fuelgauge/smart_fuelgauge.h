/*
 * smart_fuelgauge.h
 * Samsung SMART BATTERY Fuel Gauge Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is 77843 under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SMART_FUELGAUGE_H
#define __SMART_FUELGAUGE_H __FILE__

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif

#include <linux/battery/sec_charging_common.h>

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define PRINT_COUNT	10

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2
#define MAX_LOW_BATT_CHECK_CNT	10

#define SMARTFG_MANUFACTURE_ACESS 0x00
#define SMARTFG_REMCAP_ALARM 0x01
#define SMARTFG_REMTIME_ALARM 0x02
#define SMARTFG_BATT_MODE 0x03
#define SMARTFG_ATRATE 0x04
#define SMARTFG_ATRATE_TTF 0x05
#define SMARTFG_ATRATE_TTE 0x06
#define SMARTFG_ATRATE_OK 0x07
#define SMARTFG_TEMP 0x08
#define SMARTFG_VOLT 0x09
#define SMARTFG_CURR 0x0A
#define SMARTFG_AVG_CURR 0x0B
#define SMARTFG_MAX_ERROR 0x0C
#define SMARTFG_RELATIVE_SOC 0x0D
#define SMARTFG_ABSOLUTE_SOC 0x0E
#define SMARTFG_REMCAP 0x0F
#define SMARTFG_FULLCAP 0x10
#define SMARTFG_RUN_TTE 0x11
#define SMARTFG_AVG_TTE 0x12
#define SMARTFG_CHG_CURR 0x14
#define SMARTFG_CHG_VOLT 0x15
#define SMARTFG_BATT_STATUS 0x16
#define SMARTFG_CYCLE_CNT 0x17
#define SMARTFG_DESIGNCAP 0x18
#define SMARTFG_DESIGNVOLT 0x19
#define SMARTFG_SPEC_INFO 0x1A
#define SMARTFG_MANUFACTURE_DATE 0x1B
#define SMARTFG_SERIAL_NM 0x1C
#define SMARTFG_MANUFACTURER_NAME 0x20
#define SMARTFG_DEV_NAME 0x21
#define SMARTFG_DEV_CHEMISTRY 0x22
#define SMARTFG_MANUFACTURER_DATA 0x23
#define SMARTFG_OPTIONAL_MFG_FUNC5 0x2F
#define SMARTFG_OPTIONAL_MFG_FUNC4 0x3C
#define SMARTFG_OPTIONAL_MFG_FUNC3 0x3D
#define SMARTFG_OPTIONAL_MFG_FUNC2 0x3E
#define SMARTFG_OPTIONAL_MFG_FUNC1 0x3F
#define SMARTFG_BATT_ALARM 0x90

struct smart_fg_info {
	/* test print count */
	int pr_cnt;
	/* full charge comp */
	struct delayed_work	full_comp_work;
	u32 previous_fullcap;
	u32 previous_vffullcap;

	/* low battery boot */
	int low_batt_boot_flag;
	bool is_low_batt_alarm;

	/* battery info */
	u32 soc;

	/* miscellaneous */
	unsigned long fullcap_check_interval;
	int full_check_flag;
	bool is_first_check;
};

enum {
	FG_LEVEL = 0,
	FG_TEMPERATURE,
	FG_VOLTAGE,
	FG_CURRENT,
	FG_CURRENT_AVG,
	FG_CHECK_STATUS,
	FG_RAW_SOC,
	FG_VF_SOC,
	FG_AV_SOC,
	FG_FULLCAP,
	FG_FULLCAPNOM,
	FG_FULLCAPREP,
	FG_MIXCAP,
	FG_AVCAP,
	FG_REPCAP,
	FG_CYCLE,
};

enum {
	POSITIVE = 0,
	NEGATIVE,
};

enum {
	RANGE = 0,
	SLOPE,
	OFFSET,
	TABLE_MAX
};

#define CURRENT_RANGE_MAX_NUM	5

/* FullCap learning setting */
#define VFFULLCAP_CHECK_INTERVAL	300 /* sec */
/* soc should be 0.1% unit */
#define VFSOC_FOR_FULLCAP_LEARNING	950
#define LOW_CURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_CURRENT_FOR_FULLCAP_LEARNING	120
#define LOW_AVGCURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_AVGCURRENT_FOR_FULLCAP_LEARNING	100

/* power off margin */
/* soc should be 0.1% unit */
#define POWER_OFF_SOC_HIGH_MARGIN	20
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500
#define POWER_OFF_VOLTAGE_LOW_MARGIN	3400

/* FG recovery handler */
/* soc should be 0.1% unit */
#define STABLE_LOW_BATTERY_DIFF	30
#define STABLE_LOW_BATTERY_DIFF_LOWBATT	10
#define LOW_BATTERY_SOC_REDUCE_UNIT	10

struct cv_slope{
	int fg_current;
	int soc;
	int time;
};
struct smart_fuelgauge {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct mutex            i2c_lock;
	sec_fuelgauge_platform_data_t *pdata;
	struct power_supply		psy_fg;
	struct delayed_work isr_work;

	int cable_type;
	bool is_charging;

	/* HW-dedicated fuel guage info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */
	struct smart_fg_info	info;

	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity;
	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */
	unsigned int standard_capacity;

	bool initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	unsigned int pre_soc;
	int fg_irq;

	int raw_capacity;
	int current_now;
	int current_avg;
	struct cv_slope *cv_data;
	int cv_data_lenth;

	unsigned int low_temp_limit;
	unsigned int low_temp_recovery;
};
#endif /* __SMART_FUELGAUGE_H */
