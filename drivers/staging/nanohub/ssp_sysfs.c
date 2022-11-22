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
#include <linux/iio/iio.h>
#include <linux/math64.h>
#include <linux/string.h>

#include "ssp_iio.h"
#define BATCH_IOCTL_MAGIC		0xFC

extern bool ssp_pkt_dbg;

struct batch_config {
	int64_t timeout;
	int64_t delay;
	int flag;
};

/*************************************************************************/
/* SSP data delay function                                              */
/*************************************************************************/

int get_msdelay(int64_t dDelayRate)
{
	return div_s64(dDelayRate, 1000000);
}

s32 SettingVDIS_Support(struct ssp_data *data, int64_t *dNewDelay)
{
	s32 dMsDelay = 0;
	int64_t NewDelay = *dNewDelay;

	if ((NewDelay == CAMERA_GYROSCOPE_SYNC || NewDelay == CAMERA_GYROSCOPE_VDIS_SYNC
		|| NewDelay == CAMERA_GYROSCOPE_SUPER_VDIS_SYNC
		|| NewDelay == CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC) && !data->IsVDIS_Enabled) {
		data->IsVDIS_Enabled = true;
		data->ts_stacked_cnt = 0;
		send_vdis_flag(data, data->IsVDIS_Enabled);
	} else if (!(NewDelay == CAMERA_GYROSCOPE_SYNC || NewDelay == CAMERA_GYROSCOPE_VDIS_SYNC
		|| NewDelay == CAMERA_GYROSCOPE_SUPER_VDIS_SYNC
		|| NewDelay == CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC) && data->IsVDIS_Enabled) {
		data->IsVDIS_Enabled = false;
		data->ts_stacked_cnt = 0;
		send_vdis_flag(data, data->IsVDIS_Enabled);
	}
	if (NewDelay == CAMERA_GYROSCOPE_SYNC) {
		*dNewDelay = CAMERA_GYROSCOPE_SYNC_DELAY;
		data->cameraGyroSyncMode = true;
	} else if (NewDelay == CAMERA_GYROSCOPE_VDIS_SYNC) {
		*dNewDelay = CAMERA_GYROSCOPE_VDIS_SYNC_DELAY;
		data->cameraGyroSyncMode = true;
		initialize_super_vdis_setting();
	} else if (NewDelay == CAMERA_GYROSCOPE_SUPER_VDIS_SYNC) {
		*dNewDelay = CAMERA_GYROSCOPE_SUPER_VDIS_SYNC_DELAY;
		data->cameraGyroSyncMode = true;
		initialize_super_vdis_setting();
	} else if (NewDelay == CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC) {
		*dNewDelay = CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC_DELAY;
		data->cameraGyroSyncMode = true;
		initialize_super_vdis_setting();
	} else {
		data->cameraGyroSyncMode = false;
		if ((data->adDelayBuf[GYROSCOPE_SENSOR] == NewDelay)
			&& (data->aiCheckStatus[GYROSCOPE_SENSOR] == RUNNING_SENSOR_STATE)) {
			pr_err("[SSP] same delay ignored!\n");
			return 0;
		}
	}

	if ((data->cameraGyroSyncMode == true) && (data->aiCheckStatus[GYROSCOPE_SENSOR] == RUNNING_SENSOR_STATE)) {
		if (data->adDelayBuf[GYROSCOPE_SENSOR] == NewDelay)
			return 0;
	}

	dMsDelay = get_msdelay(*dNewDelay);
	return dMsDelay;
}

