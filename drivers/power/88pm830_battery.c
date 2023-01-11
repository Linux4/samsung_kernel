/*
 * Battery driver for Marvell 88PM830 charger/fuel-gauge chip
 *
 * Copyright (c) 2013 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *		Jett Zhou <jtzhou@marvell.com>
 *		Shay Pathov <shayp@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/mfd/88pm830.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include "88pm830_fg.h"

#define MONITOR_INTERVAL		(HZ * 60)
#define LOW_BAT_INTERVAL		(HZ * 10)

/* coulomb counter regieter */
#define PM830_CC_AVG(x)			(x << 4)
#define PM830_CC_CLR_ON_READ		(1 << 2)
#define PM830_DEC_2ND_ORD_EN		(1 << 1)
#define PM830_CC_EN			(1 << 0)

#define PM830_CC_CTRL2			(0x25)
#define PM830_OFFCOMP_EN		(1 << 1)
#define PM830_CC_READ_REQ		(1 << 0)

/* cc 0x26 ~ 0x2A */
#define PM830_CCNT_VAL1			(0x26)

#define PM830_CC_FLAG			(0x2E)
#define PM830_IBAT_OFFVAL_REG		(0x2F)
#define PM830_CC_MISC1			(0x30)
#define PM830_IBAT_SLP_SEL_A1	(1 << 3)
#define PM830_CC_MISC2			(0x32)
#define PM830_IBAT_SLP_TH(x)		(x << 4)
#define PM830_VBAT_SMPL_NUM(x)		(x << 1)
#define PM830_IBAT_EOC_EN		(1 << 0)

#define PM830_SLP_TIMER			(0x35)

#define PM830_BAT_CTRL2			(0x3f)
#define PM830_BAT_PRTY_EN		(0x1 << 1)

#define PM830_VBAT_AVG_SD1		(0x37)
#define PM830_VBAT_SLP1			(0x39)
#define PM830_VBAT_LOW_TH		(0x76)
#define PM830_VBAT_AVG1			(0xBA)

/* bit definitions of GPADC Bias Current 1 Register */
#define GP0_BIAS_SET			(5)

/* GPADC0 Bias Current value in uA unit */
#define GP0_BIAS_SET_UA			((GP0_BIAS_SET) * 5 + 1)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) > (b)) ? (b) : (a))

#define NTC_BIAS_CHANGE_TEMP1	(info->switch_thr[0])
#define NTC_BIAS_CHANGE_TEMP2	(info->switch_thr[1])
#define NTC_TABLE_SIZE	(info->ntc_table_size)
static int ocv_table[100];

static char *pm830_supply_to[] = {
	"ac",
	"usb",
};

struct ccnt {
	int debug;

	/* mC, 1mAH = 1mA * 3600 = 3600 mC */
	int soc;
	int max_cc;
	int last_cc;
	int cc_offs;
	int cc_int;
};
static struct ccnt ccnt_data;

struct buffed {
	int soc;
	int temp;
	int use_ocv;
};
static struct buffed extern_data;

/* fake SOC, we use it to make sure SOC jumping step <= 1 for Android */
static int cap;

static int pm830_get_fg_internal_soc(void)
{
	return ccnt_data.soc;
}

static int pm800_battery_update_buffed(struct pm830_battery_info *info,
				       struct buffed *value)
{
	int data;
	/* save values in RTC registers on PMIC */
	data = (value->soc & 0x7f) | (value->use_ocv << 7);

	/* Bits 0,1 are in use for different purpose,
	 * so give up on 2 LSB of temperatue */
	data |= (((value->temp / 10) + 50) & 0xfc) << 8;
	set_extern_data(info, PMIC_STORED_DATA, data);

	return 0;
}

static int pm800_battery_get_buffed(struct pm830_battery_info *info,
				    struct buffed *value)
{
	int data;

	/* read values from RTC registers on PMIC */
	data = get_extern_data(info, PMIC_STORED_DATA);
	value->soc = data & 0x7f;
	value->use_ocv = (data & 0x80) >> 7;
	value->temp = (((data >> 8) & 0xfc) - 50) * 10;
	if (value->temp < 0)
		value->temp = 0;

	/* If register is 0, then stored values are invalid */
	if (data == 0)
		return -1;

