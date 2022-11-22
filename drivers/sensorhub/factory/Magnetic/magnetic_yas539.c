/*
 *  Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/slab.h>
#include <linux/delay.h>
#include "../../ssp.h"
#include "../ssp_factory.h"
#include "../../ssp_cmd_define.h"
#include "../../ssp_comm.h"
#include "../../ssp_data.h"
#include "../../ssp_sysfs.h"
/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define GM_DATA_SPEC_MIN    -6500
#define GM_DATA_SPEC_MAX    6500

#define GM_SELFTEST_X_SPEC_MIN  -200
#define GM_SELFTEST_X_SPEC_MAX  200
#define GM_SELFTEST_Y_SPEC_MIN  -200
#define GM_SELFTEST_Y_SPEC_MAX  200
#define GM_SELFTEST_Z_SPEC_MIN  -1000
#define GM_SELFTEST_Z_SPEC_MAX  -200

#define YAS_STATIC_ELLIPSOID_MATRIX     {10000, 0, 0, 0, 10000, 0, 0, 0, 10000}
#define MAG_HW_OFFSET_FILE_PATH "/efs/FactoryApp/hw_offset"


int check_adc_data_spec(struct ssp_data *data, int sensortype)
{
	int data_spec_max = 0;
	int data_spec_min = 0;

	data_spec_max = GM_DATA_SPEC_MAX;
	data_spec_min = GM_DATA_SPEC_MIN;

	if ((data->buf[sensortype].x == 0) &&
	    (data->buf[sensortype].y == 0) &&
	    (data->buf[sensortype].z == 0)) {
		return FAIL;
	} else if ((data->buf[sensortype].x > data_spec_max)
	           || (data->buf[sensortype].x < data_spec_min)
	           || (data->buf[sensortype].y > data_spec_max)
	           || (data->buf[sensortype].y < data_spec_min)
	           || (data->buf[sensortype].z > data_spec_max)
	           || (data->buf[sensortype].z < data_spec_min)) {
		return FAIL;
	} else {
		return SUCCESS;
	}
}

ssize_t get_magnetic_yas539_name(char *buf)
{
	return sprintf(buf, "%s\n", "YAS539");
}

ssize_t get_magnetic_yas539_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "YAMAHA");
}

ssize_t get_magnetic_yas539_adc(struct ssp_data *data, char *buf)
{
	bool bSuccess = false;
	s16 sensor_buf[3] = {0, };
	int retries = 10;
	
	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].x = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].y = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].z = 0;

	if (!(atomic64_read(&data->sensor_en_state) & (1ULL <<
	                                             SENSOR_TYPE_GEOMAGNETIC_FIELD)))
	{
		set_delay_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_FIELD, 20, 0);
		enable_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_FIELD);
	}
	
	do {
		msleep(60);
		if (check_adc_data_spec(data, SENSOR_TYPE_GEOMAGNETIC_FIELD) == SUCCESS) {
			break;
		}
	} while (--retries);

	if (retries > 0) {
		bSuccess = true;
	}

	sensor_buf[0] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].x;
	sensor_buf[1] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].y;
	sensor_buf[2] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].z;


	if (!(atomic64_read(&data->sensor_en_state) & (1ULL <<
	                                             SENSOR_TYPE_GEOMAGNETIC_FIELD)))
		disable_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_FIELD);

	pr_info("[SSP] %s - x = %d, y = %d, z = %d\n", __func__,
	        sensor_buf[0], sensor_buf[1], sensor_buf[2]);

	return sprintf(buf, "%s,%d,%d,%d\n", (bSuccess ? "OK" : "NG"),
	               sensor_buf[0], sensor_buf[1], sensor_buf[2]);
}

ssize_t get_magnetic_yas539_dac(struct ssp_data *data, char *buf)
{
	bool bSuccess = false;
	char *buffer = NULL;
	int buffer_length = 0;
	int ret;

	if (!data->geomag_cntl_regdata) {
		bSuccess = true;
	} else {
		pr_info("[SSP] %s - check cntl register before selftest",
		        __func__);
		ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD,
		                       SENSOR_FACTORY, 1000, NULL, 0, &buffer, &buffer_length);

		if (ret != SUCCESS) {
			ssp_errf("ssp_send_command Fail %d", ret);
			return ret;
		}

		if (buffer == NULL) {
			ssp_errf("buffer is null");
			return ret;
		}

		if (buffer_length < 22) {
			ssp_errf("buffer length error %d", buffer_length);
			if (buffer != NULL) {
				kfree(buffer);
			}
			return ret;
		}
		data->geomag_cntl_regdata = buffer[21];
		bSuccess = !data->geomag_cntl_regdata;
	}

	pr_info("[SSP] %s - CTRL : 0x%x\n", __func__,
	        data->geomag_cntl_regdata);

	data->geomag_cntl_regdata = 1;      /* reset the value */

	ret = sprintf(buf, "%s,%d,%d,%d\n",
	              (bSuccess ? "OK" : "NG"), (bSuccess ? 1 : 0), 0, 0);

	if (buffer != NULL) {
		kfree(buffer);
	}

	return ret;
}

