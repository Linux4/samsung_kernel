/*
 *  s2mu003_fuelgauge.c
 *  Samsung S2MU003 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/battery/fuelgauge/s2mu003_fuelgauge.h>
#include <linux/of_gpio.h>
#include <linux/sec_batt.h>

#define BST_LEVEL_SOC		190
#define BST_LEVEL_VOLTAGE	3600
int low_soc_flag;
EXPORT_SYMBOL(low_soc_flag);

static enum power_supply_property s2mu003_fuelgauge_props[] = {
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
};

static int s2mu003_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret, i = 0;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mu003_read_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret = 0, i = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mu003_get_soc(struct s2mu003_fuelgauge_data *fuelgauge);
#define TABLE_SIZE	22
static int s2mu003_get_ocv_from_soc(struct s2mu003_fuelgauge_data *fuelgauge)
{
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
	/* 22 values of mapping table */
	int soc_arr[TABLE_SIZE] = {10198, 9692, 9186, 8681, 8176, 7670, 7165, 6659, 6153, 5648, 5142, 4637, 4132, 3626, 3120, 2615, 2109, 1604, 1098, 593, 87, -180}; // * 0.01%
	int ocv_arr[TABLE_SIZE] = {43656, 42999, 42342, 41779, 41247, 40785, 40142, 39707, 39451, 39062, 38581, 38299, 38100, 37947, 37828, 37736, 37568, 37367, 37035, 36904, 35566, 31882}; // *0.1mV
#else
	int *soc_arr;
	int *ocv_arr;
	int soc_arr_val[S2MU003_FG_NUM_AGE_STEP][TABLE_SIZE] = {
		{10198, 9692, 9186, 8681, 8176, 7670, 7165, 6659, 6153, 5648, 5142, 4637, 4132, 3626, 3120, 2615, 2109, 1604, 1098, 593, 87, -180},
		{10811, 10288, 9765, 9242, 8719, 8196, 7673, 7150, 6627, 6104, 5581, 5058, 4535, 4012, 3489, 2965, 2443, 1920, 1397, 874, 351, -172},
		{10903, 10376, 9849, 9321, 8794, 8266, 7739, 7211, 6683, 6156, 5629, 5101, 4574, 4046, 3518, 2990, 2463, 1936, 1409, 881, 353, -174},
		{10754, 10234, 9713, 9193, 8672, 8152, 7631, 7110, 6590, 6070, 5548, 5029, 4508, 3987, 3467, 2945, 2426, 1905, 1385, 864, 344, -177},
		{10912, 10380, 9848, 9315, 8783, 8251, 7719, 7187, 6654, 6122, 5590, 5058, 4525, 3993, 3461, 2929, 2396, 1864, 1332, 799, 267, -265}};
	int ocv_arr_val[S2MU003_FG_NUM_AGE_STEP][TABLE_SIZE] = {
		{43656, 42999, 42342, 41779, 41247, 40785, 40142, 39707, 39451, 39062, 38581, 38299, 38100, 37947, 37828, 37736, 37568, 37367, 37035, 36904, 35566, 31882},
		{44103, 43464, 42825, 42190, 41642, 41123, 40693, 40012, 39683, 39349, 38872, 38490, 38249, 38063, 37914, 37790, 37658, 37453, 37194, 36931, 36212, 31982},
		{43976, 43358, 42740, 42128, 41589, 41082, 40649, 39979, 39661, 39314, 38852, 38479, 38241, 38058, 37911, 37791, 37654, 37455, 37185, 36950, 36216, 32020},
		{43629, 43041, 42453, 41866, 41351, 40864, 40431, 39800, 39569, 39190, 38707, 38402, 38189, 38022, 37889, 37770, 37631, 37429, 37171, 36919, 36246, 31857},
		{43074, 42535, 41998, 41460, 40978, 40641, 39923, 39684, 39366, 38922, 38535, 38299, 38116, 37969, 37850, 37746, 37583, 37401, 37132, 36902, 36357, 31679}};
#endif
	int soc = fuelgauge->info.soc;
	//int soc = s2mu003_get_soc(fuelgauge);
	int ocv = 0;

	int high_index = TABLE_SIZE - 1;
	int low_index = 0;
	int mid_index = 0;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	soc_arr = soc_arr_val[fuelgauge->fg_age_step];
	ocv_arr = ocv_arr_val[fuelgauge->fg_age_step];	
#endif

	dev_info(&fuelgauge->i2c->dev,
		"%s: soc (%d) soc_arr[TABLE_SIZE-1] (%d) ocv_arr[TABLE_SIZE-1) (%d)\n",
		__func__, soc, soc_arr[TABLE_SIZE-1] , ocv_arr[TABLE_SIZE-1] );
	if(soc <= soc_arr[TABLE_SIZE - 1]) {
		ocv = ocv_arr[TABLE_SIZE - 1];
		goto ocv_soc_mapping;
	} else if (soc >= soc_arr[0]) {
		ocv = ocv_arr[0];
		goto ocv_soc_mapping;
	}
	while (low_index <= high_index) {
		mid_index = (low_index + high_index) >> 1;
		if(soc_arr[mid_index] > soc)
			low_index = mid_index + 1;
		else if(soc_arr[mid_index] < soc)
			high_index = mid_index - 1;
		else {
			ocv = ocv_arr[mid_index];
			goto ocv_soc_mapping;
		}
	}
	ocv = ocv_arr[high_index];
	ocv += ((ocv_arr[low_index] - ocv_arr[high_index]) *
					(soc - soc_arr[high_index])) /
					(soc_arr[low_index] - soc_arr[high_index]);

ocv_soc_mapping:
	dev_info(&fuelgauge->i2c->dev, "%s: soc (%d), ocv (%d)\n", __func__, soc, ocv);
	return ocv;
}

static int s2mu003_get_vbat(struct s2mu003_fuelgauge_data *fuelgauge);