	return 0;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int pm830_get_batt_vol(struct pm830_battery_info *info, int active)
{
	int ret, data;
	unsigned char buf[2];

	if (active) {
		ret = regmap_bulk_read(info->chip->regmap,
				       PM830_VBAT_AVG1, buf, 2);
		if (ret < 0)
			return ret;
		data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	} else {
		ret = regmap_bulk_read(info->chip->regmap,
				       PM830_VBAT_SLP1, buf, 2);
		if (ret < 0)
			return ret;
		data = (buf[0] & 0xff) | ((buf[1] & 0x0f) << 8);
	}

	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	data = pm830_get_adc_volt(PM830_GPADC_VBAT, data);
	dev_dbg(info->dev, "%s--> %s: vbat: %dmV\n",
		__func__, active ? "active" : "sleep", data);
	return data;
}

/* get soc from ocv: lookup table */
static int pm830_get_batt_soc(int ocv)
{
	int i, count;

	if (ocv < ocv_table[0]) {
		ccnt_data.soc = 0;
		return 0;
	}

	count = 100;
	for (i = count - 1; i >= 0; i--) {
		if (ocv >= ocv_table[i]) {
			ccnt_data.soc = i + 1;
			break;
		}
	}
	ccnt_data.last_cc = ccnt_data.max_cc / 1000 * (ccnt_data.soc * 10 + 5);
	return 0;
}

static int pm830_get_ibat_cc(struct pm830_battery_info *info)
{
	int ret, data;
	unsigned char buf[3];

	ret = regmap_bulk_read(info->chip->regmap,
			       PM830_IBAT_CC1, buf, 3);
	if (ret < 0)
		return ret;
	data = ((buf[2] & 0x3) << 16)
		| ((buf[1] & 0xff) << 8)
		| (buf[0] & 0xff);
	/* ibat is negative if discharging, poistive is charging */
	if (data & (1 << 17))
		data = (0xff << 24) | (0x3f << 18) | data;
	if (ccnt_data.debug)
		dev_dbg(info->dev, "%s--> *data = 0x%x, %d\n",
			__func__, data, data);
	data = (data * 458) / 10;
	if (ccnt_data.debug)
		dev_dbg(info->dev, "%s--> ibat_cc = %duA, %dmA\n",
			__func__, data, data / 1000);
	return data;
}

static int pm830_gp0_bias_set(struct pm830_battery_info *info, int bias)
{
	int data, ret;

	dev_info(info->dev, "%s: set bias to %d\n", __func__, info->gp0_bias_curr[bias]);

	data = BIAS_GP0_SET(info->gp0_bias_curr[bias]);
	ret = regmap_update_bits(info->chip->regmap, PM830_GPADC0_BIAS, BIAS_GP0_MASK, data);
	if (ret < 0)
		return ret;

	info->bias = bias;
	return 0;
}

/* read gpadc0 to get temperature */
static int pm830_get_batt_temp(struct pm830_battery_info *info, int *data)
{
	int ret;
	int voltage, i;
	unsigned char buf[2];
	int temp = 0;

	if (!(info->bat_params.present && info->temp_range_num)) {
		*data = 25;
		return 0;
	}

	ret = regmap_bulk_read(info->chip->regmap,
			       PM830_GPADC0_MEAS1, buf, 2);
	if (ret < 0)
		return ret;
	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* measure(mv) = value * 1.4 *1000/(2^12) */
	voltage = pm830_get_adc_volt(PM830_GPADC_GPADC0, *data);

	/* get the real temperature by looking up the table */
	for (i = 0; i < NTC_TABLE_SIZE; i++) {
		if (voltage >= info->ntc_table[info->bias][i].voltage) {
			temp = info->ntc_table[info->bias][i].temp;
			break;
		}
	}
	if (i == NTC_TABLE_SIZE)
		temp = info->ntc_table[info->bias][NTC_TABLE_SIZE - 1].temp;

	dev_dbg(info->dev, "temp_C=%dC, NTC_voltage=%d, bias=%d\n",
		temp, voltage, info->gp0_bias_curr[info->bias]);

	*data = temp;
	return ret;
}

/*	Temperature ranges:
 *
 * ----------|---------------|---------------|---------------|----------
 *           |               |               |               |
 *  Shutdown |   Charging    |   Charging    |   Charging    | Shutdown
 *           |  not allowed  |    allowed    |  not allowed  |
 * ----------|---------------|---------------|---------------|----------
 *   tbat_ext_cold	tbat_cold	tbat_hot	tbat_ext_hot
 *
 */
static int pm830_handle_tbat(struct pm830_battery_info *info)
{
	int gpadc_lo_th = 0, tbat_lo_th = 0;
	int gpadc_hi_th = 0, tbat_hi_th = 0;
	int first_temp, last_temp;
	int ret, temp, old_bias;

	if (!info->bat_params.present) {
		info->bat_params.health = POWER_SUPPLY_HEALTH_UNKNOWN;
		return 0;
	}

	if (!info->temp_range_num) {
		info->bat_params.health = POWER_SUPPLY_HEALTH_GOOD;
		return 0;
	}

	/* read battery temperature firstly */
	ret = pm830_get_batt_temp(info, &temp);
	if (ret < 0) {
		dev_dbg(info->chip->dev, "%s: read battery temperature fail\n",  __func__);
		return 0;
	}

	/* change bias if necessary, and read battery temperature again if need */
	old_bias = info->bias;
	switch (info->bias) {
	case NTC_BIAS_LOW:
		if (temp > NTC_BIAS_CHANGE_TEMP1)
			ret = pm830_gp0_bias_set(info, NTC_BIAS_MID);
		break;
	case NTC_BIAS_MID:
		if (temp < NTC_BIAS_CHANGE_TEMP1)
			ret = pm830_gp0_bias_set(info, NTC_BIAS_LOW);
		else if (temp > NTC_BIAS_CHANGE_TEMP2)
			ret = pm830_gp0_bias_set(info, NTC_BIAS_HI);
		break;
	case NTC_BIAS_HI:
		if (temp < NTC_BIAS_CHANGE_TEMP2)
			ret = pm830_gp0_bias_set(info, NTC_BIAS_MID);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret < 0) {
		dev_dbg(info->chip->dev, "%s: set gpadc0 bias current fail\n", __func__);
		return ret;
	}

	/* bias current changed, read battery temperature again */
	if (old_bias != info->bias) {
		msleep(20);
		pm830_get_batt_temp(info, &temp);
	}

	/* get battery health status according to battery temp */
	if (temp < info->tbat_thr[0]) {
		dev_err(info->chip->dev, "%s: TBAT extremely cold! shut down\n", __func__);
		info->bat_params.health = POWER_SUPPLY_HEALTH_DEAD;

	} else if (temp < info->tbat_thr[1]) {
		dev_err(info->chip->dev, "%s: TBAT too cold!\n", __func__);
		tbat_lo_th = info->tbat_thr[0];
		tbat_hi_th = info->tbat_thr[1] + 3;
		info->bat_params.health = POWER_SUPPLY_HEALTH_COLD;

	} else if (temp < info->tbat_thr[2]) {
		dev_info(info->chip->dev, "%s: TBAT is normal\n", __func__);
		tbat_lo_th = info->tbat_thr[1] - 3;
		tbat_hi_th = info->tbat_thr[2] + 3;
		info->bat_params.health = POWER_SUPPLY_HEALTH_GOOD;

	} else if (temp < info->tbat_thr[3]) {
		dev_err(info->chip->dev, "%s: TBAT too hot!\n", __func__);
		tbat_lo_th = info->tbat_thr[2] - 3;
		tbat_hi_th = info->tbat_thr[3];
		info->bat_params.health = POWER_SUPPLY_HEALTH_OVERHEAT;

	} else {
		dev_err(info->chip->dev, "%s: TBAT extremely hot! shut down\n", __func__);
		info->bat_params.health = POWER_SUPPLY_HEALTH_DEAD;
	}

	/* extremely hot or cold, system should shutdown */
	if (!tbat_lo_th && !tbat_hi_th)
		return 0;

	/* make sure the new values can't cross valid thresholds */
	switch (info->bias) {
	case NTC_BIAS_LOW:
		tbat_hi_th = MIN(tbat_hi_th, NTC_BIAS_CHANGE_TEMP1);
		break;
	case NTC_BIAS_MID:
		tbat_hi_th = MIN(tbat_hi_th, NTC_BIAS_CHANGE_TEMP2);
		tbat_lo_th = MAX(tbat_lo_th, NTC_BIAS_CHANGE_TEMP1);
		break;
	case NTC_BIAS_HI:
		tbat_lo_th = MAX(tbat_lo_th, NTC_BIAS_CHANGE_TEMP2);
		break;
	default:
		return -EINVAL;
	}

	/*
	 * get gpadc threshold:
	 * if temperature is inside the current table, find it using offset from first temperature,
	 * otherwise use the first/last temperature as threshold
	 */
	first_temp =  info->ntc_table[info->bias][0].temp;
	last_temp = info->ntc_table[info->bias][NTC_TABLE_SIZE - 1].temp;
	if (tbat_hi_th <= first_temp)
		gpadc_lo_th = info->ntc_table[info->bias][0].voltage;
	else if (tbat_hi_th >= last_temp)
		gpadc_lo_th = info->ntc_table[info->bias][NTC_TABLE_SIZE - 1].voltage;
	else
		gpadc_lo_th = info->ntc_table[info->bias][tbat_hi_th - first_temp].voltage;

	if (tbat_lo_th <= first_temp)
		gpadc_hi_th = info->ntc_table[info->bias][0].voltage;
	else if (tbat_lo_th >= last_temp)
		gpadc_hi_th = info->ntc_table[info->bias][NTC_TABLE_SIZE - 1].voltage;
	else
		gpadc_hi_th = info->ntc_table[info->bias][tbat_lo_th - first_temp].voltage;

	/* set the thresholds for next interrupt */
	ret = regmap_write(info->chip->regmap, PM830_GPADC0_LOW_TH,
			(pm830_get_adc_value(PM830_GPADC_GPADC0, gpadc_lo_th)) >> 4);
	if (ret < 0)
		return ret;

	ret = regmap_write(info->chip->regmap, PM830_GPADC0_UPP_TH,
			(pm830_get_adc_value(PM830_GPADC_GPADC0, gpadc_hi_th)) >> 4);
	if (ret < 0)
		return ret;
	dev_dbg(info->chip->dev, "%s: New thresholds: Low=%dmV, High=%dmV\n",
			__func__, gpadc_lo_th, gpadc_hi_th);

	return 0;
}

static void pm830_tbat_work(struct work_struct *work)
{
	struct pm830_battery_info *info =
		container_of(work, struct pm830_battery_info, tbat_work);

	pm830_handle_tbat(info);
	power_supply_changed(&info->battery);
}

static irqreturn_t pm830_tbat_handler(int irq, void *data)
{
	struct pm830_battery_info *info = data;

	dev_dbg(info->chip->dev, "%s: Temperature event\n", __func__);
	queue_work(info->bat_wqueue, &info->tbat_work);

	return IRQ_HANDLED;
}

static int is_charger_online(struct pm830_battery_info *info,
			     int *who_is_chg)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret = 0;

