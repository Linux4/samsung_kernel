/*
 * Battery driver for Marvell 88PM886 fuel-gauge chip
 *
 * Copyright (c) 2014 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
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
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm880.h>
#include <linux/mfd/88pm886.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/of_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/notifier.h>
#include <linux/pm.h>

#ifdef CONFIG_USB_MV_UDC
#include <linux/platform_data/mv_usb.h>
#endif

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
#define LOW_BAT_CAP			(15)

#define ROUND_SOC(_x)			(((_x) + 5) / 10 * 10)

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

	bool			use_ntc;
	bool			bat_temp_monitor_en;
	bool			use_sw_bd;
	int			gpadc_det_no;
	int			gpadc_temp_no;

	int			ocv_is_realiable;
	int			range_low_th;
	int			range_high_th;
	int			sleep_counter_th;

	int			power_off_th;
	int			safe_power_off_th;
	int			power_off_cnt_th[3]; /* low-temp, standard-temp, high-temp */
	int			alart_percent;

	int			irq_nums;
	struct pm88x_bat_irq	irqs[7];

	struct iio_channel	*chan[MAX_CHAN];
	struct temp_vs_ohm	*temp_ohm_table;
	int			temp_ohm_table_size;
	int			zero_degree_ohm;

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

static char *supply_interface[] = {
	"ac",
	"usb",
};

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

static int pm88x_get_batt_vol(struct pm88x_battery_info *info, int active);
static void init_ccnt_data(struct ccnt *ccnt_val, int value);

static void pm88x_bat_enable_irq(struct pm88x_bat_irq *irq)
{
	if (__test_and_clear_bit(0, &irq->disabled))
		enable_irq(irq->irq);
}

static void pm88x_bat_disable_irq(struct pm88x_bat_irq *irq)
{
	if (!__test_and_set_bit(0, &irq->disabled))
		disable_irq_nosync(irq->irq);

}

/* pay attention that we enlarge the capacity to 1000% */
static int clamp_soc(int soc)
{
	soc = max(0, soc);
	soc = min(1000, soc);
	return soc;
}

/*
 * - saved SoC
 * - ocv_is_realiable flag
 */
static int get_extern_data(struct pm88x_battery_info *info, int flag)
{
	u8 buf[2];
	unsigned int val;
	int ret;

	if (!info) {
		pr_err("%s: 88pm88x device info is empty!\n", __func__);
		return 0;
	}

	switch (flag) {
	case ALL_SAVED_DATA:
		ret = regmap_bulk_read(info->chip->base_regmap,
				       PM88X_RTC_SPARE5, buf, 2);
		if (ret < 0) {
			val = 0;
			break;
		}

		val = (buf[0] & 0xff) | ((buf[1] & 0xff) << 8);
		break;
	default:
		val = 0;
		dev_err(info->dev, "%s: unexpected case %d.\n", __func__, flag);
		break;
	}

	dev_dbg(info->dev, "%s: val = 0x%x\n", __func__, val);
	return val;
}
static void set_extern_data(struct pm88x_battery_info *info, int flag, int data)
{
	u8 buf[2];
	unsigned int val;
	int ret;

	if (!info) {
		pr_err("88pm88x device info is empty!\n");
		return;
	}

	switch (flag) {
	case ALL_SAVED_DATA:
		buf[0] = data & 0xff;
		ret = regmap_read(info->chip->base_regmap,
				  PM88X_RTC_SPARE6, &val);
		if (ret < 0)
			return;
		dev_dbg(info->dev, "%s: buf[0] = 0x%x\n", __func__, buf[0]);

		buf[1] = ((data >> 8) & 0xfc) | (val & 0x3);
		ret = regmap_bulk_write(info->chip->base_regmap,
					PM88X_RTC_SPARE5, buf, 2);
		if (ret < 0)
			return;
		dev_dbg(info->dev, "%s: buf[1] = 0x%x\n", __func__, buf[1]);

		break;
	default:
		dev_err(info->dev, "%s: unexpected case %d.\n", __func__, flag);
		break;
	}

	return;
}

static int pm88x_battery_write_buffer(struct pm88x_battery_info *info,
				      struct save_buffer *value)
{
	int data, tmp_soc;

	/* convert 0% as 128% */
	if (value->soc == 0)
		tmp_soc = 0x7f;
	else
		tmp_soc = value->soc;

	/* save values in RTC registers */
	data = (tmp_soc & 0x7f) | (value->ocv_is_realiable << 7);

	/* bits 0 is used for other purpose, so give up it */
	data |= (value->cycles & 0xfe) << 8;
	set_extern_data(info, ALL_SAVED_DATA, data);

	return 0;
}

static int pm88x_battery_read_buffer(struct pm88x_battery_info *info,
				     struct save_buffer *value)
{
	int data;

	/* read values from RTC registers */
	data = get_extern_data(info, ALL_SAVED_DATA);

	value->soc = data & 0x7f;
	/* check whether the capacity is 0% */
	if (value->soc == 0x7f)
		value->soc = 0;

	value->ocv_is_realiable = (data & 0x80) >> 7;
	value->cycles = (data >> 8) & 0xfe;
	if (value->cycles < 0)
		value->cycles = 0;

	/*
	 * if register is 0, then stored values are invalid,
	 * it may be casused by backup battery totally discharged
	 */
	if (data == 0) {
		dev_err(info->dev, "attention: saved value isn't realiable!\n");
		return -EINVAL;
	}

	return 0;
}

static int is_charger_online(struct pm88x_battery_info *info,
			     int *who_is_charging)
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
				*who_is_charging = i;
				return val.intval;
			}
		}
	}
	*who_is_charging = 0;
	return 0;
}

static int pm88x_battery_get_charger_status(struct pm88x_battery_info *info)
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

static int pm88x_enable_bat_detect(struct pm88x_battery_info *info, bool enable)
{
	int data, mask, ret = 0;

	/*
	 * 0. gpadc is in non-stop mode
	 *    enable at least another measurement
	 *    done in the mfd driver
	 */

	/* 1. choose gpadc 1/3 to detect battery */
	switch (info->gpadc_det_no) {
	case 1:
		data = 0;
		mask = PM88X_BD_GP_SEL;
		/* choose gpadc1 for battery temperature monitoring algo */
		regmap_update_bits(info->chip->gpadc_regmap,
				   PM88X_GPADC_CONFIG8,
				   PM88X_BATT_TEMP_SEL,
				   PM88X_BATT_TEMP_SEL_GP1);
		break;
	case 3:
		/* choose gpadc3 for battery temperature monitoring algo */
		data = mask = PM88X_BD_GP_SEL;
		regmap_update_bits(info->chip->gpadc_regmap,
				   PM88X_GPADC_CONFIG8,
				   PM88X_BATT_TEMP_SEL,
				   PM88X_BATT_TEMP_SEL_GP3);
		break;
	default:
		dev_err(info->dev,
			"wrong gpadc number: %d\n", info->gpadc_det_no);
		return -EINVAL;
	}

	if (enable) {
		data |= (PM88X_GPADC_BD_EN | PM88X_GPADC_BD_PREBIAS);
		mask |= (PM88X_GPADC_BD_EN | PM88X_GPADC_BD_PREBIAS);
	} else {
		data = 0;
		mask = PM88X_GPADC_BD_EN | PM88X_GPADC_BD_PREBIAS;
	}

	ret = regmap_update_bits(info->chip->gpadc_regmap,
				 PM88X_GPADC_CONFIG8, mask, data);
	return ret;
}

static bool pm88x_check_battery_present(struct pm88x_battery_info *info)
{
	static bool present;
	int data, ret = 0;

	if (!info) {
		pr_err("%s: empty battery info.\n", __func__);
		return true;
	}

	if (info->use_ntc) {
		if (info->use_sw_bd) {
			/*
			 * 1. bias_current = 1uA
			 * 2. check the gpadc voltage: if < 1.8V, present
			 *    0.6 * Vsys = 0.6 * 3V = 1.8V
			 */
			int gp = info->gpadc_det_no;
			int volt;

			if (gp != 0 && gp != 2) {
				dev_err(info->dev,
					"%s: wrong_gpadc = %d\n", __func__, gp);
				present = true;
				return present;
			}

			switch (info->chip->type) {
			case PM886:
				if (info->chip->chip_id == PM886_A1) {
					/*
					 * disable battery temperature monitoring:
					 * clear bit4
					 */
					regmap_update_bits(info->chip->battery_regmap,
							   PM88X_CHG_CONFIG1,
							   PM88X_BATTEMP_MON_EN, 0);
					/*
					 * select GPADC3 as external input for
					 * battery tempeareure monitoring algorithm
					 */
					regmap_update_bits(info->chip->gpadc_regmap,
							   PM88X_GPADC_CONFIG8,
							   PM88X_BATT_TEMP_SEL,
							   PM88X_BATT_TEMP_SEL_GP3);
				}
				break;
			default:
				break;
			}
			/* enable bias current */
			ret = extern_pm88x_gpadc_set_current_generator(gp, 1);
			if (ret < 0)
				goto out;
			/* set bias_current = 1uA */
			ret = extern_pm88x_gpadc_set_bias_current(gp, 1);
			if (ret < 0)
				goto out;
			/* measure the gpadc voltage */
			ret = extern_pm88x_gpadc_get_volt(gp, &volt);
			if (ret < 0)
				goto out;
			if (volt < 700000)
				present = true;
			else
				present = false;

		} else {
			/* use gpadc 1/3 to detect battery */
			ret = pm88x_enable_bat_detect(info, true);
			if (ret < 0) {
				present = true;
				goto out;
			}
			regmap_read(info->chip->base_regmap, PM88X_STATUS1, &data);
			present = !!(data & PM88X_BAT_DET);

			/* disable battery detection to measure temperature */
			pm88x_enable_bat_detect(info, false);

		}
	} else {
		/* battery is _not_ removable */
		present = true;
	}
out:
	if (ret < 0)
		present = true;

	return present;
}

