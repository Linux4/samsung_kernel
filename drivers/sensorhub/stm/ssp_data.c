/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "ssp_data.h"

#define U64_MS2NS 1000000ULL
#define U64_US2NS 1000ULL
#define U64_MS2US 1000ULL
#define MS_IDENTIFIER 1000000000U

#define DELTA_AVERAGING 0
#define HIFI_NOMAL_LOG 0

unsigned int batch_counter = 0;

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/
#if DELTA_AVERAGING // DELTA AVERAGING
static u64 get_ts_average(struct ssp_data *data, int sensor_type, u64 ts_delta)
{
	u8 cnt = data->ts_avg_buffer_cnt[sensor_type];
	u8 idx = data->ts_avg_buffer_idx[sensor_type];
	u64 avg = 0ULL;

	idx = (idx + 1) % SIZE_MOVING_AVG_BUFFER;

	// Remove data from idx and insert new one.
	if (cnt == SIZE_MOVING_AVG_BUFFER) {
		data->ts_avg_buffer_sum[sensor_type] -= data->ts_avg_buffer[sensor_type][idx];
	} else {
		cnt++;
	}

	// Insert Data to idx.
	data->ts_avg_buffer[sensor_type][idx] = ts_delta;
	data->ts_avg_buffer_sum[sensor_type] += data->ts_avg_buffer[sensor_type][idx];
	avg = data->ts_avg_buffer_sum[sensor_type] / cnt;

	data->ts_avg_buffer_cnt[sensor_type] = cnt;
	data->ts_avg_buffer_idx[sensor_type] = idx;
	
#if HIFI_NOMAL_LOG
	ssp_dbg("[SSP_AVG] [%3d] %lld   IN  %lld   [IDX %5u  CNT %5u]\n", 
		sensor_type, avg, ts_delta, idx, cnt);
#endif	

	return avg;
}
#endif


static void get_timestamp(struct ssp_data *data, char *dataframe,
		int *index, struct sensor_value *sensorsdata,
		u16 batch_mode, int sensor)
{
	u64 mcuTimeNs = 0;
	
	// MCU send 8byte timestamp(ns) for time sync
	memset(&mcuTimeNs, 0, 8);
	memcpy(&mcuTimeNs, dataframe + *index, 8);
	
	data->lastTimestamp[sensor] = data->report_mode[sensor] == REPORT_MODE_ON_CHANGE && batch_mode != BATCH_MODE_RUN ? data->timestamp : mcuTimeNs;
	
	sensorsdata->timestamp = data->lastTimestamp[sensor];
	data->bIsFirstData[sensor] = false;

	*index += 8;
}

void get_sensordata(struct ssp_data *data, char *dataframe,
		int *index, int sensor, struct sensor_value *sensordata)
{
	memcpy(sensordata, dataframe + *index, data->data_len[sensor]);
	*index += data->data_len[sensor];
}

bool isBatchSensor(u8 sensor_type)
{
	return ((sensor_type == ACCELEROMETER_SENSOR) || (sensor_type == GEOMAGNETIC_UNCALIB_SENSOR) ||
				(sensor_type == PRESSURE_SENSOR) || (sensor_type == GAME_ROTATION_VECTOR) || (sensor_type == BATCH_META_SENSOR));
}

bool ssp_check_buffer(struct ssp_data *data)
{
	int idx_data = 0;
	u8 sensor_type = 0;
	bool res = true;
	u64 ts = 0;

	ts = get_current_timestamp();
	pr_err("[SSP] start cehck %lld\n", ts);

	do{
		sensor_type = data->batch_event.batch_data[idx_data++];

		if (!isBatchSensor(sensor_type)) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
					sensor_type, idx_data - 1);
			res = false;
			break;
		}

		switch(sensor_type)
		{
		case ACCELEROMETER_SENSOR:
			idx_data += 10;
			break;
		case GEOMAGNETIC_UNCALIB_SENSOR:
			idx_data += 16;
			break;
		case PRESSURE_SENSOR:
			idx_data += 10;
			break;
		case GAME_ROTATION_VECTOR:
			idx_data += 21;
			break;
		case BATCH_META_SENSOR:
			idx_data += 1;
			break;
		}
		
		// for timestamp of sensor data
		if (sensor_type != BATCH_META_SENSOR)
			idx_data += 4;

		if(idx_data > data->batch_event.batch_length){
			//stop index over max length
			pr_info("[SSP_CHK] invalid data1\n");
			res = false;
			break;
		}

		// run until max length
		if(idx_data == data->batch_event.batch_length){
			//pr_info("[SSP_CHK] valid data\n");
			break;
		}
		else if(idx_data + 1 == data->batch_event.batch_length){
			//stop if only sensor type exist
			pr_info("[SSP_CHK] invalid data2\n");
			res = false;
			break;
		}
	}while(true);
	ts = get_current_timestamp();
	pr_err("[SSP] finish cehck %lld\n", ts);

	return res;
}

