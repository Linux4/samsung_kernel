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

//HIFI batching
#define U64_MS2NS 1000000ULL
#define U64_US2NS 1000ULL
#define U64_MS2US 1000ULL
#define MS_IDENTIFIER 1000000000U

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/
static void generate_data(struct ssp_data *data, struct sensor_value *sensorsdata,
						int sensor, u64 timestamp)
{
	u64 move_timestamp = data->lastTimestamp[sensor];
	if ((sensor != PROXIMITY_SENSOR) && (sensor != GESTURE_SENSOR)
		&& (sensor != STEP_DETECTOR) && (sensor != SIG_MOTION_SENSOR)
		&& (sensor != STEP_COUNTER)) {
		while ((move_timestamp * 10 + data->adDelayBuf[sensor] * 13) < (timestamp * 10)) {
			move_timestamp += data->adDelayBuf[sensor];
			sensorsdata->timestamp = move_timestamp;
			//pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,sensor,sensorsdata->timestamp);
			report_sensordata(data, sensor, sensorsdata);
		}
	}
}

static void get_timestamp(struct ssp_data *data, char *dataframe,
		int *index, struct sensor_value *sensorsdata,
		struct ssp_time_diff *sensortime, int sensor)
{
	//ssp_info("is first data %d", data->bIsFirstData[sensor]);

	if (sensortime->batch_mode == BATCH_MODE_RUN) {
		if (sensortime->batch_count == sensortime->batch_count_fixed) {
			if (sensortime->time_diff == data->adDelayBuf[sensor]) {
				//ssp_info("sensor time diff %lldd delay %lld", sensortime->time_diff, data->adDelayBuf[sensor]);
				generate_data(data, sensorsdata, sensor,
						(data->timestamp - data->adDelayBuf[sensor] * (sensortime->batch_count_fixed - 1)));
			}
			sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
		} else {
			if (sensortime->batch_count > 1)
				sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
			else
				sensorsdata->timestamp = data->timestamp;
		}
	} else {
		if (((sensortime->irq_diff * 10) > (data->adDelayBuf[sensor] * 13))
			&& ((sensortime->irq_diff * 10) < (data->adDelayBuf[sensor] * 100))) {
			if(data->bIsFirstData[sensor] == false)
				generate_data(data, sensorsdata, sensor, data->timestamp);
		}

		if(sensor == SIG_MOTION_SENSOR)
		{
			int resume_cnt = 0;
			u64 lastOffset = 0;
			while(true) {
				if(get_current_timestamp() - get_kernel_timestamp() < 0)
					lastOffset = 1;
				else
					lastOffset = get_current_timestamp() - get_kernel_timestamp();
				
				if(resume_cnt == 10) {
					pr_err("[SSP_BAT] offset updated %lld, last %lld\n",data->lastOffset, lastOffset);
					data->lastOffset = lastOffset;
					data->timestamp = get_current_timestamp();
					break;
				} else if(data->lastOffset == 0) {
					usleep_range(10000,10000);
					pr_err("[SSP_BAT] data->lastOffset %lld\n", lastOffset);

					if (get_current_timestamp() - get_kernel_timestamp() < 0)
						lastOffset = 0;
					else
						lastOffset = get_current_timestamp() - get_kernel_timestamp();
					
					data->lastOffset = lastOffset;
					pr_err("[SSP_BAT] data->lastOffset %lld\n", data->lastOffset);
					data->timestamp = get_current_timestamp();
					break;
				} else if((data->lastOffset > lastOffset) && (data->lastOffset < 3000000000ULL + lastOffset)) { // lastoffset larger than current but 5ms under
					//pr_err("[SSP_BAT] offset same sleep %lld, %lld\n", lastOffset, data->lastOffset - lastOffset);
					usleep_range(10000,10000);
				} else if((lastOffset > data->lastOffset) && (lastOffset < 3000000000ULL + data->lastOffset)) { // current offset larger than last but 5ms under
					//pr_err("[SSP_BAT] offset same sleep %lld, %lld\n", lastOffset, lastOffset - data->lastOffset);
					usleep_range(10000,10000);
				} else {
					pr_err("[SSP_BAT] offset updated %lld, last %lld\n",data->lastOffset, lastOffset);
					data->lastOffset = lastOffset;
					data->timestamp = get_current_timestamp();
					break;
				}
				resume_cnt++;
			}
		}
		sensorsdata->timestamp = data->timestamp;
	}
	//ssp_info("get_timestamp sensor %d %lld isFirst %d",sensor,sensorsdata->timestamp, data->bIsFirstData[sensor]);
	data->bIsFirstData[sensor] = false;
	*index += 4;
}

