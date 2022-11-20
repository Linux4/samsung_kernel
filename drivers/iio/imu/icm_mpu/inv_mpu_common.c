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
#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#else
#include <linux/alarmtimer.h>
#endif
s64 get_time_ns(void)
{
#if 1
	struct timespec ts;

#if defined(ANDROID_ALARM_ACTIVATED)
	ts = ktime_to_timespec(alarm_get_elapsed_realtime());
#else
	ts = ktime_to_timespec(ktime_get_boottime());
#endif
	return timespec_to_ns(&ts);
#else
	s64 nsec;
	struct timespec tv;

	getnstimeofday(&tv);

	nsec = tv.tv_sec * 1000000000LL + tv.tv_nsec;

	pr_info("[INV] %s time = %lld\n", __func__, nsec);	// log level 5

	return nsec;
#endif
}

/*****************************************************************************/
/*      Pedo logging mode features                                           */
s64 get_time_timeofday(void)
{
	s64 nsec;
	struct timespec tv;

	getnstimeofday(&tv);

	nsec = tv.tv_sec * 1000000000LL + tv.tv_nsec;

	INVLOG(IL4, "time = %lld\n", nsec);	// log level 5

	return nsec;
}
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

int inv_q30_mult(int a, int b)
{
#define DMP_MULTI_SHIFT                 30
	u64 temp;
	int result;

	temp = (u64)a * b;
	result = (int)(temp >> DMP_MULTI_SHIFT);

	return result;
}

/* inv_read_secondary(): set secondary registers for reading.
			The chip must be set as bank 3 before calling.
 */
int inv_read_secondary(struct inv_mpu_state *st, int ind, int addr,
			int reg, int len)
{
	int result;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	result = inv_plat_single_write(st, st->slv_reg[ind].addr,
						INV_MPU_BIT_I2C_READ | addr);
	if (result)
	{
		INVLOG(ERR, "INV_MPU_BIT_I2C_READ write error\n");
		return result;
	}
	result = inv_plat_single_write(st, st->slv_reg[ind].reg, reg);
	if (result)
	{
		INVLOG(ERR, "Secondary register 0x%x write error\n", reg);
		return result;
	}
	result = inv_plat_single_write(st, st->slv_reg[ind].ctrl,
						INV_MPU_BIT_SLV_EN | len);

	INVLOG(FUNC_INT_ENTRY, "Exit\n");

	return result;
}

int inv_execute_read_secondary(struct inv_mpu_state *st, int ind, int addr,
						int reg, int len, u8 *d)
{
	int result;
	INVLOG(FUNC_ENTRY, "Enter\n");

	inv_set_bank(st, BANK_SEL_3);
	result = inv_read_secondary(st, ind, addr, reg, len);
	if (result)
	{
		INVLOG(ERR, "Fail to read secondary.\n");
		inv_set_bank(st, BANK_SEL_0);
		return result;
	}
	inv_set_bank(st, BANK_SEL_0);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis |
							BIT_I2C_MST_EN);
	msleep(SECONDARY_INIT_WAIT);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
	{
		INVLOG(ERR, "Fail to write REG_USER_CTRL.\n");
		return result;
	}
	result = inv_plat_read(st, REG_EXT_SLV_SENS_DATA_00, len, d);
	if (result)
	{
		INVLOG(ERR, "Fail to read REG_EXT_SLV_SENS_DATA_00.\n");
		return result;
	}

	INVLOG(FUNC_ENTRY, "Exit\n");
	return result;
}

/* inv_write_secondary(): set secondary registers for writing.
			The chip must be set as bank 3 before calling.
 */
int inv_write_secondary(struct inv_mpu_state *st, int ind, int addr,
							int reg, int v)
{
	int result;

	result = inv_plat_single_write(st, st->slv_reg[ind].addr, addr);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].reg, reg);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].ctrl,
						INV_MPU_BIT_SLV_EN | 1);

	result = inv_plat_single_write(st, st->slv_reg[ind].d0, v);

	return result;
}

