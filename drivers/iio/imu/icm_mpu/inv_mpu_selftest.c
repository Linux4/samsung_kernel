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
#define pr_fmt(fmt) "inv_mpu: " fmt

#include "inv_mpu_iio.h"

/* full scale and LPF setting */
#define SELFTEST_ACCEL_FS           ((7 << 3) | 0)

/* register settings */
#define SELFTEST_ACCEL_SMPLRT_DIV       1
#define SELFTEST_ACCEL_DEC3_CFG         0

/* wait time before collecting data */
#define MAX_PACKETS                     20
#define SELFTEST_WAIT_TIME              (MAX_PACKETS * 2)
#define DEF_ST_STABLE_TIME              20
#define DEF_ST_PRECISION                1000
#define DEF_ST_ACCEL_FS_MG              2000UL
#define DEF_ST_SCALE                    32768
#define DEF_ST_TRY_TIMES                2
#define DEF_ST_ACCEL_RESULT_SHIFT       1
#define DEF_ST_SAMPLES                  200

#define DEF_ACCEL_ST_SHIFT_DELTA_MIN    500
#define DEF_ACCEL_ST_SHIFT_DELTA_MAX    1500

/* Accel Self Test Absolute Limits ST_AL (mg) */
#define DEF_ACCEL_ST_AL_MIN             225
#define DEF_ACCEL_ST_AL_MAX             675

struct recover_regs {
	/* Bank#0 */
	u8 fifo_cfg;			/* REG_FIFO_CFG */
	u8 user_ctrl;			/* REG_USER_CTRL */
	u8 lp_config;			/* REG_LP_CONFIG */
	u8 int_enable;			/* REG_INT_ENABLE */
	u8 int_enable_1;		/* REG_INT_ENABLE_1 */
	u8 fifo_en;				/* REG_FIFO_EN */
	u8 fifo_en_2;			/* REG_FIFO_EN_2 */
	u8 fifo_rst;			/* REG_FIFO_RST */

	/* Bank#2 */
	u8 accel_smplrt_div_1;	/* REG_ACCEL_SMPLRT_DIV_1 */
	u8 accel_smplrt_div_2;	/* REG_ACCEL_SMPLRT_DIV_2 */
	u8 accel_config;		/* REG_ACCEL_CONFIG */
	u8 accel_config_2;		/* REG_ACCEL_CONFIG_2 */
};

static struct recover_regs saved_regs;


static const u16 mpu_st_tb[256] = {
	2620, 2646, 2672, 2699, 2726, 2753, 2781, 2808,
	2837, 2865, 2894, 2923, 2952, 2981, 3011, 3041,
	3072, 3102, 3133, 3165, 3196, 3228, 3261, 3293,
	3326, 3359, 3393, 3427, 3461, 3496, 3531, 3566,
	3602, 3638, 3674, 3711, 3748, 3786, 3823, 3862,
	3900, 3939, 3979, 4019, 4059, 4099, 4140, 4182,
	4224, 4266, 4308, 4352, 4395, 4439, 4483, 4528,
	4574, 4619, 4665, 4712, 4759, 4807, 4855, 4903,
	4953, 5002, 5052, 5103, 5154, 5205, 5257, 5310,
	5363, 5417, 5471, 5525, 5581, 5636, 5693, 5750,
	5807, 5865, 5924, 5983, 6043, 6104, 6165, 6226,
	6289, 6351, 6415, 6479, 6544, 6609, 6675, 6742,
	6810, 6878, 6946, 7016, 7086, 7157, 7229, 7301,
	7374, 7448, 7522, 7597, 7673, 7750, 7828, 7906,
	7985, 8065, 8145, 8227, 8309, 8392, 8476, 8561,
	8647, 8733, 8820, 8909, 8998, 9088, 9178, 9270,
	9363, 9457, 9551, 9647, 9743, 9841, 9939, 10038,
	10139, 10240, 10343, 10446, 10550, 10656, 10763, 10870,
	10979, 11089, 11200, 11312, 11425, 11539, 11654, 11771,
	11889, 12008, 12128, 12249, 12371, 12495, 12620, 12746,
	12874, 13002, 13132, 13264, 13396, 13530, 13666, 13802,
	13940, 14080, 14221, 14363, 14506, 14652, 14798, 14946,
	15096, 15247, 15399, 15553, 15709, 15866, 16024, 16184,
	16346, 16510, 16675, 16842, 17010, 17180, 17352, 17526,
	17701, 17878, 18057, 18237, 18420, 18604, 18790, 18978,
	19167, 19359, 19553, 19748, 19946, 20145, 20347, 20550,
	20756, 20963, 21173, 21385, 21598, 21814, 22033, 22253,
	22475, 22700, 22927, 23156, 23388, 23622, 23858, 24097,
	24338, 24581, 24827, 25075, 25326, 25579, 25835, 26093,
	26354, 26618, 26884, 27153, 27424, 27699, 27976, 28255,
	28538, 28823, 29112, 29403, 29697, 29994, 30294, 30597,
	30903, 31212, 31524, 31839, 32157, 32479, 32804
};

