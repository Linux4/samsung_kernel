/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "ssp.h"
#include <linux/math64.h>
#include <linux/sched.h>

/* SSP -> AP Instruction */
#define MSG2AP_INST_BYPASS_DATA			0x37
#define MSG2AP_INST_LIBRARY_DATA		0x01
#define MSG2AP_INST_DEBUG_DATA			0x03
#define MSG2AP_INST_BIG_DATA			0x04
#define MSG2AP_INST_META_DATA			0x05
#define MSG2AP_INST_TIME_SYNC			0x06
#define MSG2AP_INST_RESET			0x07
#define MSG2AP_INST_GYRO_CAL			0x08

#define U64_MS2NS 1000000ULL
#define U64_US2NS 1000ULL
#define U64_MS2US 1000ULL
#define MS_IDENTIFIER 1000000000U

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/

static void generate_data(struct ssp_data *data,
	struct sensor_value *sensorsdata, int iSensorData, u64 timestamp)
{
	u64 move_timestamp = data->lastTimestamp[iSensorData];
	if ((iSensorData != PROXIMITY_SENSOR) && (iSensorData != GESTURE_SENSOR)
		&& (iSensorData != STEP_DETECTOR) && (iSensorData != SIG_MOTION_SENSOR)
		&& (iSensorData != STEP_COUNTER)
#ifdef CONFIG_SENSORS_SSP_SX9306
		&& (iSensorData != GRIP_SENSOR)
#endif
		&& (iSensorData != TILT_DETECTOR)
		) {
		while ((move_timestamp * 10 + data->adDelayBuf[iSensorData] * 13) < (timestamp * 10)) {
			move_timestamp += data->adDelayBuf[iSensorData];
			sensorsdata->timestamp = move_timestamp;
//			pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,iSensorData,sensorsdata->timestamp);
			data->report_sensor_data[iSensorData](data, sensorsdata);
		}
	}
}

static void get_timestamp(struct ssp_data *data, char *pchRcvDataFrame,
		int *iDataIdx, struct sensor_value *sensorsdata,
		struct ssp_time_diff *sensortime, int iSensorData)
{
	if (sensortime->batch_mode == BATCH_MODE_RUN) {
		if(iSensorData == STEP_DETECTOR) {
			sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * 200000000ULL);
			//pr_err("[SSP] stip detector batching 200ms %d-%d\n", sensortime->batch_count_fixed, sensortime->batch_count);
		}
		else if (sensortime->batch_count == sensortime->batch_count_fixed) {
			if (sensortime->time_diff == data->adDelayBuf[iSensorData]) {
				generate_data(data, sensorsdata, iSensorData,
						(data->timestamp - data->adDelayBuf[iSensorData] * (sensortime->batch_count_fixed - 1)));
			}
			sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
		} else {
			if (sensortime->batch_count > 1)
				sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
			else
				sensorsdata->timestamp = data->timestamp;
		}
	} else {
		if (((sensortime->irq_diff * 10) > (data->adDelayBuf[iSensorData] * 13))
			&& ((sensortime->irq_diff * 10) < (data->adDelayBuf[iSensorData] * 100))) {
			if(data->bIsFirstData[iSensorData] == false)
				generate_data(data, sensorsdata, iSensorData, data->timestamp);
		}
		sensorsdata->timestamp = data->timestamp;
	}
	data->bIsFirstData[iSensorData] = false;
	//pr_err("[SSP]: %s - result timestamp %lld\n", __func__,sensorsdata->timestamp);
	*iDataIdx += 4;
}

static void get_3axis_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 6);
	*iDataIdx += 6;
}

static void get_uncalib_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}

static void get_geomagnetic_uncaldata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}

static void get_geomagnetic_rawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 6);
	*iDataIdx += 6;
}

static void get_geomagnetic_caldata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 7);
	*iDataIdx += 7;
}

static void get_rot_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 17);
	*iDataIdx += 17;
}

static void get_step_det_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_light_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 10);
	*iDataIdx += 10;
}

#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
static void get_light_ir_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}
#endif

