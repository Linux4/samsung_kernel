/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.c : Main functions
 *
 *
 * Version : 2015.12.04
 *
 */

#include "mip4.h"

#if MIP_USE_WAKEUP_GESTURE
struct wake_lock mip_wake_lock;
#endif


/**
* Reboot chip
*
* Caution : IRQ must be disabled before mip_reboot and enabled after mip_reboot.
*/
void mip_reboot(struct mip_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	i2c_lock_adapter(adapter);

	mip_power_off(info);
	mip_power_on(info);

	i2c_unlock_adapter(adapter);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* I2C Read
*/
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

	while (retry--) {
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if (res == ARRAY_SIZE(msg))
			goto DONE;
		else if (res < 0)
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		else if (res != ARRAY_SIZE(msg))
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		else
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
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

	while (retry--) {
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if (res == ARRAY_SIZE(msg))
			goto DONE;
		else if (res < 0)
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		else if (res != ARRAY_SIZE(msg))
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		else
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
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

	while (retry--) {
		res = i2c_master_send(info->client, write_buf, write_len);

		if (res == write_len)
			goto DONE;
		else if (res < 0)
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		else if (res != write_len)
			dev_err(&info->client->dev, "%s [ERROR] length mismatch - write[%d] result[%d]\n", __func__, write_len, res);
		else
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
#if RESET_ON_I2C_ERROR
	mip_reboot(info);
#endif
	return 1;

DONE:
	return 0;
}

#ifdef CONFIG_SEC_FACTORY
int mip_connection_check(struct mip_ts_info *info)
{
	u8 wbuf[2];
	u8 rbuf[16];
	int ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	ret = mip_i2c_read(info, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf));

	if (ret) {
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return -ret;
	}

	dev_info(&info->client->dev, "%s [DONE]\n", __func__);

	return 0;
}
#endif

/**
* Enable device
*/
int mip_enable(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (info->enabled) {
		dev_err(&info->client->dev, "%s [ERROR] device already enabled\n", __func__);
		goto EXIT;
	}

#if MIP_USE_WAKEUP_GESTURE
	mip_set_power_state(info, MIP_CTRL_POWER_ACTIVE);

	if (wake_lock_active(&mip_wake_lock)) {
		wake_unlock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_unlock\n", __func__);
	}

	info->gesture_wakeup_mode = false;
	dev_dbg(&info->client->dev, "%s - gesture wake-up mode : off\n", __func__);
#else
	mip_power_on(info);
#endif

#if 1
	if (info->disable_esd == true)
		mip_disable_esd_alert(info);
#endif

	mutex_lock(&info->lock);

	if (info->irq_enabled == false) {
		enable_irq(info->client->irq);
		info->irq_enabled = true;
	}

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

	if (!info->enabled) {
		dev_err(&info->client->dev, "%s [ERROR] device already disabled\n", __func__);
		goto EXIT;
	}

#if MIP_USE_WAKEUP_GESTURE
	info->wakeup_gesture_code = 0;

	mip_set_wakeup_gesture_type(info, MIP_EVENT_GESTURE_ALL);
	mip_set_power_state(info, MIP_CTRL_POWER_LOW);

	info->gesture_wakeup_mode = true;
	dev_dbg(&info->client->dev, "%s - gesture wake-up mode : on\n", __func__);

	if (!wake_lock_active(&mip_wake_lock)) {
		wake_lock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_lock\n", __func__);
	}
#else
	mutex_lock(&info->lock);

	disable_irq(info->client->irq);
	info->irq_enabled = false;

	info->enabled = false;

	mutex_unlock(&info->lock);

	mip_power_off(info);
#endif

	mip_clear_input(info);

EXIT:
	dev_info(&info->client->dev, MIP_DEVICE_NAME" - Disabled\n");

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

#if MIP_USE_INPUT_OPEN_CLOSE
/**
* Open input device
*/
static int mip_input_open(struct input_dev *dev)
{
	struct mip_ts_info *info = input_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (info->init == true)
		info->init = false;
	else
		mip_enable(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return 0;
}

/**
* Close input device
*/
static void mip_input_close(struct input_dev *dev)
{
	struct mip_ts_info *info = input_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip_disable(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return;
}
#endif

/**
* Get ready status
*/
int mip_get_ready_status(struct mip_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_READY_STATUS;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	if ((ret == MIP_CTRL_STATUS_NONE) || (ret == MIP_CTRL_STATUS_LOG) || (ret == MIP_CTRL_STATUS_READY)) {
		dev_dbg(&info->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if (ret == MIP_CTRL_STATUS_LOG) {
		wbuf[0] = MIP_R0_LOG;
		wbuf[1] = MIP_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if (mip_i2c_write(info, wbuf, 3))
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Read chip vendor id
*/
int mip_get_vendor_id(struct mip_ts_info *info, u8 *ven_buf)
{
	u8 rbuf[2];
	u8 wbuf[2];

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_IC_VENDOR_ID;

	if (mip_i2c_read(info, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf)))
		goto ERROR;

	memcpy(ven_buf, rbuf, sizeof(rbuf));

	return 0;

ERROR:
	memset(ven_buf, 0xFF, sizeof(rbuf));
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return 1;
}

/**
* Read chip hw category
*/
int mip_get_hw_category(struct mip_ts_info *info, u8 *hw_buf)
{
	u8 rbuf[1];
	u8 wbuf[2];

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_IC_HW_CATEGORY;

	if (mip_i2c_read(info, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf)))
		goto ERROR;

	memcpy(hw_buf, rbuf, sizeof(rbuf));

	return 0;

ERROR:
	memset(hw_buf, 0xFF, sizeof(rbuf));
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return 1;
}

/**
* Read chip parameter version
*/
int mip_get_parameter_version(struct mip_ts_info *info, u8 *param_buf)
{
	u8 rbuf[2];
	u8 wbuf[2];

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_PARAM;

	if (mip_i2c_read(info, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf)))
		goto ERROR;

	param_buf[0] = rbuf[1];
	param_buf[1] = rbuf[0];

	return 0;

ERROR:
	memset(param_buf, 0xFF, sizeof(rbuf));
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return 1;
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
	if (mip_i2c_read(info, wbuf, 2, rbuf, 8)) {
		goto ERROR;
	};

	for (i = 0; i < MIP_FW_MAX_SECT_NUM; i++) {
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}

	return 0;

ERROR:
	memset(ver_buf, 0xFF, sizeof(rbuf));

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}
/**
*i2c fail checking point
*/
#ifdef CONFIG_SEC_FACTORY
int mip_check_i2c(struct mip_ts_info *info)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int res;

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	int retry = I2C_RETRY_COUNT;


	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = wbuf,
			.len =  ARRAY_SIZE(wbuf),
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len =  ARRAY_SIZE(rbuf),
		},
	};

	while (retry--) {
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));
		if (res == ARRAY_SIZE(msg))
			return res;
	}
	return res;
}
#endif

/**
* Read chip firmware version for u16
*/
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;

	if (mip_get_fw_version(info, rbuf))
		goto ERROR;

	for (i = 0; i < MIP_FW_MAX_SECT_NUM; i++)
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];

	return 0;