static void inv_show_saved_setting(struct inv_mpu_state *st)
{
	INVLOG(IL4, " REG_FIFO_CFG : 0x%02X\n", saved_regs.fifo_cfg);
	INVLOG(IL4, " REG_USER_CTRL : 0x%02X\n", saved_regs.user_ctrl);
	INVLOG(IL4, " REG_LP_CONFIG : 0x%02X\n", saved_regs.lp_config);
	INVLOG(IL4, " REG_INT_ENABLE : 0x%02X\n", saved_regs.int_enable);
	INVLOG(IL4, " REG_INT_ENABLE_1 : 0x%02X\n", saved_regs.int_enable_1);
	INVLOG(IL4, " REG_FIFO_EN : 0x%02x\n", saved_regs.fifo_en);
	INVLOG(IL4, " REG_FIFO_EN_2 : 0x%02x\n", saved_regs.fifo_en_2);
	INVLOG(IL4, " REG_FIFO_RST : 0x%02x\n", saved_regs.fifo_rst);
	INVLOG(IL4, " REG_ACCEL_SMPLRT_DIV_1 : 0x%02x\n",
			saved_regs.accel_smplrt_div_1);
	INVLOG(IL4, " REG_ACCEL_SMPLRT_DIV_2 : 0x%02x\n",
			saved_regs.accel_smplrt_div_2);
	INVLOG(IL4, " REG_ACCEL_CONFIG : 0x%02x\n", saved_regs.accel_config);
	INVLOG(IL4, " REG_ACCEL_CONFIG_2 : 0x%02x\n", saved_regs.accel_config_2);
}

static int inv_save_setting(struct inv_mpu_state *st)
{
	int result;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	result = inv_plat_read(st, REG_FIFO_CFG, 1, &saved_regs.fifo_cfg);
	if (result)
		return result;
	result = inv_plat_read(st, REG_USER_CTRL, 1, &saved_regs.user_ctrl);
	if (result)
		return result;
	result = inv_plat_read(st, REG_LP_CONFIG, 1, &saved_regs.lp_config);
	if (result)
		return result;
	result = inv_plat_read(st, REG_INT_ENABLE, 1, &saved_regs.int_enable);
	if (result)
		return result;
	result = inv_plat_read(st, REG_INT_ENABLE_1, 1,
			&saved_regs.int_enable_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_FIFO_EN, 1, &saved_regs.fifo_en);
	if (result)
		return result;
	result = inv_plat_read(st, REG_FIFO_EN_2, 1, &saved_regs.fifo_en_2);
	if (result)
		return result;
	result = inv_plat_read(st, REG_FIFO_RST, 1, &saved_regs.fifo_rst);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;

	result = inv_plat_read(st, REG_ACCEL_SMPLRT_DIV_1, 1,
			&saved_regs.accel_smplrt_div_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_ACCEL_SMPLRT_DIV_2, 1,
			&saved_regs.accel_smplrt_div_2);
	if (result)
		return result;
	result = inv_plat_read(st, REG_ACCEL_CONFIG, 1,
			&saved_regs.accel_config);
	if (result)
		return result;
	result = inv_plat_read(st, REG_ACCEL_CONFIG_2, 1,
			&saved_regs.accel_config_2);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	inv_show_saved_setting(st);

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return result;
}