ssize_t get_magnetic_yas539_raw_data(struct ssp_data *data, char *buf)
{

	pr_info("[SSP] %s - %d,%d,%d\n", __func__,
	        data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x,
	        data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y,
	        data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z);

	if (data->is_geomag_raw_enabled == false) {
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x = -1;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y = -1;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z = -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
	                data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x,
	                data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y,
	                data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z);
}

ssize_t set_magnetic_yas539_raw_data(struct ssp_data *data, const char *buf)
{
	char chTempbuf[8] = { 0, };
	int ret;
	int64_t dEnable;
	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0) {
		return ret;
	}

	if (dEnable) {
		int retries = 50;

		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x = 0;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y = 0;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z = 0;

		set_delay_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_POWER, 20, 0);
		enable_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_POWER);

		do {
			msleep(20);
			if (check_adc_data_spec(data, SENSOR_TYPE_GEOMAGNETIC_POWER) == SUCCESS) {
				break;
			}
		} while (--retries);

		if (retries > 0) {
			pr_info("[SSP] %s - success, %d\n", __func__, retries);
			data->is_geomag_raw_enabled = true;
		} else {
			pr_err("[SSP] %s - wait timeout, %d\n", __func__,
			       retries);
			data->is_geomag_raw_enabled = false;
		}


	} else {
		disable_legacy_sensor(data, SENSOR_TYPE_GEOMAGNETIC_POWER);
		data->is_geomag_raw_enabled = false;
	}

	return ret;
}

ssize_t get_magnetic_yas539_asa(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%d,%d,%d\n", (s16)data->uFuseRomData[0],
	               (s16)data->uFuseRomData[1], (s16)data->uFuseRomData[2]);
}

ssize_t get_magnetic_yas539_status(struct ssp_data *data, char *buf)
{
	bool bSuccess;

	if ((data->uFuseRomData[0] == 0) ||
	    (data->uFuseRomData[0] == 0xff) ||
	    (data->uFuseRomData[1] == 0) ||
	    (data->uFuseRomData[1] == 0xff) ||
	    (data->uFuseRomData[2] == 0) ||
	    (data->uFuseRomData[2] == 0xff)) {
		bSuccess = false;
	} else {
		bSuccess = true;
	}

	return sprintf(buf, "%s,%u\n", (bSuccess ? "OK" : "NG"), bSuccess);
}


