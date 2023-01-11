/*
 * Battery driver for Marvell 88PM80x PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 * Author:	Jett Zhou <jtzhou@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/power_supply.h>

#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/battery/fuelgauge/88pm80x_fg.h>
#include <linux/battery/fuelgauge/88pm80x_table.h>
#include <linux/battery/sec_fuelgauge.h>

static int pm80x_battery_suspend(struct sec_fg_info *info);
static int pm80x_battery_resume(struct sec_fg_info *info);

static void get_batt_status(struct sec_fg_info *info)
{
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
			POWER_SUPPLY_PROP_STATUS, value);
	info->bat_params.status = value.intval;
}

static int get_power_status(struct sec_fg_info *info)
{
	union power_supply_propval value;
	psy_do_property("sec-charger", get, POWER_SUPPLY_PROP_POWER_STATUS, value);
	return value.intval;
}

static void get_batt_present(fuelgauge_variable_t *fuelgauge)
{
	struct sec_fg_info *info = &fuelgauge->info;
	int volt;

	/*
	 * for example, on Goldenve, the battery NTC resistor is 2.4Kohm +-6%
	 * so, with the bias current = 76uA,
	 * if the GPADC0 voltage is in [171.456mV, 193.344mV],
	 * we think battery is present when the voltage is in [140mV, 220mV]
	 * for redundancy, else not;
	 */
	volt = extern_get_gpadc_bias_volt(info->batt_vf_nr, info->online_gp_bias_curr);
	printk(KERN_DEBUG"%s: VF voltage = %dmV\n", __func__, volt);

	if ((volt <= info->hi_volt_online) && (volt >= info->lo_volt_online))
		fuelgauge->info.bat_params.present = 1;
	else
		fuelgauge->info.bat_params.present = 0;
}

static int  __attribute__((unused)) get_gpadc2(struct sec_fg_info *info, int gpadc2_bias)
{
	int ret, tbat, data;
	unsigned char buf[2];

	fg_regmap_read(FG_REGMAP_GPADC, PM800_GPADC_BIAS3, &data);
	data &= 0xF0;
	data |= gpadc2_bias;
	fg_regmap_write(FG_REGMAP_GPADC, PM800_GPADC_BIAS3, data);

	/*
	 * There is no way to measure the temperature of battery,
	 * report the pmic temperature value
	 */
	ret = fg_regmap_bulk_read(FG_REGMAP_GPADC, PM800_GPADC2_MEAS1, buf, 2);
	if (ret < 0) {
		dev_err(info->dev, "Attention: failed to get battery tbat!\n");
		return -EINVAL;
	}

	tbat = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	dev_dbg(info->dev, "%s: tbat value = 0x%x\n", __func__, tbat);
	/* tbat = value * 1.4 *1000/(2^12) */
	tbat = ((tbat & 0xfff) * 7 * 100) >> 11;
	dev_dbg(info->dev, "%s: tbat voltage = %dmV\n", __func__, tbat);

	return tbat;
}

static int get_gpadc(struct sec_fg_info *info, int bias_current)
{
	int ret, tbat, data;
	unsigned char buf[2];
	int batt_gp_bias, batt_gp_meas;

	pr_debug("%s: info->batt_gp_nr : %d\n", __func__, info->batt_gp_nr);

	switch (info->batt_gp_nr) {
	case PM800_GPADC0:
		batt_gp_bias = PM800_GPADC_BIAS1;
		batt_gp_meas = PM800_GPADC0_MEAS1;
		break;
	case PM800_GPADC1:
		batt_gp_bias = PM800_GPADC_BIAS2;
		batt_gp_meas = PM800_GPADC1_MEAS1;
		break;
	case PM800_GPADC2:
		batt_gp_bias = PM800_GPADC_BIAS3;
		batt_gp_meas = PM800_GPADC2_MEAS1;
		break;
	case PM800_GPADC3:
		batt_gp_bias = PM800_GPADC_BIAS4;
		batt_gp_meas = PM800_GPADC3_MEAS1;
		break;
	default:
		dev_err(info->chip->dev, "get GPADC failed!\n");
		return -EINVAL;
	}

	fg_regmap_read(FG_REGMAP_GPADC, batt_gp_bias, &data);
	data &= 0xF0;
	data |= bias_current;
	fg_regmap_write(FG_REGMAP_GPADC, batt_gp_bias, data);

	ret = fg_regmap_bulk_read(FG_REGMAP_GPADC, batt_gp_meas, buf, 2);
	if (ret < 0) {
		dev_err(info->dev, "Attention: failed to get battery tbat!\n");
		return -EINVAL;
	}

	tbat = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	dev_dbg(info->dev, "%s: tbat value = 0x%x\n", __func__, tbat);
	/* tbat = value * 1.4 *1000/(2^12) */
	tbat = ((tbat & 0xfff) * 7 * 100) >> 11;
	dev_dbg(info->dev, "%s: tbat voltage = %dmV\n", __func__, tbat);

	return tbat;
}