void get_sensordata(struct ssp_data *data, char *dataframe,
		int *index, int sensor, struct sensor_value *sensordata)
{
	memcpy(sensordata, dataframe + *index, data->data_len[sensor]);
	*index += data->data_len[sensor];
}


bool ssp_check_buffer(struct ssp_data *data)
{
	int idx_data = 0;
	u8 sensor_type = 0;
	bool res = true;

	do{
		sensor_type = data->batch_event.batch_data[idx_data++];

		if ((sensor_type != ACCELEROMETER_SENSOR) &&
			(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
			(sensor_type != PRESSURE_SENSOR) &&
			(sensor_type != GAME_ROTATION_VECTOR) &&
			(sensor_type != BATCH_META_SENSOR)) {
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

	return res;
}

// Control batched data with long term
// Ref ssp_read_big_library_task
void ssp_batch_timestamp_check(struct ssp_data *data, struct ssp_batch_lengths *batch_length)
{
	u64 acc_offset = 0, uncal_mag_offset = 0, press_offset = 0, grv_offset = 0;
	//if suspend -> wakeup case. calc. FIFO last timestamp

	u8 sensor_type = 0;
	unsigned int delta_time_us = 0;
	int idx_data = 0;
	u64 timestamp = get_current_timestamp();
	//ssp_dbg("[SSP_BAT] LENGTH = %d, start index = %d ts %lld resume %lld\n", data->batch_event.batch_length, idx_data, timestamp, data->resumeTimestamp);

	while (idx_data < data->batch_event.batch_length)
	{
		struct sensor_value sensor_data;
		
		sensor_type = data->batch_event.batch_data[idx_data++];
		if(sensor_type == BATCH_META_SENSOR)	{
			sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
			batch_length->metadata_length++;
			continue;
		}

		if ((sensor_type != ACCELEROMETER_SENSOR) &&
			(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
			(sensor_type != PRESSURE_SENSOR) &&
			(sensor_type != GAME_ROTATION_VECTOR)) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
					sensor_type, idx_data - 1);
			break;
		}

		get_sensordata(data, data->batch_event.batch_data, &idx_data, sensor_type, &sensor_data);

		memset(&delta_time_us, 0, 4);
		memcpy(&delta_time_us, data->batch_event.batch_data + idx_data, 4);
		delta_time_us = (((u64) delta_time_us) * U64_MS2NS);//MS->NS

		switch(sensor_type)
		{
			case ACCELEROMETER_SENSOR:
				acc_offset += delta_time_us;
				batch_length->acc_length++;
				break;
			case GEOMAGNETIC_UNCALIB_SENSOR:
				uncal_mag_offset += delta_time_us;
				batch_length->uncal_mag_length++;
				break;
			case GAME_ROTATION_VECTOR:
				grv_offset += delta_time_us;
				batch_length->grv_length++;
				break;
			case PRESSURE_SENSOR:
				press_offset += delta_time_us;
				batch_length->press_length++;
				break;
			default:
				break;				
		}
		idx_data += 4;
	}

	if(data->bIsResumed){
		timestamp = data->resumeTimestamp;
			
		if(acc_offset > 0)
			data->lastTimestamp[ACCELEROMETER_SENSOR] = timestamp - acc_offset;
		if(uncal_mag_offset > 0)
			data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] = timestamp - uncal_mag_offset;
		if(press_offset > 0)
			data->lastTimestamp[PRESSURE_SENSOR] = timestamp - press_offset;
		if(grv_offset > 0)
			data->lastTimestamp[GAME_ROTATION_VECTOR] = timestamp - grv_offset;
	}

	data->bIsResumed = false;
	data->resumeTimestamp = 0ULL;
	//ssp_dbg("[SSP_BAT] resume calc. acc %lld. uncalmag %lld. pressure %lld. GRV %lld\n", acc_offset, uncal_mag_offset, press_offset, grv_offset);
	//pr_info("[SSP_BAT] event cnt acc %d. uncalmag %d. pressure %d. GRV %d\n", 
		         //batch_length->acc_length, batch_length->uncal_mag_length, batch_length->press_length, batch_length->grv_length);
}

void ssp_batch_report_for_one(struct ssp_data *data, int iSensorData, int length)
{
	struct ssp_time_diff sensortime;
	int idx_data = 0;
	u8 sensor_type = 0;
	int count = 0;
	
	//sensortime.batch_mode = BATCH_MODE_RUN;
	sensortime.batch_count = sensortime.batch_count_fixed = length;
	sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
	sensortime.irq_diff = data->timestamp - data->lastTimestamp[iSensorData];

	if (data->reportedData[iSensorData] == true) {
		u64 time;
		sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[iSensorData]), (s64)length);
		//pr_err("[SSP]: sensor %d time diff %lld\n", iSensorData, sensortime.time_diff);
		if (length > 8)
			time = data->adDelayBuf[iSensorData] * 18;
		else if (length > 4)
			time = data->adDelayBuf[iSensorData] * 25;
		else if (length > 2)
			time = data->adDelayBuf[iSensorData] * 50;
		else
			time = data->adDelayBuf[iSensorData] * 120;
		if ((sensortime.time_diff * 10) > time) {
			data->lastTimestamp[iSensorData] = data->timestamp - (data->adDelayBuf[iSensorData] * length);
			sensortime.time_diff = data->adDelayBuf[iSensorData];
		} else {
			time = data->adDelayBuf[iSensorData] * 11;
			if ((sensortime.time_diff * 10) > time)
				sensortime.time_diff = data->adDelayBuf[iSensorData];
		}
	} else {
		if (data->lastTimestamp[iSensorData] < (data->timestamp - (data->adDelayBuf[iSensorData] * length))) {
			data->lastTimestamp[iSensorData] = data->timestamp - (data->adDelayBuf[iSensorData] * length);
			sensortime.time_diff = data->adDelayBuf[iSensorData];
		} else
			sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[iSensorData]), (s64)length);
	}
	//pr_info("[SSP]: batch_run sensor %d time diff %lld lastTS %lld\n", iSensorData, sensortime.time_diff, data->lastTimestamp[iSensorData]);
	//pr_info("[SSP_BAT] LENGTH = %d, start index = %d ts %lld cnt %d\n", data->batch_event.batch_length, idx_data, data->timestamp, count);
	while (idx_data < data->batch_event.batch_length)
	{
		struct sensor_value sensor_data;
		sensor_type = data->batch_event.batch_data[idx_data++];

		if(iSensorData == sensor_type) // same sensor -> report, not -> pass
		{
			get_sensordata(data, data->batch_event.batch_data, &idx_data, sensor_type, &sensor_data);
			get_timestamp(data, data->batch_event.batch_data, &idx_data, &sensor_data, &sensortime, iSensorData);
			//pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,iSensorData,sensor_data.timestamp);

			if(count%80 == 0)
				usleep_range(1000,1000);
			if (sensortime.irq_diff > 2500000)
				report_sensordata(data, iSensorData, &sensor_data);
			else
				pr_err("[SSP]: %s irq_diff is under 1msec (%d)\n", __func__, iSensorData);
			sensortime.batch_count--;
			count++;
			
			//pr_err("[SSP]: %d - %d, %d\n", iSensorData, count, sensortime.batch_count);
		}
		else
		{
			if(sensor_type == BATCH_META_SENSOR)	{
				sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
				continue;
			}
			if ((sensor_type != ACCELEROMETER_SENSOR) &&
				(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
				(sensor_type != PRESSURE_SENSOR) &&
				(sensor_type != GAME_ROTATION_VECTOR)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
						sensor_type, idx_data - 1);
				return;
			}
			get_sensordata(data, data->batch_event.batch_data, &idx_data, sensor_type, &sensor_data);
			idx_data += 4;
		}
	}
	pr_info("[SSP]: %d - total %d\n", iSensorData, count);

	data->lastTimestamp[iSensorData] = data->timestamp;
	data->reportedData[iSensorData] = true;

}