/*
 * the following cases are considered as software-reboot:
 * 1. power down log is empty
 * 2. FAULT_WAKEUP is set
 * 3. HW_RESET1 is set
 */
static bool system_may_reboot(struct pm88x_battery_info *info)
{
	if (!info || !info->chip)
		return true;

	dev_info(info->dev, "%s: powerdown1 = 0x%x\n", __func__, info->chip->powerdown1);
	dev_info(info->dev, "%s: powerdown2 = 0x%x\n", __func__, info->chip->powerdown2);
	dev_info(info->dev, "%s: powerup = 0x%x\n", __func__, info->chip->powerup);

	/* FAULT_WAKEUP */
	if (info->chip->powerup & (1 << 5))
		return true;

	/* HW_RESET1 */
	if (info->chip->powerdown2 & (1 << 5))
		return true;

	/* delete HYB_DONE */
	return !(info->chip->powerdown1 || (info->chip->powerdown2 & 0xfe));
}

static bool check_battery_change(struct pm88x_battery_info *info,
				 int new_soc, int saved_soc)
{
	int status, remove_th;
	if (!info) {
		pr_err("%s: empty battery info!\n", __func__);
		return true;
	}

	info->bat_params.status = pm88x_battery_get_charger_status(info);
	status = info->bat_params.status;
	switch (status) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
		if (saved_soc < 5)
			remove_th = 5;
		else if (saved_soc < 15)
			remove_th = 10;
		else if (saved_soc < 80)
			remove_th = 15;
		else
			remove_th = 5;
		break;
	default:
		remove_th = 60;
		break;
	}
	dev_info(info->dev, "new_soc = %d, saved_soc = %d, remove = %d\n",
		 new_soc, saved_soc, remove_th);

	return !!(abs(new_soc - saved_soc) > remove_th);
}