static void enable_sensor(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	u64 uNewEnable = 0;
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);
	int ret = 0;

	// SUPPORT CAMERA SYNC ++++++
	if (iSensorType == GYROSCOPE_SENSOR) {
		dMsDelay = SettingVDIS_Support(data, &dNewDelay);
		if (dMsDelay == 0)
			return;
	}
	// SUPPORT CAMERA SYNC -----

	data->adDelayBuf[iSensorType] = dNewDelay;
	maxBatchReportLatency = data->batchLatencyBuf[iSensorType];
	batchOptions = data->batchOptBuf[iSensorType];

	switch (data->aiCheckStatus[iSensorType]) {
	case ADD_SENSOR_STATE:
		ssp_dbg("[SSP]: %s - add %llu, New = %lldns\n",
			 __func__, 1ULL << iSensorType, dNewDelay);

		if (iSensorType == PROXIMITY_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_PROX_FACTORYCAL
			get_proximity_threshold(data);
			proximity_open_calibration(data);
#endif
			set_proximity_threshold(data);
		}

		if (iSensorType == PROXIMITY_ALERT_SENSOR)
			set_proximity_alert_threshold(data);

		if (iSensorType == LIGHT_SENSOR 
			|| iSensorType == LIGHT_IR_SENSOR
			|| iSensorType == LIGHT_CCT_SENSOR
			|| iSensorType == UNCAL_LIGHT_SENSOR ) {
			data->light_log_cnt = 0;
			data->light_ir_log_cnt = 0;
			data->light_cct_log_cnt = 0;
		}

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;

		ret = send_instruction(data, ADD_SENSOR, iSensorType, uBuf, 9);
		pr_info("[SSP], delay %d, timeout %d, flag=%d, ret%d\n",
			dMsDelay, maxBatchReportLatency, uBuf[8], ret);
		if (ret <= 0) {
			uNewEnable =
				(u64)atomic64_read(&data->aSensorEnable)
				& (~(u64)(1ULL << iSensorType));
			atomic64_set(&data->aSensorEnable, uNewEnable);

			data->aiCheckStatus[iSensorType] = NO_SENSOR_STATE;
			break;
		}

		data->aiCheckStatus[iSensorType] = RUNNING_SENSOR_STATE;

		break;
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %llu, New = %lldns\n",
			__func__, 1ULL << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		data->aiCheckStatus[iSensorType] = ADD_SENSOR_STATE;
	}
}
static void change_sensor_delay(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);

	// SUPPORT CAMERA SYNC ++++++
	if (iSensorType == GYROSCOPE_SENSOR) {
		dMsDelay = SettingVDIS_Support(data, &dNewDelay);
		if (dMsDelay == 0)
			return;
	}
	// SUPPORT CAMERA SYNC -----

	data->adDelayBuf[iSensorType] = dNewDelay;
	data->batchLatencyBuf[iSensorType] = maxBatchReportLatency;
	data->batchOptBuf[iSensorType] = batchOptions;

	switch (data->aiCheckStatus[iSensorType]) {
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %llu, New = %lldns\n",
			__func__, 1ULL << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		break;
	}
}

/*************************************************************************/
/* SSP data enable function                                              */
/*************************************************************************/

static int ssp_remove_sensor(struct ssp_data *data,
	unsigned int uChangedSensor, u64 uNewEnable)
{
	u8 uBuf[4];
	int64_t dSensorDelay = data->adDelayBuf[uChangedSensor];

	ssp_dbg("[SSP]: %s - remove sensor = %lld, current state = %lld\n",
		__func__, (u64)(1ULL << uChangedSensor), uNewEnable);

	data->adDelayBuf[uChangedSensor] = DEFAULT_POLLING_DELAY;
	data->batchLatencyBuf[uChangedSensor] = 0;
	data->batchOptBuf[uChangedSensor] = 0;

	if (uChangedSensor == ORIENTATION_SENSOR) {
		if (!(atomic64_read(&data->aSensorEnable)
			& (1 << ACCELEROMETER_SENSOR))) {
			uChangedSensor = ACCELEROMETER_SENSOR;
		} else {
			change_sensor_delay(data, ACCELEROMETER_SENSOR,
				data->adDelayBuf[ACCELEROMETER_SENSOR]);
			return 0;
		}
	} else if (uChangedSensor == ACCELEROMETER_SENSOR) {
		if (atomic64_read(&data->aSensorEnable)
			& (1 << ORIENTATION_SENSOR)) {
			change_sensor_delay(data, ORIENTATION_SENSOR,
				data->adDelayBuf[ORIENTATION_SENSOR]);
			return 0;
		}
	} else if (uChangedSensor == GYROSCOPE_SENSOR) {
		data->cameraGyroSyncMode = false;
		data->IsVDIS_Enabled = false;
		send_vdis_flag(data, data->IsVDIS_Enabled);
	}

	if (!data->bSspShutdown)
		if (atomic64_read(&data->aSensorEnable) & (1ULL << uChangedSensor)) {
			s32 dMsDelay = get_msdelay(dSensorDelay);

			memcpy(&uBuf[0], &dMsDelay, 4);

			send_instruction(data, REMOVE_SENSOR,
				uChangedSensor, uBuf, 4);
		}
	data->aiCheckStatus[uChangedSensor] = NO_SENSOR_STATE;

	return 0;
}

/*************************************************************************/
/* ssp Sysfs                                                             */
/*************************************************************************/
static ssize_t show_sensors_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - cur_enable = %llu\n", __func__,
		 (u64)(atomic64_read(&data->aSensorEnable)));

	return sprintf(buf, "%llu\n", (u64)(atomic64_read(&data->aSensorEnable)));
}