static int inv_recover_setting(struct inv_mpu_state *st)
{
	int result;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	/* Stop sensors */
	result = inv_plat_single_write(st, REG_PWR_MGMT_2,
		BIT_PWR_PRESSURE_STBY | BIT_PWR_ACCEL_STBY | BIT_PWR_GYRO_STBY);

	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;


	result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_1,
			saved_regs.accel_smplrt_div_1);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_2,
			saved_regs.accel_smplrt_div_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
			saved_regs.accel_config);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2,
			saved_regs.accel_config_2);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	/* Restore FIFO configurations */
	result = inv_plat_single_write(st, REG_FIFO_CFG, saved_regs.fifo_cfg);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_LP_CONFIG, saved_regs.lp_config);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_INT_ENABLE,
			saved_regs.int_enable);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_INT_ENABLE_1,
			saved_regs.int_enable_1);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_FIFO_EN, saved_regs.fifo_en);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_FIFO_EN_2, saved_regs.fifo_en_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_FIFO_RST, MAX_5_BIT_VALUE);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_FIFO_RST, saved_regs.fifo_rst);
	if (result)
		return result;

	/* Reset DMP */
	result = inv_plat_single_write(st, REG_USER_CTRL,
			(saved_regs.user_ctrl & (~BIT_FIFO_EN)) | BIT_DMP_RST);
	if (result)
		return result;
	msleep(DMP_RESET_TIME);

	result = inv_set_power(st, false);
	return result;
}


/**
* inv_check_accel_self_test() - check accel self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_accel_self_test(struct inv_mpu_state *st,
						int *reg_avg, int *st_avg, int *ratio)
{
	int ret_val;
	int st_shift_prod[3], st_shift_cust[3], i;
	u8 *regs;
	int otp_value_zero = 0;

#define ACCEL_ST_AL_MIN ((DEF_ACCEL_ST_AL_MIN * DEF_ST_SCALE \
				 / DEF_ST_ACCEL_FS_MG) * DEF_ST_PRECISION)
#define ACCEL_ST_AL_MAX ((DEF_ACCEL_ST_AL_MAX * DEF_ST_SCALE \
				 / DEF_ST_ACCEL_FS_MG) * DEF_ST_PRECISION)

	pr_info("reg_bias : %d, %d, %d \n", reg_avg[0], reg_avg[1], reg_avg[2]);
	pr_info("st_avg : %d, %d, %d \n", st_avg[0], st_avg[1], st_avg[2]);

	ret_val = 0;
	regs = st->accel_st_data;
	INVLOG(IL4,"self_test accel shift_code - %02x %02x %02x\n",
						regs[0], regs[1], regs[2]);

	for (i = 0; i < 3; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = mpu_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	INVLOG(IL4,"self_test accel st_shift_prod - %+d %+d %+d\n",
		st_shift_prod[0], st_shift_prod[1], st_shift_prod[2]);

	if (!otp_value_zero) {
		/* Self Test Pass/Fail Criteria A */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = st_avg[i] - reg_avg[i];
			ratio[i] = 	st_shift_cust[i] / st_shift_prod[i] - DEF_ST_PRECISION;
			if (st_shift_cust[i] < DEF_ACCEL_ST_SHIFT_DELTA_MIN
							* st_shift_prod[i]) {
				ret_val = 1;
				pr_info("error : st_shift_cust %d is smaller than %d\n", st_shift_cust[0], DEF_ACCEL_ST_SHIFT_DELTA_MIN
							* st_shift_prod[i]);
			}
			if (st_shift_cust[i] > DEF_ACCEL_ST_SHIFT_DELTA_MAX
					* st_shift_prod[i]) {
				ret_val = 1;
				pr_info("error : st_shift_cust %d is bigger than %d\n", st_shift_cust[0], DEF_ACCEL_ST_SHIFT_DELTA_MIN
							* st_shift_prod[i]);
			}
		}
	} else {
		/* Self Test Pass/Fail Criteria B */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = abs(st_avg[i] - reg_avg[i]);
			if ((st_shift_cust[i] < ACCEL_ST_AL_MIN) ||
					(st_shift_cust[i] > ACCEL_ST_AL_MAX))
				ret_val = 1;
		}
	}
	INVLOG(IL4,"self_test accel st_shift_cust - %+d %+d %+d\n",
		 st_shift_cust[0], st_shift_cust[1], st_shift_cust[2]);

	return ret_val;
}

