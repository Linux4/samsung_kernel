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
#include "ssp_iio.h"
#include "ssp_sensorhub.h"
#include <linux/string.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/types.h>

static struct sensor info_table[] = {
	SENSOR_INFO_ACCELEROMETER,
	SENSOR_INFO_GEOMAGNETIC,
	SENSOR_INFO_GEOMAGNETIC_POWER,
	SENSOR_INFO_GEOMAGNETIC_UNCAL,
	SENSOR_INFO_GYRO,
	SENSOR_INFO_GYRO_UNCALIBRATED,
	SENSOR_INFO_INTERRUPT_GYRO,
	SENSOR_INFO_PRESSURE,
	SENSOR_INFO_LIGHT,
	SENSOR_INFO_LIGHT_IR,
	SENSOR_INFO_LIGHT_FLICKER,
	SENSOR_INFO_PROXIMITY,
	SENSOR_INFO_PROXIMITY_ALERT,
	SENSOR_INFO_PROXIMITY_RAW,
	SENSOR_INFO_ROTATION_VECTOR,
	SENSOR_INFO_GAME_ROTATION_VECTOR,
	SENSOR_INFO_SIGNIFICANT_MOTION,
	SENSOR_INFO_STEP_DETECTOR,
	SENSOR_INFO_STEP_COUNTER,
	SENSOR_INFO_TILT_DETECTOR,
	SENSOR_INFO_PICK_UP_GESTURE,
	SENSOR_INFO_SCONTEXT,
	SENSOR_INFO_LIGHT_CCT,
	SENSOR_INFO_THERMISTOR,
	SENSOR_INFO_PROXIMITY_POCKET,
	SENSOR_INFO_ACCEL_UNCALIBRATED,
	SENSOR_INFO_META,
	SENSOR_INFO_WAKE_UP_MOTION,
	SENSOR_INFO_MOVE_DETECTOR,
	SENSOR_INFO_CALL_GESTURE,
	SENSOR_INFO_PROXIMITY_ADC_CALIB,
	SENSOR_INFO_UNCAL_LIGHT,
	SENSOR_INFO_POCKET_MODE,
	SENSOR_INFO_LED_COVER_EVENT,
	SENSOR_INFO_TAP_TRACKER,
	SENSOR_INFO_SHAKE_TRACKER,
	SENSOR_INFO_LIGHT_SEAMLESS,
	SENSOR_INFO_AUTOROTATION,
	SENSOR_INFO_FLIP_COVER_DETECTOR,
	SENSOR_INFO_SAR_BACKOFF_MOTION,
	SENSOR_INFO_FILE_MANAGER,
};

#define IIO_ST(si, rb, sb, sh)	\
	{ .sign = si, .realbits = rb, .storagebits = sb, .shift = sh }

#define THM_UP		0
#define THM_SUB		1

int info_table_size =  sizeof(info_table)/sizeof(struct sensor);

static struct sensor sensors_info[SENSOR_MAX];
static struct iio_chan_spec indio_channels[SENSOR_MAX];

int initialize_iio_buffer_and_device(struct iio_dev *indio_dev, struct ssp_data *data, int bytes)
{
	int iRet = 0;

	iRet = ssp_iio_configure_kfifo(indio_dev, bytes);
	if (iRet)
		goto err_config_ring_buffer;

	iRet = iio_device_register(indio_dev);
	if (iRet)
		goto err_register_device;

	iio_device_set_drvdata(indio_dev, data);

	return iRet;

err_register_device:
	pr_err("[SSP]: failed to register %s device\n", indio_dev->name);
	iio_device_unregister(indio_dev);
err_config_ring_buffer:
	pr_err("[SSP]: failed to configure %s buffer\n", indio_dev->name);
	ssp_iio_unconfigure_kfifo(indio_dev);

	return iRet;
}

