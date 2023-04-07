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

#define DELTA_AVERAGING 0
#define HIFI_NOMAL_LOG 0

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/
#if DELTA_AVERAGING // DELTA AVERAGING
static u64 get_ts_average(struct ssp_data *data, int sensor_type, u64 ts_delta)
{
	u8 cnt = data->ts_avg_buffer_cnt[sensor_type];
	u8 idx = data->ts_avg_buffer_idx[sensor_type];
	u64 avg = 0ULL;
	
	// Get idx for insert data.
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

static void get_timestamp(struct ssp_data *data, char *pchRcvDataFrame,
		int *iDataIdx, struct sensor_value *sensorsdata,
		u16 batch_mode, int sensor_type)

{
	unsigned int time_delta_us = 0;
	u64 time_delta_ns = 0;
	u64 offset = 0;

	memset(&time_delta_us, 0, 4);
	memcpy(&time_delta_us, pchRcvDataFrame + *iDataIdx, 4);
	//ssp_dbg("[SSP_IDX] [%3d] TS %d]\n", sensor_type, time_delta_us);

	if (time_delta_us > MS_IDENTIFIER) {
		//We condsider, unit is ms (MS->NS)
		time_delta_ns = ((u64) (time_delta_us % MS_IDENTIFIER)) * U64_MS2NS;
	} else {
		time_delta_ns = (((u64) time_delta_us) * U64_US2NS);//US->NS
	}

	// TODO: Reverse calculation of timestamp when non wake up batching.
	if (batch_mode == BATCH_MODE_RUN) {
		// BATCHING MODE 
#if HIFI_NOMAL_LOG
		ssp_dbg("[SSP_IDX] BATCH [%3d] TS %lld %lld\n", 
			sensor_type, data->lastTimestamp[sensor_type],
			time_delta_ns);
#endif


#if 0	// TODO: Yes, we know Hub has a bug with deltaTimeNs is set to 0
		data->lastDeltaTimeNs[sensor_type] = time_delta_ns;
#endif
		data->lastTimestamp[sensor_type] += time_delta_ns;
		//pr_err("[SSP_BAT] report %lld+delta%lld\n", data->lastTimestamp[sensor_type],time_delta_ns);

	} else {
		// NORMAL MODE

		// CAMERA SYNC MODE
		if (data->cameraGyroSyncMode && sensor_type == GYROSCOPE_SENSOR) {
			offset = get_current_timestamp() - get_kernel_timestamp();
			if (time_delta_ns == 1000ULL || data->lastTimestamp[sensor_type] == 0ULL) {
				//eltaTimeNs = 0ULL;
				data->lastTimestamp[sensor_type] = data->timestamp + offset;
				time_delta_ns = 0ULL;
			} else {
				if (data->timestamp + offset < data->lastTimestamp[sensor_type]) {
					time_delta_ns = 0ULL;
				} else {
					time_delta_ns = data->timestamp + offset - data->lastTimestamp[sensor_type];
				}
			}

			if (time_delta_ns == 0ULL) {
				// Don't report when time is 0.
				data->skipEventReport = true;
			} 
			else if (time_delta_ns > (data->adDelayBuf[sensor_type] * 18ULL / 10ULL)) {
				int cnt = 0;
				int i = 0;
				cnt = time_delta_ns / (data->adDelayBuf[sensor_type]);

				for (i = 0; i < cnt; i++) {
					data->lastTimestamp[sensor_type] += data->adDelayBuf[sensor_type];
					sensorsdata->timestamp = data->lastTimestamp[sensor_type];
					data->report_sensor_data[sensor_type](data, sensorsdata);
					time_delta_ns -= data->adDelayBuf[sensor_type];
				}

				// mod is calculated automatically.
				if (time_delta_ns > (data->adDelayBuf[sensor_type] / 2ULL)) {
					data->lastTimestamp[sensor_type] += time_delta_ns;
					sensorsdata->timestamp = data->lastTimestamp[sensor_type];
					data->report_sensor_data[sensor_type](data, sensorsdata);

					data->skipEventReport = true;
				}
				time_delta_ns = 0ULL;
			}
			else if (time_delta_ns < (data->adDelayBuf[sensor_type] / 2ULL)) {
				data->skipEventReport = true;
				time_delta_ns = 0ULL;
			}
			data->lastTimestamp[sensor_type] += time_delta_ns;

		} else {
			// 80ms is magic number. reset time base.
			offset = get_current_timestamp() - get_kernel_timestamp();
			if (time_delta_ns == 0ULL || time_delta_ns == 1000ULL || time_delta_ns == 80000ULL) {
				//pr_err("[SSP_TIM] %d delta %lld\n", sensor_type, time_delta_ns);
				data->lastTimestamp[sensor_type] = data->timestamp - 15000000ULL + offset;
				time_delta_ns = 0ULL;
			}

			if (data->sensor_report_mode[sensor_type] == REPORT_MODE_ON_CHANGE) {
				data->lastTimestamp[sensor_type] = data->timestamp + offset;
			} else {
				if (data->timestamp + offset > (1000000000ULL + data->lastTimestamp[sensor_type])) {
					//pr_err("[SSP_INT] 1sec delay ls %lld irq %lld offset %lld\n", data->lastTimestamp[sensor_type], data->timestamp, offset);
					data->lastTimestamp[sensor_type] = data->timestamp + offset;
				}
				else
					data->lastTimestamp[sensor_type] += time_delta_ns;

#if 0 // future event issue
				if(data->lastTimestamp[sensor_type] > get_current_timestamp())
					pr_err("[SSP_BY] future time\n");
				if(data->lastTimestamp[sensor_type] > get_current_timestamp() + 300000000ULL){
					data->lastTimestamp[sensor_type] = data->timestamp + offset;
					pr_err("[SSP_INT] last time sync\n");
				}
#endif
			}
			//pr_err("[SSP_BY] [%d]report %lld - d %lld c %lld\n", sensor_type, data->lastTimestamp[sensor_type],time_delta_ns, get_current_timestamp());
		}
	}
	sensorsdata->timestamp = data->lastTimestamp[sensor_type];
/*
	if(data->timestamp>500000000+sensorsdata->timestamp)
		pr_err("[SSP_IDX] ----- [%3d] TS %lld DT %lld[AP  %5u]-%lld\n", 
				sensor_type,sensorsdata->timestamp, time_delta_ns,data->ts_stacked_cnt, data->timestamp-sensorsdata->timestamp);
	else
		pr_info("[SSP_IDX] ----- [%3d] TS %lld DT %lld[AP  %5u]-%lld\n", 
				sensor_type, sensorsdata->timestamp, time_delta_ns,data->ts_stacked_cnt, data->timestamp-sensorsdata->timestamp);
*/
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

/*
static void get_sensor_data(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata, int data_size)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, data_size);
	*iDataIdx += data_size;
}
*/
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

void ssp_batch_resume_check(struct ssp_data *data)
{
	u64 acc_offset = 0, uncal_mag_offset = 0, press_offset = 0, grv_offset = 0;
	u64 lastOffset = 0;
	int resume_cnt = 0;
	//if suspend -> wakeup case. calc. FIFO last timestamp
	if(data->bIsResumed)
	{
		u8 sensor_type = 0;
		struct sensor_value sensor_data;
		unsigned int delta_time_us = 0;
		int idx_data = 0;
		u64 timestamp = get_current_timestamp();
		//ssp_dbg("[SSP_BAT] LENGTH = %d, start index = %d ts %lld resume %lld\n", data->batch_event.batch_length, idx_data, timestamp, data->resumeTimestamp);
		//pr_err("[SSP_BAT] %lld - %lld = %lld\n", timestamp, data->timestamp, timestamp - data->timestamp);
		timestamp = data->timestamp;
		
		while(true) {
			lastOffset = get_current_timestamp() - get_kernel_timestamp();
			
			if(resume_cnt == 3) {
				//pr_err("[SSP_BAT] offset updated %lld, last %lld\n",data->lastOffset, lastOffset);
				data->lastOffset = lastOffset;
				break;
			}
			else if(data->lastOffset == 0) {
				usleep_range(10000,10000);
				//pr_err("[SSP_BAT] data->lastOffset %lld\n", lastOffset);
				data->lastOffset = get_current_timestamp() - get_kernel_timestamp();
				//pr_err("[SSP_BAT] data->lastOffset %lld\n", data->lastOffset);
				break;
			}
			else if((data->lastOffset > lastOffset) && (data->lastOffset < 5000000ULL + lastOffset)) {
				//pr_err("[SSP_BAT] offset same sleep %lld\n", lastOffset);
				usleep_range(10000,10000);
			}
			else if((lastOffset > data->lastOffset) && (lastOffset < 5000000ULL + data->lastOffset)) {
				//pr_err("[SSP_BAT] offset same sleep %lld\n", lastOffset);
				usleep_range(10000,10000);
			}
			else {
				//pr_err("[SSP_BAT] offset updated %lld, last %lld\n",data->lastOffset, lastOffset);
				data->lastOffset = lastOffset;
				break;
			}
			resume_cnt++;
		}

		timestamp += data->lastOffset;

		while (idx_data < data->batch_event.batch_length)
		{
			sensor_type = data->batch_event.batch_data[idx_data++];
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
				data->bIsResumed = false;
				data->resumeTimestamp = 0ULL;
				return ;
			}

			data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);

			memset(&delta_time_us, 0, 4);
			memcpy(&delta_time_us, data->batch_event.batch_data + idx_data, 4);
			if (delta_time_us > MS_IDENTIFIER) {
				//We condsider, unit is ms (MS->NS)
				delta_time_us = ((u64) (delta_time_us % MS_IDENTIFIER)) * U64_MS2NS;
			} else {
				delta_time_us = (((u64) delta_time_us) * U64_US2NS);//US->NS
			}

			switch(sensor_type)
			{
				case ACCELEROMETER_SENSOR:
					acc_offset += delta_time_us;
					break;
				case GEOMAGNETIC_UNCALIB_SENSOR:
					uncal_mag_offset += delta_time_us;
					break;
				case GAME_ROTATION_VECTOR:
					grv_offset += delta_time_us;
					break;
				case PRESSURE_SENSOR:
					press_offset += delta_time_us;
					break;
				default:
					break;				
			}
			//pr_err("[SSP_BAT] offset %lld, delta %d\n", acc_offset, delta_time_us);
			idx_data += 4;
		}
	
		//pr_err("[SSP] current %lld - kernel %lld = offset %lld\n", get_current_timestamp(), get_kernel_timestamp(), get_current_timestamp() - get_kernel_timestamp());
		if(acc_offset > 0)
			data->lastTimestamp[ACCELEROMETER_SENSOR] = timestamp - acc_offset;
		if(uncal_mag_offset > 0)
			data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] = timestamp - uncal_mag_offset;
		if(press_offset > 0)
			data->lastTimestamp[PRESSURE_SENSOR] = timestamp - press_offset;
		if(grv_offset > 0)
			data->lastTimestamp[GAME_ROTATION_VECTOR] = timestamp - grv_offset;

		//pr_info("[SSP_BAT] resume calc. acc %lld. uncalmag %lld. pressure %lld. GRV %lld\n", acc_offset, uncal_mag_offset, press_offset, grv_offset);
		//pr_info("[SSP_BAT] resume calc. acc %lld. uncalmag %lld. pressure %lld. GRV %lld\n", data->lastTimestamp[ACCELEROMETER_SENSOR], data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR]
		//, data->lastTimestamp[PRESSURE_SENSOR], data->lastTimestamp[GAME_ROTATION_VECTOR]);
	}
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

	//ssp_dbg("[SSP_BAT] LENGTH = %d, start index = %d ts %lld\n", data->batch_event.batch_length, idx_data, timestamp);

	while (idx_data < data->batch_event.batch_length)
	{
		//ssp_dbg("[SSP_BAT] bcnt %d\n", count);
		sensor_type = data->batch_event.batch_data[idx_data++];

		if(sensor_type == META_SENSOR)	{
			sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
			report_meta_data(data, &sensor_data);
			count++;
			continue;
		}

		if ((sensor_type != ACCELEROMETER_SENSOR) &&
			(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
			(sensor_type != PRESSURE_SENSOR) &&
			(sensor_type != GAME_ROTATION_VECTOR)) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
					sensor_type, idx_data - 1);
			return ;
		}

		if(count%80 == 0)
			usleep_range(1000,1000);
		data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);

		data->skipEventReport = false;
		get_timestamp(data, data->batch_event.batch_data, &idx_data, &sensor_data, BATCH_MODE_RUN, sensor_type);
		if (data->skipEventReport == false) {
			data->report_sensor_data[sensor_type](data, &sensor_data);
		}

		data->reportedData[sensor_type] = true;
		count++;
	}
	ssp_dbg("[SSP_BAT] max cnt %d\n", count);
