/*
 *  sec_dual_fuelgauge.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2023 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include "sec_battery.h"
#include "sec_dual_fuelgauge.h"

static enum power_supply_property sec_dual_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
};

static int sec_dual_fuelgauge_get_current(struct sec_dual_fuelgauge_info *fg, int bat_type, int unit)
{
	union power_supply_propval value;
	int inow = 0, main_inow = 0, sub_inow = 0;

	if (bat_type == SEC_DUAL_BATTERY_MAIN) {
		value.intval = unit;
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_NOW, value);
		inow = value.intval;
	} else if (bat_type == SEC_DUAL_BATTERY_SUB) {
		value.intval = unit;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_NOW, value);
		inow = value.intval;
	} else {
		value.intval = unit;
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_NOW, value);
		main_inow = value.intval;
		value.intval = unit;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_NOW, value);
		sub_inow = value.intval;
		inow = main_inow + sub_inow;
	}

	dev_dbg(fg->dev, "%s: main inow=%d%s, sub inow =%d%s, inow=%d%s\n", __func__,
		main_inow, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
		sub_inow, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
		inow, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");

	return inow;
}

static int sec_dual_fuelgauge_get_cycle(struct sec_dual_fuelgauge_info *fg, int bat_type)
{
	union power_supply_propval value;
	int cycle = 0;

	if (bat_type == SEC_DUAL_BATTERY_SUB) {
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CYCLES, value);
		cycle = value.intval;
	} else {
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CYCLES, value);
		cycle = value.intval;
	}

	return cycle;
}

static int sec_dual_fuelgauge_get_avg_current(struct sec_dual_fuelgauge_info *fg, int bat_type, int unit)
{
	union power_supply_propval value;
	int iavg = 0, main_iavg = 0, sub_iavg = 0;

	if (bat_type == SEC_DUAL_BATTERY_MAIN) {
		value.intval = unit;
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
		iavg = value.intval;
	} else if (bat_type == SEC_DUAL_BATTERY_SUB) {
		value.intval = unit;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
		iavg = value.intval;
	} else {
		value.intval = unit;
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
		main_iavg = value.intval;
		value.intval = unit;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
		sub_iavg = value.intval;
		iavg = main_iavg + sub_iavg;
	}

	dev_dbg(fg->dev, "%s: main inow=%d%s, sub inow =%d%s, inow=%d%s\n", __func__,
		main_iavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
		sub_iavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
		iavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");

	return iavg;
}

static int sec_dual_fuelgauge_get_isys(struct sec_dual_fuelgauge_info *fg, int unit)
{
	union power_supply_propval value;
	enum power_supply_ext_property psp = POWER_SUPPLY_EXT_PROP_MEASURE_SYS;
	int isys = 0;
#ifdef CONFIG_IFPMIC_LIMITER
	int curr = 0;
#endif

#ifdef CONFIG_IFPMIC_LIMITER
	psy_do_property(fg->pdata->main_fg_name, get,
		psp, value);
	curr = sec_dual_fuelgauge_get_current(fg, SEC_DUAL_BATTERY_SUB, unit);
	/* Chargig (Only SC) : MAIN_ISYS - (SUB INOW) */
	/* Discharging : MAIN_ISYS - (-SUB INOW) */
	if ((fg->is_charging && (curr > 0) && (value.intval > curr)) ||
		(!fg->is_charging && (curr < 0))) {
		isys = value.intval - curr;
		pr_info("%s: isys:%d%s - sub bat:%d%s = isys:%d%s\n", __func__,
			value.intval, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
			curr, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
			isys, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
	} else {
		isys = value.intval;
		pr_info("%s: isys:%d%s\n", __func__,
			isys, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
	}
#else
	psy_do_property(fg->pdata->main_fg_name, get,
		psp, value);
	isys = value.intval;
	pr_info("%s: isys:%d%s\n", __func__,
		isys, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
#endif

	return isys;
}

static int sec_dual_fuelgauge_get_isysavg(struct sec_dual_fuelgauge_info *fg, int unit)
{
	union power_supply_propval value;
	enum power_supply_ext_property psp = POWER_SUPPLY_EXT_PROP_MEASURE_SYS;
	int isysavg = 0;
#ifdef CONFIG_IFPMIC_LIMITER
	int curr = 0;
#endif

#ifdef CONFIG_IFPMIC_LIMITER
	psy_do_property(fg->pdata->main_fg_name, get,
		psp, value);
	curr = sec_dual_fuelgauge_get_avg_current(fg, SEC_DUAL_BATTERY_SUB, unit);
	/* Chargig (Only SC) : MAIN_ISYS - (SUB INOW) */
	/* Discharging : MAIN_ISYS - (-SUB INOW) */
	if ((fg->is_charging && (curr > 0) && (value.intval > curr)) ||
		(!fg->is_charging && (curr < 0))) {
		isysavg = value.intval - curr;
		pr_info("%s: isysavg:%d%s - sub avg:%d%s = isysavg:%d%s\n", __func__,
			value.intval, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
			curr, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA",
			isysavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
	} else {
		isysavg = value.intval;
		pr_info("%s: isysavg:%d%s\n", __func__,
			isysavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
	}
#else
	psy_do_property(fg->pdata->main_fg_name, get,
		psp, value);
	isysavg = value.intval;
	pr_info("%s: isysavg:%d%s\n", __func__,
		isysavg, (unit == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");
#endif

	return isysavg;
}

static unsigned int sec_dual_fuelgauge_get_voltage(struct sec_dual_fuelgauge_info *fg, int bat_type)
{
	union power_supply_propval value = {0, };

	if (bat_type == SEC_DUAL_BATTERY_MAIN) {
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	} else if (bat_type == SEC_DUAL_BATTERY_SUB) {
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	}

	return value.intval;
}

static unsigned int sec_dual_fuelgauge_get_avg_voltage(struct sec_dual_fuelgauge_info *fg, int bat_type)
{
	union power_supply_propval value = {0, };

	if (bat_type == SEC_DUAL_BATTERY_MAIN) {
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	} else if (bat_type == SEC_DUAL_BATTERY_SUB) {
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	}

	return value.intval;
}

static unsigned int sec_dual_fuelgauge_get_ocv(struct sec_dual_fuelgauge_info *fg, int bat_type)
{
	union power_supply_propval value = {0, };

	if (bat_type == SEC_DUAL_BATTERY_MAIN) {
		psy_do_property(fg->pdata->main_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_OCV, value);
	} else if (bat_type == SEC_DUAL_BATTERY_SUB) {
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_PROP_VOLTAGE_OCV, value);
	}

	return value.intval;
}

static void sec_dual_fuelgauge_get_info(struct sec_dual_fuelgauge_info *fg)
{
	union power_supply_propval value = {0, };
	int main_dcap, sub_dcap, main_iavg, sub_iavg, main_inow, sub_inow, main_fullcaprep, sub_fullcaprep, main_fullcapnom, sub_fullcapnom, main_cycle, sub_cycle;

	/* get dcap */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
	main_dcap = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
	sub_dcap = value.intval;

	/* get iavg */
	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
	main_iavg = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
	sub_iavg = value.intval;

	/* get inow */
	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
	main_inow = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_PROP_CURRENT_AVG, value);
	sub_inow = value.intval;

	/* get fullcaprep */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPREP, value);
	main_fullcaprep = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPREP, value);
	sub_fullcaprep = value.intval;

	/* get fullcapnom */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
	main_fullcapnom = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
	sub_fullcapnom = value.intval;

	/* get cycle */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CYCLES, value);
	main_cycle = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CYCLES, value);
	sub_cycle = value.intval;

	pr_info("%s: main_dcap:%d, sub_dcap:%d, main_iavg:%d, sub_iavg:%d, main_inow:%d, sub_inow:%d, main_fullcaprep:%d, sub_fullcaprep:%d, main_fullcapnom:%d, sub_fullcapnom:%d, main_cycle:%d, sub_cycle:%d\n",
		__func__, main_dcap, sub_dcap, main_iavg, sub_iavg, main_inow, sub_inow, main_fullcaprep, sub_fullcaprep, main_fullcapnom, sub_fullcapnom, main_cycle, sub_cycle);
}

