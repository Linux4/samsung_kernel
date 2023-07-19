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

/* DMP defines */
#define FIRMWARE_CRC           0xf0c3c94d

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_STEPCTR);

	return r;
}
int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_TIMECTR);

	return r;
}

int inv_read_pedometer_counter(struct inv_mpu_state *st)
{
	int result;
	u32 last_step_counter, curr_counter;
	u64 counter;

	result = read_be32_from_mem(st, &last_step_counter, STPDET_TIMESTAMP);
	if (result)
		return result;
	if (0 != last_step_counter) {
		result = read_be32_from_mem(st, &curr_counter, DMPRATE_CNTR);
		if (result)
			return result;
		counter = inv_get_cntr_diff(curr_counter, last_step_counter);
		st->ped.last_step_time = get_time_ns() - counter *
						st->eng_info[ENGINE_ACCEL].dur;
	}

	return 0;
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

	return 0;
}

int inv_check_sensor_rate(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = 0;
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
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

	return 0;
}

/*****************************************************************************/
/*      SysFS for Pedo logging mode features                                 */
s64 inv_get_pedlog_timestamp(struct inv_mpu_state *st, bool start)
{
	s64 timestamp = 0;
	static s64 start_time = 0;
	
	if(start) {
		timestamp = get_time_ns();
		start_time = timestamp;
		pr_info("[INVN:%s] HKSONG start_time %lld start %lld\n", __func__, st->pedlog.start_time, start_time);
		st->pedlog.interrupt_counter = 0;

	} else {
		timestamp = st->pedlog.interrupt_counter *
			st->pedlog.interrupt_duration;
		timestamp *= 60000000000LL;
		pr_info("%s dd: timestamp = %lld, interrupt_counter=%d\n",
			__func__, timestamp,
			st->pedlog.interrupt_counter);

		timestamp += start_time + inv_get_pedlog_elapsed_time(st);
		pr_info("%s dd: timestamp = %lld, interrupt_counter=%d\n",
			__func__, timestamp,
			st->pedlog.interrupt_counter);
		pr_info("[INVN:%s] HKSONG start_time %lld start %lld\n", __func__, st->pedlog.start_time, start_time);
	}

	pr_info("%s : timestamp = %lld, st->pedlog.interrupt_counter=%d\n",
		__func__, timestamp,
		st->pedlog.interrupt_counter);

	return timestamp;
}

