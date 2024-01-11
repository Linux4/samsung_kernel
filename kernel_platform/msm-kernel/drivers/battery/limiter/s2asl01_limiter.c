/*
 *  s2asl01_limiter.c
 *  Samsung S2ASL01 Limitter IC driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "s2asl01_limiter.h"
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>

static void s2asl01_init_regs(struct s2asl01_limiter_data *limiter);

static enum power_supply_property s2asl01_main_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
};

static enum power_supply_property s2asl01_sub_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
};

static struct device_attribute s2asl01_limiter_attrs[] = {
	S2ASL01_LIMITER_ATTR(limiter_data),
};

static int s2asl01_write_reg(struct i2c_client *client, int reg, u8 data)
{
	struct s2asl01_limiter_data *limiter = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&limiter->i2c_lock);
	ret = i2c_smbus_write_byte_data(client, reg, data);
	mutex_unlock(&limiter->i2c_lock);
	if (ret < 0)
		pr_info("%s [%s] : reg(0x%x), ret(%d)\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], reg, ret);
	return ret;
}

static int s2asl01_read_reg(struct i2c_client *client, int reg, void *data)
{
	struct s2asl01_limiter_data *limiter = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&limiter->i2c_lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&limiter->i2c_lock);
	if (ret < 0) {
		pr_info("%s [%s] : reg(0x%x), ret(%d)\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], reg, ret);
		return ret;
	}
	ret &= 0xff;
	*(u8 *)data = (u8)ret;

	return ret;
}

static int s2asl01_update_reg(struct i2c_client *client, u8 reg, u8 val, u8 mask)
{
	struct s2asl01_limiter_data *limiter = i2c_get_clientdata(client);
	int ret = 0;
	u8 old_val = 0, new_val = 0;

	mutex_lock(&limiter->i2c_lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(client, reg, new_val);
	}
	mutex_unlock(&limiter->i2c_lock);
	return ret;
}

static void s2asl01_test_read(struct i2c_client *client)
{
	struct s2asl01_limiter_data *limiter = i2c_get_clientdata(client);
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0;

	/* read address 0x00 ~ 0x36, 0x20 ~ 0x36 are hidden registers */
	for (i = 0x0; i <= 0x36; i++) {
		s2asl01_read_reg(limiter->client, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	pr_info("%s [%s]: %s\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], str);
}

static bool s2asl01_check_status(struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	bool ret = true;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_PM_ENABLE, &data);
	pr_info("%s [%s]: 0x%02x\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], data);
	data &= 0xF0;

	if (!data) {
		pr_info("%s [%s]: caution! powermeter disabled\n", __func__,
		current_limiter_type_str[limiter->pdata->bat_type]);
		s2asl01_init_regs(limiter);
		ret = false;
	}
	pr_info("%s [%s]: enb = %d, det = %d\n", __func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		gpio_get_value(limiter->pdata->bat_enb), gpio_get_value(limiter->pdata->bat_det));

	return ret;
}

static void s2asl01_set_in_ok(struct s2asl01_limiter_data *limiter, bool onoff)
{
	pr_info("%s[%s]: INOK = %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], onoff);
	if (onoff)
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			1 << S2ASL01_CTRL3_INOK_SHIFT, S2ASL01_CTRL3_INOK_MASK);
	else
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			0 << S2ASL01_CTRL3_INOK_SHIFT, S2ASL01_CTRL3_INOK_MASK);
}

static void s2asl01_set_supplement_mode(struct s2asl01_limiter_data *limiter, bool onoff)
{
	pr_info("%s[%s]: SUPPLEMENT MODE = %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], onoff);

	if (onoff) {
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			1 << S2ASL01_CTRL3_SUPLLEMENTMODE_SHIFT,
				S2ASL01_CTRL3_SUPLLEMENTMODE_MASK);
		limiter->supplement_mode = true;
	} else {
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			0 << S2ASL01_CTRL3_SUPLLEMENTMODE_SHIFT, S2ASL01_CTRL3_SUPLLEMENTMODE_MASK);
		limiter->supplement_mode = false;
	}
}

static void s2asl01_set_recharging_start(struct s2asl01_limiter_data *limiter)
{
	pr_info("%s: INOK = %d, SUPLLEMENT MODE = %d\n", __func__,
			limiter->in_ok, limiter->supplement_mode);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL1,
			1 << S2ASL01_CTRL1_RESTART_SHIFT, S2ASL01_CTRL1_RESTART_MASK);
}

static void s2asl01_set_eoc_on(struct s2asl01_limiter_data *limiter)
{
	pr_info("%s[%s]: INOK = %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], limiter->in_ok);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL1,
			1 << S2ASL01_CTRL1_EOC_SHIFT, S2ASL01_CTRL1_EOC_MASK);
}

static void s2asl01_set_dischg_mode(struct s2asl01_limiter_data *limiter, int chg_mode)
{
	u8 val = 0;
	int ret = 0;

	switch (chg_mode) {
	case CURRENT_REGULATION:
	case CURRENT_VOLTAGE_REGULATION:
	case NO_REGULATION_FULLY_ON:
		val = chg_mode;
		break;
	case CURRENT_SMARTER_VOLTAGE_REGULATION:
		pr_info("%s: chg_mode(%d) changes to CURRENT_VOLTAGE_REGULATION\n",
			__func__, chg_mode);
		val = CURRENT_VOLTAGE_REGULATION;
		break;
	default:
		pr_info("%s: wrong input(%d)\n", __func__, chg_mode);
	}

	ret = s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			val << S2ASL01_CTRL3_DIS_MODE_SHIFT, S2ASL01_CTRL3_DIS_MODE_MASK);
	if (ret < 0)
		pr_err("%s, i2c read fail\n", __func__);

	pr_info("%s, set discharge mode (%d)\n", __func__, chg_mode);
}