	for (i = 0; i < info->battery.num_supplicants; i++) {
		psy = power_supply_get_by_name(info->battery.supplied_to[i]);
		if (!psy || !psy->get_property)
			continue;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret == 0) {
			if (val.intval) {
				*who_is_chg = i;
				return val.intval;
			}
		}
	}
	*who_is_chg = 0;
	return 0;
}

static inline int pm830_calc_soc(struct pm830_battery_info *info,
				 struct ccnt *ccnt_val)
{
	ccnt_val->soc = ccnt_val->last_cc * 100 / ccnt_val->max_cc;

	/* need to make sure battery SoC in good range */
	if (ccnt_val->last_cc > (ccnt_val->max_cc + ccnt_val->max_cc * 5 / 1000)) {
		ccnt_val->soc = 100;
		ccnt_val->last_cc = (ccnt_val->max_cc / 1000) * (ccnt_val->soc * 10 + 5);
	}
	if (ccnt_val->last_cc < 0) {
		ccnt_val->soc = 0;
		ccnt_val->last_cc = 0;
	}

	return 0;
}

/*
 * if ocv is not reliable, we'll try to caculate a new ocv
 * only when
 * 1) battery is relaxed enough
 * and
 * 2) battery is in good range
 * then OCV will be updated;
 * OR, the caculated OCV is NOT reliable
 */
static void update_ocv_flag(struct pm830_battery_info *info,
			    struct ccnt *ccnt_val)
{
	int prev_soc, vol, slp_cnt;
	if (info->use_ocv)
		return;

	/* save old SOC in case to recover */
	prev_soc = ccnt_val->soc;

	/* check if battery is relaxed enough */
	slp_cnt = get_extern_data(info, PMIC_SLP_CNT);
	dev_dbg(info->dev, "pmic slp_cnt = %d seconds\n", slp_cnt);
	if (slp_cnt >= info->slp_con) {
		dev_info(info->dev, "Long sleep detected! %d seconds\n", slp_cnt);
		/* read last sleep voltage and calc new SOC */
		vol = pm830_get_batt_vol(info, 0);
		pm830_get_batt_soc(vol);
		/* check if the new OSC is in good range or not */
		if ((ccnt_val->soc < info->range_low_th)
		    || (ccnt_val->soc > info->range_high_th)) {
			info->use_ocv = 1;
			dev_info(info->dev, "SOC is in good range! new SOC = %d\n",
				 ccnt_val->soc);
		} else {
			/* SOC is NOT in good range, so restore previous SOC */
			dev_info(info->dev, "NOT in good range (%d), don't update\n",
				 ccnt_val->soc);
			ccnt_val->soc = prev_soc;
			ccnt_val->last_cc = (ccnt_val->max_cc / 1000)
				* (ccnt_val->soc * 10 + 5);
		}
	}
}

