/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 */

#include "melfas_mms400.h"
#include <linux/hall.h>

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/trustedui.h>
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern int tui_force_close(uint32_t arg);
struct mms_ts_info *tui_tsp_info;
#endif
/**
 * Reboot chip
 *
 * Caution : IRQ must be disabled before mms_reboot and enabled after mms_reboot.
 */
void mms_reboot(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	i2c_lock_adapter(adapter);

	mms_power_control(info, 0);
	mms_power_control(info, 1);

	i2c_unlock_adapter(adapter);

	usleep_range(10000, 10000);

	if (info->flip_enable)
		mms_set_cover_type(info);

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
}

/**
 * I2C Read
 */
int mms_i2c_read(struct mms_ts_info *info, char *write_buf, unsigned int write_len,
				char *read_buf, unsigned int read_len)
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

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		dev_err(&info->client->dev, 
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	while (retry--) {
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if (res == ARRAY_SIZE(msg)) {
			goto DONE;
		} else if (res < 0) {
			input_err(true, &info->client->dev,
				"%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		} else if (res != ARRAY_SIZE(msg)) {
			input_err(true, &info->client->dev,
				"%s [ERROR] i2c_transfer - result[%d]\n",
				__func__, res);
		}else {
			input_err(true, &info->client->dev,
				"%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
	mms_reboot(info);
	return 1;

DONE:
	return 0;
}


/**
 * I2C Read (Continue)
 */
int mms_i2c_read_next(struct mms_ts_info *info, char *read_buf, int start_idx,
				unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;
	u8 rbuf[read_len];

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		dev_err(&info->client->dev, 
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	while (retry--) {
		res = i2c_master_recv(info->client, rbuf, read_len);

		if (res == read_len) {
			goto DONE;
		} else if (res < 0) {
			input_err(true, &info->client->dev,
				"%s [ERROR] i2c_master_recv - errno [%d]\n", __func__, res);
		} else if (res != read_len) {
			input_err(true, &info->client->dev,
				"%s [ERROR] length mismatch - read[%d] result[%d]\n",
				__func__, read_len, res);
		} else {
			input_err(true, &info->client->dev,
				"%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
	mms_reboot(info);
	return 1;

DONE:
	memcpy(&read_buf[start_idx], rbuf, read_len);

	return 0;
}

/**
 * I2C Write
 */
int mms_i2c_write(struct mms_ts_info *info, char *write_buf, unsigned int write_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		dev_err(&info->client->dev, 
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	while (retry--) {
		res = i2c_master_send(info->client, write_buf, write_len);

		if (res == write_len) {
			goto DONE;
		} else if (res < 0) {
			input_err(true, &info->client->dev,
				"%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		} else if (res != write_len) {
			input_err(true, &info->client->dev,
				"%s [ERROR] length mismatch - write[%d] result[%d]\n",
				__func__, write_len, res);
		} else {
			input_err(true, &info->client->dev,
				"%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	goto ERROR_REBOOT;

ERROR_REBOOT:
	mms_reboot(info);
	return 1;

DONE:
	return 0;
}

/**
 * Enable device
 */
int mms_enable(struct mms_ts_info *info)
{
	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (info->enabled) {
		input_err(true, &info->client->dev,
			"%s : already enabled\n", __func__);
		return 0;
	}

	mutex_lock(&info->lock);

	if (!info->init)
		mms_power_control(info, 1);

	info->enabled = true;
	if (info->flip_enable)
		mms_set_cover_type(info);
	enable_irq(info->client->irq);

	mutex_unlock(&info->lock);

	if(info->disable_esd == true){
		mms_disable_esd_alert(info);
	}

#ifdef MMS_SUPPORT_TA_MODE
	if (info->ta_stsatus)
		mms_charger_attached(info, true);
#endif

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
 * Disable device
 */
int mms_disable(struct mms_ts_info *info)
{
	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (!info->enabled){
		input_err(true, &info->client->dev,
			"%s : already disabled\n", __func__);
		return 0;
	}

	mutex_lock(&info->lock);

	info->enabled = false;
	disable_irq(info->client->irq);
	mms_clear_input(info);
	mms_power_control(info, 0);

	mutex_unlock(&info->lock);

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

#if MMS_USE_INPUT_OPEN_CLOSE
/**
 * Open input device
 */
static int mms_input_open(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);

	if (!info->init) {
		input_info(true, &info->client->dev, "%s %s\n",
				__func__, info->lowpower_mode ? "exit LPM mode" : "");

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){
			dev_err(&info->client->dev, "%s TUI cancel event call!\n", __func__);
			msleep(100);
			tui_force_close(1);
			msleep(200);
			if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){
				dev_err(&info->client->dev, "%s TUI flag force clear!\n",	__func__);
				trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
				trustedui_set_mode(TRUSTEDUI_MODE_OFF);
			}
		}
#endif

		if (info->ic_status >= LPM_RESUME) {
			disable_irq(info->client->irq);

			mms_reboot(info);

			enable_irq(info->client->irq);

			if (device_may_wakeup(&info->client->dev))
				disable_irq_wake(info->client->irq);
		} else {
			mms_enable(info);
		}

		info->ic_status = PWR_ON;
	}

	return 0;
}

/**
 * Close input device
 */
static void mms_input_close(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);

	input_info(true, &info->client->dev, "%s %s\n",
			__func__, info->lowpower_mode ? "enter LPM mode" : "");

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
		dev_err(&info->client->dev, "%s TUI cancel event call!\n", __func__);
		msleep(100);
		tui_force_close(1);
		msleep(200);
		if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
			dev_err(&info->client->dev, "%s TUI flag force clear!\n",	__func__);
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif

	if (info->lowpower_mode) {
		mms_lowpower_mode(info, 1);
		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->client->irq);
		mms_clear_input(info);

		info->ic_status = LPM_RESUME;
	} else {
		mms_disable(info);

		info->ic_status = PWR_OFF;
	}

	return;
}
#endif

#if defined(CONFIG_SEC_DEBUG_TSP_LOG)

struct delayed_work * p_ghost_check;
void run_intensity_for_ghosttouch(struct mms_ts_info *info){

	if (mms_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		input_err(true, &info->client->dev, "%s \n", "NG");
	}
}
static void mms_ghost_touch_check(struct work_struct *work)
{
	struct mms_ts_info *info = container_of(work, struct mms_ts_info,
						ghost_check.work);
	int i;

	if(info->tsp_dump_lock==1){
		input_err(true, &info->client->dev, "%s, ignored ## already checking..\n", __func__);
		return;
	}

	info->tsp_dump_lock = 1;
	info->add_log_header = 1;
	for(i=0; i<5; i++){
		input_err(true, &info->client->dev, "%s, start ##\n", __func__);
		run_intensity_for_ghosttouch((void *)info);
		msleep(100);

	}
	input_info(true, &info->client->dev, "%s, done ##\n", __func__);
	info->tsp_dump_lock = 0;
	info->add_log_header = 0;

}

void tsp_dump(void)
{
	printk(KERN_ERR "mms %s: start \n", __func__);

#if defined(CONFIG_BATTERY_SAMSUNG)
	if (lpcharge == 1) {
		printk(KERN_ERR "%s, ignored ## lpm charging Mode!!\n", __func__);
		return;
	}
#endif
	if (p_ghost_check == NULL){
		printk(KERN_ERR "%s, ignored ## tsp probe fail!!\n", __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#else
void tsp_dump(void)
{
	printk(KERN_ERR "MELFAS %s: not support\n", __func__);
}

#endif

/**
 * Get ready status
 */
int mms_get_ready_status(struct mms_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_READY_STATUS;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	//check status
	if ((ret == MIP_CTRL_STATUS_NONE) || (ret == MIP_CTRL_STATUS_LOG)
		|| (ret == MIP_CTRL_STATUS_READY)) {
		input_info(true, &info->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	} else{
		input_err(true, &info->client->dev,
			"%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if (ret == MIP_CTRL_STATUS_LOG) {
		//skip log event
		wbuf[0] = MIP_R0_LOG;
		wbuf[1] = MIP_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		}
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
 * Read chip firmware version
 */
int mms_get_fw_version(struct mms_ts_info *info, u8 *ver_buf)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 8)) {
		goto ERROR;
	};

	for (i = 0; i < MMS_FW_MAX_SECT_NUM; i++) {
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}

	info->boot_ver_ic = ver_buf[1];
	info->core_ver_ic = ver_buf[3];
	info->config_ver_ic = ver_buf[5];

	input_info(true, &info->client->dev,
			"%s: boot:%x.%x core:%x.%x custom:%x.%x parameter:%x.%x\n",
			__func__,ver_buf[0],ver_buf[1],ver_buf[2],ver_buf[3],ver_buf[4]
			,ver_buf[5],ver_buf[6],ver_buf[7]);

	return 0;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
 * Read chip firmware version for u16
 */
int mms_get_fw_version_u16(struct mms_ts_info *info, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;

	if (mms_get_fw_version(info, rbuf)) {
		goto ERROR;
	}

	for (i = 0; i < MMS_FW_MAX_SECT_NUM; i++) {
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];
	}

	return 0;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
 * Disable ESD alert
 */
int mms_disable_esd_alert(struct mms_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[4];

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto ERROR;
	}

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		goto ERROR;
	}

	if (rbuf[0] != 1) {
		input_info(true, &info->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
 * Alert event handler - ESD
 */
static int mms_alert_handler_esd(struct mms_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[2];

	input_info(true, &info->client->dev, "%s [START] - frame_cnt[%d]\n",
		__func__, frame_cnt);

	if (frame_cnt == 0) {
		//sensor crack, not ESD
		info->esd_cnt++;
		input_info(true, &info->client->dev, "%s - esd_cnt[%d]\n",
			__func__, info->esd_cnt);

		if (info->disable_esd == true) {
			mms_disable_esd_alert(info);
		} else if (info->esd_cnt > ESD_COUNT_FOR_DISABLE) {
			//Disable ESD alert
			if (mms_disable_esd_alert(info))
				input_err(true, &info->client->dev,
					"%s - fail to disable esd alert\n", __func__);
			else
				info->disable_esd = true;
		} else {
			//Reset chip
			mms_reboot(info);
		}
	} else {
		//ESD detected
		//Reset chip
		mms_reboot(info);
		info->esd_cnt = 0;
	}

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

#ifdef MMS_SUPPORT_TA_MODE
int mms_charger_attached(struct mms_ts_info *info, bool status)
{
	u8 wbuf[4];

	input_info(true, &info->client->dev, "%s [START] %s\n", __func__, status ? "connected" : "disconnected");

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;
	wbuf[2] = status;

	if ((status == 0) || (status == 1)) {
		if (mms_i2c_write(info, wbuf, 3))
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, status);
	}
	input_dbg(true, &info->client->dev, "%s [DONE] \n", __func__);
	return 0;
}
#endif

/**
 * Interrupt handler
 */
static irqreturn_t mms_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 wbuf[8];
	u8 rbuf[256];
	unsigned int size = 0;
	int event_size = info->event_size;
	u8 category = 0;
	u8 alert_type = 0;

	if (info->lowpower_mode)
		pm_wakeup_event(info->input_dev->dev.parent, 1000);

	input_dbg(true, &client->dev, "%s [START]\n", __func__);

	//Read first packet
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if (mms_i2c_read(info, wbuf, 2, rbuf, (1 + event_size))) {
		input_err(true, &client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &client->dev, "%s - info [0x%02X]\n", __func__, rbuf[0]);

	//Check event
	size = (rbuf[0] & 0x7F);

	input_dbg(true, &client->dev, "%s - packet size [%d]\n", __func__, size);

	category = ((rbuf[0] >> 7) & 0x1);
	if (category == 0) {
		//Touch event
		if (size > event_size) {
			//Read next packet
			if (mms_i2c_read_next(info, rbuf, (1 + event_size), (size - event_size))) {
				input_err(true, &client->dev, "%s [ERROR] Read next packet\n", __func__);
				goto ERROR;
			}
		}
		info->esd_cnt = 0;
		mms_input_event_handler(info, size, rbuf);
	} else {
		//Alert event
		alert_type = rbuf[1];

		input_dbg(true, &client->dev, "%s - alert type [%d]\n", __func__, alert_type);

		if (alert_type == MIP_ALERT_ESD) {
			//ESD detection
			if (mms_alert_handler_esd(info, rbuf)) {
				goto ERROR;
			}
		} else if (alert_type == MIP_ALERT_WAKEUP) {
			if (info->lowpower_flag & MMS_LPM_FLAG_SPAY) {
				info->scrub_id = 0x04;
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);
				input_info(true, &client->dev, "%s: [Gesture] Spay, flag%x\n",
						__func__, info->lowpower_flag);
			}
		} else {
			input_err(true, &client->dev, "%s [ERROR] Unknown alert type [%d]\n",
				__func__, alert_type);
			goto ERROR;
		}
	}

	input_dbg(true, &client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;

ERROR:
	input_err(true, &client->dev, "%s [ERROR]\n", __func__);
	if (RESET_ON_EVENT_ERROR) {
		input_info(true, &client->dev, "%s - Reset on error\n", __func__);

		mms_disable(info);
		mms_clear_input(info);
		mms_enable(info);
	}
	return IRQ_HANDLED;
}

/**
 * Update firmware from kernel built-in binary
 */
int mms_fw_update_from_kernel(struct mms_ts_info *info, bool force)
{
	const char *fw_name = info->dtdata->fw_name;
	const struct firmware *fw;
	int retires = 6;
	int ret;
#ifdef CONFIG_DUAL_TSP
	int value;
#endif

	if (!fw_name)
		goto ERROR;

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);
	mms_clear_input(info);

	//Get firmware
	request_firmware(&fw, fw_name, &info->client->dev);

	if (!fw) {
		input_err(true, &info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	//Update fw
	do {
		ret = mms_flash_fw(info, fw->data, fw->size, force, true);
		if (ret >= fw_err_none) {
			break;
		}
	} while (--retires);

	if (!retires) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_flash_fw failed\n", __func__);
		ret = -1;
	}

#ifdef CONFIG_DUAL_TSP
	value = gpio_get_value(info->dtdata->tsp_sel);
	gpio_direction_output(info->dtdata->tsp_sel, !value);
	input_info(true, &info->client->dev, "%s mms_flash_fw TSP%d\n", __func__, !value);
	retires = 6;
	udelay(500);
	//Double TSP need update fw
	do {
		ret = mms_flash_fw(info, fw->data, fw->size, force, true);
		if (ret >= fw_err_none) {
			break;
		}
	} while (--retires);

	if (!retires) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_flash_fw TSP%d failed\n", __func__, !value);
		ret = -1;
	}
	gpio_direction_output(info->dtdata->tsp_sel, value);
	udelay(500);
	input_info(true, &info->client->dev, "%s mms_flash_fw TSP%d\n", __func__, value);
#endif

	release_firmware(fw);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < 0) {
		goto ERROR;
	}

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
 * Update firmware from external storage
 */
int mms_fw_update_from_storage(struct mms_ts_info *info, bool force)
{
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int ret = 0;

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);
	mms_clear_input(info);

	//Get firmware
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(EXTERNAL_FW_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		input_err(true, &info->client->dev, "%s [ERROR] file_open - path[%s]\n",
			__func__, EXTERNAL_FW_PATH);
		ret = fw_err_file_open;
		goto ERROR;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		input_info(true, &info->client->dev, "%s - path [%s] size [%zu]\n",
			__func__,EXTERNAL_FW_PATH, fw_size);

		if (nread != fw_size) {
			input_err(true, &info->client->dev, "%s [ERROR] vfs_read - size[%zu] read[%zu]\n",
				__func__, fw_size, nread);
			ret = fw_err_file_read;
		} else {
			//Update fw
			ret = mms_flash_fw(info, fw_data, fw_size, force, true);
		}

		kfree(fw_data);
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] fw_size [%zu]\n", __func__, fw_size);
		ret = fw_err_file_read;
	}

	filp_close(fp, current->files);

ERROR:
	set_fs(old_fs);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret == 0)
		input_err(true, &info->client->dev, "%s [DONE]\n", __func__);
	else
		input_err(true, &info->client->dev, "%s [ERROR] %d\n", __func__, ret);

	return ret;
}
/**
 * Getting firmware from air
 */
int mms_fw_update_from_ffu(struct mms_ts_info *info, bool force)
{
	const struct firmware *fw;
	int retires = 3;
	int ret;

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);
	mms_clear_input(info);

	//Get firmware
	request_firmware(&fw, FFU_FW_PATH, &info->client->dev);

	if (!fw) {
		input_err(true, &info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto ERROR;
	}

	//Update fw
	do {
		ret = mms_flash_fw(info, fw->data, fw->size, force, true);
		if (ret >= fw_err_none) {
			break;
		}
	} while (--retires);

	if (!retires) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_flash_fw failed\n", __func__);
		ret = -1;
	}

	release_firmware(fw);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < 0) {
		goto ERROR;
	}

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

static ssize_t mms_sys_fw_update(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;

	memset(info->print_buf, 0, PAGE_SIZE);

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	ret = mms_fw_update_from_storage(info, true);

	switch (ret) {
	case fw_err_none:
		sprintf(data, "F/W update success\n");
		break;
	case fw_err_uptodate:
		sprintf(data, "F/W is already up-to-date\n");
		break;
	case fw_err_download:
		sprintf(data, "F/W update failed : Download error\n");
		break;
	case fw_err_file_type:
		sprintf(data, "F/W update failed : File type error\n");
		break;
	case fw_err_file_open:
		sprintf(data, "F/W update failed : File open error [%s]\n", EXTERNAL_FW_PATH);
		break;
	case fw_err_file_read:
		sprintf(data, "F/W update failed : File read error\n");
		break;
	default:
		sprintf(data, "F/W update failed\n");
		break;
	}

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);

	strcat(info->print_buf, data);
	result = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return result;
}
static DEVICE_ATTR(fw_update, 0666, mms_sys_fw_update, NULL);

/**
 * Sysfs attr info
 */
static struct attribute *mms_attrs[] = {
	&dev_attr_fw_update.attr,
	NULL,
};

/**
 * Sysfs attr group info
 */
static const struct attribute_group mms_attr_group = {
	.attrs = mms_attrs,
};

/**
 * Initial config
 */

#ifdef CONFIG_DUAL_TSP
static struct mms_ts_info *tsp_driver = NULL;
void mms_set_tsp_info(struct mms_ts_info *mms_data)
{
	if (mms_data != NULL)
		tsp_driver = mms_data;
	else
		input_info(true, &mms_data->client->dev,"%s : tsp info is null\n", __func__);
}
static struct mms_ts_info *mms_get_tsp_info(void)
{
	return tsp_driver;
}

static void mms_switching_tsp_work(struct work_struct *work)
{
	struct mms_ts_info *mms_data =
			container_of(work, struct mms_ts_info,
			switching_work.work);


	if (mms_data == NULL){
		input_info(true, &mms_data->client->dev,"[TSP] %s: tsp info is null\n", __func__);
		return;
	}



	if (mms_data->flip_status != mms_data->flip_status_current)
	{
		mutex_lock(&mms_data->mms_switching_mutex);
		mms_data->flip_status = mms_data->flip_status_current;
		input_info(true, &mms_data->client->dev,
			"%s : flip %sed. using TSP_%s\n",
			__func__, mms_data->flip_status_current? "clos":"open",
			mms_data->flip_status_current ? "SUB":"MAIN");

		gpio_direction_output(mms_data->dtdata->tsp_sel, mms_data->flip_status_current);

		mutex_unlock(&mms_data->mms_switching_mutex);
	}
}

static int mms_hall_ic_notify(struct notifier_block *nb,
				unsigned long flip_cover, void *v)
{
	struct mms_ts_info *mms_data = mms_get_tsp_info();

	input_info(true, &mms_data->client->dev,"%s %s\n", __func__, flip_cover ? "close" : "open");

	mms_data->flip_status_current = flip_cover;

	schedule_delayed_work(&mms_data->switching_work,
				msecs_to_jiffies(1));

	return 0;
}
#endif

static int mms_init_config(struct mms_ts_info *info)
{
	u8 wbuf[8];
	u8 rbuf[32];
	u8 tmp[4] = MMS_CONFIG_DATE;

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	/* read product name */
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mms_i2c_read(info, wbuf, 2, rbuf, 16);
	memcpy(info->product_name, rbuf, 16);
	input_info(true, &info->client->dev, "%s - product_name[%s]\n",
		__func__, info->product_name);

	/* read fw version */
	mms_get_fw_version(info, rbuf);

	/* read fw build date */
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_BUILD_DATE;
	mms_i2c_read(info, wbuf, 2, rbuf, 4);

	if (!strncmp(rbuf, "", 4))
		memcpy(rbuf, tmp, 4);

	info->fw_year = (rbuf[0] << 8) | (rbuf[1]);
	info->fw_month = rbuf[2];
	info->fw_date = rbuf[3];

	input_info(true, &info->client->dev, "%s - fw build date : %d/%d/%d\n",
		__func__, info->fw_year, info->fw_month, info->fw_date);

	/* read checksum */
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CHECKSUM_PRECALC;
	mms_i2c_read(info, wbuf, 2, rbuf, 4);

	info->pre_chksum = (rbuf[0] << 8) | (rbuf[1]);
	info->rt_chksum = (rbuf[2] << 8) | (rbuf[3]);
	input_info(true, &info->client->dev,
		"%s - precalced checksum:%04X, real-time checksum:%04X\n",
		__func__, info->pre_chksum, info->rt_chksum);


	/* Set resolution using chip info */
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mms_i2c_read(info, wbuf, 2, rbuf, 7);

	info->max_x = (rbuf[0]) | (rbuf[1] << 8);
	info->max_y = (rbuf[2]) | (rbuf[3] << 8);
	input_info(true, &info->client->dev, "%s - max_x[%d] max_y[%d]\n",
		__func__, info->max_x, info->max_y);

	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	input_info(true, &info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n",
		__func__, info->node_x, info->node_y, info->node_key);

#if MMS_USE_TOUCHKEY
	/* Enable touchkey */
	if (info->node_key > 0) {
		info->tkey_enable = true;
	}
#endif
	info->event_size = MMS_DEFAULT_EVENT_SIZE;

	input_info(true, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
 * Initialize driver
 */
static int mms_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;

	input_err(true, &client->dev, "%s [START]\n", __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		input_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev,
			"%s [ERROR] i2c_check_functionality\n", __func__);
		ret = -EIO;
		goto ERROR;
	}

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "%s [ERROR] info alloc\n", __func__);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev, "%s [ERROR] input alloc\n", __func__);
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->irq = -1;
	info->init = true;
	mutex_init(&info->lock);
	info->touch_count = 0;

#ifdef CONFIG_DUAL_TSP
	info->flip_status = -1;
	info->flip_status_current = -1;
	mms_set_tsp_info(info);
#endif

#if MMS_USE_DEVICETREE
	if (client->dev.of_node) {
		info->dtdata  =
			devm_kzalloc(&client->dev,
				sizeof(struct mms_devicetree_data), GFP_KERNEL);
		if (!info->dtdata) {
			input_err(true, &client->dev,
				"%s [ERROR] dtdata devm_kzalloc\n", __func__);
			goto err_devm_alloc;
		}
		mms_parse_devicetree(&client->dev, info);
	} else
#endif
	{
		info->dtdata = client->dev.platform_data;
		if (info->dtdata == NULL) {
			input_err(true, &client->dev, "%s [ERROR] dtdata is null\n", __func__);
			ret = -EINVAL;
			goto err_platform_data;
		}
	}

	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl data\n", __func__);
		ret = PTR_ERR(info->pinctrl);
		goto err_platform_data;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = info;
#endif

	snprintf(info->phys, sizeof(info->phys), "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
#if MMS_USE_INPUT_OPEN_CLOSE
	input_dev->open = mms_input_open;
	input_dev->close = mms_input_close;
#endif
	//set input event buffer size
	input_set_events_per_packet(input_dev, 200);

	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	ret = input_register_device(input_dev);
	if (ret) {
		input_err(true, &client->dev, "%s [ERROR] input_register_device\n", __func__);
		ret = -EIO;
		goto err_input_register_device;
	}
#ifdef CONFIG_DUAL_TSP
		mutex_init(&(info->mms_switching_mutex));
		INIT_DELAYED_WORK(&info->switching_work, mms_switching_tsp_work);

		/* Hall IC notify priority -> ftn -> register */
		info->hall_ic_nb.priority = 1;
		info->hall_ic_nb.notifier_call = mms_hall_ic_notify;
		hall_ic_register_notify(&info->hall_ic_nb);
		input_info(true,&client->dev, "%s,touchkey:hall ic register",__func__);
#endif

	mms_power_control(info, 1);

#if MMS_USE_AUTO_FW_UPDATE
	ret = mms_fw_update_from_kernel(info, false);
	if(ret){
		input_err(true, &client->dev, "%s [ERROR] mms_fw_update_from_kernel\n", __func__);
	}
#endif

	mms_init_config(info);
	mms_config_input(info);

#ifdef USE_TSP_TA_CALLBACKS
	info->register_cb = mms_register_callback;
	info->callbacks.inform_charger = mms_charger_status_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
#endif
#ifdef MMS_SUPPORT_TA_MODE
	vbus_notifier_register(&info->vbus_nb, mms_vbus_notification,
				VBUS_NOTIFY_DEV_CHARGER);
#endif

	ret = request_threaded_irq(client->irq, NULL, mms_interrupt,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, MMS_DEVICE_NAME, info);
	if (ret) {
		input_err(true, &client->dev, "%s [ERROR] request_threaded_irq\n", __func__);
		goto err_request_irq;
	}

	disable_irq(client->irq);
	info->irq = client->irq;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	trustedui_set_tsp_irq(info->irq);
	dev_err(&client->dev, "%s[%d] called!\n",
		__func__, info->irq);
#endif

	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "mms_wakelock");

	mms_enable(info);

#if MMS_USE_DEV_MODE
	if(mms_dev_create(info)){
		input_err(true, &client->dev, "%s [ERROR] mms_dev_create\n", __func__);
		ret = -EAGAIN;
		goto err_test_dev_create;
	}

	info->class = class_create(THIS_MODULE, MMS_DEVICE_NAME);
	device_create(info->class, NULL, info->mms_dev, NULL, MMS_DEVICE_NAME);
#endif

#if MMS_USE_TEST_MODE
	if (mms_sysfs_create(info)){
		input_err(true, &client->dev, "%s [ERROR] mms_sysfs_create\n", __func__);
		ret = -EAGAIN;
		goto err_test_sysfs_create;
	}
#endif

#if MMS_USE_CMD_MODE
	if (mms_sysfs_cmd_create(info)){
		input_err(true, &client->dev, "%s [ERROR] mms_sysfs_cmd_create\n", __func__);
		ret = -EAGAIN;
		goto err_fac_cmd_create;
	}
#endif

	if (sysfs_create_group(&client->dev.kobj, &mms_attr_group)) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		ret = -EAGAIN;
		goto err_create_attr_group;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, MMS_DEVICE_NAME)) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		ret = -EAGAIN;
		goto err_create_dev_link;
	}

#if defined(CONFIG_SEC_DEBUG_TSP_LOG)
	INIT_DELAYED_WORK(&info->ghost_check, mms_ghost_touch_check);
	p_ghost_check = &info->ghost_check;
#endif
	device_init_wakeup(&client->dev, true);
	info->init = false;
	info->ic_status = PWR_ON;
	input_info(true, &client->dev,
		"MELFAS " CHIP_NAME " Touchscreen is initialized successfully\n");
	return 0;


err_create_dev_link:
	sysfs_remove_group(&client->dev.kobj, &mms_attr_group);
err_create_attr_group:
#if MMS_USE_CMD_MODE
	mms_sysfs_cmd_remove(info);
err_fac_cmd_create:
#endif
#if MMS_USE_TEST_MODE
	mms_sysfs_remove(info);
err_test_sysfs_create:
#endif
#if MMS_USE_DEV_MODE
	device_destroy(info->class, info->mms_dev);
	class_destroy(info->class);
err_test_dev_create:
#endif
	mms_disable(info);
	free_irq(info->irq, info);
	wake_lock_destroy(&info->wakelock);
err_request_irq:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
err_input_register_device:
err_platform_data:
#if MMS_USE_DEVICETREE
err_devm_alloc:
#endif
	if (info->input_dev)
		input_free_device(info->input_dev);
err_input_alloc:
	kfree(info);
err_mem_alloc:
ERROR:
	pr_err("MELFAS " CHIP_NAME " Touchscreen initialization failed.\n");
	return ret;
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	dev_err(&tui_tsp_info->client->dev, "%s, release all finger..\n",	__func__);
	mms_clear_input(tui_tsp_info);
}
#endif

/**
 * Remove driver
 */
static int mms_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0) {
		free_irq(info->irq, info);
	}

	wake_lock_destroy(&info->wakelock);

#if MMS_USE_CMD_MODE
	mms_sysfs_cmd_remove(info);
#endif

#if MMS_USE_TEST_MODE
	mms_sysfs_remove(info);
#endif

	sysfs_remove_group(&info->client->dev.kobj, &mms_attr_group);
	sysfs_remove_link(NULL, MMS_DEVICE_NAME);

#if MMS_USE_DEV_MODE
	device_destroy(info->class, info->mms_dev);
	class_destroy(info->class);
#endif

	input_unregister_device(info->input_dev);

	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if MMS_USE_DEVICETREE
/**
 * Device tree match table
 */
static const struct of_device_id mms_match_table[] = {
	{ .compatible = "melfas,mms_ts",},
	{},
};
MODULE_DEVICE_TABLE(of, mms_match_table);
#endif

/**
 * I2C Device ID
 */
static const struct i2c_device_id mms_id[] = {
	{MMS_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mms_id);

/**
 * I2C driver info
 */
static struct i2c_driver mms_driver = {
	.id_table	= mms_id,
	.probe = mms_probe,
	.remove = mms_remove,
	.driver = {
		.name = MMS_DEVICE_NAME,
		.owner = THIS_MODULE,
#if MMS_USE_DEVICETREE
		.of_match_table = mms_match_table,
#endif
	},
};

/**
 * Init driver
 */


unsigned int lcdid = 0;
static int __init get_lcd_id(char *arg)
{
	get_option(&arg, &lcdid);
	return 0;
}
early_param("lcdtype", get_lcd_id);


static int __init mms_init(void)
{
	pr_err("%s\n", __func__);
#if defined(CONFIG_SAMSUNG_LPM_MODE)
	//if (poweroff_charging) {
		//pr_notice("%s : LPM Charging Mode!!\n", __func__);
		//return 0;
	//}
#endif

	if (!lcdid) {
		pr_notice("%s: LCD is not attached\n", __func__);
		return 0;
	}

	return i2c_add_driver(&mms_driver);
}

/**
 * Exit driver
 */
static void __exit mms_exit(void)
{
	i2c_del_driver(&mms_driver);
}

module_init(mms_init);
module_exit(mms_exit);

MODULE_DESCRIPTION("MELFAS MMS400 Touchscreen");
MODULE_VERSION("2014.12.05");
MODULE_AUTHOR("Jee, SangWon <jeesw@melfas.com>");
MODULE_LICENSE("GPL");
