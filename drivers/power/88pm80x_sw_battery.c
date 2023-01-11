/*
 * Battery driver for Marvell 88PM80x PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *		Jett Zhou <jtzhou@marvell.com>
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
#include <linux/sched.h>
#include <linux/of_device.h>

#include "mrvl_battery_common.h"
#include "88pm80x_sw_fg.h"
#include "88pm80x_sw_table.h"

static int slp_fsm;
enum {
	SYS_ACTIVE = 0,
	CP_ACTIVE,
	SHRT_SLEEP,
	LONG_SLEEP,
	TBD,
};

static char *pm80x_supply_to[] = {
	"ac",
	"usb",
};

/*
 * Battery parameters from battery vendor for fuel gauge.
 * For SS aruba battery paramters, the extraction was
 * performed at T=25C and with ICHG=IDIS=0.5A, they are:
 * Rs = 0.11 Ohm + 0.01 Ohm;
 * R1 = 0.04 Ohm;
 * R2 = 0.03 Ohm;
 * C1 = 1000F
 * C2 = 10000F
 * Csoc = 5476F (Qtot = 5476C)
 */

/* we use m-Ohm by *1000 for calculation */
static int c1 = 1000;
static int c2 = 10000;
/*
 * v1, v2 is voltage, the unit of orignal formula design is "Volt",
 * since we can not do float calculation, we introduce the factor of
 * 1024 * 1024 to enlarge the resolution for LSB, the value of it is
 * "Volt with factor" in the driver.
 *
 * v1 and v2 are signded variable, normally, for charging case they
 * are negative value, for discharging case, they are positive value.
 *
 * v3 is SOC, original formular is 0~1, since we can not do float
 * calculation, for improve the resolution of it, the range value
 * is enlarged  by multiply 10000 in v3_res, but we report
 * v3 = v3_res/1000 to APP/Android level.
 */
static int v1, v2, v3, cap = 100;
static int factor = 1024 * 1024;
static int factor2 = 1000;
static int v1_mv, v2_mv, v3_res;
static int vbat_mv;
static int t1_start;
static u64 old_ns, new_ns, delta_ns;
static int factor_exp = 10000;

/*
 * LUT of exp(-x) function, we need to calcolate exp(-x) with x=SLEEP_CNT/RC,
 * Since 0<SLEEP_CNT<1000 and RCmin=20 we should calculate exp function in
 * [0-50] Because exp(-5)= 0.0067 we can use following approximation:
 *	f(x)= exp(-x) = 0   (if x>5);
 * 20 points in [0-5] with linear interpolation between points.
 *  [-0.25, 0.7788] [-0.50, 0.6065] [-0.75, 0.4724] [-1.00, 0.3679]
 *  [-1.25, 0.2865] [-1.50, 0.2231] [-1.75, 0.1738] [-2.00, 0.1353]
 *  [-2.25, 0.1054] [-2.50, 0.0821] [-2.75, 0.0639] [-3.00, 0.0498]
 *  [-3.25, 0.0388] [-3.50, 0.0302] [-3.75, 0.0235] [-4.00, 0.0183]
 *  [-4.25, 0.0143] [-4.50, 0.0111] [-4.75, 0.0087] [-5.00, 0.0067]
 */
static int exp_data[] = {
	7788, 6065, 4724, 3679,
	2865, 2231, 1738, 1353,
	1054, 821,  639,  498,
	388,  302,  235,  183,
	143,  111,  87,   67
};

/*
 * exp(-x) function , x is enlarged by mutiply 100,
 * y is already enlarged by 10000 in the table.
 * y0=y1+(y2-y1)*(x0-x1)/(x2-x1)
 */
static int calc_exp(struct pm80x_sw_bat_info *info, int x)
{
	int y1, y2, y;
	int x1, x2;

	dev_dbg(info->dev, "%s ------------->\n", __func__);
	if (x > 500)
		return 0;

	if (x < 475) {
		x1 = (x / 25 * 25);
		x2 = (x + 25) / 25 * 25;

		y1 = exp_data[x / 25];
		y2 = exp_data[(x + 25) / 25];

		y = y1 + (y2 - y1) * (x - x1)/(x2 - x1);
	} else {
		x1 = x2 = y1 = y2 = 0;
		y = exp_data[19];
	}

	dev_dbg(info->dev, "%s, x1: %d, y1: %d\n", __func__, x1, y1);
	dev_dbg(info->dev, "%s, x2: %d, y2: %d\n", __func__, x2, y2);
	dev_dbg(info->dev, "%s, x: %d, y: %d\n", __func__, x, y);
	dev_dbg(info->dev, "%s <-------------\n", __func__);

	return y;
}

void get_v3(struct pm80x_sw_bat_info *info)
{
	unsigned int data;
	/* get the stored percent(v3) from 0xee */
	regmap_read(info->chip->regmap, PM800_USER_DATA5, &data);
	data &= 0x7f;
	v3 = data;
	dev_info(info->dev, "%s: [0xee] = 0x%x, v3 = %d\n", __func__, data, v3);

	/* get fraction of stored percent(v3_res) from 0xe0 */
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_4, &data);
	v3_res = v3 * factor2 + data * 100;
	dev_info(info->dev, "%s: [0xe0] = 0x%x, v3_res = %d\n", __func__, data, v3_res);
}

void get_v1_v2(struct pm80x_sw_bat_info *info)
{
	unsigned int data;
	/* get v1_mv from 0xdd */
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_1, &data);
	v1_mv = (data & 0x3f);
	v1_mv = (v1_mv & 1 << 5) ? (v1_mv | 0xffffffc0) : (v1_mv & 0x1f);
	dev_info(info->dev, "%s: [0xdd] = 0x%x, v1_mv = %d\n", __func__, data, v1_mv);

	/* get v2_mv from 0xde */
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_2, &data);
	v2_mv = data;
	v2_mv = (v2_mv & 0x3f);
	v2_mv = (v2_mv & 1 << 5) ? (v2_mv | 0xffffffc0) : (v2_mv & 0x1f);
	dev_info(info->dev, "%s: [0xde] = 0x%x, v2_mv = %d\n", __func__, data, v2_mv);

	/* recalculate */
	v1_mv *= 3;
	v2_mv *= 3;
	v1 = v1_mv << 10;
	v2 = v2_mv << 10;
	dev_info(info->dev, "%s: v1 = %d, v2 = %d\n", __func__, v1, v2);
}