static void WA_0_issue_at_init(struct s2mu003_fuelgauge_data *fuelgauge)
{
	int a = 0, v_23, v_24, v_25;
	int v23, v24, v25, v5, v4; /* for debug */
	int temp1, temp2;
	int FG_volt, UI_volt, offset;
	//u8 v_23, v_24, v_25;

	/* Step 1: [Surge test]  get UI voltage (0.1mV)*/
	UI_volt = s2mu003_get_ocv_from_soc(fuelgauge);

	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x1A, 0x33);
	msleep(450);

	/* Step 2: [Surge test] get FG voltage (0.1mV) */
	FG_volt = s2mu003_get_vbat(fuelgauge) * 10;

	/* Step 3: [Surge test] get offset */
	offset = UI_volt - FG_volt;
	pr_info("%s: UI_volt(%d), FG_volt(%d), offset(%d)\n",
	  __func__, UI_volt, FG_volt, offset);

	/* Step 4: [Surge test] */
	v_25 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x25);
	v_24 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x24);
	v_23 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x23);

	pr_info("%s: v_25(0x%x), v_24(0x%x)\n", __func__, v_25, v_24);

	a = (v_25 & 0x0F) << 16;
	a = a + ((v_24 & 0xFF) << 8);
	a += v_23;

	pr_info("%s: a before add offset (0x%x)\n", __func__, a);

	/* 2`s complement */
	if (a & (0x01 << 19))
	 a = ( -10000 * ((a^0xFFFFF) +1)) >> 16;
	else
	 a = ( 10000 * a ) >> 16;

	a = a + offset;
	pr_info("%s: a after add offset (0x%x)\n", __func__, a);

	/* limit upper/lower offset */
	if (a > 79000)
	 a = 79000;

	if (a < (-79000))
	 a = -79000;

	a = (a << 16) / 10000;
	if (a < 0)
	 a = -1*((a^0xFFFFF)+1);

	pr_info("%s: a after add offset (0x%x)\n", __func__, a);

	a &= 0xfffff;
	pr_info("%s: (a)&0xFFF (0x%x)\n", __func__, a);

	/* modify 0x25[3:0] */
	temp1 = v_25 & 0xF0;
	temp2 = (u8)((a&0xF0000) >> 16);
	temp1 |= temp2;
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x25, temp1);

	/* modify 0x24[7:0] */
	temp2 = (u8)((a & 0xFF00) >> 8);
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x24, temp2);

	/* modify 0x23[7:0] */
	temp2 = (u8)(a & 0xFF);
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x23, temp2);

	/* restart and dumpdone */
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x1A, 0x33);
	msleep(600);

	v25 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x25);
	v24 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x24);
	v23 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x23);
	v5 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x5);
	v4 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x4);
	pr_info("%s: v25(0x%x), v24(0x%x), v23(0x%x), v5(0x%x), v4(0x%x)\n",
		__func__, v25, v24, v23, v5, v4);

	/* recovery 0x25, 0x24, 0x23 */
	temp1 = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x25);
	temp1 &= 0xF0;
	temp1 |= (v_25 & 0x0F);
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x25, temp1);
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x24, v_24);
	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x23, v_23);

}

