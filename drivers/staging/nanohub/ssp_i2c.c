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

#define LIMIT_DELAY_CNT		200
#define RECEIVEBUFFERSIZE	12
#define DEBUG_SHOW_DATA	0

void clean_msg(struct ssp_msg *msg)
{
	if (msg->free_buffer)
		kfree(msg->buffer);
	kfree(msg);
}

static int do_transfer(struct ssp_data *data, struct ssp_msg *msg,
		struct completion *done, int timeout)
{
	//func_dbg();
	return ssp_do_transfer(data, msg, done, timeout);	
}

int ssp_spi_async(struct ssp_data *data, struct ssp_msg *msg)
{
	int status = 0;
	u64 diff = get_current_timestamp() - data->resumeTimestamp;
	int timeout = diff < 5000000000ULL ? 400 : 1000; // unit: ms

	if (msg->length)
		return ssp_spi_sync(data, msg, timeout);

	status = do_transfer(data, msg, NULL, 0);

	return status;
}

int ssp_spi_sync(struct ssp_data *data, struct ssp_msg *msg, int timeout)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status = 0;
	//func_dbg();
	if (msg->length == 0) {
		pr_err("[SSP]: %s length must not be 0\n", __func__);
		clean_msg(msg);
		return status;
	}

	status = do_transfer(data, msg, &done, timeout);

	return status;
}

void clean_pending_list(struct ssp_data *data)
{
	struct ssp_msg *msg, *n;

	mutex_lock(&data->pending_mutex);
	list_for_each_entry_safe(msg, n, &data->pending_list, list)	{
		list_del(&msg->list);
		if (msg->done != NULL && !completion_done(msg->done))
			complete(msg->done);
		if (msg->dead_hook != NULL)
			*(msg->dead_hook) = true;

		clean_msg(msg);
	}
	mutex_unlock(&data->pending_mutex);
}

int ssp_send_cmd(struct ssp_data *data, char command, int arg)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = command;
	msg->length = 0;
	msg->options = AP2HUB_WRITE;
	msg->data = arg;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - command 0x%x failed %d\n",
				__func__, command, iRet);
		return ERROR;
	}

	ssp_dbg("[SSP]: %s - command 0x%x %d\n", __func__, command, arg);

	return SUCCESS;
}

int send_instruction(struct ssp_data *data, u8 uInst,
	u8 uSensorType, u8 *uSendBuf, u16 uLength)
{
	char command;
	int iRet = 0;
	struct ssp_msg *msg;

	u64 timestamp;

	if ((!(data->uSensorState & (1ULL << uSensorType)))
		&& (uInst <= CHANGE_DELAY)) {
		pr_err("[SSP]: %s - Bypass Inst Skip! - %u\n",
			__func__, uSensorType);
		return FAIL;
	}

	if (uSensorType >= SENSOR_MAX && (uInst == ADD_SENSOR || uInst == CHANGE_DELAY)){
		pr_err("[SSP]: %s - Invalid SensorType! - %u\n",
			__func__, uSensorType);
		return FAIL;
	}

	switch (uInst) {
	case REMOVE_SENSOR:
		command = MSG2SSP_INST_BYPASS_SENSOR_REMOVE;
		break;
	case ADD_SENSOR:
		command = MSG2SSP_INST_BYPASS_SENSOR_ADD;
		timestamp = get_current_timestamp();
		data->first_sensor_data[uSensorType] = true;
		break;
	case CHANGE_DELAY:
		command = MSG2SSP_INST_CHANGE_DELAY;
		timestamp = get_current_timestamp();
		data->first_sensor_data[uSensorType] = true;
		break;
	case GO_SLEEP:
		command = MSG2SSP_AP_STATUS_SLEEP;
		data->uLastAPState = MSG2SSP_AP_STATUS_SLEEP;
		break;
	case REMOVE_LIBRARY:
		command = MSG2SSP_INST_LIBRARY_REMOVE;
		break;
	case ADD_LIBRARY:
		command = MSG2SSP_INST_LIBRARY_ADD;
		break;
	default:
		command = uInst;
		break;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = command;
	msg->length = uLength + 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(uLength + 1, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = uSensorType;
	memcpy(&msg->buffer[1], uSendBuf, uLength);

	ssp_dbg("[SSP]: %s - Inst = 0x%x, Sensor Type = 0x%x, data = %u\n",
			__func__, command, uSensorType, msg->buffer[1]);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Instruction CMD Fail %d\n", __func__, iRet);
		return ERROR;
	}

	if (uInst == ADD_SENSOR || uInst == CHANGE_DELAY) {
		unsigned int BatchTimeforReset = 0;
	//current_Ts = get_current_timestamp();
		if (uLength >= 9)
			BatchTimeforReset = *(unsigned int *)(&uSendBuf[4]);// Add / change normal case, not factory.
	//pr_info("[SSP] %s timeForRest %d", __func__, BatchTimeforReset);
		data->IsBypassMode[uSensorType] = (BatchTimeforReset == 0);
	//pr_info("[SSP] sensor%d mode%d Time %lld\n", uSensorType, data->IsBypassMode[uSensorType], current_Ts);
	}
	return iRet;
}

int flush(struct ssp_data *data, u8 uSensorType)
{
	int iRet = 0;
	char buffer = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_MCU_BATCH_FLUSH;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->data = uSensorType;
	msg->buffer = &buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		return ERROR;
	}

	ssp_dbg("[SSP]: %s Sensor Type = 0x%x, data = %u\n",
			__func__, uSensorType, buffer);

	return buffer ? 0 : -1;
}