static void get_batt_temp(struct sec_fg_info *info)
{
	int temp, tbat, i;
	/* bias register value */
	unsigned int bias_value[6] = {0x06, 0x0C, 0x0F, 0x03, 0x02, 0x01};
	/* bias current value: mA; _MUST_ be aligned with bias_value */
	unsigned int bias_current[6] = {31, 61, 76, 16, 11, 6};

	/*
	 *1) set bias as 31uA firstly for root remp environment,
	 *2) check the voltage whether it's in [0.3V, 1.25V];
	 *   if yes, tbat = tbat/31; break;
	 *   else
	 *   set bias as 61uA/16uA/11uA/6uA...
	 *   for lower or higher temp environment.
	 */
	for(i = 5; i < 6; i++) {
		tbat = get_gpadc(info, bias_value[i]);
		dev_dbg(info->dev, "%s: tbat = %dmV, i = %d\n", __func__, tbat, i);
		if ((tbat > 150) && (tbat < 1250)) {
			tbat *= 10000;
			tbat /= bias_current[i];
			break;
		}
	}

	/* ABNORMAL: report the fake value 25C */
	if (i == 6) {
		dev_err(info->dev, "Fake raw temperature is %dC\n",
						info->bat_params.temp);
		return;
	}
	dev_dbg(info->dev, "%s: tbat resistor = %dohm, i = %d\n",
		__func__, tbat, i);

	/* [-25 ~ 65] */
	for (i = 0; i < 91; i++) {
		if (tbat >= temperature_table[i]) {
			temp = -25 + i;
			break;
		}
	}
	if (i == 91) /* max temperature */
		temp = 65;

	dev_dbg(info->dev, "raw temp is %dC, i = %d, temp_table[i] = %d\n",
		temp, i, temperature_table[i]);
	info->bat_params.temp = temp;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int get_batt_vol(struct sec_fg_info *info,
			int *data, int active)
{
	int ret;
	unsigned char buf[2];
	if (!data)
		return -EINVAL;

	if (active) {
		ret = fg_regmap_bulk_read(FG_REGMAP_GPADC, PM800_VBAT_AVG, buf, 2);
		if (ret < 0)
			return ret;
	} else {
		ret = fg_regmap_bulk_read(FG_REGMAP_GPADC, PM800_VBAT_SLP, buf, 2);
		if (ret < 0)
			return ret;
	}

	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	dev_dbg(info->dev,
		"%s buf[0]:[0x42: 0x%x], buf[1]:[0x43: 0x%x], data: 0x%x\n",
		__func__, buf[0], buf[1], *data);
	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	*data = ((*data & 0xfff) * 7 * 100) >> 9;
	dev_dbg(info->dev, "+++++++>%s: battery voltage = %d\n",
					__func__, *data);
	/* Ccalibrate vbat measurement only for un-trimed PMIC
	 * VBATmeas = VBATreal * gain + offset, so VBATreal =
	 * (VBATmeas - offset)/ gain;
	 * According to our test of vbat_sleep in bootup, the calculated
	 * gain as below:
	 * for aruba 0.1, offset is -13.4ma, gain is 1.008.
	 * For Aruba 0.2, offset=-0.0087V and gain=1.007.
	 */
	/*
	 *data = (*data + 13) * 1000/1008;
	 *data = (*data + 9) * 1000/1007;
	 *data = (*data + 9) * 1000/1002;
	 */

	return 0;
}

/*
 * Calculate the rtot according to different ib and temp info,
 * we think about ib is y coordinate, temp is x coordinate to
 * do interpolation eg. dischg case, ib are 300/500/700/1000/1500mA,
 * temp are -5/10/25/40C. We divide them into 3 kind of case to interpolate,
 *  they are 1/2/4 point case, the refered point is top-right.
 *
 *        |-5C       |10C        |25C         |40C
 *        |          |           |            |    1 point
 * -------|----------|-----------|------------|------------- 1500mA
 *        |          |           | 4 point    |    2 point
 * -------|----------|-----------|------------|------------- 1000mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 700mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 500mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 300mA
 *        |          |           |            |
 *        |	     |           |            |
 */

static int calc_resistor(struct sec_fg_info *info, int temp, int ib)
{
	int x1, x2;
	int y1, y2;
	int p_num = 4;
	int i = 0, j;
	int chg_status;
	int PWR_Rdy_status;

	/* get_batt_status(info); */
	chg_status = info->bat_params.status;

	PWR_Rdy_status = get_power_status(info);

	dev_dbg(info->dev, "-----> %s ib : %d temp: %d \n", __func__, ib, temp);

	if ((chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
		(chg_status == POWER_SUPPLY_STATUS_UNKNOWN) ||
		(chg_status == POWER_SUPPLY_STATUS_FULL) ||
		(PWR_Rdy_status == POWER_SUPPLY_PWR_RDY_FALSE)) {
		for(i = 0; i < 6; i++ ) {
			if (ib <= dischg_ib[i])
				break;
		}
		if (i == 0 || i == 6) {
			p_num = 2;
			if (i == 6)
				i--;
		}
		for (j = 0; j < 5; j++) {
			if (temp <= -20 + j*15)
				break;
		}
		if ((j == 0 || j == 5)) {
			if (j == 5)
				j--;
			if (p_num == 2) {
				/* 1 point case */
				p_num = 1;
				if (v3 == 100)
					rtot = *(dis_chg_rtot[i][j] + 99);
				else
					rtot = *(dis_chg_rtot[i][j] + v3);

			} else {
				/* 2 point case , between two ib */
				p_num = 2;
				if (v3 == 100)
					y1 = *(dis_chg_rtot[i-1][j] + 99);
				else
					y1 = *(dis_chg_rtot[i-1][j] + v3);
				x1 = dischg_ib[i-1];
				if (v3 == 100)
					y2 = *(dis_chg_rtot[i][j] + 99);
				else
					y2 = *(dis_chg_rtot[i][j] + v3);
				x2 = dischg_ib[i];
				rtot = y1 + (y2 - y1) * (ib - x1)/(x2-x1);
			}
			dev_dbg(info->dev, "1 p_num: %d i: %d j: %d\n",
				p_num, i, j);
		} else if (p_num == 2) {
			/* 2 point case, between two temp */
			dev_dbg(info->dev, "2 p_num: %d i: %d j: %d\n",
				p_num, i, j);
			if (v3 == 100)
				y1 = *(dis_chg_rtot[i][j-1] + 99);
			else
				y1 = *(dis_chg_rtot[i][j-1] + v3);
			x1 = (j - 1) * 15 + (-20);
			if (v3 == 100)
				y2 = *(dis_chg_rtot[i][j] + 99);
			else
				y2 = *(dis_chg_rtot[i][j] + v3);
			x2 = j * 15 + (-20);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"x1: %d, y1: %d, x2: %d, y2: %d\n",
				x1, y1, x2, y2);
		}
		if (p_num == 4) {
			int rtot1, rtot2;

			dev_dbg(info->dev, "3 p_num: %d i: %d j: %d\n",
				p_num, i, j);
			/* step1, interpolate two ib in low temp */
			if (v3 == 100)
				y1 = *(dis_chg_rtot[i-1][j-1] + 99);
			else
				y1 = *(dis_chg_rtot[i-1][j-1] + v3);
			x1 = dischg_ib[i-1];
			if (v3 == 100)
				y2 = *(dis_chg_rtot[i][j-1] + 99);
			else
				y2 = *(dis_chg_rtot[i][j-1] + v3);
			x2 = dischg_ib[i];
			dev_dbg(info->dev,
				"step1 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot1 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step2, interpolate two ib in high temp*/
			if (v3 == 100)
				y1 = *(dis_chg_rtot[i-1][j] + 99);
			else
				y1 = *(dis_chg_rtot[i-1][j] + v3);
			x1 = dischg_ib[i-1];
			if (v3 == 100)
				y2 = *(dis_chg_rtot[i][j] + 99);
			else
				y2 = *(dis_chg_rtot[i][j] + v3);
			x2 = dischg_ib[i];
			dev_dbg(info->dev,
				"step2 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot2 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step3 */
			y1 = rtot1;
			x1 = (j - 1) * 15 + (-20);
			y2 = rtot2;
			x2 = j * 15 + (-20);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"step3: rtot : %d, rtot1: %d, rtot2: %d\n",
				rtot, rtot1, rtot2);
		}

	} else if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		for(i = 0; i < 4; i++ ) {
			if (ib <= chg_ib[i])
				break;
		}
		if (i == 0 || i == 4) {
			p_num = 2;
			if (i == 4)
				i --;
		}
		for(j = 0; j < 4; j++) {
			if (temp <= -5 + j*15)
				break;
		}

		if ((j == 0 || j == 4) ) {
			/* Need double check */
			if (j == 4)
				j --;

			if (p_num == 2) {
				/* 1 point case */
				p_num = 1;
				if (v3 == 100)
					rtot = *(chg_rtot[i][j] + 99);
				else
					rtot = *(chg_rtot[i][j] + v3);
			} else {
				/* 2 point case , between two ib */
				p_num = 2;
				if (v3 == 100)
					y1 = *(chg_rtot[i-1][j] + 99);
				else
					y1 = *(chg_rtot[i-1][j] + v3);
				x1 = chg_ib[i-1];
				if (v3 == 100)
					y2 = *(chg_rtot[i][j] + 99);
				else
					y2 = *(chg_rtot[i][j] + v3);
				x2 = chg_ib[i];
				rtot = y1 + (y2 - y1) * (ib - x1)/(x2-x1);
			}
			dev_dbg(info->dev, "1 p_num: %d i: %d j: %d \n",
				p_num, i, j);
		} else if(p_num == 2) {
			/* 2 point case, between two temp */
			dev_dbg(info->dev, "2 p_num: %d i: %d j: %d \n",
				p_num, i, j);
			if (v3 == 100)
				y1 = *(chg_rtot[i][j-1] + 99);
			else
				y1 = *(chg_rtot[i][j-1] + v3);
			x1 = (j - 1) * 15 + (-5);
			if (v3 == 100)
				y2 = *(chg_rtot[i][j] + 99);
			else
				y2 = *(chg_rtot[i][j] + v3);
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"x1: %d, y1: %d, x2: %d, y2: %d \n",
				x1, y1, x2, y2 );
		}

		if (p_num == 4) {
			int rtot1, rtot2;

			dev_dbg(info->dev, "3 p_num: %d i: %d j: %d \n",
				p_num, i, j);
			// step1, interpolate two ib in low temp
			if (v3 == 100)
				y1 = *(chg_rtot[i-1][j-1] + 99);
			else
				y1 = *(chg_rtot[i-1][j-1] + v3);
			x1 = chg_ib[i-1];
			if (v3 == 100)
				y2 = *(chg_rtot[i][j-1] + 99);
			else
				y2 = *(chg_rtot[i][j-1] + v3);
			x2 = chg_ib[i];
			dev_dbg(info->dev,
				"step1 x1: %d , y1, %d , x2 : %d y2: %d \n",
				x1, y1, x2, y2);
			rtot1 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			// step2, interpolate two ib in high temp
			if (v3 == 100)
				y1 = *(chg_rtot[i-1][j] + 99);
			else
				y1 = *(chg_rtot[i-1][j] + v3);
			x1 = chg_ib[i-1];
			if (v3 == 100)
				y2 = *(chg_rtot[i][j] + 99);
			else
				y2 = *(chg_rtot[i][j] + v3);
			x2 = chg_ib[i];
			dev_dbg(info->dev, "step2 x1: %d , y1, %d , x2 : %d y2: %d \n",
				x1, y1, x2, y2);
			rtot2 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			// step3
			y1 = rtot1;
			x1 = (j - 1) * 15 + (-5);
			y2 = rtot2;
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"step3: rtot : %d, rtot1: %d, rtot2: %d \n",
				rtot, rtot1, rtot2);
		}
	}
	dev_dbg(info->dev, "<-----%s r_tot : %d p_num: %d \n",
		__func__, rtot, p_num);
	return rtot;
}