/* these two registes must not be used in other routines */
#define PM800_SLPCNT_BK_LSB	(0xb4)
#define PM800_SLPCNT_BK_MSB	(0xb5)
int get_slp_cnt(struct pm80x_sw_bat_info *info)
{
	unsigned int data;
	int ret;
	static int times;
	u8 buf[2];

	if (!info || !info->chip) {
		pr_err("battery device info is NULL.\n");
		return 0;
	}

	/* 88pm860 A0 case */
	switch (info->chip->type) {
	case CHIP_PM86X:
		if (info->chip->chip_id == CHIP_PM86X_ID_A0) {
			regmap_bulk_read(info->chip->regmap, PM860_A0_SLP_CNT1, buf, 2);
			ret = (buf[0] | ((buf[1] & 0x3) << 8));
			dev_info(info->dev, "sleep_counter = %d, 0x%x\n", ret, ret);
			/* clear sleep counter */
			regmap_update_bits(info->chip->regmap, PM860_A0_SLP_CNT2,
					   PM860_A0_SLP_CNT_RST, PM860_A0_SLP_CNT_RST);

			return ret;
		}
		/*
		 * only 88pm860 A0 chip has this change,
		 * so no need _else_ branch
		 */
		break;
	default:
		break;
	}
	/*
	 * the first time when system boots up
	 * we need to get information from sleep_counter backup registers
	 */
	if (!times) {
		/* pay attention that this registers are in power page */
		regmap_read(info->chip->subchip->regmap_power, PM800_SLPCNT_BK_LSB, &data);
		dev_info(info->dev, "boot up: power_page[0xb4]= 0x%x\n", data);
		ret = data & 0xf;

		regmap_read(info->chip->subchip->regmap_power, PM800_SLPCNT_BK_MSB, &data);
		dev_info(info->dev, "boot up: power_page[0xb5]= 0x%x\n", data);
		ret |= (data & 0xf) << 4;

		/* we only care about slp_cnt[11:4] */
		ret <<= 3;

		times = -1;
		dev_info(info->dev, "boot up: sleep_counter = %d, 0x%x\n", ret, ret);
		return ret;
	}
	/* get msb of sleep_counter */
	regmap_read(info->chip->regmap, PM800_RTC_MISC7, &data);
	ret = data << 3;
	dev_info(info->dev, "%s, msb[0xe9] = %d, 0x%x\n", __func__, data, data);

	dev_info(info->dev, "sleep_counter = %d, 0x%x\n", ret, ret);

	return ret;
}

bool is_total_powerdown(struct pm80x_sw_bat_info *info)
{
	unsigned int data1, data2, data3, data4;
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_1, &data1);
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_2, &data2);
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_3, &data3);
	regmap_read(info->chip->regmap, PM800_RTC_EXPIRE2_4, &data4);
	if (data1 + data2 + data3 + data4)
		return false;
	else {
		dev_info(info->dev, "system was totally powered down\n");
		return true;
	}
}

bool is_reboot(struct pm80x_sw_bat_info *info)
{
	int data;

	if (!info)
		return false;
	/* dump power down reason */
	dev_info(info->dev, "before->[0xe5]: 0x%x, [0xe6]: 0x%x\n",
			info->chip->powerdown1, info->chip->powerdown2);

	/*
	 * - get power down reason via [0xa9] in power page:
	 *   if the value is 0x5a, reboot, return true;
	 *   else return false;
	 */
	regmap_read(info->chip->subchip->regmap_power, 0xa9, &data);
	dev_info(info->dev, "%s: [0xa9] = 0x%x\n", __func__, data);
	if (data == 0x5a) {
		/* reboot */
		dev_info(info->dev, "system boots up from reboot.\n");
		/* reset [0xa9] in power page */
		regmap_write(info->chip->subchip->regmap_power, 0xa9, 0x5a);
		return true;
	} else {
		/* power off */
		dev_info(info->dev, "system boots up from power on.\n");
		/* reset [0xa9] in power page */
		regmap_write(info->chip->subchip->regmap_power, 0xa9, 0x5a);
		return false;
	}
}

/*
 * temp: current temperature;
 * lo_temp: the lowest temperature;
 * hi_temp: the highest temperature;
 * ib: current;
 * ib_table: current table;
 * r_table: resistor table;
 * the step between the temperatures is _15_
 */
int lookup_rtot(int temp, int temp_size, int lo_temp,
		int ib, const int *ib_table, int ib_size,
		int is_charging)
{
	static int rtot;
	int i, j, p_num = 4;
	int temp_step = 15;
	int x1, y1, x2, y2;
	const int *(*r_table)[temp_size];

	if (is_charging)
		r_table = chg_rtot;
	else
		r_table = dis_chg_rtot;

	for (i = 0; i < ib_size; i++) {
		if (ib <= ib_table[i])
			break;
	}
	if (i == 0 || i == ib_size) {
		p_num = 2;
		if (i == ib_size)
			i--;
	}
	for (j = 0; j < temp_size; j++) {
		if (temp <= lo_temp + j * temp_step)
			break;
	}
	if (j == 0 || j == temp_size) {
		if (j == temp_size)
			j--;
		if (p_num == 2) {
			/* 1 point */
			p_num = 1;
			if (v3 == 100)
				rtot = *(r_table[i][j] + 99);
			else
				rtot = *(r_table[i][j] + v3);
		} else {
			/* 2 point case , between two ib */
			p_num = 2;
			if (v3 == 100)
				y1 = *(r_table[i-1][j] + 99);
			else
				y1 = *(r_table[i-1][j] + v3);
			x1 = ib_table[i-1];
			y2 = *(r_table[i][j] + v3);
			x2 = ib_table[i];
			rtot = y1 + (y2 - y1) * (ib - x1)/(x2-x1);
		}
		pr_debug("1 p_num: %d i: %d j: %d\n", p_num, i, j);
	} else if (p_num == 2) {
		/* 2 point case, between two temp */
		pr_debug("2 p_num: %d i: %d j: %d\n", p_num, i, j);
		if (v3 == 100)
			y1 = *(r_table[i][j-1] + 99);
		else
			y1 = *(r_table[i][j-1] + v3);

		x1 = (j - 1) * temp_step + lo_temp;
		if (v3 == 100)
			y2 = *(r_table[i][j] + 99);
		else
			y2 = *(r_table[i][j] + v3);
		x2 = j * temp_step + lo_temp;
		rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);

		pr_debug("x1: %d, y1: %d, x2: %d, y2: %d\n", x1, y1, x2, y2);
	}
	if (p_num == 4) {
		int rtot1, rtot2;

		pr_debug("3 p_num: %d i: %d j: %d\n", p_num, i, j);
		/* step1, interpolate two ib in low temp */
		if (v3 == 100)
			y1 = *(r_table[i-1][j-1] + 99);
		else
			y1 = *(r_table[i-1][j-1] + v3);
		x1 = ib_table[i-1];

		if (v3 == 100)
			y2 = *(r_table[i][j-1] + 99);
		else
			y2 = *(r_table[i][j-1] + v3);
		x2 = ib_table[i];
		pr_debug("step1 x1: %d , y1, %d , x2 : %d y2: %d\n", x1, y1, x2, y2);

		rtot1 = y1 + (y2 - y1) * (ib - x1) / (x2-x1);

		/* step2, interpolate two ib in high temp */
		if (v3 == 100)
			y1 = *(r_table[i-1][j] + 99);
		else
			y1 = *(r_table[i-1][j] + v3);
		x1 = ib_table[i-1];

		if (v3 == 100)
			y2 = *(r_table[i][j] + 99);
		else
			y2 = *(r_table[i][j] + v3);
		x2 = ib_table[i];
		pr_debug("step2 x1: %d , y1, %d , x2 : %d y2: %d\n", x1, y1, x2, y2);
		rtot2 = y1 + (y2 - y1) * (ib - x1) / (x2-x1);

		/* step3 */
		y1 = rtot1;
		x1 = (j - 1) * temp_step + lo_temp;
		y2 = rtot2;
		x2 = j * temp_step + lo_temp;
		rtot = y1 + (y2 - y1) * (temp - x1) / (x2 - x1);
		pr_debug("step3: rtot : %d, rtot1: %d, rtot2: %d\n", rtot, rtot1, rtot2);
	}
	return rtot;
}