static int s2asl01_get_dischg_mode(struct s2asl01_limiter_data *limiter)
{
	u8 val = 0, chg_mode = 0;
	int ret = 0;

	ret = s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3, &val);
	chg_mode = (val & S2ASL01_CTRL3_DIS_MODE_MASK) >> S2ASL01_CTRL3_DIS_MODE_SHIFT;

	switch (chg_mode) {
	case CURRENT_REGULATION:
	case CURRENT_VOLTAGE_REGULATION:
	case NO_REGULATION_FULLY_ON:
		return chg_mode;
	case CURRENT_SMARTER_VOLTAGE_REGULATION:
		pr_info("%s: chg_mode(%d) CURRENT_VOLTAGE_REGULATION\n",
			__func__, chg_mode);
		return CURRENT_VOLTAGE_REGULATION;
	default:
		return -EINVAL;
	}
}

static void s2asl01_set_chg_mode(struct s2asl01_limiter_data *limiter, int chg_mode)
{
	u8 val = 0;
	int ret = 0;

	switch (chg_mode) {
	case CURRENT_REGULATION:
	case CURRENT_VOLTAGE_REGULATION:
	case CURRENT_SMARTER_VOLTAGE_REGULATION:
	case NO_REGULATION_FULLY_ON:
		val = chg_mode;
		break;
	default:
		pr_info("%s: wrong input(%d)\n", __func__, chg_mode);
	}

	ret = s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3,
			val << S2ASL01_CTRL3_CHG_MODE_SHIFT, S2ASL01_CTRL3_CHG_MODE_MASK);
	if (ret < 0)
		pr_err("%s, i2c read fail\n", __func__);

	pr_info("%s, set charge mode (%d)\n", __func__, chg_mode);
}

static int s2asl01_get_chg_mode(struct s2asl01_limiter_data *limiter)
{
	u8 val = 0, chg_mode = 0;
	int ret = 0;

	ret = s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL3, &val);
	chg_mode = (val & S2ASL01_CTRL3_CHG_MODE_MASK) >> S2ASL01_CTRL3_CHG_MODE_SHIFT;

	switch (chg_mode) {
	case CURRENT_REGULATION:
	case CURRENT_VOLTAGE_REGULATION:
	case CURRENT_SMARTER_VOLTAGE_REGULATION:
	case NO_REGULATION_FULLY_ON:
		return chg_mode;
	default:
		return -EINVAL;
	}
}

static int s2asl01_get_vchg(struct s2asl01_limiter_data *limiter, int mode)
{
	u8 data[2] = {0, };
	u16 temp = 0;
	u32 vchg = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL1_VCHG, &data[0]);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL2_VCHG, &data[1]);

	temp = (data[0] << 4) | ((data[1] & 0xF0) >> 4);
	temp &= 0x0FFF;
	vchg = temp * 1250;

	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vchg = vchg / 1000;

	pr_debug("%s [%s]: vchg = %d %s\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		vchg,
		(mode == SEC_BATTERY_VOLTAGE_UV) ? "uV" : "mV");

	return vchg;
}

static int s2asl01_get_vbat(struct s2asl01_limiter_data *limiter, int mode)
{
	u8 data[2] = {0, };
	u16 temp = 0;
	u32 vbat = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL1_VBAT, &data[0]);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL2_VBAT, &data[1]);

	temp = (data[0] << 4) | ((data[1] & 0xF0) >> 4);
	temp &= 0x0FFF;
	vbat = temp * 1250;

	if (mode == SEC_BATTERY_VOLTAGE_MV)
		vbat = vbat / 1000;

	pr_debug("%s [%s]: vbat = %d %s\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		vbat,
		(mode == SEC_BATTERY_VOLTAGE_UV) ? "uV" : "mV");

	s2asl01_check_status(limiter);

	return vbat;
}

static int s2asl01_get_ichg(struct s2asl01_limiter_data *limiter, int mode)
{
	u8 data[2] = {0, };
	u16 temp = 0;
	u32 ichg = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL1_ICHG, &data[0]);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL2_ICHG, &data[1]);

	temp = (data[0] << 4) | ((data[1] & 0xF0) >> 4);
	temp &= 0x0FFF;
	ichg = (temp * 15625) / 10;

	if (mode == SEC_BATTERY_CURRENT_MA)
		ichg = ichg / 1000;

	pr_debug("%s [%s]: Ichg = %d %s\n", __func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		ichg,
		(mode == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");

	return ichg;
}

static int s2asl01_get_idischg(struct s2asl01_limiter_data *limiter, int mode)
{
	u8 data[2] = {0, };
	u16 temp = 0;
	u32 idischg = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL1_IDISCHG, &data[0]);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_VAL2_IDISCHG, &data[1]);

	temp = (data[0] << 4) | ((data[1] & 0xF0) >> 4);
	temp &= 0x0FFF;
	idischg = (temp * 15625) / 10;

	if (mode == SEC_BATTERY_CURRENT_MA)
		idischg = idischg / 1000;

	pr_debug("%s [%s]: IDISCHG = %d %s\n", __func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		idischg,
		(mode == SEC_BATTERY_CURRENT_UA) ? "uA" : "mA");

	return idischg;
}

