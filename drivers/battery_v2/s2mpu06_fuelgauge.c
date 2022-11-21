/*
 *  s2mpu06_fuelgauge.c
 *  Samsung S2MPU06 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Developed by Nguyen Tien Dat (tiendat.nt@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#define SINGLE_BYTE	1

#include "include/fuelgauge/s2mpu06_fuelgauge.h"
#include <linux/of_gpio.h>
#include <linux/sec_batt.h>

#if defined(CONFIG_BATTERY_AGE_FORECAST)
struct fg_age_data_info {
	int cell_character0[S2MPU06_FG_REG_CELL_CHAR0_NUM];
	int cell_character1[S2MPU06_FG_REG_CELL_CHAR1_NUM];
};

#define fg_age_data_info_t \
	struct fg_age_data_info
#endif

struct s2mpu06_fuelgauge_data {
	struct device		*dev;
	struct i2c_client	*i2c;

	struct mutex		fuelgauge_mutex;
	s2mpu06_fuelgauge_platform_data_t *pdata;
	struct power_supply	psy_fg;

	int cable_type;
	bool is_charging;

	/* HW-dedicated fuel guage info structure
	* used in individual fuel gauge file only
	* (ex. dummy_fuelgauge.c)
	*/
	struct sec_fg_info      info;
	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity_old;      /* only for atomic calculation */
	unsigned int capacity_max;      /* only for dynamic calculation */
	unsigned int standard_capacity;
	int raw_capacity;

	bool initial_update_of_soc;
	struct mutex fg_lock;
	struct delayed_work isr_work;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	fg_age_data_info_t *age_data_info;
	int fg_num_age_step;
	int fg_age_step;
	int change_step;
	int *raw_100_vbat;
#endif

	unsigned int pre_soc;

	int irq_vbatl;
	int irq_socl;
};

static enum power_supply_property s2mpu06_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};


static int s2mpu06_write_2byte_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;

	s2mpu06_write_reg(client, reg, buf[0]);
	s2mpu06_write_reg(client, reg+1, buf[1]);
#else
	int ret, i = 0;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_i2c_block_data(client, reg,
								 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static int s2mpu06_read_2byte_reg(struct i2c_client *client, int reg, u8 *buf)
{

#if SINGLE_BYTE
	int ret = 0;
	u8 data1 = 0 , data2 = 0;

	s2mpu06_read_reg(client, reg, &data1);
	s2mpu06_read_reg(client, reg+1, &data2);
	buf[0] = data1;
	buf[1] = data2;
#else
	int ret = 0, i = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_read_i2c_block_data(client, reg,
								 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static void s2mpu06_fg_test_read(struct i2c_client *client)
{
	static const int count = 24;
	static int reg[24] = {
			0x06, 0x07, 0x0C, 0x0D, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
			0x3A, 0x3B, 0x3C, 0x3D, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65,
			0x66, 0x67, 0x68, 0x69};
	char str[512] = {0,};
	int i = 0;
	u8 data;

	for (i = 0; i < count; i++) {
		s2mpu06_read_reg(client, reg[i], &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg[i], data);
	}

	pr_err("%s: %s\n", __func__, str);
}

static void s2mpu06_alert_init(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting: 3.55V */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mpu06_write_2byte_reg(fuelgauge->i2c, S2MPU06_FG_REG_IRQ_LVL, data);
}

static bool s2mpu06_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (s2mpu06_read_2byte_reg(client, S2MPU06_FG_REG_STATUS, data) < 0)
		return ret;

	dev_dbg(&client->dev, "%s: status to (%02x%02x)\n",
		__func__, data[1], data[0]);

	if (data[0] & (0x1 << 3))
		return true;
	else
		return false;
}

static int s2mpu06_set_temperature(struct s2mpu06_fuelgauge_data *fuelgauge,
			int temperature)
{
	/*
	 * s2mpu06 include temperature sensor so,
	 * do not need to set temperature value.
	 */
	int val;

	val = temperature / 10;

	if (val < -5)
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_RTEMP, 0);
	else if (val >= -5 && val < 15)
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_RTEMP, 10);
	else if (val >= 15 && val < 35)
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_RTEMP, 25);
	else if (val >= 35)
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_RTEMP, 40);

	return temperature;
}