static int pm830_get_charger_status(struct pm830_battery_info *info)
{
	int who, ret;
	struct power_supply *psy;
	union power_supply_propval val;
	int status = POWER_SUPPLY_STATUS_UNKNOWN;

	/* report charger online timely */
	if (!is_charger_online(info, &who))
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else {
		psy = power_supply_get_by_name(info->battery.supplied_to[who]);
		if (!psy || !psy->get_property) {
			pr_info("%s: get power supply failed.\n", __func__);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		if (ret) {
			dev_err(info->dev, "get charger property failed.\n");
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			/* update battery status */
			status = val.intval;
		}
	}
	return status;
}

/* used to handle systematic error, confirmed with silicon team */
static inline int pm830_calc_ccnt_correction(struct pm830_battery_info *info,
		s64 *ccnt_mc)
{
	int vbat, count, i, x1, x2, y1, y2, ccnt_correction = 0;
	int correction_index = 0;
	s64 ccnt_mc_app;

	ccnt_mc_app = *ccnt_mc;

	/* read Vbat from 88pm830 */
	vbat = pm830_get_batt_vol(info, 1);

	if (vbat < vbat_correction_table[0])
		correction_index = 0;
	else {
		count = 6;
		for (i = count - 1; i >= 0; i--) {
			if (vbat >= vbat_correction_table[i]) {
				correction_index = i + 1;
				break;
			}
		}
	}

	if (correction_index <= 0)
		ccnt_correction = ccnt_correction_table[0];
	else if (correction_index >= 6)
		ccnt_correction = ccnt_correction_table[5];
	else {

		x1 = vbat_correction_table[correction_index-1];
		x2 = vbat_correction_table[correction_index];
		y1 = ccnt_correction_table[correction_index-1];
		y2 = ccnt_correction_table[correction_index];
		ccnt_correction = y1 + (y2 - y1) * (vbat - x1)/(x2-x1);
	}

	ccnt_mc_app = ccnt_mc_app * ccnt_correction;
	ccnt_mc_app = div_s64(ccnt_mc_app, 1000);
	(*ccnt_mc) = ccnt_mc_app;

	return 0;
}

static int pm830_fg_calc_ccnt(struct pm830_battery_info *info,
			      struct ccnt *ccnt_val)
{
	int ret, data, factor;
	unsigned char buf[5];
	s64 ccnt_uc = 0, ccnt_mc = 0;
	unsigned char buf1[2];

	ret = regmap_bulk_read(info->chip->regmap,
			       PM830_CC_FLAG, buf1, 2);
	if (ccnt_val->debug)
		dev_dbg(info->dev, "%s--> [0x2E: 0x%x], [0x2F: 0x%x]\n",
			__func__, buf1[0], buf1[1]);

	regmap_read(info->chip->regmap, PM830_CC_CTRL2, &data);
	if ((data & PM830_CC_READ_REQ) == 0)
		regmap_update_bits(info->chip->regmap,
				   PM830_CC_CTRL2,
				   PM830_CC_READ_REQ, PM830_CC_READ_REQ);

	/* data is ready when READ_REQ become low */
	do {
		regmap_read(info->chip->regmap, PM830_CC_CTRL2, &data);
	} while ((data & PM830_CC_READ_REQ) != 0);

	ret = regmap_bulk_read(info->chip->regmap, PM830_CCNT_VAL1, buf, 5);
	if (ret < 0)
		return ret;
	ccnt_uc = (s64) (((s64)(buf[4]) << 32)
			 | (u64)(buf[3] << 24) | (u64)(buf[2] << 16)
			 | (u64)(buf[1] << 8) | (u64)buf[0]);
	if (ccnt_val->debug)
		dev_dbg(info->dev, "%s--> data: 0x%llx, %lld\n", __func__, ccnt_uc, ccnt_uc);

	/* Factor is nC */
	factor = 715;
	ccnt_uc = ccnt_uc * factor;
	ccnt_uc = div_s64(ccnt_uc, 1000);
	ccnt_mc = div_s64(ccnt_uc, 1000);
	if (ccnt_val->debug)
		dev_dbg(info->dev, "%s--> ccnt_uc: %lld uC, ccnt_mc: %lld mC\n",
			__func__, ccnt_uc, ccnt_mc);

	pm830_calc_ccnt_correction(info, &ccnt_mc);
	/*
	 * add offset to fix sigma-delta issue in charge in previous versions of PM830 B2, and we
	 * have fixed this issue in PM830 B2 version.
	 */
	if (info->chip->version < PM830_B2_VERSION && ccnt_mc > 0)
		ccnt_mc += ccnt_val->cc_int * ccnt_val->cc_offs;

	ccnt_val->last_cc += ccnt_mc;

	return 0;
}

/*
 * used to make sure:
 * 1. fake SOC jumping step <= 1
 * 2. fake SOC and internal SOC can't greater than 100
 * 3. change internal SOC if need
 * 4. reduce SOC faster when it's discharing and voltage is low
 */
static void pm830_read_soc(struct pm830_battery_info *info)
{
	static int full_charge_count;
	static int dischg_count;
	static int is_boot = 1;
	static int chg_status, bat_status, soc;
	static int power_off_cnt, safe_flag, safe_power_off_cnt;
	static bool bat_full;
	int vbat_mv = pm830_get_batt_vol(info, 1);

	soc = ccnt_data.soc;
	bat_status = info->bat_params.status;
	chg_status = pm830_get_charger_status(info);
	if (bat_status == POWER_SUPPLY_STATUS_UNKNOWN) {
		/*
		 * battery status unknown, don't need to update status, just
		 * return
		 */
		dev_dbg(info->dev, "battery status unknown, dont update\n");
		return;
	}

	switch (chg_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		/* charging is still going on */
		dev_dbg(info->dev, "charge-->cap: %d, soc: %d\n", cap, soc);

		/*
		 * fake soc ramp up when it's less than internal soc, the ramp up step
		 * must less than or equal to 2
		 */
		if (soc > cap) {
			if (soc - cap > 2)
				cap++;
			else
				cap = soc;
		}

		bat_status = POWER_SUPPLY_STATUS_CHARGING;
		bat_full = false;
		dev_dbg(info->dev, "charge-->cap: %d, soc: %d\n", cap, soc);
		break;

	case POWER_SUPPLY_STATUS_FULL:
		dev_dbg(info->dev, "full-->cap: %d, soc: %d\n", cap, soc);

		/* when eoc comes, set soc to 100% immediately */
		if (!bat_full) {
			soc = 100;
			bat_full = true;
		}

		if (cap < 100) {
			/* need to increase cap every 120s until it's full */
			full_charge_count++;
			dev_dbg(info->dev, "cap: %d, full_charge_count: %d\n",
					cap, full_charge_count);
			if (full_charge_count >= 4) {
				cap++;
				full_charge_count = 0;
			}
			bat_status = POWER_SUPPLY_STATUS_CHARGING;
		} else if (soc >= 100 || cap == 100) {
			/* cap == 100 means battery is full */
			cap = 100;
			bat_status = POWER_SUPPLY_STATUS_FULL;
		}

		/* keep soc in valid range [0, 100] */
		soc = (soc > 100) ? 100 : soc;

		dev_dbg(info->dev, "full-->cap: %d, soc: %d\n", cap, soc);
		break;

	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		dev_dbg(info->dev, "discharge-->cap: %d, soc: %d\n", cap, soc);
		if (safe_flag == 1) {
			if (cap > 1) {
				safe_power_off_cnt++;
				dev_info(info->dev, "voltage lower than %d\n",
						info->safe_power_off_threshold);
				if (safe_power_off_cnt >= 1) {
					cap--;
					soc = cap;
					safe_power_off_cnt = 0;
				}
			} else if (cap > 0) {
				safe_power_off_cnt++;
				dev_info(info->dev, "voltage lower than %d\n",
						info->safe_power_off_threshold);
				if (safe_power_off_cnt > 2) {
					cap--;
					soc = cap;
					safe_power_off_cnt = 0;
				}
			}
		} else if (is_boot > 0) {
			cap = soc;
			dev_dbg(info->dev, "is_boot branch, cap = soc:%d\n",
					cap);
		} else if (soc < cap) {
			if (cap - soc > 1) {
				dischg_count++;
				dev_dbg(info->dev, "cap: %d, dischg_count:%d\n",
						cap, dischg_count);
				if (dischg_count >= 2) {
					cap--;
					dischg_count = 0;
				}
			} else
				cap = soc;
		}
		dev_dbg(info->dev, "vbat_mv = %d, safe_flag = %d, cap = %d\n",
				vbat_mv, safe_flag, cap);
		if ((!is_boot) && (vbat_mv < info->safe_power_off_threshold) &&
				(!safe_flag)) {
			cap = 1;
			safe_flag = 1;
		} else if (vbat_mv <= info->power_off_threshold) {
			/* vbat < power off threshold, decrease the cap */
			if (cap > 1) {
				/* protection for power off threshold */
				power_off_cnt++;
				dev_info(info->dev, "voltage lower than %d\n",
						info->power_off_threshold);
				if (power_off_cnt >= 1) {
					cap--;
					soc = cap;
					power_off_cnt = 0;
				}
			} else if (cap > 0) {
				power_off_cnt++;
				if (power_off_cnt > 2) {
					cap--;
					soc = cap;
					power_off_cnt = 0;
				}
			}
		} else if (cap == 0)
			cap = 1;

		bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
		bat_full = false;
		dev_dbg(info->dev, "discharge-->cap: %d, soc: %d\n", cap, soc);
		full_charge_count = 0;
		break;
	}

	info->bat_params.cap = cap;
	info->bat_params.status = bat_status;
	if (ccnt_data.soc != soc) {
		ccnt_data.soc = soc;
		ccnt_data.last_cc = (ccnt_data.max_cc / 1000) * (ccnt_data.soc
				* 10 + 5);
	}
	if (is_boot > 0)
		is_boot--;
}

/* update battery status */
static void pm830_bat_update_status(struct pm830_battery_info *info)
{
	int ibat, data = 0;

	if (info->bat_ntc) {
		pm830_get_batt_temp(info, &data);
		info->bat_params.temp = data * 10;
	} else {
		info->bat_params.temp = 260;
	}

	if (!atomic_read(&info->cc_done))
		return;

	info->bat_params.volt = pm830_get_batt_vol(info, 1);

	ibat = pm830_get_ibat_cc(info);
	info->bat_params.ibat = ibat / 1000; /* Change to mA */

	if (info->bat_params.ibat > 700)
		ccnt_data.cc_offs = 70;
	else
		ccnt_data.cc_offs = 90;

	/* charging status */
	if (info->bat_params.present == 0) {
		info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	/* update SOC using Columb counter */
	pm830_fg_calc_ccnt(info, &ccnt_data);
	pm830_calc_soc(info, &ccnt_data);

	update_ocv_flag(info, &ccnt_data);

	pm830_read_soc(info);
}

static void pm830_battery_work(struct work_struct *work)
{
	struct pm830_battery_info *info =
		container_of(work, struct pm830_battery_info,
			     monitor_work.work);
	static int prev_cap = -1;
	static int prev_volt = -1;
	static int prev_status = -1;

	pm830_bat_update_status(info);

	/* notify framework when parameters have changed */
	if ((prev_cap != info->bat_params.cap)
	|| (abs(prev_volt - info->bat_params.volt) > 100)
	|| (prev_status != info->bat_params.status)) {
		power_supply_changed(&info->battery);
		prev_cap = info->bat_params.cap;
		prev_volt = info->bat_params.volt;
		prev_status = info->bat_params.status;
		dev_info(info->dev, "cap=%d, temp=%d, volt=%d, use_ocv=%d\n",
			info->bat_params.cap, info->bat_params.temp / 10,
			info->bat_params.volt, info->use_ocv);
	}

	/* save the recent value in non-volatile memory */
	extern_data.soc = ccnt_data.soc;
	extern_data.temp = info->bat_params.temp;
	extern_data.use_ocv = info->use_ocv;
	pm800_battery_update_buffed(info, &extern_data);

	if (info->bat_params.cap <= LOW_BAT_CAP) {
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
				   LOW_BAT_INTERVAL);
		ccnt_data.cc_int = 10;
	} else {
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
				   MONITOR_INTERVAL);
		ccnt_data.cc_int = 60;
	}
}