/* PM800_RTC_MISC5:E7[1] and PM800_RTC_CONTROL:D0[3:2] can be used */
static void fg_store(struct sec_fg_info *info)
{
	int data;

	deltaR_to_store = r_off - r_off_initial;
	//deltaR_to_store = 0; // uncomment to reset the value of deltaR
	deltaR_to_store = (deltaR_to_store >= 0) ? min(deltaR_to_store, 127) : max(deltaR_to_store, -128);

	if (v3 <= 0) {
		if (vbat_mv <= 3900) {
			v3 = 0;
			pr_info("PROTECTION:  v3 <= 0; Vbat <= 3.9V; STORED V3 = 0 \n");
		} else {
			v3 = 100;
			pr_info("PROTECTION:  v3 <= 0; Vbat > 3.9V; STORED V3 = 100 \n");
		}
	} else if (v3 > 100)
		v3 = 100;
	else
		v3 &= 0x7F;

	fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &data);
	data = (data & 0x80) | v3;
	fg_regmap_write(FG_REGMAP_BASE, PM800_USER_DATA5, data);

	/* v1 is Volt with factor(2^20) */
	v1_mv = v1 >> 10;
	/* v1_mv resolution is 3mV */
	v1_mv /= 3;
	v1_mv = (v1_mv >= 0) ? min(v1_mv, 31) : max(v1_mv, -32);
	v1_mv &= 0x3F;
	//fg_regmap_write(FG_REGMAP_BASE, PM800_USER_DATA6, v1_mv);

	/* v2 is Volt with factor(2^20) */
	v2_mv = v2 >> 10;
	v2_mv /= 3;
	v2_mv = (v2_mv >= 0) ? min(v2_mv, 31) : max(v2_mv, -32);
	v2_mv &= 0x3F;
	fg_regmap_write(FG_REGMAP_BASE, 0xDD, v1_mv);
	fg_regmap_write(FG_REGMAP_BASE, 0xDE, v2_mv);
	fg_regmap_write(FG_REGMAP_BASE, 0xDF, deltaR_to_store);
	fg_regmap_read(FG_REGMAP_BASE, 0xDF, &data);
	data = v3_res % factor2 /100;
	data &= 0x0F;
	fg_regmap_write(FG_REGMAP_BASE, 0xE0, data);

	dev_dbg(info->dev, "%s, writing to 0xDD: 0x%x \n", __func__, v1_mv);
	dev_dbg(info->dev, "%s, writing to 0xDE: 0x%x \n", __func__, v2_mv);
	dev_dbg(info->dev, "%s, writing to 0xDF: 0x%x \n", __func__, deltaR_to_store);
	dev_dbg(info->dev, "%s, writing to 0xE0: 0x%x \n", __func__, data);

	fg_regmap_read(FG_REGMAP_BASE, 0xDD, &data);
	dev_dbg(info->dev, "%s, reading of 0xDD: 0x%x \n", __func__, data);
	fg_regmap_read(FG_REGMAP_BASE, 0xDE, &data);
	dev_dbg(info->dev, "%s, reading of 0xDE: 0x%x \n", __func__, data);
	fg_regmap_read(FG_REGMAP_BASE, 0xDF, &data);
	dev_dbg(info->dev, "%s, reading of 0xDF: 0x%x \n", __func__, data);
	fg_regmap_read(FG_REGMAP_BASE, 0xE0, &data);
	dev_dbg(info->dev, "%s, reading of 0xE0: 0x%x \n", __func__, data);

	fg_regmap_read(FG_REGMAP_BASE, 0xDF, &data);

	dev_dbg(info->dev, "%s deltaR_to_store: %d \n", __func__,
		(deltaR_to_store & 1 << 7) ? deltaR_to_store | 0xFFFFFF00: deltaR_to_store & 0x7F);
	dev_dbg(info->dev, "%s v1_mv: 0x%x , v2_mv : 0x%x\n",
		__func__, v1_mv, v2_mv);
	dev_dbg(info->dev, "%s v1_mv: %d \n", __func__,
		(v1_mv & 1 << 5) ? v1_mv | 0xFFFFFFC0: v1_mv & 0x1F);
	dev_dbg(info->dev, "%s v2_mv: %d \n", __func__,
		(v2_mv & 1 << 5) ? v2_mv | 0xFFFFFFC0: v2_mv & 0x1F);
}

/*
 * From OCV curves, extract OCV for a given v3 (SOC).
 * Ib= (OCV - Vbatt - v1 - v2) / Rs;
 * v1 = (1-1/(C1*R1))*v1+(1/C1)*Ib;
 * v2 = (1-1/(C2*R2))*v2+(1/C2)*Ib;
 * v3 = v3-(1/Csoc)*Ib;
 */
/* factor is for enlarge v1 and v2
 * eg: orignal v1= 0.001736 V
 * v1 with factor will be 0.001736 *(1024* 1024) =1820
 */
static struct timespec  old, new;
static struct timespec  delta;
unsigned long long old_ns, new_ns, delta_ns;