static u64 get_interval(struct pm80x_sw_bat_info *info)
{
	struct timespec ts;
	u64 ns;

	if (!info) {
		pr_info("%s: info is NULL!\n", __func__);
		return 0;
	}

	read_persistent_clock(&ts);
	if (!ts.tv_sec && !ts.tv_nsec) {
		ts = ns_to_timespec(local_clock());
		monotonic_to_bootbased(&ts);
	}
	ns = timespec_to_ns(&ts);
	dev_dbg(info->dev, "%s interval = %llu\n", __func__, ns);

	return ns;
}

static void get_batt_status(struct pm80x_sw_bat_info *info)
{
	union power_supply_propval value;

	int i;
	for (i = 0; i < info->bat_psy.num_supplicants; i++) {
		psy_do_property(info->bat_psy.supplied_to[i], get,
				POWER_SUPPLY_PROP_ONLINE, value);
		/* charger is online */
		if (value.intval != -EINVAL) {
			psy_do_property(info->bat_psy.supplied_to[i], get,
					POWER_SUPPLY_PROP_STATUS, value);
			break;

		}
	}
	if (i == info->bat_psy.num_supplicants)
		value.intval = POWER_SUPPLY_STATUS_UNKNOWN;

	dev_dbg(info->dev, "%s: fuelgauge status = %d\n", __func__, value.intval);
	info->bat_data.status = value.intval;
}

static void get_batt_present(struct pm80x_sw_bat_info *info, int gp_id)
{
	int volt;

	if (!info->use_ntc) {
		/* battery is always present */
		info->bat_data.present = 1;
		return;
	}
	/*
	 * for example, on Goldenve, the battery NTC resistor is 2.4Kohm +-6%
	 * so, with the bias current = 76uA,
	 * if the GPADC0 voltage is in [171.456mV, 193.344mV],
	 * we think battery is present when the voltage is in [140mV, 220mV]
	 * for redundancy, else not;
	 */
	volt = get_gpadc_bias_volt(info->chip, gp_id, info->bat_data.online_gp_bias_curr);
	dev_info(info->dev, "%s: VF voltage = %dmV\n", __func__, volt);

	if ((volt <= info->bat_data.hi_volt_online) &&
	    (volt >= info->bat_data.lo_volt_online))
		info->bat_data.present = 1;
	else
		info->bat_data.present = 0;
}

/*
 * Calculate the rtot according to different ib and temp info, we think about
 * ib is y coordinate, temp is x coordinate to do interpolation.
 * eg. dischg case, ib are 300/500/700/1000/1500mA, temp are -5/10/25/40C.
 * We divide them into 3 kind of case to interpolate, they are 1/2/4 point
 * case, the refered point is top-right.
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
static int calc_resistor(struct pm80x_sw_bat_info *info, int temp, int ib)
{
	int rtot;

	get_batt_status(info);
	dev_dbg(info->dev, "-> %s ib : %d temp: %d\n", __func__, ib, temp);

	switch (info->bat_data.status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		rtot = lookup_rtot(temp, 4, -5, ib, chg_ib, 5, 1);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		rtot = info->bat_data.r_tot;
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		rtot = lookup_rtot(temp, 5, -20, ib, dischg_ib, 6, 0);
		break;
	}
	dev_dbg(info->dev, "<-%s r_tot = %d\n", __func__, rtot);
	return rtot;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int get_batt_vol(struct pm80x_sw_bat_info *info, int *data, int active)
{
	int ret;
	unsigned char buf[2];
	if (!data)
		return -EINVAL;

	if (active) {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_AVG, buf, 2);
		if (ret < 0)
			return ret;
	} else {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_SLP, buf, 2);
		if (ret < 0)
			return ret;
	}

	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	*data = ((*data & 0xfff) * 7 * 100) >> 9;
	/*
	 * Calibrate vbat measurement only for un-trimed PMIC
	 * VBATmeas = VBATreal * gain + offset, so
	 * VBATreal = (VBATmeas - offset)/ gain;
	 * According to our test of vbat_sleep in bootup, the calculated
	 * gain as below:
	 * For Aruba 0.1, offset is -13.4ma, gain is 1.008.
	 * For Aruba 0.2, offset = -0.0087V and gain=1.007.
	 */
	/* data = (*data + 9) * 1000/1007; */
	return 0;
}

static void get_batt_temp(struct pm80x_sw_bat_info *info, int gp_id)
{
	int temp, tbat, volt, i;
	unsigned int bias_current[5] = {31, 61, 16, 11, 6};

	/*
	 *1) set bias as 31uA firstly for room temperature,
	 *2) check the voltage whether it's in [0.3V, 1.25V];
	 *   if yes, tbat = tbat/31; break;
	 *   else
	 *   set bias as 61uA/16uA/11uA/6uA...
	 *   for lower or higher temperature;
	 */
	for (i = 0; i < 5; i++) {
		volt = get_gpadc_bias_volt(info->chip, gp_id, bias_current[i]);
		if ((volt > info->bat_data.lo_volt_temp) && (volt < info->bat_data.hi_volt_temp)) {
			volt *= 10000;
			tbat = volt / bias_current[i];
			break;
		}
	}

	/* report the fake value 25C */
	if (i == 5) {
		info->bat_data.temp = 25;
		dev_dbg(info->dev, "Fake raw temp is 25C\n");
		return;
	}
	dev_dbg(info->dev, "%s: tbat = %dKohm, i = %d\n", __func__, tbat, i);

	/* [-25 ~ 60] */
	for (i = 0; i < 86; i++) {
		if (tbat >= temperature_table[i]) {
			temp = -25 + i;
			break;
		}
	}

	/* max temperature */
	if (i == 86)
		temp = 60;
	dev_dbg(info->dev, "raw temperature is %dC\n", temp);
	info->bat_data.temp = temp;
}