ERROR:
	memset(ver_buf_u16, 0xFFFF, sizeof(rbuf));

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

#if (CHIP_MODEL != CHIP_NONE)
/**
* Read bin(file) firmware version
*/
int mip_get_fw_version_from_bin(struct mip_ts_info *info, u8 *ver_buf)
{
	const struct firmware *fw;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);

	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	if (mip_bin_fw_version(info, fw->data, fw->size, ver_buf)) {
		memset(ver_buf, 0xFF, sizeof(ver_buf));
		dev_err(&info->client->dev, "%s [ERROR] mip_bin_fw_version\n", __func__);
		goto ERROR;
	}

	release_firmware(fw);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}
#endif

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
	if (mip_i2c_write(info, wbuf, 3)) {
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
	if (mip_i2c_write(info, wbuf, 6)) {
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
	if (mip_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}

	if (mip_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}

	if (rbuf[0] != 1) {
		dev_dbg(&info->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - ESD
*/
static int mip_alert_handler_esd(struct mip_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - frame_cnt[%d]\n", __func__, frame_cnt);

	if (frame_cnt == 0) {
		info->esd_cnt++;
		dev_dbg(&info->client->dev, "%s - esd_cnt[%d]\n", __func__, info->esd_cnt);

		if (info->disable_esd == true) {
			mip_disable_esd_alert(info);
			info->esd_cnt = 0;
		} else if (info->esd_cnt > ESD_COUNT_FOR_DISABLE) {
			if (!mip_disable_esd_alert(info)) {
				info->disable_esd = true;
				info->esd_cnt = 0;
			}
		} else {
			mip_reboot(info);
		}
	} else {
		mip_reboot(info);
		info->esd_cnt = 0;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Alert event handler - Wake-up
*/
static int mip_alert_handler_wakeup(struct mip_ts_info *info, u8 *rbuf)
{
	int gesture_code = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (mip_gesture_wakeup_event_handler(info, gesture_code))
		goto ERROR;

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
	u8 input_type = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	switch (input_type) {
	case 0:
		dev_dbg(&info->client->dev, "%s - Input type : Finger\n", __func__);
		break;
	case 1:
		dev_dbg(&info->client->dev, "%s - Input type : Glove\n", __func__);
		break;
	default:
		dev_err(&info->client->dev, "%s - Input type : Unknown [%d]\n", __func__, input_type);
		goto ERROR;
		break;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - Image
*/
static int mip_alert_handler_image(struct mip_ts_info *info, u8 *rbuf)
{
	u8 image_type = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	switch (image_type) {
#if MIP_USE_WAKEUP_GESTURE
	case MIP_IMG_TYPE_GESTURE:
		if (mip_gesture_wakeup_event_handler(info, 1)) {
			dev_err(&info->client->dev, "%s [ERROR] mip_wakeup_event_handler\n", __func__);
			goto ERROR;
		}
		if (mip_get_image(info, rbuf[1])) {
			dev_err(&info->client->dev, "%s [ERROR] mip_get_image\n", __func__);
			goto ERROR;
		}
		break;
#endif
	default:
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - 0xF1
*/
static int mip_alert_handler_F1(struct mip_ts_info *info, u8 *rbuf, u8 size)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

#if MIP_USE_LPWG
	if (mip_lpwg_event_handler(info, rbuf, size)) {
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
		return 1;
	}
#endif

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
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
	u8 category = 0;
	u8 alert_type = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto ERROR;
	}

	size = (rbuf[0] & 0x7F);
	category = ((rbuf[0] >> 7) & 0x1);
	dev_dbg(&client->dev, "%s - packet info : size[%d] category[%d]\n", __func__, size, category);

	if (size <= 0) {
		dev_err(&client->dev, "%s [ERROR] Packet size [%d]\n", __func__, size);
		goto EXIT;
	}

	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_DATA;
	if (mip_i2c_read(info, wbuf, 2, rbuf, size)) {
		dev_err(&client->dev, "%s [ERROR] Read packet data\n", __func__);
		goto ERROR;
	}

	if (category == 0) {
		info->esd_cnt = 0;

		mip_input_event_handler(info, size, rbuf);
	} else {
		alert_type = rbuf[0];

		dev_dbg(&client->dev, "%s - alert type [%d]\n", __func__, alert_type);

		if (alert_type == MIP_ALERT_ESD) {
			if (mip_alert_handler_esd(info, rbuf))
				goto ERROR;
		} else if (alert_type == MIP_ALERT_WAKEUP) {
			if (mip_alert_handler_wakeup(info, rbuf))
				goto ERROR;
		} else if (alert_type == MIP_ALERT_INPUT_TYPE) {
			if (mip_alert_handler_inputtype(info, rbuf))
				goto ERROR;
		} else if (alert_type == MIP_ALERT_IMAGE) {
			if (mip_alert_handler_image(info, rbuf))
				goto ERROR;
		} else if (alert_type == MIP_ALERT_F1) {
			if (mip_alert_handler_F1(info, rbuf, size))
				goto ERROR;
		} else {
			dev_err(&client->dev, "%s [ERROR] Unknown alert type [%d]\n", __func__, alert_type);
			goto ERROR;
		}
	}

EXIT:
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;

ERROR:
	if (RESET_ON_EVENT_ERROR) {
		dev_info(&client->dev, "%s - Reset on error\n", __func__);

		mip_disable(info);
		mip_clear_input(info);
		mip_enable(info);
	}

	dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return IRQ_HANDLED;
}

#if (CHIP_MODEL != CHIP_NONE)
/**
* Update firmware from kernel built-in binary
*/
int mip_fw_update_from_kernel(struct mip_ts_info *info, bool force)
{
	const struct firmware *fw;
	int retires = 3;
	int ret = fw_err_none;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);

	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	do {
		ret = mip_flash_fw(info, fw->data, fw->size, force, true);
		if (ret >= fw_err_none)
			break;
	} while (--retires);

	if (!retires) {
		dev_err(&info->client->dev, "%s [ERROR] mip_flash_fw failed\n", __func__);
		ret = fw_err_download;
	}

	release_firmware(fw);

	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < fw_err_none)
		goto ERROR;

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	mutex_unlock(&info->lock);
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Update firmware from external storage
*/
int mip_fw_update_from_storage(struct mip_ts_info *info, char *path, bool force)
{
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int ret = fw_err_none;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, path);
		ret = fw_err_file_open;
		goto ERROR;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		dev_dbg(&info->client->dev, "%s - path[%s] size[%u]\n", __func__, path, fw_size);

		if (nread != fw_size) {
			dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%d] read[%d]\n", __func__, fw_size, nread);
			ret = fw_err_file_read;
		} else {
			ret = mip_flash_fw(info, fw_data, fw_size, force, true);
		}

		kfree(fw_data);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] fw_size [%d]\n", __func__, fw_size);
		ret = fw_err_file_read;
	}

	filp_close(fp, current->files);

ERROR:
	set_fs(old_fs);

	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < fw_err_none)
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	else
		dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}

static ssize_t mip_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;

	memset(info->print_buf, 0, PAGE_SIZE);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	ret = mip_fw_update_from_storage(info, info->pdata->ext_fw_name, true);

	switch (ret) {
	case fw_err_none:
		sprintf(data, "F/W update success.\n");
		break;
	case fw_err_uptodate:
		sprintf(data, "F/W is already up-to-date.\n");
		break;
	case fw_err_download:
		sprintf(data, "F/W update failed : Download error\n");
		break;
	case fw_err_file_type:
		sprintf(data, "F/W update failed : File type error\n");
		break;
	case fw_err_file_open:
		sprintf(data, "F/W update failed : File open error [%s]\n", info->fw_path_ext);
		break;
	case fw_err_file_read:
		sprintf(data, "F/W update failed : File read error\n");
		break;
	default:
		sprintf(data, "F/W update failed.\n");
		break;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	result = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return result;
}
static DEVICE_ATTR(fw_update, S_IRUGO, mip_sys_fw_update, NULL);
#endif

/**
* Sysfs attr info
*/
static struct attribute *mip_attrs[] = {
#if (CHIP_MODEL != CHIP_NONE)
	&dev_attr_fw_update.attr,
#endif
	NULL,
};

/**
* Sysfs attr group info
*/
static const struct attribute_group mip_attr_group = {
	.attrs = mip_attrs,
};

/**
* Initial config
*/
int mip_init_config(struct mip_ts_info *info)
{
	u8 wbuf[8];
	u8 rbuf[64];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 16)) {
		goto ERROR;
	};

	memcpy(info->product_name, rbuf, 16);
	dev_info(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);

	mip_get_fw_version(info, rbuf);
	memcpy(info->fw_version, rbuf, 8);
	dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", __func__, info->fw_version[0], info->fw_version[1], info->fw_version[2], info->fw_version[3], info->fw_version[4], info->fw_version[5], info->fw_version[6], info->fw_version[7]);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 7)) {
		goto ERROR;
	};

