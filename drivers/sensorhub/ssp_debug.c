/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h> 
#include <linux/io.h>
#include <linux/slab.h>

#include "ssp_debug.h"
#include "ssp_type_define.h"
#include "ssp_platform.h"
#include "ssp_comm.h"
#include "ssp_dump.h"
#include "ssp_data.h"

//#define DISABLE_NOEVENT_RESET
/* define */
#define SSP_DEBUG_TIMER_SEC     (5 * HZ)
#define LIMIT_TIMEOUT_CNT       1
#define LIMIT_COMFAIL_CNT		3

int reset_mcu(struct ssp_data *data)
{
	int ret;
	ssp_infof();
	ret = sensorhub_reset(data);
	return ret;
}

void recovery_mcu(struct ssp_data *data, int reason)
{
	ssp_infof("- cnt_timeout(%u), cnt_com_fail(%u), pending(%u)",
	          data->cnt_timeout, data->cnt_com_fail, !list_empty(&data->pending_list));
	data->is_reset_from_kernel = true;
	data->is_reset_started = true;
	//save_ram_dump(data, reason);
	sensorhub_reset(data);
}

/*
check_sensor_event
 - return true : there is no accel or light sensor event
                over 5sec when sensor is registered
*/
static bool check_no_event(struct ssp_data *data)
{
	u64 timestamp = get_current_timestamp();
	int check_sensors[] = {SENSOR_TYPE_ACCELEROMETER, SENSOR_TYPE_GEOMAGNETIC_FIELD, 
		SENSOR_TYPE_GYROSCOPE, SENSOR_TYPE_PRESSURE, SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED, 
		SENSOR_TYPE_GAME_ROTATION_VECTOR, SENSOR_TYPE_GYROSCOPE_UNCALIBRATED};
#ifdef DISABLE_NOEVENT_RESET
	int len = 0;
#else 
	int len = sizeof(check_sensors) / sizeof(check_sensors[0]);
#endif
	int i, sensor;
	bool res = false;

	for (i = 0 ; i < len ; i++) {
		sensor = check_sensors[i];
		/* The sensor is registered
		   And none batching mode
		   And there is no sensor event over 5sec */
		if ((atomic64_read(&data->sensor_en_state) & (1ULL << sensor))
		    && data->delay[sensor].max_report_latency == 0
		    && data->latest_timestamp[sensor] + 5000000000ULL < timestamp) {

			ssp_infof("sensor(%d) last = %lld, cur = %lld", sensor, data->latest_timestamp[sensor], timestamp);
			res = true;
		}
	}

	if (res == true) {
		data->cnt_no_event_reset++;
	}

	return res;
}

static void print_sensordata(struct ssp_data *data, unsigned int sensor_type)
{
	switch (sensor_type) {
	case SENSOR_TYPE_ACCELEROMETER:
	case SENSOR_TYPE_GYROSCOPE:
	case SENSOR_TYPE_INTERRUPT_GYRO:
		ssp_info("%u : %d, %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].x, data->buf[sensor_type].y,
		         data->buf[sensor_type].z,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_GEOMAGNETIC_FIELD:
		ssp_info("%u : %d, %d, %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].cal_x, data->buf[sensor_type].cal_y,
		         data->buf[sensor_type].cal_z, data->buf[sensor_type].accuracy,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
	case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
		ssp_info("%u : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].uncal_x,
		         data->buf[sensor_type].uncal_y,
		         data->buf[sensor_type].uncal_z,
		         data->buf[sensor_type].offset_x,
		         data->buf[sensor_type].offset_y,
		         data->buf[sensor_type].offset_z,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_PRESSURE:
		ssp_info("%u : %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].pressure,
		         data->buf[sensor_type].temperature,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_LIGHT:
	case SENSOR_TYPE_LIGHT_CCT:
		ssp_info("%u : %u, %u, %u, %u, %u, %u (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].r, data->buf[sensor_type].g,
		         data->buf[sensor_type].b, data->buf[sensor_type].w,
		         data->buf[sensor_type].a_time, data->buf[sensor_type].a_gain,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_PROXIMITY:
		ssp_info("%u : %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].prox, data->buf[sensor_type].prox_ex,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_STEP_DETECTOR:
		ssp_info("%u : %u (%lld)  (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].step_det,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_GAME_ROTATION_VECTOR:
	case SENSOR_TYPE_ROTATION_VECTOR:
		ssp_info("%u : %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].quat_a, data->buf[sensor_type].quat_b,
		         data->buf[sensor_type].quat_c, data->buf[sensor_type].quat_d,
		         data->buf[sensor_type].acc_rot,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_SIGNIFICANT_MOTION:
		ssp_info("%u : %u (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].sig_motion,
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	case SENSOR_TYPE_STEP_COUNTER:
		ssp_info("%u : %u (%lld) (%ums, %dms)", sensor_type,
		         data->buf[sensor_type].step_diff,
		         data->latest_timestamp[sensor_type],
		         data->delay[sensor_type].sampling_period,
		         data->delay[sensor_type].max_report_latency);
		break;
	default:
		ssp_info("Wrong sensorCnt: %u", sensor_type);
		break;
	}
}



static void debug_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);
	unsigned int type;

	ssp_infof("Sensor state: 0x%llx, En: 0x%llx Reset cnt: %u, Comm fail: %u, Time out: %u No event : %u",
	          data->sensor_probe_state, data->sensor_en_state, data->cnt_reset, data->cnt_com_fail,
	          data->cnt_timeout, data->cnt_no_event_reset);

	for (type = 0; type < SENSOR_TYPE_MAX; type++)
		if (atomic64_read(&data->sensor_en_state) & (1ULL << type)) {
			print_sensordata(data, type);
		}
		
	/*if (data->cnt_timeout > LIMIT_TIMEOUT_CNT) {
		data->reset_type = RESET_KERNEL_TIME_OUT;
		recovery_mcu(data, RESET_KERNEL_TIME_OUT);
	} else if (data->cnt_com_fail > LIMIT_COMFAIL_CNT) {
		data->reset_type = RESET_KERNEL_COM_FAIL;
		recovery_mcu(data, RESET_KERNEL_COM_FAIL);
	} else*/ if(check_no_event(data)) {
		ssp_dbgf("no event, no sensorhub reset");
		data->reset_type = RESET_KERNEL_NO_EVENT;
		recovery_mcu(data, RESET_KERNEL_NO_EVENT);
	}

}

static void debug_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
	          round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
	          round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	setup_timer(&data->debug_timer, debug_timer_func, (unsigned long)data);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq) {
		return -ENOMEM;
	}

	INIT_WORK(&data->work_debug, debug_work_func);
	return 0;
}

int print_mcu_debug(char *dataframe, int *index, int dataframe_length)
{
	u16 length = 0;
	int cur = *index;

	memcpy(&length, dataframe + *index, 1);
	*index += 1;

	if (length > dataframe_length - *index || length <= 0) {
		ssp_infof("[M] invalid debug length(%u/%d/%d)",
		          length, dataframe_length, cur);
		return length ? length : -1;
	}

	ssp_info("[M] %s", &dataframe[*index]);
	*index += length;
	return 0;
}

void print_dataframe(struct ssp_data *data, char *dataframe, int frame_len)
{
	char *raw_data;
	int size = 0;
	int i = 0;

	raw_data = kzalloc(frame_len * 4, GFP_KERNEL);

	for (i = 0; i < frame_len; i++) {
		size += snprintf(raw_data + size, PAGE_SIZE, "%d ",
		                 *(dataframe + i));
	}

	ssp_info("%s", raw_data);
	kfree(raw_data);
}

