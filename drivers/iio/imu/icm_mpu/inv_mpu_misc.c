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

int cadence_arr[21] = {
	PEDLOG_CADENCE1, PEDLOG_CADENCE2, PEDLOG_CADENCE3,
	PEDLOG_CADENCE4, PEDLOG_CADENCE5, PEDLOG_CADENCE6,
	PEDLOG_CADENCE7, PEDLOG_CADENCE8, PEDLOG_CADENCE9,
	PEDLOG_CADENCE10,PEDLOG_CADENCE11,PEDLOG_CADENCE12,
	PEDLOG_CADENCE13,PEDLOG_CADENCE14,PEDLOG_CADENCE15,
	PEDLOG_CADENCE16,PEDLOG_CADENCE17,PEDLOG_CADENCE18,
	PEDLOG_CADENCE19,PEDLOG_CADENCE20,PEDLOG_BACKUP2};

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped)
{
	int r;
	inv_set_bank(st, BANK_SEL_0);

	r = read_be32_from_mem(st, ped, PEDSTD_STEPCTR);

	return r;
}
int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped)
{
	int r;
	inv_set_bank(st, BANK_SEL_0);

	r = read_be32_from_mem(st, ped, PEDSTD_TIMECTR);

	return r;
}

inv_error_t inv_read_pedometer_counter(struct inv_mpu_state *st)
{
	int result;
	u32 last_step_counter, curr_counter;
	u64 counter;

	INVLOG(FUNC_ENTRY, "enter.\n");
	inv_set_bank(st, BANK_SEL_0);

	result = read_be32_from_mem(st, &last_step_counter, STPDET_TIMESTAMP);
	if (result)
	{
		INVLOG(ERR, "Fail to read STPDET_TIMESTAMP.\n");
		return INV_ERR_DMP_REG;
	}

	if (0 != last_step_counter) {
		result = read_be32_from_mem(st, &curr_counter, DMPRATE_CNTR);
		if (result)
		{
			INVLOG(ERR, "Fail to read DMPRATE_CNTR.\n");
			return INV_ERR_DMP_REG;
		}
		counter = inv_get_cntr_diff(curr_counter, last_step_counter);
		st->ped.last_step_time = get_time_ns() - counter *
						st->eng_info[ENGINE_ACCEL].dur;
	}

	INVLOG(FUNC_ENTRY, "Exit.\n");
	return INV_SUCCESS;
}

inv_error_t inv_set_pedometer_step_threshold(struct inv_mpu_state *st, int threshold)
{
	int result;

	INVLOG(FUNC_ENTRY, "enter.\n");
	inv_set_bank(st, BANK_SEL_0);

	result = inv_write_2bytes(st, PEDSTD_SB, threshold);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_SB.\n");
		return INV_ERR_DMP_REG;
	}
	result = inv_write_2bytes(st, PEDSTD_SB_SAVE, threshold);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_SB_SAVE.\n");
		return INV_ERR_DMP_REG;
	}
	result = inv_write_2bytes(st, PEDSTD_SB_LO, threshold);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_SB_LO.\n");
		return INV_ERR_DMP_REG;
	}
	result = inv_write_2bytes(st, PEDSTD_SB_HI, threshold);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_SB_HI.\n");
		return INV_ERR_DMP_REG;
	}
	result = write_be32_to_mem(st, PEDO_PEAK_THRESHOLD, PEDSTD_PEAKTHRSH);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_PEAKTHRSH.\n");
		return INV_ERR_DMP_REG;
	}
	result = inv_write_2bytes(st, PEDSTD_SB_TIME, 150);
	if (result)
	{
		INVLOG(ERR, "Fail to write PEDSTD_SB_TIME.\n");
		return INV_ERR_DMP_REG;
	}

	st->ped.step_thresh = threshold;

	INVLOG(FUNC_ENTRY, "exit.\n");
	return INV_SUCCESS;
}

