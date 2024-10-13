/*
 *  sec_dual_fuelgauge.h
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_DUAL_FUELGAUGE_H
#define __SEC_DUAL_FUELGAUGE_H __FILE__

#include "sec_charging_common.h"

struct sec_dual_fuelgauge_platform_data {
	char *main_fg_name;
	char *sub_fg_name;
	unsigned int main_design_capacity;
	unsigned int sub_design_capacity;
	unsigned int design_capacity;

	int capacity_max;
	int capacity_calculation_type;
	int capacity_max_margin;
	int capacity_min;
};

struct sec_dual_fuelgauge_info {
	struct device *dev;
	struct sec_dual_fuelgauge_platform_data *pdata;
	struct power_supply		*psy_bat;

	unsigned int main_voltage_now;		/* cell voltage (mV) */
	unsigned int sub_voltage_now;		/* cell voltage (mV) */
	unsigned int main_voltage_avg;		/* average voltage (mV) */
	unsigned int sub_voltage_avg;		/* average voltage (mV) */
	int main_current_now;		/* current (mA) */
	int sub_current_now;		/* current (mA) */
	int current_now;			/* current (mA) */
	int main_current_avg;		/* average current (mA) */
	int sub_current_avg;		/* average current (mA) */
	int current_avg;			/* average current (mA) */
	int main_repcap;			/* capacity (mhA) */
	int sub_repcap;			/* capacity (mhA) */
	unsigned int repcap;				/* capacity (mhA) */
	int main_fullcaprep;		/* capacity (mhA) */
	int sub_fullcaprep;		/* capacity (mhA) */
	int fullcaprep;			/* capacity (mhA) */
	int main_fullcapnom;		/* capacity (mhA) */
	int sub_fullcapnom;		/* capacity (mhA) */
	int fullcapnom;			/* capacity (mhA) */
	int main_repsoc;			/* sub repsoc (%) */
	int sub_repsoc;			/* main repsoc (%) */
	int sub_vfremcap;			/* sub vfremcap (%) */
	bool is_523k_jig;					/* is 523k jig (%) */
	unsigned int repsoc;				/* total soc by repsoc (%) */
	unsigned int soc;					/* total soc (%) */
	unsigned int main_soh;				/* soh (%) */
	unsigned int sub_soh;				/* soh (%) */
	unsigned int soh;					/* soh (%) */

	unsigned int chg_mode;
	unsigned int status;
	int age_step;

	struct mutex fg_lock;
	bool capacity_max_conv;
	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */
	unsigned int g_capacity_max;	/* only for dynamic calculation */
	bool is_charging;
	int prev_raw_soc;
	int raw_capacity;
	bool initial_update_of_soc;
	bool sleep_initial_update_of_soc;
	bool initial_update_of_alert;

	char d_buf[128];
	int bd_vfocv;
	//int bd_raw_soc;
};

#endif /* __SEC_DUAL_FUELGAUGE_H */