static ssize_t set_sensors_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u64 dTemp;
	u64 uNewEnable = 0;
	unsigned int uChangedSensor = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = kstrtoull(buf, 10, &dTemp); 

	if (ret < 0) {
		pr_info("[SSP] %s - kstrtoull failed (%d)\n", __func__, ret);
		return ret;
	}

	uNewEnable = (u64)dTemp;
	ssp_dbg("[SSP]: %s - new_enable = %llu, old_enable = %llu\n", __func__,
		 uNewEnable, (u64)atomic64_read(&data->aSensorEnable));

	if ((uNewEnable != atomic64_read(&data->aSensorEnable)) &&
		!(data->uSensorState & (uNewEnable - atomic64_read(&data->aSensorEnable)))) {
		pr_info("[SSP] %s - %llu is not connected(sensortate: 0x%llx)\n",
			__func__, uNewEnable - atomic64_read(&data->aSensorEnable), data->uSensorState);
		return -EINVAL;
	}

	if (uNewEnable == atomic64_read(&data->aSensorEnable))
		return size;

	mutex_lock(&data->enable_mutex);
	for (uChangedSensor = 0; uChangedSensor < SENSOR_MAX; uChangedSensor++) {
		if ((atomic64_read(&data->aSensorEnable) & (1ULL << uChangedSensor))
			!= (uNewEnable & (1ULL << uChangedSensor))) {

			if (!(uNewEnable & (1ULL << uChangedSensor))) {
				ssp_remove_sensor(data, uChangedSensor,
					uNewEnable); /* disable */
			} else { /* Change to ADD_SENSOR_STATE from KitKat */
				if (data->aiCheckStatus[uChangedSensor] == INITIALIZATION_STATE) {
					if (uChangedSensor == ACCELEROMETER_SENSOR) {
						accel_open_calibration(data);
						if (set_accel_cal(data) < 0)
							pr_err("[SSP]: %s - set_accel_cal failed\n", __func__);
					} else if (uChangedSensor == PRESSURE_SENSOR) {
						pressure_open_calibration(data);
						open_pressure_sw_offset_file(data);
					}
					else if (uChangedSensor == PROXIMITY_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_PROX_FACTORYCAL
						get_proximity_threshold(data);
						proximity_open_calibration(data);
#endif
						set_proximity_threshold(data);
					} else if (uChangedSensor == PROXIMITY_ALERT_SENSOR) {
						set_proximity_alert_threshold(data);
					}
				}
				data->aiCheckStatus[uChangedSensor] = ADD_SENSOR_STATE;
				enable_sensor(data, uChangedSensor, data->adDelayBuf[uChangedSensor]);
			}
			break;
		}
	}
	atomic64_set(&data->aSensorEnable, uNewEnable);
	mutex_unlock(&data->enable_mutex);

	return size;
}

static ssize_t set_flush(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	u8 sensor_type = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	sensor_type = (u8)dTemp;
	if (!(atomic64_read(&data->aSensorEnable) & (1ULL << sensor_type)))
		return -EINVAL;

	if (flush(data, sensor_type) < 0) {
		pr_err("[SSP] ssp returns error for flush(%x)\n", sensor_type);
		return -EINVAL;
	}

	return size;
}

int get_index_by_name(struct iio_dev *indio_dev, struct ssp_data *data)
{
	int i = 0, index = -1;

	for (i = 0; i < SENSOR_MAX; i++) {
		if (data->indio_dev[i] != NULL && strcmp(data->indio_dev[i]->name, indio_dev->name) == 0) {
			index = i;
			break;
		}
	}

	return index;
}

static ssize_t show_sensor_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ssp_data *data = dev_get_drvdata(dev);
	int index = get_index_by_name(indio_dev, data);

	if (index >= 0) {
		snprintf(buf, PAGE_SIZE, "%lld\n", data->adDelayBuf[index]);
	}

	pr_err("[SSP]: %s, dev_name = %s index = %d\n", __func__, indio_dev->name, index);
	return 0;
}

static ssize_t set_sensor_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ssp_data *data = dev_get_drvdata(dev);
	int index = get_index_by_name(indio_dev, data);
	int64_t delay = 0;

	if (kstrtoll(buf, 10, &delay) < 0)
		return -EINVAL;

	if (index >= 0) {
		change_sensor_delay(data, index, delay);
	}

	pr_err("[SSP]: %s, dev_name = %s index = %d\n", __func__, indio_dev->name, index);
	return 0;
}

// for data injection
static ssize_t show_data_injection_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->data_injection_enable);
}

static ssize_t set_data_injection_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;
	unsigned int iRet;
	int64_t buffer;

	if (kstrtoll(buf, 10, &buffer) < 0)
		return -EINVAL;

	if (buffer != 1 && buffer != 0)
		return -EINVAL;

	data->data_injection_enable = buffer;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_DATA_INJECTION_MODE_ON_OFF;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);

	msg->free_buffer = 1;
	msg->buffer[0] = data->data_injection_enable;
	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Data Injection Mode fail %d\n", __func__, iRet);
		return iRet;
	}

	pr_info("[SSP] %s Data Injection Mode Success %d data_injection_enable %d\n",
		__func__, iRet, data->data_injection_enable);
	return size;
}

///*#ifdef CONFIG_SENSORS_SSP_PICASSO
static ssize_t show_sensor_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return sprintf(buf, "%s", data->sensor_state);
}
//#endif*/

static unsigned char fsm_setting[2048] = { 0, };