static void get_pressure_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	s16 temperature = 0;
	memcpy(&sensorsdata->pressure[0], pchRcvDataFrame + *iDataIdx, 4);
	memcpy(&temperature, pchRcvDataFrame + *iDataIdx + 4, 2);
	sensorsdata->pressure[1] = temperature;
	*iDataIdx += 6;
}

static void get_gesture_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 20);
	*iDataIdx += 20;
}

static void get_proximity_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#if defined(CONFIG_SENSORS_SSP_TMG399x)
	memset(&sensorsdata->prox[0], 0, 1);
	memcpy(&sensorsdata->prox[0], pchRcvDataFrame + *iDataIdx, 2);
	*iDataIdx += 2;
#else	//CONFIG_SENSORS_SSP_TMD4903, CONFIG_SENSORS_SSP_TMD3782
	memset(&sensorsdata->prox[0], 0, 2);
	memcpy(&sensorsdata->prox[0], pchRcvDataFrame + *iDataIdx, 1);
	memcpy(&sensorsdata->prox[1], pchRcvDataFrame + *iDataIdx + 1, 2);
	*iDataIdx += 3;
#endif
}

static void get_proximity_rawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#if defined(CONFIG_SENSORS_SSP_TMG399x)
	memcpy(&sensorsdata->prox[0], pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
#else	//CONFIG_SENSORS_SSP_TMD4903, CONFIG_SENSORS_SSP_TMD3782
	memcpy(&sensorsdata->prox[0], pchRcvDataFrame + *iDataIdx, 2);
	*iDataIdx += 2;
#endif
}

#ifdef CONFIG_SENSORS_SSP_SX9306
static void get_grip_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 9);
		*iDataIdx += 9;
}
#endif

static void get_temp_humidity_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memset(&sensorsdata->data[2], 0, 2);
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 5);
	*iDataIdx += 5;
}

static void get_sig_motion_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_step_cnt_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(&sensorsdata->step_diff, pchRcvDataFrame + *iDataIdx, 4);
	*iDataIdx += 4;
}