static int ssp_push_iio_buffer(struct iio_dev *indio_dev, u64 timestamp, u8 *data, int data_len)
{
	int buf_len = data_len + sizeof(timestamp);
	u8 *buf = kzalloc(buf_len, GFP_KERNEL);
	int iRet = 0;

	memcpy(buf, data, data_len);
	memcpy(&buf[data_len], &timestamp, sizeof(timestamp));

	mutex_lock(&indio_dev->mlock);

	//do {
	//	if(retry != 3)
	//		usleep_range(150, 151);
	//pr_err("[SSP] %s++", __func__, indio_dev->name);
	iRet = iio_push_to_buffers(indio_dev, buf);
	//pr_err("[SSP] %s--", __func__, indio_dev->name);
	//} while(iRet < 0 && retry-- > 0);

	//if(iRet < 0)
		//pr_err("[SSP] %s - %s push fail erro %d", __func__, indio_dev->name, iRet);

	mutex_unlock(&indio_dev->mlock);

	kfree(buf);
	return 0;
}

void report_iio_data(struct ssp_data *data, int type, struct sensor_value *sensor_data)
{
	memcpy(&data->buf[type], sensor_data, sensors_info[type].get_data_len);
	ssp_push_iio_buffer(data->indio_dev[type], sensor_data->timestamp, (u8 *)(&data->buf[type]), sensors_info[type].report_data_len);
}

bool get_sensor_data(int sensor_type, char *pchRcvDataFrame, int *iDataIdx,
        struct sensor_value *sensorsdata)
{
	if (sensor_type < SENSOR_MAX && (sensor_type != BULK_SENSOR && sensor_type != GPS_SENSOR)) {
		int data_size = sensors_info[sensor_type].get_data_len;
		memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, data_size);
		*iDataIdx += data_size;
		return true;
	} else {
		return false;		
	}
}

void report_meta_data(struct ssp_data *data, int sensor_type, struct sensor_value *s)
{	
	pr_info("[SSP]: %s - what: %d, sensor: %d\n", __func__,
		s->meta_data.what, s->meta_data.sensor);

//	ssp_push_iio_buffer(data->meta_indio_dev, 0, (u8 *)(&s->meta_data), 8);

	if (s->meta_data.sensor == ACCELEROMETER_SENSOR) {
		s16 accel_buf[3];

		memset(accel_buf, 0, sizeof(s16) * 3);
		ssp_push_iio_buffer(data->indio_dev[ACCELEROMETER_SENSOR], 0, (u8 *)accel_buf, sizeof(accel_buf));
	} else if (s->meta_data.sensor == GYROSCOPE_SENSOR) {
		int gyro_buf[3];

		memset(gyro_buf, 0, sizeof(int) * 3);
		ssp_push_iio_buffer(data->indio_dev[GYROSCOPE_SENSOR], 0, (u8 *)gyro_buf, sizeof(gyro_buf));
	} else if (s->meta_data.sensor == GYRO_UNCALIB_SENSOR) {
		s32 uncal_gyro_buf[6];

		memset(uncal_gyro_buf, 0, sizeof(uncal_gyro_buf));
		ssp_push_iio_buffer(data->indio_dev[GYRO_UNCALIB_SENSOR], 0, (u8 *)uncal_gyro_buf, sizeof(uncal_gyro_buf));
	} else if (s->meta_data.sensor == GEOMAGNETIC_SENSOR) {
		u8 mag_buf[8];

		memset(mag_buf, 0, sizeof(mag_buf));
		ssp_push_iio_buffer(data->indio_dev[GEOMAGNETIC_SENSOR], 0, (u8 *)mag_buf, sizeof(mag_buf));
	} else if (s->meta_data.sensor == GEOMAGNETIC_UNCALIB_SENSOR) {
		u8 uncal_mag_buf[13];

		memset(uncal_mag_buf, 0, sizeof(uncal_mag_buf));
		ssp_push_iio_buffer(data->indio_dev[GEOMAGNETIC_UNCALIB_SENSOR], 0, (u8 *)uncal_mag_buf, sizeof(uncal_mag_buf));
	} else if (s->meta_data.sensor == PRESSURE_SENSOR) {
		int pressure_buf[3];

		memset(pressure_buf, 0, sizeof(int) * 3);
		ssp_push_iio_buffer(data->indio_dev[PRESSURE_SENSOR], 0, (u8 *)pressure_buf, sizeof(pressure_buf));
	} else if (s->meta_data.sensor == ROTATION_VECTOR) {
		int rot_buf[5];

		memset(rot_buf, 0, sizeof(int) * 5);
		ssp_push_iio_buffer(data->indio_dev[ROTATION_VECTOR], 0, (u8 *)rot_buf, sizeof(rot_buf));
	} else if (s->meta_data.sensor == GAME_ROTATION_VECTOR) {
		int grot_buf[5];

		memset(grot_buf, 0, sizeof(int) * 5);
		ssp_push_iio_buffer(data->indio_dev[GAME_ROTATION_VECTOR], 0, (u8 *)grot_buf, sizeof(grot_buf));
	} else if (s->meta_data.sensor == STEP_DETECTOR) {
		u8 step_buf[1] = {0};

		ssp_push_iio_buffer(data->indio_dev[STEP_DETECTOR], 0, (u8 *)step_buf, sizeof(step_buf));
	} else {
		ssp_push_iio_buffer(data->meta_indio_dev, 0, (u8 *)(&s->meta_data), 8);
	}

}

