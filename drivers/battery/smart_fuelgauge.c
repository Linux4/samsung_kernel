/*
 *  smart_fuelgauge.c
 *  Samsung smart Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
/* #define BATTERY_LOG_MESSAGE */

#include <linux/battery/fuelgauge/smart_fuelgauge.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static enum power_supply_property smart_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHG_CURRENT,
	POWER_SUPPLY_PROP_MAX_VOLTAGE,
	POWER_SUPPLY_PROP_PRESENT,
};

int smart_fg_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&fuelgauge->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&fuelgauge->i2c_lock);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}

int smart_fg_read_word(struct i2c_client *i2c, u8 reg)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&fuelgauge->i2c_lock);
	for (i = 0; i < 3; i++) {
		ret = i2c_smbus_read_word_data(i2c, reg);
		if (ret >= 0)
			break;
	}

	mutex_unlock(&fuelgauge->i2c_lock);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	return ret;
}

int smart_fg_read_block_data(struct i2c_client *i2c, u8 reg, u8 length, u8 *value)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&fuelgauge->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, length, value);
	if (ret < 0)
		return ret;

	return ret;
}

int smart_fg_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&fuelgauge->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&fuelgauge->i2c_lock);
	if (ret < 0)
		pr_info("%s reg(0x%x), ret(%d)\n",
			__func__, reg, ret);

	return ret;
}

int smart_fg_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&fuelgauge->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&fuelgauge->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}

int smart_fg_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct smart_fuelgauge *fuelgauge = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&fuelgauge->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&fuelgauge->i2c_lock);
	return ret;
}

#if 0
static void smart_fg_test_read(struct smart_fuelgauge *fuelgauge)
{
	pr_info("[SMART_FG] MA(0x%02x || 0x%04x) ,", SMARTFG_MANUFACTURE_ACESS, smart_fg_read_word(fuelgauge->i2c, SMARTFG_MANUFACTURE_ACESS));
	pr_info("Temperature(0x%02x || 0x%04x) ,", SMARTFG_TEMP,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_TEMP));
	pr_info("Current(0x%02x || 0x%04x) ,", SMARTFG_CURR,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_CURR));
	pr_info("RSOC(0x%02x || 0x%04x) ,", SMARTFG_RELATIVE_SOC,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_RELATIVE_SOC));
	pr_info("FCC(0x%02x || 0x%04x) ,", SMARTFG_FULLCAP,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_FULLCAP));
	pr_info("CellV3(0x%02x || 0x%04x) ,", SMARTFG_OPTIONAL_MFG_FUNC1,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_OPTIONAL_MFG_FUNC1));
	pr_info("CellV2(0x%02x || 0x%04x) ,", SMARTFG_OPTIONAL_MFG_FUNC2,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_OPTIONAL_MFG_FUNC2));
	pr_info("CellV1(0x%02x || 0x%04x) ,", SMARTFG_OPTIONAL_MFG_FUNC3,  smart_fg_read_word(fuelgauge->i2c, SMARTFG_OPTIONAL_MFG_FUNC3));
	pr_info("FgStatus(0xA2 || 0x%04x) ,", smart_fg_read_word(fuelgauge->i2c, 0xA2));
	pr_info("FgMaxCap(0xA3 || 0x%04x) ,", smart_fg_read_word(fuelgauge->i2c, 0xA3));
	pr_info("FgMixSoc(0xA4 || 0x%04x) ,", smart_fg_read_word(fuelgauge->i2c, 0xA4));
	pr_info("FgAvSoc(0xA5 || 0x%04x) ,", smart_fg_read_word(fuelgauge->i2c, 0xA5));
	pr_info("FgQCounter(0xA7 || 0x%04x)\n", smart_fg_read_word(fuelgauge->i2c, 0xA7));
}