ssize_t get_magnetic_yas539_logging_data(struct ssp_data *data, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	int ret = 0;
	int logging_data[8] = {0, };

	ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD,
	                       SENSOR_FACTORY, 1000, NULL, 0, &buffer, &buffer_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");

		if (buffer != NULL) {
			kfree(buffer);
		}

		return ret;
	}

	if (buffer == NULL) {
		ssp_errf("buffer is null");
		return -EINVAL;
	}

	if (buffer_length != 14) {
		ssp_errf("buffer length error %d", buffer_length);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		if (buffer != NULL) {
			kfree(buffer);
		}
		return -EINVAL;
	}

	logging_data[0] = buffer[0];    /* ST1 Reg */
	logging_data[1] = (short)((buffer[3] << 8) + buffer[2]);
	logging_data[2] = (short)((buffer[5] << 8) + buffer[4]);
	logging_data[3] = (short)((buffer[7] << 8) + buffer[6]);
	logging_data[4] = buffer[1];    /* ST2 Reg */
	logging_data[5] = (short)((buffer[9] << 8) + buffer[8]);
	logging_data[6] = (short)((buffer[11] << 8) + buffer[10]);
	logging_data[7] = (short)((buffer[13] << 8) + buffer[12]);

	ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
	               logging_data[0], logging_data[1],
	               logging_data[2], logging_data[3],
	               logging_data[4], logging_data[5],
	               logging_data[6], logging_data[7],
	               data->uFuseRomData[0], data->uFuseRomData[1],
	               data->uFuseRomData[2]);

	kfree(buffer);

	return ret;
}

ssize_t get_magnetic_yas539_matrix(struct ssp_data *data, char *buf)
{

	return sprintf(buf,
	               "%d %d %d %d %d %d %d %d %d\n",
	               data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2],
	               data->pdc_matrix[3], data->pdc_matrix[4],
	               data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7],
	               data->pdc_matrix[8]);
}

ssize_t set_magnetic_yas539_matrix(struct ssp_data *data, const char *buf)
{
	s16 val[PDC_SIZE] = {0, };
	int ret;
	int i;
	char *token;
	char *str;
	str = (char *)buf;

	for (i = 0; i < PDC_SIZE; i++) {
		token = strsep(&str, " \n");
		if (token == NULL) {
			pr_err("[SSP] %s : too few arguments (%d needed)", __func__, PDC_SIZE);
			return -EINVAL;
		}

		ret = kstrtos16(token, 10, &val[i]);
		if (ret < 0) {
			pr_err("[SSP] %s : kstros16 error %d", __func__, ret);
			return ret;
		}
	}

	for (i = 0; i < PDC_SIZE; i++) {
		data->pdc_matrix[i] = val[i];
	}

	ssp_infof("sizeof pdc %d %d", sizeof(data->pdc_matrix), sizeof(data->pdc_matrix[0])*PDC_SIZE);

	pr_info("[SSP] %s : %d %d %d %d %d %d %d %d %d\n",
	        __func__, data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2],
	        data->pdc_matrix[3], data->pdc_matrix[4],
	        data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7],
	        data->pdc_matrix[8]);
	set_pdc_matrix(data);

	return ret;
}