static int inv_setup_selftest(struct inv_mpu_state *st)
{
	int result;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
	{
		INVLOG(ERR, "Fail to set bank 0.\n");
		return result;
	}
	/* Wake up */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_CLK_PLL);
	if (result)
	{
		INVLOG(ERR, "REG_PWR_MGMT_1 write error.\n");
		return result;
	}
	
	/* Save the current settings */
	result = inv_save_setting(st);
	if (result)
	{
		INVLOG(ERR, "inv_save_setting error.\n");
		return result;
	}

	/* Stop sensors */
	result = inv_plat_single_write(st, REG_PWR_MGMT_2,
			BIT_PWR_PRESSURE_STBY | BIT_PWR_ACCEL_STBY |
			BIT_PWR_GYRO_STBY);

	if (result)
	{
		INVLOG(ERR, "REG_PWR_MGMT_2 write error.\n");
		return result;
	}

	/* Enable FIFO */
	result = inv_plat_single_write(st, REG_USER_CTRL,
			BIT_FIFO_EN | st->i2c_dis);
	if (result)
	{
		INVLOG(ERR, "REG_USER_CTRL write error.\n");
		return result;
	}

	/* Set cycle mode */
	result = inv_plat_single_write(st, REG_LP_CONFIG,
			BIT_I2C_MST_CYCLE | BIT_ACCEL_CYCLE | BIT_GYRO_CYCLE);

	if (result)
	{
		INVLOG(ERR, "REG_LP_CONFIG write error.\n");
		return result;
	}

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
	{
		INVLOG(ERR, "Fail to set bank 2.\n");
		return result;
	}


	result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_1, 0);
	if (result)
	{
		INVLOG(ERR, "REG_ACCEL_SMPLRT_DIV_1 write error.\n");
		return result;
	}

	result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_2,
			SELFTEST_ACCEL_SMPLRT_DIV);
	if (result)
	{
		INVLOG(ERR, "REG_ACCEL_SMPLRT_DIV_2 write error.\n");
		return result;
	}

	result = inv_plat_single_write(st, REG_ACCEL_CONFIG, SELFTEST_ACCEL_FS);
	if (result)
	{
		INVLOG(ERR, "REG_ACCEL_CONFIG write error.\n");
		return result;
	}

	result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2,
			SELFTEST_ACCEL_DEC3_CFG);
	if (result)
	{
		INVLOG(ERR, "REG_ACCEL_CONFIG_2 write error.\n");
		return result;
	}

	result = inv_set_bank(st, BANK_SEL_1);
	if (result)
	{
		INVLOG(ERR, "Fail to set bank 1.\n");
		return result;
	}


	result = inv_plat_read(st, REG_SELF_TEST4, 1, &st->accel_st_data[0]);
	if (result)
	{
		INVLOG(ERR, "REG_SELF_TEST4 read error.\n");
		return result;
	}

	result = inv_plat_read(st, REG_SELF_TEST5, 1, &st->accel_st_data[1]);
	if (result)
	{
		INVLOG(ERR, "REG_SELF_TEST5 read error.\n");
		return result;
	}

	result = inv_plat_read(st, REG_SELF_TEST6, 1, &st->accel_st_data[2]);
	if (result)
	{
		INVLOG(ERR, "REG_SELF_TEST5 read error.\n");
		return result;
	}

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
	{
		INVLOG(ERR, "Fail to set bank 0.\n");
		return result;
	}

	/* Restart sensors. It SHOULD NOT be removed. */
	result = inv_plat_single_write(st, REG_PWR_MGMT_2,
			BIT_PWR_PRESSURE_STBY);
	if (result)
	{
		INVLOG(ERR, "REG_PWR_MGMT_2 write error.\n");
		return result;
	}

	msleep(50);

	return result;
}