void report_mcu_ready(struct ssp_data *data) {
		struct sensor_value sensorsdata;

		sensorsdata.meta_data.what = -1;
		sensorsdata.meta_data.sensor = -1;

		report_meta_data(data, 0, &sensorsdata);
}

void report_scontext_data(struct ssp_data *data, int sensor_type,
		struct sensor_value *scontextbuf)
{
	short start, end;
	char printbuf[72] = {0, };

	memcpy(printbuf, scontextbuf, 72);
	memcpy(&start, printbuf+4, sizeof(short));
	memcpy(&end, printbuf+6, sizeof(short));

	if (end - start + 1 <= 64) {
		ssp_sensorhub_log("ssp_AlgoPacket", printbuf + 8, end - start + 1);
	} else {
		pr_err("[SSP] : ssp_AlgoPacket packet error start:%d end:%d", start, end);
	}

	mutex_lock(&data->indio_scontext_dev->mlock);
	iio_push_to_buffers(data->indio_scontext_dev, scontextbuf);
	mutex_unlock(&data->indio_scontext_dev->mlock);

	wake_lock_timeout(data->ssp_wake_lock, 0.3*HZ);
}

short thermistor_rawToTemperature(struct ssp_data *data, int type, s16 raw)
{
	int startTemp = -20, i = 0, diff;
	int temperature = 0, temperature_norm = 0;

	for (i = 0; i < ARRAY_SIZE(data->tempTable_up) - 1; i++) {
		if (type == THM_UP) {
			if (raw > data->tempTable_up[0]) {
				temperature = -20; // MIN value
				break;
			} else if (raw <= data->tempTable_up[22]) {
				temperature = 90; // MAX value
				break;
			} else if (data->tempTable_up[i] >= raw && raw > data->tempTable_up[i + 1]) {
				diff = data->tempTable_up[i] - data->tempTable_up[i + 1];
				//value : diff = temperature : 5
				temperature = startTemp + (5 * i) + ((data->tempTable_up[i] - raw) * 5 / diff);
				temperature_norm = ((data->tempTable_up[i] - raw) * 5 % diff);
				temperature_norm = (temperature_norm * 10) / diff;
				break;
			}
		} else {
			if (raw > data->tempTable_sub[0]) {
				temperature = -20; // MIN value
				break;
			} else if (raw <= data->tempTable_sub[22]) {
				temperature = 90; // MAX value
				break;
			} else if (data->tempTable_sub[i] >= raw && raw > data->tempTable_sub[i + 1]) {
				diff = data->tempTable_sub[i] - data->tempTable_sub[i + 1];
				temperature = startTemp + (5 * i) + ((data->tempTable_sub[i] - raw) * 5 / diff);
				temperature_norm = ((data->tempTable_sub[i] - raw) * 5 % diff);
				temperature_norm = (temperature_norm * 10) / diff;
				break;
			}
		}
	}

	temperature = temperature * 10 + temperature_norm;
	pr_info("[SSP] %s type: %d raw: %d temper: %d", __func__, type, raw, temperature);
	return temperature;
}