static void smart_fg_write_remcap_alarm(struct smart_fuelgauge *fuelgauge, int value)
{
	u16 data;
	data = value;
	smart_fg_write_word(fuelgauge->i2c, SMARTFG_REMCAP_ALARM, data);
}

static int smart_fg_read_remcap_alarm(struct smart_fuelgauge *fuelgauge)
{
	int data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_REMCAP_ALARM);
	return data;
}
#endif

static u8 smart_fg_read_firmware(struct smart_fuelgauge *fuelgauge)
{
	u8 length;
	u8 data[13];

	length = 0xC;
	smart_fg_read_block_data(fuelgauge->i2c, 0x78, length, data);

	pr_info("%s : FIRMWARE(0x%02x)n", __func__, data[4]);

	return data[4];
}

static int smart_fg_read_status(struct smart_fuelgauge *fuelgauge)
{
	int data;
	int status = 0;

	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_BATT_STATUS);

	if (data & 0x0020)
		status =  POWER_SUPPLY_STATUS_FULL;

	pr_info("%s : DATA(0x%04x || %d)\n", __func__, data, status);

	return status;
}

static int smart_fg_read_fullcap(struct smart_fuelgauge *fuelgauge)
{
	int data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_FULLCAP);
	if (data < 0) {
		pr_info("%s READ FULLCAP Failed!!!\n|", __func__);
	}
	return data;
}

static int smart_fg_read_designcap(struct smart_fuelgauge *fuelgauge)
{
	int data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_DESIGNCAP);
	if (data < 0) {
		pr_info("%s READ DESIGNCAP Failed!!!\n|", __func__);
	}
	return data;
}

static int smart_fg_read_vcell(struct smart_fuelgauge *fuelgauge)
{
	int data = 0;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_VOLT);
	if (data < 0) {
		pr_info("%s READ VCELL Failed!!!\n|", __func__);
	}
	return data;
}

static int smart_fg_read_temp(struct smart_fuelgauge *fuelgauge)
{
	int data = 0;
	int temp;
	int i;

	for (i = 0; i < 5; i++) {
		data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_TEMP);
		if (data < 0) {
			pr_info("%s READ TEMP Failed!!!\n|", __func__);
			data = 3000;
			break;
		} else if (data <= 3731 ) {
			break;
		}
		pr_info("%s : DATA(%d) ERROR!! I2C READ RETRY(%d)\n",
			__func__, data, i);
	}

	temp = data - 2731;
	pr_info("%s : DATA(%d) TEMP(%d) I2C_READ_COUNT(%d)\n",
		__func__, data, temp, i);

	return temp;
}

static int smart_fg_read_soc(struct smart_fuelgauge *fuelgauge)
{
	int data = 0;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_RELATIVE_SOC);
	if (data < 0) {
		pr_info("%s READ SOC Failed!!!\n|", __func__);
		fuelgauge->initial_update_of_soc = true;
	}

	pr_info("%s : FG_SOC(%d)\n", __func__, data);
	return data;
}

#if 0
static int smart_fg_read_absolute_soc(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_ABSOLUTE_SOC);
	return data;
}

static int smart_fg_read_fullcap(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_FULLCAP);
	return data;
}

static int smart_fg_read_remcap(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_REMCAP);
	return data;
}
#endif


static int smart_fg_read_chg_current(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_CHG_CURR);
	pr_info("%s : CHG CURR (%d) \n", __func__, data);

	return data;
}

static int smart_fg_read_chg_voltage(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_CHG_VOLT);
	pr_info("%s : CHG VOLT (%d) \n", __func__, data);

	return data;
}

static int smart_fg_read_current(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_CURR);

	if (data & (0x1 << 15)) {
		data = (~data & 0xFFFF) + 1;
		data *= -1;
	}

	pr_info("%s :: CURR(%d)\n", __func__, data);

	return data;
}

static int smart_fg_read_avg_current(struct smart_fuelgauge *fuelgauge)
{
	u32 data;

	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_AVG_CURR);

	if (data & (0x1 << 15)) {
		data = (~data & 0xFFFF) + 1;
		data *= -1;
	}

	return data;
}