static bool check_soc_range(struct pm88x_battery_info *info, int soc)
{
	if (!info) {
		pr_err("%s: empty battery info!\n", __func__);
		return true;
	}

	if ((soc > info->range_low_th) && (soc < info->range_high_th))
		return false;
	else
		return true;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int pm88x_get_batt_vol(struct pm88x_battery_info *info, int active)
{
	struct iio_channel *channel;
	int vol, ret;

	if (active)
		channel = info->chan[VBATT_CHAN];
	else
		channel = info->chan[VBATT_SLP_CHAN];

	if (!channel) {
		pr_err("%s: cannot get useable channel\n", __func__);
		return -EINVAL;
	}

	ret = iio_read_channel_processed(channel, &vol);
	if (ret < 0) {
		dev_err(info->dev, "read %s channel fails!\n",
			channel->channel->datasheet_name);
		return ret;
	}

	/* change to mili-volts */
	vol /= 1000;

	dev_dbg(info->dev, "%s: active = %d, voltage = %dmV\n",
		__func__, active, vol);

	return vol;
}

/* get soc from ocv: lookup table */
static int pm88x_get_soc_from_ocv(int ocv)
{
	int i, count = 100;
	int soc = 0;

	if (ocv < ocv_table[0]) {
		soc = 0;
		goto out;
	}

	count = 100;
	for (i = count - 1; i >= 0; i--) {
		if (ocv >= ocv_table[i]) {
			soc = i + 1;
			break;
		}
	}
out:
	return soc;
}

static int pm88x_get_ibat_cc(struct pm88x_battery_info *info)
{
	int ret, data;
	unsigned char buf[3];

	ret = regmap_bulk_read(info->chip->battery_regmap, PM88X_IBAT_VAL1,
			       buf, 3);
	if (ret < 0)
		goto out;

	data = ((buf[2] & 0x3) << 16) | ((buf[1] & 0xff) << 8) | buf[0];

	/* discharging, ibat < 0; charging, ibat > 0 */
	if (data & (1 << 17))
		data = (0xff << 24) | (0x3f << 18) | data;

	/* the current LSB is 0.04578mA */
	data = (data * 458) / 10;
	dev_dbg(info->dev, "%s--> ibat_cc = %duA, %dmA\n", __func__,
		data, data / 1000);

out:
	if (ret < 0)
		data = 100;

	return data;
}

static void find_match(struct pm88x_battery_info *info,
		       unsigned int ohm, int *low, int *high)
{
	int start, end, mid;
	int size = info->temp_ohm_table_size;

	/* the resistor value decends as the temperature increases */
	if (ohm >= info->temp_ohm_table[0].ohm) {
		*low = 0;
		*high = 0;
		return;
	}

	if (ohm <= info->temp_ohm_table[size - 1].ohm) {
		*low = size - 1;
		*high = size - 1;
		return;
	}

	start = 0;
	end = size;
	while (start < end) {
		mid = start + (end - start) / 2;
		if (ohm > info->temp_ohm_table[mid].ohm) {
			end = mid;
		} else {
			start = mid + 1;
			if (ohm >= info->temp_ohm_table[start].ohm)
				end = start;
		}
	}

	*low = end;
	if (ohm == info->temp_ohm_table[end].ohm)
		*high = end;
	else
		*high = end - 1;
}

static int pm88x_get_batt_temp(struct pm88x_battery_info *info)
{
	struct iio_channel *channel = info->chan[TEMP_CHAN];
	int ohm, ret, default_temp = 25;
	int temp, low, high, low_temp, high_temp, low_ohm, high_ohm;

	if (!info->bat_temp_monitor_en)
		return default_temp;

	if (!channel) {
		pr_err("%s: cannot get useable channel.\n", __func__);
		return default_temp;
	}

	ret = iio_read_channel_scale(channel, &ohm, NULL);
	if (ret < 0) {
		dev_err(info->dev, "read %s channel fails!\n",
			channel->channel->datasheet_name);
		return default_temp;
	}

	find_match(info, ohm, &low, &high);
	if (low == high) {
		temp = info->temp_ohm_table[low].temp;
	} else {
		low_temp = info->temp_ohm_table[low].temp;
		low_ohm = info->temp_ohm_table[low].ohm;

		high_temp = info->temp_ohm_table[high].temp;
		high_ohm = info->temp_ohm_table[high].ohm;

		temp = (ohm - low_ohm) * (high_temp - low_temp) / (high_ohm - low_ohm) + low_temp;
	}
	dev_dbg(info->dev, "ohm = %d, low_index = %d, high_index = %d, temp = %d\n",
		ohm, low, high, temp);

	return temp;
}

/*	Temperature ranges:
 *
 * ----------|---------------|---------------|---------------|----------
 *           |               |               |               |
 *  Shutdown |   Charging    |   Charging    |   Charging    | Shutdown
 *           |  not allowed  |    allowed    |  not allowed  |
 * ----------|---------------|---------------|---------------|----------
 *       too_cold(t1)	    cold(t2)	    hot(t3)	   too_hot(t4)
 *
 */
enum {
	COLD_NO_CHARGING,
	LOW_TEMP_RANGE,
	STD_TEMP_RANGE,
	HIGH_TEMP_RANGE,
	HOT_NO_CHARGING,

	MAX_RANGE,
};

static int pm88x_get_batt_health(struct pm88x_battery_info *info)
{
	int temp, health, range, old_range = MAX_RANGE;
	if (!info->bat_params.present) {
		info->bat_params.health = POWER_SUPPLY_HEALTH_UNKNOWN;
		return 0;
	}

	temp = info->bat_params.temp / 10;

	if (temp <= info->t1) {
		range = COLD_NO_CHARGING;
		health = POWER_SUPPLY_HEALTH_DEAD;
	} else if (temp <= info->t2) {
		range = LOW_TEMP_RANGE;
		health = POWER_SUPPLY_HEALTH_COLD;
	} else if (temp <= info->t3) {
		range = STD_TEMP_RANGE;
		health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (temp <= info->t4) {
		range = HIGH_TEMP_RANGE;
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else {
		range = HOT_NO_CHARGING;
		health = POWER_SUPPLY_HEALTH_DEAD;
	}

	if ((old_range != range) && (old_range != MAX_RANGE)) {
		dev_dbg(info->dev, "temperature changes: %d --> %d\n",
			old_range, range);
		power_supply_changed(&info->battery);
	}
	old_range = range;

	return health;
}

static int pm88x_cycles_notifier_call(struct notifier_block *nb,
				       unsigned long val, void *v)
{
	struct pm88x_battery_info *info =
		container_of(nb, struct pm88x_battery_info, nb);
	int chg_status;

	dev_dbg(info->dev, "notifier call is called.\n");
	chg_status = pm88x_battery_get_charger_status(info);
	/* ignore whether charger is plug in or out if battery is FULL */
	if (chg_status == POWER_SUPPLY_STATUS_FULL) {
		if (info->valid_cycle) {
			info->bat_params.cycle_nums++;
			info->valid_cycle = false;
			dev_info(info->dev, "update: battery cycle = %d\n",
				 info->bat_params.cycle_nums);
		}
		return NOTIFY_DONE;
	}

	mutex_lock(&info->cycle_lock);
	switch (val) {
		/* charger cable removal */
	case NULL_CHARGER:
		if (info->bat_params.soc > info->soc_high_th_cycle) {
			if (info->valid_cycle) {
				info->bat_params.cycle_nums++;
				info->valid_cycle = false;
				dev_info(info->dev,
					 "update: battery cycle = %d\n",
					 info->bat_params.cycle_nums);
			}
		} else {
			dev_dbg(info->dev,
				"no update: battey cycle = %d\n",
				info->bat_params.cycle_nums);
			info->valid_cycle = false;
		}
		break;
	default:
		if (info->bat_params.soc < info->soc_low_th_cycle) {
			info->valid_cycle = true;
			dev_info(info->dev,
				 "begin to monitor: battery cycle = %d\n",
				 info->bat_params.cycle_nums);
		} else {
			info->valid_cycle = false;
			dev_info(info->dev,
				 "no need to monitor: battery cycle = %d\n",
				 info->bat_params.cycle_nums);
		}
		break;
	}
	mutex_unlock(&info->cycle_lock);

	return NOTIFY_OK;
}

static int pm88x_battery_get_slp_cnt(struct pm88x_battery_info *info)
{
	int ret;
	unsigned char buf[2];
	unsigned int cnt, mask, data;

	ret = regmap_bulk_read(info->chip->base_regmap, PM88X_SLP_CNT1, buf, 2);
	if (ret) {
		cnt = 0;
		goto out;
	}

	cnt = ((buf[1] & 0x07) << 8) | buf[0];
	cnt &= 0x7ff;
out:
	/* reset the sleep counter by setting SLP_CNT_RST */
	data = mask = PM88X_SLP_CNT_RST;
	ret = regmap_update_bits(info->chip->base_regmap,
				 PM88X_SLP_CNT2, mask, data);

	return cnt;
}

/*
 * check whether it's the right point to set ocv_is_realiable flag
 * if yes, set it;
 *   else, leave it as 0;
 */
static void check_set_ocv_flag(struct pm88x_battery_info *info,
			       struct ccnt *ccnt_val)
{
	int old_soc, vol, slp_cnt, new_soc, low_th, high_th, tmp_soc, chg_status;
	bool soc_in_good_range;

	chg_status = pm88x_battery_get_charger_status(info);
	/*
	 * when AC charger is inserted, system can enter suspend mode,
	 * just skip it.
	 */
	if (chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		dev_info(info->dev, "system enter suspend when charging.\n");
		return;
	}

	/* save old SOC in case to recover */
	old_soc = ROUND_SOC(ccnt_val->soc) / 10; /* 100% */
	low_th = info->range_low_th;
	high_th = info->range_high_th;

	/*
	 * check if battery is relaxed enough
	 * *_get_slp_cnt() sets SLP_CNT_RST to clear the sleep counter
	 */
	slp_cnt = pm88x_battery_get_slp_cnt(info);
	if (slp_cnt < info->sleep_counter_th) {
		dev_info(info->dev, "battery is not relaxed.\n");
		return;
	}
	dev_info(info->dev, "%s: battery has slept %d seconds.\n", __func__, slp_cnt);

	if (info->ocv_is_realiable) {
		dev_info(info->dev, "%s: ocv_is_realiable is true.\n", __func__);
		return;
	}

	/* read last sleep voltage and calc new SOC */
	vol = pm88x_get_batt_vol(info, 0);
	new_soc = pm88x_get_soc_from_ocv(vol);

	/* check if the new SoC is in good range or not */
	soc_in_good_range = check_soc_range(info, new_soc);
	if (soc_in_good_range) {
		info->ocv_is_realiable = 1;
		tmp_soc = new_soc;
		dev_info(info->dev, "good range: new SoC = %d\n", new_soc);
	} else {
		info->ocv_is_realiable = 0;
		tmp_soc = old_soc;
		dev_info(info->dev, "in bad range (%d), no update\n", old_soc);
	}

	ccnt_val->soc = tmp_soc * 10;
	ccnt_val->last_cc = (ccnt_val->max_cc / 1000) * ROUND_SOC(ccnt_val->soc);
}

static int pm88x_battery_calc_ccnt(struct pm88x_battery_info *info,
				   struct ccnt *ccnt_val)
{
	int data, ret, factor;
	u8 buf[5];
	s64 ccnt_uc = 0, ccnt_mc = 0;

	/* 1. read columb counter to get the original SoC value */
	regmap_read(info->chip->battery_regmap, PM88X_CC_CONFIG2, &data);
	/*
	 * set PM88X_CC_READ_REQ to read Qbat_cc,
	 * if it has been set, then it means the data not ready
	 */
	if (!(data & PM88X_CC_READ_REQ))
		regmap_update_bits(info->chip->battery_regmap, PM88X_CC_CONFIG2,
				   PM88X_CC_READ_REQ, PM88X_CC_READ_REQ);
	/* wait until Qbat_cc is ready */
	do {
		regmap_read(info->chip->battery_regmap, PM88X_CC_CONFIG2,
			    &data);
	} while ((data & PM88X_CC_READ_REQ));

	ret = regmap_bulk_read(info->chip->battery_regmap, PM88X_CC_VAL1,
			       buf, 5);
	if (ret < 0)
		return ret;

	ccnt_uc = (s64) (((s64)(buf[4]) << 32)
			 | (u64)(buf[3] << 24) | (u64)(buf[2] << 16)
			 | (u64)(buf[1] << 8) | (u64)buf[0]);

	dev_dbg(info->dev, "buf[0 ~ 4] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		 buf[0], buf[1], buf[2], buf[3], buf[4]);
	/* Factor is nC */
	factor = (info->cc_fixup) * 715 / 100;
	ccnt_uc = ccnt_uc * factor;
	ccnt_uc = div_s64(ccnt_uc, 1000);
	ccnt_mc = div_s64(ccnt_uc, 1000);
	dev_dbg(info->dev,
		"%s--> ccnt_uc: %lld uC, ccnt_mc: %lld mC, old->last_cc: %d mC, old->real_last_cc: %d mC\n",
		__func__, ccnt_uc, ccnt_mc, ccnt_val->last_cc, ccnt_val->real_last_cc);

	/* 2. add the value */
	ccnt_val->real_last_cc += ccnt_mc;
	ccnt_val->last_cc += ccnt_mc;

	/* 3. clap battery SoC for sanity check */
	if (ccnt_val->last_cc > ccnt_val->max_cc) {
		ccnt_val->soc = 1000;
		ccnt_val->last_cc = ccnt_val->max_cc;
	}
	if (ccnt_val->last_cc < 0) {
		ccnt_val->soc = 0;
		ccnt_val->last_cc = 0;
	}

	/* keep updating the last_cc */
	if (!ccnt_val->bypass_cc)
		ccnt_val->soc = ccnt_val->last_cc * 100 / (ccnt_val->max_cc / 10);
	else
		dev_info(info->dev, "%s: CC is bypassed.\n", __func__);

	dev_dbg(info->dev,
		 "%s<-- ccnt_val->soc: %d%%, new->last_cc: %d mC\n",
		 __func__, ccnt_val->soc, ccnt_val->last_cc);

	if (ccnt_val->real_last_cc > ccnt_val->max_cc) {
		ccnt_val->real_soc = 1000;
		ccnt_val->real_last_cc = ccnt_val->max_cc;
	}
	if (ccnt_val->real_last_cc < 0) {
		ccnt_val->real_soc = 0;
		ccnt_val->real_last_cc = 0;
	}
	ccnt_val->real_soc = ccnt_val->real_last_cc * 100 / (ccnt_val->max_cc / 10);

	dev_dbg(info->dev,
		 "%s<-- ccnt_val->real_soc: %d%%, new->real_last_cc: %d mC\n",
		 __func__, ccnt_val->real_soc, ccnt_val->real_last_cc);

	return 0;
}

static void pm88x_battery_correct_low_temp(struct pm88x_battery_info *info,
					   struct ccnt *ccnt_val)
{
	int times, offset;
	static bool temp_ever_low, temp_is_fine;

	/* the temperature is multipled by 10 times */
	if (info->bat_params.temp < -100) {
		dev_info(info->dev, "temperature is lower than -10C\n");
		times = info->times_in_minus_ten;
		offset = info->offset_in_minus_ten;
		temp_is_fine = false;
		temp_ever_low = true;
	} else if (info->bat_params.temp < 0) {
		dev_info(info->dev, "temperature is lower than 0C\n");
		times = info->times_in_zero;
		offset = info->offset_in_zero;
		temp_is_fine = false;
		temp_ever_low = true;
	} else {
		times = 1;
		offset = 0;
		temp_is_fine = true;
	}

	/* bit 0, 0xc0 register in gpadc page: cleared in boot up stage */
	if (info->bat_params.temp < 0)
		regmap_update_bits(info->chip->gpadc_regmap, 0xc0, 0x1, 0x1);

	/*
	 * now the temperature resumes back,
	 * but the temperature is low in the previous point,
	 * we need to clamp the capacity until the real capacity falls down
	 * to the same/lower value as ccnt_val->soc,
	 * which is used to report to upper layer
	 */
	if (temp_is_fine && temp_ever_low) {
		if (ccnt_val->real_soc <= ccnt_val->soc) {
			ccnt_val->soc = ccnt_val->real_soc;
			temp_ever_low = false;
			ccnt_val->bypass_cc = false;
		} else {
			/* by pass the coulom counter for ccnt_val->soc */
			ccnt_val->bypass_cc = true;
		}
	} else {
		/*
		 * when temp_is_fine or temp_ever_low is false, we should disable bypass_cc
		 * otherwise, when battery changed from full to discharging,
		 * SoC will keep full all the time.
		 */
		ccnt_val->bypass_cc = false;
	}

	/* offset are multipled by 10 times becasue the soc now is 1000% */
	offset *= 10;

	ccnt_val->soc = ccnt_val->real_soc * times;
	ccnt_val->soc -= offset;
}

/* correct SoC according to temp */
static void correct_soc_by_temp(struct pm88x_battery_info *info,
				struct ccnt *ccnt_val)
{
	static int power_off_cnt;
	/*
	 * low temperature
	 * if power_off_cnt > power_off_cnt_th[0],
	 * reset power_off_cnt value and decrease SoC by 1% step.
	 */
	if (info->bat_params.temp < 0) {
		if (power_off_cnt > info->power_off_cnt_th[0]) {
			dev_info(info->dev, "%s: power off th voltage at low temp = %d\n",
				 __func__, info->bat_params.volt);
			power_off_cnt = 0;
			if (ccnt_val->soc > 0)
				ccnt_val->soc -= 10;
		} else {
			dev_info(info->dev, "hit power off cnt at low temp = %d\n", power_off_cnt);
			power_off_cnt++;
		}
	} else {
		if (power_off_cnt > info->power_off_cnt_th[1]) {
			dev_info(info->dev, "%s: power off th voltage at room temp= %d\n",
				 __func__, info->bat_params.volt);
			power_off_cnt = 0;
			if (ccnt_val->soc > 0)
				ccnt_val->soc -= 10;
		} else {
			dev_info(info->dev, "hit power off cnt at room temp = %d\n", power_off_cnt);
			power_off_cnt++;
		}
	}
}

/* correct SoC according to user scenario */
static void pm88x_battery_correct_soc(struct pm88x_battery_info *info,
				      struct ccnt *ccnt_val)
{
	static int chg_status, old_soc;

	info->bat_params.volt = pm88x_get_batt_vol(info, 1);
	if (info->bat_params.status == POWER_SUPPLY_STATUS_UNKNOWN) {
		dev_dbg(info->dev, "battery status unknown, dont update\n");
		return;
	}

	/*
	 * use ccnt_val, which is the real capacity,
	 * not use the info->bat_parmas.soc
	 */
	chg_status = pm88x_battery_get_charger_status(info);
	old_soc = ccnt_val->soc;
	switch (chg_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		dev_dbg(info->dev, "%s: before: charging-->capacity: %d%%\n",
			__func__, ccnt_val->soc);

		/*
		 * last time when it enters into this _routine_,
		 * the battery is discharging in low temperature scenario
		 * it resumes back when arrives here
		 */
		if (ccnt_val->bypass_cc)
			ccnt_val->bypass_cc = false;

		/* the column counter has reached 99% here, clamp it to 99% */
		ccnt_val->soc = (ccnt_val->soc >= 990) ? 990 : ccnt_val->soc;
		ccnt_val->previous_soc = (ccnt_val->previous_soc >= 990) ? 990 : ccnt_val->previous_soc;

		/*
		 * in supplement mode, if the load is so heavy that
		 * it exceeds the charger's capability to power the system, the
		 * battery is connected into the system and begins to discharge,
		 * at this moment, the STATUS from charger is still "charging",
		 * which makes sense from user's view.
		 * so we need to monitor this case and hold the battery capacity
		 * to make sure it doesn't drop.
		 */
		if (ccnt_val->previous_soc > ccnt_val->soc) {
			dev_info(info->dev,
				 "%s: supplement: previous_soc = %d%%, new_soc = %d%%\n",
				 __func__,
				 ccnt_val->previous_soc, ccnt_val->soc);
			ccnt_val->soc = ccnt_val->previous_soc;
		}

		info->bat_params.status = POWER_SUPPLY_STATUS_CHARGING;

		/* clear this flag once the charging begins */
		mutex_lock(&info->volt_lock);
		is_extreme_low = false;
		/* battery voltage interrupt */
		pm88x_bat_enable_irq(&info->irqs[1]);
		mutex_unlock(&info->volt_lock);

		dev_dbg(info->dev, "%s: after: charging-->capacity: %d%%\n",
			__func__, ccnt_val->soc);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		dev_dbg(info->dev, "%s: before: full-->capacity: %d\n",
			__func__, ccnt_val->soc);

		if (ccnt_val->soc < 1000) {
			dev_info(info->dev,
				 "%s: %d: capacity %d%% < 1000%%, ramp up..\n",
				 __func__, __LINE__, ccnt_val->soc);
			ccnt_val->soc += 10;
			ccnt_val->real_soc = ccnt_val->soc;
			info->bat_params.status = POWER_SUPPLY_STATUS_CHARGING;
		}

		if (ccnt_val->soc >= 1000) {
			ccnt_val->soc = 1000;
			ccnt_val->real_soc = 1000;
			info->bat_params.status = POWER_SUPPLY_STATUS_FULL;

			ccnt_val->last_cc = ccnt_val->max_cc;
			/* also align the real_last_cc for low temperature scenario */
			ccnt_val->real_last_cc = ccnt_val->max_cc;
		}
		ccnt_val->bypass_cc = true;
		dev_dbg(info->dev, "%s: after: full-->capacity: %d%%\n",
			__func__, ccnt_val->soc);
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_UNKNOWN:
	default:
		dev_dbg(info->dev, "%s: before: discharging-->capacity: %d%%\n",
			__func__, ccnt_val->soc);
		pm88x_battery_correct_low_temp(info, ccnt_val);

		/*
		 * power_off_th      ===> capacity is no less than 1%
		 *	|
		 *	|
		 *	v
		 * safe_power_off_th ===> capacity is 0%
		 *	|
		 *	|
		 *	v
		 * system is dead
		 *
		 * we monitor the voltage with load:
		 * if voltage_with_load < power_off_th,
		 *         soc is decreased with 1% step
		 * _once_ voltage_with_load has ever been < safe_power_off_th
		 * (it shows the voltage is really very low,
		 *  we set is_extreme_low to indicate this condition)
		 *         then every time we arrive here,
		 *         the soc is decreased with 1% step
		 */
		if (is_extreme_low) {
			dev_info(info->dev, "%s: extreme_low : voltage = %d\n",
				 __func__, info->bat_params.volt);
			/* battery voltage interrupt */
			pm88x_bat_disable_irq(&info->irqs[1]);
			if (ccnt_val->soc > 10)
				ccnt_val->soc -= 10;
			else if (info->bat_params.volt < info->safe_power_off_th) {
				/* if soc <=1, when the volt is lower than safe_power_off_th, set soc=0;
				 * else keep soc=1.
				 */
				dev_info(info->dev, "%s: volt lower than safe_power_off_th\n", __func__);
				ccnt_val->soc = 0;
			}
		} else {
			if (info->bat_params.volt <= info->power_off_th)
				correct_soc_by_temp(info, ccnt_val);
			if (info->bat_params.volt < info->safe_power_off_th) {
				dev_info(info->dev,
					 "%s: for safe: voltage = %d\n",
					 __func__, info->bat_params.volt);
				mutex_lock(&info->volt_lock);
				is_extreme_low = true;
				/* battery voltage interrupt */
				pm88x_bat_disable_irq(&info->irqs[1]);
				mutex_unlock(&info->volt_lock);
			}
		}

		ccnt_val->soc = clamp_soc(ccnt_val->soc);

		/*
		 * clamp the capacity can only decrease in discharging scenario;
		 * just in case it happens
		 */
		if (ccnt_val->previous_soc < ccnt_val->soc) {
			dev_info(info->dev,
				 "%s: ?: previous_soc = %d%%, new_soc = %d%%\n",
				 __func__,
				 ccnt_val->previous_soc, ccnt_val->soc);
			ccnt_val->soc = ccnt_val->previous_soc;
		}

		dev_dbg(info->dev, "%s: after: discharging-->capacity: %d%%\n",
			__func__, ccnt_val->soc);
		break;
	}

	if (old_soc != ccnt_val->soc) {
		dev_info(info->dev, "%s: only CC needs update: %d%% -> %d%%\n",
			 __func__, old_soc, ccnt_val->soc);
		/*
		 * rounded SoC is used by upper layer,
		 * while the last_cc is used to calculate in the following cycle,
		 * so we shouldn't use the rounded_soc to calculate
		 *
		 * the following line is not right
		 * ccnt_val->last_cc = (ccnt_val->max_cc / 1000) * ROUND_SOC(ccnt_val->soc);
		 */
		ccnt_val->last_cc = (ccnt_val->max_cc / 1000) * (ccnt_val->soc);
	}

	/* corner case: we need 1% step */
	if (likely(abs(ccnt_val->previous_soc - ccnt_val->soc) <= 10)) {
		dev_dbg(info->dev,
			"%s: the step is fine: previous = %d%%, soc = %d%%\n",
			__func__, ccnt_val->previous_soc, ccnt_val->soc);
	} else {
		if (ccnt_val->previous_soc - ccnt_val->soc > 10) {
			ccnt_val->soc = ccnt_val->previous_soc - 10;
			dev_info(info->dev,
				 "%s: discharging too fast: previous = %d%%, soc = %d%%\n",
				 __func__, ccnt_val->previous_soc, ccnt_val->soc);
		} else {
			ccnt_val->soc = ccnt_val->previous_soc + 10;
			dev_info(info->dev,
				 "%s: charging too fast? previous = %d%%, soc = %d%%\n",
				 __func__, ccnt_val->previous_soc, ccnt_val->soc);
		}
	}

	/*
	 * previous_soc is used for calculation,
	 * it reflects the delta in one round: real_value + protected_value
	 */

	ccnt_val->soc = clamp_soc(ccnt_val->soc);
	ccnt_val->previous_soc = ccnt_val->soc;

	ccnt_val->soc = ROUND_SOC(ccnt_val->soc);

	return;
}

static void pm88x_bat_update_status(struct pm88x_battery_info *info)
{
	int ibat;


	info->bat_params.volt = pm88x_get_batt_vol(info, 1);

	ibat = pm88x_get_ibat_cc(info);
	info->bat_params.ibat = ibat / 1000; /* change to mA */

	if (info->bat_params.present == 0) {
		info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
		info->bat_params.temp = 250;
		info->bat_params.soc = 80;
		return;
	}

	/* use charger status if the battery is present */
	info->bat_params.status = pm88x_battery_get_charger_status(info);

	/* measure temperature if the battery is present */
	info->bat_params.temp = pm88x_get_batt_temp(info) * 10;
	info->bat_params.health = pm88x_get_batt_health(info);

	mutex_lock(&info->update_lock);

	pm88x_battery_calc_ccnt(info, &ccnt_data);
	pm88x_battery_correct_soc(info, &ccnt_data);
	info->bat_params.soc = ccnt_data.soc / 10;

	mutex_unlock(&info->update_lock);
}

static void pm88x_battery_monitor_work(struct work_struct *work)
{
	struct pm88x_battery_info *info;
	static int prev_cap = -1;
	static int prev_volt = -1;
	static int prev_status = -1;

	info = container_of(work, struct pm88x_battery_info, monitor_work.work);

	pm88x_bat_update_status(info);
	dev_dbg(info->dev, "%s is called, status update finished.\n", __func__);

	if (atomic_read(&in_resume) == 1) {
		check_set_ocv_flag(info, &ccnt_data);
		atomic_set(&in_resume, 0);
	}

	/*
	 * send a notification when:
	 * 1. capacity or status is changed - in order to update Android
	 * 2. status is FULL - trigger the charger driver to check the recharge threshold
	 */
	if ((prev_cap != info->bat_params.soc)
	    || (prev_status != info->bat_params.status)
	    || (info->bat_params.status == POWER_SUPPLY_STATUS_FULL)) {

		power_supply_changed(&info->battery);
		prev_cap = info->bat_params.soc;
		prev_volt = info->bat_params.volt;
		prev_status = info->bat_params.status;
		dev_info(info->dev,
			 "cap=%d, temp=%d, volt=%d, ocv_is_realiable=%d\n",
			 info->bat_params.soc, info->bat_params.temp / 10,
			 info->bat_params.volt, info->ocv_is_realiable);
	}

	/* save the recent value in non-volatile memory */
	extern_data.soc = ccnt_data.soc / 10;
	extern_data.ocv_is_realiable = info->ocv_is_realiable;
	extern_data.cycles = info->bat_params.cycle_nums;
	pm88x_battery_write_buffer(info, &extern_data);

	if (info->bat_params.soc <= LOW_BAT_CAP)
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
				   LOW_BAT_INTERVAL);
	else
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
				   MONITOR_INTERVAL);
	pm_relax(info->dev);
}

static void pm88x_charged_work(struct work_struct *work)
{
	struct pm88x_battery_info *info =
		container_of(work, struct pm88x_battery_info,
			     charged_work.work);

	pm88x_bat_update_status(info);
	power_supply_changed(&info->battery);
	return;
}

static int pm88x_setup_fuelgauge(struct pm88x_battery_info *info)
{
	int ret = 0, data, mask, tmp;
	u8 buf[2];

	if (!info) {
		pr_err("%s: empty battery info.\n", __func__);
		return true;
	}

	/* 0. set the CCNT_LOW_TH before the CC_EN is set 23.43mC/LSB */
	tmp = ccnt_data.alart_cc * 100 / 2343;
	buf[0] = (u8)(tmp & 0xff);
	buf[1] = (u8)((tmp & 0xff00) >> 8);
	regmap_bulk_write(info->chip->battery_regmap, PM88X_CC_LOW_TH1, buf, 2);

	/* 1. set PM88X_OFFCOMP_EN to compensate Ibat, SWOFF_EN = 0 */
	data = mask = PM88X_OFFCOMP_EN;
	ret = regmap_update_bits(info->chip->battery_regmap, PM88X_CC_CONFIG2,
				 mask, data);
	if (ret < 0)
		goto out;

	/* 2. set the EOC battery current as 100mA, done in charger driver */

	/* 3. use battery current to decide the EOC */
	data = mask = PM88X_IBAT_MEAS_EN;
	ret = regmap_update_bits(info->chip->battery_regmap,
				PM88X_IBAT_EOC_CONFIG, mask, data);
	/*
	 * 4. set SD_PWRUP to enable sigma-delta
	 *    set CC_CLR_ON_RD to clear coulomb counter on read
	 *    set CC_EN to enable coulomb counter
	 */
	data = mask = PM88X_SD_PWRUP | PM88X_CC_CLR_ON_RD | PM88X_CC_EN;
	ret = regmap_update_bits(info->chip->battery_regmap, PM88X_CC_CONFIG1,
				 mask, data);
	if (ret < 0)
		goto out;

	/* 5. enable VBAT measurement */
	data = mask = PM88X_VBAT_MEAS_EN;
	ret = regmap_update_bits(info->chip->gpadc_regmap, PM88X_GPADC_CONFIG1,
				 mask, data);
	if (ret < 0)
		goto out;

	/* 6. hold the sleep counter until this bit is released or be reset */
	data = mask = PM88X_SLP_CNT_HOLD;
	ret = regmap_update_bits(info->chip->base_regmap,
				 PM88X_SLP_CNT2, mask, data);
	if (ret < 0)
		goto out;

	/* 7. set the VBAT threashold as 3000mV: 0x890 * 1.367mV/LSB = 2.996V */
	ret = regmap_write(info->chip->gpadc_regmap, PM88X_VBAT_LOW_TH, 0x89);
out:
	return ret;
}

static void pm88x_init_soc_cycles(struct pm88x_battery_info *info,
				 struct ccnt *ccnt_val,
				 int *initial_soc, int *initial_cycles)
{
	int slp_volt, soc_from_vbat_slp, soc_from_saved, slp_cnt;
	int active_volt, soc_from_vbat_active;
	int cycles_from_saved, saved_is_valid, ever_low_temp;
	bool battery_is_changed, soc_in_good_range, realiable_from_saved;

	/*---------------- the following gets the initial_soc --------------*/
	/*
	 * 1. read vbat_sleep:
	 * - then use the vbat_sleep to calculate SoC: soc_from_vbat_slp
	 */
	slp_volt = pm88x_get_batt_vol(info, 0);
	dev_info(info->dev, "---> %s: slp_volt = %dmV\n", __func__, slp_volt);
	soc_from_vbat_slp = pm88x_get_soc_from_ocv(slp_volt);
	dev_info(info->dev, "---> %s: soc_from_vbat_slp = %d\n",
		 __func__, soc_from_vbat_slp);

	active_volt = pm88x_get_batt_vol(info, 1);
	dev_info(info->dev, "---> %s: active_volt = %dmV\n",
		 __func__, active_volt);
	soc_from_vbat_active = pm88x_get_soc_from_ocv(active_volt);
	dev_info(info->dev, "---> %s: soc_from_vbat_active = %d\n",
		 __func__, soc_from_vbat_active);

	/*
	 * need to reset sleep counter to apply sleep_cnt_hold configuration,
	 * so reading here the slp_cnt and reset the counter.
	 */
	slp_cnt = pm88x_battery_get_slp_cnt(info);

	/* 2. read saved SoC: soc_from_saved */
	/*
	 *  if system comes here because of software reboot
	 *  and
	 *  soc_from_saved is not realiable, use soc_from_vbat_slp
	 */

	saved_is_valid = pm88x_battery_read_buffer(info, &extern_data);
	soc_from_saved = extern_data.soc;
	realiable_from_saved = extern_data.ocv_is_realiable;
	cycles_from_saved = extern_data.cycles;
	dev_info(info->dev,
		 "---> %s: soc_from_saved = %d, realiable_from_saved = %d\n",
		 __func__, soc_from_saved, realiable_from_saved);

	/* ---------------------------------------------------------------- */
	/*
	 * get low_temp_flag: bit 0, 0xc0 register of gpadc page
	 * TODO: switch to a uniform interface
	 */
	regmap_read(info->chip->gpadc_regmap, 0xc0, &ever_low_temp);
	if (ever_low_temp & 0x1) {
		if (soc_from_saved == 0)
			*initial_soc = 1;
		else
			*initial_soc = soc_from_saved;
		*initial_cycles = cycles_from_saved;

		/* clear this flag */
		regmap_update_bits(info->chip->gpadc_regmap, 0xc0, 0x1, 0);
		goto end;
	}

	/*
	 * there are two cases:
	 * a) plug into the charger cable first
	 * b) then plug into the battery
	 * in this case the vbat_slp is a random value, it's not realiable
	 * if SoC saved is not valid, use soc_from_vbat_active
	 * OR
	 * a) plug into the battery first
	 * b) then plug into the charger cable
	 * if SoC saved is not valid, use soc_from_vbat_slp
	 */
	if (info->chip->powerup & 0x2) {
		if (abs(soc_from_vbat_slp - soc_from_vbat_active) > 40) {
			/* plug into the charger cable first */
			if (saved_is_valid < 0 || abs(soc_from_vbat_active - soc_from_saved) > 20)
				*initial_soc = soc_from_vbat_active;
			else
				*initial_soc = soc_from_saved;
		} else if (saved_is_valid >= 0 && abs(soc_from_vbat_slp - soc_from_saved) < 60) {
			/* plug into the battery first */
			*initial_soc = soc_from_saved;
		} else {
			*initial_soc = soc_from_vbat_slp;
		}
		*initial_cycles = cycles_from_saved;
		goto end;
	}

	if (system_may_reboot(info)) {
		dev_info(info->dev,
			 "---> %s: arrive here from reboot.\n", __func__);
		if (saved_is_valid < 0) {
			*initial_soc = soc_from_vbat_active;
			info->ocv_is_realiable = false;
		} else {
			*initial_soc = soc_from_saved;
			info->ocv_is_realiable = realiable_from_saved;
		}
		*initial_cycles = cycles_from_saved;
		goto end;
	}
	dev_info(info->dev, "---> %s: arrive here from power on.\n", __func__);

	/*
	 * 3. compare the soc_from_vbat_slp and the soc_from_saved
	 *    to decide whether the battery is changed:
	 *    if changed, initial_soc = soc_from_vbat_slp;
	 *    else, --> battery is not changed
	 *        if sleep_counter < threshold --> battery is not relaxed
	 *                initial_soc = soc_from_saved;
	 *        else, ---> battery is relaxed
	 *            if soc_from_vbat_slp in good range
	 *                initial_soc = soc_from_vbat_slp;
	 *            else
	 *                initial_soc = soc_from_saved;
	 */
	battery_is_changed = check_battery_change(info, soc_from_vbat_slp,
						  soc_from_saved);
	/*
	 * the data saved is not realiable,
	 * suppose the battery has been changed
	 */
	if (saved_is_valid < 0) {
		dev_info(info->dev,
			 "%s: the data saved is not realiable.\n", __func__);
		battery_is_changed = true;
	}

	dev_info(info->dev, "battery_is_changed = %d\n", battery_is_changed);
	if (battery_is_changed) {
		dev_info(info->dev, "----> %s: battery is changed\n", __func__);
		*initial_soc = soc_from_vbat_slp;
		info->ocv_is_realiable = false;
		/* let's assume it's new battery */
		*initial_cycles = 0;
		goto end;
	}

	/* battery unchanged */
	*initial_cycles = cycles_from_saved;
	dev_info(info->dev, "----> %s: battery is unchanged:\n", __func__);
	dev_info(info->dev, "\t\t slp_cnt = %d\n", slp_cnt);

	if (slp_cnt < info->sleep_counter_th) {
		*initial_soc = soc_from_saved;
		info->ocv_is_realiable = realiable_from_saved;
		dev_info(info->dev,
			 "---> %s: battery is unchanged, and not relaxed:\n",
			 __func__);
		dev_info(info->dev,
			 "\t\t use soc_from_saved and realiable_from_saved.\n");
		goto end;
	}

	dev_info(info->dev,
		 "---> %s: battery is unchanged and relaxed\n", __func__);

	soc_in_good_range = check_soc_range(info, soc_from_vbat_slp);
	dev_info(info->dev, "soc_in_good_range = %d\n", soc_in_good_range);
	if (soc_in_good_range) {
		*initial_soc = soc_from_vbat_slp;
		info->ocv_is_realiable = true;
		dev_info(info->dev,
			 "OCV is in good range, use soc_from_vbat_slp.\n");
	} else {
		*initial_soc = soc_from_saved;
		info->ocv_is_realiable = realiable_from_saved;
		dev_info(info->dev,
			 "OCV is in bad range, use soc_from_saved.\n");
	}

end:
	/* update ccnt_data timely */
	init_ccnt_data(ccnt_val, *initial_soc);

	dev_info(info->dev,
		 "<---- %s: initial soc = %d\n", __func__, *initial_soc);

	return;
}

static void pm88x_pre_setup_fuelgauge(struct pm88x_battery_info *info)
{
	int data;
	switch (info->chip->type) {
	case PM886:
		if (info->chip->chip_id == PM886_A0) {
			pr_info("%s: fix up for the fuelgauge driver.\n", __func__);
			/* 1. base page 0x1f.0 = 1 ---> unlock test page */
			regmap_write(info->chip->base_regmap, 0x1f, 0x1);
			/* 2. tune ibat */
			regmap_read(info->chip->test_regmap, 0x8f, &data);
			/* if the bit [5:0] = 0, needs to rewrite to 0x22, b'10_0010 */
			if ((data & 0x3f) == 0) {
				dev_info(info->dev, "%s: -->test[0x8f] = 0x%x\n",
					 __func__, data);
				data &= 0xc0;
				data |= 0x22;
				regmap_write(info->chip->test_regmap, 0x8f, data);
				dev_info(info->dev, "%s: <--test[0x8f] = 0x%x\n",
					 __func__, data);
			}
			/* 3. base page 0x1f.0 = 0 --> lock the test page */
			regmap_write(info->chip->base_regmap, 0x1f, 0x0);
		}
		break;
	default:
		break;
	}
}

static int pm88x_init_fuelgauge(struct pm88x_battery_info *info)
{
	int ret, initial_soc, initial_cycles;
	if (!info) {
		pr_err("%s: empty battery info.\n", __func__);
		return -EINVAL;
	}

	pm88x_pre_setup_fuelgauge(info);

	/* configure HW registers to enable the fuelgauge */
	ret = pm88x_setup_fuelgauge(info);
	if (ret < 0) {
		dev_err(info->dev, "setup fuelgauge registers fail.\n");
		return ret;
	}

	/*------------------- the following is SW related policy -------------*/
	/* 1. check whether the battery is present */
	info->bat_params.present = pm88x_check_battery_present(info);
	/* 2. get initial_soc and initial_cycles */
	pm88x_init_soc_cycles(info, &ccnt_data, &initial_soc, &initial_cycles);
	info->bat_params.soc = initial_soc;
	info->bat_params.cycle_nums = initial_cycles;
	/* 3. the following initial the software status */
	if (info->bat_params.present == 0) {
		info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
		info->bat_params.temp = 250;
		info->bat_params.soc = 80;
		info->bat_params.cycle_nums = 1;
		return 0;
	}
	info->bat_params.status = pm88x_battery_get_charger_status(info);
	/* 4. hardcode type[Lion] */
	info->bat_params.tech = POWER_SUPPLY_TECHNOLOGY_LION;

	/*
	 * 5. disable battery temperature monitoring2 for 88pm886 A1 and 88pm880 A0/A1:
	 * set bit5
	 */
	switch (info->chip->type) {
	case PM886:
		if (info->chip->chip_id == PM886_A1)
			regmap_update_bits(info->chip->battery_regmap,
					   PM88X_CHG_CONFIG1,
					   PM88X_BATTEMP_MON2_DIS,
					   PM88X_BATTEMP_MON2_DIS);
		break;
	case PM880:
 		regmap_update_bits(info->chip->battery_regmap,
 				   PM88X_CHG_CONFIG1,
 				   PM88X_BATTEMP_MON2_DIS,
 				   PM88X_BATTEMP_MON2_DIS);
		break;
	default:
		break;
	}

	return 0;
}

static void pm88x_external_power_changed(struct power_supply *psy)
{
	struct pm88x_battery_info *info;

	info = container_of(psy, struct pm88x_battery_info, battery);
	queue_delayed_work(info->bat_wqueue, &info->charged_work, 0);
	return;
}

static enum power_supply_property pm88x_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static int pm88x_batt_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct pm88x_battery_info *info = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!info->bat_params.present)
			info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
		val->intval = info->bat_params.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->bat_params.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* report fake capacity if battery is absent */
		if (!info->bat_params.present)
			info->bat_params.soc = 80;
		val->intval = info->bat_params.soc;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = info->bat_params.tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		info->bat_params.volt = pm88x_get_batt_vol(info, 1);
		val->intval = (info->bat_params.volt * 1000);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		info->bat_params.ibat = pm88x_get_ibat_cc(info);
		info->bat_params.ibat /= 1000;
		val->intval = info->bat_params.ibat;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!info->bat_params.present)
			info->bat_params.temp = 250;
		else
			info->bat_params.temp = pm88x_get_batt_temp(info) * 10;
		val->intval = info->bat_params.temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		info->bat_params.health = pm88x_get_batt_health(info);
		val->intval = info->bat_params.health;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		mutex_lock(&info->cycle_lock);
		if (!info->bat_params.present)
			val->intval = 1;
		else
			val->intval = info->bat_params.cycle_nums;
		mutex_unlock(&info->cycle_lock);
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static void init_ccnt_data(struct ccnt *ccnt_val, int value)
{
	ccnt_val->soc = 10 * value; /* multiply 10 */
	ccnt_val->real_soc = ccnt_val->soc;

	ccnt_val->last_cc = (ccnt_val->max_cc / 1000) * ROUND_SOC(ccnt_val->soc);
	ccnt_val->real_last_cc = ccnt_val->last_cc;

	ccnt_val->previous_soc = ccnt_val->soc;
}