static void fg_store(struct pm80x_sw_bat_info *info)
{
	int data;

	int delta_r = info->bat_data.r_off - info->bat_data.r_init_off;
	if (delta_r >= 0)
		delta_r = min(delta_r, 127);
	else
		delta_r = max(delta_r, -128);

	/* clamp the battery capacity in [0, 100] */
	if (v3 > 100)
		v3 = 100;
	if (v3 < 0)
		v3 = 0;

	/* only care about the value in [0, 100] */
	v3 &= 0x7f;
	regmap_write(info->chip->regmap, PM800_USER_DATA5, v3);

	/* v1 is Volt with factor(2^20) */
	v1_mv = v1 >> 10;
	/* v1_mv resolution is 3mV */
	v1_mv /= 3;
	v1_mv = (v1_mv >= 0) ? min(v1_mv, 31) : max(v1_mv, -32);
	v1_mv &= 0x3f;

	/* v2 is Volt with factor(2^20) */
	v2_mv = v2 >> 10;
	v2_mv /= 3;
	v2_mv = (v2_mv >= 0) ? min(v2_mv, 31) : max(v2_mv, -32);
	v2_mv &= 0x3f;

	regmap_write(info->chip->regmap, PM800_RTC_EXPIRE2_1, v1_mv); /* 0xdd */
	regmap_write(info->chip->regmap, PM800_RTC_EXPIRE2_2, v2_mv); /* 0xde */
	regmap_write(info->chip->regmap, PM800_RTC_EXPIRE2_3, delta_r); /* 0xdf */

	data = v3_res % factor2 / 100;
	regmap_write(info->chip->regmap, PM800_RTC_EXPIRE2_4, data); /* 0xe0 */

	dev_dbg(info->dev, "%s delta_r: %d\n", __func__,
		(delta_r & 1 << 7) ? delta_r | 0xffffff00 : delta_r & 0x7f);
	dev_dbg(info->dev, "%s v1_mv: 0x%x , v2_mv : 0x%x\n", __func__, v1_mv, v2_mv);
	dev_dbg(info->dev, "%s v1_mv: %d\n",
		__func__, (v1_mv & 1 << 5) ? v1_mv | 0xffffffc0 : v1_mv & 0x1f);
	dev_dbg(info->dev, "%s v2_mv: %d\n",
		__func__, (v2_mv & 1 << 5) ? v2_mv | 0xffffffc0 : v2_mv & 0x1f);
}

/*
 * From OCV curves, extract OCV for a given v3 (SOC).
 * Ib= (OCV - Vbatt - v1 - v2) / Rs;
 * v1 = (1-1/(C1*R1))*v1+(1/C1)*Ib;
 * v2 = (1-1/(C2*R2))*v2+(1/C2)*Ib;
 * v3 = v3-(1/Csoc)*Ib;
 * Factor is for enlarge v1 and v2
 * eg: orignal v1= 0.001736 V
 * v1 with factor will be 0.001736 *(1024* 1024) =1820
 */