static unsigned int s2asl01_check_health(struct s2asl01_limiter_data *limiter)
{
	int vchg, vbat, ichg, idischg;
	u8 pm_enable = 0;
	int ret = 0;

	ret = s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_PM_ENABLE, &pm_enable);
	pm_enable &= 0xF0;

	/* do not check limiter health when powermeter is abnormal or i2c fail  */
	if (!pm_enable || ret < 0)
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;

	vchg = s2asl01_get_vchg(limiter, SEC_BATTERY_VOLTAGE_MV);
	vbat = s2asl01_get_vbat(limiter, SEC_BATTERY_VOLTAGE_MV);
	ichg = s2asl01_get_ichg(limiter, SEC_BATTERY_CURRENT_MA);
	idischg = s2asl01_get_idischg(limiter, SEC_BATTERY_CURRENT_MA);

	pr_info("%s [%s]: vchg=%dmV, vbat=%dmV, ichg=%dmA, idischg=%dmA\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		vchg, vbat, ichg, idischg);

	if (vchg && vbat)
		return POWER_SUPPLY_HEALTH_GOOD;

	return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
}

static void s2asl01_set_fast_charging_current_limit(
		struct s2asl01_limiter_data *limiter, int charging_current)
{
	u8 data = 0;
	int dest_current = 0;

	if (limiter->rev_id == 0 || limiter->ic_ver == VER_6130) {
		if (charging_current <= 50)
			data = 0x00;
		else if (charging_current > 50 && charging_current <= 3200)
			data = (charging_current / 50) - 1;
		else
			data = 0x3F;
		dest_current = (data + 1) * 50;
	} else {
		if (charging_current <= 50)
			data = 0x00;
		else if (charging_current > 50 && charging_current <= 350)
			data = (charging_current - 50) / 75;
		else if (charging_current > 350 && charging_current <= 3300)
			data = ((charging_current - 350) / 50) + 4;
		else
			data = 0x3F;

		if ((data >= 0x00) && (data <= 0x04))
			dest_current = (data * 75) + 50;
		else
			dest_current = ((data - 4) * 50) + 350;
	}

	pr_info("%s [%s]: current %d -> %d, 0x%02x\n",
		__func__, current_limiter_type_str[limiter->pdata->bat_type], charging_current, dest_current, data);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL4,
			data, FCC_CHG_CURRENTLIMIT_MASK);
}

static int s2asl01_get_fast_charging_current_limit(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int charging_current = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL4, &data);

	data = data & FCC_CHG_CURRENTLIMIT_MASK;

	if (data > 0x3F) {
		pr_err("%s: Invalid fast charging current limit value\n", __func__);
		data = 0x3F;
	}

	if (limiter->rev_id == 0 || limiter->ic_ver == VER_6130)
		charging_current = (data + 1) * 50;
	else {
		if ((data >= 0x00) && (data <= 0x04))
			charging_current = (data * 75) + 50;
		else
			charging_current = ((data - 4) * 50) + 350;
	}

	return charging_current;
}

static void s2asl01_set_trickle_charging_current_limit(
		struct s2asl01_limiter_data *limiter, int charging_current)
{
	u8 data = 0;

	if (limiter->ic_ver == VER_6130) {
		if (charging_current <= 50)
			data = 0x00;
		else if (charging_current > 50 && charging_current <= 550)
			data = (charging_current / 50) - 1;
		else
			data = 0x0A;
	} else {
		if (limiter->rev_id == 0) {
			if (charging_current <= 50)
				data = 0x00;
			else if (charging_current > 50 && charging_current <= 500)
				data = (charging_current / 50) - 1;
			else
				data = 0x09;
		} else {
			if (charging_current <= 50)
				data = 0x00;
			else if (charging_current > 50 && charging_current <= 350)
				data = (charging_current - 50) / 75;
			else if (charging_current > 350 && charging_current <= 550)
				data = ((charging_current - 350) / 50) + 4;
			else
				data = 0x08;
		}
	}

	pr_info("%s [%s]: current %d, 0x%02x\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], charging_current, data);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL5,
			data, TRICKLE_CHG_CURRENT_LIMIT_MASK);
}

static int s2asl01_get_trickle_charging_current_limit(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int charging_current = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL5, &data);

	data = data & TRICKLE_CHG_CURRENT_LIMIT_MASK;

	if (limiter->ic_ver == VER_6130) {
		if (data > 0x0A) {
			pr_err("%s: Invalid trickle charging current limit value\n", __func__);
			data = 0x0A;
		}
		charging_current = (data + 1) * 50;
	} else {
		if (limiter->rev_id == 0) {
			if (data > 0x09) {
				pr_err("%s: Invalid trickle charging current limit value\n", __func__);
				data = 0x09;
			}
			charging_current = (data + 1) * 50;
		} else {
			if (data > 0x08) {
				pr_err("%s: Invalid trickle charging current limit value\n", __func__);
				data = 0x08;
			}
			if ((data >= 0x00) && (data <= 0x04))
				charging_current = (data * 75) + 50;
			else
				charging_current = ((data - 4) * 50) + 350;
		}
	}

	return charging_current;
}