int send_fsm_setting(struct ssp_data *ssp_data_info) {
	struct ssp_msg *msg;
	int ret = 0;
	int fsm_setting_size = 0;
	char read_buffer[1024] =  {0, };
	int read_buffer_size = sizeof(read_buffer);

	ret = file_manager_read(ssp_data_info, SSP_FSM_SETTING_PATH, (char *)&read_buffer, read_buffer_size, 0);

	if (ret > 0) {
		char *pstr1 = NULL, *pstr2 = NULL, *pstr3 = NULL;
		unsigned char temp = 0;

		pr_err("[SSP] %s: fsm setting read_size = %d", __func__, ret);
		
		fsm_setting_size = ret;

		pstr1 = pstr2 = kstrdup(read_buffer, GFP_KERNEL);
		while (((pstr3 = strsep(&pstr2, ", ")) != NULL) && (fsm_setting_size < sizeof(fsm_setting))) {
			if(strlen(pstr3) != 0) {
				if(kstrtou8(pstr3, 16, &temp) >= 0){
					pr_err("[SSP] %s: %s, fsm_setting[%d] = %d",
					       	__func__, pstr3, fsm_setting_size, temp);

					fsm_setting[fsm_setting_size++] = temp;
				}
			}
		}

		kfree(pstr1);

	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_FSM_SETTING;
	msg->length = fsm_setting_size;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(fsm_setting_size, GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, fsm_setting, fsm_setting_size);

	ret = ssp_spi_async(ssp_data_info, msg);
	if (ret != SUCCESS) {
		        pr_err("[SSP] %s - i2c fail %d\n", __func__, ret);
			        ret = ERROR;
	}

	return ret;
}

void show_calibration_file(struct ssp_data *data) {
	char cal_data[SENSOR_CAL_FILE_SIZE] =  { 0, };
	char output[1024] = {0, };
	int output_size = 0;
	int i = 0, j = 0;

	int cpos[5] = {0,12,25,33,41};
	int csize[5] = {12,13,8,8,22};
	char cstr[5][256] = {"GYRO","MAG","LIGHT","PROX","OCTA"};

	file_manager_read(data, "/efs/FactoryApp/gyro_cal_data", (char *)cal_data, sizeof(cal_data), 0);

	for(i = 0; i < SENSOR_CAL_FILE_SIZE; i++) {
		output_size += sprintf(output + output_size, "0x%X ", cal_data[i]);

		if(i == cpos[j] + csize[j] - 1){
			pr_err("[SSP] %s(%d) : %s", cstr[j], csize[j], output);
			output_size = 0;
			//memset(output, 0, sizeof(output));
			j++;
		}
	}
}

static ssize_t set_ssp_control(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	char *pos = 0;

	pr_info("[SSP] SSP_CONTROL : %s\n", buf);

	if (strstr(buf, SSP_DEBUG_TIME_FLAG_ON))
		ssp_debug_time_flag = true;
	else if (strstr(buf, SSP_DEBUG_TIME_FLAG_OFF))
		ssp_debug_time_flag = false;

	else if (strstr(buf, SSP_DEBUG_ON))
		ssp_pkt_dbg = true;
	else if (strstr(buf, SSP_DEBUG_OFF))
		ssp_pkt_dbg = false;
	else if (strstr(buf, SSP_HALL_IC_ON)
 		|| strstr(buf, SSP_HALL_IC_OFF)) {

		data->hall_ic_status = strstr(buf, SSP_HALL_IC_ON) ? 1 : 0;
	
		iRet = send_hall_ic_status(data->hall_ic_status);

		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - hall ic command, failed %d\n", __func__, iRet);
			return iRet;
		}

		pr_info("[SSP] %s HALL IC ON/OFF, %d enabled %d\n",
			__func__, iRet, data->hall_ic_status);

	}
	else if (strstr(buf, SSP_FSM_SETTING)){
		send_fsm_setting(data);
	}
	else if (strstr(buf, SSP_SENSOR_CAL_READ)) {
		show_calibration_file(data);
	}
	else if((pos = strstr(buf, SSP_ACC_POSITION))){
		int len = strlen(SSP_ACC_POSITION);
		if (pos + len != 0) {
			u8 temp = 0;
			if (kstrtou8(pos + len, 10, &temp) < 0)
				return size;
			data->accel_position = temp;
			set_sensor_position(data);
		}
	} 
	else if((pos = strstr(buf, SSP_MAG_POSITION))) {
		int len = strlen(SSP_MAG_POSITION);
		if (pos + len != 0) {
			u8 temp = 0;
			if (kstrtou8(pos + len, 10, &temp) < 0)
				return size;
			data->mag_position = temp;
			set_sensor_position(data);			
		}
	}
	else if((pos = strstr(buf, SSP_HUB_COMMAND))) {
		int len = strlen(SSP_HUB_COMMAND);
		int iRet = 0;
		struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

		if (msg == NULL) {
			iRet = -ENOMEM;
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
			return iRet;
		}
		msg->cmd = MSG2SSP_AP_HUB_COMMAND;
		msg->length = (strlen(buf) - len);
		msg->options = AP2HUB_WRITE;
		msg->buffer = (char *)(buf + len);
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);
		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
			return size;
		}
	}
	else if ((pos = strstr(buf, SSP_PMS_HYSTERESYIS))) {
		int len = strlen(SSP_PMS_HYSTERESYIS);
		int iRet = 0;
		struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

		if (msg == NULL) {
			iRet = -ENOMEM;
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
			return iRet;
		}
		msg->cmd = MSG2SSP_PMS_HYSTERESYIS;
		msg->length = (size - len);
		msg->options = AP2HUB_WRITE;
		msg->buffer = (char *)(buf + len);
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);

		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
			return size;
		} else {
			pr_err("[SSP] %s, PMS hysteresis send success\n", __func__);
		}
	}
	else if ((pos = strstr(buf, SSP_AUTO_ROTATION_ORIENTATION))) {
		int len = strlen(SSP_AUTO_ROTATION_ORIENTATION);
		int iRet = 0;
		struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

		if (msg == NULL) {
			iRet = -ENOMEM;
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
			return iRet;
		}
		msg->cmd = MSG2SSP_AUTO_ROTATION_ORIENTATION;
		msg->length = 1;
		msg->options = AP2HUB_WRITE;
		msg->buffer = (char *)(buf + len);
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);

		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
			return size;
		} else {
			pr_err("[SSP] %s, AUTO_ROTATION_ORIENTATION send success\n", __func__);
		}
	}
	else if (strstr(buf, SSP_SAR_BACKOFF_MOTION_NOTI)) {
		int len = strlen(SSP_SAR_BACKOFF_MOTION_NOTI);
		int iRet = 0;
		struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

		if (msg == NULL) {
			iRet = -ENOMEM;
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
			return iRet;
		}
		msg->cmd = MSG2SSP_AP_SAR_BACKOFF_MOTION_NOTI;
		msg->length = 4;
		msg->options = AP2HUB_WRITE;
		msg->buffer = (char *)(buf + len);
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);

		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
			return size;
		} else {
			pr_err("[SSP] %s, SSP_SAR_BACKOFF_MOTION_NOTI send success\n", __func__);
		}
	}
	else if((pos = strstr(buf, SSP_FILE_MANAGER_READ))){
		int len = strlen(SSP_FILE_MANAGER_READ);

		if (size >= (FM_BUFFER_SIZE - len)) {
			pr_err("[SSP] %s, invalid data size for ssp_msg\n", __func__);
			return size;
		}

		memcpy(&data->hub_data->fm_rx_buffer, buf + len, size - len);
		complete(&data->hub_data->fm_read_done);
	}
	else if((pos = strstr(buf,SSP_FILE_MANAGER_WRITE))){
		complete(&data->hub_data->fm_write_done);
	}
	else if (strstr(buf,SSP_LIGHT_SEAMLESS_THD)) {
		int thd[2] = {200, 1000000};
		int index = 0;

		char *str = (char *)(buf + strlen(SSP_LIGHT_SEAMLESS_THD));
		char *token = strsep(&str, ",");

		int iRet = 0;
		struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

		while (token != NULL && index < 2) {
			iRet = kstrtoint(token, 10, &thd[index++]);
			if (iRet < 0) {
				pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iRet);
				if (msg != NULL)
					kfree(msg);
				return iRet;
			}
			token = strsep(&str, ",");
		}

		pr_err("set light_seamless threshold = %d,%d", thd[0], thd[1]);

		if (msg == NULL) {
			iRet = -ENOMEM;
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
			return iRet;
		}
		msg->cmd = MSG2SSP_AP_LIGHT_SEAMLESS_THD;
		msg->length = sizeof(thd);
		msg->options = AP2HUB_WRITE;
		msg->buffer = (char *)&thd;
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);
		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
			return size;
		}
	}
	else if (strstr(buf,SSP_SENSOR_HAL_INIT)) {
		initialize_mcu(data);
		sync_sensor_state(data);
	}

	return size;
}