static int pm88x_batt_set_prop_capacity(struct pm88x_battery_info *info, int data)
{
	if (!info->bat_params.present) {
		dev_warn(info->dev, "battery is not present\n");
		return -EPERM;
	}

	if (data < 0 || data > 100) {
		dev_warn(info->dev, "invalid capacity: %d\n", data);
		return -EINVAL;
	}

	info->bat_params.soc = data;
	init_ccnt_data(&ccnt_data, data);

	return 0;
}

static int pm88x_batt_set_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	struct pm88x_battery_info *info = dev_get_drvdata(psy->dev->parent);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = pm88x_batt_set_prop_capacity(info, val->intval);
		break;
	default:
		ret = -EPERM;
		break;
	}

	return ret;
}

static int pm88x_batt_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		return 1;
	default:
		return 0;
	}
}

static irqreturn_t pm88x_battery_cc_handler(int irq, void *data)
{
	struct pm88x_battery_info *info = data;
	if (!info) {
		pr_err("%s: empty battery info.\n", __func__);
		return IRQ_NONE;
	}
	dev_info(info->dev, "battery columb counter interrupt is served\n");

	/* update the battery status when this interrupt is triggered. */
	pm88x_bat_update_status(info);
	power_supply_changed(&info->battery);

	return IRQ_HANDLED;
}