static int fg_active_mode(struct sec_fg_info *info)
{
	int data = 0;
	int ret, ib, tmp, delta_ms = 0;
	int ocv, vbat;
	int temp, ib_ma;
	int *ocv_table = ocv_dischg;
	int chg_status;
	int PWR_Rdy_status;

	PWR_Rdy_status = get_power_status(info);
	dev_dbg(info->dev, "%s ------------->PWR_Rdy_status = %d \n", __func__,PWR_Rdy_status);

	fg_regmap_read(FG_REGMAP_GPADC, 0x38, &data);
	dev_dbg(info->dev, "%s, VBAT_SLP_AVG: 0x%x \n", __func__, data);

	mutex_lock(&info->lock);
	new_ns = sched_clock();
	delta_ns = new_ns - old_ns;
	delta_ms = delta_ns >> 20;
	old_ns = new_ns;
	t1_start = t1_start + delta_ms;
	dev_dbg(info->dev, "t1_start: %d \n", t1_start);
	mutex_unlock(&info->lock);

	ret = get_batt_vol(info, &vbat, 1);
	if (ret)
		return -EINVAL;

	/* get_batt_status(info); */
	chg_status = info->bat_params.status;

	if ((chg_status == POWER_SUPPLY_STATUS_DISCHARGING) || (chg_status == POWER_SUPPLY_STATUS_UNKNOWN) ||
	    (chg_status == POWER_SUPPLY_STATUS_CHARGING))
		dev_dbg(info->dev, "%s, delta_ms: %d, vbat: %d, v3: %d\n",
			__func__, delta_ms, vbat, v3);

	vbat_mv = vbat;

	if (v3 > 100)
		v3 = 100;
	else if (v3 < 0)
		v3 = 0;

	/* get OCV from SOC-OCV curves */
	if ((PWR_Rdy_status == POWER_SUPPLY_PWR_RDY_FALSE) ||
	    (chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	    (chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: dis-chging ...\n", __func__);
	} else if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		ocv_table = ocv_chg;
		dev_dbg(info->dev, "%s, chg_status: chging ...\n", __func__);
	} else if (chg_status == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(info->dev, "%s, chg_status: full\n", __func__);
		/* chg done, keep v3 as 100 and no need calculation of v3 */
		goto out;
	} else {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: %d\n", __func__, chg_status);
	}

	ocv = ocv_table[v3];
	dev_dbg(info->dev, "%s, ocv: %d , v1:%d, v2:%d, v3:%d \n",
		__func__, ocv, v1, v2, v3);

	/* (mV/m-Ohm) = A , so, the ib current unit is A with factor */
	//ib = (ocv - vbat - v1 - v2) * factor/ r_s;
	ib = (ocv - vbat) * factor - (v1 + v2) * 1000;
	dev_dbg(info->dev, "%s, voltage with factor: %d\n", __func__, ib);
	ib /= r_s;
	dev_dbg(info->dev, "%s, ib with factor: %d\n", __func__, ib);

	get_batt_temp(info);
	temp = info->bat_params.temp;

	dev_dbg(info->dev, "%s bat temperature : %d \n", __func__, temp);

	ib_ma = ib >> 10;
	calc_resistor(info, temp, ib_ma);
	//r1 = r2 = r_s = rtot / 3;
	/* added by M.C. to account for Rsense */
	//r1 = r2 = r_s = (rtot + 10) / 3;
	if (!!rtot)
		r1 = r2 = rtot / 3;
	//r_off = r_off + deltaR;// original, adaptive

	if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (v3 < 2)
			r_off = 240;
		else if (v3 <= 60 && v3 >= 6)
			r_off = r_off_initial-r_off_correction;
		else
			r_off = r_off_initial;
	}

	if ((chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	   (chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		if (v3 <= 60 && v3 >= 6)
			r_off = r_off_initial - r_off_correction;
		else
			r_off = r_off_initial;
	}

	dev_dbg(info->dev, "%s, roff: %d, deltaR: %d \n",
		__func__, r_off, deltaR);

	if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		r_s = (rtot / 3) + r_off;
	} else	{
		r_s = (rtot / 3) + r_off;
	}

	dev_dbg(info->dev, "%s, new r1=r2: %d \n", __func__, r1);
	dev_dbg(info->dev, "%s, new r_s: %d \n", __func__, r_s);
	deltaR = 0;

	tmp = ((c1 * r1 - delta_ms)/r1) * v1 / c1;
	dev_dbg(info->dev, "%s, v1-1: %d \n", __func__, tmp);
	v1 = tmp + ib/c1 * delta_ms/1000;
	dev_dbg(info->dev, "%s, v1 with factor: %d \n", __func__, v1);

	tmp = ((c2 * r2 - delta_ms)/r2) * v2 / c2;
	dev_dbg(info->dev, "%s, v2-1: %d \n", __func__, tmp);
	v2 = tmp + ib/c2 * delta_ms/1000;
	dev_dbg(info->dev, "%s, v2: with factor %d \n", __func__, v2);

	tmp = (ib / c_soc) * delta_ms / 1000;
	/* we change x3 from 0~1 to 0~100 , so mutiply 100 here */
	tmp = tmp * 100 * factor2;
	dev_dbg(info->dev, "%s, v3: tmp1: %d \n", __func__, tmp);
	/* remove  factor */
	tmp >>=  20;
	dev_dbg(info->dev, "%s, v3: tmp2: %d v3_res: %d \n",
		__func__, tmp, v3_res);
	v3_res = v3_res - tmp;
	dev_dbg(info->dev, "%s, v3: v3_res: %d \n", __func__, v3_res);

	if (v3_res > 100000) {
		dev_dbg(info->dev, "%s, Over Charge Protection, v3_res: %d \n", __func__, v3_res);
		v3_res = 100000;
	}

	v3 = (v3_res + factor2/2) / factor2;  //round

	if ((chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	   (chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		if (temp < -10) {
			dev_dbg(info->dev,
				"temperature for fg is lower than -10C\n");
			v3 = G_Tm10 * v3 / 10 + Off_Tm10;
		} else if (temp < 0) {
			dev_dbg(info->dev, "temperature for fg is lower than 0C\n");
			v3 = G_T0 * v3 + Off_T0;
		}
	}
	dev_dbg(info->dev, "%s, v3 after T correction: %d \n", __func__, v3);

	/* fg_store(info); */

	dev_dbg(info->dev, "%s, End v3: %d \n", __func__, v3);
	dev_dbg(info->dev, "%s <------------- \n", __func__);

	/*
	 * sleep cnt reading, added to solve sw reboot problem
	 * (slp_cnt is not zero after a sw reboot)
	 */
out:
	fg_store(info);

	return 0;
}

static int fg_lookup_v3(struct sec_fg_info *info)
{
	int ret, i;
	int vbat_slp, count;
	int *ocv_table = ocv_dischg;
	int chg_status;
	int PWR_Rdy_status;

	/* get_batt_status(info); */
	chg_status = info->bat_params.status;
	PWR_Rdy_status = get_power_status(info);

	if ((PWR_Rdy_status == POWER_SUPPLY_PWR_RDY_FALSE) ||
	    (chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	    (chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: dis-chging...\n", __func__);
	} else if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		ocv_table = ocv_chg;
		dev_dbg(info->dev, "%s, chg_status: chging...\n", __func__);
	} else if (chg_status == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(info->dev, "%s, chg_status: full\n", __func__);
	} else {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: %d \n", __func__, chg_status);
	}

	ret = get_batt_vol(info, &vbat_slp, 0);
	if (ret)
		return -EINVAL;

	if (vbat_slp_correction_flag == 1)  {
		dev_dbg(info->dev, "%s, vbat_slp correction, RAW vbat_slp : %d\n", __func__, vbat_slp);
		dev_dbg(info->dev, "%s, offset1: %d\n", __func__, offset1);
		dev_dbg(info->dev, "%s, offset2: %d\n", __func__, offset2);
		if (offset2 < 127) {
			vbat_slp = vbat_slp - offset2 * 5600000 / 4095000 ;
			if (offset1 > 127) {
				offset1 = offset1 - 256;
			}
			vbat_slp = vbat_slp + offset1 * 5600000 / 4095000;
		} else {
			offset2 = offset2 - 256;
			vbat_slp = vbat_slp - offset2 * 5600000 / 4095000;
			if (offset1 > 127) {
				offset1 = offset1 - 256;
			}
			vbat_slp = vbat_slp + offset1 * 5600000 / 4095000;
		}
		vbat_slp_correction_flag = 0;
	}

	if (vbat_slp < ocv_table[0]) {
		v3 = 0;
		goto out;
	}

	count = 100;
	for (i = count - 1; i >= 0; i--) {
		if (vbat_slp >= ocv_table[i]) {
			v3 = i + 1;
			break;
		}
	}
out:
	v3_res = v3 * factor2;
	dev_info(info->dev, "%s, vbat_slp: %d , v3: %d , v3_res: %d\n",
		__func__, vbat_slp, v3, v3_res);
	return 0;
}
/*
 *  From OCV curves, extract v3=SOC for a given VBATT_SLP.
 *  v1 = v2 = 0;
 *  v3 = v3;
 */
static int fg_long_slp_mode(struct sec_fg_info *info)
{

	dev_dbg(info->dev, "%s -------------> \n", __func__);
	v3_res_t2 = v3_res;
	dev_dbg(info->dev, "%s, v3_res_before: %d \n",
		__func__, v3_res);
	fg_lookup_v3(info);
	dev_dbg(info->dev, "%s, v3_res_after: %d \n",
		__func__, v3_res);

	dev_dbg(info->dev, "%s, first_long_sleep_flag_adaptive: %d\n",
		__func__, first_long_sleep_flag_adaptive);

	if (first_long_sleep_flag_adaptive == 1) {
		deltat_t2mt1 = t1_start;
		delta_soc_t1mt3 = v3_res_t1 - v3_res;
		delta_soc_t1mt2 = v3_res_t1 - v3_res_t2;
		dev_dbg(info->dev, "%s, deltat_t2mt1: %d, delta_soc_t1mt3: %d, delta_soc_t1mt2: %d\n",
			__func__, deltat_t2mt1, delta_soc_t1mt3, delta_soc_t1mt2);
		if (deltat_t2mt1 < factor2) {
			ib_adaptive = (delta_soc_t1mt3) * c_soc;
			} else {
			ib_adaptive = (delta_soc_t1mt3) * c_soc / (deltat_t2mt1/factor2);
		 }
		dev_dbg(info->dev, "%s, ib_adaptive with factor: %d \n",
			__func__, ib_adaptive);

		if ((deltat_t2mt1/1000 > 900) &&
		    (deltat_t2mt1/1000 < 7200) &&
		    (max(delta_soc_t1mt3,delta_soc_t1mt2)-min(delta_soc_t1mt3,delta_soc_t1mt2)>1000) &&
		    (v3_res_t1/1000 < 90) &&
		    (v3_res/1000 < 90) &&
		    (v3_res_t1/1000 > 30) &&
		    (v3_res/1000 > 30) &&
		    (delta_soc_t1mt3 > 0) &&
		    (delta_soc_t1mt2 > 0)) {
			term1 = adaptive_gain_x_I0/60;
			term2 = (delta_soc_t1mt2-delta_soc_t1mt3)/factor2;
			term3 = (10800-deltat_t2mt1/1000)/60;
			dev_dbg(info->dev, "%s, term1: %d , term2: %d , term3: %d\n",
				__func__, term1, term2, term3);

			deltaR = term1 * term2 * term3/ib_adaptive;
			dev_dbg(info->dev, "%s, deltaRnew : %d \n",
				__func__, deltaR);
		}

		dev_dbg(info->dev, "%s, deltaR : %d \n",
			__func__, deltaR);
	}

	v3_res_t1 = v3_res;
	t1_start = 0;
	if (first_long_sleep_flag_adaptive == 0) {
		first_long_sleep_flag_adaptive = 1;
	}
	//short_sleep_flag_adaptive = 0;
	dev_dbg(info->dev, "%s, v3_res_t1: %d, t1_start: %d, first_long_sleep_flag_adaptive: %d\n",
		__func__, v3_res_t1,t1_start,first_long_sleep_flag_adaptive);

	v1 = v2 = 0;
	fg_store(info);
	dev_dbg(info->dev, "%s <------------- \n", __func__);
	return 0;
}

/* exp(-x) function , x is enlarged by mutiply 100,
 * y is already enlarged by 10000 in the table.
 * y0=y1+(y2-y1)*(x0-x1)/(x2-x1)
 */
static int factor_exp = 10000;
static int calc_exp(struct sec_fg_info *info, int x)
{
	int y1, y2, y;
	int x1, x2;

	dev_dbg(info->dev, "%s -------------> \n", __func__);
	if(x > 500)
		return 0;

	if (x < 475) {
		x1 = (x / 25 * 25);
		x2 = (x + 25) / 25 * 25;

	y1 = exp_data[x/25];
	y2 = exp_data[(x + 25)/25];

		y = y1 + (y2 - y1) * (x - x1)/(x2 - x1);
	} else {
		x1 = x2 = y1 = y2 =0;
		y = exp_data[19];
	}

	dev_dbg(info->dev, "%s, x1: %d, y1: %d \n", __func__, x1, y1);
	dev_dbg(info->dev, "%s, x2: %d, y2: %d \n", __func__, x2, y2);
	dev_dbg(info->dev, "%s, x: %d, y: %d \n", __func__, x, y);
	dev_dbg(info->dev, "%s <------------- \n", __func__);

	return y;
}
/*
 * LUT of exp(-x) function, we need to calcolate exp(-x) with x=SLEEP_CNT/RC,
 * Since 0<SLEEP_CNT<1000 and RCmin=20 we should calculate exp function in [0-50]
 * Because exp(-5)= 0.0067 we can use following approximation:
 *	f(x)= exp(-x) = 0   (if x>5);
 *  v1 = v1 * exp (-SLEEP_CNT/(R1*C1));
 *  v2 = v2 * exp (-SLEEP_CNT/(R2*C2));
 *  v3 = v3;
 */
static int fg_short_slp_mode(struct sec_fg_info *info, int slp_cnt)
{
	int tmp;

	dev_dbg(info->dev, "%s, v1: %d, v2: %d, v3: %d, slp_cnt: %d \n",
		__func__, v1, v2, v3, slp_cnt);

	dev_dbg(info->dev, "%s -------------> \n", __func__);
	/* v1 calculation*/
	tmp = r1 * c1 / 1000;
	/* enlarge */
	tmp = slp_cnt * 100/tmp;
	v1 = v1 * calc_exp(info, tmp);
	//v1 = v1 * factor/ factor_exp;
	v1 = v1 / factor_exp;

	/* v2 calculation*/
	tmp = r2 * c2 / 1000;
	tmp = slp_cnt * 100/tmp;
	v2 = v2 * calc_exp(info, tmp);
	//v2 = v2 * factor/ factor_exp;
	v2 = v2 / factor_exp;
	dev_dbg(info->dev, "%s, v1: %d, v2: %d \n", __func__, v1, v2);

	fg_store(info);
	dev_dbg(info->dev, "%s <------------- \n", __func__);
	return 0;
}

static void read_soc(struct sec_fg_info *info)
{
	static int full_charge_count = 0;
	static int disch_count = 0;
	static int is_boot = 2;
	int chg_status;
	int PWR_Rdy_status;
	static int power_off_cnt, safe_flag, safe_power_off_cnt;

	/* get_batt_status(info); */
	chg_status = info->bat_params.status;
	PWR_Rdy_status = get_power_status(info);

	if ((PWR_Rdy_status == POWER_SUPPLY_PWR_RDY_FALSE) ||
	    (chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	    (chg_status == POWER_SUPPLY_STATUS_UNKNOWN))  {
		dev_dbg(info->dev, "%s, cap: %d, v3: %d before discharging protection \n", __func__,cap, v3);
		/* cap decrease only in dis charge case */
		//if (max(cap, v3) - min(cap, v3) > 10) {
		if (safe_flag == 1) {
			if (cap > 1) {
				safe_power_off_cnt++;
				dev_info(info->dev, "voltage is lower than %d\n", SAFE_POWER_OFF_THRESHOLD);
				if (safe_power_off_cnt >= 5) {
					cap--;
					v3 = cap;
					v3_res = v3*1000 + (v3_res%1000);
					safe_power_off_cnt = 0;
				}
			} else {
				safe_power_off_cnt++;
				dev_info(info->dev, "voltage is lower than %d\n", SAFE_POWER_OFF_THRESHOLD);
				if (safe_power_off_cnt > 30) {
					cap--;
					v3 = cap;
					v3_res = v3*1000 + (v3_res%1000);
					safe_power_off_cnt = 0;
				}
			}
		} else if (is_boot > 0) {
			/*
			 * initial value of cap is 100.
			 * If output v3 from table is much lower than this avoid long count down.
			 */
			cap = v3;
			dev_dbg(info->dev, "%s, is_boot branch, cap=v3: %d  \n", __func__,cap);
		} else if (v3 < cap) {
			if (cap - v3 > 1) {
				if (resume_flag == true) {
					if ((v3<15) || (cap-v3 > 10)) {
						cap = v3;
					}
				} else {
				disch_count++;
				dev_dbg(info->dev, "%s, cap: %d, disch_count: %d  \n", __func__,cap, disch_count);
				if (disch_count == 60)	{
					cap--;
					disch_count = 0;
					}
				}
			} else
				cap = v3;
		}
		resume_flag = false;
		/*
		 * the voltage is lower than 3.0V when discharge,
		 * just report 0%, then power off, hardcode here;
		 * it's a protection for the system when the temperature is low;
		 */
		pr_debug(".....> vbat_mv = %d, safe_flag = %d, cap = %d\n", vbat_mv, safe_flag, cap);
		if ((!is_boot) && (vbat_mv < SAFE_POWER_OFF_THRESHOLD) && (!safe_flag)) {
			cap = 1;
			safe_flag = 1;
		}

		/* vbat < POWER_OFF_THRESHOLD, decrease the cap */
		else if (vbat_mv <= POWER_OFF_THRESHOLD) {
			if (cap > 1) {
				/* protection for power off threshold */
				power_off_cnt++;
				dev_info(info->dev, "voltage is lower than %d\n", POWER_OFF_THRESHOLD);
				if (power_off_cnt >= 5) {
					cap--;
					v3 = cap;
					v3_res = v3*1000 + (v3_res%1000);
					power_off_cnt = 0;
				}
			} else {
				power_off_cnt++;
				if (power_off_cnt > 30) {
					cap--;
					v3 = cap;
					v3_res = v3*1000 + (v3_res%1000);
					power_off_cnt = 0;
				}
			}
		} else if (cap == 0)
			cap = 1;
		dev_dbg(info->dev, "%s, cap: %d, v3: %d after discharging protection \n", __func__,cap, v3);

		full_charge_count = 0;
	} else if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		dev_dbg(info->dev, "%s, cap: %d, v3: %d before charging protection \n", __func__,cap, v3);
		safe_flag = 0;

		if (v3 >= 100 && cap < 100)
			cap = 99;
		else if (v3 > cap)
			/* cap increase only in charge case */
			cap = v3;
		/* In charging case, cap should not be 100 */
		if (cap == 100)
			cap = v3;
		/*section added to reset adaptive variables and to wait for an other long sleep */
		first_long_sleep_flag_adaptive = 0;
		v3_res_t1 = v3_res;
		t1_start = 0;
		dev_dbg(info->dev, "%s, cap: %d, v3: %d after charging protection \n", __func__,cap, v3);

	} else if (chg_status == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(info->dev, "%s, cap: %d, v3: %d before full protection \n", __func__,cap, v3);

		/*
		 * workaround: the capacity is increased every 150s
		 * every cycle(MONITOR_INTERVAL),
		 * 3 psy(battery, ac, usb)are read
		 */
		if (cap < 100)   {
			full_charge_count++;
			dev_dbg(info->dev, "%s, cap: %d, full_charge_count: %d  \n", __func__,cap, full_charge_count);
			if (full_charge_count == 150)	{
				cap++;
				v3 = cap;
				v3_res = v3 * 1000;
				full_charge_count = 0;
			}
		} else if (v3 >= 100 || cap == 100) {
			cap = 100;
			v3 = 100;
			v3_res = 100000;
		}

		pr_debug("%s, line: %d, chg_status: %d \n", __func__, __LINE__, chg_status);
		/* section added to reset adaptive variables and to wait for an other long sleep */
		first_long_sleep_flag_adaptive = 0;
		v3_res_t1 = v3_res;
		t1_start = 0;
		dev_dbg(info->dev, "%s, cap: %d, v3: %d after full protection \n", __func__,cap, v3);
	} else {
		pr_debug("%s, line: %d, chg_status: %d \n", __func__, __LINE__, chg_status);
	}

	info->bat_params.cap = cap;

	if (is_boot > 0)
		is_boot--;
}

/* update battery status */
static int pm80x_bat_update_status(struct sec_fg_info *info)
{
	int chg_status;
	int cap_soc;
	static int pre_status = POWER_SUPPLY_STATUS_DISCHARGING;
	static unsigned long last_poll_time;
	struct timespec ts;
	ktime_t current_time;

	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);

	if (ts.tv_sec - last_poll_time <= 0) {
			dev_info(info->dev, "%s : current time : %lu, last time : %lu\n", 
			__func__, (unsigned long )ts.tv_sec, last_poll_time);
			return 0;
	}

	get_batt_status(info);
	chg_status = info->bat_params.status;
	if (chg_status != pre_status) {
		dev_info(info->dev,"%s: Status changed : %d\n", __func__, chg_status);
		pre_status = chg_status;
	}

	read_soc(info);
	cap_soc = info->bat_params.cap;
	dev_dbg(info->dev, "----->Android soc: %d\n", cap_soc);

	fg_active_mode(info);

	last_poll_time = ts.tv_sec;

	return 0;
}

static void pm80x_battery_work(struct work_struct *work)
{
	struct sec_fg_info *info =
		container_of(work, struct sec_fg_info,
			     monitor_work.work);
	dev_dbg(info->dev, "88pm80x fg update start\n");
	pm80x_bat_update_status(info);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);
	dev_dbg(info->dev, "88pm80x fg update end\n");
}

static void pm80x_init_battery(fuelgauge_variable_t *fuelgauge)
{
	int data = 0;
	int data1 = 0;
	int data2 = 0;
	int battery_removal_threshold = 0;
	int previous_v3 = 0;
	int slp_cnt;
	int reg_0xdd;
	int reg_0xde;
	int reg_0xdf;
	int reg_0xe0;
	int reg_0xee;
	int flag_false_pwrdown = 0;
	int rtc_reset = 0;
	int chg_status;
	int slp_cnt_boot;

	pr_info("%s: init battery start\n", __func__);

	fg_regmap_write(FG_REGMAP_BASE, 0x1f, 0x1);

	fg_regmap_read(FG_REGMAP_TEST, 0x7d, &data);
	dev_info(fuelgauge->info.dev, "%s OFFSET2, reg 0x7d: %x\n", __func__, data);
	offset2 = data;

	fg_regmap_read(FG_REGMAP_TEST, 0x74, &data);
	dev_info(fuelgauge->info.dev, "%s OFFSET1, reg 0x74: %x\n", __func__, data);
	offset1 = data;

	/* copy offset1 in offset2 */
	fg_regmap_write(FG_REGMAP_TEST, 0x7d, data);

	fg_regmap_read(FG_REGMAP_TEST, 0x7d, &data);
	dev_info(fuelgauge->info.dev, "%s OFFSET2, reg 0x7d: %x\n", __func__, data);

	fg_regmap_write(FG_REGMAP_BASE, 0x1f, 0x0);

	fg_regmap_read(FG_REGMAP_BASE, 0x00, &data);
	dev_info(fuelgauge->info.dev, "%s Chip version: %x\n", __func__, data);

	fg_regmap_read(FG_REGMAP_BASE, 0xE5, &data1);
	fg_regmap_read(FG_REGMAP_BASE, 0xE6, &data2);
	dev_info(fuelgauge->info.dev,
		"%s read fault event [0xe5]: 0x%x, [0xe6]: 0x%x\n",
		 __func__, data1, data2);

	/* for sw reboot */
	fg_regmap_read(FG_REGMAP_POWER, 0xa9, &data);
	pr_info(">>>>[0xA9] = 0x%x\n", data);
	if (data == 0x5A) {
		flag_false_pwrdown = 1;
	} else {
		/* power off */
		flag_false_pwrdown = 0;
	}
	dev_info(fuelgauge->info.dev,
		"%s flag_false_pwrdown: %d\n", __func__, flag_false_pwrdown);
	data = 0x5A;
	fg_regmap_write(FG_REGMAP_POWER, 0xA9, data);

	data = 0xFF;
	fg_regmap_write(FG_REGMAP_BASE, 0xE5, data);
	fg_regmap_read(FG_REGMAP_BASE, 0xE5, &data);
	dev_info(fuelgauge->info.dev,
		"%s after clear [0xe5]: 0x%x\n", __func__, data);

	data = 0xFF;
	fg_regmap_write(FG_REGMAP_BASE, 0xE6, data);
	fg_regmap_read(FG_REGMAP_BASE, 0xE6, &data);
	dev_info(fuelgauge->info.dev,
		"%s after clear [0xe6]: 0x%x\n", __func__, data);

	fg_regmap_read(FG_REGMAP_BASE, 0xDF, &data);
	deltaR=data;
	deltaR = (deltaR & 1 << 7) ? (deltaR | 0xFFFFFF00): (deltaR & 0x7F);
	dev_info(fuelgauge->info.dev,
		"%s read back [0xdf]: 0x%x\n", __func__, deltaR);

	/*
	 * enable VBAT, used to measure voltage
	 */
	fg_regmap_read(FG_REGMAP_GPADC, PM800_GPADC_MEAS_EN1, &data);
	data |= PM800_MEAS_EN1_VBAT;
	fg_regmap_write(FG_REGMAP_GPADC, PM800_GPADC_MEAS_EN1, data);

	/* enable GPADC3, used to detect battery */
	fg_regmap_read(FG_REGMAP_GPADC, PM800_GPADC_MEAS_EN2, &data);
	data |= PM800_MEAS_GP3_EN;
	fg_regmap_write(FG_REGMAP_GPADC, PM800_GPADC_MEAS_EN2, data);

	fg_regmap_read(FG_REGMAP_GPADC, PM800_GPADC_BIAS4, &data);
	data |= 0x0f; /* 76uA */
	fg_regmap_write(FG_REGMAP_GPADC, PM800_GPADC_BIAS4, data);

	fg_regmap_read(FG_REGMAP_GPADC, PM800_GP_BIAS_ENA1, &data);
	data |= ((1 << 0)| (1 << 4));
	fg_regmap_write(FG_REGMAP_GPADC, PM800_GP_BIAS_ENA1, data);

	/* set VBAT low threshold as 3.5V */
	fg_regmap_write(FG_REGMAP_GPADC, PM800_VBAT_LOW_TH, 0xa0);

	fg_regmap_read(FG_REGMAP_GPADC, 0x07, &data);
	dev_info(fuelgauge->info.dev, "%s, 0x07: 0x%x \n", __func__, data);

	fg_regmap_read(FG_REGMAP_GPADC, 0x08, &data);
	dev_info(fuelgauge->info.dev, "%s, 0x08: 0x%x \n", __func__, data);
	data &= 0xf0;
//	fg_regmap_write(FG_REGMAP_GPADC, 0x08, data);
	fg_regmap_read(FG_REGMAP_GPADC, 0x08, &data);
	dev_info(fuelgauge->info.dev, "%s, 0x08: 0x%x \n", __func__, data);

	/* check whether battery present */
	/*
	get_batt_present(fuelgauge);
	dev_info(fuelgauge->info.dev, "%s: >>>>>88pm80x battery present: %d\n", __func__,
		fuelgauge->info.bat_params.present);
	*/
	fg_regmap_read(FG_REGMAP_BASE, PM800_RTC_MISC7, &data);
	slp_cnt = data << 3;
	dev_dbg(fuelgauge->info.dev,"%s, MSB: data: %d 0x%x \n",
		__func__, data, data);

	dev_info(fuelgauge->info.dev,"%s, slp_cnt: %d 0x%x \n", __func__, slp_cnt, slp_cnt);

	fg_regmap_update_bits(FG_REGMAP_BASE, PM800_RTC_MISC6,
			   SLP_CNT_RD_LSB, 0);
	fg_regmap_read(FG_REGMAP_BASE, PM800_RTC_MISC6, &data);
	dev_info(fuelgauge->info.dev,"%s, 0xE8: 0x%x \n", __func__, data);

	//////////////// SLP_CNT reading /////////////////////////
	fg_regmap_read(FG_REGMAP_POWER, 0xB4, &data);
	dev_info(fuelgauge->info.dev, "%s 0xB4 at boot is %x\n", __func__,data);
	slp_cnt_boot = (data & 0xf);

	fg_regmap_read(FG_REGMAP_POWER, 0xB5, &data);
	dev_info(fuelgauge->info.dev, "%s 0xB5 at boot is %x\n", __func__,data);
	slp_cnt_boot =((data & 0xf) << 4) | slp_cnt_boot; //MSB of slp_cnt

	slp_cnt = slp_cnt_boot << 3;
	dev_info(fuelgauge->info.dev, "%s, slp_cnt at boot is: %d 0x%x \n", __func__, slp_cnt, slp_cnt);
	//////////////////////////////////////////////////////////

	memset(&old, 0, sizeof(old));
	memset(&new, 0, sizeof(new));
	memset(&delta, 0, sizeof(delta));

	fg_regmap_read(FG_REGMAP_BASE, PM800_POWER_UP_LOG, &data);
	dev_info(fuelgauge->info.dev,
		"%s, power up flag: 0x%x \n", __func__, data);
	dev_info(fuelgauge->info.dev, "%s, flag_false_pwrdown : 0x%x \n",
		__func__, flag_false_pwrdown);

	dev_info(fuelgauge->info.dev,
		"%s: before calculate: v3 = %d, v3_res = %d\n",
		__func__, v3, v3_res);

	fg_regmap_read(FG_REGMAP_BASE, 0xDD, &reg_0xdd);
	fg_regmap_read(FG_REGMAP_BASE, 0xDE, &reg_0xde);
	fg_regmap_read(FG_REGMAP_BASE, 0xDF, &reg_0xdf);
	fg_regmap_read(FG_REGMAP_BASE, 0xE0, &reg_0xe0);
	fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &reg_0xee);
	dev_info(fuelgauge->info.dev, "%s, 0xdd: 0x%x, 0xde: 0x%x, 0xdf: 0x%x, "
		"0xe0: 0x%x, 0xee : 0x%x\n", __func__,
		reg_0xdd, reg_0xde, reg_0xdf, reg_0xe0, reg_0xee);

	if ((reg_0xdd == 0) && (reg_0xde == 0) && (reg_0xdf == 0) &&
			(reg_0xe0 == 0) && (reg_0xee == 0)) {
		rtc_reset = 1;
	} else {
		rtc_reset = 0;
	}

	if (fuelgauge->pdata->check_jig_status &&
			fuelgauge->pdata->check_jig_status()) {
		dev_info(fuelgauge->info.dev,
			"%s: Bootup by JIG !!!!\n", __func__);
		fg_regmap_write(FG_REGMAP_BASE, PM800_USER_DATA5, reg_0xee | 0x80);

		fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &reg_0xee);
		sec_hal_fg_reset(fuelgauge);
		dev_info(fuelgauge->info.dev,
			"%s: USER DATA 5: 0x%x\n", __func__, reg_0xee);
		goto end;
	} else if (reg_0xee & 0x80) {
		dev_info(fuelgauge->info.dev,
			"%s: Last bootup by JIG !!!!\n", __func__);
		sec_hal_fg_reset(fuelgauge);
		fg_regmap_write(FG_REGMAP_BASE, PM800_USER_DATA5, reg_0xee & ~0x80);
		goto end;
	}

	dev_info(fuelgauge->info.dev, "%s: rtc_reset  = %d, \n", __func__, rtc_reset);

	if (flag_false_pwrdown == 1) {
		dev_info(fuelgauge->info.dev,
			"%s, flag_false_pwrdown inside if: 0x%x \n",
			__func__, flag_false_pwrdown);

		fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &data);
		dev_info(fuelgauge->info.dev, "%s read back: 0xEE: 0x%x \n", __func__, data);
		data &= 0x7F;
		dev_info(fuelgauge->info.dev, "%s, data(v3): %d \n", __func__, data);
		v3 = data;
		// read the fraction bit of v3_res
		fg_regmap_read(FG_REGMAP_BASE, 0xE0, &data);
		if (data > 0x09) {
			data = 0;
			dev_info(fuelgauge->info.dev,
				"%s decimal part of v3 set to 0 \n", __func__);
		}
		v3_res = v3 * factor2 + data * 100;
		dev_info(fuelgauge->info.dev, "%s v3_res: %d \n",
			__func__, v3_res);
		//v3_res = v3_res + 50;//added to take in account of reboot sw
		dev_info(fuelgauge->info.dev, "%s v3_res after: %d \n",
			__func__, v3_res);

		fg_regmap_read(FG_REGMAP_BASE, 0xDD, &data);
		dev_info(fuelgauge->info.dev,
			"%s read back: 0xDD: 0x%x \n", __func__, data);
		/* extract v1_mv */
		v1_mv = (data & 0x3F);
		v1_mv = (v1_mv & 1 << 5) ? (v1_mv | 0xFFFFFFC0): (v1_mv & 0x1F);

		fg_regmap_read(FG_REGMAP_BASE, 0xDE, &data);
		dev_info(fuelgauge->info.dev,
			"%s read back: 0xDE: 0x%x \n", __func__, data);
		/* extract v2_mv */
		v2_mv = data;
		v2_mv = (v2_mv & 0x3F);
		v2_mv = (v2_mv & 1 << 5) ? (v2_mv | 0xFFFFFFC0): (v2_mv & 0x1F);
		dev_info(fuelgauge->info.dev, "%s v1_mv: 0x%x , v2_mv : 0x%x\n",
			__func__, v1_mv, v2_mv);
		dev_info(fuelgauge->info.dev, "%s v1_mv: %d , v2_mv : %d\n",
			__func__, v1_mv, v2_mv);
		v1_mv *= 3;
		v2_mv *= 3;
		v1 = v1_mv << 10;
		v2 = v2_mv << 10;
		dev_info(fuelgauge->info.dev, "%s v1: %d, v2 : %d \n",
			__func__, v1, v2);
	} else if ((slp_cnt < 1000 ) && (rtc_reset == 0)) {
		fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &data);
		dev_info(fuelgauge->info.dev,
			"%s read back: 0xEE: 0x%x \n", __func__, data);
		data &= 0x7F;
		dev_info(fuelgauge->info.dev, "%s, data(v3): %d \n", __func__, data);
		previous_v3=data;

		/* Only look up ocv table to avoid big error of v3 */
		fg_lookup_v3(&fuelgauge->info);
		get_batt_status(&fuelgauge->info);
		chg_status = fuelgauge->info.bat_params.status;

		if ((chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
				(chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
			pr_info("%s: Charger Discharging\n", __func__);
			if (v3 < 5)
				battery_removal_threshold = 7;
			else if (v3 < 15)
				battery_removal_threshold = 10;
			else if (v3 < 80)
				battery_removal_threshold = 15;
			else
				battery_removal_threshold = 5;
		} else {
			pr_info("%s: Charger Charging\n", __func__);
			battery_removal_threshold = 60;
			if (v3 <= 5)
				battery_removal_threshold = 0;
		}

		/* Retain v3 only for previous 100% if threshold is smaller than 15 */
		if (previous_v3 == 100)
			battery_removal_threshold = 15;

		dev_info(fuelgauge->info.dev,
			"v3 = %d, battery_removal_threshold = %d\n",
			v3, battery_removal_threshold);

		if (max(data, v3) - min(data, v3) < battery_removal_threshold) {
			v3 = data;
			//v3_res = data * factor2 + factor2 / 2;
			// read the fraction bit of v3_res
			fg_regmap_read(FG_REGMAP_BASE, 0xE0, &data);
			dev_info(fuelgauge->info.dev,
				"%s read back: 0xE0: 0x%x \n", __func__, data);
			if (data > 0x09) {
				data = 0;
				dev_info(fuelgauge->info.dev,
					"%s decimal part of v3 set to 0 \n",
					__func__);
			}
			v3_res = v3 * factor2 + data * 100;
			dev_info(fuelgauge->info.dev, "%s v3_res: %d \n",
				__func__, v3_res);
			//v3_res = v3_res + 50;//added to take in account of reboot sw
			dev_info(fuelgauge->info.dev, "%s v3_res after: %d \n",
				__func__, v3_res);
		}
		dev_info(fuelgauge->info.dev, "%s, v3: %d, v3_res: %d \n",
			__func__, v3, v3_res);

		if (max(previous_v3, v3) - min(previous_v3, v3) < battery_removal_threshold) {

			fg_regmap_read(FG_REGMAP_BASE, 0xDD, &data);
			dev_info(fuelgauge->info.dev,
				"%s read back: 0xDD: 0x%x \n", __func__, data);
			/* extract v1_mv */
			v1_mv = (data & 0x3F);
			v1_mv = (v1_mv & 1 << 5) ? (v1_mv | 0xFFFFFFC0): (v1_mv & 0x1F);

			fg_regmap_read(FG_REGMAP_BASE, 0xDE, &data);
			dev_info(fuelgauge->info.dev,
				"%s read back: 0xDE: 0x%x \n",
				__func__, data);
			/* extract v2_mv */
			v2_mv = data;
			v2_mv = (v2_mv & 0x3F);
			v2_mv = (v2_mv & 1 << 5) ? (v2_mv | 0xFFFFFFC0): (v2_mv & 0x1F);
			dev_info(fuelgauge->info.dev,
				"%s v1_mv: 0x%x , v2_mv : 0x%x\n",
				__func__, v1_mv, v2_mv);
			dev_info(fuelgauge->info.dev, "%s v1_mv: %d , v2_mv : %d\n",
				__func__, v1_mv, v2_mv);
			v1_mv *= 3;
			v2_mv *= 3;
			v1 = v1_mv << 10;
			v2 = v2_mv << 10;
			dev_info(fuelgauge->info.dev, "%s v1: %d, v2 : %d \n",
				__func__, v1, v2);
		} else {
			v1 = 0;
			v2 = 0;
		}

		fg_short_slp_mode(&fuelgauge->info, slp_cnt);
	} else
		fg_long_slp_mode(&fuelgauge->info);

end:
	dev_info(fuelgauge->info.dev,
		"%s: after calculate: v3 = %d, v3_res = %d\n",
		__func__, v3, v3_res);

	/* Sync with the first v3 */
	cap = v3;

	get_batt_vol(&fuelgauge->info, &vbat_mv, 1);
	// Removed based on previous model's patches
	//fg_regmap_write(FG_REGMAP_GPADC, 0x38, 0x20); // AVG samples
}