static unsigned int sec_dual_fuelgauge_get_soc(struct sec_dual_fuelgauge_info *fg, int bat_type)
{
	union power_supply_propval value;
	unsigned int soc = 0;

	/* get repsoc */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_REPSOC, value);
	fg->main_repsoc = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_REPSOC, value);
	fg->sub_repsoc = value.intval;

	/* get fullcaprep */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPREP, value);
	fg->main_fullcaprep = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPREP, value);
	fg->sub_fullcaprep = value.intval;
	fg->fullcaprep = fg->main_fullcaprep + fg->sub_fullcaprep;

	/* get repcap */
	value.intval = 0;
	psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_REPCAP, value);
	fg->main_repcap = value.intval;

	value.intval = 0;
	psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_REPCAP, value);
	fg->sub_repcap = value.intval;

	pr_info("%s: main repsoc=%d, sub repsoc=%d, main repcap=%d, sub repcap=%d, main_fullcaprep=%d, sub_fullcaprep=%d, fullcaprep=%d\n",
		__func__, fg->main_repsoc, fg->sub_repsoc,
		fg->main_repcap, fg->sub_repcap, fg->main_fullcaprep,
		fg->sub_fullcaprep, fg->fullcaprep);

	/* 1. total soc by repsoc for debug */
	if ((fg->main_repsoc < 0) || (fg->sub_repsoc < 0)) {
		soc = 0;
		pr_err("%s: [by repsoc] Abnormal condition FG returned -1 values : soc=%d\n",
			__func__, soc);
	} else {
		soc = ((fg->main_repsoc * fg->pdata->main_design_capacity) + (fg->sub_repsoc * fg->pdata->sub_design_capacity)) / fg->pdata->design_capacity;
		pr_info("%s: [by repsoc] main soc=%d.%d%%, sub soc=%d.%d%% -> soc=%d.%d%%\n",
			__func__, fg->main_repsoc/10, fg->main_repsoc%10,
			fg->sub_repsoc/10, fg->sub_repsoc%10,
			soc/10, soc%10);
	}

	/* 2. total soc by repcap */
	if (fg->is_523k_jig) {
		value.intval = 0;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_VFREMCAP, value);
		fg->sub_vfremcap = value.intval;

		value.intval = 0;
		psy_do_property(fg->pdata->sub_fg_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
		fg->sub_fullcapnom = value.intval;

		pr_info("%s:[Jig Case] main repcap=%d, sub vfremcap=%d, main fullcaprep=%d, sub fullcapnom=%d\n",
			__func__, fg->main_repcap, fg->sub_vfremcap,
			fg->main_fullcaprep, fg->sub_fullcapnom);

		if ((fg->main_repcap < 0) || (fg->sub_vfremcap < 0) || (fg->main_fullcaprep < 0) || (fg->sub_fullcapnom < 0)) {
			soc = 0;
			pr_err("%s: [Jig case][by repcap] Abnormal condition FG returned -1 values : returning soc=%d\n",
				__func__, soc);
		} else if ((fg->main_fullcaprep + fg->sub_fullcapnom) == 0) {
			soc = 0;
			pr_err("%s: [Jig case][by repcap] Abnormal condition (mainfullcaprep + subfullcapnom = 0), returning soc=%d\n",
				__func__, soc);
		} else {
			soc = (fg->main_repcap + fg->sub_vfremcap) * 1000 / (fg->main_fullcaprep + fg->sub_fullcapnom);
			pr_err("%s: [Jig Case][by repcap] (main repcap=%d + sub vfremcap=%d) / (main fullcaprep=%d + sub fullcapnom=%d) -> soc=%d.%d%%\n",
				__func__, fg->main_repcap, fg->sub_vfremcap,
				fg->main_fullcaprep, fg->sub_fullcapnom,
				soc/10, soc%10);
		}
	} else {
		if ((fg->main_repcap < 0) || (fg->sub_repcap < 0) || (fg->main_fullcaprep < 0) || (fg->sub_fullcaprep < 0)) {
			soc = 0;
			pr_err("%s: [by repcap] Abnormal condition FG returned -1 values : returning soc=%d\n",
				__func__, soc);
		} else if (fg->fullcaprep == 0) {
			soc = 0;
			pr_err("%s: [by repcap] Abnormal condition Fullcaprep = 0, returning soc=%d\n",
				__func__, soc);
		} else {
			soc = (fg->main_repcap + fg->sub_repcap) * 1000 / fg->fullcaprep;
			pr_info("%s: [by repcap] (main repcap=%d + sub repcap=%d) / (main fullcap=%d + sub fullcap=%d) -> soc=%d.%d%%\n",
				__func__, fg->main_repcap, fg->sub_repcap,
				fg->main_fullcaprep, fg->sub_fullcaprep,
				soc/10, soc%10);
		}
	}