static int inv_selftest_read_samples(struct inv_mpu_state *st, enum INV_SENSORS
					type, int *sum_result, int *s)
{
	u8 w;
	u16 fifo_count;
	s16 vals[3];
	u8 d[HARDWARE_FIFO_SIZE];
	int r, i, j, t, packet_count;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	r = inv_plat_single_write(st, REG_FIFO_EN_2, 0);
	if (r)
	{
		INVLOG(ERR, "REG_FIFO_EN_2 write error.\n");
		return r;
	}

	/* Reset FIFO */
	r = inv_plat_single_write(st, REG_FIFO_RST, 0x1F);
	if (r)
	{
		INVLOG(ERR, "REG_FIFO_RST write error.\n");
		return r;
	}
	r = inv_plat_single_write(st, REG_FIFO_RST, 0x1E);
	if (r)
	{
		INVLOG(ERR, "REG_FIFO_RST write error.\n");
		return r;
	}

	w = BIT_ACCEL_FIFO_EN;

	while (*s < DEF_ST_SAMPLES) {
		r = inv_plat_single_write(st, REG_FIFO_EN_2, w);
		if (r)
		{
			INVLOG(ERR, "REG_FIFO_EN_2 write error.\n");
			return r;
		}

		mdelay(SELFTEST_WAIT_TIME);

		r = inv_plat_single_write(st, REG_FIFO_EN_2, 0);
		if (r)
		{
			INVLOG(ERR, "REG_FIFO_EN_2 write error.\n");
			return r;
		}

		r = inv_plat_read(st, REG_FIFO_COUNT_H, FIFO_COUNT_BYTE, d);
		if (r)
		{
			INVLOG(ERR, "REG_FIFO_COUNT_H read error.\n");
			return r;
		}
		fifo_count = be16_to_cpup((__be16 *)(&d[0]));
		INVLOG(IL4, "self_test fifo_count - %d\n",  fifo_count);

		packet_count = fifo_count / BYTES_PER_SENSOR;
		r = inv_plat_read(st, REG_FIFO_R_W, packet_count * BYTES_PER_SENSOR, d);
		if (r)
		{
			INVLOG(ERR, "REG_FIFO_R_W read error.\n");
			return r;
		}


		i = 0;
		while (i < packet_count) {
			for (j = 0; j < THREE_AXES; j++) {
				t = 2 * j + i * BYTES_PER_SENSOR;
				vals[j] = (s16)be16_to_cpup((__be16 *)(&d[t]));
				sum_result[j] += vals[j];
			}
			pr_debug("self_test data - %d %+d %+d %+d",
						*s, vals[0], vals[1], vals[2]);
			(*s)++;
			i++;
		}
	}
	return 0;
}

/*
 *  inv_do_test_accel() - do the actual test of self testing
 */