void report_thermistor_data(struct ssp_data *data, int sensor_type,
		struct sensor_value *thermistor_data)
{
	u8 reportData[3];
	s16 temperature = thermistor_rawToTemperature(data, thermistor_data->thermistor_type, thermistor_data->thermistor_raw);

	reportData[0] = thermistor_data->thermistor_type;
	memcpy(&reportData[1], &temperature, sizeof(temperature));
	ssp_push_iio_buffer(data->indio_dev[THERMISTOR_SENSOR], thermistor_data->timestamp, (u8 *)reportData, sizeof(reportData));
	wake_lock_timeout(data->ssp_wake_lock, 0.3*HZ);

	data->buf[THERMISTOR_SENSOR].thermistor_type = thermistor_data->thermistor_type;
	data->buf[THERMISTOR_SENSOR].thermistor_raw = thermistor_data->thermistor_raw;

}

void report_sensor_data(struct ssp_data *data, int sensor_type, struct sensor_value *s) {
	if (sensor_type == META_SENSOR) {
		report_meta_data(data, sensor_type, s);
	} 
	else if (sensor_type == GEOMAGNETIC_RAW) {
		data->buf[GEOMAGNETIC_RAW].x = s->x;
		data->buf[GEOMAGNETIC_RAW].y = s->y;
		data->buf[GEOMAGNETIC_RAW].z = s->z;
	}
	else if (sensor_type == PRESSURE_SENSOR) {
		int temp[3] = {0, };
#ifdef CONFIG_SEC_FACTORY
		data->buf[PRESSURE_SENSOR].pressure =
			s->pressure - data->iPressureCal;
#else
		data->buf[PRESSURE_SENSOR].pressure =
			s->pressure - data->iPressureCal - (data->sw_offset * data->convert_coef);
#endif
		data->buf[PRESSURE_SENSOR].temperature = s->temperature;
		temp[0] = data->buf[PRESSURE_SENSOR].pressure;
		temp[1] = data->buf[PRESSURE_SENSOR].temperature;
		temp[2] = data->sealevelpressure;

		ssp_push_iio_buffer(data->indio_dev[PRESSURE_SENSOR], s->timestamp, (u8 *)temp, sizeof(temp));
	}
	else if (sensor_type == STEP_COUNTER){
		data->buf[STEP_COUNTER].step_diff = s->step_diff;
		data->step_count_total += data->buf[STEP_COUNTER].step_diff;
		ssp_push_iio_buffer(data->indio_dev[STEP_COUNTER], s->timestamp,
				(u8 *)(&data->step_count_total), sensors_info[STEP_COUNTER].report_data_len);
	}
	else if (sensor_type == THERMISTOR_SENSOR){
		report_thermistor_data(data, sensor_type, s);
	}
	else if (sensor_type == FLIP_COVER_DETECTOR) {
		// Value 100 indicates factory data, so process only for factory test and skip reporting to HAL
		if (s->value == 100) {
			if (data->fcd_data.factory_cover_status)
				check_cover_detection_factory(data, s);
		} else {
			report_iio_data(data, FLIP_COVER_DETECTOR, s);

			pr_info("[SSP]: %s: value = %d nfc_result = %d diff = %d magX = %d stable_min_mag = %d stable_max_mag = %d ts = %llu", __func__,
				s->value, s->nfc_result, s->diff, s->magX, s->stable_min_mag, s->stable_max_mag, s->timestamp);
		}

		wake_lock_timeout(data->ssp_wake_lock, 0.3*HZ);
	}
	else {
		if (sensor_type == PROXIMITY_RAW) {
			if (data->uFactoryProxAvg[0]++ >= PROX_AVG_READ_NUM) {
				data->uFactoryProxAvg[2] /= PROX_AVG_READ_NUM;
				data->buf[PROXIMITY_RAW].prox_raw[1] = (u16)data->uFactoryProxAvg[1];
				data->buf[PROXIMITY_RAW].prox_raw[2] = (u16)data->uFactoryProxAvg[2];
				data->buf[PROXIMITY_RAW].prox_raw[3] = (u16)data->uFactoryProxAvg[3];

				data->uFactoryProxAvg[0] = 0;
				data->uFactoryProxAvg[1] = 0;
				data->uFactoryProxAvg[2] = 0;
				data->uFactoryProxAvg[3] = 0;
			} else {
				data->uFactoryProxAvg[2] += s->prox_raw[0];

				if (data->uFactoryProxAvg[0] == 1)
					data->uFactoryProxAvg[1] = s->prox_raw[0];
				else if (s->prox_raw[0] < data->uFactoryProxAvg[1])
					data->uFactoryProxAvg[1] = s->prox_raw[0];

				if (s->prox_raw[0] > data->uFactoryProxAvg[3])
					data->uFactoryProxAvg[3] = s->prox_raw[0];
			}

			data->buf[PROXIMITY_RAW].prox_raw[0] = s->prox_raw[0];

		}
		
		report_iio_data(data, sensor_type, s);
		//check wakeup type
		if (sensors_info[sensor_type].wakeup == true) {
			wake_lock_timeout(data->ssp_wake_lock, 0.3*HZ);
			pr_err("[SSP]: %s: %s ts: %llu", __func__, data->sensor_name[sensor_type], s->timestamp);
		}
		//check log 
		if (sensor_type == LIGHT_SENSOR || sensor_type == UNCAL_LIGHT_SENSOR) {
			if (data->light_log_cnt < 8) {
						char *header = sensor_type == UNCAL_LIGHT_SENSOR ? "#>UL" : "#>L";
						ssp_dbg("[SSP] %s lux=%u cct=%d r=%d g=%d b=%d c=%d atime=%d again=%d brightness=%d min_lux_flag=%d",
							header, data->buf[sensor_type].light_t.lux, data->buf[sensor_type].light_t.cct,
							data->buf[sensor_type].light_t.r, data->buf[sensor_type].light_t.g, data->buf[sensor_type].light_t.b,
							data->buf[sensor_type].light_t.w, data->buf[sensor_type].light_t.a_time, data->buf[sensor_type].light_t.a_gain,
							data->buf[sensor_type].light_t.brightness, data->buf[sensor_type].light_t.min_lux_flag);
						data->light_log_cnt++;
			}
		}
		else if (sensor_type == PROXIMITY_SENSOR) {
			ssp_dbg("[SSP] Proximity Sensor Detect : %u, raw : %u light_diff: %d ts : %llu\n",
				s->prox_detect, s->prox_adc, s->light_diff, s->timestamp);
		}
		else if (sensor_type == PROXIMITY_ALERT_SENSOR) {
			ssp_dbg("[SSP] Proximity alert Sensor Detect : %d, raw : %u ts : %llu\n",
	            s->prox_alert_detect, s->prox_alert_adc, s->timestamp);
		}
		else  if (sensor_type == POCKET_MODE_SENSOR) {
			pr_err("[SSP]: %s: state = %d reason = %d base = %d current = %d temp = %d ts: %llu", __func__,
				s->pocket_mode, s->pocket_reason, s->pocket_base_proxy,
				s->pocket_current_proxy, s->pocket_temp, s->timestamp);
		}		
	}
}