static int s2mpu06_get_temperature(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	s32 temperature = 0;

	if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
				S2MPU06_FG_REG_RTEMP, data) < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	if (data[0] & (0x1 << 7)) {
		/* Negative */
		temperature = ((~(data[0])) & 0xFF) + 1;
		temperature *= -10;
	} else {
		temperature = data[0] & 0x7F;
		temperature *= 10;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: temperature (%d)\n",
		__func__, temperature);

	return temperature;
}

/* soc should be 0.01% unit */
static int s2mpu06_get_soc(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	for (i = 0; i < 50; i++) {
		if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RSOC, check_data) < 0)
			return -EINVAL;
		dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n",
						 __func__, data[0], data[1]);

		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n",
						__func__, data[0], data[1]);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_info(&fuelgauge->i2c->dev, "[DEBUG]%s: raw capacity (0x%x:%d)\n",
		 __func__, compliment, rsoc);

	return (min(rsoc, 10000) / 10);
}

static int s2mpu06_get_rawsoc(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	for (i = 0; i < 50; i++) {
		if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RSOC, check_data) < 0)
			return -EINVAL;
		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
	}

	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_dbg(&fuelgauge->i2c->dev,
			"[DEBUG]%s: raw capacity (0x%x:%d)\n", __func__,
			compliment, rsoc);
	return min(rsoc, 10000);
}

static int s2mpu06_get_current(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RCUR, data) < 0)
		return -EINVAL;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: rCUR(0x%4x)\n",
					__func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: current (%d)mA\n",
					__func__, curr);

	return curr;
}

static int s2mpu06_get_ocv(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 rocv = 0;

	if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
			S2MPU06_FG_REG_ROCV, data) < 0)
		return -EINVAL;

	rocv = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: rocv (%d)\n", __func__, rocv);

	return rocv;
}

static int s2mpu06_get_vbat(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
			S2MPU06_FG_REG_RVBAT, data) < 0)
		return -EINVAL;
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mpu06_get_avgvbat(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 new_vbat, old_vbat = 0;
	int cnt;

	for (cnt = 0; cnt < 5; cnt++) {
		if (s2mpu06_read_2byte_reg(fuelgauge->i2c,
					S2MPU06_FG_REG_RVBAT, data) < 0)
			return -EINVAL;

		new_vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

		if (cnt == 0)
			old_vbat = new_vbat;
		else
			old_vbat = new_vbat / 2 + old_vbat / 2;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n",
						__func__, old_vbat);

	return old_vbat;
}

/* capacity is  0.1% unit */
static void s2mpu06_fg_get_scaled_capacity(
		struct s2mpu06_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	dev_info(&fuelgauge->i2c->dev,
			"%s: scaled capacity (%d.%d)\n",
			__func__, val->intval/10, val->intval%10);
}