#if !defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
	/* get only integer part */
	soc /= 10;
#endif
	fg->soc = soc;

	return fg->soc;
}

#if defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
static unsigned int sec_dual_fuelgauge_get_atomic_capacity(
	struct sec_dual_fuelgauge_info *fuelgauge, unsigned int soc)
{
	pr_info("%s : NOW(%d), OLD(%d)\n",
		__func__, soc, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
	    SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		if (fuelgauge->capacity_old < soc)
			soc = fuelgauge->capacity_old + 1;
		else if (fuelgauge->capacity_old > soc)
			soc = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
	    SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
		    fuelgauge->capacity_old < soc) {
			pr_err("%s: capacity (old %d : new %d)\n",
			       __func__, fuelgauge->capacity_old, soc);
			soc = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = soc;

	return soc;
}

static int sec_dual_fuelgauge_check_capacity_max(
	struct sec_dual_fuelgauge_info *fuelgauge, int capacity_max)
{
	int cap_max, cap_min;

	cap_max = fuelgauge->pdata->capacity_max;
	cap_min =
		(fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin);

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

static void sec_dual_fuelgauge_calculate_dynamic_scale(
	struct sec_dual_fuelgauge_info *fuelgauge, int capacity, bool scale_by_full)
{
	union power_supply_propval raw_soc_val;
	int min_cap = fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin;
	int scaling_factor = 1;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc_val.intval = sec_dual_fuelgauge_get_soc(fuelgauge, SEC_DUAL_BATTERY_TOTAL);
	if (raw_soc_val.intval < 0) {
		pr_info("%s: failed to get raw soc\n", __func__);
		return;
	}

	//raw_soc_val.intval = raw_soc_val.intval / 10;

	if (capacity < 100)
		fuelgauge->capacity_max_conv = false;  //Force full sequence , need to decrease capacity_max

