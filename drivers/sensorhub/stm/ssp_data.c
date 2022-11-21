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

static void get_timestamp(struct ssp_data *data, char *dataframe,
		int *index, struct sensor_value *sensorsdata,
		struct ssp_time_diff *sensortime, int sensor)
{
	unsigned int deltaTimeUs = 0;
	u64 deltaTimeNs = 0;

	memset(&deltaTimeUs, 0, 4);
	memcpy(&deltaTimeUs, dataframe + *index, 4);

	if (deltaTimeUs > MS_IDENTIFIER) {
		//We condsider, unit is ms (MS->NS)
		deltaTimeNs = ((u64) (deltaTimeUs % MS_IDENTIFIER)) * U64_MS2NS;
	} else {
		deltaTimeNs = (((u64) deltaTimeUs) * U64_US2NS);//US->NS
	}

	if (sensortime->batch_mode == BATCH_MODE_RUN) {
		// BATCHING MODE
		data->lastTimestamp[sensor] += deltaTimeNs;
	} else {
		// NORMAL MODE

		// CAMERA SYNC MODE
		if (data->cameraGyroSyncMode && sensor == GYROSCOPE_SENSOR) {
			if (deltaTimeNs == 1000ULL || data->lastTimestamp[sensor] == 0ULL) {
				data->lastTimestamp[sensor] = data->timestamp;
				deltaTimeNs = 0ULL;
			} else {
				if (data->timestamp < data->lastTimestamp[sensor]) {
					deltaTimeNs = 0ULL;
				} else {
					deltaTimeNs = data->timestamp - data->lastTimestamp[sensor];
				}
			}

			if (deltaTimeNs == 0ULL) {
				// Don't report when time is 0.
				data->skipEventReport = true;
			} else if (deltaTimeNs > (data->adDelayBuf[sensor] * 18ULL / 10ULL)) {
				int cnt = 0;
				int i = 0;
				cnt = deltaTimeNs / (data->adDelayBuf[sensor]);

				for (i = 0; i < cnt; i++) {
					data->lastTimestamp[sensor] += data->adDelayBuf[sensor];
					sensorsdata->timestamp = data->lastTimestamp[sensor];
					report_sensordata(data, sensor, sensorsdata);
					deltaTimeNs -= data->adDelayBuf[sensor];
				}

				// mod is calculated automatically.
				if (deltaTimeNs > (data->adDelayBuf[sensor] / 2ULL)) {
					data->lastTimestamp[sensor] += deltaTimeNs;
					sensorsdata->timestamp = data->lastTimestamp[sensor];
					report_sensordata(data, sensor, sensorsdata);
					data->skipEventReport = true;
				}
				deltaTimeNs = 0ULL;
			}
			else if (deltaTimeNs < (data->adDelayBuf[sensor] / 2ULL)) {
				data->skipEventReport = true;
				deltaTimeNs = 0ULL;
			}
			data->lastTimestamp[sensor] += deltaTimeNs;

		} else {
			// 80ms is magic number. reset time base.
			if (deltaTimeNs == 1ULL || deltaTimeNs == 80000ULL) {
				data->lastTimestamp[sensor] = data->timestamp - 15000000ULL;
				deltaTimeNs = 1000ULL;
			}

			if (data->report_mode[sensor] == REPORT_MODE_ON_CHANGE) {
				data->lastTimestamp[sensor] = data->timestamp;
			} else {
				data->lastTimestamp[sensor] += deltaTimeNs;
			}
		}
	}
	sensorsdata->timestamp = data->lastTimestamp[sensor];
	*index += 4;
}

void get_sensordata(struct ssp_data *data, char *dataframe,
		int *index, int sensor, struct sensor_value *sensordata)
{
	memcpy(sensordata, dataframe + *index, data->data_len[sensor]);
	*index += data->data_len[sensor];
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
	queue_work(data->debug_wq, &big->work);
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
	s16 caldata[3] = {0, };
	u64 time_threshold = 0;

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
			sensortime.batch_count = sensortime.batch_count_fixed = length;
			sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;

			do {
				get_sensordata(data, dataframe, &index,
					sensor, &sensorsdata);

				if (data->cameraGyroSyncMode) {
					data->skipEventReport = false;
					get_timestamp(data, dataframe, &index, &sensorsdata, &sensortime, sensor);

					if (data->skipEventReport == false) {
						report_sensordata(data, sensor, &sensorsdata);
					}
				} else {
					get_timestamp(data, dataframe, &index, &sensorsdata, &sensortime, sensor);

					if (sensortime.batch_mode == BATCH_MODE_NONE) {
						if (data->adDelayBuf[sensor] < 30000000ULL) {
							time_threshold = 30000000ULL;
						} else if (data->adDelayBuf[sensor] < 180000000ULL) {
							time_threshold = 100000000ULL;
						} else {
							time_threshold = 180000000ULL;
						}

						if (data->timestamp > time_threshold + data->lastTimestamp[sensor]) {
							report_sensordata(data, sensor, &sensorsdata);
							index -= 4;
							get_timestamp(data, dataframe, &index, &sensorsdata, &sensortime, sensor);
						}
					}
					report_sensordata(data, sensor, &sensorsdata);
				}
				
				sensortime.batch_count--;
			} while ((sensortime.batch_count > 0) && (index < frame_len));

			if (sensortime.batch_count > 0)
				ssp_errf("batch count error (%d)", sensortime.batch_count);

			//data->lastTimestamp[sensor] = data->timestamp;
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
		}
	}

	return SUCCESS;
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->ssp_big_task[BIG_TYPE_DUMP] = ssp_dump_task;
	data->ssp_big_task[BIG_TYPE_READ_LIB] = ssp_read_big_library_task;
}