static void s2mu003_reset_fg(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 i;

#if defined(CONFIG_A3XE)
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
	/* 40 ~ B1 */
	static u8 model_param1rev1[] = {
		0xed,0xa,0x66,0xa,0xe0,0x9,0x6c,0x9,0xff,0x8,0xa1,0x8,0x1d,0x8,0xc4,
		0x7,0x90,0x7,0x40,0x7,0xdd,0x6,0xa4,0x6,0x7b,0x6,0x5c,0x6,0x43,0x6,
		0x30,0x6,0xe,0x6,0xe5,0x5,0xa1,0x5,0x86,0x5,0x74,0x4,0x81,0x1,0x29,
		0x8,0xc1,0x7,0x59,0x7,0xf2,0x6,0x8a,0x6,0x23,0x6,0xbb,0x5,0x54,0x5,
		0xec,0x4,0x85,0x4,0x1d,0x4,0xb6,0x3,0x4e,0x3,0xe7,0x2,0x7f,0x2,0x18,
		0x2,0xb0,0x1,0x48,0x1,0xe1,0x0,0x79,0x0,0x12,0x0,0xdb,0xf,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4,
	};
	static u8 model_param2rev1[] = {0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25,
	};
	/* D0 ~ E5 */
	static u8 model_param3rev1[] = {0x30,0x30,0x30,0x31,0x31,0x31,0x31,0x31,0x30,0x30,0x31,0x31,0x31,0x31,0x32,0x32,0x32,0x32,0x32,0x33,0x31,0xb3,
	};

	static u8 model_param1rev0[] = {
		0x73,0xB,0xF1,0xA,0x6F,0xA,0xED,0x9,0x75,0x9,0x3,0x9,0x99,0x8,0x36,
		0x8,0xDF,0x7,0x8F,0x7,0xB,0x7,0xCB,0x6,0x9B,0x6,0x73,0x6,0x53,0x6,
		0x39,0x6,0x13,0x6,0xE2,0x5,0x8F,0x5,0x77,0x5,0x98,0x4,0xBD,0x1,0x3A,
		0x8,0xD4,0x7,0x6D,0x7,0x6,0x7,0xA0,0x6,0x39,0x6,0xD2,0x5,0x6B,0x5,
		0x4,0x5,0x9E,0x4,0x37,0x4,0xD0,0x3,0x69,0x3,0x3,0x3,0x9C,0x2,0x35,
		0x2,0xCE,0x1,0x68,0x1,0x1,0x1,0x9A,0x0,0x33,0x0,0xF2,0xF,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x53,0x29,0x28,0x13,0x14,0x13,0xf2,0xed,0xf2,0xed,
		0xf2,0xed,0xf2,0xed,0xb0,0xad,0xb0,0xba,0xb0,0xba,
	};

	static u8 model_param2rev0[] = {0x3,0x7,0x1c,0xf,0x4,0x19,0x11,0x21,0x16,0x27,0x1f,0x27,0x2c,0x2c,
	};
	/* D0 ~ E5 */
	static u8 model_param3rev0[] = {0x3A,0x3A,0x3A,0x3A,0x3A,0x3B,0x3B,0x3B,0x3B,0x3C,0x3D,0x3D,0x3E,0x3F,0x3F,0x40,0x41,0x43,0x46,0x48,0x49,0x9A,
	};
#else
	/*0x40 ~ 0xB1, 114 values*/
	static u8 age_model_param1[S2MU003_FG_NUM_AGE_STEP][S2MU003_FG_NUM_MODELPARAM1] = {
		{0xed,0xa,0x66,0xa,0xe0,0x9,0x6c,0x9,0xff,0x8,0xa1,0x8,0x1d,0x8,0xc4,
		0x7,0x90,0x7,0x40,0x7,0xdd,0x6,0xa4,0x6,0x7b,0x6,0x5c,0x6,0x43,0x6,
		0x30,0x6,0xe,0x6,0xe5,0x5,0xa1,0x5,0x86,0x5,0x74,0x4,0x81,0x1,0x29,
		0x8,0xc1,0x7,0x59,0x7,0xf2,0x6,0x8a,0x6,0x23,0x6,0xbb,0x5,0x54,0x5,
		0xec,0x4,0x85,0x4,0x1d,0x4,0xb6,0x3,0x4e,0x3,0xe7,0x2,0x7f,0x2,0x18,
		0x2,0xb0,0x1,0x48,0x1,0xe1,0x0,0x79,0x0,0x12,0x0,0xdb,0xf,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4},
		
		{0x48,0xb,0xc5,0xa,0x43,0xa,0xc0,0x9,0x50,0x9,0xe6,0x8,0x8e,0x8,0x3,
		0x8,0xbf,0x7,0x7b,0x7,0x19,0x7,0xcb,0x6,0x99,0x6,0x73,0x6,0x55,0x6,
		0x3b,0x6,0x20,0x6,0xf6,0x5,0xc1,0x5,0x8b,0x5,0xf8,0x4,0x96,0x1,0xa6,
		0x8,0x3b,0x8,0xd0,0x7,0x65,0x7,0xfa,0x6,0x8f,0x6,0x23,0x6,0xb8,0x5,
		0x4d,0x5,0xe2,0x4,0x77,0x4,0xc,0x4,0xa1,0x3,0x36,0x3,0xcb,0x2,0x5f,
		0x2,0xf4,0x1,0x89,0x1,0x1e,0x1,0xb3,0x0,0x48,0x0,0xdd,0xf, 0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4},
		
		{0x2e,0xb,0xb0,0xa,0x31,0xa,0xb4,0x9,0x45,0x9,0xde,0x8,0x85,0x8,0xfc,
		0x7,0xbb,0x7,0x73,0x7,0x15,0x7,0xc8,0x6,0x98,0x6,0x72,0x6,0x54,0x6,
		0x3c,0x6,0x20,0x6,0xf7,0x5,0xbf,0x5,0x8f,0x5,0xf9,0x4,0x9e,0x1,0xb9,
		0x8,0x4d,0x8,0xe1,0x7,0x75,0x7,0x9,0x7,0x9d,0x6,0x31,0x6,0xc5,0x5,
		0x59,0x5,0xed,0x4,0x81,0x4,0x15,0x4,0xa9,0x3,0x3d,0x3,0xd0,0x2,0x65,
		0x2,0xf8,0x1,0x8c,0x1,0x21,0x1,0xb4,0x0,0x48,0x0,0xdc,0xf,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4},
		
		{0xe7,0xa,0x6f,0xa,0xf6,0x9,0x7e,0x9,0x15,0x9,0xb1,0x8,0x58,0x8,0xd7,
		0x7,0xa8,0x7,0x5a,0x7,0xf7,0x6,0xb9,0x6,0x8d,0x6,0x6b,0x6,0x50,0x6,
		0x37,0x6,0x1b,0x6,0xf1,0x5,0xbd,0x5,0x89,0x5,0xff,0x4,0x7c,0x1,0x9b,
		0x8,0x30,0x8,0xc5,0x7,0x5b,0x7,0xf0,0x6,0x85,0x6,0x1b,0x6,0xb0,0x5,
		0x46,0x5,0xdb,0x4,0x70,0x4,0x6,0x4,0x9b,0x3,0x31,0x3,0xc6,0x2,0x5b,
		0x2,0xf1,0x1,0x86,0x1,0x1c,0x1,0xb1,0x0,0x46,0x0,0xdc,0xf,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4},
		
		{0x76,0xa,0x7,0xa,0x99,0x9,0x2b,0x9,0xc8,0x8,0x83,0x8,0xf0,0x7,0xbf,
		0x7,0x7e,0x7,0x23,0x7,0xd4,0x6,0xa4,0x6,0x7e,0x6,0x60,0x6,0x48,0x6,
		0x32,0x6,0x11,0x6,0xec,0x5,0xb5,0x5,0x85,0x5,0x16,0x5,0x58,0x1,0xbb,
		0x8,0x4e,0x8,0xe1,0x7,0x74,0x7,0x7,0x7,0x9a,0x6,0x2d,0x6,0xc0,0x5,
		0x53,0x5,0xe6,0x4,0x79,0x4,0xc,0x4,0x9f,0x3,0x32,0x3,0xc5,0x2,0x58,
		0x2,0xeb,0x1,0x7e,0x1,0x11,0x1,0xa4,0x0,0x37,0x0,0xca,0xf,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x5b,0x29,0x28,0x1f,0x35,0x15,0x21,0x21,
		0x14,0x21,0x27,0xa,0xe6,0xf7,0x14,0xed,0xca,0xd4,0xd3,0xd4}
	};
	/*0xC0 ~ 0xCD, 14 values*/
	static u8 age_model_param2[S2MU003_FG_NUM_AGE_STEP][S2MU003_FG_NUM_MODELPARAM2] = {
		{0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25},
		{0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25},
		{0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25},
		{0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25},
		{0x3,0x6,0x18,0xd,0x4,0x16,0xf,0x1c,0x13,0x22,0x1a,0x22,0x25,0x25}
	};
	/*0xD0 ~ 0xE5, 22 values*/
	static u8 age_model_param3[S2MU003_FG_NUM_AGE_STEP][S2MU003_FG_NUM_MODELPARAM3] = {
		{0x30,0x30,0x30,0x31,0x31,0x31,0x31,0x31,0x30,0x30,0x31,0x31,0x31,0x31,0x32,0x32,0x32,0x32,0x32,0x33,0x31,0xb3},
		{0x44,0x44,0x44,0x45,0x45,0x45,0x44,0x45,0x43,0x43,0x44,0x45,0x46,0x49,0x4b,0x4d,0x53,0x5c,0x66,0x77,0x9b,0xf1},
		{0x45,0x45,0x45,0x44,0x44,0x44,0x44,0x44,0x43,0x43,0x43,0x44,0x44,0x47,0x4a,0x4c,0x51,0x58,0x61,0x72,0x90,0xf5},
		{0x45,0x45,0x45,0x44,0x45,0x44,0x44,0x44,0x43,0x43,0x44,0x44,0x45,0x48,0x4a,0x4d,0x53,0x5b,0x64,0x7b,0xa6,0xe6},
		{0x43,0x43,0x44,0x44,0x45,0x44,0x45,0x43,0x43,0x43,0x44,0x45,0x46,0x49,0x4b,0x4f,0x55,0x5d,0x6b,0x83,0xba,0xb9}
	};
	/*0x10 ~ 0x11, battery capacity*/
	static u8 age_batcap[S2MU003_FG_NUM_AGE_STEP][2] = {
		{0x60, 0x09},
		{0xca, 0x08},
		{0xa7, 0x08},
		{0x8e, 0x08},
		{0x16, 0x08}
	};
	/*0x12, 0x13, Rcomp*/
	static u8 age_rcomp[S2MU003_FG_NUM_AGE_STEP][2] = {
		{0xF6, 0x1A},
		{0x00, 0x00},
		{0x00, 0x00},
		{0x00, 0x00},
		{0x00, 0x00}
	};
#endif
#else
	/* 40 ~ B1 */
	static u8 model_param1[] = {
		0x73,0xB,0xF1,0xA,0x6F,0xA,0xED,0x9,0x75,0x9,0x3,0x9,0x99,0x8,0x36,
		0x8,0xDF,0x7,0x8F,0x7,0xB,0x7,0xCB,0x6,0x9B,0x6,0x73,0x6,0x53,0x6,
		0x39,0x6,0x13,0x6,0xE2,0x5,0x8F,0x5,0x77,0x5,0x98,0x4,0xBD,0x1,0x3A,
		0x8,0xD4,0x7,0x6D,0x7,0x6,0x7,0xA0,0x6,0x39,0x6,0xD2,0x5,0x6B,0x5,
		0x4,0x5,0x9E,0x4,0x37,0x4,0xD0,0x3,0x69,0x3,0x3,0x3,0x9C,0x2,0x35,
		0x2,0xCE,0x1,0x68,0x1,0x1,0x1,0x9A,0x0,0x33,0x0,0xF2,0xF,0xfe,0x29,
		0xdc,0x29,0x8a,0x29,0x53,0x29,0x28,0x13,0x26,0x13,0xf2,0xed,0xf2,0xed,
		0xf2,0xed,0xf2,0xed,0xb0,0xad,0xb0,0xba,0xb0,0xba,
	};
	static u8 model_param2[] = {0x3,0x7,0x1c,0xf,0x4,0x19,0x11,0x21,0x1f,0x27,0x1f,0x27,0x2c,0x2c,
	};
	/* D0 ~ E5 */
	static u8 model_param3[] = {0x3A,0x3A,0x3A,0x3A,0x3A,0x3B,0x3B,0x3B,0x3B,0x3C,0x3D,0x3D,0x3E,0x3F,0x3F,0x40,0x41,0x43,0x46,0x48,0x49,0x9A,
	};
#endif

	i2c_smbus_write_byte_data(fuelgauge->i2c, 0x0E, 0x28);

#if defined(CONFIG_A3XE)
		pr_info("%s: Using A3X revision: %d\n", __func__, fuelgauge->pdata->system_rev);
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
		if (fuelgauge->pdata->system_rev == 0) {
			/* set param for REV0.0 */
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x10, 0xFC);
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x11, 0x08);
		} else {
			/* set param for REV0.1 and above */
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x10, 0x60);
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x11, 0x09);
			/* FG Rcomp parameters */
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x12, 0xF6);
			i2c_smbus_write_byte_data(fuelgauge->i2c, 0x13, 0x1A);
		}