bool sec_hal_fg_init(fuelgauge_variable_t *fuelgauge)
{
	mutex_init(&fuelgauge->info.lock);
	pm80x_init_battery(fuelgauge);

	fuelgauge->info.batt_gp_nr = fuelgauge->pdata->temp_adc_channel;
	fuelgauge->info.batt_vf_nr = fuelgauge->pdata->vf_adc;
	fuelgauge->info.online_gp_bias_curr = fuelgauge->pdata->online_gp_bias_curr;
	fuelgauge->info.hi_volt_online = fuelgauge->pdata->hi_volt_online;
	fuelgauge->info.lo_volt_online = fuelgauge->pdata->lo_volt_online;
	fuelgauge->info.bat_wqueue = create_singlethread_workqueue("bat-88pm800");
	if (!fuelgauge->info.bat_wqueue) {
		dev_info(fuelgauge->info.chip->dev,
			 "[%s]Failed to create bat_wqueue\n", __func__);
		return -ESRCH;
	}

	INIT_DEFERRABLE_WORK(&fuelgauge->info.monitor_work,
				     pm80x_battery_work);
	queue_delayed_work(fuelgauge->info.bat_wqueue, &fuelgauge->info.monitor_work,
			   MONITOR_INTERVAL);
	return true;
}

bool sec_hal_fg_suspend(fuelgauge_variable_t *fuelgauge)
{
	pm80x_battery_suspend(&fuelgauge->info);
	return true;
}