static void pm830_charged_work(struct work_struct *work)
{
	struct pm830_battery_info *info =
		container_of(work, struct pm830_battery_info,
			     charged_work.work);

	pm830_bat_update_status(info);
	power_supply_changed(&info->battery);
	return;
}

/* need about 5s to finish offset compensation */
static void pm830_cc_work(struct work_struct *work)
{
	static int count;
	unsigned int data;

	struct pm830_battery_info *info =
		container_of(work, struct pm830_battery_info, cc_work.work);

	/* wait for offset compensation to be completed for another five minutes if need */
	regmap_read(info->chip->regmap, PM830_CC_CTRL2, &data);
	if (((data & PM830_OFFCOMP_EN) != 0) && (count++ < 5)) {
		dev_info(info->dev, "%s: wait for offset compensation to be complete\n", __func__);
		queue_delayed_work(info->bat_wqueue, &info->cc_work, HZ);
		return;
	}

	/* enable coulomb counter */
	regmap_update_bits(info->chip->regmap,
			   PM830_CC_CTRL1, PM830_CC_EN, PM830_CC_EN);

	atomic_set(&info->cc_done, 1);

	/* notify charger driver to start charging */
	power_supply_changed(&info->battery);
}

static void pm830_fg_setup(struct pm830_battery_info *info)
{
	int data, mask;

	/*
	 * 1) Enable system clock of fg
	 * 2) Sigma-delta power up
	 * done in mfd driver
	 */

	/* Ibat offset compensation */
	data = mask = PM830_OFFCOMP_EN;
	regmap_update_bits(info->chip->regmap,
			   PM830_CC_CTRL2, mask, data);

	/*
	 * enable ibat sel and eoc measurement
	 * IBAT_EOC_EN is used by charger to decide EOC
	 */
	if (info->chip->version > PM830_A1_VERSION) {
		/*
		* B0 change: use Ibat_EOC to detect Low-Power
		* state (sleep)
		*/
		data = mask = PM830_VBAT_SMPL_NUM(3)
			| PM830_IBAT_EOC_EN
			| PM830_IBAT_SLP_TH(3);
		regmap_update_bits(info->chip->regmap,
				   PM830_CC_MISC2, mask, data);
	} else {
		data = mask = PM830_IBAT_SLP_SEL_A1
			| PM830_VBAT_SMPL_NUM(3)
			| PM830_IBAT_EOC_EN
			| PM830_IBAT_SLP_TH(3);
		regmap_update_bits(info->chip->regmap,
				   PM830_CC_MISC2, mask, data);
	}

	/* enable the second order filter */
	data = mask = PM830_DEC_2ND_ORD_EN
		| PM830_CC_CLR_ON_READ
		| PM830_CC_AVG(0x6);
	regmap_update_bits(info->chip->regmap,
			PM830_CC_CTRL1, mask, data);
}