#else
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x10, age_batcap[fuelgauge->fg_age_step][0]);
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x11, age_batcap[fuelgauge->fg_age_step][1]);

		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x12, age_rcomp[fuelgauge->fg_age_step][0]);
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x13, age_rcomp[fuelgauge->fg_age_step][1]);
#endif
#else
		/* set param for other models*/
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x10, 0xF0);
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x11, 0x0A);
#endif

#if defined(CONFIG_A3XE)
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
		if (fuelgauge->pdata->system_rev == 0) {
			for(i = 0x40; i < 0xB2; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param1rev0[i - 0x40]);
			}
			for(i = 0xC0; i < 0xCE; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param2rev0[i - 0xC0]);
			}	
			for(i = 0xD0; i < 0xE6; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param3rev0[i - 0xD0]);
			}
		} else {
			for(i = 0x40; i < 0xB2; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param1rev1[i - 0x40]);
			}
			for(i = 0xC0; i < 0xCE; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param2rev1[i - 0xC0]);
			}	
			for(i = 0xD0; i < 0xE6; i++) {
				i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param3rev1[i - 0xD0]);
			}
		}
#else
		for(i = 0x40; i < 0xB2; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, age_model_param1[fuelgauge->fg_age_step][i - 0x40]);
		}
		for(i = 0xC0; i < 0xCE; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, age_model_param2[fuelgauge->fg_age_step][i - 0xC0]);
		}	
		for(i = 0xD0; i < 0xE6; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, age_model_param3[fuelgauge->fg_age_step][i - 0xD0]);
		}
#endif
#else
		for(i = 0x40; i < 0xB2; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param1[i - 0x40]);
		}
		for(i = 0xC0; i < 0xCE; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param2[i - 0xC0]);
		}		
		for(i = 0xD0; i < 0xE6; i++) {
			i2c_smbus_write_byte_data(fuelgauge->i2c, i, model_param3[i - 0xD0]);
		}