bool sec_hal_fg_resume(fuelgauge_variable_t *fuelgauge)
{
	pm80x_battery_resume(&fuelgauge->info);
	return true;
}

bool sec_hal_fg_shutdown(fuelgauge_variable_t *fuelgauge)
{
	cancel_delayed_work(&fuelgauge->info.monitor_work);
	flush_workqueue(fuelgauge->info.bat_wqueue);
	return true;
}

bool sec_hal_fg_fuelalert_init(fuelgauge_variable_t *fuelgauge, int soc)
{
	return true;
}

bool sec_hal_fg_is_fuelalerted(fuelgauge_variable_t *fuelgauge)
{
	return false;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

bool sec_hal_fg_full_charged(fuelgauge_variable_t *fuelgauge)
{
	return true;
}

bool sec_hal_fg_reset(fuelgauge_variable_t *fuelgauge)
{
	int ret, i, reg_0xee;
	int vbat_slp, count;
	int *ocv_table = ocv_dischg;
	int chg_status;
	int PWR_Rdy_status;

	fg_regmap_read(FG_REGMAP_BASE, PM800_USER_DATA5, &reg_0xee);

	if (reg_0xee & 0x80) {
		get_batt_status(&fuelgauge->info);
		chg_status = fuelgauge->info.bat_params.status;
		PWR_Rdy_status = get_power_status(&fuelgauge->info);

		if ((PWR_Rdy_status == POWER_SUPPLY_PWR_RDY_FALSE) ||
			(chg_status == POWER_SUPPLY_STATUS_DISCHARGING) ||
				(chg_status == POWER_SUPPLY_STATUS_UNKNOWN)) {
			ocv_table = ocv_dischg;
		} else if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
			ocv_table = ocv_chg;
		} else if (chg_status == POWER_SUPPLY_STATUS_FULL) {
			dev_dbg(fuelgauge->info.dev, "%s, chg_status: full\n", __func__);
		} else {
			ocv_table = ocv_dischg;
		}

		ret = get_batt_vol(&fuelgauge->info, &vbat_slp, 1);
		if (ret)
			return -EINVAL;

		if (vbat_slp < ocv_table[0]) {
			v3 = 0;
			goto out;
		}

		count = 100;
		for (i = count - 1; i >= 0; i--) {
			if (vbat_slp >= ocv_table[i]) {
				v3 = i + 1;
				break;
			}
		}
out:
		v3_res = v3 * factor2;
		cap = v3;
		dev_info(fuelgauge->info.dev, "%s, vbat_slp: %d , v3: %d , v3_res: %d\n",
				__func__, vbat_slp, v3, v3_res);
	}

	return true;
}