void file_manager_cmd(struct ssp_data *data, char flag)
{
	int ret = 0;
	char cmd[9] = {0,};

	cmd[0] = flag;

	mutex_lock(&data->indio_file_manager_dev->mlock);

	ret = iio_push_to_buffers(data->indio_file_manager_dev, &cmd);

	if (ret < 0)
		pr_err("[SSP] iio_push_fail(%d)", ret);

	mutex_unlock(&data->indio_file_manager_dev->mlock);
}

int file_manager_read(struct ssp_data *data, char *filename, char *buf, int size, long long pos)
{
	int ret = 0;
	char cmd[9] = {0,};
	struct ssp_sensorhub_data *phub = data->hub_data;

	mutex_lock(&data->indio_file_manager_dev->mlock);

	memset(phub->fm_rx_buffer, 0, sizeof(phub->fm_rx_buffer));
	memset(phub->fm_tx_buffer, 0, sizeof(phub->fm_tx_buffer));

	phub->fm_read_done.done = 0;

	phub->fm_tx_buffer_length = snprintf(phub->fm_tx_buffer, sizeof(phub->fm_tx_buffer), "%s,%d,%lld,0", filename, size, pos);
	pr_err("[SSP] file_manager_read: %s(%s)",phub->fm_tx_buffer,filename);

	ret = iio_push_to_buffers(data->indio_file_manager_dev, &cmd);
	if(ret < 0) {
		pr_err("[SSP] iio_push_fail(%d)",ret);
	} else {
		ret = wait_for_completion_timeout(&phub->fm_read_done, 0.1*HZ);
		if (ret == 0) {
			pr_err("[SSP] file_manager_read, completion timeout");
		}
	}

	memcpy(buf, phub->fm_rx_buffer, size);
	mutex_unlock(&data->indio_file_manager_dev->mlock);
	return size;
}