static ssize_t get_ssp_control(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ssp_data *data = dev_get_drvdata(dev);

//	pr_err("[SSP_CONTROL] get_ssp_control = %s", data->hub_data->fm_tx_buffer);
	memcpy(buf, data->hub_data->fm_tx_buffer, data->hub_data->fm_tx_buffer_length);

	return data->hub_data->fm_tx_buffer_length;
}

static ssize_t sensor_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	char str_no_sensor_dump[] = "there is no sensor dump";
	int i = 0, ret;
	char *sensor_dump;
	char temp[sensor_dump_length(DUMPREGISTER_MAX_SIZE)+LENGTH_SENSOR_TYPE_MAX+3] = {0,};

	sensor_dump = kzalloc((sensor_dump_length(DUMPREGISTER_MAX_SIZE)+LENGTH_SENSOR_TYPE_MAX+3)*(ARRAY_SIZE(types)),
			GFP_KERNEL);

	for (i = 0; i < ARRAY_SIZE(types); i++) {

		if (data->sensor_dump[types[i]] != NULL) {
			snprintf(temp, (int)strlen(data->sensor_dump[types[i]])+LENGTH_SENSOR_TYPE_MAX+3,
				"%3d\n%s\n\n",/* %3d -> 3 : LENGTH_SENSOR_TYPE_MAX */
				types[i], data->sensor_dump[types[i]]);
			strcpy(&sensor_dump[(int)strlen(sensor_dump)], temp);
		}
	}

	if ((int)strlen(sensor_dump) == 0)
		ret = snprintf(buf, (int)strlen(str_no_sensor_dump)+1, "%s\n", str_no_sensor_dump);
	else
		ret = snprintf(buf, (int)strlen(sensor_dump)+1, "%s\n", sensor_dump);

	kfree(sensor_dump);
	sensorhub_save_ram_dump();
	pr_info("[SSP]: %s, f/w version: BR01%u,BR01%u\n", __func__, get_module_rev(data), data->uCurFirmRev);

	return ret;
}