static void s2asl01_set_dischg_charging_current_limit(
		struct s2asl01_limiter_data *limiter, int charging_current)
{
	u8 data = 0;

	if (limiter->rev_id == 0) {
		if (charging_current <= 50)
			data = 0x00;
		else if (charging_current > 50 && charging_current <= 6400)
			data = (charging_current / 50) - 1;
		else
			data = 0x7F;
	} else {
		if (charging_current <= 50)
			data = 0x00;
		else if (charging_current > 50 && charging_current <= 350)
			data = (charging_current - 50) / 75;
		else if (charging_current > 350 && charging_current <= 6500)
			data = ((charging_current - 350) / 50) + 4;
		else
			data = 0x7F;
	}
	pr_info("%s [%s]: current %d, 0x%02x\n",
		__func__, current_limiter_type_str[limiter->pdata->bat_type], charging_current, data);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL6,
			data, DISCHG_CURRENT_LIMIT_MASK);
}

static int s2asl01_get_dischg_charging_current_limit(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int dischg_current = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL6, &data);

	data = data & DISCHG_CURRENT_LIMIT_MASK;

	if (data > 0x7F) {
		pr_err("%s: Invalid discharging current limit value\n", __func__);
		data = 0x7F;
	}

	if (limiter->rev_id == 0)
		dischg_current = (data + 1) * 50;
	else {
		if ((data >= 0x00) && (data <= 0x04))
			dischg_current = (data * 75) + 50;
		else
			dischg_current = ((data - 4) * 50) + 350;
	}

	return dischg_current;
}

static void s2asl01_set_recharging_voltage(
		struct s2asl01_limiter_data *limiter, int charging_voltage)
{
	u8 data = 0;

	if (charging_voltage < 4000)
		data = 0x90;
	else if (charging_voltage >= 4000 && charging_voltage < 4400)
		data = (charging_voltage - 2560) / 10;
	else
		data = 0xB8;

	pr_info("%s: voltage %d, 0x%02x\n", __func__, charging_voltage, data);

	s2asl01_write_reg(limiter->client, S2ASL01_LIMITER_TOP_RECHG_CTRL1, data);
}

static int s2asl01_get_recharging_voltage(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int charging_voltage = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_TOP_RECHG_CTRL1, &data);
	charging_voltage = (data * 10) + 2560;

	return charging_voltage;
}

static void s2asl01_set_eoc_voltage(
		struct s2asl01_limiter_data *limiter, int charging_voltage)
{
	u8 data = 0;

	if (charging_voltage < 4000)
		data = 0x90;
	else if (charging_voltage >= 4000 && charging_voltage < 4400)
		data = (charging_voltage - 2560) / 10;
	else
		data = 0xB8;

	pr_info("%s [%s]: voltage %d, 0x%02x\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], charging_voltage, data);

	s2asl01_write_reg(limiter->client, S2ASL01_LIMITER_TOP_EOC_CTRL1, data);
}

static int s2asl01_get_eoc_voltage(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int charging_voltage = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_TOP_EOC_CTRL1, &data);
	charging_voltage = (data * 10) + 2560;

	return charging_voltage;
}

static void s2asl01_set_eoc_current(
		struct s2asl01_limiter_data *limiter, int charging_current)
{
	u8 data = 0;

	if (charging_current < 100)
		data = 0x04;
	else if (charging_current >= 100 && charging_current < 775)
		data = charging_current / 25;
	else
		data = 0x1F;

	pr_info("%s [%s]: current %d, 0x%02x\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], charging_current, data);

	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_TOP_EOC_CTRL2, data, 0x1F);
}

static int s2asl01_get_eoc_current(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;
	int charging_current = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_TOP_EOC_CTRL2, &data);
	data &= 0x1F;
	charging_current = data * 25;

	return charging_current;
}

static void s2asl01_powermeter_onoff(
		struct s2asl01_limiter_data *limiter, bool onoff)
{
	u8 data = 0;
	pr_info("%s [%s]: (%d)\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], onoff);

	/* Power Meter Continuous Operation Mode
	   [7]: VCHG
	   [6]: VBAT
	   [5]: ICHG
	   [4]: IDISCHG */
	if (onoff) {
		s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_PM_ENABLE, &data);
		data &= 0xF0;
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_PM_ENABLE, 0xF0, 0xF0);
		limiter->power_meter = true;
		if (!data) {
			/* To guarantee correct voltage measurement after turn on powermeter */
			msleep(100);
		}
	} else {
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_PM_ENABLE, 0x00, 0xF0);
		limiter->power_meter = false;
	}
}

static bool s2asl01_get_poweroff_mode2(
		struct s2asl01_limiter_data *limiter)
{
	u8 data = 0;

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL2, &data);
	data &= S2ASL01_SUB_PWR_OFF_MODE_MASK;

	pr_info("%s [%s]: mode = %d [0 : mode2, 1: mode 1]\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], data);

	if (!data)
		return true;
	else
		return false;
}