static void get_shake_cam_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_tilt_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_pickup_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
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
			(sensor_type != META_SENSOR)) {
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
		case META_SENSOR:
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
		if(sensor_type == META_SENSOR)	{
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

		data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);

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
	
	sensortime.batch_mode = BATCH_MODE_RUN;
	sensortime.batch_count = sensortime.batch_count_fixed = length;
	sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
	sensortime.irq_diff = data->timestamp - data->lastTimestamp[iSensorData];

	if (data->reportedData[iSensorData] == true) {
		u64 time;
		sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[iSensorData]), (s64)length);
		//pr_info("[SSP]: sensor %d time diff %lld\n", iSensorData, sensortime.time_diff);
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
			data->get_sensor_data[iSensorData](data->batch_event.batch_data, &idx_data, &sensor_data);
			get_timestamp(data, data->batch_event.batch_data, &idx_data, &sensor_data, &sensortime, iSensorData);
			//pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,iSensorData,sensor_data.timestamp);

			if(count%80 == 0)
				usleep_range(1000,1000);
			if (sensortime.irq_diff > 2500000)
				data->report_sensor_data[iSensorData](data, &sensor_data);
			else
				pr_err("[SSP]: %s irq_diff is under 1msec (%d)\n", __func__, iSensorData);
			sensortime.batch_count--;
			count++;
			
			//pr_err("[SSP]: %d - %d, %d\n", iSensorData, count, sensortime.batch_count);
		}
		else
		{
			if(sensor_type == META_SENSOR)	{
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
			data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);
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

			if(sensor_type == META_SENSOR)	{
				sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
				report_meta_data(data, &sensor_data);
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
			data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);
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

int handle_big_data(struct ssp_data *data, char *pchRcvDataFrame, int *pDataIdx)
{
	u8 bigType = 0;
	struct ssp_big *big = kzalloc(sizeof(*big), GFP_KERNEL);
	big->data = data;
	bigType = pchRcvDataFrame[(*pDataIdx)++];
	memcpy(&big->length, pchRcvDataFrame + *pDataIdx, 4);
	*pDataIdx += 4;
	memcpy(&big->addr, pchRcvDataFrame + *pDataIdx, 4);
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

int parse_dataframe(struct ssp_data *data, char *pchRcvDataFrame, int iLength)
{
	int iDataIdx, iSensorData;
	u16 length = 0;
	struct sensor_value sensorsdata;
	struct ssp_time_diff sensortime;
	s16 caldata[3] = { 0, };
    char msg_inst = 0;
    
	sensortime.time_diff = 0;
	data->uIrqCnt++;

	for (iDataIdx = 0; iDataIdx < iLength;) {
		switch (msg_inst = pchRcvDataFrame[iDataIdx++]) {
		case MSG2AP_INST_BYPASS_DATA:
			iSensorData = pchRcvDataFrame[iDataIdx++];
			if ((iSensorData < 0) || (iSensorData >= SENSOR_MAX)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d\n", __func__,iSensorData);
				return ERROR;
			}

			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;
			sensortime.batch_count = sensortime.batch_count_fixed = length;
			sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			sensortime.irq_diff = data->timestamp - data->lastTimestamp[iSensorData];
//			pr_err("[SSP]: %s sensor %d irq diff %lld\n", __func__,	iSensorData, sensortime.irq_diff);
			if (sensortime.batch_mode == BATCH_MODE_RUN) {
				if (data->reportedData[iSensorData] == true) {
					u64 time;
					sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[iSensorData]), (s64)length);
//					pr_err("[SSP]: sensor %d time diff %lld\n", iSensorData, sensortime.time_diff);
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
				//pr_err("[SSP]: batch_run sensor %d time diff %lld lastTS %lld\n", iSensorData, sensortime.time_diff, data->lastTimestamp[iSensorData]);
			} else {
				if (data->reportedData[iSensorData] == false)
					sensortime.irq_diff = data->adDelayBuf[iSensorData];
				//pr_err("[SSP]: batch_none sensor %d irq diff %lld\n", iSensorData, sensortime.irq_diff);
			}

			do {
                if(data->get_sensor_data[iSensorData] == NULL)
                    goto error_return;
				data->get_sensor_data[iSensorData](pchRcvDataFrame, &iDataIdx, &sensorsdata);
				get_timestamp(data, pchRcvDataFrame, &iDataIdx, &sensorsdata, &sensortime, iSensorData);
				//pr_err("[SSP]: %s - sensor %d result timestamp %lld\n", __func__,iSensorData,sensorsdata.timestamp);
				
				if (sensortime.irq_diff > 2500000)
					data->report_sensor_data[iSensorData](data, &sensorsdata);
				else if ((iSensorData == PROXIMITY_SENSOR) || (iSensorData == PROXIMITY_RAW)
						|| (iSensorData == GESTURE_SENSOR) || (iSensorData == SIG_MOTION_SENSOR)
						|| (iSensorData == STEP_COUNTER)   || (iSensorData == STEP_DETECTOR)
						|| (iSensorData == TILT_DETECTOR) || (iSensorData == PICKUP_GESTURE))
					data->report_sensor_data[iSensorData](data, &sensorsdata);
				else
					pr_err("[SSP]: %s irq_diff is under 1msec (%d)\n", __func__, iSensorData);
				sensortime.batch_count--;
			} while ((sensortime.batch_count > 0) && (iDataIdx < iLength));

			if (sensortime.batch_count > 0)
				pr_err("[SSP]: %s batch count error (%d)\n", __func__, sensortime.batch_count);

			data->lastTimestamp[iSensorData] = data->timestamp;
			data->reportedData[iSensorData] = true;
			break;
		case MSG2AP_INST_DEBUG_DATA:
			iSensorData = print_mcu_debug(pchRcvDataFrame, &iDataIdx, iLength);
			if (iSensorData) {
				pr_err("[SSP]: %s - Mcu data frame3 error %d\n", __func__,
						iSensorData);
				return ERROR;
			}
			break;
		case MSG2AP_INST_LIBRARY_DATA:
			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;
			ssp_sensorhub_handle_data(data, pchRcvDataFrame, iDataIdx,
					iDataIdx + length);
			iDataIdx += length;
			break;
		case MSG2AP_INST_BIG_DATA:
			handle_big_data(data, pchRcvDataFrame, &iDataIdx);
			break;
		case MSG2AP_INST_META_DATA:
			sensorsdata.meta_data.what = pchRcvDataFrame[iDataIdx++];
			sensorsdata.meta_data.sensor = pchRcvDataFrame[iDataIdx++];
            
            if(sensorsdata.meta_data.what != 1)
                goto error_return;
            
        	if ((sensorsdata.meta_data.sensor < 0) || (sensorsdata.meta_data.sensor >= SENSOR_MAX)) {
			pr_err("[SSP]: %s - Mcu meta_data frame1 error %d\n", __func__,
					sensorsdata.meta_data.sensor);
			return ERROR;
			}
			report_meta_data(data, &sensorsdata);
			break;
		case MSG2AP_INST_TIME_SYNC:
			data->bTimeSyncing = true;
			break;
		case MSG2AP_INST_GYRO_CAL:
			pr_info("[SSP]: %s - Gyro caldata received from MCU\n",  __func__);
			memcpy(caldata, pchRcvDataFrame + iDataIdx, sizeof(caldata));
			wake_lock(&data->ssp_wake_lock);
			save_gyro_caldata(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			iDataIdx += sizeof(caldata);
			break;
		case SH_MSG2AP_GYRO_CALIBRATION_EVENT_OCCUR:
			data->gyro_lib_state = GYRO_CALIBRATION_STATE_EVENT_OCCUR;
			break;
        default :
                goto error_return;
		}
	}
	if(data->pktErrCnt >= 1) // if input error packet doesn't comes continually, do not reset
	data->pktErrCnt = 0;
	return SUCCESS;
    
error_return:
    pr_err("[SSP] %s err Inst 0x%02x\n", __func__, msg_inst);
    data->pktErrCnt++;
    if(data->pktErrCnt >= 2)
    {
    	pr_err("[SSP] %s packet is polluted\n", __func__);
    	data->mcuAbnormal = true;
    	data->pktErrCnt = 0; 
    	data->errorCount++;
    }
    return ERROR;  
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->get_sensor_data[ACCELEROMETER_SENSOR] = get_3axis_sensordata;
	data->get_sensor_data[GYROSCOPE_SENSOR] = get_3axis_sensordata;
	data->get_sensor_data[GEOMAGNETIC_UNCALIB_SENSOR] =
		get_geomagnetic_uncaldata;
	data->get_sensor_data[GEOMAGNETIC_RAW] = get_geomagnetic_rawdata;
	data->get_sensor_data[GEOMAGNETIC_SENSOR] =
		get_geomagnetic_caldata;
	data->get_sensor_data[PRESSURE_SENSOR] = get_pressure_sensordata;
	data->get_sensor_data[GESTURE_SENSOR] = get_gesture_sensordata;
	data->get_sensor_data[PROXIMITY_SENSOR] = get_proximity_sensordata;
	data->get_sensor_data[PROXIMITY_RAW] = get_proximity_rawdata;
#ifdef CONFIG_SENSORS_SSP_SX9306
	data->get_sensor_data[GRIP_SENSOR] = get_grip_sensordata;
#endif
	data->get_sensor_data[LIGHT_SENSOR] = get_light_sensordata;
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	data->get_sensor_data[LIGHT_IR_SENSOR] = get_light_ir_sensordata;
#endif
	data->get_sensor_data[TEMPERATURE_HUMIDITY_SENSOR] =
		get_temp_humidity_sensordata;
	data->get_sensor_data[ROTATION_VECTOR] = get_rot_sensordata;
	data->get_sensor_data[GAME_ROTATION_VECTOR] = get_rot_sensordata;
	data->get_sensor_data[STEP_DETECTOR] = get_step_det_sensordata;
	data->get_sensor_data[SIG_MOTION_SENSOR] = get_sig_motion_sensordata;
	data->get_sensor_data[GYRO_UNCALIB_SENSOR] = get_uncalib_sensordata;
	data->get_sensor_data[STEP_COUNTER] = get_step_cnt_sensordata;
	data->get_sensor_data[SHAKE_CAM] = get_shake_cam_sensordata;
#ifdef CONFIG_SENSORS_SSP_INTERRUPT_GYRO_SENSOR
	data->get_sensor_data[INTERRUPT_GYRO_SENSOR] = get_3axis_sensordata;
#endif
	data->get_sensor_data[TILT_DETECTOR] = get_tilt_sensordata;
	data->get_sensor_data[PICKUP_GESTURE] = get_pickup_sensordata;

	data->report_sensor_data[ACCELEROMETER_SENSOR] = report_acc_data;
	data->report_sensor_data[GYROSCOPE_SENSOR] = report_gyro_data;
	data->report_sensor_data[GEOMAGNETIC_UNCALIB_SENSOR] =
		report_mag_uncaldata;
	data->report_sensor_data[GEOMAGNETIC_RAW] = report_geomagnetic_raw_data;
	data->report_sensor_data[GEOMAGNETIC_SENSOR] =
		report_mag_data;
	data->report_sensor_data[PRESSURE_SENSOR] = report_pressure_data;
	data->report_sensor_data[GESTURE_SENSOR] = report_gesture_data;
	data->report_sensor_data[PROXIMITY_SENSOR] = report_prox_data;
	data->report_sensor_data[PROXIMITY_RAW] = report_prox_raw_data;
#ifdef CONFIG_SENSORS_SSP_SX9306
	data->report_sensor_data[GRIP_SENSOR] = report_grip_data;
#endif
	data->report_sensor_data[LIGHT_SENSOR] = report_light_data;
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	data->report_sensor_data[LIGHT_IR_SENSOR] = report_light_ir_data;
#endif
	data->report_sensor_data[TEMPERATURE_HUMIDITY_SENSOR] =
		report_temp_humidity_data;
	data->report_sensor_data[ROTATION_VECTOR] = report_rot_data;
	data->report_sensor_data[GAME_ROTATION_VECTOR] = report_game_rot_data;
	data->report_sensor_data[STEP_DETECTOR] = report_step_det_data;
	data->report_sensor_data[SIG_MOTION_SENSOR] = report_sig_motion_data;
	data->report_sensor_data[GYRO_UNCALIB_SENSOR] = report_uncalib_gyro_data;
	data->report_sensor_data[STEP_COUNTER] = report_step_cnt_data;
	data->report_sensor_data[SHAKE_CAM] = report_shake_cam_data;
#ifdef CONFIG_SENSORS_SSP_INTERRUPT_GYRO_SENSOR
	data->report_sensor_data[INTERRUPT_GYRO_SENSOR] = report_interrupt_gyro_data;
#endif
	data->report_sensor_data[TILT_DETECTOR] = report_tilt_data;
	data->report_sensor_data[PICKUP_GESTURE] = report_pickup_data;

	data->ssp_big_task[BIG_TYPE_DUMP] = ssp_dump_task;
	data->ssp_big_task[BIG_TYPE_READ_LIB] = ssp_read_big_library_task;
	/** HiFi Sensor with Long Time batch **/
	data->ssp_big_task[BIG_TYPE_READ_HIFI_BATCH] = ssp_batch_data_read_task;
	/** HiFi Sensor with Long Time batch **/
	data->ssp_big_task[BIG_TYPE_VOICE_NET] = ssp_send_big_library_task;
	data->ssp_big_task[BIG_TYPE_VOICE_GRAM] = ssp_send_big_library_task;
	data->ssp_big_task[BIG_TYPE_VOICE_PCM] = ssp_pcm_dump_task;
	data->ssp_big_task[BIG_TYPE_TEMP] = ssp_temp_task;
}