static int fg_active_mode(struct pm80x_sw_bat_info *info)
{
	int ret, ib, tmp = 0, delta_ms = 0;
	int ocv, vbat, ib_ma;
	const int *ocv_table = ocv_dischg;

	dev_dbg(info->dev, "%s --->\n", __func__);
	mutex_lock(&info->lock);
	new_ns = get_interval(info);
	delta_ns = new_ns - old_ns;
	delta_ms = delta_ns >> 20;
	old_ns = new_ns;
	t1_start = t1_start + delta_ms;
	dev_dbg(info->dev, "t1_start: %d\n", t1_start);
	mutex_unlock(&info->lock);

	ret = get_batt_vol(info, &vbat, 1);
	if (ret)
		return -EINVAL;

	get_batt_status(info);
	dev_dbg(info->dev, "%s, delta_ms: %d, vbat: %d, v3: %d\n",
		__func__, delta_ms, vbat, v3);

	vbat_mv = vbat;

	/* clamp the battery capacity in [0, 100] */
	if (v3 > 100)
		v3 = 100;
	if (v3 < 0)
		v3 = 0;

	/* get OCV from SOC-OCV curves */
	switch (info->bat_data.status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		ocv_table = ocv_chg;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		v3_res = 100000;
		v3     = 100;
		goto out;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		ocv_table = ocv_dischg;
		break;
	}

	ocv = ocv_table[v3];
	dev_dbg(info->dev, "%s, ocv: %d , v1:%d, v2:%d, v3:%d\n",
			__func__, ocv, v1, v2, v3);

	/* (mV/m-Ohm) = A , so, the ib current unit is A with factor */
	/* ib = (ocv - vbat - v1 - v2) * factor/ r_s; */
	ib = (ocv - vbat) * factor - (v1 + v2) * 1000;
	dev_dbg(info->dev, "%s, voltage with factor : %d\n", __func__, ib);
	ib /= info->bat_data.rs;
	dev_dbg(info->dev, "%s, ib with factor : %d\n", __func__, ib);

	get_batt_temp(info, info->bat_data.temp_gp_id);
	dev_dbg(info->dev, "%s bat temp = %d\n", __func__, info->bat_data.temp);

	ib_ma = ib >> 10;
	info->bat_data.r_tot = calc_resistor(info, info->bat_data.temp, ib_ma);
	if (info->bat_data.r_tot != 0)
		info->bat_data.r1 = info->bat_data.r2 = info->bat_data.r_tot / 3;

#if 0
	/* change according to board design */
	if (info->bat_data.status == POWER_SUPPLY_STATUS_CHARGING) {
		if (temp > 10) {
			if (v3 < 6)
				r_off = 240;
			else if (v3 < 60)
				r_off = r_off_initial - 20;
			else
				r_off = r_off_initial - 75;
		} else {
			if (v3 < 6)
				r_off = 240;
			else
				r_off = r_off_initial + 100;
		}
	}

	if ((info->bat_data.status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	   (info->bat_data.status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		if (v3 <= 60 && v3 >= 10)
			r_off = 15;
		else
			r_off = r_off_initial;
	}

	dev_dbg(info->dev, "%s, roff: %d\n", __func__, r_off);
#endif
	info->bat_data.rs = (info->bat_data.r_tot / 3) + info->bat_data.r_off;
	dev_dbg(info->dev, "%s, new r1=r2: %d\n", __func__, info->bat_data.r1);
	dev_dbg(info->dev, "%s, new r_s: %d\n", __func__, info->bat_data.rs);
	dev_info(info->dev, "%s, slp_fsm = %d\n", __func__, slp_fsm);

	switch (slp_fsm) {
	case SYS_ACTIVE:
		/* ------------------normal case----------------------- */
	case SHRT_SLEEP:
		/* ------------------sleep_cnt < SLP_TH --------------- */

		/* v1 = (1-Ts/(C1*R1))*v1+(Ts/C1)*Ib, the v1 unit is V with factor */
		/* tmp = ((c1 * info->bat_data.r1 - delta_ms)* v1/(c1 * info->bat_data.r1)); */
		tmp = ((c1 * info->bat_data.r1 - delta_ms) / info->bat_data.r1) * v1 / c1;
		dev_dbg(info->dev, "%s, v1-1: %d\n", __func__, tmp);
		v1 = tmp + ib/c1 * delta_ms/1000;
		dev_dbg(info->dev, "%s, v1 with factor: %d\n", __func__, v1);

		/* v2 = (1-Ts/(C2*R2))*v2+(Ts/C2)*Ib, the v2 unit is V with factor */
		/* tmp = ((c2 * info->bat_data.r2 - delta_ms)* v2/(c2 * info->bat_data.r2)); */
		tmp = ((c2 * info->bat_data.r2 - delta_ms) / info->bat_data.r2) * v2 / c2;
		dev_dbg(info->dev, "%s, v2-1: %d\n", __func__, tmp);
		v2 = tmp + ib / c2 * delta_ms / 1000;
		dev_dbg(info->dev, "%s, v2: with factor %d\n", __func__, v2);

		/* v3 = v3-(1/Csoc)*Ib , v3 is SOC */
		/* tmp = ib * delta_ms /(c_soc * 1000); */
		tmp = (ib / info->bat_data.total_cap) * delta_ms / 1000;
		/* we change x3 from 0~1 to 0~100 , so mutiply 100 here */
		tmp = tmp * 100 * factor2;
		dev_dbg(info->dev, "%s, v3: tmp1: %d\n", __func__, tmp);
		/* remove  factor */
		tmp >>=  20;

		slp_fsm = SYS_ACTIVE;
		break;
	case CP_ACTIVE:
		/* ---------------ap sleep, cp active------------------- */

		/* TODO: we may needn't check the charging status */
		if (info->bat_data.status == POWER_SUPPLY_STATUS_DISCHARGING) {
			/* 45mA is estimated */
			tmp = delta_ms / 1000 * 45 / info->bat_data.total_cap;
			slp_fsm = SYS_ACTIVE;
		}
		break;
	case LONG_SLEEP:
		/* --------------------long sleep ---------------------- */

		/* this case shouldn't enter this routine  */
		dev_err(info->dev, "???active fsm is not right.\n\n");
		/* TODO: we may needn't check the charging status */
		if (info->bat_data.status == POWER_SUPPLY_STATUS_DISCHARGING)
			slp_fsm = SYS_ACTIVE;
		break;
	}

	dev_dbg(info->dev, "%s, v3: tmp2: %d v3_res: %d\n", __func__, tmp, v3_res);
	v3_res = v3_res - tmp;
	dev_dbg(info->dev, "%s, v3: v3_res: %d\n", __func__, v3_res);

	v3 = (v3_res + factor2 / 2) / factor2;

	/* protection for low temperatur, case by case */
	if ((info->bat_data.status == POWER_SUPPLY_STATUS_DISCHARGING) ||
	    (info->bat_data.status == POWER_SUPPLY_STATUS_UNKNOWN)) {
		if (info->bat_data.temp < -10) {
			dev_dbg(info->dev, "temperature is lower than -10C\n");
			v3 *= info->bat_data.times_in_ten;
			/* offset is positive number */
			v3 -= info->bat_data.offset_in_ten;
		} else if (info->bat_data.temp < 0) {
			dev_dbg(info->dev, "temperature is lower than 0C\n");
			v3 *= info->bat_data.times_in_zero;
			/* offset is positive number */
			v3 -= info->bat_data.offset_in_zero;
		}
	}
	dev_dbg(info->dev, "%s, v3 after T correction: %d\n", __func__, v3);

	/* store the result to the RTC domain */
	fg_store(info);
	dev_dbg(info->dev, "%s, End v3: %d\n", __func__, v3);
out:
	dev_dbg(info->dev, "%s <---\n", __func__);
	return 0;
}

static int fg_lookup_v3(struct pm80x_sw_bat_info *info)
{
	int ret, i;
	int vbat_slp, count;
	const int *ocv_table = ocv_dischg;

	get_batt_status(info);

	/* get OCV from SOC-OCV curves */
	switch (info->bat_data.status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		ocv_table = ocv_chg;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		return 0;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		ocv_table = ocv_dischg;
		break;
	}

	ret = get_batt_vol(info, &vbat_slp, 0);
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
	dev_dbg(info->dev, "%s, vbat_slp: %d , v3: %d , v3_res: %d\n",
			__func__, vbat_slp, v3, v3_res);
	return 0;
}
/*
 *  From OCV curves, extract v3=SOC for a given VBATT_SLP.
 *  v1 = v2 = 0;
 *  v3 = v3;
 */
static int fg_long_slp_mode(struct pm80x_sw_bat_info *info)
{
	dev_dbg(info->dev, "%s --->\n", __func__);

	dev_dbg(info->dev, "%s before lookup v3_res: %d\n", __func__, v3_res);
	fg_lookup_v3(info);
	dev_dbg(info->dev, "%s after lookup v3_res: %d\n", __func__, v3_res);

	t1_start = 0;

	v1 = v2 = 0;
	fg_store(info);
	dev_dbg(info->dev, "%s <---\n", __func__);

	return 0;
}

/*
 * LUT of exp(-x) function, we need to calcolate exp(-x) with x=SLEEP_CNT/RC,
 * Since 0<SLEEP_CNT<1000 and RCmin=20 we should calculate exp function in
 * [0-50]. Because exp(-5)= 0.0067 we can use following approximation:
 *	f(x)= exp(-x) = 0   (if x>5);
 *  v1 = v1 * exp (-SLEEP_CNT/(R1*C1));
 *  v2 = v2 * exp (-SLEEP_CNT/(R2*C2));
 *  v3 = v3;
 */
static int fg_short_slp_mode(struct pm80x_sw_bat_info *info, int slp_cnt)
{
	int tmp;
	dev_dbg(info->dev, "%s, v1: %d, v2: %d, v3: %d, slp_cnt: %d\n",
			__func__, v1, v2, v3, slp_cnt);

	dev_dbg(info->dev, "%s --->\n", __func__);
	/* v1 calculation */
	tmp = info->bat_data.r1 * c1 / 1000;
	/* enlarge by multiply 100 */
	tmp = slp_cnt * 100 / tmp;
	v1 = v1 * calc_exp(info, tmp);
	v1 = v1 / factor_exp;

	/* v2 calculation */
	tmp = info->bat_data.r2 * c2 / 1000;
	/* enlarge by multiply 100 */
	tmp = slp_cnt * 100 / tmp;
	v2 = v2 * calc_exp(info, tmp);
	v2 = v2 / factor_exp;
	dev_dbg(info->dev, "%s, v1: %d, v2: %d\n", __func__, v1, v2);

	fg_store(info);
	dev_dbg(info->dev, "%s <---\n", __func__);
	return 0;
}

static int pm80x_calc_soc(struct pm80x_sw_bat_info *info)
{
	static int safe_flag;
	static int is_boot = 2;
	static int dischg_cnt;
	static int power_off_cnt;
	static int full_charge_cnt;

	get_batt_status(info);

	switch (info->bat_data.status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		dev_dbg(info->dev, "charge-->cap: %d, v3: %d\n", cap, v3);
		if (v3 >= 100)
			cap = 99;
		else if (v3 > cap)
			cap = v3;
		if (cap == 100)
			cap = v3;
		dev_dbg(info->dev, "charge<--cap: %d, v3: %d\n", cap, v3);
		break;

	case POWER_SUPPLY_STATUS_FULL:
		dev_dbg(info->dev, "full-->cap: %d, v3: %d\n", cap, v3);
		if (cap < 100) {
			full_charge_cnt++;
			if (full_charge_cnt == info->soc_ramp_up_th) {
				cap++;
				v3 = cap;
				v3_res = v3 * 1000;
				full_charge_cnt = 0;
			}
		} else if (v3 >= 100 || cap == 100) {
			cap = 100;
			v3 = cap;
			v3_res = v3 * 1000;
		}
		dev_dbg(info->dev, "full<--cap: %d, v3: %d, full_charge_cnt: %d\n", cap, v3, full_charge_cnt);
		break;

	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		dev_dbg(info->dev, "discharge-->cap: %d, v3: %d\n", cap, v3);
		if (safe_flag == 1)
			cap = 0;
		else if (is_boot > 0) {
			cap = v3;
			dev_dbg(info->dev, "is_boot = %d, v3 = %d\n", is_boot, v3);
		} else if (v3 < cap) {
			if (cap - v3 > 1) {
				dischg_cnt++;
				if (dischg_cnt == 10) {
					cap--;
					dischg_cnt = 0;
				}
			} else
				cap = v3;
		}
		if ((!is_boot) && (!safe_flag) &&
		    (vbat_mv < info->safe_power_off_th)) {
			cap = 1;
			safe_flag = 1;
		} else if (vbat_mv <= info->power_off_th) {
			if (cap != 0) {
				power_off_cnt++;
				if ((power_off_cnt >= 5) && (cap > 0)) {
					cap--;
					power_off_cnt = 0;
				}
			}

		} else if (cap == 0)
			cap = 1;
		dev_dbg(info->dev, "discharge<--cap: %d, v3: %d\n", cap, v3);
		break;
	}

	info->bat_data.percent = cap;

	if (is_boot > 0)
		is_boot--;

	dev_dbg(info->dev, "%s, vbat_mv: %d, soc: %d\n", __func__, vbat_mv, cap);

	return 0;
}

/* Update battery status */
static void pm80x_bat_update_status(struct pm80x_sw_bat_info *info)
{
	info->bat_data.volt = vbat_mv;

	get_batt_present(info, info->bat_data.online_gp_id);
	get_batt_status(info);
	get_batt_temp(info, info->bat_data.temp_gp_id);

	if (info->bat_data.temp >= 50)
		info->bat_data.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (info->bat_data.temp <= 0)
		info->bat_data.health = POWER_SUPPLY_HEALTH_COLD;
	else
		info->bat_data.health = POWER_SUPPLY_HEALTH_GOOD;

	info->bat_data.tech = POWER_SUPPLY_TECHNOLOGY_LION;

	pm80x_calc_soc(info);
	info->bat_data.percent = cap;
}

static void pm80x_fg_work(struct work_struct *work)
{
	struct pm80x_sw_bat_info *info =
		container_of(work, struct pm80x_sw_bat_info, fg_work.work);

	fg_active_mode(info);
	queue_delayed_work(info->bat_wqueue, &info->fg_work,
			   FG_INTERVAL);
}

static void pm80x_battery_work(struct work_struct *work)
{
	struct pm80x_sw_bat_info *info =
		container_of(work, struct pm80x_sw_bat_info, monitor_work.work);

	pm80x_bat_update_status(info);

	power_supply_changed(&info->bat_psy);
	if (vbat_mv <= LOW_BAT_THRESHOLD)
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   LOW_BAT_INTERVAL);
	else
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);
}