	if ((raw_soc_val.intval < min_cap) || (fuelgauge->capacity_max_conv)) {
		pr_info("%s: skip routine - raw_soc(%d), min_cap(%d), cap_max_conv(%d)\n",
			__func__, raw_soc_val.intval, min_cap, fuelgauge->capacity_max_conv);
		return;
	}

	if (capacity == 100)
		scaling_factor = 2;

	fuelgauge->capacity_max = (raw_soc_val.intval * 100 / (capacity + scaling_factor));
	fuelgauge->capacity_old = capacity;

	fuelgauge->capacity_max =
		sec_dual_fuelgauge_check_capacity_max(fuelgauge, fuelgauge->capacity_max);

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);
	if ((capacity == 100) && !fuelgauge->capacity_max_conv && scale_by_full) {
		fuelgauge->capacity_max_conv = true;
		fuelgauge->g_capacity_max = 990;
		pr_info("%s: Goal capacity max %d\n", __func__, fuelgauge->g_capacity_max);
	}
}

static unsigned int sec_dual_fuelgauge_get_scaled_capacity(
	struct sec_dual_fuelgauge_info *fuelgauge, unsigned int soc)
{
	int raw_soc = soc;

	soc = (soc < fuelgauge->pdata->capacity_min) ?
	    0 : ((soc - fuelgauge->pdata->capacity_min) * 1000 /
		 (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_info("%s : capacity_max (%d) scaled capacity(%d.%d), raw_soc(%d.%d)\n",
		__func__, fuelgauge->capacity_max, soc / 10, soc % 10,
		raw_soc / 10, raw_soc % 10);

	return soc;
}

static void sec_dual_fuelgauge_adjust_capacity_max(
	struct sec_dual_fuelgauge_info *fuelgauge, int curr_raw_soc)
{
	int diff = 0;

	if (fuelgauge->is_charging && fuelgauge->capacity_max_conv) {
		diff = curr_raw_soc - fuelgauge->prev_raw_soc;

		if ((diff >= 1) && (fuelgauge->capacity_max < fuelgauge->g_capacity_max)) {
			fuelgauge->capacity_max++;
		} else if ((fuelgauge->capacity_max >= fuelgauge->g_capacity_max) || (curr_raw_soc == 1000)) {
			fuelgauge->g_capacity_max = 0;
			fuelgauge->capacity_max_conv = false;
		}
		pr_info("%s: curr_raw_soc(%d) prev_raw_soc(%d) capacity_max_conv(%d) Capacity Max(%d | %d)\n",
			__func__, curr_raw_soc, fuelgauge->prev_raw_soc, fuelgauge->capacity_max_conv,
			fuelgauge->capacity_max, fuelgauge->g_capacity_max);
	}

	fuelgauge->prev_raw_soc = curr_raw_soc;
}

static unsigned int sec_dual_fuelgauge_get_uisoc(
	struct sec_dual_fuelgauge_info *fuelgauge, union power_supply_propval *val)
{
	int scale_to = 1020;

	if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
		val->intval = sec_dual_fuelgauge_get_soc(fuelgauge, SEC_DUAL_BATTERY_TOTAL) * 10;
		pr_info("%s: read raw soc (%d)\n", __func__, val->intval);
	} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT) {
		val->intval = fuelgauge->raw_capacity % 10;
	} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
		val->intval = fuelgauge->raw_capacity;
		pr_info("%s: dynamic scaled capacity (%d)\n", __func__, val->intval);
	} else {
		val->intval = sec_dual_fuelgauge_get_soc(fuelgauge, SEC_DUAL_BATTERY_TOTAL);

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {

			sec_dual_fuelgauge_adjust_capacity_max(fuelgauge, val->intval);

			val->intval = sec_dual_fuelgauge_get_scaled_capacity(fuelgauge, val->intval);

			if (val->intval > scale_to) {
				pr_info("%s: scaled capacity (%d)\n", __func__, val->intval);
				sec_dual_fuelgauge_calculate_dynamic_scale(fuelgauge, 100, false);
			}
		}
		/* capacity should be between 0% and 100%
		 * (0.1% degree)
		 */
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		fuelgauge->raw_capacity = val->intval;

		/* get only integer part */
		val->intval /= 10;

		/* Check UI soc reached 100% from 99% , no need to adjust now */
		if ((val->intval == 100) && (fuelgauge->capacity_old < 100) &&
			(fuelgauge->capacity_max_conv == true))
			fuelgauge->capacity_max_conv = false;

		/* (Only for atomic capacity)
		 * In initial time, capacity_old is 0.
		 * and in resume from sleep,
		 * capacity_old is too different from actual soc.
		 * should update capacity_old
		 * by val->intval in booting or resume.
		 */

		if (fuelgauge->initial_update_of_soc) {
			fuelgauge->initial_update_of_soc = false;
			fuelgauge->capacity_old = val->intval;
			return val->intval;
		}

		if (fuelgauge->sleep_initial_update_of_soc) {
			// updated old capacity in case of resume
			if (fuelgauge->is_charging ||
				((!fuelgauge->is_charging) && (fuelgauge->capacity_old >= val->intval))) {
				fuelgauge->capacity_old = val->intval;
				fuelgauge->sleep_initial_update_of_soc = false;
				return val->intval;
			}
		}

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)) {
			val->intval = sec_dual_fuelgauge_get_atomic_capacity(fuelgauge, val->intval);
		}
	}
	return val->intval;
}
#endif

