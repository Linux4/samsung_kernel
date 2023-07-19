/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "inv_mpu_iio.h"

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20645"},
	{128, "ICM10320"},
};

static int inv_set_dmp(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH,
						st->dmp_start_address >> 8);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH + 1,
						st->dmp_start_address & 0xff);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}


static int inv_read_timebase(struct inv_mpu_state *st)
{
	int result;
	u8 d;
	s8 t;

	result = inv_set_bank(st, BANK_SEL_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_TIMEBASE_CORRECTION_PLL, 1, &d);
	if (result)
		return result;
	t = abs(d & 0x7f);
	if (d & 0x80)
		t = -t;

	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_I2C].base_time  = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;


	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}


int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
			(st->chip_config.accel_fs << SHIFT_ACCEL_FS) | 0);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_secondary(struct inv_mpu_state *st)
{
	int r;

	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_I2C_MST_CTRL, BIT_I2C_MST_P_NSR);
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
							MIN_MST_ODR_CONFIG);
	if (r)
		return r;

	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}
static int inv_init_secondary(struct inv_mpu_state *st)
{
	st->slv_reg[0].addr = REG_I2C_SLV0_ADDR;
	st->slv_reg[0].reg  = REG_I2C_SLV0_REG;
	st->slv_reg[0].ctrl = REG_I2C_SLV0_CTRL;
	st->slv_reg[0].d0   = REG_I2C_SLV0_DO;

	st->slv_reg[1].addr = REG_I2C_SLV1_ADDR;
	st->slv_reg[1].reg  = REG_I2C_SLV1_REG;
	st->slv_reg[1].ctrl = REG_I2C_SLV1_CTRL;
	st->slv_reg[1].d0   = REG_I2C_SLV1_DO;

	st->slv_reg[2].addr = REG_I2C_SLV2_ADDR;
	st->slv_reg[2].reg  = REG_I2C_SLV2_REG;
	st->slv_reg[2].ctrl = REG_I2C_SLV2_CTRL;
	st->slv_reg[2].d0   = REG_I2C_SLV2_DO;

	st->slv_reg[3].addr = REG_I2C_SLV3_ADDR;
	st->slv_reg[3].reg  = REG_I2C_SLV3_REG;
	st->slv_reg[3].ctrl = REG_I2C_SLV3_CTRL;
	st->slv_reg[3].d0   = REG_I2C_SLV3_DO;

	return 0;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size         = ACCEL_DATA_SZ;
	st->sensor[SENSOR_ALS].sample_size           = ALS_DATA_SZ;
	st->sensor[SENSOR_ACCEL].odr_addr         = ODR_ACCEL;
	st->sensor[SENSOR_ALS].odr_addr           = ODR_ALS;
	st->sensor[SENSOR_ACCEL].counter_addr         = ODR_CNTR_ACCEL;
	st->sensor[SENSOR_ALS].counter_addr           = ODR_CNTR_ALS;
	st->sensor[SENSOR_ACCEL].output         = ACCEL_SET;
	st->sensor[SENSOR_ALS].output           = ALS_SET;

	st->sensor[SENSOR_ACCEL].a_en           = true;
	st->sensor[SENSOR_ALS].a_en             = false;
	st->sensor[SENSOR_ACCEL].c_en         = false;
	st->sensor[SENSOR_ALS].c_en           = false;
	st->sensor[SENSOR_ACCEL].p_en         = false;
	st->sensor[SENSOR_ALS].p_en           = false;
	st->sensor[SENSOR_ACCEL].engine_base         = ENGINE_ACCEL;
	st->sensor[SENSOR_ALS].engine_base           = ENGINE_I2C;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_ALS].base = SENSOR_ALS;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_ALS_WAKE].base = SENSOR_ALS;


	st->sensor_l[SENSOR_L_ACCEL].header         = ACCEL_HDR;
	st->sensor_l[SENSOR_L_ALS].header           = ALS_HDR;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].header    = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_ALS_WAKE].header      = ALS_WAKE_HDR;

	st->sensor_l[SENSOR_L_ACCEL].wake_on = false;
	st->sensor_l[SENSOR_L_ALS].wake_on           = false;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_ALS_WAKE].wake_on           = true;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].sample_size =
							ACCEL_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].output = ACCEL_ACCURACY_SET;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_ACCEL_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->firmware = 0;

	inv_init_secondary(st);
	inv_init_sensor_struct(st);

	res = inv_read_timebase(st);
	if (res)
		return res;
	res = inv_set_dmp(st);
	if (res)
		return res;

	res = inv_set_accel_sf(st);
	if (res)
		return res;

	res = inv_set_secondary(st);

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	return res;
}

int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;
	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;

	result = inv_set_power(st, true);
	if (result)
		return result;
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	INVLOG(IL4, "REG_WHO_AM_I=%x\n", v);
	if (result)
	{
		return result;
	}

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_init_config(st);
	if (result)
		return result;

	INVLOG(IL4, "plat->sec_slave_type %d\n", plat->sec_slave_type);
	if (SECONDARY_SLAVE_TYPE_ALS == plat->read_only_slave_type)
		st->chip_config.has_als = 1;
	else
		st->chip_config.has_als = 0;

	result = mem_r(MPU_SOFT_REV_ADDR, 1, &v);
	if (result)
		return result;
	INVLOG(IL4, "swRev=%x\n", v);
#if 0
	if (v & MPU_SOFT_REV_MASK) {
		INVLOG(ERR, "incorrect software revision=%x\n", v);

		return -EINVAL;
	} else {
		if (v == SW_REV_LP_EN_MODE)
			st->chip_config.lp_en_mode_off = 0;
		else
			st->chip_config.lp_en_mode_off = 1;
	}
#else
	st->chip_config.lp_en_mode_off = 0;
#endif
	result = inv_set_power(st, false);

	return result;
}