int inv_execute_write_secondary(struct inv_mpu_state *st, int ind, int addr,
								int reg, int v)
{
	int result;
	INVLOG(FUNC_INT_ENTRY, "enter\n");

	inv_set_bank(st, BANK_SEL_3);
	result = inv_write_secondary(st, ind, addr, reg, v);
	if (result)
	{
		INVLOG(ERR, "Fail to write secondary.\n");
		inv_set_bank(st, BANK_SEL_0);
		return result;
	}
	inv_set_bank(st, BANK_SEL_0);
	result = inv_plat_single_write(st, REG_USER_CTRL,
					st->i2c_dis | BIT_I2C_MST_EN);
	if (result)
	{
		INVLOG(ERR, "Fail to write REG_USER_CTRL.\n");
		return result;
	}
	msleep(SECONDARY_INIT_WAIT);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
	{
		INVLOG(ERR, "Fail to write REG_USER_CTRL.\n");
		return result;
	}

	INVLOG(FUNC_INT_ENTRY, "exit\n");
	return result;
}
int inv_set_power(struct inv_mpu_state *st, bool power_on)
{
	u8 d;
	int r;

	INVLOG(FUNC_INT_ENTRY, "enter\n");
	inv_set_bank(st, BANK_SEL_0);
	if ((!power_on) == st->chip_config.is_asleep)
	{
		INVLOG(IL4, "It's already in %s\n", st->chip_config.is_asleep?"sleep":"wake");
		return 0;
	}

	d = BIT_CLK_PLL;
	if (!power_on && !st->wom_enable && !st->pedlog.enabled)
	{
		INVLOG(IL4, "Set to sleep\n");
		d |= BIT_SLEEP;
	}

	r = inv_plat_single_write(st, REG_PWR_MGMT_1, d);
	if (r)
	{
		INVLOG(ERR, "Fail to write REG_PWR_MGMT_1\n");
		return r;
	}

	if (power_on)
	{
		INVLOG(IL4, "Make delay for power on.\n");
		usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	}

	st->chip_config.is_asleep = !power_on;

	INVLOG(FUNC_INT_ENTRY, "exit\n");
	return 0;
}

int inv_stop_dmp(struct inv_mpu_state *st)
{
	return inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
}

static int inv_lp_en_off_mode(struct inv_mpu_state *st, bool on)
{
	int r;

	INVLOG(FUNC_LOW_ENTRY, "Enter.\n");
	if (!st->chip_config.is_asleep)
		return 0;

	r = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_CLK_PLL);
	st->chip_config.is_asleep = 0;

	INVLOG(FUNC_LOW_ENTRY, "Exit.\n");
	return r;
}

static int inv_lp_en_on_mode(struct inv_mpu_state *st, bool on)
{
	int r;
	u8 w;
	INVLOG(FUNC_LOW_ENTRY, "Enter\n");

	if ((!st->chip_config.is_asleep)  &&
			((!on) == st->chip_config.lp_en_set))
		return 0;

	w = BIT_CLK_PLL;
	if (!on)
		w |= BIT_LP_EN;
	r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);
	st->chip_config.is_asleep = 0;
	st->chip_config.lp_en_set = (!on);
	INVLOG(FUNC_LOW_ENTRY, "Exit.\n");

	return r;
}

inv_error_t inv_switch_power_in_lp(struct inv_mpu_state *st, bool on)
{
	inv_error_t result;
	static u8 count = 0;

	if (on)
	{
		count++;
		if(count > 1)
		{
			INVLOG(IL2, "already on: %d\n", count);
			return INV_SUCCESS;
		}
	}
	else
	{
		if(count <= 0)
		{
			INVLOG(IL2, "already off: %d\n", count);
			count = 0;
			return INV_SUCCESS;
		}

		count--;
		if(count > 0)
		{
			INVLOG(IL2, "off: left count %d\n", count);
			return INV_SUCCESS;
		}
	}

	INVLOG(IL4, "Change lp mode. ON %d count %d.\n", on, count);

	if (st->chip_config.lp_en_mode_off)
		result = inv_lp_en_off_mode(st, on);
	else
		result = inv_lp_en_on_mode(st, on);

	usleep_range(100,100);

	return result;
}