static void pm80x_charged_work(struct work_struct *work)
{
	struct pm80x_sw_bat_info *info =
		container_of(work, struct pm80x_sw_bat_info, charged_work.work);

	pm80x_bat_update_status(info);
	power_supply_changed(&info->bat_psy);
	/* NO need to be scheduled again */
	return;
}

static irqreturn_t pm80x_bat_handler(int irq, void *data)
{
	struct pm80x_sw_bat_info *info = data;
	dev_info(info->dev, "%s is triggered!\n", __func__);
	/* report status when voltage is abnormal */
	pm80x_bat_update_status(info);

	return IRQ_HANDLED;
}

static void pm80x_init_battery(struct pm80x_sw_bat_info *info)
{
	static int data, mask;
	static int slp_cnt, rtc_rst;
	static int bat_rm_th, hi, lo;

	dev_info(info->dev, "%s: init battery start...\n", __func__);

	if (info->need_offset_trim) {
		/* need to trim VBAT offset, maybe remove it for PM820 new
		 * samples later
		 */
		regmap_write(info->chip->regmap, 0x1f, 0x1);

		regmap_read(info->chip->subchip->regmap_test, 0x7d, &data);
		dev_info(info->dev, "%s OFFSET2, reg 0x7d: %x\n",
				__func__, data);

		regmap_read(info->chip->subchip->regmap_test, 0x74, &data);
		dev_info(info->dev, "%s OFFSET1, reg 0x74: %x\n",
				__func__, data);

		/* copy offset1 in offset2 */
		regmap_write(info->chip->subchip->regmap_test, 0x7d, data);

		regmap_read(info->chip->subchip->regmap_test, 0x7d, &data);
		dev_info(info->dev, "%s OFFSET2, reg 0x7d: %x\n",
				__func__, data);

		regmap_write(info->chip->regmap, 0x1f, 0x0);
	}

	/* sanity check: dump power up reason */
	regmap_read(info->chip->regmap, PM800_POWER_UP_LOG, &data);
	dev_info(info->dev, "%s: power up flag[0x10] = 0x%x\n", __func__, data);

	/* enable VBAT and set low threshold as 3.5V */
	data = mask = PM800_MEAS_EN1_VBAT;
	regmap_update_bits(info->chip->subchip->regmap_gpadc,
			   PM800_GPADC_MEAS_EN1, mask, data);
	regmap_write(info->chip->subchip->regmap_gpadc, PM800_VBAT_LOW_TH, 0xa0);

	/* check boot up reason */
	if (!is_reboot(info)) {
		slp_cnt = get_slp_cnt(info);
		rtc_rst = is_total_powerdown(info);
		/* backup battery is not totally discharged */
		if ((slp_cnt < 1000) && (!rtc_rst)) {
			/* get the stored percent(v3) from 0xee */
			regmap_read(info->chip->regmap, PM800_USER_DATA5, &data);
			data &= 0x7f;
			dev_info(info->dev, "%s: [0xee] = 0x%x, %d\n", __func__, data, data);

			/* look up ocv table to get v3 */
			fg_lookup_v3(info);
			if (v3 < 5)
				bat_rm_th = 5;
			else if (v3 < 15)
				bat_rm_th = 10;
			else if (v3 < 60)
				bat_rm_th = 15;
			else
				bat_rm_th = 5;
			dev_info(info->dev, "v3 = %d, bat_rm_th = %d\n", v3, bat_rm_th);

			hi = max(data, v3);
			lo = min(data, v3);
			/* battery is not changed, use stored value */
			if (hi - lo < bat_rm_th) {
				dev_info(info->dev, "battery is not changed.\n");
				get_v3(info);
				get_v1_v2(info);
			} else {
				dev_info(info->dev, "new battery is plugged in.\n");
				v1 = v2 = 0;
			}
			fg_short_slp_mode(info, slp_cnt);
		} else {
			dev_info(info->dev, "battery is relaxed for power off.\n");
			fg_long_slp_mode(info);
		}
	} else {
		get_v3(info);
		get_v1_v2(info);
	}

	dev_info(info->dev, "end of %s: v3 = %d, v3_res = %d\n", __func__, v3, v3_res);
}