int get_batch_count(struct ssp_data *data, u8 uSensorType)
{
	int iRet = 0;
	s32 result = 0;
	char buffer[4] = {0, };
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_MCU_BATCH_COUNT;
	msg->length = 4;
	msg->options = AP2HUB_READ;
	msg->data = uSensorType;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		return ERROR;
	}

	memcpy(&result, buffer, 4);

	ssp_dbg("[SSP]: %s Sensor Type = 0x%x, data = %u\n",
			__func__, uSensorType, result);

	return result;
}

int get_chipid(struct ssp_data *data)
{
	int iRet, iReties = 0;
	char buffer = 0;
	struct ssp_msg *msg;
func_dbg();
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_WHOAMI;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (buffer != DEVICE_ID && iReties++ < 2) {
		mdelay(5);
		pr_err("[SSP] %s - get chip ID retry\n", __func__);
		goto retries;
	}

	if (iRet == SUCCESS)
		return buffer;

	pr_err("[SSP] %s - get chip ID failed %d\n", __func__, iRet);
	return ERROR;
}

int set_ap_information(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
				__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_INFORMATION;
	msg->length = 2;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(2, GFP_KERNEL);
	msg->free_buffer = 1;
	msg->buffer[0] = (char)data->ap_type;
	msg->buffer[1] = (char)data->ap_rev;

	iRet = ssp_spi_async(data, msg);

	pr_info("[SSP] AP TYPE = %d AP REV = %d\n", data->ap_type, data->ap_rev);

	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

int set_sensor_position(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_SENSOR_FORMATION;
	msg->length = 3;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(3, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = data->accel_position;
	msg->buffer[1] = data->accel_position;
	msg->buffer[2] = data->mag_position;

	iRet = ssp_spi_async(data, msg);

	pr_info("[SSP] Sensor Posision A : %u, G : %u, M: %u, P: %u\n",
			data->accel_position,
			data->accel_position,
			data->mag_position, 0);

	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

int set_magnetic_static_matrix(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX;
	msg->length = data->mag_matrix_size;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(data->mag_matrix_size, GFP_KERNEL);
	msg->free_buffer = 1;

	memcpy(msg->buffer, data->mag_matrix, data->mag_matrix_size);

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to %s %d\n",
				__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

void set_proximity_threshold(struct ssp_data *data)
{
	int iRet = 0;

	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << PROXIMITY_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!,proximity sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_SENSOR_PROXTHRESHOLD;
	msg->length = 4;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(4, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = ((char) (data->uProxHiThresh >> 8) & 0xff);
	msg->buffer[1] = (char) data->uProxHiThresh;
	msg->buffer[2] = ((char) (data->uProxLoThresh >> 8) & 0xff);
	msg->buffer[3] = (char) data->uProxLoThresh;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_PROXTHRESHOLD CMD fail %d\n",
			__func__, iRet);
		return;
	}

	pr_info("[SSP]: Proximity Threshold - %u, %u\n",
		data->uProxHiThresh, data->uProxLoThresh);
}

void set_proximity_alert_threshold(struct ssp_data *data)
{
	int iRet = 0;

	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << PROXIMITY_ALERT_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!,proximity alert sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_SENSOR_PROX_ALERT_THRESHOLD;
	msg->length = 2;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(2, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = ((char) (data->uProxAlertHiThresh >> 8) & 0xff);
	msg->buffer[1] = (char) data->uProxAlertHiThresh;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_PROX_ALERT_THRESHOLD CMD fail %d\n",
			__func__, iRet);
		return;
	}

	pr_info("[SSP]: %s Proximity alert Threshold - %u\n",
		__func__, data->uProxAlertHiThresh);
}

void set_light_coef(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << LIGHT_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!,light sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_SET_LIGHT_COEF;
	msg->length = sizeof(data->light_coef);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(data->light_coef), GFP_KERNEL);
	msg->free_buffer = 1;

	memcpy(msg->buffer, data->light_coef, sizeof(data->light_coef));

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - MSG2SSP_AP_SET_LIGHT_COEF CMD fail %d\n",
			__func__, iRet);
		return;
	}

	pr_info("[SSP]: %s - %d %d %d %d %d %d %d\n", __func__,
		data->light_coef[0], data->light_coef[1], data->light_coef[2],
		data->light_coef[3], data->light_coef[4], data->light_coef[5], data->light_coef[6]);
}

