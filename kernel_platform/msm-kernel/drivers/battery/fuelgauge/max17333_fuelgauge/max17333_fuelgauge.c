/*
 * Copyright (c) 2021 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mfd/max17333.h>
#include <linux/of_gpio.h>
#include "max17333_fuelgauge.h"

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

static bool is_probe_done;
static bool low_temp_wa;
static char *batt_supplied_to[] = {
	"max17333-battery",
};

static char *batt_sub_supplied_to[] = {
	"max17333-battery-sub",
};

static char *max17333_battery_type[] = {
	"SUB",
	"MAIN",
};

static enum power_supply_property max17333_fg_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN,
	POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG
};

static struct device_attribute max17333_fg_attrs[] = {
	MAX17333_FG_ATTR(max17333_fg_vcell),
	MAX17333_FG_ATTR(max17333_fg_avgvcell),
	MAX17333_FG_ATTR(max17333_fg_current),
	MAX17333_FG_ATTR(max17333_fg_avgcurrent),
	MAX17333_FG_ATTR(max17333_fg_avgcell),
	MAX17333_FG_ATTR(max17333_fg_qh),
	MAX17333_FG_ATTR(max17333_fg_designcap),
	MAX17333_FG_ATTR(max17333_fg_fullcaprep),
	MAX17333_FG_ATTR(max17333_fg_fullcapnom),
	MAX17333_FG_ATTR(max17333_fg_avcap),
	MAX17333_FG_ATTR(max17333_fg_repcap),
	MAX17333_FG_ATTR(max17333_fg_mixcap),
	MAX17333_FG_ATTR(max17333_fg_avsoc),
	MAX17333_FG_ATTR(max17333_fg_repsoc),
	MAX17333_FG_ATTR(max17333_fg_mixsoc),
	MAX17333_FG_ATTR(max17333_fg_age),
	MAX17333_FG_ATTR(max17333_fg_ageforecast),
	MAX17333_FG_ATTR(max17333_fg_cycles),
	MAX17333_FG_ATTR(max17333_fg_temp),
	MAX17333_FG_ATTR(max17333_fg_porcmd)
};

static void max17333_reset_bat_id(struct max17333_fg_data *fg);

static inline int max17333_raw_voltage_to_uvolts(u16 lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}

static inline int max17333_raw_current_to_uamps(struct max17333_fg_data *fg,
												int curr)
{
	return curr * 15625 / ((int)fg->rsense * 10);
}

static inline int max17333_raw_capacity_to_uamph(struct max17333_fg_data *fg,
						 int cap)
{
	return cap * 5000 / (int)fg->rsense;
}

static int max17333_read_capacity(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret, cnvt_val;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	cnvt_val = max17333_raw_capacity_to_uamph(fg, sign_extend32(rval, 15));

	/* Convert to milliamphs */
	*val = cnvt_val / 1000;

	return ret;
}

static int max17333_read_voltage(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret, cnvt_val;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	cnvt_val = max17333_raw_voltage_to_uvolts(rval);

	/* Convert to millivolts */
	*val = cnvt_val / 1000;

	return ret;
}

static int max17333_read_current(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret, cnvt_val;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	cnvt_val = max17333_raw_current_to_uamps(fg, sign_extend32(rval, 15));

	if (*val == SEC_BATTERY_CURRENT_UA)
		*val = cnvt_val; /* microamps */
	else
		*val = cnvt_val / 1000; /* milliamps */

	return ret;
}

static int max17333_read_percentage(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	*val = (rval >> 8) * 10; /* Percentage LSB: 1/256 % */

	return ret;
}

static int max17333_read_percentage_repsoc(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret;
	u16 rval;
	u8 data0, data1;
	int soc;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	data0 = rval & 0x00FF;
	data1 = rval >> 8; /* Percentage LSB: 1/256 % */

	soc = ((data1 * 100) + (data0 * 100 / 256)) / 10;

	*val = min(soc, 1000);

	return ret;
}

static int max17333_read_temperature(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret, cnvt_val;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	cnvt_val = sign_extend32(rval, 15);
	/* The value is converted into centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */
	*val = cnvt_val >> 8;

	return ret;
}

static int max17333_read_timeto(struct max17333_fg_data *fg, u8 addr, int *val)
{
	int ret;
	u16 rval;

	ret = max17333_read(fg->regmap, addr, &rval);
	if (ret < 0)
		return ret;

	*val = (rval * 45) >> 3; /* TTF LSB: 5.625 sec */

	return ret;
}

static int max17333_get_tte(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_timeto(fg, REG_TTE, val);
}

static int max17333_get_ttf(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_timeto(fg, REG_TTF, val);
}

static int max17333_get_vcell(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_voltage(fg, REG_VCELL, val);
}

static int max17333_get_avgvcell(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_voltage(fg, REG_AVGVCELL, val);
}

static int max17333_get_avgcell(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_voltage(fg, REG_AVGCELL1, val);
}

static int max17333_get_vfocv(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_voltage(fg, REG_VFOCV, val);
}

static int max17333_get_current(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_current(fg, REG_CURRENT, val);
}

static int max17333_get_avgcurrent(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_current(fg, REG_AVGCURRENT, val);
}

static int max17333_get_qh(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_QH, val);
}

static int max17333_get_designcap(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_DESIGNCAP, val);
}

static int max17333_get_fullcaprep(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_FULLCAPREP, val);
}

static int max17333_get_fullcapnom(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_FULLCAPNOM, val);
}

static int max17333_get_vfsoc(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_percentage_repsoc(fg, REG_VFSOC, val);
}

static int max17333_get_vfremcap(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_VFREMCAP, val);
}

static int max17333_get_avcap(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_AVCAP, val);
}

static int max17333_get_repcap(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_REPCAP, val);
}

static int max17333_get_mixcap(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_capacity(fg, REG_MIXCAP, val);
}

static int max17333_get_avsoc(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_percentage(fg, REG_AVSOC, val);
}

static int max17333_get_repsoc(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_percentage_repsoc(fg, REG_REPSOC, val);
}

static int max17333_get_mixsoc(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_percentage(fg, REG_MIXSOC, val);
}

static int max17333_get_age(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_percentage(fg, REG_AGE, val);
}

static int max17333_get_temperature(struct max17333_fg_data *fg, int *val)
{
	return max17333_read_temperature(fg, REG_TEMP, val);
}