static void pm830_init_fg(struct pm830_battery_info *info)
{
	int data, mask, ret, i;
	int vbat, vbat_slp, slp_cnt, battery_removal_threshold;
	struct power_supply *psy;
	union power_supply_propval val;
	bool battery_changed = false;
	int bias_current;

	/* enable VBAT, which is used to measure voltage */
	data = mask = PM830_VBAT_MEAS_EN;
	regmap_update_bits(info->chip->regmap,
			   PM830_GPADC_MEAS_EN, mask, data);

	/* gpadc0 is used to measure NTC */
	if (info->bat_ntc) {
		data = mask = PM830_GPADC0_MEAS_EN;
		regmap_update_bits(info->chip->regmap,
				   PM830_GPADC_MEAS_EN, mask, data);

		if (info->temp_range_num)
			bias_current = info->gp0_bias_curr[info->bias];
		else
			bias_current = 16; /* uA */
		data = BIAS_GP0_SET(bias_current);
		mask = BIAS_GP0_MASK;
		regmap_update_bits(info->chip->regmap,
				   PM830_GPADC0_BIAS, mask, data);

		/*
		 * if OBM used to detect battery presentation, we don't need
		 * to configure registers for battery detect again
		 */
		if (!info->chip->obm_config_bat_det) {
			mask = PM830_GPADC0_BIAS_EN | PM830_GPADC0_GP_BIAS_OUT;
			regmap_update_bits(info->chip->regmap,
					   PM830_GPADC_BIAS_ENA, mask, mask);

			mask = PM830_BD_GP_SEL(0) | PM830_BD_EN | PM830_BD_PREBIAS;
			data = PM830_BD_GP_SEL(0) | PM830_BD_EN;
			regmap_update_bits(info->chip->regmap,
					   PM830_GPADC_CONFIG2, mask, data);
		}

		/* battery detection */
		regmap_read(info->chip->regmap,
			    PM830_STATUS, &data);

		if (PM830_BAT_DET & data)
			info->bat_params.present = 1;
		else
			info->bat_params.present = 0;
	} else {
		/* no NTC - assume battery present */
		info->bat_params.present = 1;
	}

	/* fg set up */
	pm830_fg_setup(info);

	if (info->bat_params.present == 0) {
		dev_info(info->dev, "battery not present...\n");
		vbat = pm830_get_batt_vol(info, 1);
		/*
		 * handle special debug setup:
		 * if battery not present but VBAT is valid -
		 * this means we are in power-supply mode.
		 * in that case,
		 * disable battery detection and set battery priority
		 */
		if (vbat > ocv_table[0]) {
			dev_info(info->dev, "power supply mode...\n");
			regmap_update_bits(info->chip->regmap,
				PM830_GPADC_CONFIG2, PM830_BD_EN, 0);
			regmap_update_bits(info->chip->regmap, PM830_BAT_CTRL2,
				PM830_BAT_PRTY_EN, PM830_BAT_PRTY_EN);
			info->bat_params.tech = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		}
		return;
	}

	/* set VBAT low threshold as 3.5V */
	regmap_write(info->chip->regmap,
		     PM830_VBAT_LOW_TH, 0xa0);

	/* read vbat sleep value from PMIC */
	data = get_extern_data(info, PMIC_VBAT_SLP);
	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	vbat_slp = ((data & 0xfff) * 7 * 100) >> 9;

	/* read sleep conter from PMIC */
	slp_cnt = get_extern_data(info, PMIC_SLP_CNT);

	dev_info(info->dev, "PMIC: vbat_slp = %dmV, slp_cnt = %ds\n",
		 vbat_slp, slp_cnt);

	/* read Vbat from 88pm830 */
	vbat = pm830_get_batt_vol(info, 1);
	dev_info(info->dev, "88pm830: vbat = %dmV\n", vbat);

	/*
	 * 1) 88pm800 slp_cnt is 0, use 88pm830 vbat
	 * 2) 88pm800 slp_cnt is not 0, use 88pm800 vbat_slp
	 */
	if (slp_cnt != 0)
		vbat = vbat_slp;

	/*
	 * calc SOC via present voltage:
	 * 1) stored values invalid: get soc via ocv;
	 * 2) battery changed: get soc via ocv;
	 * 3) battery no changed:
	 *	a) relaxed: good range->get soc via ocv
	 *		    bad  range->use stored value
	 *	b) no-relaxed: use stored value
	 */
	pm830_get_batt_soc(vbat);
	dev_info(info->dev, "%s: SOC caculated from VBAT is %d%%\n",
		 __func__, ccnt_data.soc);

	/* recover previous SOC from non-volatile memory */
	ret = pm800_battery_get_buffed(info, &extern_data);
	dev_info(info->dev,
		 "%s: stored: soc=%d, temp=%d, use_ocv=%d\n", __func__,
		 extern_data.soc,
		 extern_data.temp,
		 extern_data.use_ocv);

	if (ret < 0)
		battery_changed = true;

	/*
	 * decide the battery removal threshold according to SOC we caculated
	 * and battery status
	 */
	info->bat_params.status = POWER_SUPPLY_STATUS_DISCHARGING;
	for (i = 0; i < ARRAY_SIZE(pm830_supply_to); i++) {
		psy = power_supply_get_by_name(pm830_supply_to[i]);
		if (!psy || !psy->get_property)
			continue;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret == 0) {
			if (!val.intval)
				continue;
			ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS,
					&val);
			if (ret)
				info->bat_params.status =
					POWER_SUPPLY_STATUS_DISCHARGING;
			else
				info->bat_params.status = val.intval;
			break;
		}
	}

	if ((info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING) ||
		(info->bat_params.status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		if (ccnt_data.soc < 5)
			battery_removal_threshold = 5;
		else if (ccnt_data.soc < 15)
			battery_removal_threshold = 10;
		else if (ccnt_data.soc < 80)
			battery_removal_threshold = 15;
		else
			battery_removal_threshold = 5;
	} else
		battery_removal_threshold = 60;

	/* retain SOC only for previous 100% if threshold is smaller than 15 */
	if (extern_data.soc == 100)
		battery_removal_threshold = 15;

	/* if it's software reboot, just read previous SOC and use it */
	if (!sys_is_reboot(info) && !battery_changed) {
		dev_info(info->dev, "SW reset case, use stored!\n");
		ccnt_data.soc = extern_data.soc;
		ccnt_data.last_cc = (ccnt_data.max_cc / 1000)
			* (ccnt_data.soc * 10 + 5);
		info->bat_params.temp = extern_data.temp;
		info->use_ocv = extern_data.use_ocv;
	} else if (battery_changed ||
	    (extern_data.soc > ccnt_data.soc + battery_removal_threshold) ||
	    (ccnt_data.soc > extern_data.soc + battery_removal_threshold)) {
		if (battery_changed) /* stored values are invalid */
			dev_info(info->dev, "battery may be changed!\n");
		else /* battery is changed */
			dev_info(info->dev, "battery is changed!\n");
		info->use_ocv = 0;
	} else {
		/* same battery */
		dev_info(info->dev, "%s: same battery\n", __func__);
		if (ccnt_data.soc >= extern_data.soc) {
			/* mainly for the relaxed + good range case */
			dev_dbg(info->dev, "clamp soc for same battery!\n");
			ccnt_data.soc = extern_data.soc;
		}
		if (slp_cnt >= info->slp_con) {
			/* battery was relaxed */
			dev_info(info->dev, "battery relxed for %ds",
				 info->slp_con);
			if ((ccnt_data.soc < info->range_low_th)
			    || (ccnt_data.soc > info->range_high_th)) {
				dev_info(info->dev, "in good range, caculate...\n");
				info->use_ocv = 1;
			} else {/* in the flaten range */
				dev_info(info->dev,
					 "NOT in good range, use stored value...\n");
				ccnt_data.soc = extern_data.soc;
				ccnt_data.last_cc = (ccnt_data.max_cc / 1000)
					* (ccnt_data.soc * 10 + 5);
				info->use_ocv = extern_data.use_ocv;
			}
		} else {
			/*
			 * for battery is not relaxed,
			 * new measured value is not reliable,
			 * use the stored value instead
			 */
			dev_info(info->dev, "NOT relaxed enough, use stored SOC\n");
			ccnt_data.soc = extern_data.soc;
			ccnt_data.last_cc = (ccnt_data.max_cc / 1000)
				* (ccnt_data.soc * 10 + 5);
			info->use_ocv = extern_data.use_ocv;
		}
	}

	cap = ccnt_data.soc;

	dev_info(info->dev, "%s: 88pm830 battery present: %d soc: %d%%\n",
		 __func__, info->bat_params.present, ccnt_data.soc);
}