#if MIP_AUTOSET_RESOLUTION
	info->max_x = (rbuf[0]) | (rbuf[1] << 8);
	info->max_y = (rbuf[2]) | (rbuf[3] << 8);
#else
	info->max_x = info->pdata->max_x;
	info->max_y = info->pdata->max_y;
#endif
	dev_info(&info->client->dev, "%s - max_x[%d] max_y[%d]\n", __func__, info->max_x, info->max_y);

	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	dev_info(&info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n", __func__, info->node_x, info->node_y, info->node_key);

	if (info->pdata->key_flag == true) {
		if (info->node_key > 0) {
			info->key_enable = true;
			info->key_num = info->node_key;
		}
	} else {
		pr_info("[TSP] key do not setup\n");
	}

#if MIP_AUTOSET_EVENT_FORMAT
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_SUPPORTED_FUNC;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 7)) {
		goto ERROR;
	};
	info->event_format = (rbuf[4]) | (rbuf[5] << 8);
	info->event_size = rbuf[6];
#else
	info->event_format = 3;
	info->event_size = 11;
#endif
	dev_dbg(&info->client->dev, "%s - event_format[%d] event_size[%d] \n", __func__, info->event_format, info->event_size);
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return 0;
ERROR:
	dev_info(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
	return -1;
}