static void pm80x_external_power_changed(struct power_supply *psy)
{
	struct pm80x_sw_bat_info *info;

	info = container_of(psy, struct pm80x_sw_bat_info, bat_psy);
	queue_delayed_work(info->bat_wqueue, &info->charged_work, HZ);
	return;
}

static int pm80x_batt_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct pm80x_sw_bat_info *info = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		get_batt_status(info);
		val->intval = info->bat_data.status;
		if ((info->bat_data.status == POWER_SUPPLY_STATUS_FULL) &&
				(info->bat_data.percent < 100))
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = info->bat_data.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->bat_data.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* report fake capacity without battery */
		if (!info->bat_data.present)
			val->intval = 80;
		else {
			pm80x_calc_soc(info);
			val->intval = info->bat_data.percent;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = info->bat_data.tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		info->bat_data.volt = vbat_mv;
		val->intval = info->bat_data.volt;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!info->bat_data.present)
			val->intval = 240;
		else {
			get_batt_temp(info, info->bat_data.temp_gp_id);
			val->intval = info->bat_data.temp * 10;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->bat_data.health;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static enum power_supply_property pm80x_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
};

static int pm80x_bat_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm80x_sw_bat_pdata *pdata)
{
	int ret;

	pdata->use_ntc = of_property_read_bool(np, "sw-fg-use-ntc");

	pdata->need_offset_trim = of_property_read_bool(np, "need-offset-trim");

	ret = of_property_read_u32(np, "full-capacity", &pdata->capacity);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "r1-resistor", &pdata->r1);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "r2-resistor", &pdata->r2);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "rs-resistor", &pdata->rs);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "roff-resistor", &pdata->r_off);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "roff-initial-resistor",
				   &pdata->r_init_off);
	if (ret)
		return ret;
	if (pdata->use_ntc) {
		ret = of_property_read_u32(np, "online-gpadc",
					   &pdata->online_gp_id);
		if (ret)
			return ret;

		ret = of_property_read_u32(np, "online-gp-bias-curr",
					   &pdata->online_gp_bias_curr);
		if (ret)
			return ret;

		ret = of_property_read_u32(np, "hi-volt-online",
					   &pdata->hi_volt_online);
		if (ret)
			return ret;

		ret = of_property_read_u32(np, "lo-volt-online",
					   &pdata->lo_volt_online);
		if (ret)
			return ret;
	} else
		pdata->online_gp_id = -1;

	ret = of_property_read_u32(np, "temperature-gpadc", &pdata->temp_gp_id);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "hi-volt-temp", &pdata->hi_volt_temp);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "lo-volt-temp", &pdata->lo_volt_temp);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "times-in-zero-degree", &pdata->times_in_zero);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "offset-in-zero-degree", &pdata->offset_in_zero);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "times-in-ten-degree", &pdata->times_in_ten);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "offset-in-ten-degree", &pdata->offset_in_ten);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "power-off-threshold", &pdata->power_off_th);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "safe-power-off-threshold", &pdata->safe_power_off_th);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "soc-ramp-up-interval",
			&pdata->soc_ramp_up_interval);
	if (ret)
		return ret;

	return 0;
}

static int pm80x_battery_probe(struct platform_device *pdev)
{
	struct pm80x_sw_bat_info *info;
	struct pm80x_sw_bat_pdata *pdata = pdev->dev.platform_data;
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	int ret;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm80x_bat_dt_init(node, &pdev->dev, pdata);
		if (ret) {
			dev_dbg(&pdev->dev, "dt init failed!\n");
			return ret;
		}
	}

	info = devm_kzalloc(&pdev->dev,
			    sizeof(struct pm80x_sw_bat_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "request mem failed!\n");
		return -ENOMEM;
	}

	info->chip = chip;
	info->dev = &pdev->dev;
	info->use_ntc = pdata->use_ntc;

	info->bat_data.status = POWER_SUPPLY_STATUS_UNKNOWN;
	info->bat_data.total_cap = pdata->capacity * 36 / 10;
	info->bat_data.r1 = pdata->r1;
	info->bat_data.r2 = pdata->r2;
	info->bat_data.rs = pdata->rs;
	info->bat_data.r_off = pdata->r_off;
	info->bat_data.r_init_off = pdata->r_init_off;

	info->bat_data.online_gp_id = pdata->online_gp_id;
	info->bat_data.temp_gp_id = pdata->temp_gp_id;

	info->bat_data.times_in_zero = pdata->times_in_zero;
	info->bat_data.offset_in_zero = pdata->offset_in_zero;
	info->bat_data.times_in_ten = pdata->times_in_ten;
	info->bat_data.offset_in_ten = pdata->offset_in_ten;

	info->bat_data.online_gp_bias_curr = pdata->online_gp_bias_curr;
	info->bat_data.hi_volt_temp = pdata->hi_volt_temp;
	info->bat_data.lo_volt_temp = pdata->lo_volt_temp;
	info->bat_data.hi_volt_online = pdata->hi_volt_online;
	info->bat_data.lo_volt_online = pdata->lo_volt_online;

	info->safe_power_off_th = pdata->safe_power_off_th;
	info->power_off_th = pdata->power_off_th;
	info->soc_ramp_up_th = pdata->soc_ramp_up_interval / (MONITOR_INTERVAL / HZ);
	info->need_offset_trim = pdata->need_offset_trim;

	platform_set_drvdata(pdev, info);

	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		return -EINVAL;
	}

	mutex_init(&info->lock);

	pm80x_init_battery(info);

	info->bat_psy.name = "battery";
	info->bat_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	info->bat_psy.properties = pm80x_batt_props;
	info->bat_psy.num_properties = ARRAY_SIZE(pm80x_batt_props);
	info->bat_psy.get_property = pm80x_batt_get_prop;
	info->bat_psy.external_power_changed = pm80x_external_power_changed;
	info->bat_psy.supplied_to = pm80x_supply_to;
	info->bat_psy.num_supplicants = ARRAY_SIZE(pm80x_supply_to);

	ret = power_supply_register(&pdev->dev, &info->bat_psy);
	if (ret)
		return ret;

	info->bat_psy.dev->parent = &pdev->dev;
	pm80x_bat_update_status(info);

	ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
					pm80x_bat_handler,
					IRQF_ONESHOT | IRQF_NO_SUSPEND, "battery", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out;
	}

	info->bat_wqueue = create_singlethread_workqueue("bat-88pm800");
	if (!info->bat_wqueue) {
		dev_info(chip->dev,
			"[%s]Failed to create bat_wqueue\n", __func__);
		ret = -ESRCH;
		goto out;
	}

	INIT_DEFERRABLE_WORK(&info->fg_work, pm80x_fg_work);
	INIT_DEFERRABLE_WORK(&info->monitor_work, pm80x_battery_work);

	INIT_DELAYED_WORK(&info->charged_work, pm80x_charged_work);
	queue_delayed_work(info->bat_wqueue, &info->fg_work, FG_INTERVAL);
	queue_delayed_work(info->bat_wqueue,
			   &info->monitor_work, MONITOR_INTERVAL);

	device_init_wakeup(&pdev->dev, 1);

	return 0;