void ssp_batch_resume_check(struct ssp_data *data)
{
	if(data->bIsResumed)
	{
		u8 sensor_type = 0;
		int idx_data = 0;
		int batch_index[SENSOR_MAX] = {0, };
		u64 current_timestamp = data->timestamp;
		u64 resume_timestamp = 0;
		int mode = 0; // mode, 0: check batch index of each batch type sensor
							//mode, 1: replcae timestamp of each batch type sensor
		//pr_info("[SSP_BAT] LENGTH = %d, start index = %d ts %lld resume %lld\n", data->batch_event.batch_length, idx_data, timestamp, data->resumeTimestamp);
resume_check_loop:		
		idx_data = 0;
		while (idx_data < data->batch_event.batch_length)
		{
			sensor_type = data->batch_event.batch_data[idx_data++];
			if(sensor_type == BATCH_META_SENSOR){
				idx_data++;
				continue;
			}
			
			if (!isBatchSensor(sensor_type)){
				pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
						sensor_type, idx_data - 1);
				goto resume_check_end;
			}
			
			idx_data += data->data_len[sensor_type];			
			switch(mode){
				case 0: 
					batch_index[sensor_type]++;
					break;
				case 1:
					batch_index[sensor_type]--;
					resume_timestamp = current_timestamp - ((u64)(data->adDelayBuf[sensor_type])*batch_index[sensor_type]);
					memcpy(data->batch_event.batch_data + idx_data, &resume_timestamp, 8);
					//pr_info("[SSP_BAT] sensortype = %d, index = %d ts %lld delay %lld\n", sensor_type, batch_index[sensor_type], resume_timestamp, (u64)(data->adDelayBuf[sensor_type]));
					break;
			}
			idx_data += 8;
		}
				
		if(mode == 0){
			mode = 1;
			goto resume_check_loop;
		}		
	}
	
resume_check_end:
	data->bIsResumed = false;
	data->resumeTimestamp = 0ULL;
}

void ssp_batch_report(struct ssp_data *data)
{
	u8 sensor_type = 0;
	struct sensor_value sensor_data;
	int idx_data = 0;
	int count = 0;
	//u64 timestamp = get_current_timestamp();

	//pr_info("[SSP_BAT] LENGTH = %d, start index = %d ts %lld\n", data->batch_event.batch_length, idx_data, timestamp);

	while (idx_data < data->batch_event.batch_length)
	{
		//ssp_dbg("[SSP_BAT] bcnt %d\n", count);
		sensor_type = data->batch_event.batch_data[idx_data++];

		if(sensor_type == BATCH_META_SENSOR)	{
			//pr_info("[SSP_BAT] get metadata %d\n", idx_data);
			sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
			report_meta_data(data, META_SENSOR, &sensor_data);
			count++;
			continue;
		}

		if (!isBatchSensor(sensor_type)) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d data %d\n", __func__,
					sensor_type, idx_data - 1, batch_counter);
			return ;
		}

		if(count%80 == 0)
			usleep_range(1000,1000);
		get_sensordata(data, data->batch_event.batch_data, &idx_data,sensor_type, &sensor_data);

		data->skipEventReport = false;
		//pr_info("[SSP_BAT] get ts %d\n", idx_data);
		get_timestamp(data, data->batch_event.batch_data, &idx_data, &sensor_data, BATCH_MODE_RUN, sensor_type);
		if (data->skipEventReport == false) {
			report_sensordata(data, sensor_type, &sensor_data);
		}

		data->reportedData[sensor_type] = true;
		count++;
	}
	ssp_dbg("[SSP_BAT] max cnt %d\n", count);
}

//#define BATCH_LOG_FILE_NAME		"/data/akmd_log.txt"
void ssp_log_save(char *buff, int buf_length)
{
	struct file *log_file;
	mm_segment_t old_fs;
	int ret = 0;
	char log_path[BIN_PATH_SIZE+1];
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	
	sprintf(log_path, "/data/log/batchlog%d.log", batch_counter);
	//sprintf(log_path, "/efs/batchlog%d.log", batch_counter);
	
	ssp_dbg("[SSP] log file path %s\n",log_path);
	log_file = filp_open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if (IS_ERR(log_file)) {
		ssp_dbg("[SSP] open batch log file err %ld\n", IS_ERR(log_file));
		set_fs(old_fs);
		batch_counter++;
		return;
	}

	//Open setting file for write. "/data/batch_log%d.txt"
	ssp_dbg("[SSP] start write %s\n",log_path);
	ret = vfs_write(log_file, (char __user *) buff, buf_length, &log_file->f_pos);
	ssp_dbg("[SSP] write finish");
	if (ret < 0) {
		ssp_dbg("[SSP] write pcm dump to file err(%d)\n", ret);
	}
	ssp_dbg("[SSP] file close");
	filp_close(log_file, current->files);
	ssp_dbg("[SSP] return old_fs");
	set_fs(old_fs);
	batch_counter++;
}