static irqreturn_t pm88x_battery_vbat_handler(int irq, void *data)
{
	static int enter_low;
	struct pm88x_battery_info *info = data;
	if (!info) {
		pr_err("%s: empty battery info.\n", __func__);
		return IRQ_NONE;
	}

	dev_info(info->dev, "battery voltage interrupt is served\n");

	mutex_lock(&info->volt_lock);
	if (!is_extreme_low) {
		enter_low++;
		dev_info(info->dev, "enter_low = %d\n", enter_low);
		if (enter_low >= 5) {
			enter_low = 0;
			is_extreme_low = true;
		}
	} else {
		mutex_unlock(&info->volt_lock);
		dev_dbg(info->dev, "battery voltage low again!\n");
		return IRQ_HANDLED;
	}
	mutex_unlock(&info->volt_lock);

	/* update the battery status when this interrupt is triggered. */
	pm88x_bat_update_status(info);
	power_supply_changed(&info->battery);

	return IRQ_HANDLED;
}

static struct pm88x_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm88x_irq_descs[] = {
	{"columb counter", pm88x_battery_cc_handler},
	{"battery voltage", pm88x_battery_vbat_handler},
};

static int pm88x_battery_dt_init(struct device_node *np,
				 struct pm88x_battery_info *info)
{
	int ret, size, rows, i, ohm, temp,  index = 0;
	const __be32 *values;