/**
* Initialize driver
*/
static int mip_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	bool force = false;

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mip_ts_info *info;
	struct input_dev *input_dev;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s [ERROR] i2c_check_functionality\n", __func__);
		ret = -EIO;
		goto ERROR_PATH;
	}

	info = devm_kzalloc(&client->dev, sizeof(struct mip_ts_info), GFP_KERNEL);
	if (unlikely(!info)) {
		dev_err(&client->dev, "%s [ERROR]\n", __func__);
		ret = -ENOMEM;
		goto ERROR_PATH;
	}

	input_dev = devm_input_allocate_device(&client->dev);
	if (unlikely(!input_dev)) {
		dev_err(&client->dev, "%s [ERROR]\n", __func__);
		ret = -ENOMEM;
		goto ERROR_PATH;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->irq = -1;
	info->init = true;
	info->power = -1;
	info->irq_enabled = false;
#if MIP_USE_CALLBACK
	info->charger_state = 0;
#endif

	info->fw_path_ext = kstrdup(FW_PATH_EXTERNAL, GFP_KERNEL);
	if (unlikely(!info->fw_path_ext)) {
		dev_err(&client->dev, "%s [ERROR]\n", __func__);
		ret = -ENOMEM;
		goto ERROR_PATH;
	}

	mutex_init(&info->lock);

	/*sprd_i2c_ctl_chg_clk(1, 100000); // up h/w i2c 1 100k*/

#if MIP_USE_DEVICETREE
	if (client->dev.of_node) {
		info->pdata = devm_kzalloc(&client->dev, sizeof(struct melfas_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			dev_err(&client->dev, "%s [ERROR] pdata devm_kzalloc\n", __func__);
			goto ERROR;
		}

		ret = mip_parse_devicetree(&client->dev, info);
		if (ret) {
			dev_err(&client->dev, "%s [ERROR] mip_parse_dt\n", __func__);
			goto ERROR;
		}
	} else {
		info->pdata = client->dev.platform_data;
		if (info->pdata == NULL) {
			dev_err(&client->dev, "%s [ERROR] pdata is null\n", __func__);
			ret = -EINVAL;
			goto ERROR;
		}
	}
#else
	info->pdata = client->dev.platform_data;
		if (info->pdata == NULL) {
			dev_err(&client->dev, "%s [ERROR] pdata is null\n", __func__);
			ret = -EINVAL;
			goto ERROR;
		}
#endif
	info->input_dev->name = "sec_touchscreen";
	snprintf(info->phys, sizeof(info->phys), "%s/input1", info->input_dev->name);

	info->input_dev->phys = info->phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &client->dev;

#if MIP_USE_INPUT_OPEN_CLOSE
	info->input_dev->open = mip_input_open;
	info->input_dev->close = mip_input_close;
#endif

	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	mip_power_on(info);

#ifdef CONFIG_SEC_FACTORY
	ret = mip_connection_check(info);
	if (ret)
		goto ERROR;
#endif

#if MIP_USE_AUTO_FW_UPDATE
	ret = mip_fw_update_from_kernel(info, force);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] mip_fw_update_from_kernel\n", __func__);
		goto ERROR;
	}