// Control batched data with long term
// Ref ssp_read_big_library_task
void ssp_batch_data_read_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_data *data = big->data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;
	int received_buffer = 0;
	u64 ts = 0;

	mutex_lock(&data->batch_events_lock);
	wake_lock(&data->ssp_wake_lock);

	residue = big->length;
	data->batch_event.batch_length = big->length;
	data->batch_event.batch_data = vmalloc(big->length);
	if (data->batch_event.batch_data == NULL)
	{
		ssp_dbg("[SSP_BAT] batch data alloc fail \n");
		kfree(big);
		wake_unlock(&data->ssp_wake_lock);
		mutex_unlock(&data->batch_events_lock);
		return;
	}

	//ssp_dbg("[SSP_BAT] IN : LENGTH = %d \n", big->length);

	while (residue > 0) {
		buf_len = residue > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg),GFP_ATOMIC);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = buf_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = data->batch_event.batch_data + pos;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(big->data, msg, 1000);
		if (ret != SUCCESS) {
			pr_err("[SSP_BAT] read batch data err(%d) ignor recive %d\n", ret, received_buffer);
			//ssp_log_save(data->batch_event.batch_data, received_buffer);
			vfree(data->batch_event.batch_data);
			data->batch_event.batch_data = NULL;
			data->batch_event.batch_length = 0;
			kfree(big);
			wake_unlock(&data->ssp_wake_lock);
			mutex_unlock(&data->batch_events_lock);
			return;
		}
		received_buffer += buf_len;

		pos += buf_len;
		residue -= buf_len;

		pr_info("[SSP_BAT] read batch data (%5d / %5d)\n", pos, big->length);
	}
	
	// TODO: Do not parse, jut put in to FIFO, and wake_up thread.

	// READ DATA FROM MCU COMPLETED 
	//Wake up check
	if(ssp_check_buffer(data))
	{
		ssp_batch_resume_check(data);

		// PARSE DATA FRAMES, Should run loop
		ts = get_current_timestamp();
		pr_info("[SSP] report start %lld\n", ts);
		ssp_batch_report(data);
		ts = get_current_timestamp();
		pr_info("[SSP] report finish %lld\n", ts);
	}

	//ssp_log_save(data->batch_event.batch_data, data->batch_event.batch_length);
	
	vfree(data->batch_event.batch_data);
	data->batch_event.batch_data = NULL;
	data->batch_event.batch_length = 0;
	kfree(big);
	wake_unlock(&data->ssp_wake_lock);
	mutex_unlock(&data->batch_events_lock);
	//pr_info("[SSP_BAT] task finished\n");
}

int handle_big_data(struct ssp_data *data, char *dataframe, int *pDataIdx)
{
	u8 bigType = 0;
	struct ssp_big *big = kzalloc(sizeof(*big), GFP_KERNEL);
	big->data = data;
	bigType = dataframe[(*pDataIdx)++];
	memcpy(&big->length, dataframe + *pDataIdx, 4);
	*pDataIdx += 4;
	memcpy(&big->addr, dataframe + *pDataIdx, 4);
	*pDataIdx += 4;

	if (bigType >= BIG_TYPE_MAX) {
		kfree(big);
		return FAIL;
	}

	INIT_WORK(&big->work, data->ssp_big_task[bigType]);
	
	if(bigType != BIG_TYPE_READ_HIFI_BATCH)
		queue_work(data->debug_wq, &big->work);
	else
		queue_work(data->batch_wq, &big->work);

	return SUCCESS;
}

void refresh_task(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
			struct ssp_data, work_refresh);

	if (data->bSspShutdown == true) {
		ssp_errf("ssp already shutdown");
		return;
	}

	wake_lock(&data->ssp_wake_lock);
	ssp_errf();
	data->uResetCnt++;

	if (initialize_mcu(data) > 0) {
		sync_sensor_state(data);
		ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_RESET);
		if (data->uLastAPState != 0)
			ssp_send_cmd(data, data->uLastAPState, 0);
		if (data->uLastResumeState != 0)
			ssp_send_cmd(data, data->uLastResumeState, 0);
		data->uTimeOutCnt = 0;
	} else
		data->uSensorState = 0;

	wake_unlock(&data->ssp_wake_lock);
}