static char output[1024 * 2] = {0,};
static int output_size = 0;

int file_manager_write(struct ssp_data *data, char *filename, char *buf, int size, long long pos)
{
	int ret = 0;
	char cmd[9] = {1,};
	int i = 0;

	struct ssp_sensorhub_data *phub = data->hub_data;

	mutex_lock(&data->indio_file_manager_dev->mlock);
	
	memset(output, 0, sizeof(output));
	output_size = 0;

	memset(phub->fm_tx_buffer, 0, sizeof(phub->fm_tx_buffer));
	phub->fm_write_done.done = 0;

	phub->fm_tx_buffer_length = snprintf(phub->fm_tx_buffer, sizeof(phub->fm_tx_buffer), "%s,%d,%lld,", filename, size, pos);
	memcpy(phub->fm_tx_buffer + phub->fm_tx_buffer_length, buf, size);

	for(i = 0; i < size; i++)
		output_size += sprintf(output + output_size, "0x%x ", buf[i]);

	pr_err("[SSP] file_manager_write: %s(%s)",phub->fm_tx_buffer,filename);
	pr_err("[SSP] file_manager_write: %s",output);

	phub->fm_tx_buffer_length += size;

	ret = iio_push_to_buffers(data->indio_file_manager_dev, &cmd);
	if (ret < 0) {
		pr_err("[SSP] iio_push_fail(%d)", ret);
	} else {
		ret = wait_for_completion_timeout(&phub->fm_write_done, 0.1*HZ);
		if (ret == 0) {
			pr_err("[SSP] file_manager_write, completion timeout");
		}
	}

	mutex_unlock(&data->indio_file_manager_dev->mlock);
	return size;
}

static const struct iio_chan_spec indio_meta_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = IIO_CHANNEL,
		.scan_index = IIO_SCAN_INDEX,
		.scan_type = IIO_ST('s', 16 * 8, 16 * 8, 0)
	}
};