#if 0
static int smart_fg_read_cycle(struct smart_fuelgauge *fuelgauge)
{
	u32 data;
	data = smart_fg_read_word(fuelgauge->i2c, SMARTFG_CYCLE_CNT);
	return data;
}
#endif

static void smart_fg_get_scaled_capacity(struct smart_fuelgauge *fuelgauge,
					 union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 100 /
		     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_info("%s: scaled capacity (%d)\n", __func__, val->intval);
}

/* capacity is integer */
static void smart_fg_get_atomic_capacity(struct smart_fuelgauge *fuelgauge,
					 union power_supply_propval *val)
{

	pr_info("%s : NOW(%d), OLD(%d)\n",
		__func__, val->intval, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
	if (fuelgauge->capacity_old < val->intval)
		val->intval = fuelgauge->capacity_old + 1;
	else if (fuelgauge->capacity_old > val->intval)
		val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type & 
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s: capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int smart_fg_calculate_dynamic_scale(struct smart_fuelgauge *fuelgauge, int capacity)
{
	int soc_val;
	soc_val = smart_fg_read_soc(fuelgauge);

	if (soc_val <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		pr_debug("%s: capacity_max (%d)", __func__,
			 fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(soc_val >
			fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) :
			soc_val;
		pr_debug("%s: raw soc (%d)", __func__,
			 fuelgauge->capacity_max);
	}

	if (capacity != 100) {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 100 / capacity);
		fuelgauge->capacity_old = capacity;
	} else {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 99 / 100);
		fuelgauge->capacity_old = 100;
	}

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);

	return fuelgauge->capacity_max;
}

static int smart_fg_age_forecast(struct smart_fuelgauge *fuelgauge)
{
	int designcap, fullcap, temp;

	designcap = smart_fg_read_designcap(fuelgauge);
	fullcap = smart_fg_read_fullcap(fuelgauge);

	temp = (fullcap * 100) / designcap;
	if ((temp < 0) || (temp > 100))
		temp = 100;

	pr_info ("%s : %d = FULLCAP(%d) / DESIGNCAP(%d) * 100\n",
		 __func__, temp, fullcap, designcap);

	return temp;
}

static int smart_fg_check_capacity_max(struct smart_fuelgauge *fuelgauge, int capacity_max)
{
	int new_capacity_max = capacity_max;

	if (new_capacity_max < (fuelgauge->pdata->capacity_max -
				fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin);

		pr_info("%s: set capacity max(%d --> %d)\n",
			__func__, capacity_max, new_capacity_max);
	} else if (new_capacity_max > (fuelgauge->pdata->capacity_max +
				       fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin);

		pr_info("%s: set capacity max(%d --> %d)\n",
			__func__, capacity_max, new_capacity_max);
	}

	return new_capacity_max;
}