ssize_t get_magnetic_yas539_selftest(struct ssp_data *data, char *buf)
{
	char *buf_selftest = NULL;
	int buf_selftest_length = 0;

	int ret = 0;
	s8 id = 0, x = 0, y1 = 0, y2 = 0, dir = 0;
	s16 sx = 0, sy1 = 0, sy2 = 0, ohx = 0, ohy = 0, ohz = 0;
	s8 err[7] = {-1, };

	pr_info("[SSP] %s in\n", __func__);

Retry_selftest:
	ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD,
	                       SENSOR_FACTORY, 1000, NULL, 0, &buf_selftest, &buf_selftest_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	if (buf_selftest == NULL) {
		ssp_errf("buffer is null");
		goto exit;
	}

	if (buf_selftest_length < 24) {
		ssp_errf("buffer length error %d", buf_selftest_length);
		goto exit;
	}

	id = (s8)(buf_selftest[0]);
	err[0] = (s8)(buf_selftest[1]);
	err[1] = (s8)(buf_selftest[2]);
	err[2] = (s8)(buf_selftest[3]);
	x = (s8)(buf_selftest[4]);
	y1 = (s8)(buf_selftest[5]);
	y2 = (s8)(buf_selftest[6]);
	err[3] = (s8)(buf_selftest[7]);
	dir = (s8)(buf_selftest[8]);
	err[4] = (s8)(buf_selftest[9]);
	ohx = (s16)((buf_selftest[10] << 8) + buf_selftest[11]);
	ohy = (s16)((buf_selftest[12] << 8) + buf_selftest[13]);
	ohz = (s16)((buf_selftest[14] << 8) + buf_selftest[15]);
	err[6] = (s8)(buf_selftest[16]);
	sx = (s16)((buf_selftest[17] << 8) + buf_selftest[18]);
	sy1 = (s16)((buf_selftest[19] << 8) + buf_selftest[20]);
	sy2 = (s16)((buf_selftest[21] << 8) + buf_selftest[22]);
	err[5] = (s8)(buf_selftest[23]);

	if (unlikely(id != 0x8)) {
		err[0] = -1;
	}
	if (unlikely(x < -30 || x > 30)) {
		err[3] = -1;
	}
	if (unlikely(y1 < -30 || y1 > 30)) {
		err[3] = -1;
	}
	if (unlikely(y2 < -30 || y2 > 30)) {
		err[3] = -1;
	}
	if (unlikely(sx < 16544 || sx > 17024)) {
		err[5] = -1;
	}
	if (unlikely(sy1 < 16517 || sy1 > 17184)) {
		err[5] = -1;
	}
	if (unlikely(sy2 < 15584 || sy2 > 16251)) {
		err[5] = -1;
	}
	if (unlikely(ohx < -1000 || ohx > 1000)) {
		err[6] = -1;
	}
	if (unlikely(ohy < -1000 || ohy > 1000)) {
		err[6] = -1;
	}
	if (unlikely(ohz < -1000 || ohz > 1000)) {
		err[6] = -1;
	}

	pr_info("[SSP] %s\n"
	        "[SSP] Test1 - err = %d, id = %d\n"
	        "[SSP] Test3 - err = %d\n"
	        "[SSP] Test4 - err = %d, offset = %d,%d,%d\n"
	        "[SSP] Test5 - err = %d, direction = %d\n"
	        "[SSP] Test6 - err = %d, sensitivity = %d,%d,%d\n"
	        "[SSP] Test7 - err = %d, offset = %d,%d,%d\n"
	        "[SSP] Test2 - err = %d\n",
	        __func__, err[0], id, err[2], err[3], x, y1, y2, err[4], dir,
	        err[5], sx, sy1, sy2, err[6], ohx, ohy, ohz, err[1]);

exit:
	return sprintf(buf,
	               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
	               err[0], id, err[2], err[3], x, y1, y2, err[4], dir,
	               err[5], sx, sy1, sy2, err[6], ohx, ohy, ohz, err[1]);
}


struct magnetic_sensor_operations magnetic_yas539_ops = {
	.get_magnetic_name = get_magnetic_yas539_name,
	.get_magnetic_vendor = get_magnetic_yas539_vendor,
	.get_magnetic_adc = get_magnetic_yas539_adc,
	.get_magnetic_dac = get_magnetic_yas539_dac,
	.get_magnetic_raw_data = get_magnetic_yas539_raw_data,
	.set_magnetic_raw_data = set_magnetic_yas539_raw_data,
	.get_magnetic_asa = get_magnetic_yas539_asa,
	.get_magnetic_status = get_magnetic_yas539_status,
	.get_magnetic_logging_data = get_magnetic_yas539_logging_data,
	.get_magnetic_matrix = get_magnetic_yas539_matrix,
	.set_magnetic_matrix = set_magnetic_yas539_matrix,
	.get_magnetic_selftest = get_magnetic_yas539_selftest,
};

struct magnetic_sensor_operations* get_magnetic_yas539_function_pointer(struct ssp_data *data)
{
	return &magnetic_yas539_ops;
}

