/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.c : Main functions
 *
 *
 * Version : 2015.05.20
 *
 */

#include "mip4.h"

#if MIP_USE_WAKEUP_GESTURE
struct wake_lock mip_wake_lock;
#endif

int mip_i2c_read(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = read_len,
		},
	};

	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			goto DONE;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}
		else if(res != ARRAY_SIZE(msg)){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	goto ERROR_REBOOT;
	
ERROR_REBOOT:
	mip_reboot(info);
	return 1;
	
DONE:
	return 0;
}


/**
* I2C Read (Continue)
*/
int mip_i2c_read_next(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, int start_idx, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;
	u8 rbuf[read_len];

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = read_len,
		},
	};
	
	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			goto DONE;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}
		else if(res != ARRAY_SIZE(msg)){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	
	goto ERROR_REBOOT;
	
ERROR_REBOOT:
	mip_reboot(info);
	return 1;

DONE:
	memcpy(&read_buf[start_idx], rbuf, read_len);
	
	return 0;
}

/**
* I2C Write
*/
int mip_i2c_write(struct mip_ts_info *info, char *write_buf, unsigned int write_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	while(retry--){
		res = i2c_master_send(info->client, write_buf, write_len);

		if(res == write_len){
			goto DONE;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		}
		else if(res != write_len){
			dev_err(&info->client->dev, "%s [ERROR] length mismatch - write[%d] result[%d]\n", __func__, write_len, res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	
	goto ERROR_REBOOT;
	
ERROR_REBOOT:
	mip_reboot(info);
	return 1;
	
DONE:
	return 0;
}

void mip_regulator_control(struct mip_ts_info *info, bool enable)
{
	int ret;
	static struct regulator *tsp_vdd;

	tsp_vdd = regulator_get(&info->client->dev, "tsp_vdd");

	if (IS_ERR(tsp_vdd)) {
		dev_err(&info->client->dev, "regulator_get error!\n");
		return;
	}

	if (enable) {
		if (!regulator_is_enabled(tsp_vdd)) {
			regulator_set_voltage(tsp_vdd, 3000000, 3000000);
			ret = regulator_enable(tsp_vdd);
			if (ret) {
				dev_err(&info->client->dev, "power on error!!\n");
				return;
			}
			dev_info(&info->client->dev, "power on!\n");
		} else
			dev_dbg(&info->client->dev, "already power on!\n");
	} else {
		if (regulator_is_enabled(tsp_vdd)) {
			ret = regulator_disable(tsp_vdd);
			if (ret) {
				dev_err(&info->client->dev, "power off error!\n");
				return;
			}
			dev_info(&info->client->dev, "power off!\n");
		} else
			dev_dbg(&info->client->dev, "already power off!\n");
	}

	regulator_put(tsp_vdd);

	return;
}

void mip_power_off(struct mip_ts_info *info)
{		
	mip_regulator_control(info, false);
	msleep(20);
}

void mip_power_on(struct mip_ts_info *info)
{
	mip_regulator_control(info, true);
	msleep(200);
}

/**
* Enable device
*/
int mip_enable(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	if (info->enabled){
		dev_err(&info->client->dev, "%s [ERROR] device already enabled\n", __func__);
		goto EXIT;
	}

#if MIP_USE_WAKEUP_GESTURE
	mip_set_power_state(info, MIP_CTRL_POWER_ACTIVE);

	if(wake_lock_active(&mip_wake_lock)){
		wake_unlock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_unlock\n", __func__);
	}
	
	info->nap_mode = false;
	dev_dbg(&info->client->dev, "%s - nap mode : off\n", __func__);	
#else	
	mip_power_on(info);
#endif
	if(info->enter_noise_mode == true){
		if(mip_enter_noise_mode(info)){
			info->enter_noise_mode = false;
		}
	}
	
#if 1
	if(info->disable_esd == true){
		//Disable ESD alert
		mip_disable_esd_alert(info);
	}	
#endif

	mutex_lock(&info->lock);

	enable_irq(info->client->irq);
	info->enabled = true;

	mutex_unlock(&info->lock);

#if MIP_USE_CALLBACK
	info->cb.inform_charger(&info->cb, info->charger_state);
#endif	
	
EXIT:
	dev_info(&info->client->dev, MIP_DEVICE_NAME" - Enabled\n");
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Disable device
*/
int mip_disable(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	if (!info->enabled){
		dev_err(&info->client->dev, "%s [ERROR] device already disabled\n", __func__);
		goto EXIT;
	}

#if MIP_USE_WAKEUP_GESTURE
	info->wakeup_gesture_code = 0;

	mip_set_wakeup_gesture_type(info, MIP_EVENT_GESTURE_ALL);
	mip_set_power_state(info, MIP_CTRL_POWER_LOW);
	
	info->nap_mode = true;
	dev_dbg(&info->client->dev, "%s - nap mode : on\n", __func__);

	if(!wake_lock_active(&mip_wake_lock)) {
		wake_lock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_lock\n", __func__);
	}
#else
	mutex_lock(&info->lock);

	disable_irq(info->client->irq);
	info->enabled = false;
		
	mutex_unlock(&info->lock);

	mip_power_off(info);
	
#endif
	
	/* Release all the fingers after the TSP INT disable and power off */
	mip_clear_input(info);

EXIT:	
	dev_info(&info->client->dev, MIP_DEVICE_NAME" - Disabled\n");
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

void mip_reboot(struct mip_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
	
	i2c_lock_adapter(adapter);
	
	mip_power_off(info);
	mip_power_on(info);

	i2c_unlock_adapter(adapter);
}

int mip_get_ready_status(struct mip_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_READY_STATUS;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	//check status
	if((ret == MIP_CTRL_STATUS_NONE) || (ret == MIP_CTRL_STATUS_LOG) || (ret == MIP_CTRL_STATUS_READY)){
		//dev_dbg(&info->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if (ret == MIP_CTRL_STATUS_LOG) {
		wbuf[0] = MIP_R0_LOG;
		wbuf[1] = MIP_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if(mip_i2c_write(info, wbuf, 3)){
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		}
	}
	
	return ret;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Read chip firmware version
*/
int mip_get_fw_version(struct mip_ts_info *info, u8 *ver_buf)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;
	
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 8)){
		goto ERROR;
	};

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf, 0xFF, sizeof(ver_buf));
	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Read chip firmware version for u16
*/
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;
	
	if(mip_get_fw_version(info, rbuf)){
		goto ERROR;
	}

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf_u16, 0xFFFF, sizeof(ver_buf_u16));
	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Set power state
*/
int mip_set_power_state(struct mip_ts_info *info, u8 mode)
{
	u8 wbuf[3];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - mode[%02X]\n", __func__, mode);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Set wake-up gesture type
*/
int mip_set_wakeup_gesture_type(struct mip_ts_info *info, u32 type)
{
	u8 wbuf[6];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - type[%08X]\n", __func__, type);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GESTURE_TYPE;
	wbuf[2] = (type >> 24) & 0xFF;
	wbuf[3] = (type >> 16) & 0xFF;
	wbuf[4] = (type >> 8) & 0xFF;
	wbuf[5] = type & 0xFF;
	if(mip_i2c_write(info, wbuf, 6)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Disable ESD alert
*/
int mip_disable_esd_alert(struct mip_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[4];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}	

	if(rbuf[0] != 1){
		dev_dbg(&info->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Enter Noise Mode
*/
int mip_enter_noise_mode(struct mip_ts_info *info){
	u8 wbuf[4];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	if(info->enter_noise_mode == true){
		wbuf[0] = MIP_ADDR_CONTROL;
		wbuf[1] = MIP_CONTROL_NOISE;
		wbuf[2] = ENTER_NOISE_MODE;
		if(mip_i2c_write(info, wbuf, 3)){
			dev_err(&info->client->dev, "%s [ERROR] Enter Noise Mode\n", __func__); 
			goto ERROR;
		}
	}
	dev_dbg(&info->client->dev, "%s [DONE]Enter Noise Mode\n", __func__);
	return 1;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 0;
}

/**
* Alert event handler - ESD
*/
static int mip_alert_handler_esd(struct mip_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - frame_cnt[%d]\n", __func__, frame_cnt);

	if(frame_cnt == 0){
		//sensor crack, not ESD
		info->esd_cnt++;
		dev_dbg(&info->client->dev, "%s - esd_cnt[%d]\n", __func__, info->esd_cnt);

		if(info->disable_esd == true){
			mip_disable_esd_alert(info);
			info->esd_cnt = 0;
		}
		else if(info->esd_cnt > ESD_COUNT_FOR_DISABLE){
			//Disable ESD alert
			if(mip_disable_esd_alert(info)){
			}
			else{
				info->disable_esd = true;
				info->esd_cnt = 0;
			}
		}
		else{
			//Reset chip
			mip_reboot(info);
		}
	}
	else{
		//ESD detected
		//Reset chip
		mip_reboot(info);
		info->esd_cnt = 0;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:	
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

/**
* Alert event handler - Wake-up
*/
static int mip_alert_handler_wakeup(struct mip_ts_info *info, u8 *rbuf)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(mip_wakeup_event_handler(info, rbuf)){
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - Input type
*/
static int mip_alert_handler_inputtype(struct mip_ts_info *info, u8 *rbuf)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//...
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Alert event handler - Noise Mode
*/
static int mip_alert_handler_noisemode(struct mip_ts_info *info, u8 *rbuf){
	u8 noise_mode = rbuf[1]; /* 0 : Exit Noise Mode, 1 : Enter Noise Mode*/
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(noise_mode == 1){
		dev_dbg(&info->client->dev, "%s noise_mode : %d\n", __func__, noise_mode);
		info->enter_noise_mode = true;
	}
	else{
		dev_dbg(&info->client->dev, "%s noise_mode : %d\n", __func__, noise_mode);
		info->enter_noise_mode = false;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Alert event handler - 0xF1
*/
static int mip_alert_handler_F1(struct mip_ts_info *info, u8 *rbuf, u8 size)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

#if MIP_USE_LPWG
	if(mip_lpwg_event_handler(info, rbuf, size)){
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return 1;
	}
#endif
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

void mip_clear_input(struct mip_ts_info *info)
{
	int i;

	for (i = 0; i < MAX_FINGER_NUM; i++) {
		info->finger_state[i] = false;
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
	}
	info->finger_cnt = 0;

	if (info->key_enable) {
		for(i = 0; i < info->key_num; i++)
			input_report_key(info->input_dev, info->key_code[i], 0);
	}
	input_sync(info->input_dev);

	return;
}

void mip_input_event_handler(struct mip_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	int id, x, y;
	int pressure = 0;
	int size = 0;
	int touch_major = 0;
	int touch_minor = 0;
	int palm = 0;

	for (i = 0; i < sz; i += info->event_size) {
		u8 *tmp = &buf[i];

		//Report input data
		if ((tmp[0] & MIP_EVENT_INPUT_SCREEN) == 0) {
			//Touchkey Event
			int key = tmp[0] & 0xf;
			int key_state = (tmp[0] & MIP_EVENT_INPUT_PRESS) ? 1 : 0;
			int key_code = 0;

			//Report touchkey event
			if((key > 0) && (key <= info->key_num)){
				key_code = info->key_code[key - 1];
				input_report_key(info->input_dev, key_code, key_state);
				dev_dbg(&info->client->dev, "%s - Key : ID[%d] Code[%d] State[%d]\n",
						__func__, key, key_code, key_state);
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown key id [%d]\n",
										__func__, key);
				continue;
			}
		} else { /* Touchscreen Event */
			if (info->event_format == 0) {
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				touch_major = tmp[5];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			} else if(info->event_format == 1) {
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				size = tmp[5];
				touch_major = tmp[6];
				touch_minor = tmp[7];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			} else if(info->event_format == 2) {
				id = (tmp[0] & 0xf) - 1;
				x = tmp[2] | ((tmp[1] & 0xf) << 8);
				y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
				pressure = tmp[4];
				touch_major = tmp[5];
				touch_minor = tmp[6];
				palm = (tmp[0] & MIP_EVENT_INPUT_PALM) >> 4;
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n",
						__func__, info->event_format);
				return;
			}

			if((tmp[0] & MIP_EVENT_INPUT_PRESS) == 0) {
				/* Release */
				info->finger_cnt--;
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
				dev_info(&info->client->dev,
						"Touch : ID[%d] Release. remain: %d\n",
									id, info->finger_cnt);
				info->finger_state[id] = false;
				input_sync(info->input_dev);
				continue;
			}

			/* Press or Move */
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
			dev_dbg(&info->client->dev,
					"Touch : ID[%d] X[%d] Y[%d] P[%d] Major[%d] Minor[%d]\n",
						id, x, y, pressure, touch_major, touch_minor);
			if (!info->finger_state[id]) {
				info->finger_cnt++;
				info->finger_state[id] = true;
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				dev_info(&info->client->dev,
					"Touch : ID[%d] Press. remain: %d\n",
								id, info->finger_cnt);
#else
				dev_info(&info->client->dev,
					"Touch : ID[%d] (%d, %d) Press. remain: %d\n",
								id, x, y, info->finger_cnt);
#endif
			}
		}
	}

	input_sync(info->input_dev);

	return;
}

int mip_wakeup_event_handler(struct mip_ts_info *info, u8 *rbuf)
{
	u8 wbuf[4];
	u8 gesture_code = rbuf[1];

	info->wakeup_gesture_code = gesture_code;

	switch(gesture_code){
		case MIP_EVENT_GESTURE_C:
		case MIP_EVENT_GESTURE_W:
		case MIP_EVENT_GESTURE_V:
		case MIP_EVENT_GESTURE_M:				
		case MIP_EVENT_GESTURE_S:				
		case MIP_EVENT_GESTURE_Z:				
		case MIP_EVENT_GESTURE_O:				
		case MIP_EVENT_GESTURE_E:				
		case MIP_EVENT_GESTURE_V_90:
		case MIP_EVENT_GESTURE_V_180:		
		case MIP_EVENT_GESTURE_FLICK_RIGHT:	
		case MIP_EVENT_GESTURE_FLICK_DOWN:	
		case MIP_EVENT_GESTURE_FLICK_LEFT:	
		case MIP_EVENT_GESTURE_FLICK_UP:		
		case MIP_EVENT_GESTURE_DOUBLE_TAP:
			input_report_key(info->input_dev, KEY_POWER, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_POWER, 0);
			input_sync(info->input_dev);
			break;
			
		default:
			wbuf[0] = MIP_R0_CTRL;
			wbuf[1] = MIP_R1_CTRL_POWER_STATE;
			wbuf[2] = MIP_CTRL_POWER_LOW;
			if(mip_i2c_write(info, wbuf, 3)){
				dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
				goto ERROR;
			}	
				
			break;
	}
	
	return 0;
	
ERROR:
	return 1;
}

/**
* Interrupt handler
*/
static irqreturn_t mip_interrupt(int irq, void *dev_id)
{
	struct mip_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 wbuf[8];
	u8 rbuf[256];
	unsigned int size = 0;
	//int event_size = info->event_size;
	u8 category = 0;
	u8 alert_type = 0;
	
	dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	//Read packet info
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto ERROR;
	}

	size = (rbuf[0] & 0x7F);	
	category = ((rbuf[0] >> 7) & 0x1);
	dev_dbg(&client->dev, "%s - packet info : size[%d] category[%d]\n", __func__, size, category);	
	
	//Check size
	if(size <= 0){
		dev_err(&client->dev, "%s [ERROR] Packet size [%d]\n", __func__, size);
		goto EXIT;
	}

	//Read packet data
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_DATA;
	if(mip_i2c_read(info, wbuf, 2, rbuf, size)){
		dev_err(&client->dev, "%s [ERROR] Read packet data\n", __func__);
		goto ERROR;
	}

	//Event handler
	if(category == 0){
		//Touch event
		info->esd_cnt = 0;
		
		mip_input_event_handler(info, size, rbuf);
	}
	else{
		//Alert event
		alert_type = rbuf[0];
		
		dev_dbg(&client->dev, "%s - alert type [%d]\n", __func__, alert_type);
				
		if(alert_type == MIP_ALERT_ESD){
			//ESD detection
			if(mip_alert_handler_esd(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_WAKEUP){
			//Wake-up gesture
			if(mip_alert_handler_wakeup(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_INPUTTYPE){
			//Input type
			if(mip_alert_handler_inputtype(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_NOISE_ALERT){
			if(mip_alert_handler_noisemode(info, rbuf)){
				goto ERROR;
			}
			
		}
		else if(alert_type == MIP_ALERT_F1){
			//0xF1
			if(mip_alert_handler_F1(info, rbuf, size)){
				goto ERROR;
			}
		}
		else{
			dev_err(&client->dev, "%s [ERROR] Unknown alert type [%d]\n", __func__, alert_type);
			goto ERROR;
		}		
	}

EXIT:
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;
	
ERROR:
	if(RESET_ON_EVENT_ERROR){	
		dev_info(&client->dev, "%s - Reset on error\n", __func__);
		
		mip_disable(info);
		mip_clear_input(info);
		mip_enable(info);
	}

	dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return IRQ_HANDLED;
}

/**
* Update firmware from kernel built-in binary
*/
int mip_fw_update_from_kernel(struct mip_ts_info *info, bool force)
{
	const struct firmware *fw;
	int retires = 3;
	int ret = fw_err_none;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ	
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	//Get firmware
	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);
	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	//Update fw
	do {
		ret = mip_flash_fw(info, fw->data, fw->size, force, true);
		if(ret >= fw_err_none){
			break;
		}
	} while (--retires);

	if (!retires) {
		dev_err(&info->client->dev, "%s [ERROR] mip_flash_fw failed\n", __func__);
		ret = fw_err_download;
	}

	release_firmware(fw);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if(ret < fw_err_none){
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Update firmware from external storage
*/
int mip_fw_update_from_storage(struct mip_ts_info *info, bool force)
{
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	//Get firmware
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	//fp = filp_open(EXTERNAL_FW_PATH, O_RDONLY, S_IRUSR);
	fp = filp_open(info->pdata->ext_fw_name, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev, "%s [ERROR] file_open\n", __func__);
		ret = fw_err_file_open;
		goto ERROR;
	}
	
 	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		//Read firmware
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		dev_dbg(&info->client->dev, "%s - size[%u]\n", __func__, fw_size);
		
		if (nread != fw_size) {
			dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%d] read[%d]\n", __func__, fw_size, nread);
			ret = fw_err_file_read;
		}
		else{
			//Update firmware
			ret = mip_flash_fw(info, fw_data, fw_size, force, true);
		}
		
		kfree(fw_data);
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR] fw_size [%d]\n", __func__, fw_size);
		ret = fw_err_file_read;
	}
	
 	filp_close(fp, current->files);

ERROR:
	set_fs(old_fs);	

	//Enable IRQ
	enable_irq(info->client->irq);	
	mutex_unlock(&info->lock);

	if(ret == 0){
		dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	}
	
	return ret;
}

#if MIP_USE_CALLBACK
void mip_callback_charger(struct mip_callbacks *cb, int charger_status)
{
	struct mip_ts_info *info = container_of(cb, struct mip_ts_info, cb);
	u8 wbuf[3];
	u8 rbuf[1];
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;

	if (mip_i2c_read(info, wbuf, 2, rbuf, 1))
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);

	if (rbuf[0] != charger_status) {
		wbuf[2] = charger_status;

		if (mip_i2c_write(info, wbuf, 3))
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		dev_info(&info->client->dev, "%s - change charger_status[%d]\n", __func__, charger_status);
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

int mip_callback_notifier(struct notifier_block *nb, unsigned long noti_type, void *v )
{
	struct mip_callbacks *cb = container_of(nb, struct mip_callbacks, nb);
	struct mip_ts_info *info = container_of(cb, struct mip_ts_info, cb);

	if (noti_type == MUIC_VBUS_NOTI) {
		info->charger_state = *(int *)v;

	} else {
		dev_err(&info->client->dev, "%s [faile]\n", __func__);
		return 0;
	}

	if (info->enabled) {
		cb->inform_charger(cb, info->charger_state);
	} else {
		dev_err(&info->client->dev, "not enabled error\n", __func__);
		return 0;
	}

	return 1;
}

void mip_config_callback(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	info->cb.nb.notifier_call = mip_callback_notifier;
	register_muic_notifier(&info->cb.nb);
	info->cb.inform_charger= mip_callback_charger;

	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return;
}
#endif

/**
* Initial config
*/
static int mip_init_config(struct mip_ts_info *info)
{
	u8 wbuf[8];
	u8 rbuf[64];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Product name
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mip_i2c_read(info, wbuf, 2, rbuf, 16);
	memcpy(info->product_name, rbuf, 16);
	dev_dbg(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);

	//Firmware version
	mip_get_fw_version(info, rbuf);
	memcpy(info->fw_version, rbuf, 8);
	dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", __func__, info->fw_version[0], info->fw_version[1], info->fw_version[2], info->fw_version[3], info->fw_version[4], info->fw_version[5], info->fw_version[6], info->fw_version[7]);	

	//Resolution
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);

#if MIP_AUTOSET_RESOLUTION
	//Set resolution using chip info
	info->max_x = (rbuf[0]) | (rbuf[1] << 8);
	info->max_y = (rbuf[2]) | (rbuf[3] << 8);
#else
	//Set resolution using platform data
	info->max_x = info->pdata->max_x;
	info->max_y = info->pdata->max_y;
#endif

	dev_dbg(&info->client->dev, "%s - max_x[%d] max_y[%d]\n", __func__, info->max_x, info->max_y);

	//Node info
	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	dev_dbg(&info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n", __func__, info->node_x, info->node_y, info->node_key);

	//Key info
	info->node_key = 0;
	if(info->node_key > 0){
		//Enable touchkey
		info->key_enable = true;
		info->key_num = info->node_key;
	}

	//Protocol
#if MIP_AUTOSET_EVENT_FORMAT
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_SUPPORTED_FUNC;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);
	info->event_format = (rbuf[4]) | (rbuf[5] << 8);
	info->event_size = rbuf[6];
#else
	info->event_format = 0;
	info->event_size = 6;
#endif
	dev_dbg(&info->client->dev, "%s - event_format[%d] event_size[%d] \n", __func__, info->event_format, info->event_size);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
//ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

#if MIP_USE_DEVICETREE
int mip_parse_devicetree(struct device *dev, struct mip_ts_info *info)
{
	struct device_node *np = dev->of_node;
	int ret;
	u32 val;
	
	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_x", &val);
	if (ret) {
		dev_err(dev, "%s [ERROR] max_x\n", __func__);
		info->pdata->max_x = 1080;
	} else
		info->pdata->max_x = val;

	ret = of_property_read_u32(np, MIP_DEVICE_NAME",max_y", &val);
	if (ret) {
		dev_err(dev, "%s [ERROR] max_y\n", __func__);
		info->pdata->max_y = 1920;
	} else
		info->pdata->max_y = val;
	
	
	info->pdata->gpio_intr = of_get_named_gpio(np, MIP_DEVICE_NAME",irq-gpio", 0);
	if (info->pdata->gpio_intr < 0) {
		dev_err(dev, "%s [ERROR] of_get_named_gpio : gpio_irq\n", __func__);
		return -EINVAL;
	}

	ret = gpio_request(info->pdata->gpio_intr, "gpio_irq");
	if (ret < 0) {
		dev_err(dev, "%s [ERROR] gpio_request : gpio_irq\n", __func__);
		return -EINVAL;
	}	
	gpio_direction_input(info->pdata->gpio_intr);	
	info->client->irq = gpio_to_irq(info->pdata->gpio_intr); 
	
	ret = of_property_read_string(np, MIP_DEVICE_NAME",fw_name", &info->pdata->fw_name);
	if (ret < 0) {
		dev_err(dev, "%s: failed to get built-in firmware path!\n", __func__);
		return -EINVAL;
	}
	
	ret = of_property_read_string(np, MIP_DEVICE_NAME",ext_fw_name", &info->pdata->ext_fw_name);
	if (ret < 0) {
		dev_err(dev, "%s: failed to get external firmware path!\n", __func__);
		return -EINVAL;
	}

	return 0;
}
#endif

static int mip_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mip_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s [ERROR] i2c_check_functionality\n", __func__);
		return -EIO;
	}

	info = kzalloc(sizeof(struct mip_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "failed to allocate info data!\n");
		return -ENOMEM;
	}
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input_dev\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->irq = -1;

#if MIP_USE_CALLBACK
	info->charger_state = 0;
#endif

	mutex_init(&info->lock);

#if MIP_USE_DEVICETREE
	if (client->dev.of_node) {
		info->pdata  = devm_kzalloc(&client->dev,
					sizeof(struct melfas_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			dev_err(&client->dev, "failed to allocate pdata\n");
			ret = -ENOMEM;
			goto err_pdata_alloc;
		}
		ret = mip_parse_devicetree(&client->dev, info);
		if (ret){
			dev_err(&client->dev, "failed to parse_dt\n");
			ret = -EINVAL;
			goto err_parse_dt;
		}
	} else
#endif
	{
		info->pdata = client->dev.platform_data;
		if (!info->pdata) {
			dev_err(&client->dev, "no platform data\n");
			ret = -EINVAL;
			goto err_get_pdata;
		}
	}

	info->input_dev->name = "sec_touchscreen";
	snprintf(info->phys, sizeof(info->phys), "%s/input1", info->input_dev->name);

	info->input_dev->phys = info->phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &client->dev;

	mip_power_on(info);

		//Firmware update
#if MIP_USE_AUTO_FW_UPDATE
		ret = mip_fw_update_from_kernel(info, false);
		if(ret){
			dev_err(&client->dev, "%s [ERROR] mip_fw_update_from_kernel\n", __func__);
			goto err_fw_update;
		}
#endif

	mip_init_config(info);

	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);

#if MIP_USE_WAKEUP_GESTURE
	set_bit(KEY_POWER, input_dev->keybit);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input device\n");
		ret = -EIO;
		goto err_reg_input;
	}

#if MIP_USE_CALLBACK
	mip_config_callback(info);
#endif

	ret = request_threaded_irq(client->irq, NULL, mip_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					MIP_DEVICE_NAME, info);
	if (ret) {
		dev_err(&client->dev, "failed to request irq handler\n");
		goto err_req_irq;
	}

	disable_irq(client->irq);
	sprd_i2c_ctl_chg_clk(1, 400000); // up h/w i2c 1 400k
	info->irq = client->irq;

#if MIP_USE_WAKEUP_GESTURE
	wake_lock_init(&mip_wake_lock, WAKE_LOCK_SUSPEND, "mip_wake_lock");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +1;
	info->early_suspend.suspend = mip_early_suspend;
	info->early_suspend.resume = mip_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	mip_enable(info);

#if MIP_USE_DEV
	if (mip_dev_create(info)) {
		dev_err(&client->dev, "failed to create mip_dev_create\n");
		ret = -EAGAIN;
		goto err_dev_create;
	}

	info->class = class_create(THIS_MODULE, MIP_DEVICE_NAME);
	device_create(info->class, NULL, info->mip_dev, NULL, MIP_DEVICE_NAME);
#endif

#if MIP_USE_SYS
	if (mip_sysfs_create(info)) {
		dev_err(&client->dev, "failed to create mip_sysfs_create\n");
		ret = -EAGAIN;
		goto err_sysfs_create;
	}
#endif

#if MIP_USE_CMD
	if (mip_sysfs_cmd_create(info)) {
		dev_err(&client->dev, "failed to create mip_sysfs_cmd_create\n");
		ret = -EAGAIN;
		goto err_cmd_create;
	}
#endif

	if (sysfs_create_link(NULL, &client->dev.kobj, MIP_DEVICE_NAME)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		ret = -EAGAIN;
		goto err_create_link;
	}

	dev_info(&client->dev, "MELFAS " CHIP_NAME " Touchscreen is initialized successfully.\n");
	return 0;

err_create_link:
#if MIP_USE_CMD
err_cmd_create:
#endif
#if MIP_USE_SYS
err_sysfs_create:
#endif
#if MIP_USE_DEV
err_dev_create:
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif
	free_irq(client->irq, info);
err_req_irq:
	input_unregister_device(input_dev);
err_reg_input:
err_fw_update:
err_get_pdata:
#if MIP_USE_DEVICETREE
err_parse_dt:
err_pdata_alloc:
#endif
	input_free_device(input_dev);
	input_dev = NULL;
err_input_alloc:
	kfree(info);

	dev_err(&client->dev, "MELFAS " CHIP_NAME " Touchscreen initialization failed.\n");
	return ret;
}

static int mip_remove(struct i2c_client *client)
{
	struct mip_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0)
		free_irq(info->irq, info);

#if MIP_USE_CMD
	mip_sysfs_cmd_remove(info);
#endif

#if MIP_USE_SYS
	mip_sysfs_remove(info);
#endif

	sysfs_remove_link(NULL, MIP_DEVICE_NAME);
	if(info->print_buf != NULL) 
		kfree(info->print_buf);

#if MIP_USE_DEV
	device_destroy(info->class, info->mip_dev);
	class_destroy(info->class);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	input_unregister_device(info->input_dev);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
int mip_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);

	mip_disable(info);

	return 0;

}

int mip_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);

	mip_enable(info);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void mip_early_suspend(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	mip_suspend(&info->client->dev);
}

void mip_late_resume(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	mip_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
const struct dev_pm_ops mip_pm_ops = {
#if 0
	SET_SYSTEM_SLEEP_PM_OPS(mip_suspend, mip_resume)
#else
	.suspend	= mip_suspend,
	.resume = mip_resume,
#endif
};
#endif

#if MIP_USE_DEVICETREE
static const struct of_device_id mip_match_table[] = {
	{ .compatible = "melfas,"MIP_DEVICE_NAME,},
	{},
};
MODULE_DEVICE_TABLE(of, mip_match_table);
#endif

static const struct i2c_device_id mip_id[] = {
	{MIP_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mip_id);

static struct i2c_driver mip_driver = {
	.id_table = mip_id,
	.probe = mip_probe,
	.remove = mip_remove,
	.driver = {
		.name = MIP_DEVICE_NAME,
		.owner = THIS_MODULE,
#if MIP_USE_DEVICETREE
		.of_match_table = mip_match_table,
#endif
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &mip_pm_ops,
#endif
	},
};

static u32 boot_panel_id;
static int get_panel_id(char *str)
{
	char *endp;

	if ((str[0] == 'I') && (str[1] == 'D'))
		sscanf(&str[2], "%x", &boot_panel_id);
	else
		boot_panel_id = memparse(str, &endp);

	return 0;
}
early_param("lcd_id", get_panel_id);

static int __init mip_init(void)
{
	if (!boot_panel_id) {
		pr_info("mip4_ts: panel_id %d\n", boot_panel_id);
		return -1;
	}

	return i2c_add_driver(&mip_driver);
}

static void __exit mip_exit(void)
{
	i2c_del_driver(&mip_driver);
}

module_init(mip_init);
module_exit(mip_exit);

MODULE_DESCRIPTION("MELFAS MIP4 Touchscreen");
MODULE_VERSION("2015.05.20");
MODULE_AUTHOR("Jee, SangWon <jeesw@melfas.com>");
MODULE_LICENSE("GPL");