void set_proximity_barcode_enable(struct ssp_data *data, bool bEnable)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_SENSOR_BARCODE_EMUL;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	data->bBarcodeEnabled = bEnable;
	msg->buffer[0] = bEnable;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_BARCODE_EMUL CMD fail %d\n",
				__func__, iRet);
		return;
	}

	pr_info("[SSP] Proximity Barcode En : %u\n", bEnable);
}

void set_gesture_current(struct ssp_data *data, unsigned char uData1)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_SENSOR_GESTURE_CURRENT;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = uData1;
	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_GESTURE_CURRENT CMD fail %d\n",
			__func__, iRet);
		return;
	}

	pr_info("[SSP]: Gesture Current Setting - %u\n", uData1);
}

u64 get_sensor_scanning_info(struct ssp_data *data)
{
	int iRet = 0, z = 0, cnt = 1;
	u64 result = 0, len = sizeof(data->sensor_state);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
#if defined (CONFIG_SENSORS_SSP_DAVINCI)
	int disable_sensor_type[10] = {7,9,10,24,30,31,32,35,39,40};
#endif
	
	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_SENSOR_SCANNING;
	msg->length = 8;
	msg->options = AP2HUB_READ;
	msg->buffer = (char *) &result;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS)
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);

	memset(data->sensor_state, 0, sizeof(data->sensor_state));
	for (z = 0; z < SENSOR_MAX; z++) {
		if(z % 10 == 0 && z != 0)
			data->sensor_state[len - 1 - cnt++] = ' ';
		data->sensor_state[len - 1 - cnt++] = (result & (1ULL << z)) ? '1' : '0';
	}
	//data->sensor_state[cnt - 1] = '\0';
#if defined (CONFIG_SENSORS_SSP_DAVINCI)
	if (data->ap_rev == 18 &&  data->ap_type <= 1) {
		iRet = 0; len = 0;
		for (int j = 0; j < SENSOR_MAX; j++) {
			if (disable_sensor_type[iRet] == j) {
				iRet++;
				continue;
			}
			if (result & (1ULL << j))
				len |= (1ULL << j);
		}
//		for(int i = 0; i < ARRAY_SIZE(disable_sensor_type); i++) {
//			data->sensor_state[sizeof(data->sensor_state)- (disable_sensor_type[i] + (disable_sensor_type[i] / 10)+1)] = '0'; // divide 10 for blank
//		}
		result = len;
	} // D1 D1x && hw _rev 18 disable prox light sensor type