/*
	if(data->batchLatencyBuf[ACCELEROMETER_SENSOR] != 0)
	{
		if(data->timestamp > 3000000000ULL + data->lastTimestamp[ACCELEROMETER_SENSOR])
			data->lastTimestamp[ACCELEROMETER_SENSOR] = data->timestamp;
		else if(data->lastTimestamp[ACCELEROMETER_SENSOR] > 3000000000ULL + data->timestamp)
			data->lastTimestamp[ACCELEROMETER_SENSOR] = data->timestamp;
	}
	if(data->batchLatencyBuf[GEOMAGNETIC_UNCALIB_SENSOR] != 0)
	{
		if(data->timestamp > 3000000000ULL + data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR])
			data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] = data->timestamp;
		else if(data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] > 3000000000ULL + data->timestamp)
			data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] = data->timestamp;
	}
	if(data->batchLatencyBuf[PRESSURE_SENSOR] != 0)
	{
		if(data->timestamp > 3000000000ULL + data->lastTimestamp[PRESSURE_SENSOR])
			data->lastTimestamp[PRESSURE_SENSOR] = data->timestamp;
		else if(data->lastTimestamp[PRESSURE_SENSOR] > 3000000000ULL + data->timestamp)
			data->lastTimestamp[PRESSURE_SENSOR] = data->timestamp;
	}
	if(data->batchLatencyBuf[GAME_ROTATION_VECTOR] != 0)
	{
		if(data->timestamp > 3000000000ULL + data->lastTimestamp[GAME_ROTATION_VECTOR])
			data->lastTimestamp[GAME_ROTATION_VECTOR] = data->timestamp;
		else if(data->lastTimestamp[GAME_ROTATION_VECTOR] > 3000000000ULL + data->timestamp)
			data->lastTimestamp[GAME_ROTATION_VECTOR] = data->timestamp;
	}
*/
}