static int max17333_get_cycles(struct max17333_fg_data *fg, int *val)
{
	int ret;
	u16 rval;

	ret = max17333_read(fg->regmap, REG_CYCLES, &rval);
	if (ret < 0)
		return ret;

	*val = rval * 25 / 100;

	return ret;
}

static int max17333_get_ageforecast(struct max17333_fg_data *fg, int *val)
{
	int ret;
	u16 rval;

	ret = max17333_read(fg->regmap, REG_AGEFORECAST, &rval);
	if (ret < 0)
		return ret;

	*val = rval * 16 / 100;

	return ret;
}

static int max17333_get_temperature_alert_min(struct max17333_fg_data *fg,
											  int *temp)
{
	int ret;
	u16 val;

	ret = max17333_read(fg->regmap, REG_TALRTTH, &val);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	/* Convert 1DegreeC LSB to 0.1DegreeC LSB */
	*temp = sign_extend32(val & 0xff, 7) * 10;

	return 0;
}

static int max17333_get_temperature_alert_max(struct max17333_fg_data *fg,
											  int *temp)
{
	int ret;
	u16 val;

	ret = max17333_read(fg->regmap, REG_TALRTTH, &val);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	/* Convert 1DegreeC LSB to 0.1DegreeC LSB */
	*temp = sign_extend32(val >> 8, 7) * 10;

	return 0;
}