void ssp_batch_report(struct ssp_data *data, struct ssp_batch_lengths *batch_length)
{
/*
	pr_info("[SSP]: sensor count acc %d unmag %d press %d grv %d\n", 
		batch_length->acc_length, batch_length->uncal_mag_length , 
		batch_length->press_length, batch_length->grv_length);
*/
	if(batch_length->acc_length > 0)
		ssp_batch_report_for_one(data, ACCELEROMETER_SENSOR, batch_length->acc_length);
	if(batch_length->uncal_mag_length > 0)
		ssp_batch_report_for_one(data, GEOMAGNETIC_UNCALIB_SENSOR, batch_length->uncal_mag_length);
	if(batch_length->grv_length > 0)
		ssp_batch_report_for_one(data, GAME_ROTATION_VECTOR, batch_length->grv_length);
	if(batch_length->press_length > 0)
		ssp_batch_report_for_one(data, PRESSURE_SENSOR, batch_length->press_length);
	
	if(batch_length->metadata_length > 0)
	{
		int idx_data = 0;
		u8 sensor_type = 0;
		while (idx_data < data->batch_event.batch_length)
		{
			struct sensor_value sensor_data;
			sensor_type = data->batch_event.batch_data[idx_data++];

			if(sensor_type == BATCH_META_SENSOR)	{
				sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
				report_meta_data(data, META_SENSOR, &sensor_data);
				continue;
			}
			if ((sensor_type != ACCELEROMETER_SENSOR) &&
				(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
				(sensor_type != PRESSURE_SENSOR) &&
				(sensor_type != GAME_ROTATION_VECTOR)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
						sensor_type, idx_data - 1);
				return;
			}
			get_sensordata(data, data->batch_event.batch_data, &idx_data, sensor_type, &sensor_data);
			idx_data += 4;
		}
	}
}