static ssize_t sensor_dump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data  = dev_get_drvdata(dev);
	int sensor_type, ret;
	char name[LENGTH_SENSOR_NAME_MAX+1] = {0,};
	int iRet;

	iRet = sscanf(buf, "%30s", name);		/* 30 : LENGTH_SENSOR_NAME_MAX */
	if (iRet < 0)
		return iRet;

	if ((strcmp(name, "all")) == 0) {
		ret = send_all_sensor_dump_command(data);
	} else {
		if (strcmp(name, "accelerometer") == 0)
			sensor_type = ACCELEROMETER_SENSOR;
		else if (strcmp(name, "gyroscope") == 0)
			sensor_type = GYROSCOPE_SENSOR;
		else if (strcmp(name, "magnetic") == 0)
			sensor_type = GEOMAGNETIC_UNCALIB_SENSOR;
		else if (strcmp(name, "pressure") == 0)
			sensor_type = PRESSURE_SENSOR;
		else if (strcmp(name, "proximity") == 0)
			sensor_type = PROXIMITY_SENSOR;
		else if (strcmp(name, "light") == 0)
			sensor_type = LIGHT_SENSOR;
		else {
			pr_err("[SSP] %s - is not supported : %s", __func__, buf);
			sensor_type = -1;
			return -EINVAL;
		}
		ret = send_sensor_dump_command(data, sensor_type);
	}

	return (ret == SUCCESS) ? size : ret;
}

static ssize_t show_thermistor_channel0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	short buffer = 0;
	int iRet = 0;

	msg->cmd = MSG2SSP_AP_THERMISTOR_CHANNEL_0;
	msg->length = sizeof(buffer);
	msg->options = AP2HUB_READ;
	msg->buffer = (char *)&buffer;

	iRet = ssp_spi_sync(data, msg, 1000);
	buffer = thermistor_rawToTemperature(data, 0, buffer);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		return 0;
	} else {
		pr_err("[SSP]: %s -- %d\n", __func__, (int)buffer / 10);

		return sprintf(buf, "%d\n", (int)buffer / 10);
	}
}

static ssize_t show_thermistor_channel1(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	short buffer = 0;
	int iRet = 0;

	msg->cmd = MSG2SSP_AP_THERMISTOR_CHANNEL_1;
	msg->length = sizeof(buffer);
	msg->options = AP2HUB_READ;
	msg->buffer = (char *)&buffer;

	iRet = ssp_spi_sync(data, msg, 1000);
	buffer = thermistor_rawToTemperature(data, 1, buffer);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		return 0;
	} else {
		pr_err("[SSP]: %s -- %d\n", __func__, (int)buffer / 10);

		return sprintf(buf, "%d\n", (int)buffer / 10);
	}
}

char resetString[1024] = {0, };
static ssize_t reset_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (strlen(data->resetInfo) == 0)
		return 0;

	return  sprintf(buf, "%s", data->resetInfo);
}
static ssize_t show_spu_verify(struct device *dev, 
		struct device_attribute *attr, char *buf) {
	//const struct firmware *fw_entry;
	//int ret = request_firmware_update(URGENT_FIRMWARE_TEST_PATH, &fw_entry);
	int ret = 0;
	if(ret < 0) {
		return sprintf(buf, "%s\n", "NG"); 
	} else {
		return sprintf(buf, "%s\n", "OK"); 
	}
}

bool fs_ready = false;

static ssize_t fs_ready_show(struct device *dev, 
		struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%d\n", fs_ready); 
}

static ssize_t fs_ready_store(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct ssp_data *data  = dev_get_drvdata(dev);
	fs_ready = true;

	sensorhub_fs_ready(data);
	return size;
}

static ssize_t show_fw_name(struct device *dev, 
		struct device_attribute *attr, char *buf) {
	struct ssp_data *data  = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", data->fw_name); 
}

static DEVICE_ATTR(mcu_rev, 0444, mcu_revision_show, NULL);
static DEVICE_ATTR(mcu_name, 0444, mcu_model_name_show, NULL);
static DEVICE_ATTR(mcu_reset, 0444, mcu_reset_show, NULL);
static DEVICE_ATTR(mcu_test, 0664,
	mcu_factorytest_show, mcu_factorytest_store);