static int inv_do_test_accel(struct inv_mpu_state *st, int *accel_result,
		int *accel_st_result)
{
	int result, i, j;
	int accel_s;
	u8 w;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	for (i = 0; i < THREE_AXES; i++) {
		accel_result[i] = 0;
		accel_st_result[i] = 0;
	}

	accel_s = 0;
	result = inv_selftest_read_samples(st, SENSOR_ACCEL,
			accel_result, &accel_s);
	if (result)
	{
		INVLOG(ERR, "inv_selftest_read_samples error\n");
		return result;
	}

	for (j = 0; j < THREE_AXES; j++) {
		accel_result[j] = accel_result[j] / accel_s;
		accel_result[j] *= DEF_ST_PRECISION;
	}

	/* Set Self-Test Bit */
	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
	{
		INVLOG(ERR, "inv_set_bank error\n");
		return result;
	}
	w = BIT_ACCEL_CTEN | SELFTEST_ACCEL_DEC3_CFG;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, w);
	if (result)
	{
		INVLOG(ERR, "inv_plat_single_write error\n");
		return result;
	}

	result = inv_set_bank(st, BANK_SEL_0);
	msleep(DEF_ST_STABLE_TIME);

	accel_s = 0;
	result = inv_selftest_read_samples(st, SENSOR_ACCEL,
			accel_st_result, &accel_s);
	if (result)
	{
		INVLOG(ERR, "inv_selftest_read_samples error\n");
		return result;
	}

	for (j = 0; j < THREE_AXES; j++) {
		accel_st_result[j] = accel_st_result[j] / accel_s;
		accel_st_result[j] *= DEF_ST_PRECISION;
	}

	INVLOG(IL4, "accel=%d, %d, %d\n", accel_result[0], accel_result[1],
							accel_result[2]);

	return 0;
}
/*
 *  inv_do_test() - do the actual test of self testing
 */
/*
 *  inv_accel_self_test() - main function to do hardware self test
 */
int inv_accel_self_test(struct inv_mpu_state *st, int *accel_ratio)
{
	int result;
	int accel_bias_st[THREE_AXES], accel_bias_regular[THREE_AXES];
	int test_times, i;
	char accel_result;

	accel_result = 0;

	result = inv_setup_selftest(st);
	if (result)
		goto test_fail;

	/* accel test */
	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		result = inv_do_test_accel(st, accel_bias_regular,
				accel_bias_st);
		if (result == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (result)
		goto test_fail;
	pr_debug("self_test accel bias_regular - %+d %+d %+d\n",
						accel_bias_regular[0],
						accel_bias_regular[1],
						accel_bias_regular[2]);
	pr_debug("self_test accel bias_st - %+d %+d %+d\n",
						accel_bias_st[0],
						accel_bias_st[1],
						accel_bias_st[2]);

	for (i = 0; i < 3; i++)
		st->accel_st_bias[i] = accel_bias_regular[i] / DEF_ST_PRECISION;

	accel_result = !inv_check_accel_self_test(st,
				accel_bias_regular, accel_bias_st, accel_ratio);

test_fail:
	inv_recover_setting(st);
	return 	accel_result;
}


/*
 *  inv_hw_self_test() - main function to do hardware self test
 */
int inv_hw_self_test(struct inv_mpu_state *st)
{
	int result;
	int accel_bias_st[THREE_AXES], accel_bias_regular[THREE_AXES];
	int accel_ratio[THREE_AXES];
	int test_times, i;
	char accel_result;
	accel_result = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = inv_setup_selftest(st);
	if (result)
	{
		INVLOG(ERR, "inv_setup_selftest error.\n");
		goto test_fail;
	}

	/* accel test */
	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		result = inv_do_test_accel(st, accel_bias_regular,
				accel_bias_st);
		if (result == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (result)
		goto test_fail;
	pr_debug("self_test accel bias_regular - %+d %+d %+d\n",
						accel_bias_regular[0],
						accel_bias_regular[1],
						accel_bias_regular[2]);
	pr_debug("self_test accel bias_st - %+d %+d %+d\n",
						accel_bias_st[0],
						accel_bias_st[1],
						accel_bias_st[2]);

	for (i = 0; i < 3; i++) {
		st->accel_st_bias[i] = accel_bias_regular[i] / DEF_ST_PRECISION;
	}

	accel_result = inv_check_accel_self_test(st,
				accel_bias_regular, accel_bias_st, accel_ratio);
test_fail:
	inv_recover_setting(st);
	return (accel_result << DEF_ST_ACCEL_RESULT_SHIFT);
}