// Control batched data with long term
// Ref ssp_read_big_library_task
void ssp_batch_data_read_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_data *data = big->data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;
	struct ssp_batch_lengths batch_length = {0,0,0,0,0};

	mutex_lock(&data->batch_events_lock);
	wake_lock(&data->ssp_wake_lock);

	residue = big->length;
	data->batch_event.batch_length = big->length;
	data->batch_event.batch_data = vmalloc(big->length);
	if (data->batch_event.batch_data == NULL)
	{
		pr_err("[SSP_BAT] batch data alloc fail \n");
		kfree(big);
		wake_unlock(&data->ssp_wake_lock);
		mutex_unlock(&data->batch_events_lock);
		return;
	}

	ssp_dbg("[SSP_BAT] IN : LENGTH = %d \n", big->length);

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
			pr_err("[SSP_BAT] read batch data err(%d) ignor\n", ret);
			vfree(data->batch_event.batch_data);
			data->batch_event.batch_data = NULL;
			data->batch_event.batch_length = 0;
			kfree(big);
			wake_unlock(&data->ssp_wake_lock);
			mutex_unlock(&data->batch_events_lock);
			return;
		}

		pos += buf_len;
		residue -= buf_len;
		pr_info("[SSP_BAT] read batch data (%5d / %5d)\n", pos, big->length);
	}

	if(ssp_check_buffer(data))
	{
		// TODO: Do not parse, jut put in to FIFO, and wake_up thread.
		// READ DATA FROM MCU COMPLETED 
		//Wake up check
		ssp_batch_timestamp_check(data, &batch_length);
		
		// PARSE DATA FRAMES, Should run loop
		ssp_batch_report(data, &batch_length);
	}

	vfree(data->batch_event.batch_data);
	data->batch_event.batch_data = NULL;
	data->batch_event.batch_length = 0;
	kfree(big);
	wake_unlock(&data->ssp_wake_lock);
	mutex_unlock(&data->batch_events_lock);
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
	struct ssp_time_diff sensortime;
	int sensor, index;
	u16 length = 0;
	s16 caldata[3] = { 0, };

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
			sensortime.batch_count = sensortime.batch_count_fixed = length;
			sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			sensortime.irq_diff = data->timestamp - data->lastTimestamp[sensor];
			//pr_err("[SSP]: %s sensor %d irq diff %lld\n", __func__, sensor, sensortime.irq_diff);

			if (sensortime.batch_mode == BATCH_MODE_RUN) {
				if (data->reportedData[sensor] == true) {
					u64 time;
					sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor]), (s64)length);
					//pr_err("[SSP]: sensor %d time diff %lld\n", sensor, sensortime.time_diff);
					if (length > 8)
						time = data->adDelayBuf[sensor] * 18;
					else if (length > 4)
						time = data->adDelayBuf[sensor] * 25;
					else if (length > 2)
						time = data->adDelayBuf[sensor] * 50;
					else
						time = data->adDelayBuf[sensor] * 100;
					if ((sensortime.time_diff * 10) > time) {
						data->lastTimestamp[sensor] = data->timestamp - (data->adDelayBuf[sensor] * length);
						sensortime.time_diff = data->adDelayBuf[sensor];
					} else {
						time = data->adDelayBuf[sensor] * 11;
						if ((sensortime.time_diff * 10) > time)
							sensortime.time_diff = data->adDelayBuf[sensor];
					}
				} else {
					if (data->lastTimestamp[sensor] < (data->timestamp - (data->adDelayBuf[sensor] * length))) {
						data->lastTimestamp[sensor] = data->timestamp - (data->adDelayBuf[sensor] * length);
						sensortime.time_diff = data->adDelayBuf[sensor];
					} else
						sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor]), (s64)length);
				}
				//pr_err("[SSP]: batch_run sensor %d time diff %lld lastTS %lld\n", sensor, sensortime.time_diff, data->lastTimestamp[sensor]);
			} else {
				if (data->reportedData[sensor] == false)
					sensortime.irq_diff = data->adDelayBuf[sensor];
				//pr_err("[SSP]: batch_none sensor %d irq diff %lld\n", sensor, sensortime.irq_diff);
			}

			do {
				get_sensordata(data, dataframe, &index,
					sensor, &sensorsdata);
				get_timestamp(data, dataframe, &index, &sensorsdata, &sensortime, sensor);
				//pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,sensor,sensorsdata.timestamp);
				if (sensortime.irq_diff > 1000000)
					report_sensordata(data, sensor, &sensorsdata);
				else if ((sensor == PROXIMITY_SENSOR) || (sensor == PROXIMITY_RAW)
						|| (sensor == STEP_COUNTER)   || (sensor == STEP_DETECTOR)
						|| (sensor == GESTURE_SENSOR) || (sensor == SIG_MOTION_SENSOR)
						|| (sensor == TILT_DETECTOR) || (sensor == PICKUP_GESTURE))
					report_sensordata(data, sensor, &sensorsdata);
				else
					ssp_errf("irq_diff is under 1msec (%d)", sensor);
				sensortime.batch_count--;
			} while ((sensortime.batch_count > 0) && (index < frame_len));

			if (sensortime.batch_count > 0)
				ssp_errf("batch count error (%d)", sensortime.batch_count);

			data->lastTimestamp[sensor] = data->timestamp;
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