out:
	power_supply_unregister(&info->bat_psy);
	return ret;
}

static int pm80x_battery_remove(struct platform_device *pdev)
{
	struct pm80x_sw_bat_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->fg_work);
	cancel_delayed_work_sync(&info->monitor_work);
	cancel_delayed_work_sync(&info->charged_work);
	flush_workqueue(info->bat_wqueue);
	power_supply_unregister(&info->bat_psy);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm80x_battery_shutdown(struct platform_device *pdev)
{
	struct pm80x_sw_bat_info *info = platform_get_drvdata(pdev);

	if (!info)
		return;

	/* set 0xa9 to 0x5a to identify it's a reboot operation */
	regmap_write(info->chip->subchip->regmap_power, 0xa9, 0x5a);

	devm_free_irq(&pdev->dev, info->irq, info);

	if (vbat_mv <= LOW_BAT_THRESHOLD)
		regmap_write(info->chip->regmap, PM800_USER_DATA5, cap);

	cancel_delayed_work(&info->fg_work);
	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->charged_work);
	flush_workqueue(info->bat_wqueue);
	power_supply_unregister(&info->bat_psy);
	platform_set_drvdata(pdev, NULL);
}

#ifdef CONFIG_PM
static int pm80x_battery_suspend(struct device *dev)
{
	struct pm80x_sw_bat_info *info = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&info->fg_work);
	cancel_delayed_work_sync(&info->monitor_work);
	return pm80x_dev_suspend(dev);
}

/* let's use this value at present */
#define SLP_TH (200)
static int pm80x_battery_resume(struct device *dev)
{
	struct pm80x_sw_bat_info *info = dev_get_drvdata(dev);
	static int short_slp_cnt;
	int data, slp_cnt;

	slp_cnt = get_slp_cnt(info);

	regmap_read(info->chip->regmap, PM800_POWER_DOWN_LOG2, &data);
	if (data & HYB_DONE) {
		/* the system entered suspend */
		dev_info(info->dev, "%s: HYB_DONE, slp_cnt: %d\n", __func__, slp_cnt);
		dev_info(info->dev, "%s, ---->v3_res = %d\n", __func__, v3_res);
		dev_info(info->dev, "short_slp_cnt = 0x%x\n", short_slp_cnt);

		/* short sleep */
		if (slp_cnt < SLP_TH) {
			/* protection: (v3_res - 9) every 7 seconds */
			if (short_slp_cnt == 6)  {
				v3_res = v3_res - 9;
				v3 = (v3_res  + factor2 / 2) / factor2;
				short_slp_cnt = 0;
			} else {
				short_slp_cnt = short_slp_cnt + 1;
			}

			dev_info(info->dev, "in short sleep case.\n");
			dev_info(info->dev, "%s,  <----v3_res = %d\n", __func__, v3_res);

			fg_short_slp_mode(info, slp_cnt);
			slp_fsm = SHRT_SLEEP;
			fg_active_mode(info);
		} else {
			/* long sleep */
			slp_fsm = LONG_SLEEP;
			dev_info(info->dev, "in long sleep case.\n");
			dev_info(info->dev, "%s,  <----v3_res = %d\n", __func__, v3_res);
			fg_long_slp_mode(info);
		}
		regmap_update_bits(info->chip->regmap,
				   PM800_POWER_DOWN_LOG2, HYB_DONE, HYB_DONE);
	} else {
		/*
		 * the reason we are here is:
		 * the AP entered into suspend, while the CP wake up periodically,
		 * which is transparent to AP;
		 * then triggered by some wakeup source, here we come
		 */
		slp_fsm = CP_ACTIVE;
		dev_info(info->dev, "in ap suspend and cp sleep case.\n");
		dev_info(info->dev, "%s,  <----v3_res = %d\n", __func__, v3_res);
		fg_active_mode(info);
	}

	mutex_lock(&info->lock);
	old_ns = get_interval(info);
	mutex_unlock(&info->lock);
	dev_dbg(info->dev, "%s old_ns: %llu\n", __func__, old_ns);

	queue_delayed_work(info->bat_wqueue, &info->monitor_work, HZ);
	queue_delayed_work(info->bat_wqueue, &info->fg_work, FG_INTERVAL);

	return pm80x_dev_resume(dev);
}

static const struct dev_pm_ops pm80x_battery_pm_ops = {
	.suspend	= pm80x_battery_suspend,
	.resume		= pm80x_battery_resume,
};
#endif

static const struct of_device_id pm80x_bat_dt_match[] = {
	{ .compatible = "marvell,88pm80x-sw-battery", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm80x_bat_dt_match);

static struct platform_driver pm80x_battery_driver = {
	.driver		= {
		.name	= "88pm80x-sw-fuelgauge",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm80x_battery_pm_ops,
#endif
		.of_match_table = of_match_ptr(pm80x_bat_dt_match),
	},
	.probe		= pm80x_battery_probe,
	.remove		= pm80x_battery_remove,
	.shutdown	= &pm80x_battery_shutdown,
};

static int __init pm80x_battery_init(void)
{
	return platform_driver_register(&pm80x_battery_driver);
}
module_init(pm80x_battery_init);

static void __exit pm80x_battery_exit(void)
{
	platform_driver_unregister(&pm80x_battery_driver);
}
module_exit(pm80x_battery_exit);

MODULE_DESCRIPTION("Marvell 88PM80x Software Battery driver");
MODULE_LICENSE("GPL");