	if (of_get_property(np, "bat-ntc-support", NULL))
		info->use_ntc = true;
	else
		info->use_ntc = false;

	if (info->use_ntc) {
		ret = of_property_read_u32(np, "gpadc-det-no", &info->gpadc_det_no);
		if (ret)
			return ret;
	}

	info->bat_temp_monitor_en = of_property_read_bool(np, "bat-temp-monitor-en");

	if (of_get_property(np, "bat-software-battery-detection", NULL))
		info->use_sw_bd = true;
	else
		info->use_sw_bd = false;

	ret = of_property_read_u32(np, "gpadc-temp-no", &info->gpadc_temp_no);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "bat-capacity", &info->total_capacity);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "sleep-period", &info->sleep_counter_th);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "low-threshold", &info->range_low_th);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "high-threshold", &info->range_high_th);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "alart-percent", &info->alart_percent);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "power-off-th", &info->power_off_th);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "safe-power-off-th",
				   &info->safe_power_off_th);
	if (ret)
		return ret;
	ret = of_property_read_u32_array(np, "power-off-cnt-th",
					 info->power_off_cnt_th, 3);
	if (ret)
		return ret;

	/* initialize the ocv table */
	ret = of_property_read_u32_array(np, "ocv-table", ocv_table, 100);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "zero-degree-ohm",
				   &info->zero_degree_ohm);
	if (ret)
		return ret;

	values = of_get_property(np, "ntc-table", &size);
	if (!values) {
		pr_warn("No NTC table for %s\n", np->name);
		return 0;
	}

	size /= sizeof(*values);
	/* <ohm, temp>*/
	rows = size / 2;
	info->temp_ohm_table = kzalloc(sizeof(struct temp_vs_ohm) * rows,
				       GFP_KERNEL);
	if (!info->temp_ohm_table)
		return -ENOMEM;
	info->temp_ohm_table_size = rows;

	for (i = 0; i < rows; i++) {
		ohm = be32_to_cpup(values + index++);
		info->temp_ohm_table[i].ohm = ohm;

		temp = be32_to_cpup(values + index++);
		if (ohm > info->zero_degree_ohm)
			info->temp_ohm_table[i].temp = 0 - temp;
		else
			info->temp_ohm_table[i].temp = temp;
	}

	/* ignore if fails */
	ret = of_property_read_u32(np, "abs-lowest-temp", &info->abs_lowest_temp);
	ret = of_property_read_u32(np, "t1-degree", &info->t1);
	ret = of_property_read_u32(np, "t2-degree", &info->t2);
	ret = of_property_read_u32(np, "t3-degree", &info->t3);
	ret = of_property_read_u32(np, "t4-degree", &info->t4);

	info->t1 -= info->abs_lowest_temp;
	info->t2 -= info->abs_lowest_temp;
	info->t3 -= info->abs_lowest_temp;
	info->t4 -= info->abs_lowest_temp;

	ret = of_property_read_u32(np, "times-in-minus-ten", &info->times_in_minus_ten);
	ret = of_property_read_u32(np, "offset-in-minus-ten", &info->offset_in_minus_ten);
	ret = of_property_read_u32(np, "times-in-zero", &info->times_in_zero);
	ret = of_property_read_u32(np, "offset-in-zero", &info->offset_in_zero);

	ret = of_property_read_u32(np, "soc-low-th-cycle", &info->soc_low_th_cycle);
	ret = of_property_read_u32(np, "soc-high-th-cycle", &info->soc_high_th_cycle);

	/*
	 * introduced by PCB,
	 * software needs to compensate the CC value,
	 * multipiled by 100
	 */
	ret = of_property_read_u32(np, "cc-fixup", &info->cc_fixup);

	return 0;
}