bool sec_hal_fg_get_property(fuelgauge_variable_t *fuelgauge,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		get_batt_present(fuelgauge);
		val->intval = (fuelgauge->info.bat_params).present;
		printk(KERN_INFO"%s:battery is %s\n", __func__,
			((fuelgauge->info.bat_params).present)?"present":"not present");
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = vbat_mv;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		read_soc(&fuelgauge->info);
		val->intval = (fuelgauge->info.bat_params).cap;
		/* report raw capacity: max = 1000 */
		val->intval *= 10;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		get_batt_temp(&fuelgauge->info);
		val->intval = (fuelgauge->info.bat_params).temp * 10;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = 250;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if ((fuelgauge->info.bat_params).temp >= 450)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if ((fuelgauge->info.bat_params).temp <= -100)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if ((fuelgauge->info.bat_params).temp <= -500)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;

	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(fuelgauge_variable_t *fuelgauge,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		break;
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	int i = 0;

	switch (offset) {
	case FG_DATA:
	case FG_REGS:
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	int ret = 0;

	switch (offset) {
	case FG_REG:
	case FG_DATA:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
static int  __attribute__((unused)) pm80x_battery_remove(struct platform_device *pdev)
{
	struct sec_fg_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->monitor_work);
	flush_workqueue(info->bat_wqueue);
	kfree(info);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int pm80x_battery_suspend(struct sec_fg_info *info)
{
	cancel_delayed_work_sync(&info->monitor_work);
	return 0;
}

static int pm80x_battery_resume(struct sec_fg_info *info)
{
	int data;
	int slp_cnt;

	mutex_lock(&info->lock);
	old_ns = sched_clock();
	printk("==== %s old_ns: %llu \n",__func__, old_ns);
	mutex_unlock(&info->lock);
	/* sleep cnt */
	/*
	fg_regmap_update_bits(FG_REGMAP_BASE, PM800_RTC_MISC6,
			   SLP_CNT_RD_LSB, 0);
	fg_regmap_read(FG_REGMAP_BASE,
		    PM800_RTC_MISC6, &data);
	dev_info(info->dev, "%s, 0xE8: 0x%x \n", __func__, data);
	*/
	fg_regmap_read(FG_REGMAP_BASE, PM800_RTC_MISC7, &data);
	dev_info(info->dev, "%s, MSB before shift: data: %d 0x%x \n", __func__, data, data);
	slp_cnt = data << 3;
	dev_info(info->dev, "%s, MSB: data: %d 0x%x \n", __func__, data, data);

	fg_regmap_read(FG_REGMAP_BASE, PM800_POWER_DOWN_LOG2, &data);

	dev_info(info->dev, "%s, POWERDOWNLOG2 after sleep: data: %d 0x%x \n", __func__, data, data);

	if(data & HYB_DONE) {
		dev_info(info->dev, "%s HYB_DONE, slp_cnt: %d \n",
			 __func__, slp_cnt);
		if(slp_cnt < 1000)  {
			dev_dbg(info->dev, "%s,  v3_res before: %d \n", __func__, v3_res);
			dev_dbg(info->dev, "%s, short_slp_counter: %d 0x%x \n",
				__func__, short_slp_counter, short_slp_counter);
			if(short_slp_counter == 6)  { // v3_res - 9 each 7 sec
				v3_res = v3_res - 9;
				v3 = (v3_res  + factor2/2)/ factor2;
				short_slp_counter = 0;
			} else {
				short_slp_counter = short_slp_counter + 1;
			}
			dev_dbg(info->dev, "%s,  v3_res after: %d \n", __func__, v3_res);
			fg_short_slp_mode(info, slp_cnt);
			fg_active_mode(info);

		} else if(slp_cnt >= 1000)
			fg_long_slp_mode(info);
	}
	resume_flag = true;
	queue_delayed_work(info->bat_wqueue, &info->monitor_work, 0);

	return 0;
}
#endif