#endif

	ret = mip_init_config(info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] init_config \n", __func__);
		goto ERROR;
	}

	mip_config_input(info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] input_register_device\n", __func__);
		ret = -EIO;
		goto ERROR;
	}

#if MIP_USE_CALLBACK
	mip_config_callback(info);
#endif

	client->irq = gpio_to_irq(info->pdata->gpio_intr);
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, mip_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, MIP_DEVICE_NAME, info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] request_threaded_irq\n", __func__);
		goto ERROR;
	}

	disable_irq(client->irq);
	info->irq = client->irq;
	sprd_i2c_ctl_chg_clk(1, 400000); /* up h/w i2c 1 400k*/

#if MIP_USE_WAKEUP_GESTURE
	wake_lock_init(&mip_wake_lock, WAKE_LOCK_SUSPEND, "mip_wake_lock");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = mip_early_suspend;
	info->early_suspend.resume = mip_late_resume;

	register_early_suspend(&info->early_suspend);

	dev_dbg(&client->dev, "%s - register_early_suspend\n", __func__);
#endif

	mip_enable(info);

#if MIP_USE_DEV
	if (mip_dev_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip_dev_create\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}

	info->class = class_create(THIS_MODULE, MIP_DEVICE_NAME);
	device_create(info->class, NULL, info->mip_dev, NULL, MIP_DEVICE_NAME);