// Control batched data with long term
// Ref ssp_read_big_library_task
void ssp_batch_data_read_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_data *data = big->data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;
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
	else
		pr_err("[SSP_BAT] invalid buffer recieved\n");

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
	int iDataIdx;
	int sensor_type;
	u16 length = 0;

	struct sensor_value sensorsdata;
	char msg_inst = 0;
	u16 batch_event_count;
	u16 batch_mode;

	s16 caldata[3] = { 0, };

	data->uIrqCnt++;

	for (iDataIdx = 0; iDataIdx < iLength;) {
		switch (msg_inst = pchRcvDataFrame[iDataIdx++]) {
		case MSG2AP_INST_BYPASS_DATA:
			sensor_type = pchRcvDataFrame[iDataIdx++];
			if ((sensor_type < 0) || (sensor_type >= SENSOR_MAX)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d\n", __func__,
						sensor_type);
				goto error_return;
			}

			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;

			batch_event_count = length;
			batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;

			// TODO: When batch_event_count = 0, we should not run.
			do {
                if(data->get_sensor_data[sensor_type] == NULL)
                    goto error_return;
				data->get_sensor_data[sensor_type](pchRcvDataFrame, &iDataIdx, &sensorsdata);
				// TODO: Integrate get_sensor_data function.
				// TODO: get_sensor_data(pchRcvDataFrame, &iDataIdx, &sensorsdata, data->sensor_data_size[sensor_type]);
				// TODO: Divide control data batch and non batch.

				data->skipEventReport = false;
				get_timestamp(data, pchRcvDataFrame, &iDataIdx, &sensorsdata, batch_mode, sensor_type);
				if (data->skipEventReport == false) {
					data->report_sensor_data[sensor_type](data, &sensorsdata);
				}

				batch_event_count--;
			} while ((batch_event_count > 0) && (iDataIdx < iLength));

			if (batch_event_count > 0)
				pr_err("[SSP]: %s batch count error (%d)\n", __func__, batch_event_count);
			data->reportedData[sensor_type] = true;
			//pr_err("[SSP]: (%d / %d)\n", iDataIdx, iLength);
			break;
		case MSG2AP_INST_DEBUG_DATA:
			sensor_type = print_mcu_debug(pchRcvDataFrame, &iDataIdx, iLength);
			if (sensor_type) {
				pr_err("[SSP]: %s - Mcu data frame3 error %d\n", __func__,
						sensor_type);
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
				goto error_return;
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
	data->get_sensor_data[MOTOR_TEST] = get_3axis_sensordata;

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
	data->report_sensor_data[MOTOR_TEST] = report_motor_test_data;

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