int queue_refresh_task(struct ssp_data *data, int delay)
{
	cancel_delayed_work_sync(&data->work_refresh);

	INIT_DELAYED_WORK(&data->work_refresh, refresh_task);
	queue_delayed_work(data->debug_wq, &data->work_refresh,
			msecs_to_jiffies(delay));
	return SUCCESS;
}

int parse_dataframe(struct ssp_data *data, char *dataframe, int frame_len)
{
	struct sensor_value sensorsdata;
	u16 batch_event_count;
	u16 batch_mode;

	int sensor, index;
	u16 length = 0;
	s16 caldata[3] = {0, };

	if (data->bSspShutdown) {
		ssp_infof("ssp shutdown, do not parse");
		return SUCCESS;
	}

	if (data->debug_enable)
		print_dataframe(data, dataframe, frame_len);

	memset(&sensorsdata, 0, sizeof(sensorsdata));

	for (index = 0; index < frame_len;) {
		switch (dataframe[index++]) {
		case MSG2AP_INST_BYPASS_DATA:
			sensor = dataframe[index++];
			if ((sensor < 0) || (sensor >= SENSOR_MAX)) {
				ssp_errf("Mcu bypass dataframe err %d", sensor);
				return ERROR;
			}

			memcpy(&length, dataframe + index, 2);
			index += 2;
			batch_event_count = length;
			batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			if(batch_mode == BATCH_MODE_NONE)
			{
				data->LastSensorTimeforReset[sensor] = get_current_timestamp();
			}
			
			//pr_err("[SSP]: %s - %d - %lld\n", __func__,sensor, data->timestamp);

			// TODO: When batch_event_count = 0, we should not run.
			do {
				get_sensordata(data, dataframe, &index,	sensor, &sensorsdata);
				// TODO: Integrate get_sensor_data function.
				// TODO: get_sensor_data(pchRcvDataFrame, &iDataIdx, &sensorsdata, data->sensor_data_size[sensor_type]);
				// TODO: Divide control data batch and non batch.

				if (data->cameraGyroSyncMode) {
					data->skipEventReport = false;
					get_timestamp(data, dataframe, &index, &sensorsdata, batch_mode, sensor);

					if (data->skipEventReport == false) {
						report_sensordata(data, sensor, &sensorsdata);
					}
				} else {
					get_timestamp(data, dataframe, &index, &sensorsdata, batch_mode, sensor);
					report_sensordata(data, sensor, &sensorsdata);
				}

				batch_event_count--;
			} while ((batch_event_count > 0) && (index < frame_len));

			if (batch_event_count > 0)
				ssp_errf("batch count error (%d)", batch_event_count);

			data->reportedData[sensor] = true;
			break;
		case MSG2AP_INST_DEBUG_DATA:
			sensor = print_mcu_debug(dataframe, &index, frame_len);
			if (sensor) {
				ssp_errf("Mcu debug dataframe err %d", sensor);
				return ERROR;
			}
			break;
		case MSG2AP_INST_LIBRARY_DATA:
			memcpy(&length, dataframe + index, 2);
			index += 2;
			ssp_sensorhub_handle_data(data, dataframe, index,
					index + length);
			index += length;
			break;
		case MSG2AP_INST_BIG_DATA:
			handle_big_data(data, dataframe, &index);
			break;
		case MSG2AP_INST_META_DATA:
			sensorsdata.meta_data.what = dataframe[index++];
			sensorsdata.meta_data.sensor = dataframe[index++];
			report_meta_data(data, META_SENSOR, &sensorsdata);
			break;
		case MSG2AP_INST_TIME_SYNC:
			data->bTimeSyncing = true;
			break;
		case MSG2AP_INST_RESET:
			ssp_infof("Reset MSG received from MCU");
			queue_refresh_task(data, 0);
			break;
		case MSG2AP_INST_GYRO_CAL:
			ssp_infof("Gyro caldata received from MCU");
			memcpy(caldata, dataframe + index, sizeof(caldata));
			wake_lock(&data->ssp_wake_lock);
			save_gyro_caldata(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			index += sizeof(caldata);
			break;
		case MSG2AP_INST_DUMP_DATA:
			debug_crash_dump(data, dataframe, frame_len);
			return SUCCESS;
			break;
		case SH_MSG2AP_GYRO_CALIBRATION_EVENT_OCCUR:
			data->gyro_lib_state = GYRO_CALIBRATION_STATE_EVENT_OCCUR;
			break;
		}
	}

	return SUCCESS;
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->ssp_big_task[BIG_TYPE_DUMP] = ssp_dump_task;
	data->ssp_big_task[BIG_TYPE_READ_LIB] = ssp_read_big_library_task;
	/** HiFi Sensor with Long Time batch **/
	data->ssp_big_task[BIG_TYPE_READ_HIFI_BATCH] = ssp_batch_data_read_task;
	/** HiFi Sensor with Long Time batch **/
}