static DEVICE_ATTR(mcu_sleep_test, 0664,
	mcu_sleep_factorytest_show, mcu_sleep_factorytest_store);
static DEVICE_ATTR(poll_delay, 0664, show_sensor_delay, set_sensor_delay);
static DEVICE_ATTR(enable, 0664,
	show_sensors_enable, set_sensors_enable);
static DEVICE_ATTR(ssp_flush, 0220,
	NULL, set_flush);

// for data injection
static DEVICE_ATTR(data_injection_enable, 0664,
	show_data_injection_enable, set_data_injection_enable);

static DEVICE_ATTR(ssp_control, 0664, get_ssp_control, set_ssp_control);
static DEVICE_ATTR(sensor_dump, 0664,
	sensor_dump_show, sensor_dump_store);
static DEVICE_ATTR(reset_info, 0440, reset_info_show, NULL);
static DEVICE_ATTR(sensor_state, 0444, show_sensor_state, NULL);

static DEVICE_ATTR(thermistor_channel_0, 0444, show_thermistor_channel0, NULL);
static DEVICE_ATTR(thermistor_channel_1, 0444, show_thermistor_channel1, NULL);

static DEVICE_ATTR(spu_verify, 0444, show_spu_verify, NULL);
static DEVICE_ATTR(fs_ready, 0660, fs_ready_show, fs_ready_store);

static DEVICE_ATTR(fw_name, 0444, show_fw_name, NULL);

static struct device_attribute *mcu_attrs[] = {
	&dev_attr_enable,
	&dev_attr_mcu_rev,
	&dev_attr_mcu_name,
	&dev_attr_mcu_test,
	&dev_attr_mcu_reset,
	&dev_attr_mcu_sleep_test,
	&dev_attr_ssp_flush,
	&dev_attr_data_injection_enable,
	&dev_attr_sensor_state,
	&dev_attr_ssp_control,
	&dev_attr_sensor_dump,
	&dev_attr_thermistor_channel_0,
	&dev_attr_thermistor_channel_1,
	&dev_attr_reset_info,
	&dev_attr_spu_verify,
	&dev_attr_fs_ready,
	&dev_attr_fw_name,
	NULL,
};

static long ssp_batch_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_data *data
		= container_of(file->private_data,
			struct ssp_data, batch_io_device);

	struct batch_config batch;

	void __user *argp = (void __user *)arg;
	int retries = 2;
	int ret = 0;
	int sensor_type, ms_delay;
	int64_t dNewDelay;
	int timeout_ms = 0;
	u8 uBuf[9];

	sensor_type = (cmd & 0xFF);

	if (sensor_type >= SENSOR_MAX) {
		pr_err("[SSP] Invalid sensor_type %d\n", sensor_type);
		return -EINVAL;
	}

	if ((cmd >> 8 & 0xFF) != BATCH_IOCTL_MAGIC) {
		pr_err("[SSP] Invalid BATCH CMD %x\n", cmd);
		return -EINVAL;
	}

	while (retries--) {
		ret = copy_from_user(&batch, argp, sizeof(batch));
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		pr_err("[SSP] batch ioctl err(%d)\n", ret);
		return -EINVAL;
	}
	dNewDelay = batch.delay;
	ms_delay = get_msdelay(batch.delay);
	timeout_ms = div_s64(batch.timeout, 1000000);
	memcpy(&uBuf[0], &ms_delay, 4);
	memcpy(&uBuf[4], &timeout_ms, 4);
	uBuf[8] = batch.flag;

	// SUPPORT CAMERA SYNC ++++++
	if (sensor_type == GYROSCOPE_SENSOR) {
		ms_delay = SettingVDIS_Support(data, &dNewDelay);
		if (ms_delay == 0)
			return 0;
		memcpy(&uBuf[0], &ms_delay, 4);
	}
	// SUPPORT CAMERA SYNC -----

	if (batch.timeout) { /* add or dry */

		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* real batch, NOT DRY, change delay */
			ret = 1;
			/* if sensor is not running state, enable will be called.
			 *  MCU return fail when receive chage delay inst during NO_SENSOR STATE
			 */
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE)
				ret = send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);

			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = dNewDelay;
			}
		} else { /* real batch, DRY RUN */
			ret = send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = dNewDelay;
			}
		}
	} else { /* remove batch or normal change delay, remove or add will be called. */
		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* no batch, NOT DRY, change delay */
			data->batchOptBuf[sensor_type] = 0;
			data->batchLatencyBuf[sensor_type] = 0;
			data->adDelayBuf[sensor_type] = dNewDelay;
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE)
				send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);
		}
	}

	pr_info("[SSP] batch %d: delay %lld, timeout %lld, flag %d, ret %d\n",
		sensor_type, dNewDelay, batch.timeout, batch.flag, ret);
	if (!batch.timeout)
		return 0;
	if (ret <= 0)
		return -EINVAL;
	else
		return 0;
}