#endif

	WA_0_issue_at_init(fuelgauge);
	pr_info("%s: Reset FG completed\n", __func__);

}

static void s2mu003_restart_gauging(struct s2mu003_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	u8 data[2];

	pr_info("%s: Re-calculate SOC and voltage\n", __func__);

	ret = i2c_smbus_write_byte_data(fuelgauge->i2c, 0x1A, 0x33);
	if (ret < 0)
		dev_err(&fuelgauge->i2c->dev, "%s: Error(%d)\n", __func__, ret);

	msleep(700);

	s2mu003_read_reg(fuelgauge->i2c, 0x04, data);

	pr_info("%s: 0x04-> %02x  0x05-> %02x \n",
			__func__, data[0], data[1]);

	s2mu003_read_reg(fuelgauge->i2c, 0x0A, data);

	pr_info("%s: 0x0A-> %02x  0x0B-> %02x \n",
			__func__, data[0], data[1]);
}

static int s2mu003_init_regs(struct s2mu003_fuelgauge_data *fuelgauge)
{
	int ret = 0;

	pr_info("%s: s2mu003 fuelgauge initialize\n", __func__);

	return ret;
}

static void s2mu003_alert_init(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mu003_write_reg(fuelgauge->i2c, S2MU003_REG_IRQ_LVL, data);
}

static bool s2mu003_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (s2mu003_read_reg(client, S2MU003_REG_STATUS, data) < 0)
		return ret;

	dev_dbg(&client->dev, "%s: status to (%02x%02x)\n",
		__func__, data[1], data[0]);

	if (data[1] & (0x1 << 1))
		return true;
	else
		return false;
}

static int s2mu003_set_temperature(struct s2mu003_fuelgauge_data *fuelgauge,
			int temperature)
{
	u8 data[2];
	char val;

	val = temperature / 10;

	if (temperature >= 350)
		val = 0x28;
	else if (temperature >= 150)
		val = 0x19;
	else if (temperature >= 50)
		val = 0xA;
	else
		val = 0x0;
	
	data[0] = val;
	data[1] = 0x00;
	s2mu003_write_reg(fuelgauge->i2c, S2MU003_REG_RTEMP, data);

	dev_dbg(&fuelgauge->i2c->dev, "%s: temperature to (%d)\n",
		__func__, temperature);

	return temperature;
}

static int s2mu003_get_temperature(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	s32 temperature = 0;

	if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RTEMP, data) < 0)
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
static int s2mu003_get_soc(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	int por_state = 0, dump_done;
	u16 compliment;
	int rsoc, i;
	union power_supply_propval value;

	mutex_lock(&fuelgauge->fg_lock);

	dump_done = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x1A);
	por_state = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x19);
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	if(((por_state & 0x10)&&(fuelgauge->age_reset_status != 1)) || ((dump_done & 0x03) == 0)) {
#else
	if((por_state & 0x10) || ((dump_done & 0x03) == 0)) {
#endif
		por_state &= ~0x10;
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x19, por_state);
		value.intval = 0;
		psy_do_property("sec-charger", set,
			POWER_SUPPLY_PROP_CHARGE_INITIALIZE, value);

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		s2mu003_reset_fg(fuelgauge);

	}

	for (i = 0; i < 50; i++) {
		if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RSOC, check_data) < 0)
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

	fuelgauge->info.soc = rsoc;

	dev_info(&fuelgauge->i2c->dev, "%s: raw capacity (0x%x:%d)\n", __func__,
		compliment, rsoc);

	if (rsoc <= BST_LEVEL_SOC) {
		low_soc_flag |= 0x01;
		if (low_soc_flag == 0x03) {
			value.intval = 0;
			psy_do_property("sec-charger", set,
				POWER_SUPPLY_PROP_BOOST_DISABLE, value);
		}
	} else {
		low_soc_flag &= 0xFE;
		value.intval = 1;
		psy_do_property("sec-charger", set,
			POWER_SUPPLY_PROP_BOOST_DISABLE, value);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	return (min(rsoc, 10000) / 10);
}

static int s2mu003_get_rawsoc(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;
	union power_supply_propval value;

	for (i = 0; i < 50; i++) {
		if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RSOC, check_data) < 0)
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

	dev_info(&fuelgauge->i2c->dev, "%s: raw capacity (0x%x:%d)\n", __func__,
			compliment, rsoc);

	if (rsoc <= BST_LEVEL_SOC) {
		low_soc_flag |= 0x01;
		if (low_soc_flag == 0x03) {
			value.intval = 0;
			psy_do_property("sec-charger", set,
				POWER_SUPPLY_PROP_BOOST_DISABLE, value);
		}
	} else {
		low_soc_flag &= 0xFE;
		value.intval = 1;
		psy_do_property("sec-charger", set,
			POWER_SUPPLY_PROP_BOOST_DISABLE, value);
	}

	return min(rsoc, 10000) ;
}

static int s2mu003_get_ocv(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 rocv = 0;

	if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_ROCV, data) < 0)
		return -EINVAL;

	rocv = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: rocv (%d)\n", __func__, rocv);

	return rocv;
}

static int s2mu003_get_vbat(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;
	union power_supply_propval value;

	if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RVBAT, data) < 0)
		return -EINVAL;

	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	if (vbat <= BST_LEVEL_VOLTAGE) {
		low_soc_flag |= 0x02;
		if (low_soc_flag == 0x03) {
			value.intval = 0;
			psy_do_property("sec-charger", set,
				POWER_SUPPLY_PROP_BOOST_DISABLE, value);
		}
	} else {
		low_soc_flag &= 0xFD;
		value.intval = 1;
		psy_do_property("sec-charger", set,
			POWER_SUPPLY_PROP_BOOST_DISABLE, value);
	}

	return vbat;
}

static int s2mu003_get_avgvbat(struct s2mu003_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 new_vbat, old_vbat = 0;
	int cnt;

	for (cnt = 0; cnt < 5; cnt++) {
		if (s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RVBAT, data) < 0)
			return -EINVAL;

		new_vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

		if (cnt == 0)
			old_vbat = new_vbat;
		else
			old_vbat = new_vbat / 2 + old_vbat / 2;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n", __func__, old_vbat);

	return old_vbat;
}