static const struct iio_chan_spec indio_file_manager_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = IIO_CHANNEL,
		.scan_index = 0,
		.scan_type = IIO_ST('s', 9 * 8, 9 * 8, 0)
	}
};

static const struct iio_chan_spec indio_scontext_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = IIO_CHANNEL,
		.scan_index = IIO_SCAN_INDEX,
		.scan_type.sign = 's',
		.scan_type.realbits = 192,
		.scan_type.storagebits = 192,
		.scan_type.shift = 0,
		.scan_type.repeat = 3
	}
};

static int ssp_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
		int *val, int *val2, long mask)
{
	return 0;
}


static const struct iio_info ssp_iio_info = {
	.read_raw = &ssp_read_raw,
};

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0, i = 0;

	memset(&sensors_info, -1, sizeof(struct sensor) * SENSOR_MAX);

	for (i = 0; i < info_table_size; i++) {
		int index = info_table[i].type;
		int bit_size = (info_table[i].report_data_len + 8) * BITS_PER_BYTE;
		int repeat_size = bit_size / 256 + 1;
		int	data_size = bit_size / repeat_size + (bit_size - (bit_size / repeat_size) * repeat_size);

		if (index < SENSOR_MAX) {
			sensors_info[index] = info_table[i];
			pr_info("[SSP-IIO] %s, bit_size=%d, repeat_size=%d, data_size=%d", sensors_info[index].name, bit_size, repeat_size, data_size);
			indio_channels[index].type = IIO_TIMESTAMP;
			indio_channels[index].channel = IIO_CHANNEL;
			indio_channels[index].scan_index = IIO_SCAN_INDEX;
			indio_channels[index].scan_type.sign = 's';
			indio_channels[index].scan_type.realbits = data_size;
			indio_channels[index].scan_type.storagebits = data_size;
			indio_channels[index].scan_type.shift = 0;
			indio_channels[index].scan_type.repeat = repeat_size;
		}

		// we do not check index < SENSOR_MAX because of META_SENSOR
		// scontext indio is not a sensor type
		if (index != -1 && info_table[i].report_data_len > 0) {
			struct iio_dev *indio_dev = NULL;

			indio_dev = iio_device_alloc(&data->pdev->dev, sizeof(data->pdev->dev));
			indio_dev->name = info_table[i].name;
			indio_dev->dev.parent = &data->pdev->dev;
			indio_dev->info = &ssp_iio_info;

			if (index == META_SENSOR)
				indio_dev->channels = indio_meta_channels;
			else if (strcmp(info_table[i].name, "scontext_iio") == 0)
				indio_dev->channels = indio_scontext_channels;
			else if (strcmp(info_table[i].name, "file_manager") == 0)
				indio_dev->channels = indio_file_manager_channels;
			else
				indio_dev->channels = &indio_channels[index];
			indio_dev->num_channels = 1;
			indio_dev->modes = INDIO_DIRECT_MODE;
			indio_dev->currentmode = INDIO_DIRECT_MODE;

			//iRet = initialize_iio_buffer_and_device(indio_dev, data, data_size / BITS_PER_BYTE);
			iRet = initialize_iio_buffer_and_device(indio_dev, data, bit_size / BITS_PER_BYTE);

			if (index == META_SENSOR)
				data->meta_indio_dev = indio_dev;
			else if (strcmp(info_table[i].name, "scontext_iio") == 0)
				data->indio_scontext_dev = indio_dev;
			else if (strcmp(info_table[i].name, "file_manager") == 0)
				data->indio_file_manager_dev = indio_dev;
			else
				data->indio_dev[index] = indio_dev;

			pr_err("[SSP] %s(%d) - iio_device, iRet = %d\n", info_table[i].name, i, iRet);
		}
	}

	return iRet;
}