int inv_set_bank(struct inv_mpu_state *st, u8 bank)
{
	int r;

	r = inv_plat_single_write(st, REG_BANK_SEL, bank);

	return r;
}

int write_be16_to_mem(struct inv_mpu_state *st, u16 data, int addr)
{
	u8 d[2];
	
	d[0] = (data >> 8) & 0xff;
	d[1] = data & 0xff;
	
	return mem_w(addr, sizeof(d), d);
}

int write_be32_to_mem(struct inv_mpu_state *st, u32 data, int addr)
{
	cpu_to_be32s(&data);
	return mem_w(addr, sizeof(data), (u8 *)&data);
}

int read_be16_from_mem(struct inv_mpu_state *st, u16 *o, int addr)
{
	int result;
	u8 d[2];

	result = mem_r(addr, 2, (u8 *)&d);
	*o = d[0] << 8 | d[1];

	return result;
}

int read_be32_from_mem(struct inv_mpu_state *st, u32 *o, int addr)
{
	int result;
	u32 d;

	result = mem_r(addr, 4, (u8 *)&d);
	*o = be32_to_cpup((__be32 *)(&d));

	return result;
}

u32 inv_get_cntr_diff(u32 curr_counter, u32 prev)
{
	u32 diff;

	if (curr_counter > prev)
		diff = curr_counter - prev;
	else
		diff = 0xffffffff - prev + curr_counter + 1;

	return diff;
}

/**
 *  inv_write_2bytes() - Write 2 bytes data to designated DMP address.
 *  @st:	Device driver instance.
 *  @addr:	DMP address.
 *  @data:	2 Bytes data
 */
int inv_write_2bytes(struct inv_mpu_state *st, int addr, int data)
{
	u8 d[2];

	if (data < 0 || data > USHRT_MAX)
		return -EINVAL;

	d[0] = (u8)((data >> 8) & 0xff);
	d[1] = (u8)(data & 0xff);

	return mem_w(addr, ARRAY_SIZE(d), d);
}

/**
 *  inv_write_cntl() - Write control word to designated address.
 *  @st:	Device driver instance.
 *  @wd:        control word.
 *  @en:	enable/disable.
 *  @cntl:	control address to be written.
 */
int inv_write_cntl(struct inv_mpu_state *st, u16 wd, bool en, int cntl)
{
	int result;
	u8 reg[2], d_out[2];

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = mem_r(cntl, 2, d_out);
	if (result)
		return result;
	reg[0] = ((wd >> 8) & 0xff);
	reg[1] = (wd & 0xff);
	if (!en) {
		d_out[0] &= ~reg[0];
		d_out[1] &= ~reg[1];
	} else {
		d_out[0] |= reg[0];
		d_out[1] |= reg[1];
	}
	result = mem_w(cntl, 2, d_out);

	return result;
}

static inv_error_t inv_control_state(struct inv_mpu_state *st, int do_backup)
{
	int i = 0;
	static struct android_l_sensor sensor_l[SENSOR_L_NUM_MAX];

	if(do_backup)
	{
		for(i = 0 ; i < SENSOR_L_NUM_MAX ; i++)
			sensor_l[i] = st->sensor_l[i];
	}
	else
	{
		for(i = 0 ; i < SENSOR_L_NUM_MAX ; i++)
			st->sensor_l[i] = sensor_l[i];
	}

	return INV_SUCCESS;
}

inv_error_t inv_store_state(struct inv_mpu_state *st)
{
	inv_control_state(st, true);

	return INV_SUCCESS;
}

inv_error_t inv_restore_state(struct inv_mpu_state *st)
{
	inv_control_state(st, false);

	return INV_SUCCESS;
}