static int max17333_get_battery_health(struct max17333_fg_data *fg, int *health)
{
	int ret;
	u16 val;

	ret = max17333_read(fg->regmap, REG_PROTSTATUS, &val);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_PROTSTATUS\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	if (val & BIT_PREQF_INT) {
		*health = POWER_SUPPLY_HEALTH_UNKNOWN;
	} else if ((val & BIT_TOOHOTC_INT) ||
			 (val & BIT_TOOHOTD_INT) ||
			 (val & BIT_DIEHOT_INT)) {
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if ((val & BIT_UVP_INT) ||
			 (val & BIT_PERMFAIL_INT) ||
			 (val & BIT_SHDN_INT)) {
		*health = POWER_SUPPLY_HEALTH_DEAD;
	} else if ((val & BIT_TOOCOLDC_INT) ||
			 (val & BIT_TOOCOLDD_INT)) {
		*health = POWER_SUPPLY_HEALTH_COLD;
	} else if (val & BIT_OVP_INT) {
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if ((val & BIT_QOVFLW_INT) ||
			 (val & BIT_OCCP_INT) ||
			 (val & BIT_ODCP_INT)) {
		*health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	} else if (val & BIT_CHGWDT_INT) {
		*health = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
	} else {
		*health = POWER_SUPPLY_HEALTH_GOOD;
	}

	return 0;
}

static int max17333_set_temp_lower_limit(struct max17333_fg_data *fg, int temp)
{
	int ret;
	u16 data;

	ret = max17333_read(fg->regmap, REG_TALRTTH, &data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	/* Input in deci-centigrade, convert to centigrade */
	temp /= 10;

	data &= 0xFF00;
	data |= (temp & 0xFF);

	ret = max17333_write(fg->regmap, REG_TALRTTH, data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to write REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	return 0;
}

static int max17333_set_temp_upper_limit(struct max17333_fg_data *fg, int temp)
{
	int ret;
	u16 data;

	ret = max17333_read(fg->regmap, REG_TALRTTH, &data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	/* Input in deci-centigrade, convert to centigrade */
	temp /= 10;

	data &= 0xFF;
	data |= ((temp << 8) & 0xFF00);

	ret = max17333_write(fg->regmap, REG_TALRTTH, data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to write REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	return 0;
}

static int max17333_set_min_capacity_alert_th(struct max17333_fg_data *fg, unsigned int th)
{
	int ret;
	u16 data;

	ret = max17333_read(fg->regmap, REG_SALRTTH, &data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_SALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	data &= 0xFF00;
	data |= (th & 0xFF);

	ret = max17333_write(fg->regmap, REG_SALRTTH, data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to write REG_SALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	return 0;
}

static int max17333_set_max_capacity_alert_th(struct max17333_fg_data *fg, unsigned int th)
{
	int ret;
	u16 data;

	ret = max17333_read(fg->regmap, REG_SALRTTH, &data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to read REG_SALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	data &= 0xFF;
	data |= ((th & 0xFF) << 8);

	ret = max17333_write(fg->regmap, REG_SALRTTH, data);
	if (ret < 0) {
		pr_err("%s[%s] : fail to write REG_SALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return ret;
	}

	return 0;
}

static void max17333_set_alert_thresholds(struct max17333_fg_data *fg)
{
	struct max17333_fg_platform_data *pdata = fg->pdata;
	u16 val;
	int ret;

	/* Set VAlrtTh */
	val = (pdata->volt_min / 20);
	val |= ((pdata->volt_max / 20) << 8);
	ret = max17333_write(fg->regmap, REG_VALRTTH, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_VALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return;
	}

	/* Set TAlrtTh */
	val = pdata->temp_min & 0xFF;
	val |= ((pdata->temp_max & 0xFF) << 8);
	ret = max17333_write(fg->regmap, REG_TALRTTH, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_TALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return;
	}

	/* Set SAlrtTh */
	val = pdata->soc_min;
	val |= (pdata->soc_max << 8);
	ret = max17333_write(fg->regmap, REG_SALRTTH, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_SALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return;
	}

	/* Set IAlrtTh */
	val = (pdata->curr_min * fg->rsense / 400) & 0xFF;
	val |= (((pdata->curr_max * fg->rsense / 400) & 0xFF) << 8);
	ret = max17333_write(fg->regmap, REG_IALRTTH, val);
	if (ret < 0) {
		pr_err("%s[%s]: fail to write REG_IALRTTH\n", __func__, max17333_battery_type[fg->battery_type]);
		return;
	}
}

void max17333_fg_read_soc_data(struct max17333_fg_data *fg)
{
	u16 read_vfsoc, read_vfocv, read_hconfig, read_vcell, read_fullcaprep;
	u16 read_repcap, read_repsoc, read_vfremcap, read_fullcapnom;

	max17333_read(fg->regmap, REG_VFSOC, &read_vfsoc);
	max17333_read(fg->regmap, REG_VFOCV, &read_vfocv);
	max17333_read(fg->regmap, REG_HCONFIG, &read_hconfig);
	max17333_read(fg->regmap, REG_VCELL, &read_vcell);
	max17333_read(fg->regmap, REG_FULLCAPREP, &read_fullcaprep);
	max17333_read(fg->regmap, REG_REPCAP, &read_repcap);
	max17333_read(fg->regmap, REG_REPSOC, &read_repsoc);
	max17333_read(fg->regmap, REG_VFREMCAP, &read_vfremcap);
	max17333_read(fg->regmap, REG_FULLCAPNOM, &read_fullcapnom);

	pr_info("%s:  VFSOC(0x%x), VFOCV(0x%x), HCONFIG(0x%x), VCELL(0x%x), FULLCAPREP(0x%x)\n",
		__func__, read_vfsoc, read_vfocv, read_hconfig, read_vcell, read_fullcaprep);
	pr_info("%s:  REPCAP(0x%x), REPSOC(0x%x), VFREMCAP(0x%x), FULLCAPNOM(0x%x)\n",
		__func__, read_repcap, read_repsoc, read_vfremcap, read_fullcapnom);
}

static void max17333_fg_patchallow_check(struct max17333_fg_data *fg)
{
	int ret;
	u16 patchallow;

	ret = max17333_read(fg->regmap_shdw, REG_PATCH_ALLOW, &patchallow);
	if (ret < 0) {
		dev_err(fg->dev, "[%s]: fail to read REG_PATCH_ALLOW\n", max17333_battery_type[fg->battery_type]);
		return;
	}

	pr_info("%s [%s] : 0x%x\n", __func__, max17333_battery_type[fg->battery_type], patchallow);

	patchallow = (patchallow & 0x000C);

	if (patchallow == 0x000C) {
		msleep(375);
	}
}

int max17333_fg_reset_soc(struct max17333_fg_data *fg, int mode)
{
	int ret;
	u16 commstat, nchgctrl_pckp, i2ccmd_pckp, hconfig;

	pr_info("%s: Before Reset SOC\n", __func__);
	max17333_fg_read_soc_data(fg);

	pr_info("%s: Start Reset SOC\n", __func__);
	/* Unlock */
	ret = max17333_read(fg->regmap, REG_COMMSTAT, &commstat);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_COMMSTAT, commstat & 0xFF06);
	ret |= max17333_write(fg->regmap, REG_COMMSTAT, commstat & 0xFF06);
	if (ret < 0)
		return -1;

	pr_info("%s: mode=%d\n", __func__, mode);
	/* Set MG_PCKP */
	ret = max17333_read(fg->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl_pckp);
	if (ret < 0)
		return -1;

	if (mode == MAX17333_VCELL_EQUAL_PKCP)
		nchgctrl_pckp |= MAX17333_NCHGCTRL2_MG_PCKP;
	else
		nchgctrl_pckp &= ~MAX17333_NCHGCTRL2_MG_PCKP;
	pr_info("%s: nchgctrl_pckp=%d\n", __func__, nchgctrl_pckp);

	ret = max17333_write(fg->regmap_shdw, REG_NCHGCTRL2_SHDW, nchgctrl_pckp);
	if (ret < 0)
		return -1;

	ret = max17333_read(fg->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_pckp);
	if (ret < 0)
		return -1;

	if (mode == MAX17333_VCELL_EQUAL_PKCP)
		i2ccmd_pckp |= MAX17333_I2CCMD_MG_PCKP;
	else
		i2ccmd_pckp &= ~MAX17333_I2CCMD_MG_PCKP;
	pr_info("%s: i2ccmd_pckp=%d\n", __func__, i2ccmd_pckp);

	ret = max17333_write(fg->regmap_shdw, REG_I2CCMD_SHDW, i2ccmd_pckp);
	if (ret < 0)
		return -1;

	/* Delay time reflecting the effect : 761 ms */
	mdelay(761);
	pr_info("%s: MG_PCKP Enabled\n", __func__);
	max17333_fg_read_soc_data(fg);

	/* Enter Test Mode */
	ret = max17333_write(fg->regmap, REG_TESTLOCK1, 0x8700);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_TESTLOCK2, 0xD200);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_TESTLOCK3, 0xC27E);
	if (ret < 0)
		return -1;

	/* Check Patch Allow */
	max17333_fg_patchallow_check(fg);

	/* POK = 0 & Halt Firmware */
	ret = max17333_write(fg->regmap, REG_TESTCTRL, 0x0400);
	if (ret < 0)
		return -1;

	/* HConfig */
	ret = max17333_read(fg->regmap, REG_HCONFIG, &hconfig);
	if (ret < 0)
		return -1;

	hconfig |= 0x8000;

	ret = max17333_write(fg->regmap, REG_HCONFIG, hconfig);
	if (ret < 0)
		return -1;

	/* Clear Sleep */
	ret = max17333_write(fg->regmap,  REG_SLEEP, 0x0000);
	if (ret < 0)
		return -1;

	/* Exit Test Mode */
	ret = max17333_write(fg->regmap, REG_TESTLOCK3, 0x0000);
	if (ret < 0)
		return -1;

	/* Delay */
	msleep(3000);

	pr_info("%s: End Reset SOC\n", __func__);

	pr_info("%s: After Reset SOC\n", __func__);
	max17333_fg_read_soc_data(fg);

	return 0;
}

#if IS_ENABLED(CONFIG_DUAL_FUELGAUGE)
int max17333_reset_fuelgauge_wo_delay(struct max17333_fg_data *fg, int mode)
{
	int ret;
	u16 commstat, nchgctrl_pckp, i2ccmd_pckp, hconfig;

	pr_info("%s: Before Reset SOC\n", __func__);
	max17333_fg_read_soc_data(fg);

	pr_info("%s: Start Reset SOC\n", __func__);
	/* Unlock */
	ret = max17333_read(fg->regmap, REG_COMMSTAT, &commstat);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_COMMSTAT, commstat & 0xFF06);
	ret |= max17333_write(fg->regmap, REG_COMMSTAT, commstat & 0xFF06);
	if (ret < 0)
		return -1;

	pr_info("%s: mode=%d\n", __func__, mode);
	/* Set MG_PCKP */
	ret = max17333_read(fg->regmap_shdw, REG_NCHGCTRL2_SHDW, &nchgctrl_pckp);
	if (ret < 0)
		return -1;

	if (mode == MAX17333_VCELL_EQUAL_PKCP)
		nchgctrl_pckp |= MAX17333_NCHGCTRL2_MG_PCKP;
	else
		nchgctrl_pckp &= ~MAX17333_NCHGCTRL2_MG_PCKP;
	pr_info("%s: nchgctrl_pckp=%d\n", __func__, nchgctrl_pckp);

	ret = max17333_write(fg->regmap_shdw, REG_NCHGCTRL2_SHDW, nchgctrl_pckp);
	if (ret < 0)
		return -1;

	ret = max17333_read(fg->regmap_shdw, REG_I2CCMD_SHDW, &i2ccmd_pckp);
	if (ret < 0)
		return -1;

	if (mode == MAX17333_VCELL_EQUAL_PKCP)
		i2ccmd_pckp |= MAX17333_I2CCMD_MG_PCKP;
	else
		i2ccmd_pckp &= ~MAX17333_I2CCMD_MG_PCKP;
	pr_info("%s: i2ccmd_pckp=%d\n", __func__, i2ccmd_pckp);

	ret = max17333_write(fg->regmap_shdw, REG_I2CCMD_SHDW, i2ccmd_pckp);
	if (ret < 0)
		return -1;

	/* Delay time reflecting the effect : 761 ms */
	mdelay(761);
	pr_info("%s: MG_PCKP Enabled\n", __func__);
	max17333_fg_read_soc_data(fg);

	/* Enter Test Mode */
	ret = max17333_write(fg->regmap, REG_TESTLOCK1, 0x8700);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_TESTLOCK2, 0xD200);
	if (ret < 0)
		return -1;

	ret = max17333_write(fg->regmap, REG_TESTLOCK3, 0xC27E);
	if (ret < 0)
		return -1;

	/* Check Patch Allow */
	max17333_fg_patchallow_check(fg);

	/* POK = 0 & Halt Firmware */
	ret = max17333_write(fg->regmap, REG_TESTCTRL, 0x0400);
	if (ret < 0)
		return -1;

	/* HConfig */
	ret = max17333_read(fg->regmap, REG_HCONFIG, &hconfig);
	if (ret < 0)
		return -1;

	hconfig |= 0x8000;

	ret = max17333_write(fg->regmap, REG_HCONFIG, hconfig);
	if (ret < 0)
		return -1;

	/* Exit Test Mode */
	ret = max17333_write(fg->regmap, REG_TESTLOCK3, 0x0000);
	if (ret < 0)
		return -1;

	pr_info("%s: End Reset SOC\n", __func__);

	return 0;
}
#endif

static void max17333_write_temp(struct max17333_fg_data *fg, int temp)
{
	int diff = 0;
	int read_temp = 0;
	int ret = 0;
	int temp_celcius = 0;
	u16 value;

	ret = max17333_get_temperature(fg, &read_temp);
	if (ret < 0)
		return;
	temp_celcius = temp/10;
	diff = abs(temp_celcius - read_temp);
	pr_info("%s : temp(%d), read_temp(%d), temp_celcius(%d)\n", __func__, temp, read_temp, temp_celcius);
	if (diff >= 2) {
		value = (temp_celcius << 8);
		max17333_write(fg->regmap, REG_TEMP, value);
		pr_info("%s : written temp_celcius(%d)\n", __func__, temp_celcius);
	}
}

bool max17333_fg_reset(struct max17333_fg_data *fg, int mode)
{
	if (!max17333_fg_reset_soc(fg, mode))
		return true;
	else
		return false;
}

bool max17333_set_por_cmd(struct max17333_fg_data *fg)
{
	u16 read_val, data, cnt = 0;

	pr_info("%s: Start Set POR Cmd\n", __func__);

	data = MAX17333_CONFIG2_POR_CMD;

	max17333_read(fg->regmap, REG_CONFIG2, &read_val);

	max17333_write(fg->regmap, REG_CONFIG2, (read_val | data));

	for (cnt = 0; cnt < 6; cnt++) {
		/* The POR_CMD bit is set to 0 at power-up and automatically clears itself after firmware restart */
		mdelay(500);
		read_val = 0;
		max17333_read(fg->regmap, REG_CONFIG2, &read_val);
		if ((read_val & MAX17333_CONFIG2_POR_CMD) == 0) {
			pr_info("%s: Success Set POR Cmd\n", __func__);
			return true;
		}
	}

	pr_info("%s: Fail Set POR Cmd\n", __func__);

	return false;
}

static int max17333_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max17333_fg_data *fg = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	u16 pval;
	int ret;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = max17333_get_vcell(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = max17333_get_avgvcell(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = max17333_read(fg->regmap, REG_MAXMINVOLT, &pval);
		if (ret < 0)
			return ret;
		val->intval = pval >> 8;
		val->intval *= 20; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = max17333_read(fg->regmap, REG_VEMPTY, &pval);
		if (ret < 0)
			return ret;
		val->intval = pval >> 7;
		val->intval *= 10; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = max17333_get_vfocv(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = max17333_get_repsoc(fg, &val->intval);
		if (ret < 0)
			return ret;
		val->intval /= 10; /* UI soc */
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
		ret = max17333_read(fg->regmap, REG_SALRTTH, &pval);
		if (ret < 0)
			return ret;
		val->intval = pval & 0xFF;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
		ret = max17333_read(fg->regmap, REG_SALRTTH, &pval);
		if (ret < 0)
			return ret;
		val->intval = (pval >> 8) & 0xFF;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = max17333_get_battery_health(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = max17333_get_temperature(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max17333_get_temperature_alert_min(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max17333_get_temperature_alert_max(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = max17333_get_current(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = max17333_get_avgcurrent(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = max17333_get_tte(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		ret = max17333_get_ttf(fg, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_REPSOC:
			ret = max17333_get_repsoc(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_REPCAP:
			ret = max17333_get_repcap(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_FULLCAPREP:
			ret = max17333_get_fullcaprep(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_FULLCAPNOM:
			ret = max17333_get_fullcapnom(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_CYCLES:
			ret = max17333_get_cycles(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_DESIGNCAP:
			ret = max17333_get_designcap(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			if (!val->intval) {
				if (fg->pdata->bat_gpio_cnt > 0)
					max17333_reset_bat_id(fg);
				val->intval = fg->battery_id;
				pr_info("%s: bat_id_gpio = %d\n", __func__, val->intval);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			break;
		case POWER_SUPPLY_EXT_PROP_VFSOC:
			ret = max17333_get_vfsoc(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_VFREMCAP:
			ret = max17333_get_vfremcap(fg, &val->intval);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_EXT_PROP_FG_PROBE_STATUS:
			val->intval = is_probe_done;
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

static int max17333_fg_set_property(struct power_supply *psy,
								 enum power_supply_property psp,
								 const union power_supply_propval *val)
{
	struct max17333_fg_data *fg = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	int ret = 0;

	pr_info("%s[%s] set property\n", __func__, max17333_battery_type[fg->battery_type]);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max17333_set_temp_lower_limit(fg, val->intval);
		if (ret < 0)
			dev_err(fg->dev, "[%s] temp alert min set fail : %d\n",
					max17333_battery_type[fg->battery_type], ret);
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max17333_set_temp_upper_limit(fg, val->intval);
		if (ret < 0)
			dev_err(fg->dev, "[%s] temp alert max set fail : %d\n",
					max17333_battery_type[fg->battery_type], ret);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
		ret = max17333_set_min_capacity_alert_th(fg, val->intval);
		if (ret < 0)
			dev_err(fg->dev, "[%s] capacity alert min set fail : %d\n",
					max17333_battery_type[fg->battery_type], ret);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
		ret = max17333_set_max_capacity_alert_th(fg, val->intval);
		if (ret < 0)
			dev_err(fg->dev, "[%s] capacity alert max set fail : %d\n",
					max17333_battery_type[fg->battery_type], ret);
		break;
	case POWER_SUPPLY_PROP_TEMP:
	{
		u16 filtercfg_val_before, filtercfg_val_after, filtercfg_val;

		if (val->intval < 0 && !low_temp_wa) {
			low_temp_wa = true;
			max17333_read(fg->regmap, 0x29, &filtercfg_val_before);
			filtercfg_val = (filtercfg_val_before & 0xFFF0)|0x7;
			pr_info("%s: FilterCFG_before(0x%0x), FilterCFG_val(0x%0x)\n",
				__func__, filtercfg_val_before, filtercfg_val);
			max17333_write(fg->regmap, 0x29, filtercfg_val);
			max17333_read(fg->regmap, 0x29, &filtercfg_val_after);
			pr_info("%s: FilterCFG_after(0x%0x)\n", __func__, filtercfg_val_after);
		} else if (val->intval > 30 && low_temp_wa) {
			low_temp_wa = false;
			max17333_read(fg->regmap, 0x29, &filtercfg_val_before);
			filtercfg_val = (filtercfg_val_before & 0xFFF0)|0x4;
			pr_info("%s: FilterCFG_before(0x%0x), FilterCFG_val(0x%0x)\n",
				__func__, filtercfg_val_before, filtercfg_val);
			max17333_write(fg->regmap, 0x29, filtercfg_val);
			max17333_read(fg->regmap, 0x29, &filtercfg_val_after);
			pr_info("%s: FilterCFG_after(0x%0x)\n", __func__, filtercfg_val_after);
		}
		pr_info("%s: low_temp_wa(%d)\n", __func__, low_temp_wa);
		max17333_write_temp(fg, val->intval);
	}
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB) {
			if (!max17333_fg_reset(fg, 0))
				return -EINVAL;
			pr_info("%s : [%s] max17333_fg_reset_soc done.[VCELL_EQUAL_VBATT]\n",
					__func__, max17333_battery_type[fg->battery_type]);
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET_SUB_PKCP) {
			if (!max17333_fg_reset(fg, 1))
				return -EINVAL;
			pr_info("%s : [%s] max17333_fg_reset_soc done.[VCELL_EQUAL_PKCP]\n",
					__func__, max17333_battery_type[fg->battery_type]);
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if IS_ENABLED(CONFIG_DUAL_FUELGAUGE)
		case POWER_SUPPLY_EXT_PROP_CAPACITY_DUAL:
			if (val->intval == 1) {
				if (max17333_reset_fuelgauge_wo_delay(fg, 1))
					return -EINVAL;
				pr_info("%s : [%s] max17333_fg_reset_soc donee.[VCELL_EQUAL_PKCP]\n",
					__func__, max17333_battery_type[fg->battery_type]);
			} else {
				max17333_fg_read_soc_data(fg);
				pr_info("%s : [%s] max17333_fg_read_soc_data donee.\n",
					__func__, max17333_battery_type[fg->battery_type]);
			}
		break;
#endif
		default:
			return -EINVAL;
		}
	break;
	default:
		return -EINVAL;
	}

	return ret;
}

int max17333_get_bat_id(int bat_id[], int bat_gpio_cnt)
{
	int battery_id = 0;
	int i = 0;

	for (i = (bat_gpio_cnt - 1); i >= 0; i--)
		battery_id += bat_id[i] << i;

	pr_info("%s: battery_id = %d\n", __func__, battery_id);
	return battery_id;
}

static void max17333_reset_bat_id(struct max17333_fg_data *fg)
{
	int bat_id[BAT_GPIO_NO] = {-1};
	int i = 0;

	for (i = 0; i < fg->pdata->bat_gpio_cnt; i++)
		bat_id[i] = gpio_get_value(fg->pdata->bat_id_gpio[i]);

	fg->battery_id =
		max17333_get_bat_id(bat_id, fg->pdata->bat_gpio_cnt);

}

static int max17333_property_is_writeable(struct power_supply *psy,
										  enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int max17333_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max17333_fg_attrs); i++) {
		rc = device_create_file(dev, &max17333_fg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max17333_fg_attrs[i]);

	return rc;
}

ssize_t max17333_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_fg_data *fg = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_fg_attrs;
	int rval = 0, ret, rc = 0;

	pr_info("%s : [%s]\n", __func__, max17333_battery_type[fg->battery_type]);

	switch (offset) {
	case MAX17333_FG_VCELL:
		ret = max17333_get_vcell(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "VCELL : %d mV\n", rval);
		break;

	case MAX17333_FG_AVGVCELL:
		ret = max17333_get_avgvcell(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AVGVCELL : %d mV\n", rval);
		break;

	case MAX17333_FG_CURRENT:
		ret = max17333_get_current(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "CURRENT : %d mA\n", rval);
		break;

	case MAX17333_FG_AVGCURRENT:
		ret = max17333_get_avgcurrent(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AVGCURRENT : %d mA\n", rval);
		break;

	case MAX17333_FG_AVGCELL:
		ret = max17333_get_avgcell(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AVGCELL1 : %d mV\n", rval);
		break;

	case MAX17333_FG_QH:
		ret = max17333_get_qh(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "QH : %d mAh\n", rval);
		break;

	case MAX17333_FG_DESIGNCAP:
		ret = max17333_get_designcap(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "DESIGNCAP : %d mAh\n", rval);
		break;

	case MAX17333_FG_FULLCAPREP:
		ret = max17333_get_fullcaprep(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "FULLCAPREP : %d mAh\n", rval);
		break;

	case MAX17333_FG_FULLCAPNOM:
		ret = max17333_get_fullcapnom(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "FULLCAPNOM : %d mAh\n", rval);
		break;

	case MAX17333_FG_AVCAP:
		ret = max17333_get_avcap(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AVCAP : %d mAh\n", rval);
		break;

	case MAX17333_FG_REPCAP:
		ret = max17333_get_repcap(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "REPCAP : %d mAh\n", rval);
		break;

	case MAX17333_FG_MIXCAP:
		ret = max17333_get_mixcap(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "REPCAP : %d mAh\n", rval);
		break;

	case MAX17333_FG_AVSOC:
		ret = max17333_get_avsoc(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AVSOC : %d %%\n", rval);
		break;

	case MAX17333_FG_REPSOC:
		ret = max17333_get_repsoc(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "REPSOC : %d %%\n", rval);
		break;

	case MAX17333_FG_MIXSOC:
		ret = max17333_get_mixsoc(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "MIXSOC : %d %%\n", rval);
		break;

	case MAX17333_FG_AGE:
		ret = max17333_get_age(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AGE : %d %%\n", rval);
		break;

	case MAX17333_FG_AGEFORECAST:
		ret = max17333_get_ageforecast(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "AGEFORECAST : %d\n", rval);
		break;

	case MAX17333_FG_CYCLES:
		ret = max17333_get_cycles(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "CYCLES : %d cycles\n", rval);
		break;
	case MAX17333_FG_TEMP:
		ret = max17333_get_temperature(fg, &rval);
		if (ret < 0)
			return ret;
		rc += snprintf(buf + rc, PAGE_SIZE - rc, "TEMP : %d\n", rval);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

ssize_t max17333_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17333_fg_data *fg = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17333_fg_attrs;
	int x, ret;
	u16 value;

	pr_info("%s : [%s]\n", __func__, max17333_battery_type[fg->battery_type]);

	switch (offset) {
	case MAX17333_FG_TEMP:
		if (sscanf(buf, "%5d\n", &x) == 1) {
			pr_info("%s : [%s] Temperature = %d\n", __func__, max17333_battery_type[fg->battery_type], x);
			value = (x << 8);
			max17333_write(fg->regmap, REG_TEMP, value);
		}
		ret = count;
		break;
	case MAX17333_FG_PORCMD:
		max17333_set_por_cmd(fg);
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int max17333_fg_initialize(struct max17333_fg_data *fg)
{
	int ret;
	u16 pval;

	ret = max17333_read(fg->regmap, REG_VERSION, &pval);
	if (ret < 0) {
		dev_err(fg->dev, "[%s]: fail to read REG_VERSION\n", max17333_battery_type[fg->battery_type]);
		return ret;
	}

	pr_info("%s : IC Version: 0x%04x\n", __func__, pval);

	/* Optional step - alert threshold initialization */
	max17333_set_alert_thresholds(fg);

	/* Clear Status.POR */
	ret = max17333_read(fg->regmap, REG_STATUS, &pval);
	if (ret < 0) {
		dev_err(fg->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[fg->battery_type]);
		return ret;
	}

	ret = max17333_write(fg->regmap, REG_STATUS, pval & ~BIT_STATUS_POR);
	if (ret < 0) {
		dev_err(fg->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[fg->battery_type]);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_OF
static int max17333_fg_parse_dt(struct max17333_fg_data *fg)
{
	struct device *dev = fg->dev;
	struct device_node *np;
	struct max17333_fg_platform_data *pdata;
	int ret = 0;
	int i = 0;
	u8 battery_id = 0;
	int bat_id[BAT_GPIO_NO] = {-1};
	u32 pval;

	pr_info("%s[%s] start\n", __func__, max17333_battery_type[fg->battery_type]);
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return -ENOMEM;

	if (fg->battery_type)
		np = of_find_node_by_name(NULL, "fuelgauge-main");
	else
		np = of_find_node_by_name(NULL, "fuelgauge-sub");

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s[%s] np NULL\n", __func__, max17333_battery_type[fg->battery_type]);
		return -EINVAL;
	}

	pr_info("%s[%s] talrt-min\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "talrt-min", &pval);
	pdata->temp_min = (!ret) ? pval : -128; /* DegreeC */ /* -128 == Disable alert */

	pr_info("%s[%s] talrt-max\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "talrt-max", &pval);
	pdata->temp_max = (!ret) ? pval : 127; /* DegreeC */ /* 127 == Disable alert */

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	pr_info("%s[%s] valrt-min\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "valrt-min", &pval);
	pdata->volt_min = (!ret) ? pval : 0; /* mV */ /* 0 == Disable alert */
#else
	pdata->volt_min = 0; /* mV */ /* 0 == Disable alert */
#endif

	pr_info("%s[%s] valrt-max\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "valrt-max", &pval);
	pdata->volt_max = (!ret) ? pval : 5100; /* mV */ /* 5100 == Disable alert */

	pr_info("%s[%s] ialrt-min\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "ialrt-min", &pval);
	pdata->curr_min = (!ret) ? pval : -5120; /* mA */ /* -5120 == Disable alert */

	pr_info("%s[%s] ialrt-max\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "ialrt-max", &pval);
	pdata->curr_max = (!ret) ? pval : 5080; /* mA */ /* 5080 == Disable alert */

	pr_info("%s[%s] salrt-min\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "salrt-min", &pval);
	pdata->soc_min = (!ret) ? pval : 0; /* Percent */ /* 0 == Disable alert */

	pr_info("%s[%s] salrt-max\n", __func__, max17333_battery_type[fg->battery_type]);
	ret = of_property_read_u32(np, "salrt-max", &pval);
	pdata->soc_max = (!ret) ? pval : 255; /* Percent */ /* 255 == Disable alert */

	pdata->bat_gpio_cnt = of_gpio_named_count(np, "fuelgauge,sub_bat_id_gpio");
	/* not run if gpio gpio cnt is less than 1 */
	if (pdata->bat_gpio_cnt > 0) {
		pr_info("%s: Has %d bat_id_gpios\n", __func__, pdata->bat_gpio_cnt);
		if (pdata->bat_gpio_cnt > BAT_GPIO_NO) {
			pr_err("%s: bat_id_gpio, catch out-of bounds array read\n",
					__func__);
			pdata->bat_gpio_cnt = BAT_GPIO_NO;
		}
		for (i = 0; i < pdata->bat_gpio_cnt; i++) {
			pdata->bat_id_gpio[i] = of_get_named_gpio(np, "fuelgauge,sub_bat_id_gpio", i);
			if (pdata->bat_id_gpio[i] >= 0) {
				bat_id[i] = gpio_get_value(pdata->bat_id_gpio[i]);
			} else {
				pr_err("%s: error reading bat_id_gpio = %d\n",
					__func__, pdata->bat_id_gpio[i]);
				bat_id[i] = 0;
			}
		}
		fg->battery_id =
				max17333_get_bat_id(bat_id, pdata->bat_gpio_cnt);
	} else
		fg->battery_id = 0;

	battery_id = fg->battery_id;
	pr_info("%s: battery_id(batt_id:%d) = %d\n", __func__, fg->battery_id, battery_id);

	fg->pdata = pdata;
	pr_info("%s[%s] finished\n", __func__, max17333_battery_type[fg->battery_type]);
	return 0;

}
#endif

static irqreturn_t max17333_fg_irq_isr(int irq, void *data)
{
	struct max17333_fg_data *fg = data;
	int ret;
	u16 status;

#if 1
    u16 vg_trim_val;
    u16 cg_trim_val;
    u16 tg_trim_val;
    u16 hconfig2;
    u16 temp_raw;
    u16 volt_raw;
    u16 curr_raw;
    u16 hconfig;
    u16 vfocv;
	u16 hprotcfg;
	u16 hprotcfg2;
#endif

	/* Check alert type */
	ret = max17333_read(fg->regmap, REG_STATUS, &status);
	if (ret < 0) {
		dev_err(fg->dev, "[%s]: fail to read REG_STATUS\n", max17333_battery_type[fg->battery_type]);
		return IRQ_HANDLED;
	}

	pr_info("%s : [%s]STATUS (%04xh): Interrupt Handler!\n", __func__, max17333_battery_type[fg->battery_type], status);

	if (status & BIT_STATUS_POR)
		pr_info("%s : [%s] Alert: POR Detect!\n", __func__, max17333_battery_type[fg->battery_type]);

	if (status & BIT_STATUS_SMN)
		pr_info("%s : [%s] Alert: SOC MIN!\n", __func__, max17333_battery_type[fg->battery_type]);

	if (status & BIT_STATUS_VMN)
		pr_info("%s : [%s] Alert: VOLT MIN!\n", __func__, max17333_battery_type[fg->battery_type]);

	/* Clear Status */
	if (status & BIT_STATUS_PA) {
		ret = max17333_write(fg->regmap, REG_STATUS, REG_STATUS_MASK | BIT_STATUS_PA);
		if (ret < 0) {
			dev_err(fg->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[fg->battery_type]);
			return IRQ_HANDLED;
		}
	} else {
		ret = max17333_write(fg->regmap, REG_STATUS, REG_STATUS_MASK);
		if (ret < 0) {
			dev_err(fg->dev, "[%s]: fail to write REG_STATUS\n", max17333_battery_type[fg->battery_type]);
			return IRQ_HANDLED;
		}
	}

#if 1 // dump all
    max17333_read(fg->regmap, REG_VG_TRIM, &vg_trim_val);
    max17333_read(fg->regmap, REG_CG_TRIM, &cg_trim_val);
    max17333_read(fg->regmap, REG_TG_TRIM, &tg_trim_val);
    max17333_read(fg->regmap, REG_HCONFIG2, &hconfig2);
    max17333_read(fg->regmap, REG_TEMP_RAW, &temp_raw);
    max17333_read(fg->regmap, REG_VOLT_RAW, &volt_raw);
    max17333_read(fg->regmap, REG_CURR_RAW, &curr_raw);
	max17333_read(fg->regmap, REG_HCONFIG, &hconfig);
	max17333_read(fg->regmap, REG_VFOCV, &vfocv);
	max17333_read(fg->regmap, REG_HPROTCFG, &hprotcfg);
	max17333_read(fg->regmap, REG_HPROTCFG2, &hprotcfg2);

    pr_info("[MAX17333][%s] HW: %04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh\n", max17333_battery_type[fg->battery_type],
			vg_trim_val, cg_trim_val, tg_trim_val, hconfig2, temp_raw, volt_raw, curr_raw, hconfig, vfocv, hprotcfg, hprotcfg2);
#endif

	return IRQ_HANDLED;
}

static int max17333_fg_probe(struct platform_device *pdev)
{
	struct max17333_dev *max17333 = dev_get_drvdata(pdev->dev.parent);
	struct max17333_fg_platform_data *pdata = dev_get_platdata(max17333->dev);
	struct max17333_fg_data *fg;
	struct power_supply_config fg_cfg = {};
	int ret = 0;
	u16 val_filtercfg, filtercfg_val;

	pr_info("%s: MAX17333 Fuelgauge Driver Loading\n", __func__);

	fg = kzalloc(sizeof(*fg), GFP_KERNEL);
	if (!fg)
		return -ENOMEM;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(!pdata)) {
		pr_err("%s: out of memory\n", __func__);
		pdata = ERR_PTR(-ENOMEM);
		ret = -ENOMEM;
		goto error;
	}

	mutex_init(&fg->lock);

	fg->dev = &pdev->dev;
	fg->max17333 = max17333;
	fg->regmap = max17333->regmap_pmic;
	fg->regmap_shdw = max17333->regmap_shdw;
	fg->pdata = pdata;
	fg->rsense = max17333->pdata->rsense;
	fg->battery_type = max17333->pdata->battery_type;

#if defined(CONFIG_OF)
	ret = max17333_fg_parse_dt(fg);
	if (ret < 0)
		pr_err("%s not found fuelgauge dt! ret[%d]\n", __func__, ret);
#else
	pdata = dev_get_platdata(&pdev->dev);
#endif

	platform_set_drvdata(pdev, fg);
	if (fg->battery_type) {
		pr_info("%s: max17333 battery main power supply register\n", __func__);
		fg->psy_batt_d.name     = "max17333-battery";
		fg->psy_batt_d.type     = POWER_SUPPLY_TYPE_UNKNOWN;
		fg->psy_batt_d.properties   = max17333_fg_battery_props;
		fg->psy_batt_d.get_property = max17333_fg_get_property;
		fg->psy_batt_d.set_property = max17333_fg_set_property;
		fg->psy_batt_d.property_is_writeable   = max17333_property_is_writeable;
		fg->psy_batt_d.num_properties =
			ARRAY_SIZE(max17333_fg_battery_props);
		fg->psy_batt_d.no_thermal = true;
		fg_cfg.drv_data = fg;
		fg_cfg.supplied_to = batt_supplied_to;
		fg_cfg.of_node = max17333->dev->of_node;
		fg_cfg.num_supplicants = ARRAY_SIZE(batt_supplied_to);
	} else {
		pr_info("%s: max17333 battery sub power supply register\n", __func__);
		fg->psy_batt_d.name     = "max17333-battery-sub";
		fg->psy_batt_d.type     = POWER_SUPPLY_TYPE_UNKNOWN;
		fg->psy_batt_d.properties   = max17333_fg_battery_props;
		fg->psy_batt_d.get_property = max17333_fg_get_property;
		fg->psy_batt_d.set_property = max17333_fg_set_property;
		fg->psy_batt_d.property_is_writeable   = max17333_property_is_writeable;
		fg->psy_batt_d.num_properties =
			ARRAY_SIZE(max17333_fg_battery_props);
		fg->psy_batt_d.no_thermal = true;
		fg_cfg.drv_data = fg;
		fg_cfg.supplied_to = batt_sub_supplied_to;
		fg_cfg.of_node = max17333->dev->of_node;
		fg_cfg.num_supplicants = ARRAY_SIZE(batt_sub_supplied_to);
	}

	fg->psy_fg =
		devm_power_supply_register(max17333->dev,
				&fg->psy_batt_d,
				&fg_cfg);
	if (IS_ERR(fg->psy_fg)) {
		pr_err("Couldn't register battery rc=%ld\n",
				PTR_ERR(fg->psy_fg));
		goto error;
	}

	if (max17333->irq > 0) {
		ret = devm_request_threaded_irq(max17333->dev, max17333->irq, NULL,
			max17333_fg_irq_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
			"fuelgauge-irq", fg);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto error_fg_irq;
		}
		dev_info(&pdev->dev, "MAX17333 Fuel-Gauge irq requested %d\n", max17333->irq);
	}

	/* Clear alerts */
	max17333_write(fg->regmap, REG_PROTALRTS, REG_PROTALRTS_MASK);
	max17333_write(fg->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);
	max17333_write(fg->regmap, REG_STATUS, REG_STATUS_MASK);
	max17333_write(fg->regmap, REG_PROTSTATUS, REG_PROTSTATUS_MASK);

	/* Enable alert on fuel-gauge outputs */
	max17333_update_bits(fg->regmap, REG_CONFIG, BIT_CONFIG_ALRT_EN, BIT_CONFIG_ALRT_EN);

	ret = max17333_fg_create_attrs(&fg->psy_fg->dev);
	if (ret)
		dev_err(fg->dev, "%s : Failed to create_attrs\n", __func__);

	ret = max17333_fg_initialize(fg);
	if (ret < 0) {
		dev_err(fg->dev, "Error: Initializing fuel-gauge\n");
		goto error_init;
	}
	max17333_read(fg->regmap, 0x29, &val_filtercfg);
	if ((val_filtercfg & 0xF) == 7) {
		//Check 3:0 bits value if 7 then wa active.
		low_temp_wa = true;
		pr_info("%s: FilterCFG(0x%0x), low_temp_wa(%d)\n", __func__, val_filtercfg, low_temp_wa);
	} else if ((val_filtercfg & 0xF) == 3) {
		//Check 3:0 bits value if 3 then wa active.
		filtercfg_val = (val_filtercfg & 0xFFF0)|0x4;
		pr_info("%s: FilterCFG_before(0x%0x), FilterCFG_val(0x%0x)\n",
			__func__, val_filtercfg, filtercfg_val);
		max17333_write(fg->regmap, 0x29, filtercfg_val);
		max17333_read(fg->regmap, 0x29, &val_filtercfg);
		pr_info("%s: FilterCFG(0x%0x), low_temp_wa(%d)\n", __func__, val_filtercfg, low_temp_wa);
	}
	is_probe_done = true;
	pr_info("%s: Done to Load MAX17333 Fuelgauge Driver\n", __func__);
	return 0;

error_init:
	if (max17333->irq)
		free_irq(max17333->irq, fg);

error_fg_irq:
	power_supply_unregister(fg->psy_fg);

error:
	kfree(fg);

	return ret;
}

static int max17333_fg_remove(struct platform_device *pdev)
{
	struct max17333_fg_data *fg = platform_get_drvdata(pdev);

	power_supply_unregister(fg->psy_fg);
	kfree(fg);

	return 0;
}

#if defined(CONFIG_PM)
static int max17333_fg_suspend(struct device *dev)
{

	pr_info("%s\n", __func__);

	return 0;
}

static int max17333_fg_resume(struct device *dev)
{

	pr_info("%s\n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops max17333_fg_pm_ops = {
	.suspend = max17333_fg_suspend,
	.resume = max17333_fg_resume,
};

static const struct platform_device_id max17333_fg_id[] = {
	{ "max17333-battery", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_fg_id);

static struct platform_driver max17333_fg_driver = {
	.driver = {
		.name = "max17333-battery",
#if defined(CONFIG_PM)
		.pm = &max17333_fg_pm_ops,
#endif
	},
	.probe = max17333_fg_probe,
	.remove = max17333_fg_remove,
	.id_table = max17333_fg_id,
};

static const struct platform_device_id max17333_fg_sub_id[] = {
	{ "max17333-battery-sub", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17333_fg_sub_id);

static struct platform_driver max17333_fg_sub_driver = {
	.driver = {
		.name = "max17333-battery-sub",
#if defined(CONFIG_PM)
		.pm = &max17333_fg_pm_ops,
#endif
	},
	.probe = max17333_fg_probe,
	.remove = max17333_fg_remove,
	.id_table = max17333_fg_sub_id,
};

static int __init max17333_fg_init(void)
{
	pr_info("%s:\n", __func__);
	platform_driver_register(&max17333_fg_driver);
	platform_driver_register(&max17333_fg_sub_driver);
	return 0;
}

static void __exit max17333_fg_exit(void)
{
	platform_driver_unregister(&max17333_fg_driver);
	platform_driver_unregister(&max17333_fg_sub_driver);
}

module_init(max17333_fg_init);
module_exit(max17333_fg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX17333 Fuel Gauge");
MODULE_VERSION(MAX17333_RELEASE_VERSION);