static void pm830_external_power_changed(struct power_supply *psy)
{
	struct pm830_battery_info *info;

	info = container_of(psy, struct pm830_battery_info, battery);
	queue_delayed_work(info->bat_wqueue,
			   &info->charged_work, 0);
	return;
}

static enum power_supply_property pm830_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
};

static int pm830_batt_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct pm830_battery_info *info = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		/* block charging until cc is ready */
		if (!atomic_read(&info->cc_done))
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		else
			val->intval = info->bat_params.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->bat_params.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* report fake capacity to android layer */
		if (!info->bat_params.present)
			info->bat_params.cap = 80;
		val->intval = info->bat_params.cap;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = info->bat_params.tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->bat_params.volt;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		info->bat_params.ibat = pm830_get_ibat_cc(info);
		info->bat_params.ibat /= 1000;
		val->intval = info->bat_params.ibat;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!info->bat_params.present)
			info->bat_params.temp = 240;
		val->intval = info->bat_params.temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->bat_params.health;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int s = 0;
	s += sprintf(buf, "debug  %d\n", ccnt_data.debug);
	return s;
}

static ssize_t debug_store(
			   struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret;
	ret = sscanf(buf, "%d", &ccnt_data.debug);
	if (ret != 1) {
		dev_err(dev, "%s: sscanf failed!\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s: debug: %d\n", __func__, ccnt_data.debug);
	return size;
}
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, debug_show, debug_store);

static irqreturn_t pm830_bat_handler(int irq, void *data)
{
	struct pm830_battery_info *info = data;
	int value;

	dev_info(info->dev, "battery in/out interrupt is served\n");

	regmap_read(info->chip->regmap, PM830_STATUS, &value);
	if (value & PM830_BAT_DET) {
		/* battery is plug in */
		dev_dbg(info->dev, "battery plug in.\n");
		info->bat_params.present = 1;
	} else {
		/* battery is plug out */
		dev_dbg(info->dev, "battery plug out.\n");
		info->bat_params.present = 0;
	}

	/* notify CHG that battery was inserted or removed */
	power_supply_changed(&info->battery);

	return IRQ_HANDLED;
}

static struct pm830_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm830_irq_descs[] = {
	{"battery detect", pm830_bat_handler},
	{"gpadc0-tbat", pm830_tbat_handler},
	{"columb counter", NULL},
	{"charge sleep", NULL},
};

static int pm830_fg_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm830_bat_pdata *pdata)
{
	int ret;
	struct device_node *pmic_node;

	ret = of_property_read_bool(np, "marvell,fg-has-external-storage");
	if (!ret) {
		pr_err("88pm830 has no external storage.\n");
		return -EINVAL;
	}
	/* handle external storage */
	ret = of_property_read_u32(np, "external-storage", &pdata->ext_storage);
	if (ret) {
		pr_err("cannot get external storage property.\n");
		return -EINVAL;
	}

	pmic_node = of_find_node_by_phandle(pdata->ext_storage);
	if (!pmic_node) {
		pr_err("cannot find external storage node.\n");
		return -EINVAL;
	}
	pdata->client = of_find_i2c_device_by_node(pmic_node);
	if (!pdata->client) {
		pr_err("cannot find external storage i2c client.\n");
		return -EINVAL;
	}

	of_node_put(pmic_node);
	pmic_node = NULL;
	i2c_use_client(pdata->client);

	ret = of_property_read_u32(np, "bat-ntc-support", &pdata->bat_ntc);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "bat-capacity", &pdata->capacity);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "external-resistor", &pdata->r_int);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "sleep-period", &pdata->slp_con);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "low-threshold", &pdata->range_low_th);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "high-threshold", &pdata->range_high_th);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "power-off-threshold", &pdata->power_off_threshold);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "safe-power-off-threshold",
			&pdata->safe_power_off_threshold);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(np, "ocv-table", pdata->ocv_table, 100);
	if (ret)
		return ret;

	/* battery temperature feature parameters init */
	ret = of_property_read_u32(np, "temp-range-num", &pdata->temp_range_num);
	if (ret)
		pdata->temp_range_num = 0;
	if (pdata->temp_range_num) {
		ret = of_property_read_u32_array(np, "switch-thr", pdata->switch_thr, 2);
		if (ret)
			return ret;
		ret = of_property_read_u32_array(np, "r-tbat-thr", pdata->r_tbat_thr, 4);
		if (ret)
			return ret;
		ret = of_property_read_u32_array(np, "gp0-bias-curr", pdata->gp0_bias_curr, 3);
		if (ret)
			return ret;
		ret = of_property_read_u32(np, "ntc-table-size", &pdata->ntc_table_size);
		if (ret)
			return ret;
		pdata->ntc_table = kzalloc(sizeof(int) * pdata->ntc_table_size, GFP_KERNEL);
		if (!pdata->ntc_table)
			return -ENOMEM;
		ret = of_property_read_u32_array(np, "ntc-table", pdata->ntc_table,
				pdata->ntc_table_size);
		if (ret)
			return ret;
	}

	return 0;
}