static void s2asl01_set_poweroff_mode2(
		struct s2asl01_limiter_data *limiter, bool en)
{
	pr_info("%s [%s]: (%d)\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], en);

	if (en) {
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL2,
			S2ASL01_SUB_PWR_OFF_MODE2, S2ASL01_SUB_PWR_OFF_MODE_MASK);		
	} else {
		s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL2,
			S2ASL01_SUB_PWR_OFF_MODE1, S2ASL01_SUB_PWR_OFF_MODE_MASK);
	}
}

static void s2asl01_tsd_onoff(
		struct s2asl01_limiter_data *limiter, bool onoff)
{
	pr_info("%s(%d)\n", __func__, onoff);

	if (limiter->rev_id == 0) {
		if (onoff) {
			s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_COMMON1,
					S2ASL01_COMMON1_CM_TSD_EN, S2ASL01_COMMON1_CM_TSD_EN);
			limiter->tsd = true;
		} else {
			s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_COMMON1,
					0, S2ASL01_COMMON1_CM_TSD_EN);
			limiter->tsd = false;
		}
	}
}

static int s2asl01_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2asl01_limiter_data *limiter =
					power_supply_get_drvdata(psy);

	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2asl01_check_health(limiter);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHGIN_OK:
			val->intval = limiter->in_ok ? 1 : 0;
			break;
		case POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE:
			val->intval = limiter->supplement_mode ? 1 : 0;
			break;
		case POWER_SUPPLY_EXT_PROP_DISCHG_MODE:
			val->intval = s2asl01_get_dischg_mode(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_MODE:
			val->intval = s2asl01_get_chg_mode(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_VOLTAGE:
			val->intval = s2asl01_get_vchg(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE:
			val->intval = s2asl01_get_vbat(limiter, val->intval);
			s2asl01_test_read(limiter->client);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_CURRENT:
			val->intval = s2asl01_get_ichg(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT:
			val->intval = s2asl01_get_idischg(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT:
			val->intval = s2asl01_get_fast_charging_current_limit(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_TRICKLECHG_LIMIT_CURRENT:
			val->intval = s2asl01_get_trickle_charging_current_limit(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_DISCHG_LIMIT_CURRENT:
			val->intval = s2asl01_get_dischg_charging_current_limit(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_RECHG_VOLTAGE:
			val->intval = s2asl01_get_recharging_voltage(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_EOC_VOLTAGE:
			val->intval = s2asl01_get_eoc_voltage(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_EOC_CURRENT:
			val->intval = s2asl01_get_eoc_current(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_POWERMETER_ENABLE:
			val->intval = limiter->power_meter;
			break;
		case POWER_SUPPLY_EXT_PROP_POWER_MODE2:
			val->intval = s2asl01_get_poweroff_mode2(limiter);
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

static int s2asl01_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2asl01_limiter_data *limiter =
				power_supply_get_drvdata(psy);

	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pr_info("%s [%s]: is it full? %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], val->intval);
		s2asl01_set_supplement_mode(limiter, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		pr_info("%s [%s]: pwr off mode 2 %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], val->intval);
		if (val->intval)
			s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL2,
					S2ASL01_SUB_PWR_OFF_MODE2, S2ASL01_SUB_PWR_OFF_MODE_MASK);
		else
			s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_CTRL2,
					S2ASL01_SUB_PWR_OFF_MODE1, S2ASL01_SUB_PWR_OFF_MODE_MASK);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			pr_info("%s [%s]: chg en ? %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHGIN_OK:
			pr_info("%s [%s]: chgin_ok ? %d\n", __func__, current_limiter_type_str[limiter->pdata->bat_type], val->intval);
			limiter->in_ok = val->intval;
			s2asl01_set_in_ok(limiter, limiter->in_ok);
			break;
		case POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE:
			s2asl01_set_supplement_mode(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_RECHG_ON:
			s2asl01_set_recharging_start(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_EOC_ON:
			s2asl01_set_eoc_on(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_DISCHG_MODE:
			s2asl01_set_dischg_mode(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_MODE:
			s2asl01_set_chg_mode(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT:
			s2asl01_set_fast_charging_current_limit(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_TRICKLECHG_LIMIT_CURRENT:
			s2asl01_set_trickle_charging_current_limit(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DISCHG_LIMIT_CURRENT:
			s2asl01_set_dischg_charging_current_limit(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_RECHG_VOLTAGE:
			s2asl01_set_recharging_voltage(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_EOC_VOLTAGE:
			s2asl01_set_eoc_voltage(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_EOC_CURRENT:
			s2asl01_set_eoc_current(limiter, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_POWERMETER_ENABLE:
			s2asl01_init_regs(limiter);
			break;
		case POWER_SUPPLY_EXT_PROP_POWER_MODE2:
			s2asl01_set_poweroff_mode2(limiter, val->intval);
			s2asl01_test_read(limiter->client);
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

#if 0
static irqreturn_t s2asl01_irq_handler(int irq, void *irq_data)
{
	struct s2asl01_limiter_data *limiter = irq_data;
	u8 data1 = 0, data2 = 0, data3 = 0, data4 = 0;

	pr_info("%s\n[%s] : irq", __func__, current_limiter_type_str[limiter->pdata->bat_type]);

	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_INT1, &data1);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_INT2, &data2);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_PM_INT, &data3);
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_CORE_STATUS1, &data4);

	pr_info("%s[%s] : CORE_INT1 = %02x, CORE_INT2 = %02x, PM_INT = %02x, STATUS = %02x\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		data1, data2, data3, data4);

	pr_info("%s[%s] : vchg=%duV, vbat=%duV, ichg=%duA, idischg=%duA\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		s2asl01_get_vchg(limiter, SEC_BATTERY_VOLTAGE_MV),
		s2asl01_get_vbat(limiter, SEC_BATTERY_VOLTAGE_MV),
		s2asl01_get_ichg(limiter, SEC_BATTERY_CURRENT_MA),
		s2asl01_get_idischg(limiter, SEC_BATTERY_CURRENT_MA));

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_OF
static int s2asl01_limiter_parse_dt(struct device *dev, struct s2asl01_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s parsing start\n", __func__);

	if (np == NULL) {
		pr_err("%s np is NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "limiter,bat_type",
					&pdata->bat_type);
		if (ret < 0)
			pr_info("%s : bat_type is empty\n", __func__);

		if (pdata->bat_type & LIMITER_MAIN) {
			pr_info("%s : It is MAIN battery dt\n", __func__);

			ret = pdata->bat_det = of_get_named_gpio(np, "limiter,main_bat_det_gpio", 0);
			if (ret < 0) {
				dev_err(dev, "%s : can't main_bat_det_gpio\n", __func__);
				pdata->bat_det = 0;
			}

			ret = pdata->bat_enb = of_get_named_gpio(np, "limiter,main_bat_enb_gpio", 0);
			if (ret < 0) {
				dev_err(dev, "%s : can't main_bat_enb_gpio\n", __func__);
				pdata->bat_enb = 0;
			}
		} else if (pdata->bat_type & LIMITER_SUB) {
			pr_info("%s : It is SUB battery dt\n", __func__);

			ret = pdata->bat_det = of_get_named_gpio(np, "limiter,sub_bat_det_gpio", 0);
			if (ret < 0) {
				dev_err(dev, "%s : can't sub_bat_det_gpio\n", __func__);
				pdata->bat_det = 0;
			}

			ret = pdata->bat_enb = of_get_named_gpio(np, "limiter,sub_bat_enb_gpio", 0);
			if (ret < 0) {
				dev_err(dev, "%s : can't sub_bat_enb_gpio\n", __func__);
				pdata->bat_enb = 0;
			}
		}

		ret = of_property_read_string(np, "limiter,limiter_name",
			(char const **)&pdata->limiter_name);
		if (ret < 0) {
			pr_info("%s : Switching IC name is empty\n", __func__);
			pdata->limiter_name = "s2asl01-limiter";
		}

		ret = of_property_read_u32(np, "limiter,chg_current_limit",
					&pdata->chg_current_limit);
		if (ret < 0) {
			pr_info("%s : Chg current limit is empty\n", __func__);
			pdata->chg_current_limit = 1650;
		}

		ret = of_property_read_u32(np, "limiter,eoc",
					&pdata->eoc);
		if (ret < 0) {
			pr_info("%s : eoc is empty\n", __func__);
			pdata->eoc = 200; /* for interrupt setting, not used */
		}

		ret = of_property_read_u32(np, "limiter,float_voltage",
					&pdata->float_voltage);
		if (ret < 0) {
			pr_info("%s : float voltage is empty\n", __func__);
			pdata->float_voltage = 4350; /* for interrupt setting, not used */
		}

		ret = of_property_read_u32(np, "limiter,hys_vchg_level",
					&pdata->hys_vchg_level);
		if (ret < 0) {
			pr_info("%s : Hysteresis level is empty(vchg)\n", __func__);
			pdata->hys_vchg_level = 4; /* 250mV(default) */
		}

		ret = of_property_read_u32(np, "limiter,hys_vbat_level",
					&pdata->hys_vbat_level);
		if (ret < 0) {
			pr_info("%s : Hysteresis level is empty(vbat)\n", __func__);
			pdata->hys_vbat_level = 4; /* 250mV(default) */
		}

		ret = of_property_read_u32(np, "limiter,hys_ichg_level",
					&pdata->hys_ichg_level);
		if (ret < 0) {
			pr_info("%s : Hysteresis level is empty(ichg)\n", __func__);
			pdata->hys_ichg_level = 4; /* 500mA(default) */
		}

		ret = of_property_read_u32(np, "limiter,hys_idischg_level",
					&pdata->hys_idischg_level);
		if (ret < 0) {
			pr_info("%s : Hysteresis level is empty(idischg)\n", __func__);
			pdata->hys_idischg_level = 4; /* 500mA(default) */
		}

		ret = of_property_read_u32(np, "limiter,chg_current_fully_turnon",
					&pdata->chg_current_fully_turnon);
		if (ret < 0) {
			pr_info("%s : chg_current_fully_turnon is empty\n", __func__);
			pdata->chg_current_fully_turnon = 0;
		}

		pdata->tsd_en = (of_find_property(np, "limiter,tsd-en", NULL))
						? true : false;
	}

	pr_info("%s parsing end\n", __func__);
	return 0;
}

static const struct of_device_id s2asl01_limiter_match_table[] = {
		{ .compatible = "samsung,s2asl01-limiter",},
		{},
};
#else
static int s2asl01_limiter_parse_dt(struct device *dev, struct s2asl01_limiter_data *limiter)
{
	return 0;
}

#define s2asl01_limiter_match_table NULL
#endif /* CONFIG_OF */

static int s2asl01_limiter_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(s2asl01_limiter_attrs); i++) {
		rc = device_create_file(dev, &s2asl01_limiter_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &s2asl01_limiter_attrs[i]);
	return rc;
}

ssize_t s2asl01_limiter_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2asl01_limiter_data *limiter = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2asl01_limiter_attrs;
	int i = 0, j = 0;
	u8 data = 0;

	dev_info(limiter->dev, "%s\n", __func__);

	switch (offset) {
	case LIMITER_DATA:
		dev_info(limiter->dev, "%s\n", __func__);
		for (j = 0; j <= S2ASL01_LIMITER_PM_I_OPTION; j++) {
			s2asl01_read_reg(limiter->client, j, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"0x%02x:\t0x%02x\n", j, data);
		}

		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t s2asl01_limiter_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2asl01_limiter_data *limiter = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2asl01_limiter_attrs;
	int ret = 0;
	int x, y;

	dev_info(limiter->dev, "%s\n", __func__);

	switch (offset) {
	case LIMITER_DATA:
		dev_info(limiter->dev, "%s\n", __func__);
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			u8 addr = x;
			u8 data = y;

			if (s2asl01_write_reg(limiter->client, addr, data) < 0) {
				dev_info(limiter->dev,
					"%s: addr: 0x%x write fail\n", __func__, addr);
			}
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static void s2asl01_get_rev_id(struct s2asl01_limiter_data *limiter)
{
	u8 val1 = 0;

	/* rev ID */
	s2asl01_read_reg(limiter->client, S2ASL01_LIMITER_ID, &val1);
	if (val1 & 0x01)
		limiter->ic_ver = VER_6130;
	else
		limiter->ic_ver = VER_6030;

	limiter->es_num = (val1 & 0xC0) >> 6;
	limiter->rev_id = (val1 & 0x30) >> 4;

	pr_info("%s [%s]: rev id : %d, es_num = %d, ic_ver = %d\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		limiter->rev_id, limiter->es_num, limiter->ic_ver);
}

static void s2asl01_init_regs(struct s2asl01_limiter_data *limiter)
{
	u8 temp = 0;

	pr_err("%s: s2asl01 limiter initialize\n", __func__);

#if defined(CONFIG_SEC_FACTORY)
	if (limiter->pdata->bat_type & LIMITER_MAIN) {
		/* MAIN disable SUB_PWR_OFF_MODE */
		s2asl01_set_poweroff_mode2(limiter, 0);
	} else {
		/* SUB enable SUB_PWR_OFF_MODE */
		s2asl01_set_poweroff_mode2(limiter, 1);
	}
#else
	/* MAIN/SUB both disable SUB_PWR_OFF_MODE */
	s2asl01_set_poweroff_mode2(limiter, 0);
#endif
	s2asl01_powermeter_onoff(limiter, 1);

	/* chg mode setting */
	if (limiter->pdata->chg_current_fully_turnon)
		s2asl01_set_chg_mode(limiter, NO_REGULATION_FULLY_ON);
	else
		s2asl01_set_chg_mode(limiter, CURRENT_REGULATION);

	/* dischg mode setting */
	s2asl01_set_dischg_mode(limiter, NO_REGULATION_FULLY_ON);

	/* dischg current limit setting */
	s2asl01_set_dischg_charging_current_limit(limiter, 6400);

	/* fast chg current limit setting */
	s2asl01_set_fast_charging_current_limit(limiter, limiter->pdata->chg_current_limit);

	/* cutoff current setting */
	s2asl01_set_eoc_current(limiter, limiter->pdata->eoc);

	/* cutoff voltage setting */
	s2asl01_set_eoc_voltage(limiter, limiter->pdata->float_voltage);

	s2asl01_set_eoc_on(limiter);

	/* Power Meter Hysteresis Level */
	temp = (limiter->pdata->hys_vchg_level << 5) |
			(limiter->pdata->hys_vbat_level << 2);
	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_PM_HYST_LEVEL1, temp, 0xFC);

	temp = (limiter->pdata->hys_ichg_level << 5) |
			(limiter->pdata->hys_idischg_level << 2);
	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_PM_HYST_LEVEL2, temp, 0xFC);

	/* interrupt ALL UNMASK */
	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_INT1M,
				0, S2ASL01_SW_CORE1_INT_MASK);
	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_CORE_INT2M,
				0, S2ASL01_SW_CORE2_INT_MASK);
	s2asl01_update_reg(limiter->client, S2ASL01_LIMITER_PM_INT,
				0, S2ASL01_SW_PM_INT_MASK);

	/* TSD function */
	if (limiter->pdata->tsd_en)
		s2asl01_tsd_onoff(limiter, true);

	s2asl01_test_read(limiter->client);
}

static const struct power_supply_desc s2asl01_main_power_supply_desc = {
	.name = "s2asl01-limiter-main",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2asl01_main_props,
	.num_properties = ARRAY_SIZE(s2asl01_main_props),
	.get_property = s2asl01_get_property,
	.set_property = s2asl01_set_property,
	.no_thermal = true,
};

static const struct power_supply_desc s2asl01_sub_power_supply_desc = {
	.name = "s2asl01-limiter-sub",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2asl01_sub_props,
	.num_properties = ARRAY_SIZE(s2asl01_sub_props),
	.get_property = s2asl01_get_property,
	.set_property = s2asl01_set_property,
	.no_thermal = true,
};

static int s2asl01_limiter_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct s2asl01_limiter_data *limiter;
	struct s2asl01_platform_data *pdata = client->dev.platform_data;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	dev_err(&client->dev, "%s: S2ASL01 Switching Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = s2asl01_limiter_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	dev_err(&client->dev, "%s: %s det=%d\n", __func__, current_limiter_type_str[pdata->bat_type], gpio_get_value(pdata->bat_det));
#if defined(CONFIG_SEC_FACTORY)
	if (gpio_get_value(pdata->bat_det)) {
		dev_err(&client->dev, "%s is not connected, do not load limiter driver\n", current_limiter_type_str[pdata->bat_type]);
		ret = ENXIO;
		goto err_limiter_nomem;
	}
#endif

	limiter = kzalloc(sizeof(*limiter), GFP_KERNEL);
	if (limiter == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_limiter_nomem;
	}
	limiter->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(limiter->dev, "I2C functionality is not supported.\n");
		goto err_i2cfunc_not_support;
	}
	limiter->client = client;
	limiter->pdata = pdata;

	i2c_set_clientdata(client, limiter);
	mutex_init(&limiter->i2c_lock);

	if (limiter->pdata->bat_type & LIMITER_MAIN) {
		psy_cfg.drv_data = limiter;
		limiter->psy_sw =
		    power_supply_register(limiter->dev,
					  &s2asl01_main_power_supply_desc,
					  &psy_cfg);
		if (IS_ERR(limiter->psy_sw)) {
			ret = PTR_ERR(limiter->psy_sw);
			pr_err("%s: Failed to Register psy_sw(%d)\n", __func__, ret);
			goto err_supply_unreg;
		}
	} else if (limiter->pdata->bat_type & LIMITER_SUB) {
		psy_cfg.drv_data = limiter;
		limiter->psy_sw =
		    power_supply_register(limiter->dev,
					  &s2asl01_sub_power_supply_desc,
					  &psy_cfg);
		if (IS_ERR(limiter->psy_sw)) {
			ret = PTR_ERR(limiter->psy_sw);
			pr_err("%s: Failed to Register psy_sw(%d)\n", __func__, ret);
			goto err_supply_unreg;
		}
	}

	limiter->wqueue = create_singlethread_workqueue("limiter_workqueue");
	if (!limiter->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	limiter->in_ok = false;
	limiter->supplement_mode = false;
	limiter->power_meter = false;

	pr_info("%s [%s]: enb = %d\n",
		__func__,
		current_limiter_type_str[limiter->pdata->bat_type],
		gpio_get_value(limiter->pdata->bat_enb));

	s2asl01_get_rev_id(limiter);
	s2asl01_init_regs(limiter);

	/* sysfs create */
	ret = s2asl01_limiter_create_attrs(&limiter->psy_sw->dev);
	if (ret) {
		dev_err(limiter->dev,
			"%s : Failed to create_attrs\n", __func__);
		goto err_irq;
	}

	dev_err(&client->dev, "%s: S2ASL01 Switching Driver Loaded\n", __func__);
	return 0;

err_irq:
err_pdata_free:
	power_supply_unregister(limiter->psy_sw);
err_supply_unreg:
	mutex_destroy(&limiter->i2c_lock);
err_i2cfunc_not_support:
	kfree(limiter);
err_parse_dt:
err_limiter_nomem:
	devm_kfree(&client->dev, pdata);

	return ret;
}

static const struct i2c_device_id s2asl01_limiter_id[] = {
	{"s2asl01-limiter", 0},
	{}
};

static void s2asl01_limiter_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
}

static int s2asl01_limiter_remove(struct i2c_client *client)
{
	struct s2asl01_limiter_data *limiter = i2c_get_clientdata(client);

	power_supply_unregister(limiter->psy_sw);
	mutex_destroy(&limiter->i2c_lock);
	return 0;
}

#if defined CONFIG_PM
static int s2asl01_limiter_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int s2asl01_limiter_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}
#else
#define s2asl01_limiter_suspend NULL
#define s2asl01_limiter_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2asl01_limiter_pm_ops, s2asl01_limiter_suspend,
		s2asl01_limiter_resume);

static struct i2c_driver s2asl01_limiter_driver = {
	.driver = {
		.name = "s2asl01-limiter",
		.owner = THIS_MODULE,
		.pm = &s2asl01_limiter_pm_ops,
		.of_match_table = s2asl01_limiter_match_table,
	},
	.probe  = s2asl01_limiter_probe,
	.remove = s2asl01_limiter_remove,
	.shutdown   = s2asl01_limiter_shutdown,
	.id_table   = s2asl01_limiter_id,
};

static int __init s2asl01_limiter_init(void)
{
	pr_info("%s: S2ASL01 Switching Init\n", __func__);
	return i2c_add_driver(&s2asl01_limiter_driver);
}

static void __exit s2asl01_limiter_exit(void)
{
	i2c_del_driver(&s2asl01_limiter_driver);
}
module_init(s2asl01_limiter_init);
module_exit(s2asl01_limiter_exit);

MODULE_DESCRIPTION("Samsung S2ASL01 Limitter IC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