#endif
	pr_err("[SSP]: state: %s\n", data->sensor_state);

	return result;
}

unsigned int get_firmware_rev(struct ssp_data *data)
{
	int iRet;
	u32 result = SSP_INVALID_REVISION;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_FIRMWARE_REV;
	msg->length = 4;
	msg->options = AP2HUB_READ;
	msg->buffer = (char *) &result;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS)
		pr_err("[SSP]: %s - transfer fail %d\n", __func__, iRet);

	return result;
}

int set_big_data_start(struct ssp_data *data, u8 type, u32 length)
{
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		return iRet;
	}
	msg->cmd = MSG2SSP_AP_START_BIG_DATA;
	msg->length = 5;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(5, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = type;
	memcpy(&msg->buffer[1], &length, 4);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

#if defined(CONFIG_SEC_VIB_NOTIFIER)
int send_motor_state(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_MCU_SET_MOTOR_STATUS;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	/*if 1: start, 0: stop*/
	msg->buffer[0] = data->motor_state;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n",
				__func__, iRet);
		return iRet;
	}
	pr_info("[SSP] %s - En : %u\n", __func__, data->motor_state);
	return data->motor_state;
}
#endif

u8 get_accel_range(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;
	u8 rxbuffer[1] = {0x00};

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_MCU_GET_ACCEL_RANGE;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = rxbuffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n",
				__func__, iRet);
		return iRet;
	}

	pr_info("[SSP] %s - Range : %u\n", __func__, rxbuffer[0]);
	return rxbuffer[0];
}

int set_prox_dynamic_cal_to_ssp(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;
#ifdef CONFIG_SEC_FACTORY
	u32 prox_dynamic_cal = 0;
#else
	u32 prox_dynamic_cal = data->svc_octa_change == true ? 1 : 0;
#endif
	if (prox_dynamic_cal) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_SET_PROX_DYNAMIC_CAL;
		msg->length = sizeof(prox_dynamic_cal);
		msg->options = AP2HUB_WRITE;
		msg->buffer = (u8 *)&prox_dynamic_cal;
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);
	}

	pr_err("[SSP]: %s - prox_dynamic_cal = %d\n", __func__, prox_dynamic_cal);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

int set_prox_call_min_to_ssp(struct ssp_data *data) {
	int iRet = 0;
	struct ssp_msg *msg;
	
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_PROX_CALL_MIN;
	msg->length = sizeof(data->prox_call_min);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&data->prox_call_min;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);
	

	pr_err("[SSP]: %s - prox_call_min = %d\n", __func__, data->prox_call_min);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

int set_factory_binary_flag_to_ssp(struct ssp_data *data) {
#ifdef CONFIG_SEC_FACTORY
	int isFactory = 1;
#else
	int isFactory = 0;
#endif
	int iRet = 0;
	struct ssp_msg *msg;
	
	if(isFactory) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_SET_FACTORY_BINARY_FLAG;
		msg->length = sizeof(isFactory);
		msg->options = AP2HUB_WRITE;
		msg->buffer = (u8 *)&isFactory;
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);
	}

	pr_err("[SSP]: %s - factory binary flag = %d\n", __func__, isFactory);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

int set_flip_cover_detector_status(struct ssp_data *data)
{
	struct ssp_msg *msg;
	int8_t shub_data[5] = {0};
	int iRet = 0;

	memcpy(shub_data, &data->fcd_data.axis_update, sizeof(data->fcd_data.axis_update));
	memcpy(shub_data + 1, &data->fcd_data.threshold_update, sizeof(data->fcd_data.threshold_update));

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_FCD_AXIS_THRES;
	msg->length = sizeof(shub_data);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&shub_data;
	msg->free_buffer = 0;
	iRet = ssp_spi_async(data, msg);

	pr_err("[SSP]  %s: axis=%d, threshold=%d \n", __func__, data->fcd_data.axis_update,
		data->fcd_data.threshold_update);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail to %s %d\n",
			__func__, __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}