/* capacity is integer */
static void s2mpu06_fg_get_atomic_capacity(
		struct s2mpu06_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
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

static int s2mpu06_fg_check_capacity_max(
		struct s2mpu06_fuelgauge_data *fuelgauge, int capacity_max)
{
	int new_capacity_max = capacity_max;

	if (new_capacity_max < (fuelgauge->pdata->capacity_max -
				fuelgauge->pdata->capacity_max_margin - 10)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max -
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(&fuelgauge->i2c->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	} else if (new_capacity_max > (fuelgauge->pdata->capacity_max +
				fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(&fuelgauge->i2c->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	}

	return new_capacity_max;
}

static int s2mpu06_fg_calculate_dynamic_scale(
		struct s2mpu06_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;

	raw_soc_val.intval = s2mpu06_get_rawsoc(fuelgauge) / 10;

	if (raw_soc_val.intval <
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin) {
		dev_info(&fuelgauge->i2c->dev, "%s: raw_soc(%d) is very low,keep capacity_max(%d)\n",
				__func__, raw_soc_val.intval, fuelgauge->capacity_max);
		return fuelgauge->capacity_max;
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			 fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		dev_dbg(&fuelgauge->i2c->dev, "%s: raw soc (%d)",
				__func__, fuelgauge->capacity_max);
	}

	fuelgauge->capacity_max = s2mpu06_fg_check_capacity_max(
			fuelgauge, (fuelgauge->capacity_max * 100 / (capacity + 1)));
	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = capacity;
	dev_info(&fuelgauge->i2c->dev, "%s: %d is used for capacity_max\n",
			__func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

bool s2mpu06_fuelgauge_fuelalert_init(struct s2mpu06_fuelgauge_data *fuelgauge, int soc)
{
	u8 data[2];

	fuelgauge->is_fuel_alerted = false;

	s2mpu06_alert_init(fuelgauge);

	if (s2mpu06_read_2byte_reg(fuelgauge->i2c, S2MPU06_FG_REG_IRQ, data) < 0)
		return -1;


	s2mpu06_write_2byte_reg(fuelgauge->i2c, S2MPU06_FG_REG_IRQ, data);

	return true;
}

bool s2mpu06_fuelgauge_is_fuelalerted(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	return s2mpu06_check_status(fuelgauge->i2c);
}

bool s2mpu06_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct s2mpu06_fuelgauge_data *fuelgauge = irq_data;
	int ret;

	ret = s2mpu06_write_2byte_reg(fuelgauge->i2c, S2MPU06_FG_REG_IRQ, 0x00);
	if (ret < 0)
		dev_err(&fuelgauge->i2c->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

bool s2mpu06_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

#define DISCHARGE_SAMPLE_CNT 5

/* if ret < 0, discharge */
static int s2mpu06_bat_check_discharge(int vcell)
{
	static int cnt;
	static int pre_vcell = 0;
	union power_supply_propval value_bat;

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_HEALTH, value_bat);

        if (vcell > 3400)
		return 1;
        else if ((value_bat.intval == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		(value_bat.intval == POWER_SUPPLY_HEALTH_COLD))
		return -1;

	if (pre_vcell == 0)
		pre_vcell = vcell;
	else if (pre_vcell > vcell)
		cnt++;
	else
		cnt--;

	pre_vcell = vcell;

	if (cnt >= DISCHARGE_SAMPLE_CNT)
		return -1;
	else
		return 1;
}

/* judge power off or not by current_avg */
static int s2mpu06_fg_get_current_average(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	int vcell, soc, curr_avg;
	int check_discharge;

	vcell = s2mpu06_get_vbat(fuelgauge);
	soc = s2mpu06_get_soc(fuelgauge) / 10;
	check_discharge = s2mpu06_bat_check_discharge(vcell);

	/* if 0% && under 3.4v && low power charging(1000mA), power off */
	if ((soc <= 0) && ((check_discharge < 0) || (vcell < 3200))) {
		pr_info("%s: SOC(%d), Vnow(%d)\n",
			__func__, soc, vcell);
		curr_avg = -1;
	} else {
		curr_avg = 0;
	}

	return curr_avg;
}

static void s2mpu06_external_power_setting(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	u8 reg_data;

	s2mpu06_read_reg(fuelgauge->i2c, 0x2E, &reg_data);
	reg_data &= 0x7F;
	reg_data |= 0x80;
	s2mpu06_write_reg(fuelgauge->i2c, 0x2E, reg_data);
	pr_info("%s: 0x2E[7] = 1 setting for external power flag\n", __func__);
}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
static void s2mpu06_reset_fg(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	int i;
	u8 temp = 0;
	unsigned int offset = 0;
	unsigned char reg2F, reg30, reg31;
	unsigned char reg2F_t, reg30_t, reg31_t;
	unsigned char vbat_i2c_h, vbat_i2c_l;
	unsigned int vbat_i2c_temp;

	for (i = 0; i < S2MPU06_FG_REG_CELL_CHAR0_NUM; i++) {
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_CELL_CHAR0 + i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[i]);
	}
	for (i = 0; i < S2MPU06_FG_REG_CELL_CHAR1_NUM; i++) {
		s2mpu06_write_reg(fuelgauge->i2c, S2MPU06_FG_REG_CELL_CHAR1 + i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character1[i]);
	}

	s2mpu06_read_reg(fuelgauge->i2c, 0x2F, &reg2F);
	s2mpu06_read_reg(fuelgauge->i2c, 0x30, &reg30);
	s2mpu06_read_reg(fuelgauge->i2c, 0x31, &reg31);
	pr_info("%s: original reg2F:0x%02x, reg30:0x%02x, reg31:0x%02x, offset:0x%x \n",
		__func__, reg2F, reg30, reg31, offset);

	/* For long life algorithm reset, use defualt offset */
	offset = 0x30000;

	s2mpu06_write_reg(fuelgauge->i2c, 0x31, (offset >> 16) & 0x0F);
	s2mpu06_write_reg(fuelgauge->i2c, 0x30, (offset >> 8) & 0xFF);
	s2mpu06_write_reg(fuelgauge->i2c, 0x2F, offset & 0xFF);

	s2mpu06_read_reg(fuelgauge->i2c, 0x2F, &reg2F_t);
	s2mpu06_read_reg(fuelgauge->i2c, 0x30, &reg30_t);
	s2mpu06_read_reg(fuelgauge->i2c, 0x31, &reg31_t);
	pr_info("%s: temp reg2F:0x%02x, reg30:0x%02x, reg31:0x%02x\n",
		__func__, reg2F_t, reg30_t, reg31_t);

	/* Set i2c vbat for long life algorithm reset */
	/* This reset occurs after 2nd top-off, */
	/* so target Vbat is 'rawsoc 100% voltage' + 10mV */
	vbat_i2c_temp = fuelgauge->raw_100_vbat[fuelgauge->fg_age_step] + 10;
	vbat_i2c_temp = ((vbat_i2c_temp - 3000) << 15) / 3000;
	vbat_i2c_h = (vbat_i2c_temp & 0xFF00) >> 8;
	vbat_i2c_l = vbat_i2c_temp & 0xFF;

	pr_info("%s: vbat = %d, vbat_i2c_h = 0x%02x, vbat_i2c_l = 0x%02x\n",
			__func__, fuelgauge->raw_100_vbat[fuelgauge->fg_age_step],
			vbat_i2c_h, vbat_i2c_l);

	s2mpu06_write_reg(fuelgauge->i2c, 0x23, vbat_i2c_h);
	s2mpu06_write_reg(fuelgauge->i2c, 0x22, vbat_i2c_l);
	s2mpu06_update_reg(fuelgauge->i2c, 0x2A, 0x08, 0x08);

	msleep(10);

	/*Dump Done*/
	s2mpu06_write_reg(fuelgauge->i2c, 0x1A, 0x33);
	msleep(500);

	/* Disable i2c vbat */
	s2mpu06_update_reg(fuelgauge->i2c, 0x2A, 0x00, 0x08);

	s2mpu06_write_reg(fuelgauge->i2c, 0x2F, reg2F);
	s2mpu06_write_reg(fuelgauge->i2c, 0x30, reg30);
	s2mpu06_write_reg(fuelgauge->i2c, 0x31, reg31);

	s2mpu06_read_reg(fuelgauge->i2c, 0x2F, &reg2F);
	s2mpu06_read_reg(fuelgauge->i2c, 0x30, &reg30);
	s2mpu06_read_reg(fuelgauge->i2c, 0x31, &reg31);
	pr_info("%s: original reg2F:0x%02x, reg30:0x%02x, reg31:0x%02x, offset:0x%x \n",
		__func__, reg2F, reg30, reg31, offset);

	/* after FG reset , battery data version get reset to default value 1, causing mismatch in bootloader and kernel FG data verion.
	 * below code restores the FG data version in 0x2A[2:0] register to it's initalized value */

	pr_info("%s: FG data version %02x\n", __func__, fuelgauge->info.data_ver);
	s2mpu06_read_reg(fuelgauge->i2c, 0x2A, &temp);
	temp &= 0xF8;
	temp |= (fuelgauge->info.data_ver & 0x07);
	s2mpu06_write_reg(fuelgauge->i2c, 0x2A, temp);	
}

static int s2mpu06_fg_aging_check(
	struct s2mpu06_fuelgauge_data *fuelgauge, int step)
{
	u8 cell_char[4];
	u8 por_state = 0;
    union power_supply_propval value;
	int charging_enabled = false;
	int i;

	if (fuelgauge->fg_num_age_step == 0)
		return 0;

	if (step < 0)
		fuelgauge->fg_age_step = 0;
	else if (step >= fuelgauge->fg_num_age_step)
		fuelgauge->fg_age_step = fuelgauge->fg_num_age_step - 1;
	else
		fuelgauge->fg_age_step = step;

	for (i = 0; i < 4; i++)
		s2mpu06_read_reg(fuelgauge->i2c, S2MPU06_FG_REG_CELL_CHAR0 + i, &cell_char[i]);

	pr_info("%s: [Long life] step = %d, orig. data : %02x, %02x, %02x, %02x, fg_age_step data : %02x, %02x, %02x, %02x\n",
		__func__, step, cell_char[0], cell_char[1], cell_char[2], cell_char[3],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[0],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[1],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[2],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[3]);

	if ((cell_char[0] !=
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[0]) ||
		(cell_char[1] !=
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[1]) ||
		(cell_char[2] !=
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[2]) ||
		(cell_char[3] !=
			fuelgauge->age_data_info[fuelgauge->fg_age_step].cell_character0[3])) {
		/* Set por_state to restore battery params when abnormal power off*/
		s2mpu06_read_reg(fuelgauge->i2c, 0x1B, &por_state);
		por_state = por_state | 0x10;
		s2mpu06_write_reg(fuelgauge->i2c, 0x1B, por_state);

		psy_do_property("s2mpu06-charger", get, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		charging_enabled = value.intval;

		if (charging_enabled == true) {
			pr_info("%s: [Long life] disable charger for reset gauge age forecast \n", __func__);
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			psy_do_property("s2mpu06-charger", set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
			msleep(100);
		}
		s2mpu06_reset_fg(fuelgauge);

		if (charging_enabled == true) {
			psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
			charging_enabled = value.intval;

			if (charging_enabled == 1) { /* POWER_SUPPLY_STATUS_CHARGING 1 */
				pr_info("%s: [Long life] enable charger for reset gauge age forecast \n", __func__);
				value.intval = SEC_BAT_CHG_MODE_CHARGING;
				psy_do_property("s2mpu06-charger", set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
			}
		}

		/* Finished normally, Clear por_state*/
		s2mpu06_read_reg(fuelgauge->i2c, 0x1B, &por_state);
		por_state = por_state & (~0x10);
		s2mpu06_write_reg(fuelgauge->i2c, 0x1B, por_state);

		return 1;
	}
	return 0;
}
#endif

static int s2mpu06_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mpu06_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mpu06_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fuelgauge->pdata->capacity_full * fuelgauge->raw_capacity;
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mpu06_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mpu06_get_avgvbat(fuelgauge);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mpu06_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = s2mpu06_get_current(fuelgauge);
		s2mpu06_fg_test_read(fuelgauge->i2c);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = s2mpu06_fg_get_current_average(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mpu06_get_rawsoc(fuelgauge);
		} else {
			val->intval = s2mpu06_get_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				s2mpu06_fg_get_scaled_capacity(fuelgauge, val);

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

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
				fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				s2mpu06_fuelgauge_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc);
			}

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
				s2mpu06_fg_get_atomic_capacity(fuelgauge, val);
		}

		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mpu06_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpu06_fg_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct s2mpu06_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mpu06_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			s2mpu06_fg_aging_check(fuelgauge,
					fuelgauge->change_step);
#endif
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (!val->intval) {
			/* Set FG POR Bit : 0x1B[4] */
			s2mpu06_update_reg(fuelgauge->i2c, 0x1B, 0x10, 0x10);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			s2mpu06_fg_calculate_dynamic_scale(fuelgauge, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY ||
			val->intval == POWER_SUPPLY_TYPE_WATER) {
			fuelgauge->is_charging = false;
		} else  {
			fuelgauge->is_charging = true;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			s2mpu06_write_reg(fuelgauge->i2c, 0x1A, 0x33);
			msleep(300);
		}
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		s2mpu06_set_temperature(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		s2mpu06_external_power_setting(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		dev_info(&fuelgauge->i2c->dev,
			"%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max =
			 s2mpu06_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		/* rt5033_fg_reset_capacity_by_jig_connection(fuelgauge->i2c); */
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		{
			enum power_supply_ext_property ext_psp = psp;
			switch (ext_psp) {
#if defined(CONFIG_BATTERY_AGE_FORECAST)
			case POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA:
				fuelgauge->change_step = val->intval;
				break;
#endif
			default:
				return -EINVAL;
			}
			break;
		}
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mpu06_fg_isr_work(struct work_struct *work)
{
	struct s2mpu06_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mpu06_fuelgauge_data,
							isr_work.work);
	u8 fg_alert_status = 0;

	s2mpu06_read_reg(fuelgauge->i2c,
			S2MPU06_FG_REG_STATUS+1, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s : fg_alert_status(0x%x)\n",
				__func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s : Battery Level is very Low!\n", __func__);

	if (fg_alert_status & 0x02)
		pr_info("%s : Battery Voltage is Very Low!\n", __func__);

	if (!fg_alert_status) {
		fuelgauge->is_fuel_alerted = false;
		pr_info("%s : SOC or Volage is Good!\n", __func__);
		wake_unlock(&fuelgauge->fuel_alert_wake_lock);
	}
}

static irqreturn_t s2mpu06_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mpu06_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mpu06_read_reg(fuelgauge->i2c, S2MPU06_FG_REG_IRQ, &fg_irq);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_irq(0x%x)\n",
		__func__, fg_irq);

	if (fuelgauge->is_fuel_alerted) {
		return IRQ_HANDLED;
	} else {
		wake_lock(&fuelgauge->fuel_alert_wake_lock);
		fuelgauge->is_fuel_alerted = true;
		schedule_delayed_work(&fuelgauge->isr_work, 0);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
static int s2mpu06_fuelgauge_parse_dt(struct device *dev,
			struct s2mpu06_fuelgauge_platform_data *pdata)
{
#else
static int s2mpu06_fuelgauge_parse_dt(struct s2mpu06_fuelgauge_data *fuelgauge)
{
	struct s2mpu06_fuelgauge_platform_data *pdata = fuelgauge->pdata;
#endif
	struct device_node *np;
	int ret = 0;

	/* reset, irq gpio info */
	np = of_find_node_by_name(NULL, "s2mpu06-fuelgauge");
	if (!np) {
		pr_err("%s fuelgauge node is null\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "fuelgauge,capacity_max",
			&pdata->capacity_max);
	if (ret) {
		pr_err("%s error reading capacity_max %d\n", __func__, ret);
		goto err;
	}

	ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
			&pdata->capacity_max_margin);
	if (ret) {
		pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);
		goto err;
	}

	ret = of_property_read_u32(np, "fuelgauge,capacity_min",
			&pdata->capacity_min);
	if (ret) {
		pr_err("%s error reading capacity_min %d\n", __func__, ret);
		goto err;
	}

	ret = of_property_read_u32(np, "fuelgauge,capacity_full",
			&fuelgauge->pdata->capacity_full);
	if (ret < 0)
		pr_err("%s error reading capacity_full %d\n", __func__, ret);

	ret = of_property_read_u32(np,
			"fuelgauge,capacity_calculation_type",
			&pdata->capacity_calculation_type);
	if (ret) {
		pr_err("%s error reading capacity_calculation_type %d\n", __func__, ret);
		goto err;
	}

	ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
			&pdata->fuel_alert_soc);
	if (ret) {
		pr_err("%s error reading pdata->fuel_alert_soc %d\n", __func__, ret);
		goto err;
	}

	pdata->repeated_fuelalert = of_property_read_bool(np,
			"fuelgauge,repeated_fuelalert");

#if defined(CONFIG_BATTERY_AGE_FORECAST)
{
	int len = 0;
	unsigned int i = 0;

	of_get_property(np, "fuelgauge,cell_character", &len);
	fuelgauge->fg_num_age_step = len / sizeof(fg_age_data_info_t);
	fuelgauge->age_data_info = kzalloc(len, GFP_KERNEL);
	ret = of_property_read_u32_array(np, "fuelgauge,cell_character",
			(int *)fuelgauge->age_data_info, len/sizeof(int));

	of_get_property(np, "fuelgauge,raw_100_vbat", &len);
	fuelgauge->raw_100_vbat	= kzalloc(len, GFP_KERNEL);
	ret = of_property_read_u32_array(np, "fuelgauge,raw_100_vbat",
			(int *)fuelgauge->raw_100_vbat, len/sizeof(int));

	for (i = 0 ; i < fuelgauge->fg_num_age_step ; i++) {
		pr_err("%s: [Long life] age_step = %d, cell_character0[0] = %02x, cell_character1[0] = %02x, raw_100_vbat = %d\n",
			__func__, i, fuelgauge->age_data_info[i].cell_character0[0],
			fuelgauge->age_data_info[i].cell_character1[0], fuelgauge->raw_100_vbat[i]);
	}
}
#endif

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s battery node is null\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_string(np,
			"battery,fuelgauge_name",
			(char const **)&pdata->fuelgauge_name);
	if (ret) {
		pr_err("%s fuelgauge name is unknown\n", __func__);
		goto err;
	}

	/* batt data version */
	ret = of_property_read_u32(np, "battery,data_ver", &fuelgauge->info.data_ver);
	if (ret < 0) {
		pr_err("%s: error reading data_ver (%d)\n", __func__, ret);
		fuelgauge->info.data_ver = 0;
	}
	pr_info("%s: battery,data_ver <%d>\n", __func__, fuelgauge->info.data_ver);

	return 0;

err:
	return ret;
}

static struct of_device_id s2mpu06_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mpu06-fuelgauge",},
	{},
};
#else
static int s2mpu06_fuelgauge_parse_dt(struct device *dev,
			struct s2mpu06_fuelgauge_platform_data *pdata)
{
	return -ENOSYS;
}

#define s2mpu06_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static int s2mpu06_fuelgauge_probe(struct platform_device *pdev)
{
	struct s2mpu06_dev *s2mpu06 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu06_platform_data *pdata = dev_get_platdata(s2mpu06->dev);
	struct s2mpu06_fuelgauge_data *fuelgauge;

	union power_supply_propval raw_soc_val;
	u8 temp = 0;
	int ret = 0;

	pr_info("%s: S2MPU06 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->i2c = s2mpu06->fuelgauge;

	fuelgauge->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fuelgauge->pdata)),
			GFP_KERNEL);
	if (!fuelgauge->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	fuelgauge->change_step = 0;
	ret = s2mpu06_fuelgauge_parse_dt(fuelgauge);
#else
	ret = s2mpu06_fuelgauge_parse_dt(&pdev->dev, fuelgauge->pdata);
#endif
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "sec-fuelgauge";

	fuelgauge->psy_fg.name          = fuelgauge->pdata->fuelgauge_name;
	fuelgauge->psy_fg.type          = POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property  = s2mpu06_fg_get_property;
	fuelgauge->psy_fg.set_property  = s2mpu06_fg_set_property;
	fuelgauge->psy_fg.properties    = s2mpu06_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
			ARRAY_SIZE(s2mpu06_fuelgauge_props);

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = s2mpu06_get_rawsoc(fuelgauge);
	if (raw_soc_val.intval == 0)
		raw_soc_val.intval = s2mpu06_get_rawsoc(fuelgauge);

	raw_soc_val.intval = raw_soc_val.intval / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mpu06_fg_calculate_dynamic_scale(fuelgauge, 100);

	ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		/* 1. Set s2mpu06 alert configuration. */
		s2mpu06_alert_init(fuelgauge);
		wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");
		INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mpu06_fg_isr_work);

		fuelgauge->irq_vbatl =
			 pdata->irq_base + S2MPU06_FG_IRQ_VBAT_L_INT;
		ret = request_threaded_irq(fuelgauge->irq_vbatl, NULL,
			s2mpu06_fg_irq_thread, 0, "fuel_vbatl", fuelgauge);
		if (ret < 0) {
			dev_err(s2mpu06->dev,
				"%s: Fail to request vbat low in IRQ: %d: %d\n",
				__func__, fuelgauge->irq_vbatl, ret);
			goto err_reg_irq;
		}

		fuelgauge->irq_socl =
				pdata->irq_base + S2MPU06_FG_IRQ_SOC_L_INT;
		ret = request_threaded_irq(fuelgauge->irq_socl, NULL,
			s2mpu06_fg_irq_thread, 0, "fuel_socl", fuelgauge);
		if (ret < 0) {
			dev_err(s2mpu06->dev,
				"%s: Fail to request soc low in IRQ: %d: %d\n",
				__func__, fuelgauge->irq_socl, ret);
			goto err_reg_irq;
		}
	}

	fuelgauge->initial_update_of_soc = true;

	if (!fuelgauge->info.data_ver) {
		s2mpu06_read_reg(fuelgauge->i2c, 0x2A, &temp);
		fuelgauge->info.data_ver = (temp & 0x07);
	}

	pr_info("%s: S2MPU06 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_reg_irq:
	power_supply_unregister(&fuelgauge->psy_fg);
err_data_free:
	if (pdev->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static void s2mpu06_fuelgauge_shutdown(struct device *dev)
{
	pr_info("%s: S2MPU06 FG driver shutdown\n", __func__);
}

static int s2mpu06_fuelgauge_remove(struct platform_device *pdev)
{
	struct s2mpu06_fuelgauge_data *fuelgauge =
			platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

#if defined CONFIG_PM
static int s2mpu06_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mpu06_fuelgauge_resume(struct device *dev)
{
	struct s2mpu06_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	if (fuelgauge->is_charging)
		fuelgauge->initial_update_of_soc = true;

	return 0;
}
#else
#define s2mpu06_fuelgauge_suspend NULL
#define s2mpu06_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mpu06_fuelgauge_pm_ops, s2mpu06_fuelgauge_suspend,
		s2mpu06_fuelgauge_resume);

static struct platform_driver s2mpu06_fuelgauge_driver = {
	.driver = {
		.name = "s2mpu06-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mpu06_fuelgauge_pm_ops,
		.of_match_table = s2mpu06_fuelgauge_match_table,
		.shutdown   = s2mpu06_fuelgauge_shutdown,
	},
	.probe  = s2mpu06_fuelgauge_probe,
	.remove = s2mpu06_fuelgauge_remove,
};

static int __init s2mpu06_fuelgauge_init(void)
{
	pr_info("%s: S2MPU06 Fuelgauge Init\n", __func__);
	return platform_driver_register(&s2mpu06_fuelgauge_driver);
}
module_init(s2mpu06_fuelgauge_init);

static void __exit s2mpu06_fuelgauge_exit(void)
{
	platform_driver_unregister(&s2mpu06_fuelgauge_driver);
}
module_exit(s2mpu06_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MPU06 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