#endif

#if MIP_USE_SYS
	if (mip_sysfs_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip_sysfs_create\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}
#endif

#if MIP_USE_CMD
	if (mip_sysfs_cmd_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip_sysfs_cmd_create\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}
#endif

	if (sysfs_create_group(&client->dev.kobj, &mip_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, MIP_DEVICE_NAME)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		ret = -EAGAIN;
		goto ERROR;
	}

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	dev_info(&client->dev, "MELFAS " CHIP_NAME " Touchscreen is initialized successfully.\n");
	return 0;

ERROR:
	kfree(info->fw_path_ext);
ERROR_PATH:
	dev_dbg(&client->dev, "%s [ERROR]\n", __func__);
	dev_err(&client->dev, "MELFAS " CHIP_NAME " Touchscreen initialization failed.\n");
	return ret;
}

/**
* Remove driver
*/
static int mip_remove(struct i2c_client *client)
{
	struct mip_ts_info *info = i2c_get_clientdata(client);

#if MIP_USE_CMD
	mip_sysfs_cmd_remove(info);
#endif

#if MIP_USE_SYS
	mip_sysfs_remove(info);
#endif

	sysfs_remove_group(&info->client->dev.kobj, &mip_attr_group);
	sysfs_remove_link(NULL, MIP_DEVICE_NAME);
	kfree(info->print_buf);

#if MIP_USE_DEV
	device_destroy(info->class, info->mip_dev);
	class_destroy(info->class);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
/**
* Device suspend event handler
*/
int mip_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	mip_disable(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return 0;

}

/**
* Device resume event handler
*/
int mip_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	mip_enable(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return ret;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
* Early suspend handler
*/
void mip_early_suspend(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip_suspend(&info->client->dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* Late resume handler
*/
void mip_late_resume(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip_resume(&info->client->dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
* PM info
*/
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
/**
* Device tree match table
*/
static const struct of_device_id mip_match_table[] = {
	{ .compatible = "melfas,"MIP_DEVICE_NAME,},
	{},
};
MODULE_DEVICE_TABLE(of, mip_match_table);
#endif

/**
* I2C Device ID
*/
static const struct i2c_device_id mip_id[] = {
	{MIP_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mip_id);

/**
* I2C driver info
*/
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

/**
* Init driver
*/
static int __init mip_init(void)
{
	return i2c_add_driver(&mip_driver);
}

/**
* Exit driver
*/
static void __exit mip_exit(void)
{
	i2c_del_driver(&mip_driver);
}

module_init(mip_init);
module_exit(mip_exit);

MODULE_DESCRIPTION("MELFAS MIP4 Touchscreen");
MODULE_VERSION("2015.12.05");
MODULE_AUTHOR("Jee, SangWon <jeesw@melfas.com>");
MODULE_LICENSE("GPL");