int inv_check_sensor_on(struct inv_mpu_state *st)
{
	int i;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].on = false;
	st->chip_config.wake_on = false;
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on && st->sensor_l[i].rate) {
			st->sensor[st->sensor_l[i].base].on = true;
			st->chip_config.wake_on |= st->sensor_l[i].wake_on;
		}
	}
	if (st->step_detector_wake_l_on ||
			st->step_counter_wake_l_on ||
			st->chip_config.pick_up_enable ||
			st->smd.on ||
			st->chip_config.tilt_enable)
		st->chip_config.wake_on = true;

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return 0;
}

int inv_check_sensor_rate(struct inv_mpu_state *st)
{
	int i;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = 0;
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			INVLOG(IL4, "sensor.rate %d, sensor_l.rate %d\n", 
				st->sensor[st->sensor_l[i].base].rate, st->sensor_l[i].rate);
			st->sensor[st->sensor_l[i].base].rate = max(
				st->sensor[st->sensor_l[i].base].rate,
				st->sensor_l[i].rate);
		}
	}
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			if (st->sensor_l[i].rate)
				st->sensor_l[i].div =
					st->sensor[st->sensor_l[i].base].rate
						/ st->sensor_l[i].rate;
			else
				st->sensor_l[i].div = 0xffff;
		}
	}

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return 0;
}

/*****************************************************************************/
/*      SysFS for Pedo logging mode features                                 */