/* if ret < 0, discharge */
static int sec_bat_check_discharge(int vcell)
{
	static int cnt;
	static int pre_vcell = 0;

	if (pre_vcell == 0)
		pre_vcell = vcell;
	else if (pre_vcell > vcell)
		cnt++;
	else if (vcell >= 3400)
		cnt = 0;
	else
		cnt--;

	pre_vcell = vcell;

	if (cnt >= 3)
		return -1;
	else
		return 1;
}

/* judge power off or not by current_avg */
static int s2mu003_get_current_average(struct i2c_client *client)
{
	struct s2mu003_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);
	union power_supply_propval value_bat;
	int vcell, soc, curr_avg;
	int check_discharge;

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_HEALTH, value_bat);
	vcell = s2mu003_get_vbat(fuelgauge);
	soc = s2mu003_get_soc(fuelgauge) / 10;
	check_discharge = sec_bat_check_discharge(vcell);

	/* if 0% && under 3.4v && low power charging(1000mA), power off */
	if (!lpcharge && (soc <= 0) && (vcell < 3400) &&
			((check_discharge < 0) ||
			 ((value_bat.intval == POWER_SUPPLY_HEALTH_OVERHEAT) ||
			  (value_bat.intval == POWER_SUPPLY_HEALTH_COLD)))) {
		pr_info("%s: SOC(%d), Vnow(%d) \n",
				__func__, soc, vcell);
		curr_avg = -1;
	} else {
		curr_avg = 0;
	}

	return curr_avg;
}

/* capacity is  0.1% unit */
static void s2mu003_fg_get_scaled_capacity(
		struct s2mu003_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	dev_dbg(&fuelgauge->i2c->dev,
			"%s: scaled capacity (%d.%d)\n",
			__func__, val->intval/10, val->intval%10);
}