int inv_enable_pedlog(struct inv_mpu_state *st, bool en, bool irq_en)
{
	inv_error_t result;
	u8 reg[2] = {0x0,S_HEALTH_EN};
	u8 mec_reg[2];
	int mec_data;

	INVLOG(FUNC_INT_ENTRY, "Enter\n");
	result = mem_r(MOTION_EVENT_CTL, 2, mec_reg);
	INVLOG(IL4, "0x%x 0x%x\n", mec_reg[0], mec_reg[1]);

	if (en) {
		mec_reg[0] |= reg[0];
		mec_reg[1] |= reg[1];
		st->pedlog.start_time = get_time_timeofday();
		pr_info("[INVN:%s] HKSONG start_time %lld\n", __func__, st->pedlog.start_time);
		st->pedlog.interrupt_time = -1;
		st->pedlog.stop_time = -1;
		st->pedlog.valid_count = 0;
		if(st->pedlog.interrupt_duration == 0)
			st->pedlog.interrupt_duration = 20; //default 20 minute;

		pr_info("[INVN:%s] Enable start:%lld, Interrupt Duration:%d\n",
			__func__, st->pedlog.start_time,
			st->pedlog.interrupt_duration);
		
		/* reset pedlog interrupt period */
		inv_set_pedlog_interrupt_period(st,
				st->pedlog.interrupt_duration);
		
		/* set 1minute timer value */
		st->pedlog.tick_count = 3360;
		inv_set_pedlog_update_timer(st, st->pedlog.tick_count);
		
		inv_clear_pedlog_cadence(st);
	}
	else {
		mec_reg[0] &= ~reg[0];
		mec_reg[1] &= ~reg[1];

		if (result)
			return result;

		if(st->pedlog.start_time > 0) {
			st->pedlog.stop_time = get_time_timeofday();
		
			inv_get_pedlog_cadence(st, 0, 19 , st->pedlog.cadence);
			inv_get_pedlog_valid_count(st);
			pr_info("[INVN:%s] Disable start:%lld valid_count %d\n",
				__func__, st->pedlog.start_time,
				st->pedlog.valid_count);
			
			if(irq_en) {
				st->pedlog.interrupt_mask |= PEDLOG_INT_CADENCE;
				complete(&st->pedlog.wait);
			}
		}
	}

	INVLOG(IL4, "Write to MOTION_EVENT_CTL : 0x%x 0x%x\n", mec_reg[0], mec_reg[1]);
	//result += mem_w(MOTION_EVENT_CTL, 2, mec_reg);
	mec_data = (mec_reg[0] << 8) | mec_reg[1];
	inv_write_2bytes(st, MOTION_EVENT_CTL, mec_data);
	result += mem_r(MOTION_EVENT_CTL, 2, reg);
	INVLOG(IL4, "Read MOTION_EVENT_CTL : 0x%x 0x%x\n", reg[0], reg[1]);

	if (result) {
		INVLOG(ERR, "Fail to enable\n");
		return result;
	}

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
			//end_time = inv_get_pedlog_timestamp(st, false);
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

int inv_set_pedlog_interrupt_period(struct inv_mpu_state *st, u16 period)
{
	u16 s_health_period;
	inv_error_t result;

	INVLOG(IL4, "Enter\n");
	s_health_period = period - 1; // DMP period is offset by 1
	result = write_be16_to_mem(st, s_health_period, PEDLOG_INT_PERIOD);

	if (result) {
		INVLOG(ERR, "Fail to write PEDLOG_INT_PERIOD\n");
		return result;
	}

	result = write_be16_to_mem(st, s_health_period, PEDLOG_INT_PERIOD2);

	if (result) {
		INVLOG(ERR, "Fail to write PEDLOG_INT_PERIOD2\n");
		return result;
	} else {
		return INV_SUCCESS;
	}

}

s64 inv_get_pedlog_elapsed_time(struct inv_mpu_state *st)
{
	int result = 0;
	u16 period_min, period2_min;
	u16 period_sec, period2_sec;
	u64 elapsed = 0;

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

int inv_get_pedlog_cadence(struct inv_mpu_state *st, u8 start, u8 end, s32 cadence[]) 
{
	int i, result = 0;
	u16 cad;
	u8 len;

	len = end - start + 1;
	
	if(len < 0 || len >  PEDLOG_CADENCE_LEN + 1)
		return -EINVAL;

	for(i = start; i <= end; i ++) {
		result = read_be16_from_mem(st, &cad, cadence_arr[i]);
		if(result)
			goto read_fail;

		cadence[i - start] = cad & 0xFFFF;

		pr_info("[INVN:%s] r/w cadence r/w %u %u \n",
			__func__, cad>>8, cad&0xFF);
	}

	return 0;

read_fail:
	pr_info("%s error\n", __func__);
	return result;
}

int inv_reset_pedlog_update_timer(struct inv_mpu_state *st)
{
	int result = 0;
	u16 d = 0;

	INVLOG(IL5, "Enter\n");
	/*reset cadence update timer*/
	result = read_be16_from_mem(st, &d, PEDLOG_MIN_CONST);
	if(result)
		goto read_fail;

	result = write_be16_to_mem(st, d, PEDLOG_MIN_CNTR);
	if(result)
		goto read_fail;

	INVLOG(IL4,"Timer = %d\n", d);

	return result;

read_fail:
	INVLOG(ERR, "Error\n");
	return result;
}

int inv_set_pedlog_update_timer(struct inv_mpu_state *st, u16 timer)
{
	int result = 0;
	u16 data = 0;

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
	return result;

read_fail:
	pr_err("%s error\n", __func__);
	return result;
}

int inv_get_pedlog_update_timer(struct inv_mpu_state *st, char *buf)
{
	int result = 0;
	u16 data = 0;

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