inv_error_t inv_enable_pedometer_interrupt(struct inv_mpu_state *st, bool en)
{
	inv_error_t result;
	u8 data[2];
	u16 reg_data;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);
	result = mem_r(MOTION_EVENT_CTL, 2, data);
	if(result)
	{
		INVLOG(ERR, "Fail to read MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	INVLOG(IL4, "MOTION_EVENT_CTL 0x%x%x\n", data[0], data[1]);

	reg_data = data[0] << 8 | data[1];
	if(en)
		reg_data |= (PEDOMETER_INT_EN << 8);
	else
		reg_data &= ~(PEDOMETER_INT_EN << 8);

	INVLOG(IL3, "Write to MOTION_EVENT_CTL : 0x%x\n", reg_data);
	result = inv_write_2bytes(st, MOTION_EVENT_CTL, reg_data);
	if(result)
	{
		INVLOG(ERR, "Fail to write MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	result = mem_r(MOTION_EVENT_CTL, 2, data);
	if(result)
	{
		INVLOG(ERR, "Fail to read MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	INVLOG(IL3, "Read MOTION_EVENT_CTL : 0x%x%x\n", data[0], data[1]);

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return INV_SUCCESS;
}

inv_error_t inv_enable_pedlog_interrupt(struct inv_mpu_state *st, bool en)
{
	inv_error_t result;
	u8 data[2];
	u16 reg_data;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);
	result = mem_r(MOTION_EVENT_CTL, 2, data);
	if(result)
	{
		INVLOG(ERR, "Fail to read MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	INVLOG(IL4, "MOTION_EVENT_CTL 0x%x%x\n", data[0], data[1]);

	reg_data = data[0] << 8 | data[1];
	if(en)
		reg_data |= S_HEALTH_EN;
	else
		reg_data &= ~S_HEALTH_EN;

	INVLOG(IL3, "Write to MOTION_EVENT_CTL : 0x%x\n", reg_data);
	result = inv_write_2bytes(st, MOTION_EVENT_CTL, reg_data);
	if(result)
	{
		INVLOG(ERR, "Fail to write MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	result = mem_r(MOTION_EVENT_CTL, 2, data);
	if(result)
	{
		INVLOG(ERR, "Fail to read MOTION_EVENT_CTL.\n");
		return INV_ERR_DMP_REG;
	}
	INVLOG(IL3, "Read MOTION_EVENT_CTL : 0x%x%x\n", data[0], data[1]);

	INVLOG(FUNC_INT_ENTRY, "Exit\n");
	return INV_SUCCESS;
}

int inv_enable_pedlog(struct inv_mpu_state *st, bool en, bool irq_en)
{
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	if(en)
	{
		st->pedlog.start_time = get_time_timeofday();
		st->pedlog.interrupt_time = -1;
		st->pedlog.stop_time = -1;
		st->pedlog.valid_count = 0;
		if(st->pedlog.interrupt_duration <= 0 || st->pedlog.interrupt_duration  > 20)
			st->pedlog.interrupt_duration = 20; //default 20 minute;

		INVLOG(IL2, "Enable start:%lld, Interrupt Duration:%d\n",
			st->pedlog.start_time,
			st->pedlog.interrupt_duration);

		/* reset pedlog interrupt period */
		inv_set_pedlog_interrupt_period(st,
				st->pedlog.interrupt_duration);

		/* set 1minute timer value */
		st->pedlog.tick_count = 3360;
		inv_set_pedlog_update_timer(st, st->pedlog.tick_count);

		inv_clear_pedlog_cadence(st);
	}
	else
	{
		if(st->pedlog.start_time > 0) {
			st->pedlog.stop_time = get_time_timeofday();

			inv_get_pedlog_cadence(st, 0, 19 , st->pedlog.cadence);
			inv_get_pedlog_valid_count(st);
			pr_info("[INVN:%s] Disable start:%lld valid_count %d\n",
				__func__, st->pedlog.start_time,
				st->pedlog.valid_count);

			if (st->pedlog.valid_count > 20)
			{
				INVLOG(IL2, "Valid count is lager than 20. It is set to 20.\n");
				st->pedlog.valid_count = 20;
				st->pedlog.start_time = st->pedlog.stop_time - 1200000000000LL;
			}

			if(irq_en) {
				st->pedlog.interrupt_mask |= PEDLOG_INT_CADENCE;
				complete(&st->pedlog.wait);
			}
		}
	}
	inv_enable_pedometer_interrupt(st, !en);
	inv_enable_pedlog_interrupt(st, en);

	st->pedlog.enabled = en;
	st->pedlog.state = PEDLOG_STAT_STOP;

	return INV_SUCCESS;
}

u16 inv_calc_pedlog_tick_cnt(struct inv_mpu_state *st,
			s64 start_time, s64 end_time)
{
	u32 time_diff_ms = 0, time_dur_ms = 0;
	s64 time_period_ns = 0;
	s64 time_dur_ns, time_diff_ns;
	u16 tick_cnt;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	time_dur_ms = st->pedlog.interrupt_duration * 60 * 1000;
	time_period_ns = time_dur_ms * 1000000LL;
	time_dur_ns = end_time - start_time;
	time_diff_ns = time_dur_ns - time_period_ns;

	if(time_diff_ns > -500000000LL && time_diff_ns < 500000000LL)
	{
		tick_cnt = st->pedlog.tick_count;
	}
	else
	{
		time_diff_ms = (u32)((time_dur_ns) >> 9) / 1953;
		tick_cnt =  time_dur_ms * (st->pedlog.tick_count + 1)
				/ time_diff_ms - 1;
	}

	return tick_cnt;
}

int inv_get_pedlog_valid_count(struct inv_mpu_state *st)
{
	s64 diff_time = 0;
	s32 sec_diff_time = 0;
	s16 valid_count = 0;
	s16 valid_count_diff = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	if(st->pedlog.interrupt_time == -1) {
		diff_time = st->pedlog.stop_time - st->pedlog.start_time;

		pr_info("[INVN:%s] start %lld, stop %lld\n",
			__func__, st->pedlog.start_time,
			st->pedlog.stop_time);
	} else {
		diff_time = st->pedlog.stop_time - st->pedlog.interrupt_time;

		pr_info("[INVN:%s] interrupt %lld, stop %lld\n",
			__func__, st->pedlog.interrupt_time,
			st->pedlog.stop_time);
	}

	sec_diff_time = (s32)(diff_time >> 30) / 56;
	pr_info("[INVN:%s] diff_time %lld, "
		"(s32)(diff_time>>30) %d\n", __func__,
		diff_time, (s32)(diff_time>>30));

	valid_count = (s16)sec_diff_time + 1;
	st->pedlog.valid_count = valid_count;

	pr_info("[INVN:%s] diff %d, count %d\n", __func__,
		sec_diff_time, valid_count);

	valid_count_diff = st->pedlog.valid_count_verify - st->pedlog.valid_count;
	if (valid_count_diff == 1)
	{
		INVLOG(IL2, "Last cadence %d. Valid_count %d -> %d.\n",
			st->pedlog.valid_count_verify, st->pedlog.valid_count,
			st->pedlog.valid_count_verify);
		st->pedlog.valid_count = st->pedlog.valid_count_verify;
	}
	return 0;
}

ssize_t inv_get_pedlog_instant_cadence(struct inv_mpu_state *st, char* buf)
{
	int i = 0, result;
	s32 cadence_array[PEDLOG_CADENCE_LEN] = {0,};
	s32 cadence;
	char concat[256];
	s64 start_time = 0, end_time = 0;
	bool is_data_ready = false;
	u16 valid_count = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	if(st->pedlog.enabled)
		result = inv_get_pedlog_cadence(st, 0, 19, cadence_array);
	
	if(st->pedlog.enabled ||st->pedlog.stop_time > 0)
		is_data_ready = true;
	
	if(is_data_ready) {
		/* If interrupt time exists, time difference should be
		   calculated between interrupt & stop time. */
		if(st->pedlog.interrupt_time > 0)
		{
			start_time = st->pedlog.interrupt_time;
		}
		else if(st->pedlog.start_time > 0)
		{
			start_time = st->pedlog.start_time;
		}

		if(st->pedlog.stop_time > 0)
			end_time = st->pedlog.stop_time;
		else {
			end_time = get_time_timeofday();
		}
	}

	/*timestamps*/
	sprintf(concat, "%lld,%lld,", start_time, end_time);
	strcat(buf, concat);
	
	/*valid count of cadence*/
	if(st->pedlog.enabled)
		valid_count = (u16)(((s32)((end_time - start_time) >> 30)) / 56) + 1;
	sprintf(concat, "%d,", valid_count);
	strcat(buf, concat);

	pr_info("%s start: %lld, stop: %lld, vc: %d\n", __func__,
		start_time, end_time, valid_count);
	
	for( i = 0; i < PEDLOG_CADENCE_LEN; i++) {
		cadence = cadence_array[i];
	
		sprintf(concat, "%u,", cadence);
		strcat(buf, concat);
	}
	strcat(buf, "\n");

	return strlen(buf);
}

inv_error_t inv_set_pedlog_interrupt_period(struct inv_mpu_state *st, u16 period)
{
	u16 s_health_period;
	inv_error_t result;

	INVLOG(FUNC_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	s_health_period = period - 1; // DMP period is offset by 1
	result = write_be16_to_mem(st, s_health_period, PEDLOG_INT_PERIOD);
	if (result) {
		INVLOG(ERR, "Fail to write PEDLOG_INT_PERIOD\n");
		return INV_ERR_DMP_REG;
	}

	result = write_be16_to_mem(st, s_health_period, PEDLOG_INT_PERIOD2);

	if (result) {
		INVLOG(ERR, "Fail to write PEDLOG_INT_PERIOD2\n");
		return INV_ERR_DMP_REG;
	}

	INVLOG(FUNC_ENTRY, "Exit\n");
	return INV_SUCCESS;
}

s64 inv_get_pedlog_elapsed_time(struct inv_mpu_state *st)
{
	int result = 0;
	u16 period_min, period2_min;
	u16 period_sec, period2_sec;
	u64 elapsed = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	/*minute*/
	result = read_be16_from_mem(st, &period_min, PEDLOG_INT_PERIOD);
	result |= read_be16_from_mem(st, &period2_min,PEDLOG_INT_PERIOD2);
	if(result)
	{
		pr_err("[INVN:%s] Fail to read minutes.\n", __func__);
		return result;
	}

	/*second*/
	result = read_be16_from_mem(st, &period_sec, PEDLOG_MIN_CNTR);
	result |= read_be16_from_mem(st, &period2_sec, PEDLOG_MIN_CONST);
	if(result)
	{
		pr_err("[INVN:%s] Fail to read seconds.\n", __func__);
		return result;
	}
	elapsed = (u64)(period2_min - period_min) *60000000000LL + 
		(u64)(period2_sec - period_sec) * 20000000LL;

	pr_info("[pedlog:%s] period_min:%d period2_min:%d\n",
		__func__, period_min, period2_min);
	pr_info("[pedlog:%s] period_sec:%d period2_sec:%d\n",
		__func__, period_sec, period2_sec);

	pr_info("%s : elapsed = %lld, period = %d, period2= %d\n",
		__func__,elapsed, period_sec, period2_sec);
	return elapsed;
}

inv_error_t inv_get_pedlog_cadence(struct inv_mpu_state *st, u8 start, u8 end, s32 cadence[]) 
{
	int i, result = 0;
	u16 cad;
	u8 len;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	len = end - start + 1;

	if(len < 0 || len >  PEDLOG_CADENCE_LEN + 1)
	{
		INVLOG(ERR, "Length is not available.\n");
		return INV_ERROR;
	}

	st->pedlog.valid_count_verify = 0;
	for(i = start; i <= end; i ++) {
		result = read_be16_from_mem(st, &cad, cadence_arr[i]);
		if (result)
		{
			INVLOG(ERR, "Fail to read Cadence data.\n");
			return INV_ERR_DMP_REG;
		}

		cadence[i - start] = cad & 0xFFFF;

		/* check last data position */
		if (cadence[i - start])
		{
			st->pedlog.valid_count_verify = i - start + 1;
		}

		INVLOG(IL3, "Cadence r/w %u %u \n", cad>>8, cad&0xFF);
	}

	if (st->pedlog.valid_count_verify)
	{
		INVLOG(IL3, "Valid count from cadence checker is %d.\n",
			st->pedlog.valid_count_verify);
	}

	return INV_SUCCESS;
}

int inv_reset_pedlog_update_timer(struct inv_mpu_state *st)
{
	int result = 0;
	u16 d = 0;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);
	/*reset cadence update timer*/
	result = read_be16_from_mem(st, &d, PEDLOG_MIN_CONST);
	if (result)
	{
		INVLOG(ERR, "Fail to read PEDLOG_MIN_CONST.\n");
		return INV_ERR_DMP_REG;
	}

	result = write_be16_to_mem(st, d, PEDLOG_MIN_CNTR);
	if (result)
	{
		INVLOG(ERR, "Fail to read PEDLOG_MIN_CNTR.\n");
		return INV_ERR_DMP_REG;
	}

	INVLOG(IL4,"Timer = %d\n", d);

	return INV_SUCCESS;
}

int inv_set_pedlog_update_timer(struct inv_mpu_state *st, u16 timer)
{
	int result = 0;
	u16 data = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	/*reset cadence update timer*/
	result = read_be16_from_mem(st, &data, PEDLOG_MIN_CONST);
	if(result)
		goto read_fail;

	pr_info("[INVN:%s] before timer=%d\n", __func__, data);	// log level 4

	result = write_be16_to_mem(st, timer, PEDLOG_MIN_CONST);
	result |= read_be16_from_mem(st, &data, PEDLOG_MIN_CONST);

	if(result)
		goto read_fail;

	pr_info("%s after timer=%d\n", __func__, data);
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	return result;

read_fail:
	pr_err("%s error\n", __func__);
	return result;
}

int inv_get_pedlog_update_timer(struct inv_mpu_state *st, char *buf)
{
	int result = 0;
	u16 data = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	/*reset cadence update timer*/
	result = read_be16_from_mem(st, &data, PEDLOG_MIN_CONST);
	if(result)
		goto read_fail;

	pr_info("[pedlog] %s before timer=%d\n", __func__, data);
	sprintf(buf, "%d\n", data);

	return result;

read_fail:
	pr_err("[pedlog] %s error\n", __func__);
	return result;
}


int inv_clear_pedlog_cadence(struct inv_mpu_state *st) 
{
	int i, result = 0;
	u32 data = 0;
	u16 cadence = 0;
	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	inv_set_bank(st, BANK_SEL_0);

	/*reset cadence update timer*/
	result = inv_reset_pedlog_update_timer(st);
	if(result)
		goto read_fail;

	/*reset counter : copy current to previous*/
	result = read_be32_from_mem(st, &data, PEDLOG_STEP_CNT);
	pr_info("[INVN:%s] Pedo logging mode STEP CNT %d\n", __func__, data);
	if(result)
		goto read_fail;

	result = write_be32_to_mem(st, data, PEDLOG_STEP_CNT_P);
	if(result)
		goto read_fail;

	/*reset data*/
	for(i = 0; i < 21; i ++) {
		result = write_be16_to_mem(st, cadence, cadence_arr[i]);
		if(result)
			goto read_fail;
	}

	return 0;

read_fail:
	pr_err("[pedlog] %s error\n", __func__);
	return result;
}

inv_error_t inv_lpf_enable(struct inv_mpu_state *st, unsigned char enable)
{
	inv_error_t result;
	u8 cfg1[1] = {0,};
	u8 cfg2[1] = {0,};

	INVLOG(FUNC_INT_ENTRY, "Enter.\n");

	inv_switch_power_in_lp(st, true);
	inv_set_bank(st, BANK_SEL_2);	// select reg bank 2
	result = inv_plat_read(st, REG_ACCEL_CONFIG, 1, cfg1);
	if(result)
	{
		INVLOG(ERR, "Fail to read REG_ACCEL_CONFIG.\n");
		goto error;
	}
	INVLOG(IL4, "REG_ACCEL_CONFIG 0x%x\n", cfg1[0]);
	result = inv_plat_read(st, REG_ACCEL_CONFIG_2, 1, cfg2);
	if(result)
	{
		INVLOG(ERR, "Fail to read REG_ACCEL_CONFIG.\n");
		goto error;
	}
	INVLOG(IL4, "REG_ACCEL_CONFIG_2 0x%x\n", cfg2[0]);

	if(enable)
	{
		INVLOG(IL4, "LPF enable.\n");
		cfg1[0] |= ACCEL_FCHOICE | ACCEL_DLPCFG;
		cfg2[0] |= DEC3_CFG;
	}
	else
	{
		INVLOG(IL4, "LPF disable.\n");
		cfg1[0] = ACCEL_FS_SEL << SHIFT_ACCEL_FS;
		cfg2[0] = 0;
	}
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG, cfg1[0]);
	if(result)
	{
		INVLOG(ERR, "Fail to read REG_ACCEL_CONFIG.\n");
		goto error;
	}
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, cfg2[0]);
	if(result)
	{
		INVLOG(ERR, "Fail to read REG_ACCEL_CONFIG.\n");
		goto error;
	}
	result = INV_SUCCESS;
error:
	inv_set_bank(st, BANK_SEL_0);	// select reg bank 0

	inv_switch_power_in_lp(st, false);
	return result;
}

inv_error_t inv_wom_enable(struct inv_mpu_state *st, unsigned char enable)
{
	int result = 0;
	u8 d[4] = {0, 0, 0, 0};
	INVLOG(FUNC_INT_ENTRY, "Enter\n");

	INVLOG(IL2, ": %d\n", enable);
	if(enable)
	{
		u8 mask;
#if defined(CONFIG_SENSORS)
		st->reactive_state = 0;
#endif
		// set bank 0
		inv_set_bank(st, BANK_SEL_0);	// select reg bank 0

		// PWR_MGMT_1:	Sleep = 0, gyro_standby = 0
		// these are the bits we need to clear
		mask = BIT_SLEEP;
		result = inv_plat_read(st, REG_PWR_MGMT_1, 1, d);
		INVLOG(IL4, "REG_PWR_MGMT_1 0x%x\n", d[0]);
		// if either sleep or gyro standby enabled
		if(d[0] & mask)
		{
			d[0] &= ~mask;
			result = inv_plat_single_write(st, REG_PWR_MGMT_1, d[0]);

			// wait at least 100usec....
			mdelay(1);
			if(result)
			{
				INVLOG(ERR, "Fail to write to REG_PWR_MGMT_1\n");
				return result;
			}
		}

		// PWR_MGMT_2:	accel on.  Should have gyro off, but not forcing that
		result = inv_plat_read(st, REG_PWR_MGMT_2, 1, d);
		d[0] &= ~BIT_PWR_ACCEL_STBY;

		result += inv_plat_single_write(st, REG_PWR_MGMT_2, d[0]);
		if(result)
		{
			INVLOG(ERR, "Fail to write to REG_PWR_MGMT_2\n");
			return result;
		}

		// set bank 0
		inv_set_bank(st, BANK_SEL_0);	// select reg bank 0

		// Enable WOM interrupt
		result = inv_plat_read(st, REG_INT_ENABLE, 1, d);
		INVLOG(IL4, "REG_INT_ENABLE 0x%x\n", d[0]);
		d[0] |= BIT_WOM_INT_EN;

		result += inv_plat_single_write(st, REG_INT_ENABLE, d[0]);
		if(result)
		{
			INVLOG(ERR, "Fail to write to REG_INT_ENABLE\n");
			return result;
		}

		// bank 2
		inv_set_bank(st, BANK_SEL_2);	// select reg bank 2
#if defined(CONFIG_SENSORS)
		if(st->reactive_factory)
			d[0] = 0;
		else
			d[0] = 110;
#else
			d[0] = 500 / 4;
#endif
		result = inv_plat_single_write(st, REG_ACCEL_WOM_THR, d[0]);

		d[0] = BIT_ACCEL_INTEL_EN | BIT_ACCEL_INTEL_MODE_INT;
		result += inv_plat_single_write(st, REG_ACCEL_INTEL_CTRL, d[0]);
		inv_set_bank(st, BANK_SEL_0);	// select reg bank 0

		if(result)
		{
			INVLOG(ERR, "Fail to write to REG_ACCEL_INTEL_CTRL\n");
			return result;
		}
	}
	else
	{
		inv_set_bank(st, BANK_SEL_0);	// select reg bank 0
		result = inv_plat_read(st, REG_INT_ENABLE, 1, d);
		INVLOG(IL4, "REG_INT_ENABLE 0x%x\n", d[0]);

		d[0] &= ~BIT_WOM_INT_EN;
		result += inv_plat_single_write(st, REG_INT_ENABLE, d[0]);

		inv_set_bank(st, BANK_SEL_2);	// select reg bank 2
		d[0] = BIT_ACCEL_INTEL_MODE_INT;
		result += inv_plat_single_write(st, REG_ACCEL_INTEL_CTRL, d[0]);

		inv_set_bank(st, BANK_SEL_0);	// select reg bank 0
		if(result)
		{
			INVLOG(ERR, "Fail to write to REG_INT_ENABLE\n");
			return result;
		}
	}

	st->wom_enable = enable;
	INVLOG(FUNC_INT_ENTRY, "Exit\n");

	return INV_SUCCESS;
}