static struct file_operations const ssp_batch_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = ssp_batch_ioctl,
};

static ssize_t ssp_data_injection_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_data *data
		= container_of(file->private_data,
			struct ssp_data, ssp_data_injection_device);

	struct ssp_msg *msg;
	int ret = 0;
	char *send_buffer;
	unsigned int sensor_type = 0;
	size_t x = 0;

	if (unlikely(count < 4)) {
		pr_err("[SSP] %s data length err(%d)\n", __func__, (u32)count);
		return -EINVAL;
	}

	send_buffer = kcalloc(count, sizeof(char), GFP_KERNEL);
	if (unlikely(!send_buffer)) {
		pr_err("[SSP] %s allocate memory for kernel buffer err\n", __func__);
		return -ENOMEM;
	}

	ret = copy_from_user(send_buffer, buf, count);
	if (unlikely(ret)) {
		pr_err("[SSP] %s memcpy for kernel buffer err\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	pr_info("[SSP] %s count %d endable %d\n", __func__, (u32)count, data->data_injection_enable);

// sensorhub sensor type is enum, HAL layer sensor type is 1 << sensor_type. So it needs to change to enum format.
	for (sensor_type = 0; sensor_type < SENSOR_MAX; sensor_type++) {
		if (send_buffer[0] == (1ULL << sensor_type)) {
			send_buffer[0] = sensor_type; // sensor type change to enum format.
			pr_info("[SSP] %s sensor_type = %d %d\n", __func__, sensor_type, (1ULL << sensor_type));
			break;
		}
		if (sensor_type == SENSOR_MAX - 1)
			pr_info("[SSP] %s there in no sensor_type\n", __func__);
	}

	for (x = 0; x < count; x++)
		pr_info("[SSP] %s Data Injection : %d 0x%x\n", __func__, send_buffer[x], send_buffer[x]);


	// injection data send to sensorhub.
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_DATA_INJECTION_SEND;
	msg->length = count;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(count, GFP_KERNEL);
	msg->free_buffer = 1;

	memcpy(msg->buffer, send_buffer, count);

	ret = ssp_spi_async(data, msg);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Data Injection fail %d\n", __func__, ret);
		goto exit;
		//return ret;
	}

	pr_info("[SSP] %s Data Injection Success %d\n", __func__, ret);

exit:
	kfree(send_buffer);
	return ret >= 0 ? 0 : -1;
}

static struct file_operations const ssp_data_injection_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_data_injection_write,

};

static void initialize_mcu_factorytest(struct ssp_data *data)
{
	sensors_register_dcopy(&data->mcu_device, data, mcu_attrs, "ssp_sensor");
}

static void remove_mcu_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->mcu_device, mcu_attrs);
}

int initialize_sysfs(struct ssp_data *data)
{
	int i = 0;

	for (i = 0; i < SENSOR_MAX; i++) {
		if (data->indio_dev[i] != NULL) {
			if (device_create_file(&data->indio_dev[i]->dev, &dev_attr_poll_delay) < 0)
				pr_err("[SSP]: %s -  device_create_filefail %d\n", __func__);
		}
	}

	data->batch_io_device.minor = MISC_DYNAMIC_MINOR;
	data->batch_io_device.name = "batch_io";
	data->batch_io_device.fops = &ssp_batch_fops;
	if (misc_register(&data->batch_io_device))
		goto err_batch_io_dev;

	data->ssp_data_injection_device.minor = MISC_DYNAMIC_MINOR;
	data->ssp_data_injection_device.name = "ssp_data_injection";
	data->ssp_data_injection_device.fops = &ssp_data_injection_fops;
	if ((misc_register(&data->ssp_data_injection_device)) < 0)
		goto err_ssp_data_injection_device;

	initialize_accel_factorytest(data);
	initialize_gyro_factorytest(data);
	initialize_prox_factorytest(data);
	initialize_light_factorytest(data);
	initialize_pressure_factorytest(data);
	initialize_magnetic_factorytest(data);
	initialize_mcu_factorytest(data);
	if (data->fcd_data.enable)
		initialize_fcd_factorytest(data);

	initialize_thermistor_factorytest(data);

	return SUCCESS;

err_ssp_data_injection_device:
	misc_deregister(&data->batch_io_device);
err_batch_io_dev:
	pr_err("[SSP] error init sysfs\n");
	return ERROR;
}

void remove_sysfs(struct ssp_data *data)
{
	misc_deregister(&data->batch_io_device);

	misc_deregister(&data->ssp_data_injection_device);

	remove_accel_factorytest(data);
	remove_gyro_factorytest(data);
	remove_fcd_factorytest(data);
#ifndef CONFIG_SENSORS_SSP_CM3323
	remove_prox_factorytest(data);
#endif
	remove_light_factorytest(data);
	remove_pressure_factorytest(data);
	remove_magnetic_factorytest(data);
	remove_mcu_factorytest(data);

	remove_thremistor_factorytest(data);

	destroy_sensor_class();
}