static int pm830_battery_probe(struct platform_device *pdev)
{
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm830_battery_info *info;
	struct pm830_bat_pdata *pdata;
	struct device_node *node = pdev->dev.of_node;
	int ret;
	int i, j;

	info = devm_kzalloc(&pdev->dev,
			    sizeof(struct pm830_battery_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pdata = pdev->dev.platform_data;
	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm830_fg_dt_init(node, &pdev->dev, pdata);
		if (ret)
			return -EINVAL;
	} else if (!pdata) {
		return -EINVAL;
	}

	chip->get_fg_internal_soc = pm830_get_fg_internal_soc;
	info->extern_chip = i2c_get_clientdata(pdata->client);

	info->bat_ntc = pdata->bat_ntc;
	if (pdata->capacity)
		ccnt_data.max_cc = pdata->capacity * 3600;
	else
		ccnt_data.max_cc = 1500 * 3600;
	info->r_int = pdata->r_int;
	info->slp_con = pdata->slp_con;
	info->range_low_th = pdata->range_low_th;
	info->range_high_th = pdata->range_high_th;

	info->temp_range_num = pdata->temp_range_num;
	if (info->temp_range_num) {
		info->switch_thr = pdata->switch_thr;
		info->gp0_bias_curr = pdata->gp0_bias_curr;
		info->bias = NTC_BIAS_LOW;
		info->ntc_table_size = pdata->ntc_table_size;
		for (i = 0; i < info->temp_range_num; i++) {
			info->ntc_table[i] = kzalloc(sizeof(struct ntc_convert) *
					info->ntc_table_size, GFP_KERNEL);
			if (!info->ntc_table[i])
				goto out_ntc;

			for (j = 0; j < info->ntc_table_size; j++) {
				info->ntc_table[i][j].temp = j - 24;
				info->ntc_table[i][j].voltage =
					info->gp0_bias_curr[i] * pdata->ntc_table[j] / 1000;
			}
		}
		/*
		 * get battery temp threshold and these threshold can be used to detemine battery
		 * health status
		 */
		info->tbat_thr = pdata->r_tbat_thr;
		for (i = 0, j = 0; i < info->ntc_table_size; i++) {
			if (pdata->ntc_table[i] <= pdata->r_tbat_thr[j]) {
				info->tbat_thr[j++] = info->ntc_table[0][i].temp;
				if (j >= ARRAY_SIZE(pdata->r_tbat_thr))
					break;
			}
		}
		kfree(pdata->ntc_table);
		pdata->ntc_table = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->ocv_table); i++)
		ocv_table[i] = pdata->ocv_table[i];
	info->power_off_threshold = pdata->power_off_threshold;
	info->safe_power_off_threshold = pdata->safe_power_off_threshold;

	atomic_set(&info->cc_done, 0);

	info->chip = chip;
	info->dev = &pdev->dev;
	info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
	/* default type [Lion] */
	info->bat_params.tech = POWER_SUPPLY_TECHNOLOGY_LION;

	platform_set_drvdata(pdev, info);

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irq[j] = platform_get_irq(pdev, i);
		if (info->irq[j] < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	pm830_init_fg(info);
	pm830_bat_update_status(info);

	info->battery.name = "battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.properties = pm830_batt_props;
	info->battery.num_properties = ARRAY_SIZE(pm830_batt_props);
	info->battery.get_property = pm830_batt_get_prop;
	info->battery.external_power_changed = pm830_external_power_changed;
	info->battery.supplied_to = pm830_supply_to;
	info->battery.num_supplicants = ARRAY_SIZE(pm830_supply_to);

	power_supply_register(&pdev->dev, &info->battery);

	info->battery.dev->parent = &pdev->dev;
	info->bat_wqueue = create_singlethread_workqueue("bat-88pm830");
	if (!info->bat_wqueue) {
		dev_info(chip->dev,
			 "[%s]Failed to create bat_wqueue\n", __func__);
		ret = -ESRCH;
		goto out;
	}

	pm830_handle_tbat(info);
	/* interrupt should be request in the last stage */
	for (i = 0; i < info->irq_nums; i++) {
		if (pm830_irq_descs[i].handler != NULL) {
			ret = devm_request_threaded_irq(info->dev,
				info->irq[i], NULL,
				pm830_irq_descs[i].handler,
				IRQF_ONESHOT | IRQF_NO_SUSPEND,
				pm830_irq_descs[i].name, info);
			if (ret < 0) {
				dev_err(info->dev, "failed to request IRQ: #%d: %d\n",
					info->irq[i], ret);
				goto out_irq;
			}
		}
	}

	INIT_DEFERRABLE_WORK(&info->monitor_work, pm830_battery_work);
	INIT_DELAYED_WORK(&info->charged_work, pm830_charged_work);
	INIT_DELAYED_WORK(&info->cc_work, pm830_cc_work);
	INIT_WORK(&info->tbat_work, pm830_tbat_work);
	queue_delayed_work(info->bat_wqueue, &info->cc_work, 5 * HZ);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work, HZ);

	ret = device_create_file(&pdev->dev, &dev_attr_debug);
	if (ret < 0) {
		dev_err(&pdev->dev, "device attr create fail: %d\n", ret);
		goto out;
	}

	device_init_wakeup(&pdev->dev, 1);
	return 0;

out_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irq[i], info);

out_ntc:
	if (info->temp_range_num) {
		for (i = 0; i < info->temp_range_num; i++)
			kfree(info->ntc_table[i]);
		kfree(pdata->ntc_table);
	}

out:
	power_supply_unregister(&info->battery);

	return ret;
}

static int pm830_battery_remove(struct platform_device *pdev)
{
	int i;
	struct pm830_battery_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm830_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irq[i], info);
	}

	for (i = 0; i < info->temp_range_num; i++) {
		kfree(info->ntc_table[i]);
		info->ntc_table[i] = NULL;
	}

	cancel_delayed_work_sync(&info->monitor_work);
	cancel_delayed_work_sync(&info->charged_work);
	flush_workqueue(info->bat_wqueue);
	power_supply_unregister(&info->battery);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm830_battery_shutdown(struct platform_device *pdev)
{
	pm830_battery_remove(pdev);
	return;
}

#ifdef CONFIG_PM
static int pm830_battery_suspend(struct device *dev)
{
	struct pm830_battery_info *info = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&info->monitor_work);
	return pm830_dev_suspend(dev);
}

static int pm830_battery_resume(struct device *dev)
{
	struct pm830_battery_info *info = dev_get_drvdata(dev);

	/*
	 * When exiting sleep, we might want to have new OCV.
	 * Since reading the sleep counter resets it,
	 * We would like to avoid reading it on short wakeups.
	 * Therefore delay this work with 300 msec,
	 * so in case of short wake up -
	 * it will be canceled before entering sleep again.
	 */
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   300 * HZ / 1000);
	return pm830_dev_resume(dev);
}

static const struct dev_pm_ops pm830_battery_pm_ops = {
	.suspend	= pm830_battery_suspend,
	.resume		= pm830_battery_resume,
};
#endif

static const struct of_device_id pm830_fg_dt_match[] = {
	{ .compatible = "marvell,88pm830-bat", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm830_fg_dt_match);

static struct platform_driver pm830_battery_driver = {
	.driver		= {
		.name	= "88pm830-bat",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm830_fg_dt_match),
#ifdef CONFIG_PM
		.pm	= &pm830_battery_pm_ops,
#endif
	},
	.probe		= pm830_battery_probe,
	.remove		= pm830_battery_remove,
	.shutdown	= pm830_battery_shutdown,
};

static int __init pm830_battery_init(void)
{
	return platform_driver_register(&pm830_battery_driver);
}
module_init(pm830_battery_init);

static void __exit pm830_battery_exit(void)
{
	platform_driver_unregister(&pm830_battery_driver);
}
module_exit(pm830_battery_exit);

MODULE_DESCRIPTION("Marvell 88PM830 battery driver");
MODULE_LICENSE("GPL");