static int pm88x_battery_setup_adc(struct pm88x_battery_info *info)
{
	struct iio_channel *chan;
	char s[20];

	/* active vbat voltage channel */
	chan = iio_channel_get(info->dev, "vbat");
	if (PTR_ERR(chan) == -EPROBE_DEFER) {
		dev_info(info->dev, "get vbat iio channel defers.\n");
		return -EPROBE_DEFER;
	}
	info->chan[VBATT_CHAN] = IS_ERR(chan) ? NULL : chan;

	/* sleep vbat voltage channel */
	chan = iio_channel_get(info->dev, "vbat_slp");
	if (PTR_ERR(chan) == -EPROBE_DEFER) {
		dev_info(info->dev, "get vbat_slp iio channel defers.\n");
		return -EPROBE_DEFER;
	}
	info->chan[VBATT_SLP_CHAN] = IS_ERR(chan) ? NULL : chan;

	/* temperature channel */
	sprintf(s, "gpadc%d_res", info->gpadc_temp_no);
	chan = iio_channel_get(info->dev, s);
	if (PTR_ERR(chan) == -EPROBE_DEFER) {
		dev_info(info->dev, "get %s iio channel defers.\n", s);
		return -EPROBE_DEFER;
	}
	info->chan[TEMP_CHAN] = IS_ERR(chan) ? NULL : chan;

	return 0;
}