static int smart_fg_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct smart_fuelgauge *fuelgauge =
		container_of(psy, struct smart_fuelgauge, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smart_fg_read_status(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = smart_fg_age_forecast(fuelgauge);
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = smart_fg_read_vcell(fuelgauge);
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CURRENT_UA:
			val->intval = smart_fg_read_current(fuelgauge) * 1000;
			break;
		case SEC_BATTERY_CURRENT_MA:
		default:
			val->intval = smart_fg_read_current(fuelgauge);
			break;
		}
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		switch (val->intval) {
		case SEC_BATTERY_CURRENT_UA:
			val->intval = smart_fg_read_avg_current(fuelgauge) * 1000;
			break;
		case SEC_BATTERY_CURRENT_MA:
		default:
			val->intval = smart_fg_read_avg_current(fuelgauge);
			break;
		}
		break;
		/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smart_fg_read_soc(fuelgauge);

		fuelgauge->capacity = val->intval;

		if (fuelgauge->pdata->capacity_calculation_type &
		    (SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		     SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
			smart_fg_get_scaled_capacity(fuelgauge, val);

		/* capacity should be between 0% and 100% */
		if (val->intval > 100)
			val->intval = 100;
		if (val->intval < 0)
			val->intval = 0;

		/* (Only for atomic capacity)
		 * In initial time, capacity_old is 0.
		 * and in resume from sleep,
		 * capacity_old is too different from actual soc.
		 * should update capacity_old
		 * by val->intval in booting or resume.
		 */
		if (fuelgauge->initial_update_of_soc) {
			/* updated old capacity */
			fuelgauge->capacity_old = val->intval;
			fuelgauge->initial_update_of_soc = false;
			break;
		}

		if (fuelgauge->pdata->capacity_calculation_type &
		    (SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
		     SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
			smart_fg_get_atomic_capacity(fuelgauge, val);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = smart_fg_read_temp(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
	{
		int fullcap = smart_fg_read_fullcap(fuelgauge);
		int designcap = smart_fg_read_designcap(fuelgauge);
		val->intval = fullcap * 100 / designcap;
		pr_info("%s : asoc(%d), fullcap(%d), designcap(%d)\n",
			__func__, val->intval, fullcap, designcap);
	}
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		break;
	case POWER_SUPPLY_PROP_CHG_CURRENT:
		val->intval = smart_fg_read_chg_current(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_MAX_VOLTAGE:
		val->intval = smart_fg_read_chg_voltage(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smart_fg_read_firmware(fuelgauge);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int smart_fg_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct smart_fuelgauge *fuelgauge =
		container_of(psy, struct smart_fuelgauge, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			smart_fg_calculate_dynamic_scale(fuelgauge, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			fuelgauge->is_charging = false;
		} else {
			fuelgauge->is_charging = true;
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	case POWER_SUPPLY_PROP_TEMP:
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		if (val->intval > 100) {
			pr_info("%s: capacity_max changed, %d -> 100\n",
				__func__, fuelgauge->capacity_max);
			fuelgauge->capacity_max = 100;
		} else {
			pr_info("%s: capacity_max changed, %d -> %d\n",
				__func__, fuelgauge->capacity_max, val->intval);
			fuelgauge->capacity_max = smart_fg_check_capacity_max(fuelgauge, val->intval);
		}
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_CHG_CURRENT:
		break;
	case POWER_SUPPLY_PROP_MAX_VOLTAGE:
	case POWER_SUPPLY_PROP_PRESENT:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int smart_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
	struct smart_fuelgauge *fuelgauge = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SMART FUELGAUGE IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0; reg < 0x1D; reg++) {
		reg_data = smart_fg_read_word(fuelgauge->i2c, reg);
		seq_printf(s, "0x%02x:\t0x%04x\n", reg, reg_data);
	}

	for (reg = 0x20; reg < 0x24; reg++) {
		reg_data = smart_fg_read_word(fuelgauge->i2c, reg);
		seq_printf(s, "0x%02x:\t0x%04x\n", reg, reg_data);
	}

	for (reg = 0x3C; reg < 0x40; reg++) {
		reg_data = smart_fg_read_word(fuelgauge->i2c, reg);
		seq_printf(s, "0x%02x:\t0x%04x\n", reg, reg_data);
	}

	reg = 0x90;
	smart_fg_read_word(fuelgauge->i2c, reg);
	seq_printf(s, "0x%02x:\t0x%04x\n", reg, reg_data);

	seq_printf(s, "\n");
	return 0;
}

static int smart_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, smart_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations smart_fuelgauge_debugfs_fops = {
	.open           = smart_fuelgauge_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_OF
static int smart_fuelgauge_parse_dt(struct smart_fuelgauge *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "smart-fuelgauge");
	sec_fuelgauge_platform_data_t *pdata = fuelgauge->pdata;
	int ret;
	int i, len;
	const u32 *p;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&pdata->capacity_max);
		if (ret < 0) {
			fuelgauge->pdata->capacity_max = 100;
			pr_err("%s error reading capacity_max %d\n", __func__, ret);
		}

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		np = of_find_node_by_name(NULL, "battery");
		ret = of_property_read_u32(np, "battery,thermal_source",
					   &pdata->thermal_source);
		if (ret < 0) {
			pr_err("%s error reading pdata->thermal_source %d\n",
			       __func__, ret);
		}

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
						  GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				 "battery,input_current_limit", i,
				 &pdata->charging_current[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				 "battery,fast_charging_current", i,
				 &pdata->charging_current[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_1st", i,
				 &pdata->charging_current[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_2nd", i,
				 &pdata->charging_current[i].full_check_current_2nd);
		}
	}

	return 0;
}
#endif

static int __devinit smart_fuelgauge_probe(struct i2c_client *client,
					   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smart_fuelgauge *fuelgauge;
	int soc_val;
	int ret = 0;

	pr_info("%s: smart Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->i2c_lock);

	fuelgauge->i2c = client;

	if (client->dev.of_node) {
		fuelgauge->pdata = devm_kzalloc(&client->dev, sizeof(*(fuelgauge->pdata)),
					      GFP_KERNEL);
		if (!fuelgauge->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
#if defined(CONFIG_OF)
		ret = smart_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0) {
			pr_err("%s not found charger dt! ret[%d]\n",
			       __func__, ret);
			goto err_parse_dt;
		}
#endif
	}

	i2c_set_clientdata(client, fuelgauge);

	fuelgauge->psy_fg.name		= "smart-fuelgauge";
	fuelgauge->psy_fg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property	= smart_fg_get_property;
	fuelgauge->psy_fg.set_property	= smart_fg_set_property;
	fuelgauge->psy_fg.properties	= smart_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
		ARRAY_SIZE(smart_fuelgauge_props);

	if (fuelgauge->pdata->capacity_max > 100)
		fuelgauge->pdata->capacity_max = 100;

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	soc_val = smart_fg_read_soc(fuelgauge);

	if (soc_val > fuelgauge->capacity_max)
		smart_fg_calculate_dynamic_scale(fuelgauge, 100);

	(void) debugfs_create_file("smart-fuelgauge-regs",
		S_IRUGO, NULL, (void *)fuelgauge, &smart_fuelgauge_debugfs_fops);

	ret = power_supply_register(&client->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: smart Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_data_free:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->i2c_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id smart_fuelgauge_id[] = {
	{"smart-fuelgauge", 0},
	{}
};

static int smart_fuelgauge_remove(struct i2c_client *client)
{
	return 0;
}

#if defined CONFIG_PM
static int smart_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int smart_fuelgauge_resume(struct device *dev)
{
	return 0;
}
#else
#define bq24773_charger_suspend NULL
#define bq24773_charger_resume NULL
#endif

static void smart_fuelgauge_shutdown(struct i2c_client *client)
{
}

static SIMPLE_DEV_PM_OPS(smart_fuelgauge_pm_ops, smart_fuelgauge_suspend,
			 smart_fuelgauge_resume);

static struct i2c_driver smart_fuelgauge_driver = {
	.driver = {
		   .name = "smart-fuelgauge",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &smart_fuelgauge_pm_ops,
#endif
	},
	.probe	= smart_fuelgauge_probe,
	.remove	= smart_fuelgauge_remove,
	.shutdown = smart_fuelgauge_shutdown,
	.id_table = smart_fuelgauge_id,
};

static int __init smart_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return i2c_add_driver(&smart_fuelgauge_driver);

}

static void __exit smart_fuelgauge_exit(void)
{
	i2c_del_driver(&smart_fuelgauge_driver);
}

module_init(smart_fuelgauge_init);
module_exit(smart_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung SMART Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