static void sec_dual_fg_bd_log(struct sec_dual_fuelgauge_info *fg)
{
	memset(fg->d_buf, 0x0, sizeof(fg->d_buf));

	snprintf(fg->d_buf, sizeof(fg->d_buf),
		"%d,%d,%d",
		fg->bd_vfocv,
		fg->soc,
		fg->capacity_max);
}

static int sec_dual_fuelgauge_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_dual_fuelgauge_info *fg = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sec_dual_fuelgauge_get_voltage(fg, SEC_DUAL_BATTERY_MAIN);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = sec_dual_fuelgauge_get_avg_voltage(fg, SEC_DUAL_BATTERY_MAIN);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = sec_dual_fuelgauge_get_ocv(fg, SEC_DUAL_BATTERY_MAIN);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sec_dual_fuelgauge_get_current(fg, SEC_DUAL_BATTERY_TOTAL, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = sec_dual_fuelgauge_get_avg_current(fg, SEC_DUAL_BATTERY_TOTAL, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&fg->fg_lock);
#if defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
		val->intval = sec_dual_fuelgauge_get_uisoc(fg, val);
#else
		val->intval = sec_dual_fuelgauge_get_soc(fg, val->intval);
#endif
		mutex_unlock(&fg->fg_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fg->soc * fg->pdata->design_capacity;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			val->intval = fg->pdata->design_capacity;
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = fg->pdata->design_capacity;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fg->capacity_max;
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		{
			int fullcapnom_main, fullcapnom_sub;
			int main_dcap, sub_dcap;

			value.intval = 0;
			psy_do_property(fg->pdata->main_fg_name, get,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
			fullcapnom_main = value.intval;

			value.intval = 0;
			psy_do_property(fg->pdata->main_fg_name, get,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
			main_dcap = value.intval;

			value.intval = 0;
			psy_do_property(fg->pdata->sub_fg_name, get,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
			fullcapnom_sub = value.intval;

			value.intval = 0;
			psy_do_property(fg->pdata->sub_fg_name, get,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
			sub_dcap = value.intval;

			if ((main_dcap + sub_dcap) > 0) {
				val->intval = (fullcapnom_main + fullcapnom_sub) * 100 /
					(main_dcap + sub_dcap);
			}
			pr_info("%s: asoc(%d), fullcapnom_m(%d),fullcapnom_s(%d), main_dcap(%d), sub_dcap(%d)\n", __func__,
				val->intval, fullcapnom_main, fullcapnom_sub,
					main_dcap, sub_dcap);
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DESIGNCAP:
			val->intval = 0;
			break;
		case POWER_SUPPLY_EXT_PROP_REPSOC:
			switch (val->intval) {
			case SEC_DUAL_BATTERY_MAIN:
				val->intval = fg->main_repsoc;
				break;
			case SEC_DUAL_BATTERY_SUB:
				val->intval = fg->sub_repsoc;
				break;
			default:
				val->intval = fg->repsoc;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FG_SOC:
			switch (val->intval) {
			case SEC_DUAL_BATTERY_MAIN:
				val->intval = fg->main_repsoc;
				break;
			case SEC_DUAL_BATTERY_SUB:
				if (fg->is_523k_jig) {
					value.intval = 0;
					psy_do_property(fg->pdata->sub_fg_name, get,
							(enum power_supply_property)POWER_SUPPLY_EXT_PROP_VFSOC, value);
					val->intval = value.intval;
				} else {
					val->intval = fg->sub_repsoc;
				}
				break;
			default:
				val->intval = fg->repsoc;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_REPCAP:
			switch (val->intval) {
			case SEC_DUAL_BATTERY_MAIN:
				val->intval = fg->main_repcap;
				break;
			case SEC_DUAL_BATTERY_SUB:
				val->intval = fg->sub_repcap;
				break;
			default:
				val->intval = fg->main_repcap + fg->sub_repcap;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FULLCAPREP:
			switch (val->intval) {
			case SEC_DUAL_BATTERY_MAIN:
				val->intval = fg->main_fullcaprep;
				break;
			case SEC_DUAL_BATTERY_SUB:
				val->intval = fg->sub_fullcaprep;
				break;
			default:
				val->intval = fg->main_fullcaprep + fg->sub_fullcaprep;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FULLCAPNOM:
			val->intval = 0;
			break;
		case POWER_SUPPLY_EXT_PROP_CYCLES:
			val->intval = sec_dual_fuelgauge_get_cycle(fg, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			switch (val->intval) {
			case SEC_BATTERY_VSYS:
				psy_do_property(fg->pdata->main_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			case SEC_BATTERY_ISYS_AVG_UA:
				val->intval = sec_dual_fuelgauge_get_isysavg(fg, SEC_BATTERY_CURRENT_UA);
				break;
			case SEC_BATTERY_ISYS_AVG_MA:
				val->intval = sec_dual_fuelgauge_get_isysavg(fg, SEC_BATTERY_CURRENT_MA);
				break;
			case SEC_BATTERY_ISYS_UA:
				val->intval = sec_dual_fuelgauge_get_isys(fg, SEC_BATTERY_CURRENT_UA);
				break;
			case SEC_BATTERY_ISYS_MA:
			default:
				val->intval = sec_dual_fuelgauge_get_isys(fg, SEC_BATTERY_CURRENT_MA);
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_VBYP:
				psy_do_property(fg->pdata->main_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			case SEC_BATTERY_IIN_UA:
				val->intval = SEC_BATTERY_CURRENT_UA;
				psy_do_property(fg->pdata->main_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			case SEC_BATTERY_IIN_MA:
			default:
				val->intval = SEC_BATTERY_CURRENT_MA;
				psy_do_property(fg->pdata->main_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			switch (val->intval) {
			case SEC_DUAL_BATTERY_MAIN:
				psy_do_property(fg->pdata->main_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			case SEC_DUAL_BATTERY_SUB:
				psy_do_property(fg->pdata->sub_fg_name, get,
					psp, value);
				val->intval = value.intval;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_DUMP:
			sec_dual_fg_bd_log(fg);
			val->strval = fg->d_buf;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_do_property(fg->pdata->main_fg_name, get,
				psp, value);
			psy_do_property(fg->pdata->sub_fg_name, get,
				psp, value);
			sec_dual_fuelgauge_get_info(fg);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = fg->is_charging;
			break;
		case POWER_SUPPLY_EXT_PROP_MAIN_ENERGY_FULL:
			{
				int fullcapnom_main;
				int main_dcap;

				value.intval = 0;

				psy_do_property(fg->pdata->main_fg_name, get,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
				fullcapnom_main = value.intval;

				value.intval = 0;
				psy_do_property(fg->pdata->main_fg_name, get,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
				main_dcap = value.intval;

				if ((main_dcap) > 0)
					val->intval = (fullcapnom_main) * 100 / (main_dcap);

				pr_info("%s: main_asoc(%d), fullcapnom_m(%d), main_dcap(%d)\n", __func__,
					val->intval, fullcapnom_main, main_dcap);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_SUB_ENERGY_FULL:
			{
				int fullcapnom_sub;
				int sub_dcap;

				value.intval = 0;

				psy_do_property(fg->pdata->sub_fg_name, get,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULLCAPNOM, value);
				fullcapnom_sub = value.intval;

				value.intval = 0;
				psy_do_property(fg->pdata->sub_fg_name, get,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DESIGNCAP, value);
				sub_dcap = value.intval;

				if ((sub_dcap) > 0)
					val->intval = (fullcapnom_sub) * 100 / (sub_dcap);

				pr_info("%s: sub_asoc(%d),fullcapnom_s(%d), sub_dcap(%d)\n", __func__,
					val->intval, fullcapnom_sub, sub_dcap);
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_dual_fuelgauge_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_dual_fuelgauge_info *fg = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_TEMP:
	{
		value.intval = val->intval;
		psy_do_property(fg->pdata->main_fg_name, set, POWER_SUPPLY_PROP_TEMP, value);
		psy_do_property(fg->pdata->sub_fg_name, set, POWER_SUPPLY_PROP_TEMP, value);
	}
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* fuelgauge reset before battery insertion control */
			if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB) {
				value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB;
				psy_do_property(fg->pdata->sub_fg_name, set, POWER_SUPPLY_PROP_CAPACITY, value);
				pr_info("%s :Sub FG: do reset SOC for battery insertion control [VCELL_EQUAL_VBATT]\n", __func__);
			} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB_PKCP) {
				value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB_PKCP;
				psy_do_property(fg->pdata->sub_fg_name, set, POWER_SUPPLY_PROP_CAPACITY, value);
				pr_info("%s :Sub FG: do reset SOC for battery insertion control [VCELL_EQUAL_PKCP]\n", __func__);
			} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET_DUAL) {
				value.intval = 1;
				psy_do_property(fg->pdata->main_fg_name, set,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CAPACITY_DUAL, value);
				pr_info("%s : Main FG: do reset SOC for battery insertion control dual\n", __func__);

				value.intval = 1;
				psy_do_property(fg->pdata->sub_fg_name, set,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CAPACITY_DUAL, value);
				pr_info("%s : Sub FG: do reset SOC for battery insertion control [VCELL_EQUAL_PKCP] dual\n",
					__func__);
				msleep(800);

				value.intval = 2;
				psy_do_property(fg->pdata->main_fg_name, set,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CAPACITY_DUAL, value);
				pr_info("%s : Main FG: do calculate SOC for battery insertion control dual\n",
					__func__);
				msleep(1600);

				value.intval = 2;
				psy_do_property(fg->pdata->sub_fg_name, set,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CAPACITY_DUAL, value);
				pr_info("%s : Sub FG: do read SOC for battery insertion control dual\n", __func__);
			} else {
				value.intval = val->intval;
				psy_do_property(fg->pdata->main_fg_name, set, POWER_SUPPLY_PROP_CAPACITY, value);
				pr_info("%s :Main FG: do reset SOC for battery insertion control\n", __func__);
			}
			fg->initial_update_of_soc = true;
		break;
#if defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fg->pdata->capacity_calculation_type &
		    SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
			sec_dual_fuelgauge_calculate_dynamic_scale(fg, val->intval, true);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		mutex_lock(&fg->fg_lock);
		pr_info("%s: capacity_max changed, %d -> %d\n",
			__func__, fg->capacity_max, val->intval);
		fg->capacity_max =
			sec_dual_fuelgauge_check_capacity_max(fg, val->intval);
		fg->initial_update_of_soc = true;
		fg->g_capacity_max = 990;
		fg->capacity_max_conv = true;
		mutex_unlock(&fg->fg_lock);
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_523K_JIG:
			fg->is_523k_jig = true;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			switch (val->intval) {
			case SEC_BAT_CHG_MODE_BUCK_OFF:
			case SEC_BAT_CHG_MODE_CHARGING_OFF:
				fg->is_charging = false;
				break;
			case SEC_BAT_CHG_MODE_CHARGING:
			case SEC_BAT_CHG_MODE_PASS_THROUGH:
				fg->is_charging = true;
				break;
			};
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if !defined(CONFIG_SEC_FACTORY)
static int check_dependent_fg_probe(struct sec_dual_fuelgauge_info *fg)
{
	union power_supply_propval value;
	int ret = 0;
	int main_probe = 0;
	int sub_probe = 0;

	value.intval = 0;
	ret = psy_do_property(fg->pdata->main_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FG_PROBE_STATUS, value);
	if (ret < 0) {
		pr_err("%s: Main FG probe psy read failed\n", __func__);
		goto fail_status;
	}
	main_probe = value.intval;

	value.intval = 0;
	ret = psy_do_property(fg->pdata->sub_fg_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FG_PROBE_STATUS, value);
	if (ret < 0) {
		pr_err("%s: Sub FG probe psy read failed\n", __func__);
		goto fail_status;
	}
	sub_probe = value.intval;

	pr_info("%s: main_probe = %d, sub_probe = %d\n", __func__, main_probe, sub_probe);

	if (main_probe && sub_probe)
		return 0;

fail_status:
	return -EPROBE_DEFER;
}
#endif

#ifdef CONFIG_OF
static int sec_dual_fuelgauge_parse_dt(struct device *dev,
		struct sec_dual_fuelgauge_info *fg)
{
	struct device_node *np = dev->of_node;
	//struct sec_dual_fuelgauge_platform_data *pdata = fg->pdata;
	int ret = 0;
	//int len;
	//const u32 *p;

	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return 1;
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
					&fg->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s: error reading capacity_max %d\n", __func__, ret);
		pr_info("%s : capacity_max : %d\n", __func__, fg->pdata->capacity_max);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
					   &fg->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s: error reading capacity_min %d\n", __func__, ret);
		pr_info("%s : capacity min : %d\n", __func__, fg->pdata->capacity_min);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fg->pdata->capacity_max_margin);
		if (ret < 0) {
			pr_err("%s: error reading capacity_max_margin %d\n",
				__func__, ret);
			fg->pdata->capacity_max_margin = 300;
		}
		pr_info("%s : capacity min : %d\n", __func__, fg->pdata->capacity_max_margin);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
					&fg->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s: error reading capacity_calculation_type %d\n", __func__, ret);
		pr_info("%s : capacity calculation type : %d\n", __func__, fg->pdata->capacity_calculation_type);

		ret = of_property_read_string(np, "battery,main_fuelgauge_name",
				(char const **)&fg->pdata->main_fg_name);
		if (ret)
			pr_err("%s: main_fg_name is Empty\n", __func__);

		ret = of_property_read_string(np, "battery,sub_fuelgauge_name",
				(char const **)&fg->pdata->sub_fg_name);
		if (ret)
			pr_err("%s: sub_fg_name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,main_design_capacity",
			&fg->pdata->main_design_capacity);
		if (ret)
			pr_info("%s : main_design_capacity is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,sub_design_capacity",
			&fg->pdata->sub_design_capacity);
		if (ret)
			pr_info("%s : sub_design_capacity is Empty\n", __func__);

		fg->pdata->design_capacity = fg->pdata->main_design_capacity + fg->pdata->sub_design_capacity;
	}
	return 0;
}
#endif

static const struct power_supply_desc sec_dual_fuelgauge_power_supply_desc = {
	.name = "sec-dual-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_dual_fuelgauge_props,
	.num_properties = ARRAY_SIZE(sec_dual_fuelgauge_props),
	.get_property = sec_dual_fuelgauge_get_property,
	.set_property = sec_dual_fuelgauge_set_property,
};

static int sec_dual_fuelgauge_probe(struct platform_device *pdev)
{
	struct sec_dual_fuelgauge_info *fg;
	struct sec_dual_fuelgauge_platform_data *pdata = NULL;
	struct power_supply_config dual_fg_cfg = {};
	union power_supply_propval raw_soc_val;
	int ret = 0;

	dev_info(&pdev->dev,
		"%s: SEC Dual Fuelgauge Driver Loading\n", __func__);

	fg = kzalloc(sizeof(*fg), GFP_KERNEL);
	if (!fg)
		return -ENOMEM;

	mutex_init(&fg->fg_lock);
	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_dual_fuelgauge_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_fg_free;
		}

		fg->pdata = pdata;
		if (sec_dual_fuelgauge_parse_dt(&pdev->dev, fg)) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec-dual-fuelgauge dt\n", __func__);
			ret = -EINVAL;
			goto err_fg_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		fg->pdata = pdata;
	}

#if !defined(CONFIG_SEC_FACTORY)
	ret = check_dependent_fg_probe(fg);
	if (ret < 0) {
		pr_err("%s: Eprobe defer\n", __func__);
		goto err_pdata_free;
	}
#endif

	fg->capacity_max = fg->pdata->capacity_max;
#if defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
	raw_soc_val.intval = sec_dual_fuelgauge_get_soc(fg, SEC_DUAL_BATTERY_TOTAL);
#else
	raw_soc_val.intval = sec_dual_fuelgauge_get_soc(fg, SEC_DUAL_BATTERY_TOTAL) * 10;
#endif
	fg->sleep_initial_update_of_soc = false;
	fg->initial_update_of_soc = true;

#if defined(CONFIG_SEC_DUAL_FG_DYNAMIC_SCALING)
	if (raw_soc_val.intval > fg->capacity_max)
		sec_dual_fuelgauge_calculate_dynamic_scale(fg, 100, false);
#endif

	platform_set_drvdata(pdev, fg);
	fg->dev = &pdev->dev;
	dual_fg_cfg.drv_data = fg;
	fg->is_523k_jig = false;

	fg->psy_bat = power_supply_register(&pdev->dev, &sec_dual_fuelgauge_power_supply_desc, &dual_fg_cfg);
	if (IS_ERR(fg->psy_bat)) {
		ret = PTR_ERR(fg->psy_bat);
		dev_err(fg->dev,
			"%s: Failed to Register psy_bat(%d)\n", __func__, ret);
		goto err_pdata_free;
	}

	sec_chg_set_dev_init(SC_DEV_DUAL_FG);
	dev_info(fg->dev,
		"%s: SEC Dual Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_pdata_free:
	devm_kfree(&pdev->dev, pdata);
err_fg_free:
	kfree(fg);
	pr_err("%s: freed\n", __func__);
	return ret;
}

static int sec_dual_fuelgauge_remove(struct platform_device *pdev)
{
	struct sec_dual_fuelgauge_info *fg = platform_get_drvdata(pdev);

	power_supply_unregister(fg->psy_bat);

	dev_dbg(fg->dev, "%s: End\n", __func__);

	kfree(fg->pdata);
	kfree(fg);

	return 0;
}

static int sec_dual_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int sec_dual_fuelgauge_resume(struct device *dev)
{
	struct sec_dual_fuelgauge_info *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->sleep_initial_update_of_soc = true;

	pr_debug("%s: ++\n", __func__);

	return 0;
}

static void sec_dual_fuelgauge_shutdown(struct platform_device *pdev)
{
}

#ifdef CONFIG_OF
static struct of_device_id sec_dual_fuelgauge_dt_ids[] = {
	{ .compatible = "samsung,sec-dual-fuelgauge" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_dual_fuelgauge_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_dual_fuelgauge_pm_ops = {
	.suspend = sec_dual_fuelgauge_suspend,
	.resume = sec_dual_fuelgauge_resume,
};

static struct platform_driver sec_dual_fuelgauge_driver = {
	.driver = {
		.name = "sec-dual-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &sec_dual_fuelgauge_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_dual_fuelgauge_dt_ids,
#endif
	},
	.probe = sec_dual_fuelgauge_probe,
	.remove = sec_dual_fuelgauge_remove,
	.shutdown = sec_dual_fuelgauge_shutdown,
};

static int __init sec_dual_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&sec_dual_fuelgauge_driver);
}

static void __exit sec_dual_fuelgauge_exit(void)
{
	platform_driver_unregister(&sec_dual_fuelgauge_driver);
}

device_initcall_sync(sec_dual_fuelgauge_init);
module_exit(sec_dual_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung Dual Fuelgauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