static void pm88x_battery_release_adc(struct pm88x_battery_info *info)
{
	int i;
	for (i = 0; i < MAX_CHAN; i++) {
		if (!info->chan[i])
			continue;

		iio_channel_release(info->chan[i]);
		info->chan[i] = NULL;
	}
}

static int pm88x_battery_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_battery_info *info;
	struct pm88x_bat_pdata *pdata;
	struct device_node *node = pdev->dev.of_node;
	int ret;
	int i, j;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_battery_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* initialize the property in case the are missing in device tree */
	info->abs_lowest_temp = 0;
	info->t1 = 0;
	info->t2 = 15;
	info->t3 = 25;
	info->t4 = 40;

	info->times_in_minus_ten = 1;
	info->offset_in_minus_ten = 0;
	info->times_in_zero = 1;
	info->offset_in_zero = 0;

	info->soc_low_th_cycle = 15;
	info->soc_high_th_cycle = 85;

	info->cc_fixup = 100;

	pdata = pdev->dev.platform_data;
	ret = pm88x_battery_dt_init(node, info);
	if (ret) {
		ret = -EINVAL;
		goto out_free;
	}

	/* give default total capacity */
	if (info->total_capacity)
		ccnt_data.max_cc = info->total_capacity * 3600;
	else
		ccnt_data.max_cc = 1500 * 3600;

	if (info->alart_percent > 100 || info->alart_percent < 0)
		info->alart_percent = 5;
	ccnt_data.alart_cc = ccnt_data.max_cc * info->alart_percent / 100;

	info->chip = chip;
	info->dev = &pdev->dev;
	info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;

	platform_set_drvdata(pdev, info);

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irqs[j].irq = platform_get_irq(pdev, i);
		if (info->irqs[j].irq < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	mutex_init(&info->cycle_lock);
	mutex_init(&info->volt_lock);
	mutex_init(&info->update_lock);

	ret = pm88x_battery_setup_adc(info);
	if (ret < 0)
		goto out_free;

	ret = pm88x_init_fuelgauge(info);
	if (ret < 0)
		goto out_free;

	pm88x_bat_update_status(info);

	info->battery.name = "battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.properties = pm88x_batt_props;
	info->battery.num_properties = ARRAY_SIZE(pm88x_batt_props);
	info->battery.get_property = pm88x_batt_get_prop;
	info->battery.set_property = pm88x_batt_set_prop;
	info->battery.property_is_writeable = pm88x_batt_property_is_writeable;
	info->battery.external_power_changed = pm88x_external_power_changed;
	info->battery.supplied_to = supply_interface;
	info->battery.num_supplicants = ARRAY_SIZE(supply_interface);
	ret = power_supply_register(&pdev->dev, &info->battery);
	info->battery.dev->parent = &pdev->dev;

	info->bat_wqueue = create_singlethread_workqueue("88pm88x-battery-wq");
	if (!info->bat_wqueue) {
		dev_info(chip->dev, "%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto out;
	}

	/* interrupt should be request in the last stage */
	for (i = 0; i < info->irq_nums; i++) {
		if (pm88x_irq_descs[i].handler) {
			ret = devm_request_threaded_irq(info->dev,
							info->irqs[i].irq, NULL,
							pm88x_irq_descs[i].handler,
							IRQF_ONESHOT | IRQF_NO_SUSPEND,
							pm88x_irq_descs[i].name, info);
			if (ret < 0) {
				dev_err(info->dev, "failed to request IRQ: #%d: %d\n",
					info->irqs[i].irq, ret);
				goto out_irq;
			}
		}
	}

	INIT_DELAYED_WORK(&info->charged_work, pm88x_charged_work);
	INIT_DELAYED_WORK(&info->monitor_work, pm88x_battery_monitor_work);

	pm_stay_awake(info->dev);
	/* update the status timely */
	queue_delayed_work(info->bat_wqueue, &info->monitor_work, 0);

	info->nb.notifier_call = pm88x_cycles_notifier_call;
#ifdef CONFIG_USB_MV_UDC
	ret = mv_udc_register_client(&info->nb);
	if (ret < 0) {
		dev_err(info->dev,
			"%s: failed to register client!\n", __func__);
		goto out_irq;
	}
#endif
	device_init_wakeup(&pdev->dev, 1);
	dev_info(info->dev, "%s is successful to be probed.\n", __func__);

	return 0;

out_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irqs[i].irq, info);
out:
	power_supply_unregister(&info->battery);
out_free:
	kfree(info->temp_ohm_table);
	return ret;
}

static int pm88x_battery_remove(struct platform_device *pdev)
{
	int i;
	struct pm88x_battery_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm88x_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irqs[i].irq, info);
	}

	cancel_delayed_work_sync(&info->monitor_work);
	cancel_delayed_work_sync(&info->charged_work);
	flush_workqueue(info->bat_wqueue);

	power_supply_unregister(&info->battery);
	kfree(info->temp_ohm_table);
	pm88x_battery_release_adc(info);

#ifdef CONFIG_USB_MV_UDC
	mv_udc_unregister_client(&info->nb);
#endif
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int pm88x_battery_suspend(struct device *dev)
{
	struct pm88x_battery_info *info = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&info->monitor_work);
	return 0;
}

static int pm88x_battery_resume(struct device *dev)
{
	struct pm88x_battery_info *info = dev_get_drvdata(dev);

	/*
	 * avoid to reading in short sleep case
	 * to update ocv_is_realiable flag effectively
	 */
	pm_stay_awake(info->dev);
	atomic_set(&in_resume, 1);
	queue_delayed_work(info->bat_wqueue,
			   &info->monitor_work, 300 * HZ / 1000);
	return 0;
}

static const struct dev_pm_ops pm88x_battery_pm_ops = {
	.suspend	= pm88x_battery_suspend,
	.resume		= pm88x_battery_resume,
};
#endif

static const struct of_device_id pm88x_fg_dt_match[] = {
	{ .compatible = "marvell,88pm88x-battery", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_fg_dt_match);

static struct platform_driver pm88x_battery_driver = {
	.driver		= {
		.name	= "88pm88x-battery",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_fg_dt_match),
#ifdef CONFIG_PM
		.pm	= &pm88x_battery_pm_ops,
#endif
	},
	.probe		= pm88x_battery_probe,
	.remove		= pm88x_battery_remove,
};

static int pm88x_battery_init(void)
{
	return platform_driver_register(&pm88x_battery_driver);
}
module_init(pm88x_battery_init);

static void pm88x_battery_exit(void)
{
	platform_driver_unregister(&pm88x_battery_driver);
}
module_exit(pm88x_battery_exit);

MODULE_DESCRIPTION("Marvell 88PM88X battery driver");
MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_LICENSE("GPL v2");