/* capacity is integer */
static void s2mu003_fg_get_atomic_capacity(
		struct s2mu003_fuelgauge_data *fuelgauge,
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
			dev_err(&fuelgauge->i2c->dev,
					"%s: capacity (old %d : new %d)\n",
					__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int s2mu003_fg_check_capacity_max(
		struct s2mu003_fuelgauge_data *fuelgauge, int capacity_max)
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

static int s2mu003_fg_calculate_dynamic_scale(
		struct s2mu003_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mu003_get_rawsoc(fuelgauge) / 10;

	if (raw_soc_val.intval <
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		dev_dbg(&fuelgauge->i2c->dev, "%s: capacity_max (%d)",
				__func__, fuelgauge->capacity_max);
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

	if (capacity != 100) {
		fuelgauge->capacity_max = s2mu003_fg_check_capacity_max(
			fuelgauge, (fuelgauge->capacity_max * 100 / capacity));
	} else  {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 99 / 100);
	}

	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = capacity;

	dev_info(&fuelgauge->i2c->dev, "%s: %d is used for capacity_max\n",
			__func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

bool s2mu003_fuelgauge_fuelalert_init(struct i2c_client *client, int soc)
{
	struct s2mu003_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	/* 1. Set s2mu003 alert configuration. */
	s2mu003_alert_init(fuelgauge);

	if (s2mu003_read_reg(client, S2MU003_REG_IRQ, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= 0xfc;

	/*Disable IDLE_ST, INIT_ST */
	data[1] |= 0x0c;

	s2mu003_write_reg(client, S2MU003_REG_IRQ, data);

	dev_dbg(&client->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
			__func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	fuelgauge->is_fuel_alerted = false;

	return true;
}

bool s2mu003_fuelgauge_is_fuelalerted(struct s2mu003_fuelgauge_data *fuelgauge)
{
	return s2mu003_check_status(fuelgauge->i2c);
}

bool s2mu003_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct s2mu003_fuelgauge_data *fuelgauge = irq_data;
	int ret;

	ret = i2c_smbus_write_byte_data(fuelgauge->i2c, S2MU003_REG_IRQ, 0x00);
	if (ret < 0)
		dev_err(&fuelgauge->i2c->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

bool s2mu003_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
static int s2mu003_fg_aging_check(
		struct s2mu003_fuelgauge_data *fuelgauge, int step)
{
	u8 batcap[2], rcomp[2];
	u8 por_state = 0;
	union power_supply_propval value;
	int charging_enabled = false;

	/*0x10 ~ 0x11, battery capacity*/
	static u8 age_batcap[S2MU003_FG_NUM_AGE_STEP][2] = {
		{0x60, 0x09},
		{0xca, 0x08},
		{0xa7, 0x08},
		{0x8e, 0x08},
		{0x16, 0x08}
	};
	/*0x12, 0x13, Rcomp*/
	static u8 age_rcomp[S2MU003_FG_NUM_AGE_STEP][2] = {
		{0xF6, 0x1A},
		{0x00, 0x00},
		{0x00, 0x00},
		{0x00, 0x00},
		{0x00, 0x00}
	};

	if(step <= (fuelgauge->fg_num_age_step - 1))
		fuelgauge->fg_age_step = step;
	else
		fuelgauge->fg_age_step = fuelgauge->fg_num_age_step - 1;

	/*Check age_step of FG*/
	s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RBATCAP, batcap);
	s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_RZADJ, rcomp);

	pr_info("%s: [Long life] orig. batcap : %02x %02x, fg_age_step batcap : %02x %02x\n", __func__, \
		batcap[0], batcap[1], age_batcap[fuelgauge->fg_age_step][0], age_batcap[fuelgauge->fg_age_step][1]);
	pr_info("%s: [Long life] orig. Rcomp : %02x %02x, fg_age_step Rcomp : %02x %02x\n", __func__, \
		rcomp[0], rcomp[1], age_rcomp[fuelgauge->fg_age_step][0], age_rcomp[fuelgauge->fg_age_step][1]);

	if ((batcap[0] != age_batcap[fuelgauge->fg_age_step][0]) || \
		(batcap[1] != age_batcap[fuelgauge->fg_age_step][1]) || \
		(rcomp[0] != age_rcomp[fuelgauge->fg_age_step][0]) || \
		(rcomp[1] != age_rcomp[fuelgauge->fg_age_step][1])) {

		pr_info("%s: [Long life] reset gauge for age forecast , step[%d] \n", __func__, fuelgauge->fg_age_step);

		fuelgauge->age_reset_status = 1;

		/*Set POR state to reset fg, when reset_fg is not finished well*/
		por_state = i2c_smbus_read_byte_data(fuelgauge->i2c, 0x19);
		por_state |= 0x10;
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x19, por_state);
		
		/* check charging enable */
		psy_do_property("sec-charger", get, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		charging_enabled = value.intval;

		if(charging_enabled == true) {
			pr_info("%s: [Long life] disable charger for reset gauge age forecast \n", __func__);
			value.intval = 0;
			psy_do_property("sec-charger", set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		}

		s2mu003_reset_fg(fuelgauge);

		if(charging_enabled == true) {
			/*Charger can be turned off during fg reset.
			Check again before charger turn on.*/
			psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
			charging_enabled = (value.intval == POWER_SUPPLY_STATUS_CHARGING)?true:false;
		
			if(charging_enabled == true) {
				pr_info("%s: [Long life] enable charger for reset gauge age forecast \n", __func__);
				value.intval = 1;
				psy_do_property("sec-charger", set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
			}
		}

		/*Clear POR state*/
		por_state &= ~0x10;
		i2c_smbus_write_byte_data(fuelgauge->i2c, 0x19, por_state);
		fuelgauge->age_reset_status = 0;

		return 1;
	}
	return 0;
}
#endif

static int s2mu003_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu003_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mu003_fuelgauge_data, psy_fg);
	union power_supply_propval chg_current;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		return -ENODATA;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mu003_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
			case SEC_BATTERY_VOLTAGE_AVERAGE:
				val->intval = s2mu003_get_avgvbat(fuelgauge);
				break;
			case SEC_BATTERY_VOLTAGE_OCV:
				val->intval = s2mu003_get_ocv(fuelgauge);
				break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		psy_do_property("sec-charger", get,
			POWER_SUPPLY_PROP_CURRENT_NOW, chg_current);
		val->intval = chg_current.intval;
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = s2mu003_get_current_average(fuelgauge->i2c);
		if (val->intval >= 0) {
			psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_CURRENT_NOW, chg_current);
			val->intval = chg_current.intval;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mu003_get_rawsoc(fuelgauge);
		} else {
			val->intval = s2mu003_get_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				s2mu003_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
					fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				s2mu003_fuelgauge_fuelalert_init(fuelgauge->i2c,
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
				s2mu003_fg_get_atomic_capacity(fuelgauge, val);
		}

		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mu003_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mu003_fg_set_property(struct power_supply *psy,
                            enum power_supply_property psp,
                            const union power_supply_propval *val)
{
	struct s2mu003_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mu003_fuelgauge_data, psy_fg);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			if (fuelgauge->pdata->capacity_calculation_type &
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
#if defined(CONFIG_PREVENT_SOC_JUMP)
				s2mu003_fg_calculate_dynamic_scale(fuelgauge, val->intval);
#else
				s2mu003_fg_calculate_dynamic_scale(fuelgauge, 100);
#endif
			}
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			fuelgauge->cable_type = val->intval;
			if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
				fuelgauge->is_charging = false;
			else
				fuelgauge->is_charging = true;
		case POWER_SUPPLY_PROP_CAPACITY:
			if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
				fuelgauge->initial_update_of_soc = true;
				s2mu003_restart_gauging(fuelgauge);
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
		case POWER_SUPPLY_PROP_TEMP_AMBIENT:
			s2mu003_set_temperature(fuelgauge, val->intval);
			break;
		case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
			dev_info(&fuelgauge->i2c->dev,
				"%s: capacity_max changed, %d -> %d\n",
				__func__, fuelgauge->capacity_max, val->intval);
			fuelgauge->capacity_max = s2mu003_fg_check_capacity_max(fuelgauge, val->intval);
			fuelgauge->initial_update_of_soc = true;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			/* rt5033_fg_reset_capacity_by_jig_connection(fuelgauge->i2c); */
			break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		case POWER_SUPPLY_PROP_UPDATE_BATTERY_DATA:
			s2mu003_fg_aging_check(fuelgauge, val->intval);
			break;
#endif
		default:
			return -EINVAL;
	}

	return 0;
}

static void s2mu003_fg_isr_work(struct work_struct *work)
{

	struct s2mu003_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mu003_fuelgauge_data, isr_work.work);
	u8 fg_alert_status[2];
	int ret;

	ret = s2mu003_read_reg(fuelgauge->i2c, S2MU003_REG_IRQ, fg_alert_status);
	if (ret < 0)
		dev_err(&fuelgauge->i2c->dev, "%s: Error(%d)\n", __func__, ret);

	dev_info(&fuelgauge->i2c->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
			__func__, fg_alert_status[1], fg_alert_status[0], fuelgauge->pdata->fg_irq);

	fg_alert_status[0] &= 0x03;
	if (fg_alert_status[0]) {
		wake_unlock(&fuelgauge->fuel_alert_wake_lock);
	}

	if (fg_alert_status[0] & S2MU003_VBAT_L) {
		pr_info("%s : Battery Voltage is Very Low!!\n", __func__);
	} else if (fg_alert_status[0] & S2MU003_SOC_L) {
		pr_info("%s : SOC is Very Low!!\n", __func__);
	}
}

static irqreturn_t s2mu003_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mu003_fuelgauge_data *fuelgauge = irq_data;

	dev_info(&fuelgauge->i2c->dev, "%s: FG ALRTB IRQ!! \n",	__func__);

	if (!fuelgauge->is_fuel_alerted) {
		wake_lock(&fuelgauge->fuel_alert_wake_lock);
		fuelgauge->is_fuel_alerted = true;
		schedule_delayed_work(&fuelgauge->isr_work, 0);
	}
	return IRQ_HANDLED;
}


#ifdef CONFIG_OF
static int s2mu003_fuelgauge_parse_dt(struct s2mu003_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu003-fuelgauge");
	int ret;
	int i, len;
	const u32 *p;

	        /* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		fuelgauge->pdata->fg_irq = of_get_named_gpio(np, "fuelgauge,fuel_alert", 0);
		if (fuelgauge->pdata->fg_irq < 0)
			pr_err("%s error reading fg_irq = %d\n",
				__func__, fuelgauge->pdata->fg_irq);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgauge,repeated_fuelalert");

		np = of_find_node_by_name(NULL, "battery");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_string(np,
				"battery,fuelgauge_name",
				(char const **)&fuelgauge->pdata->fuelgauge_name);
			p = of_get_property(np,
					"battery,input_current_limit", &len);
			if (!p)
				return 1;

			len = len / sizeof(u32);
			fuelgauge->pdata->charging_current =
					kzalloc(sizeof(struct sec_charging_current) * len,
					GFP_KERNEL);

			for(i = 0; i < len; i++) {
				ret = of_property_read_u32_index(np,
					"battery,input_current_limit", i,
					&fuelgauge->pdata->charging_current[i].input_current_limit);
				ret = of_property_read_u32_index(np,
					"battery,fast_charging_current", i,
					&fuelgauge->pdata->charging_current[i].fast_charging_current);
				ret = of_property_read_u32_index(np,
					"battery,full_check_current_1st", i,
					&fuelgauge->pdata->charging_current[i].full_check_current_1st);
				ret = of_property_read_u32_index(np,
					"battery,full_check_current_2nd", i,
					&fuelgauge->pdata->charging_current[i].full_check_current_2nd);
			}
		}
		np = of_find_all_nodes(NULL);
		ret = of_property_read_u32(np, "model_info-hw_rev",
				&fuelgauge->pdata->system_rev);
		if (ret) {
			pr_info("%s: model_info-hw_rev is Empty\n", __func__);
			fuelgauge->pdata->system_rev = 0;
		}
		pr_info("%s: Using A3X revision: %d\n", __func__,
				fuelgauge->pdata->system_rev);
	}

	return 0;
}

static struct of_device_id s2mu003_fuelgauge_match_table[] = {
        { .compatible = "samsung,s2mu003-fuelgauge",},
        {},
};
#else
static int s2mu003_fuelgauge_parse_dt(struct s2mu003_fuelgauge_data *fuelgauge)
{
    return -ENOSYS;
}

#define s2mu003_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static int s2mu003_fuelgauge_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s2mu003_fuelgauge_data *fuelgauge;
	union power_supply_propval raw_soc_val;
	int ret = 0;

	pr_info("%s: S2MU003 Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->i2c = client;

	if (client->dev.of_node) {
		fuelgauge->pdata = devm_kzalloc(&client->dev, sizeof(*(fuelgauge->pdata)),
				GFP_KERNEL);
		if (!fuelgauge->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = s2mu003_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		fuelgauge->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "sec-fuelgauge";

	fuelgauge->psy_fg.name          = fuelgauge->pdata->fuelgauge_name;
	fuelgauge->psy_fg.type          = POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property  = s2mu003_fg_get_property;
	fuelgauge->psy_fg.set_property  = s2mu003_fg_set_property;
	fuelgauge->psy_fg.properties    = s2mu003_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
			ARRAY_SIZE(s2mu003_fuelgauge_props);

	fuelgauge->info.soc = 0;
	low_soc_flag = 0;

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = s2mu003_get_rawsoc(fuelgauge) / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mu003_fg_calculate_dynamic_scale(fuelgauge, 100);

	ret = s2mu003_init_regs(fuelgauge);
	if (ret < 0) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__); 
		/* goto err_data_free; */
	}

	ret = power_supply_register(&client->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mu003_fuelgauge_fuelalert_init(fuelgauge->i2c,
					fuelgauge->pdata->fuel_alert_soc);
		wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");

		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(
					&fuelgauge->isr_work, s2mu003_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			dev_info(&client->dev,
					"%s: fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
						NULL, s2mu003_fg_irq_thread,
						IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING
						| IRQF_ONESHOT,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					dev_err(&client->dev,
							"%s: Failed to Reqeust IRQ\n", __func__);
					goto err_supply_unreg;
				}

				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					dev_err(&client->dev,
							"%s: Failed to Enable Wakeup Source(%d)\n",
							__func__, ret);
			} else {
				dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n",
						__func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

	fuelgauge->initial_update_of_soc = true;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	fuelgauge->fg_num_age_step = S2MU003_FG_NUM_AGE_STEP;
	fuelgauge->age_reset_status = 0;
#endif

	pr_info("%s: S2MU003 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(&fuelgauge->psy_fg);
err_data_free:
	if (client->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id s2mu003_fuelgauge_id[] = {
	{"s2mu003-fuelgauge", 0},
	{}
};

static void s2mu003_fuelgauge_shutdown(struct i2c_client *client)
{
}

static int s2mu003_fuelgauge_remove(struct i2c_client *client)
{
	struct s2mu003_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

#if defined CONFIG_PM
static int s2mu003_fuelgauge_suspend(struct device *dev)
{
        return 0;
}

static int s2mu003_fuelgauge_resume(struct device *dev)
{
        return 0;
}
#else
#define s2mu003_fuelgauge_suspend NULL
#define s2mu003_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mu003_fuelgauge_pm_ops, s2mu003_fuelgauge_suspend,
		s2mu003_fuelgauge_resume);

static struct i2c_driver s2mu003_fuelgauge_driver = {
	.driver = {
		.name = "s2mu003-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mu003_fuelgauge_pm_ops,
		.of_match_table = s2mu003_fuelgauge_match_table,
	},
	.probe  = s2mu003_fuelgauge_probe,
	.remove = s2mu003_fuelgauge_remove,
	.shutdown   = s2mu003_fuelgauge_shutdown,
	.id_table   = s2mu003_fuelgauge_id,
};

static int __init s2mu003_fuelgauge_init(void)
{
	pr_info("%s: S2MU003 Fuelgauge Init\n", __func__);
	return i2c_add_driver(&s2mu003_fuelgauge_driver);
}

static void __exit s2mu003_fuelgauge_exit(void)
{
	i2c_del_driver(&s2mu003_fuelgauge_driver);
}
module_init(s2mu003_fuelgauge_init);
module_exit(s2mu003_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MU003 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
