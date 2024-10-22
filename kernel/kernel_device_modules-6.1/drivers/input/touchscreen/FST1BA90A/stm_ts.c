// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/input/sec_input/stm_fst1ba90a_i2c/stm_ts.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* (C) COPYRIGHT 2012 STMicroelectronics
 *
 * File Name		: stm_ts.c
 * Authors		: AMS(Analog Mems Sensor) Team
 * Description	: STM_TS Capacitive touch screen controller (FingerTipS)
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *******************************************************************************/

#include "stm_ts.h"

static int stm_ts_enable(struct device *dev);
static int stm_ts_disable(struct device *dev);

static void stm_ts_reset(struct stm_ts_data *ts, unsigned int ms);
static void stm_ts_reset_work(struct work_struct *work);
static void stm_ts_read_info_work(struct work_struct *work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
static void tsp_dump(struct device *dev);
static void dump_tsp_rawdata(struct work_struct *work);
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t secure_filter_interrupt(struct stm_ts_data *ts);

static irqreturn_t stm_ts_interrupt_handler(int irq, void *handle);

static ssize_t stm_ts_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t stm_ts_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t stm_ts_secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static struct device_attribute attrs[] = {
	__ATTR(secure_touch_enable, (0664),
			stm_ts_secure_touch_enable_show,
			stm_ts_secure_touch_enable_store),
	__ATTR(secure_touch, (0444),
			stm_ts_secure_touch_show,
			NULL),
};

static ssize_t stm_ts_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t stm_ts_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	err = kstrtoul(buf, 10, &value);
	if (err != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, err);
		return err;
	}

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&ts->st_enabled) == 0) {
			input_err(true, &ts->client->dev, "%s: secure_touch is not enabled, pending:%d\n",
					__func__, atomic_read(&ts->st_pending_irqs));
			break;
		}

		pm_runtime_put_sync(ts->client->adapter->dev.parent);

		atomic_set(&ts->st_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		stm_ts_interrupt_handler(ts->client->irq, ts);

		complete(&ts->st_powerdown);
		complete(&ts->st_interrupt);

		input_info(true, &ts->client->dev, "%s: secure_touch is disabled\n", __func__);

		break;

	case 1:
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is on goning becuse i2c fail\n",
					__func__);
			return -EBUSY;
		}

		if (atomic_read(&ts->st_enabled)) {
			input_err(true, &ts->client->dev, "%s: secure_touch is already enabled, pending:%d\n",
					__func__, atomic_read(&ts->st_pending_irqs));
			err = -EBUSY;
			break;
		}

		/* synchronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		sec_input_irq_disable(ts->plat_data);

		/* Release All Finger */
		stm_ts_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->adapter->dev.parent) < 0) {
			input_err(true, &ts->client->dev, "%s: pm_runtime_get failed\n", __func__);
			err = -EIO;
			sec_input_irq_enable(ts->plat_data);
			break;
		}

		reinit_completion(&ts->st_powerdown);
		reinit_completion(&ts->st_interrupt);
		atomic_set(&ts->st_enabled, 1);
		atomic_set(&ts->st_pending_irqs, 0);

		sec_input_irq_enable(ts->plat_data);

		input_info(true, &ts->client->dev, "%s: secure_touch is enabled\n", __func__);

		break;

	default:
		input_err(true, &ts->client->dev, "%s: unsupported value: %lu\n", __func__, value);
		err = -EINVAL;
		break;
	}

	return err;
}

static ssize_t stm_ts_secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->st_enabled) == 0) {
		input_err(true, &ts->client->dev, "%s: secure_touch is not enabled, st_pending_irqs: %d\n",
				__func__, atomic_read(&ts->st_pending_irqs));
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->st_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: st_pending_irqs: %d\n",
				__func__, atomic_read(&ts->st_pending_irqs));
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->st_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_info(true, &ts->client->dev, "%s: st_pending_irqs: %d, val: %d\n",
				__func__, atomic_read(&ts->st_pending_irqs), val);
	}

	complete(&ts->st_interrupt);

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}

static void secure_touch_init(struct stm_ts_data *ts)
{
	init_completion(&ts->st_powerdown);
	init_completion(&ts->st_interrupt);
}

static void secure_touch_stop(struct stm_ts_data *ts, int blocking)
{
	if (atomic_read(&ts->st_enabled)) {
		atomic_set(&ts->st_pending_irqs, -1);
		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (blocking)
			wait_for_completion_interruptible(&ts->st_powerdown);
	}
}

static irqreturn_t secure_filter_interrupt(struct stm_ts_data *ts)
{
	if (atomic_read(&ts->st_enabled)) {
		if (atomic_cmpxchg(&ts->st_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");
		} else {
			input_info(true, &ts->client->dev, "%s: st_pending_irqs: %d\n",
					__func__, atomic_read(&ts->st_pending_irqs));
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
#endif

int stm_ts_write(struct stm_ts_data *ts,
		u8 *reg, u16 num_com)
{
	struct i2c_msg xfer_msg[2];
	int ret;
	int retry = STM_TS_I2C_RETRY_CNT;
	u8 *buff;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	buff = kzalloc(num_com, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;
	memcpy(buff, reg, num_com);

	mutex_lock(&ts->i2c_mutex);

	xfer_msg[0].addr = ts->client->addr;
	xfer_msg[0].len = num_com;
	xfer_msg[0].flags = 0 | I2C_M_DMA_SAFE;
	xfer_msg[0].buf = buff;

	do {
		ret = i2c_transfer(ts->client->adapter, xfer_msg, 1);
		if (ret < 0) {
			ts->plat_data->hw_param.comm_err_count++;
			input_err(true, &ts->client->dev,
					"%s failed(%d). ret:%d, addr:%x, cnt:%d\n",
					__func__, retry, ret, xfer_msg[0].addr, ts->plat_data->hw_param.comm_err_count);
			sec_delay(10);
		} else {
			break;
		}
	} while (--retry > 0);

	mutex_unlock(&ts->i2c_mutex);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;

		if (ts->debug_string & STM_TS_DEBUG_SEND_UEVENT)
			sec_cmd_send_event_to_user(&ts->sec, NULL, "RESULT=I2C");
#ifdef USE_POR_AFTER_I2C_RETRY
		if (ts->probe_done && !ts->reset_is_on_going && !atomic_read(&ts->fw_update_is_running) && !atomic_read(&ts->plat_data->shutdown_called))
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
#endif
	}

	if (ts->debug_string & STM_TS_DEBUG_PRINT_I2C_WRITE_CMD) {
		int i;

		pr_info("sec_input: i2c_cmd: W: ");
		for (i = 0; i < num_com; i++)
			pr_cont("%02X ", buff[i]);
		pr_cont("\n");

	}

	kfree(buff);

	return ret;
}

int stm_ts_read(struct stm_ts_data *ts, u8 *reg, int cnum,
		u8 *buf, int num)
{
	struct i2c_msg xfer_msg[2];
	int ret;
	int retry = STM_TS_I2C_RETRY_CNT;
	u8 *buff;
	u8 *msg_buff;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	msg_buff = kzalloc(cnum, GFP_KERNEL);
	if (!msg_buff)
		return -ENOMEM;

	memcpy(msg_buff, reg, cnum);

	buff = kzalloc(num, GFP_KERNEL);
	if (!buff) {
		kfree(msg_buff);
		return -ENOMEM;
	}

	mutex_lock(&ts->i2c_mutex);

	xfer_msg[0].addr = ts->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0 | I2C_M_DMA_SAFE;
	xfer_msg[0].buf = msg_buff;

	xfer_msg[1].addr = ts->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD | I2C_M_DMA_SAFE;
	xfer_msg[1].buf = buff;

	do {
		ret = i2c_transfer(ts->client->adapter, xfer_msg, 2);
		if (ret < 0) {
			ts->plat_data->hw_param.comm_err_count++;
			input_err(true, &ts->client->dev,
					"%s failed(%d). ret:%d, addr:%x, cnt:%d\n",
					__func__, retry, ret, xfer_msg[0].addr, ts->plat_data->hw_param.comm_err_count);
			sec_delay(10);
		} else {
			break;
		}
	} while (--retry > 0);

	mutex_unlock(&ts->i2c_mutex);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;

		if (ts->debug_string & STM_TS_DEBUG_SEND_UEVENT)
			sec_cmd_send_event_to_user(&ts->sec, NULL, "RESULT=I2C");
#ifdef USE_POR_AFTER_I2C_RETRY
		if (ts->probe_done && !ts->reset_is_on_going && !atomic_read(&ts->fw_update_is_running) && !atomic_read(&ts->plat_data->shutdown_called))
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
#endif
	}

	if (ts->debug_string & STM_TS_DEBUG_PRINT_I2C_READ_CMD) {
		int i;

		pr_info("sec_input: i2c_cmd: R: ");
		for (i = 0; i < cnum; i++)
			pr_cont("%02X ", msg_buff[i]);
		pr_cont("|");
		for (i = 0; i < num; i++)
			pr_cont("%02X ", buff[i]);
		pr_cont("\n");

	}

	memcpy(buf, buff, num);
	kfree(msg_buff);
	kfree(buff);

	return ret;
}

static int stm_ts_read_from_sponge(struct stm_ts_data *ts,
		u16 offset, u8 *data, int length)
{
	u8 sponge_reg[3];
	u8 *buf;
	int rtn;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	mutex_lock(&ts->sponge_mutex);
	offset += STM_TS_CMD_SPONGE_ACCESS;
	sponge_reg[0] = 0xAA;
	sponge_reg[1] = (offset >> 8) & 0xFF;
	sponge_reg[2] = offset & 0xFF;

	buf = kzalloc(length, GFP_KERNEL);
	if (buf == NULL) {
		rtn = -ENOMEM;
		goto out;
	}

	rtn = stm_ts_read(ts, sponge_reg, 3, buf, length);
	if (rtn >= 0)
		memcpy(data, &buf[0], length);
	else
		input_err(true, &ts->client->dev, "%s: failed\n", __func__);

	kfree(buf);
out:
	mutex_unlock(&ts->sponge_mutex);
	return rtn;
}

/*
 * int stm_ts_write_to_sponge(struct stm_ts_data *, u16 *, u8 *, int)
 * send command or write specific value to the sponge area.
 * sponge area means guest image or display lab firmware.. etc..
 */
static int stm_ts_write_to_sponge(struct stm_ts_data *ts,
		u16 offset, u8 *data, int length)
{
	u8 *regAdd;
	int ret = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	regAdd = kzalloc(3 + length, GFP_KERNEL);
	if (!regAdd)
		return -ENOMEM;

	mutex_lock(&ts->sponge_mutex);
	offset += STM_TS_CMD_SPONGE_ACCESS;
	regAdd[0] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;
	regAdd[1] = (offset >> 8) & 0xFF;
	regAdd[2] = offset & 0xFF;

	memcpy(&regAdd[3], &data[0], length);

	ret = ts->stm_ts_write(ts, &regAdd[0], 3 + length);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: sponge command is failed. ret: %d\n", __func__, ret);
		goto out;
	}

	// Notify Command
	regAdd[0] = STM_TS_CMD_SPONGE_NOTIFY_CMD;
	regAdd[1] = (offset >> 8) & 0xFF;
	regAdd[2] = offset & 0xFF;

	ret = ts->stm_ts_write(ts, &regAdd[0], 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: sponge notify is failed.\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev,
			"%s: sponge notify is OK[0x%02X].\n", __func__, *data);
out:
	mutex_unlock(&ts->sponge_mutex);
	kfree(regAdd);
	return ret;
}

int stm_ts_check_custom_library(struct stm_ts_data *ts)
{
	struct stm_ts_sponge_information *sponge_info;

	u8 regAdd[3] = { 0xA4, 0x06, 0x91 };
	u8 data[sizeof(struct stm_ts_sponge_information)] = { 0 };
	int ret = -1;

	sec_input_irq_disable(ts->plat_data);

	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		goto out;
	}

	sec_input_irq_enable(ts->plat_data);

	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	ret = stm_ts_read(ts, &regAdd[0], 3, &data[0], sizeof(struct stm_ts_sponge_information));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		goto out;
	}

	sponge_info = (struct stm_ts_sponge_information *) &data[0];

	input_info(true, &ts->client->dev,
			"%s: SPONGE INFO: use:%d, ver:%d, model name %s\n",
			__func__, sponge_info->sponge_use,
			sponge_info->sponge_ver, sponge_info->sponge_model_name);

	ret = ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_OFFSET_MODE,
			&ts->plat_data->lowpower_mode, sizeof(ts->plat_data->lowpower_mode));
	if (ret < 0)
		goto out;

	if (!ts->plat_data->support_fod)
		goto out;

	ret = ts->stm_ts_read_from_sponge(ts, STM_TS_CMD_SPONGE_FOD_INFO, regAdd, 3);
	if (ret < 0)
		goto out;

	sec_input_set_fod_info(&ts->client->dev, regAdd[0], regAdd[1], regAdd[2], 0);

out:
	return ret;
}

int stm_ts_set_press_property(struct stm_ts_data *ts)
{
	int ret = 0;

	if (!ts->plat_data->support_fod)
		return 0;

	ret = ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_PRESS_PROPERTY,
			&ts->plat_data->fod_data.press_prop, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->fod_data.press_prop);

	return ret;
}

int stm_ts_command(struct stm_ts_data *ts, u8 cmd, bool checkEcho)
{
	u8 regAdd = cmd;
	int ret = 0;

	sec_input_irq_disable(ts->plat_data);
	input_info(true, &ts->client->dev, "%s: 0x%02X\n", __func__, regAdd);

	if (checkEcho)
		ret = stm_ts_wait_for_echo_event(ts, &regAdd, 1, 0);
	else
		ret = ts->stm_ts_write(ts, &regAdd, 1);
	if (ret < 0)
		input_info(true, &ts->client->dev,
				"%s: failed to write cmd, ret = %d\n", __func__, ret);

	sec_input_irq_enable(ts->plat_data);

	return ret;
}

int stm_ts_set_scanmode(struct stm_ts_data *ts, u8 scan_mode)
{
	u8 regAdd[3] = { STM_TS_CMD_SET_FUNCTION_ONOFF, STM_TS_FUNCTION_ENABLE_VSYNC, 0x00 };
	int rc;

	sec_input_irq_disable(ts->plat_data);

	if (ts->plat_data->disable_vsync_scan) {
		rc = ts->stm_ts_write(ts, regAdd, 3);
		if (rc < 0)
			input_err(true, &ts->client->dev,
					"%s: failed to disable vsync scan, ret = %d\n", __func__, rc);
		else
			input_info(true, &ts->client->dev, "%s: disable vsync scan\n", __func__);
	}

	/* read vsync scan mode */
	rc = stm_ts_read(ts, regAdd, 2, &ts->vsync_scan, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to get vsync scan mode, ret = %d\n", __func__, rc);
		ts->vsync_scan = 0;
	}

	/* set scan mode */
	regAdd[0] = 0xA0;
	regAdd[1] = 0x00;
	regAdd[2] = scan_mode;
	rc = ts->stm_ts_write(ts, regAdd, 3);
	if (rc < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set scan mode, ret = %d\n", __func__, rc);
	sec_delay(50);
	sec_input_irq_enable(ts->plat_data);
	input_info(true, &ts->client->dev, "%s: 0x%02X(%s), vs:0x%02X\n",
			__func__, scan_mode, scan_mode == STM_TS_SCAN_MODE_SCAN_OFF ? "OFF" : "ON", ts->vsync_scan);

	return rc;
}

int stm_ts_set_opmode(struct stm_ts_data *ts, u8 mode)
{
	int ret, i, j;
	u8 regAdd[2] = {STM_TS_CMD_SET_GET_OPMODE, mode};
	u8 para = 0;

	for (i = 0; i < STM_TS_MODE_CHANGE_RETRY_CNT; i++) {
		ret = ts->stm_ts_write(ts, &regAdd[0], 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to set mode: %d, retry=%d\n", __func__, mode, i);
			return ret;
		}

		sec_delay(ts->lpmode_change_delay);

		for (j = 0; j < STM_TS_MODE_CHANGE_RETRY_CNT; j++) {
			ret = ts->stm_ts_read(ts, &regAdd[0], 1, &para, 1);
			if (ret < 0) {
				input_err(true, &ts->client->dev,
						"%s: read power mode failed!, retry=%d\n", __func__, j);
				return ret;
			}

			input_info(true, &ts->client->dev, "%s: write(%d) read(%d) retry %d\n",
					__func__, mode, para, j);
			if (mode == para)
				break;

			sec_delay(10);
		}

		if (mode == para)
			break;
	}

	if (ts->plat_data->lowpower_mode) {
		regAdd[0] = STM_TS_CMD_WRITE_WAKEUP_GESTURE;
		regAdd[1] = 0x02;
		ret = ts->stm_ts_write(ts, &regAdd[0], 2);
		if (ret < 0)
			input_err(true, &ts->client->dev,
					"%s: Failed to enable wakeup gesture\n", __func__);
	}

	return ret;
}

void stm_ts_set_fod_finger_merge(struct stm_ts_data *ts)
{
	int ret;
	u8 regAdd[2] = {STM_TS_CMD_SET_FOD_FINGER_MERGE, 0};

	if (!ts->plat_data->support_fod)
		return;

	if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)
		regAdd[1] = 1;
	else
		regAdd[1] = 0;

	mutex_lock(&ts->sponge_mutex);
	input_info(true, &ts->client->dev, "%s: %d\n", __func__, regAdd[1]);

	ret = ts->stm_ts_write(ts, regAdd, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed\n", __func__);
	mutex_unlock(&ts->sponge_mutex);
}

int stm_ts_set_touch_function(struct stm_ts_data *ts)
{
	u8 regAdd[3] = { 0 };
	int ret;

	input_info(true, &ts->client->dev, "%s: 0x%04X\n", __func__, ts->plat_data->touch_functions);

	regAdd[0] = STM_TS_CMD_SET_GET_TOUCH_FUNCTION;
	regAdd[1] = (u8)(ts->plat_data->touch_functions & 0xFF);
	regAdd[2] = (u8)(ts->plat_data->touch_functions >> 8);
	ret = ts->stm_ts_write(ts, &regAdd[0], 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to send touch type\n", __func__);
	}

	return ret;
}

void stm_ts_set_cover_type(struct stm_ts_data *ts, bool enable)
{
	int ret;
	u8 regAdd[3] = {0};

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->cover_type);

	ts->cover_cmd = sec_input_check_cover_type(&ts->client->dev) & 0xFF;

	if (enable) {
		regAdd[0] = STM_TS_CMD_SET_GET_COVERTYPE;
		regAdd[1] = ts->cover_cmd;
		ret = ts->stm_ts_write(ts, &regAdd[0], 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to send covertype command: %d",
					__func__, ts->cover_cmd);
		}

		ts->plat_data->touch_functions |= STM_TS_TOUCHTYPE_BIT_COVER;
	} else {
		ts->plat_data->touch_functions &= ~STM_TS_TOUCHTYPE_BIT_COVER;
	}

	stm_ts_set_touch_function(ts);
}

int stm_ts_vbus_charger_mode(struct device *dev, bool on)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	if (on)
		ts->charger_mode = STM_TS_CHARGER_MODE_WIRE_CHARGER;
	else
		ts->charger_mode = STM_TS_CHARGER_MODE_NORMAL;

	return stm_ts_charger_mode(ts);
}

int stm_ts_charger_mode(struct stm_ts_data *ts)
{
	u8 regAdd[2] = {STM_TS_CMD_SET_GET_CHARGER_MODE, ts->charger_mode};
	int ret;

	input_info(true, &ts->client->dev, "%s: Set charger mode CMD[%02X]\n", __func__, regAdd[1]);
	ret = ts->stm_ts_write(ts, regAdd, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	return ret;
}

void stm_ts_change_scan_rate(struct stm_ts_data *ts, u8 rate)
{
	u8 regAdd[2] = {STM_TS_CMD_SET_GET_REPORT_RATE, rate};
	int ret = 0;

	ret = ts->stm_ts_write(ts, &regAdd[0], 2);

	input_dbg(true, &ts->client->dev, "%s: scan rate (%d Hz), ret = %d\n", __func__, regAdd[1], ret);
}

#if 0
void stm_ts_ic_interrupt_set(struct stm_ts_data *ts, int enable)
{
	u8 regAdd[3] = { 0xA4, 0x01, 0x00 };

	if (enable)
		regAdd[2] = 0x01;
	else
		regAdd[2] = 0x00;

	ts->stm_ts_write(ts, &regAdd[0], 3);
	sec_delay(10);
}
#endif

static int stm_ts_read_chip_id(struct stm_ts_data *ts)
{
	u8 regAdd = STM_TS_READ_DEVICE_ID;
	u8 val[5] = {0};
	int ret;

	ret = stm_ts_read(ts, &regAdd, 1, &val[0], 5);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		return -STM_TS_I2C_ERROR;
	}

	input_info(true, &ts->client->dev, "%s: %c %c %02X %02X %02X\n",
			__func__, val[0], val[1], val[2], val[3], val[4]);

	if ((val[2] != STM_TS_ID0) && (val[3] != STM_TS_ID1))
		return -STM_TS_ERROR_INVALID_CHIP_ID;

	return STM_TS_NOT_ERROR;
}

static int stm_ts_read_chip_id_hw(struct stm_ts_data *ts)
{
	u8 regAdd[5] = { 0xFA, 0x20, 0x00, 0x00, 0x00 };
	u8 val[8] = {0};
	int ret;

	ret = stm_ts_read(ts, regAdd, 5, &val[0], 8);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);

	if ((val[0] == STM_TS_ID1) && (val[1] == STM_TS_ID0))
		return STM_TS_NOT_ERROR;

	return -STM_TS_ERROR_INVALID_CHIP_ID;
}

static int stm_ts_wait_for_ready(struct stm_ts_data *ts)
{
	struct stm_ts_event_status *p_event_status;
	int rc = -STM_TS_I2C_ERROR;
	u8 regAdd = STM_TS_READ_ONE_EVENT;
	u8 data[STM_TS_EVENT_SIZE];
	int retry = 0;
	int err_cnt = 0;

	mutex_lock(&ts->wait_for);

	sec_input_irq_disable(ts->plat_data);

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	while (stm_ts_read(ts, &regAdd, 1, (u8 *)data, STM_TS_EVENT_SIZE) > 0) {
		p_event_status = (struct stm_ts_event_status *) &data[0];

		if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFORMATION) &&
				(p_event_status->status_id == STM_TS_INFO_READY_STATUS)) {
			if (rc == -STM_TS_ERROR_BROKEN_OSC_TRIM)
				break;

			rc = STM_TS_NOT_ERROR;
			break;
		}

		if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			input_err(true, &ts->client->dev,
					"%s: Err detected %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

			// check if config / cx / panel configuration area is corrupted
			if (((data[1] >= 0x20) && (data[1] <= 0x21)) || ((data[1] >= 0xA0) && (data[1] <= 0xA8))) {
				rc = -STM_TS_ERROR_FW_CORRUPTION;
				ts->plat_data->hw_param.checksum_result = 1;
				ts->fw_corruption = true;
				input_err(true, &ts->client->dev, "%s: flash corruption:%02X\n",
						__func__, data[1]);
				break;
			}

			/*
			 * F3 24 / 25 / 29 / 2A / 2D / 34: the flash about OSC TRIM value is broken.
			 */
			if (data[1] == 0x24 || data[1] ==  0x25 || data[1] ==  0x29 || data[1] ==  0x2A || data[1] == 0x2D || data[1] == 0x34) {
				input_err(true, &ts->client->dev, "%s: osc trim is broken\n", __func__);
				rc = -STM_TS_ERROR_BROKEN_OSC_TRIM;
				ts->fw_corruption = true;
				ts->osc_trim_error = true;
				//break;
			}

			if (err_cnt++ > 32) {
				rc = -STM_TS_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 15) {
			rc = -STM_TS_ERROR_TIMEOUT;
			if (data[0] == 0 && data[1] == 0 && data[2] == 0)
				rc = -STM_TS_ERROR_TIMEOUT_ZERO;

			input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);

			if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE))
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
			break;
		}
		sec_delay(20);
	}

	input_info(true, &ts->client->dev,
			"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
			__func__, data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);

	sec_input_irq_enable(ts->plat_data);

	mutex_unlock(&ts->wait_for);

	return rc;
}

int stm_ts_systemreset(struct stm_ts_data *ts, unsigned int msec)
{
	u8 regAdd[6] = { 0xFA, 0x20, 0x00, 0x00, 0x24, 0x81 };
	int rc;

	sec_input_irq_disable(ts->plat_data);

	ts->osc_trim_error = false;

	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		goto out;

	sec_delay(msec + 10);

	rc = stm_ts_wait_for_ready(ts);

out:
	input_info(true, &ts->client->dev, "%s: %d\n", __func__, rc);

	stm_ts_release_all_finger(ts);
	sec_input_irq_enable(ts->plat_data);

	return rc;
}

int stm_ts_fw_corruption_check(struct stm_ts_data *ts)
{
	u8 regAdd[6] = { 0xFA, 0x20, 0x00, 0x00, 0x24, 0x81 };
	u8 val = 0;
	int rc;

	sec_input_irq_disable(ts->plat_data);

	/* stm_ts_systemreset */
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0) {
		rc = -STM_TS_I2C_ERROR;
		goto out;
	}
	sec_delay(10);

	/* Firmware Corruption Check */
	regAdd[0] = 0xFA;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	regAdd[3] = 0x00;
	regAdd[4] = 0x78;
	rc = stm_ts_read(ts, regAdd, 5, &val, 1);
	if (rc < 0) {
		rc = -STM_TS_I2C_ERROR;
		goto out;
	}

	if (val & 0x03) { // Check if crc error
		input_err(true, &ts->client->dev, "%s: firmware corruption. CRC status:%02X\n",
				__func__, val & 0x03);
		rc = -STM_TS_ERROR_FW_CORRUPTION;
	} else {
		rc = STM_TS_NOT_ERROR;
	}
out:
	sec_input_irq_enable(ts->plat_data);

	return rc;
}

int stm_ts_get_sysinfo_data(struct stm_ts_data *ts, u8 sysinfo_addr, u8 read_cnt, u8 *data)
{
	int ret;
	u8 *buff = NULL;

	u8 regAdd[3] = { 0xA4, 0x06, 0x01 }; // request system information

	sec_input_irq_disable(ts->plat_data);

	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: timeout wait for event\n", __func__);
		goto ERROR;
	}

	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = sysinfo_addr;

	buff = kzalloc(read_cnt, GFP_KERNEL);
	if (!buff) {
		ret = -ENOMEM;
		goto ERROR;
	}

	ret = stm_ts_read(ts, &regAdd[0], 3, &buff[0], read_cnt);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n",
				__func__, ret);
		goto ERROR;
	}

	memcpy(data, &buff[0], read_cnt);

ERROR:
	if (buff)
		kfree(buff);
	sec_input_irq_enable(ts->plat_data);
	return ret;
}

int stm_ts_get_version_info(struct stm_ts_data *ts)
{
	int rc;
	u8 regAdd = STM_TS_READ_FW_VERSION;
	u8 data[STM_TS_VERSION_SIZE] = { 0 };

	memset(data, 0x0, STM_TS_VERSION_SIZE);

	rc = stm_ts_read(ts, &regAdd, 1, (u8 *)data, STM_TS_VERSION_SIZE);

	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID] = data[6];
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER] = data[7];
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER] = data[8];
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER] = data[4];

	input_info(true, &ts->client->dev,
			"%s: [IC :STM] Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X\n",
			__func__, (data[0] << 8) + data[1], (data[2] << 8) + data[3], data[4] + (data[5] << 8));
	input_info(true, &ts->client->dev,
			"%s: [IC :SEC] IC Ver: 0x%02X, Project ID: 0x%02X, Module Ver: 0x%02X, FW Ver:0x%02X\n",
			__func__, ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER]);

	return rc;
}

int stm_ts_fix_active_mode(struct stm_ts_data *ts, bool enable)
{
	u8 regAdd[3] = {0xA0, 0x00, 0x00};
	int ret;

	if (enable) {
		regAdd[1] = 0x03;
		regAdd[2] = 0x00;
	} else {
		regAdd[1] = 0x00;
		regAdd[2] = 0x01;
	}

	ret = ts->stm_ts_write(ts, &regAdd[0], 3);
	if (ret < 0)
		input_info(true, &ts->client->dev, "%s: err: %d\n", __func__, ret);
	else
		input_info(true, &ts->client->dev, "%s: %s\n", __func__,
				enable ? "fix" : "release");

	sec_delay(10);

	return ret;
}

int stm_ts_get_hf_data(struct stm_ts_data *ts)
{
	u8 regAdd[4] = { 0 };
	int ret;
	short min = 0x7FFF;
	short max = 0x8000;

	ts->stm_ts_systemreset(ts, 0);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	regAdd[0] = 0xA4;
	regAdd[1] = 0x04;
	regAdd[2] = 0xFF;
	regAdd[3] = 0x01;
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 4, 100);
	if (ret < 0)
		goto out;

	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, &min, &max);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get rawdata\n", __func__);

out:
	ts->stm_ts_systemreset(ts, 0);

	stm_ts_set_cover_type(ts, ts->flip_enable);

	sec_delay(10);

	if (ts->charger_mode) {
		stm_ts_charger_mode(ts);
		sec_delay(10);
	}

	stm_ts_set_scanmode(ts, ts->scan_mode);

	return ret;
}

int stm_ts_get_miscal_data(struct stm_ts_data *ts, short *min, short *max)
{
	u8 cmd = STM_TS_READ_ONE_EVENT;
	u8 regAdd[2] = { 0 };
	u8 data[STM_TS_EVENT_SIZE];
	int retry = 200, ret;
	int result = STM_TS_EVENT_ERROR_REPORT;
	short min_print = 0x7FFF;
	short max_print = 0x8000;

	ts->stm_ts_systemreset(ts, 0);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	mutex_lock(&ts->wait_for);

	regAdd[0] = 0xC7;
	regAdd[1] = 0x02;

	ret = ts->stm_ts_write(ts, &regAdd[0], 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: failed to write miscal cmd\n", __func__);
		result = -EIO;
		mutex_unlock(&ts->wait_for);
		goto error;
	}

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	while (retry-- >= 0) {
		memset(data, 0x00, sizeof(data));
		ret = ts->stm_ts_read(ts, &cmd, 1, data, STM_TS_EVENT_SIZE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read failed: %d\n", __func__, ret);
			result = -EIO;
			mutex_unlock(&ts->wait_for);
			goto error;
		}

		if (data[0] != 0x00)
			input_info(true, &ts->client->dev, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

		sec_delay(10);

		if (data[0] == STM_TS_EVENT_PASS_REPORT || data[0] == STM_TS_EVENT_ERROR_REPORT) {
			result = data[0];
			*max = data[3] << 8 | data[2];
			*min = data[5] << 8 | data[4];
			break;
		}

		if (retry == 0) {
			result = -EIO;
			mutex_unlock(&ts->wait_for);
			goto error;
		}
	}
	mutex_unlock(&ts->wait_for);

	input_raw_info(true, &ts->client->dev, "%s: %s %d,%d\n",
			__func__, data[0] == STM_TS_EVENT_PASS_REPORT ? "pass" : "fail", *min, *max);

	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, &min_print, &max_print);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get rawdata\n", __func__);

error:
	stm_ts_reinit(ts, false);

	return result;
}

int stm_ts_osc_trim_recovery(struct stm_ts_data *ts)
{
	u8 regaddr[3];
	int ret;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_input_irq_disable(ts->plat_data);

	/*  OSC trim error recovery command. */
	regaddr[0] = 0xA4;
	regaddr[1] = 0x00;
	regaddr[2] = 0x05;
	ret = stm_ts_wait_for_echo_event(ts, &regaddr[0], 3, 800);
	if (ret < 0) {
		ret = -STM_TS_ERROR_BROKEN_OSC_TRIM;
		goto out;
	}

	/* save panel configuration area */
	regaddr[0] = 0xA4;
	regaddr[1] = 0x05;
	regaddr[2] = 0x04;
	ret = stm_ts_wait_for_echo_event(ts, &regaddr[0], 3, 200);
	if (ret < 0) {
		ret = -STM_TS_ERROR_BROKEN_OSC_TRIM;
		goto out;
	}

	sec_delay(500);
	ret = ts->stm_ts_systemreset(ts, 0);
	sec_delay(50);

out:
	sec_input_irq_enable(ts->plat_data);

	return ret;
}

int stm_ts_set_temperature(struct device *dev, u8 temperature_data)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret = 0;
	u8 regAdd[2] = {0x3C, temperature_data};

	ret = ts->stm_ts_write(ts, regAdd, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to write\n", __func__);
		return SEC_ERROR;
	}

	return SEC_SUCCESS;
}

static int stm_ts_hw_init(struct stm_ts_data *ts)
{
	u8 retry = 3;
	u8 regAdd[8] = { 0 };
	int rc;
	u8 data[STM_TS_EVENT_SIZE] = { 0 };

	rc = stm_ts_read(ts, &regAdd[0], 1, (u8 *)data, STM_TS_EVENT_SIZE);
	if (rc == -ENOTCONN)
		return rc;

	do {
		ts->plat_data->hw_param.checksum_result = 0;
		ts->fw_corruption = false;

		rc = stm_ts_fw_corruption_check(ts);
		if (rc == -STM_TS_ERROR_FW_CORRUPTION) {
			ts->plat_data->hw_param.checksum_result = 1;
			break;
		} else if (rc < 0) {
			goto reset;
		}

		rc = stm_ts_wait_for_ready(ts);
reset:
		if (rc < 0) {
			if (ts->plat_data->hw_param.checksum_result) {
#ifdef CONFIG_SEC_FACTORY
				stm_ts_execute_autotune(ts, true);
				continue;
#else
				break;
#endif
			} else if (ts->osc_trim_error) {
				break;
			} else if (rc == -STM_TS_ERROR_TIMEOUT_ZERO) {
				rc = stm_ts_read_chip_id_hw(ts);
				if (rc == STM_TS_NOT_ERROR) {
					ts->plat_data->hw_param.checksum_result = 1;
					input_err(true, &ts->client->dev,
							"%s: config corruption\n", __func__);
					break;
				}
			}
			stm_ts_reset(ts, 20);
		} else {
			break;
		}
	} while (--retry);

	ts->stm_ts_get_version_info(ts);

	retry = 1;
	do {
		if (ts->osc_trim_error) {
			rc = stm_ts_osc_trim_recovery(ts);
			if (rc < 0)
				input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
		} else {
			break;
		}
	} while (retry--);

	if (!ts->plat_data->hw_param.checksum_result && rc < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to system reset\n", __func__);
		//return -EIO;
	}

	if (ts->plat_data->hw_param.checksum_result)
		memset(ts->plat_data->img_version_of_ic, 0x00, sizeof(ts->plat_data->img_version_of_ic));

	rc = stm_ts_read_chip_id(ts);
	if (rc < 0) {
		stm_ts_reset(ts, 500);	/* Delay to discharge the IC from ESD or On-state.*/

		input_err(true, &ts->client->dev, "%s: Reset caused by chip id error\n", __func__);

		rc = stm_ts_read_chip_id(ts);
		//if (rc < 0)
		//	return 1;
	}

	rc = stm_ts_fw_update_on_probe(ts);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to firmware update\n",
				__func__);
		return rc;
	}

	rc = stm_ts_get_channel_info(ts);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, rc);
		return rc;
	}
	input_info(true, &ts->client->dev, "%s: Sense(%02d) Force(%02d)\n", __func__,
			ts->SenseChannelLength, ts->ForceChannelLength);

	ts->pFrame = devm_kzalloc(&ts->client->dev, ts->SenseChannelLength * ts->ForceChannelLength * 2 + 1, GFP_KERNEL);
	if (!ts->pFrame)
		return -ENOMEM;

	ts->cx_data = devm_kzalloc(&ts->client->dev, ts->SenseChannelLength * ts->ForceChannelLength + 1, GFP_KERNEL);
	if (!ts->cx_data)
		return -ENOMEM;

	ts->vp_cap_data = devm_kzalloc(&ts->client->dev, ts->SenseChannelLength * ts->ForceChannelLength + 1, GFP_KERNEL);
	if (!ts->vp_cap_data)
		return -ENOMEM;

	/* stm driver set functional feature */
	ts->charger_mode = STM_TS_CHARGER_MODE_NORMAL;

#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif

	ts->plat_data->touch_functions = STM_TS_TOUCHTYPE_DEFAULT_ENABLE;
	stm_ts_set_touch_function(ts);
	sec_delay(10);

	ts->stm_ts_command(ts, STM_TS_CMD_FORCE_CALIBRATION, true);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	ts->scan_mode = STM_TS_SCAN_MODE_DEFAULT;

	stm_ts_set_scanmode(ts, ts->scan_mode);

	input_info(true, &ts->client->dev, "%s: resolution: x:%d,y:%d\n",
			__func__, ts->plat_data->max_x, ts->plat_data->max_y);

	input_info(true, &ts->client->dev, "%s: Initialized\n", __func__);

	return 0;
}

static void stm_ts_print_info_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			work_print_info.work);

	if (!ts->probe_done)
		return;

	sec_input_print_info(&ts->client->dev, ts->tdata);

	if (atomic_read(&ts->sec.cmd_is_running))
		input_err(true, &ts->client->dev, "%s: skip set temperature, cmd running\n", __func__);
	else
		sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_NORMAL);

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static const char finger_mode[10] = {'N', '1', '2', 'G', '4', 'P'};

static void stm_ts_status_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_status *p_event_status;

	p_event_status = (struct stm_ts_event_status *)event_buff;

	if (p_event_status->stype > 0)
		input_info(true, &ts->client->dev, "%s: STATUS %02X %02X %02X %02X %02X %02X %02X %02X\n",
				__func__, event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);

	if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_ERROR) &&
			(p_event_status->status_id == STM_TS_ERR_EVENT_QUEUE_FULL)) {
		input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
		stm_ts_release_all_finger(ts);
	}

	if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_ERROR) &&
			(p_event_status->status_id == STM_TS_ERR_EVENT_ESD)) {
		input_err(true, &ts->client->dev, "%s: ESD detected. run reset\n", __func__);
		if (!ts->reset_is_on_going)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
	}

	if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFORMATION) &&
			(p_event_status->status_id == STM_TS_INFO_WET_MODE)) {
		ts->plat_data->wet_mode = p_event_status->status_data_1;

		input_info(true, &ts->client->dev, "%s: WET MODE %s[%d]\n",
				__func__, ts->plat_data->wet_mode == 0 ? "OFF" : "ON",
				p_event_status->status_data_1);

		if (ts->plat_data->wet_mode)
			ts->plat_data->hw_param.wet_count++;
	}

	if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFORMATION) &&
			(p_event_status->status_id == STM_TS_INFO_NOISE_MODE)) {
		atomic_set(&ts->plat_data->touch_noise_status, p_event_status->status_data_1);

		input_info(true, &ts->client->dev, "%s: NOISE MODE %s[%02X]\n",
				__func__, atomic_read(&ts->plat_data->touch_noise_status) == 0 ? "OFF" : "ON",
				p_event_status->status_data_1);

		if (atomic_read(&ts->plat_data->touch_noise_status))
			ts->plat_data->hw_param.noise_count++;
	}
}

static void stm_ts_coord_parsing(struct stm_ts_data *ts, struct stm_ts_event_coordinate *p_event_coord, u8 t_id)
{
	ts->plat_data->coord[t_id].id = t_id;
	ts->plat_data->coord[t_id].action = p_event_coord->tchsta;
	ts->plat_data->coord[t_id].x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p_event_coord->z & 0x3F;
	ts->plat_data->coord[t_id].ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
	ts->plat_data->coord[t_id].major = p_event_coord->major;
	ts->plat_data->coord[t_id].minor = p_event_coord->minor;

	if (!ts->plat_data->coord[t_id].palm && (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM))
		ts->plat_data->coord[t_id].palm_count++;

	ts->plat_data->coord[t_id].palm = (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM);
	if (ts->plat_data->coord[t_id].palm)
		ts->plat_data->palm_flag |= (1 << t_id);
	else
		ts->plat_data->palm_flag &= ~(1 << t_id);

	ts->plat_data->coord[t_id].left_event = p_event_coord->left_event;

	ts->plat_data->coord[t_id].noise_level = max(ts->plat_data->coord[t_id].noise_level,
							p_event_coord->noise_level);
	ts->plat_data->coord[t_id].max_strength = max(ts->plat_data->coord[t_id].max_strength,
							p_event_coord->max_strength);
	ts->plat_data->coord[t_id].noise_status = p_event_coord->noise_status;

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}

static void stm_ts_coordinate_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_coordinate *p_event_coord;
	u8 t_id = 0;

	if (atomic_read(&ts->plat_data->power_state) != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev,
				"%s: device is closed %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
		return;
	}

	p_event_coord = (struct stm_ts_event_coordinate *)event_buff;

	t_id = p_event_coord->tid;

	if (t_id < SEC_TS_SUPPORT_TOUCH_COUNT) {
		ts->plat_data->prev_coord[t_id] = ts->plat_data->coord[t_id];
		stm_ts_coord_parsing(ts, p_event_coord, t_id);

		if ((ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_NORMAL)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_WET)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_GLOVE)) {
			sec_input_coord_event_fill_slot(&ts->client->dev, t_id);
		} else {
			input_err(true, &ts->client->dev,
					"%s: do not support coordinate type(%d)\n",
					__func__, ts->plat_data->coord[t_id].ttype);
		}
	} else {
		input_err(true, &ts->client->dev, "%s: tid(%d) is out of range\n", __func__, t_id);
	}
}

static void stm_ts_gesture_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_gesture_status *p_gesture_status;
	int x, y;

	p_gesture_status = (struct stm_ts_gesture_status *)event_buff;
	input_info(true, &ts->client->dev, "%s: [GESTURE] type:%X sf:%X id:%X | %X, %X, %X, %X\n",
		__func__, p_gesture_status->stype, p_gesture_status->sf, p_gesture_status->gesture_id,
		p_gesture_status->gesture_data_1, p_gesture_status->gesture_data_2,
		p_gesture_status->gesture_data_3, p_gesture_status->gesture_data_4);

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->sf == STM_TS_GESTURE_SAMSUNG_FEATURE) {
		switch (p_gesture_status->stype) {
		case STM_TS_SPONGE_EVENT_SWIPE_UP:
			sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_SPAY, 0, 0);
			break;
		case STM_TS_SPONGE_EVENT_DOUBLETAP:
			if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_AOD) {
				sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
			} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
				input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
				input_sync(ts->plat_data->input_dev);
				input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
				input_info(true, &ts->client->dev, "%s: DOUBLE TAP TO WAKEUP\n", __func__);
			}
			break;
		case STM_TS_SPONGE_EVENT_SINGLETAP:
			sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
			break;
		case STM_TS_SPONGE_EVENT_PRESS:
			if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_LONG ||
					p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_NORMAL) {
				sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_FOD_PRESS, x, y);
				input_info(true, &ts->client->dev, "%s: FOD %sPRESS\n",
						__func__, p_gesture_status->gesture_id ? "" : "LONG");
			} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_RELEASE) {
				sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_FOD_RELEASE, x, y);
				input_info(true, &ts->client->dev, "%s: FOD RELEASE\n", __func__);
			} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_OUT) {
				sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_FOD_OUT, x, y);
				input_info(true, &ts->client->dev, "%s: FOD OUT\n", __func__);
			} else {
				input_info(true, &ts->client->dev, "%s: invalid id %d\n",
						__func__, p_gesture_status->gesture_id);
				break;
			}
			break;
		}
	}
}

static u8 stm_ts_event_handler_type_b(struct stm_ts_data *ts)
{
	u8 regAdd;
	int left_event_count = 0;
	int EventNum = 0;
	u8 event_id = 0;
	u8 data[STM_TS_FIFO_MAX * STM_TS_EVENT_SIZE] = {0};
	u8 *event_buff;

	regAdd = STM_TS_READ_ONE_EVENT;
	stm_ts_read(ts, &regAdd, 1, (u8 *)&data[0 * STM_TS_EVENT_SIZE], STM_TS_EVENT_SIZE);
	left_event_count = (data[7] & 0x3F);

	if (left_event_count >= STM_TS_FIFO_MAX)
		left_event_count = STM_TS_FIFO_MAX - 1;

	if (left_event_count > 0) {
		regAdd = STM_TS_READ_ALL_EVENT;
		stm_ts_read(ts, &regAdd, 1, (u8 *)&data[1 * STM_TS_EVENT_SIZE], STM_TS_EVENT_SIZE * (left_event_count));
	}

	do {
		/* for event debugging */
		if (ts->debug_string & 0x1)
			input_info(true, &ts->client->dev, "[%d] %02X %02X %02X %02X %02X %02X %02X %02X\n",
					EventNum, data[EventNum * STM_TS_EVENT_SIZE+0], data[EventNum * STM_TS_EVENT_SIZE+1],
					data[EventNum * STM_TS_EVENT_SIZE+2], data[EventNum * STM_TS_EVENT_SIZE+3],
					data[EventNum * STM_TS_EVENT_SIZE+4], data[EventNum * STM_TS_EVENT_SIZE+5],
					data[EventNum * STM_TS_EVENT_SIZE+6], data[EventNum * STM_TS_EVENT_SIZE+7]);

		event_buff = (u8 *) &data[EventNum * STM_TS_EVENT_SIZE];
		event_id = event_buff[0] & 0x3;

		switch (event_id) {
		case STM_TS_STATUS_EVENT:
			stm_ts_status_event(ts, event_buff);
			break;
		case STM_TS_COORDINATE_EVENT:
			stm_ts_coordinate_event(ts, event_buff);
			break;
		case STM_TS_GESTURE_EVENT:
			stm_ts_gesture_event(ts, event_buff);
			break;
		case STM_TS_VENDOR_EVENT: // just print message for debugging
			if (event_buff[1] == 0x01) {  // echo event
				input_info(true, &ts->client->dev,
						"%s: echo event %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
						event_buff[0], event_buff[1], event_buff[2],
						event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7]);
			} else {
				input_info(true, &ts->client->dev,
						"%s: %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
						event_buff[0], event_buff[1], event_buff[2],
						event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7]);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				/* do not reset when it is not ship build. because it need to be debugged */
				if ((event_buff[0] == STM_TS_EVENT_ERROR_REPORT) && (event_buff[1] <= 0x05 || (event_buff[1] >= 0x12 && event_buff[1] <= 0x17))) {
					input_err(true, &ts->client->dev, "%s: abnormal error detected, do reset\n", __func__);
					if (!ts->reset_is_on_going)
						schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
				}
#endif
			}
			break;
		default:
			input_info(true, &ts->client->dev,
					"%s: unknown event %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
					event_buff[0], event_buff[1], event_buff[2], event_buff[3],
					event_buff[4], event_buff[5], event_buff[6], event_buff[7]);
			break;
		}

		EventNum++;
		left_event_count--;
	} while (left_event_count >= 0);

	sec_input_coord_event_sync_slot(&ts->client->dev);

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_IN_IRQ);

	return 0;
}

/**
 * stm_ts_interrupt_handler()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t stm_ts_interrupt_handler(int irq, void *handle)
{
	struct stm_ts_data *ts = handle;

	int ret;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		ret = wait_for_completion_interruptible_timeout((&ts->st_interrupt),
				msecs_to_jiffies(10 * MSEC_PER_SEC));
		return IRQ_HANDLED;
	}
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret == SEC_ERROR)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);

	ret = stm_ts_event_handler_type_b(ts);

	mutex_unlock(&ts->eventlock);

	return IRQ_HANDLED;
}

int stm_ts_irq_setting(struct stm_ts_data *ts,
		bool enable)
{
	int retval = 0;

	if (enable) {
		retval = devm_request_threaded_irq(&ts->client->dev, ts->plat_data->irq, NULL,
				stm_ts_interrupt_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				STM_TS_DRV_NAME, ts);
		if (retval < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to create irq thread %d\n",
					__func__, retval);
			return retval;
		}
	} else {
		sec_input_irq_disable(ts->plat_data);
	}

	return retval;
}

static void stm_ts_parse_dt(struct device *dev, struct stm_ts_data *ts)
{
	struct device_node *np = dev->of_node;

	if (!np) {
		input_err(true, dev, "%s: of_node is not exist\n", __func__);
		return;
	}

	if (of_property_read_u32(np, "stm,lpmode_change_delay", &ts->lpmode_change_delay))
		ts->lpmode_change_delay = 5;

	input_info(true, dev, "%s: lpmode_change_delay:%d\n", __func__, ts->lpmode_change_delay);
}

static int stm_ts_init(struct i2c_client *client)
{
	int retval = 0;
	struct sec_ts_plat_data *pdata;
	struct stm_ts_data *ts;
	struct sec_tclm_data *tdata = NULL;

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			input_err(true, &client->dev, "%s: Failed to allocate platform data\n", __func__);
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		retval = sec_input_parse_dt(&client->dev);
		if (retval) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			return retval;
		}

		tdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata)
			return -ENOMEM;

#ifdef TCLM_CONCEPT
		sec_tclm_parse_dt(&client->dev, tdata);
#endif
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		input_err(true, &client->dev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "%s: could not get pinctrl\n", __func__);
	else
		pdata->pinctrl_configure = sec_input_pinctrl_configure;

	pdata->power = sec_input_power;

	ts = devm_kzalloc(&client->dev, sizeof(struct stm_ts_data), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	stm_ts_parse_dt(&client->dev, ts);

	client->irq = gpio_to_irq(pdata->irq_gpio);

	ts->client = client;
	ts->plat_data = pdata;
	ts->plat_data->irq = client->irq;
	ts->stop_device = stm_ts_stop_device;
	ts->start_device = stm_ts_start_device;
	ts->stm_ts_command = stm_ts_command;
	ts->stm_ts_read = stm_ts_read;
	ts->stm_ts_write = stm_ts_write;
	ts->stm_ts_systemreset = stm_ts_systemreset;
	ts->stm_ts_get_version_info = stm_ts_get_version_info;
	ts->stm_ts_get_sysinfo_data = stm_ts_get_sysinfo_data;
	ts->stm_ts_wait_for_ready = stm_ts_wait_for_ready;
	ts->stm_ts_read_from_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_to_sponge = stm_ts_write_to_sponge;
	ts->plat_data->set_temperature = stm_ts_set_temperature;
	ts->plat_data->set_grip_data = stm_ts_set_grip_data_to_ic;
	if (ts->plat_data->support_vbus_notifier)
		ts->plat_data->set_charger_mode = stm_ts_vbus_charger_mode;

	ts->tdata = tdata;
	if (!ts->tdata) {
		input_err(true, &client->dev, "%s: No tclm data found\n", __func__);
		return -EINVAL;
	}

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(ts->tdata);
	ts->tdata->dev = &ts->client->dev;
	ts->tdata->tclm_read = stm_tclm_data_read;
	ts->tdata->tclm_write = stm_tclm_data_write;
	ts->tdata->tclm_execute_force_calibration = stm_ts_tclm_execute_force_calibration;
	ts->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif

	INIT_DELAYED_WORK(&ts->reset_work, stm_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, stm_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, stm_ts_print_info_work);

	i2c_set_clientdata(client, ts);

	return retval;
}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int stm_touch_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct stm_ts_data *ts = container_of(n, struct stm_ts_data, stm_input_nb);
	int ret = 0;
	u8 regAdd[3] = { STM_TS_CMD_SET_FUNCTION_ONOFF, STM_TS_FUNCTION_SET_HOVER_DETECTION, 0 };

	if (atomic_read(&ts->fw_update_is_running)) {
		input_info(true, &ts->client->dev, "%s: skip, because fw update is running\n", __func__);
		return ret;
	}

	if (atomic_read(&ts->plat_data->shutdown_called))
		return -ENODEV;

	switch (data) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		regAdd[2] = 1;
		ret = stm_ts_write(ts, regAdd, 3);
		input_info(true, &ts->client->dev, "%s: pen hover in detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		regAdd[2] = 0;
		ret = stm_ts_write(ts, regAdd, 3);
		input_info(true, &ts->client->dev, "%s: pen hover out detect, ret=%d\n", __func__, ret);
		break;
	default:
		break;
	}
	return ret;
}
#endif

static int stm_ts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	int retval;
	struct stm_ts_data *ts = NULL;

	input_info(true, &client->dev, "%s: start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: STM_TS err = EIO!\n", __func__);
		return -EIO;
	}

	/* Build up driver data */
	retval = stm_ts_init(client);
	if (retval < 0) {
		input_err(true, &client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}

	ts = (struct stm_ts_data *)i2c_get_clientdata(client);
	if (!ts) {
		input_err(true, &client->dev, "%s: Failed to get driver data\n", __func__);
		retval = -ENODEV;
		goto err_get_drv_data;
	}
	i2c_set_clientdata(client, ts);

	ts->probe_done = false;

	if (ts->plat_data->pinctrl_configure)
		ts->plat_data->pinctrl_configure(&ts->client->dev, true);
	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, true);
	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);

	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->sponge_mutex);
	mutex_init(&ts->wait_for);

	retval = stm_ts_hw_init(ts);
	if (retval < 0) {
		input_err(true, &ts->client->dev, "%s: stm_ts_hw_init fail!\n", __func__);
		goto err_stm_ts_hw_init;
	}

	mutex_lock(&ts->device_mutex);
	ts->reinit_done = true;
	mutex_unlock(&ts->device_mutex);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	retval = sec_input_device_register(&client->dev, ts);
	if (retval) {
		input_err(true, &ts->client->dev, "%s: sec_input_device_register fail!\n", __func__);
		goto err_register_input;
	}

	mutex_init(&ts->plat_data->enable_mutex);
	ts->plat_data->enable = stm_ts_enable;
	ts->plat_data->disable = stm_ts_disable;

	retval = stm_ts_irq_setting(ts, true);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}

	retval = stm_ts_fn_init(ts);
	if (retval) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	for (i = 0; i < (int)ARRAY_SIZE(attrs); i++) {
		retval = sysfs_create_file(&ts->plat_data->input_dev->dev.kobj,
				&attrs[i].attr);
		if (retval < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
		}
	}

	secure_touch_init(ts);

	sec_secure_touch_register(ts, &ts->client->dev, 1, &ts->plat_data->input_dev->dev.kobj);

#endif
	ts->plat_data->sec_ws = wakeup_source_register(NULL, "tsp");

	stm_ts_check_custom_library(ts);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	sec_input_dumpkey_register(MULTI_DEV_NONE, tsp_dump, &ts->client->dev);
	INIT_DELAYED_WORK(&ts->debug_work, dump_tsp_rawdata);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	ts->ss_drv = sec_secure_touch_register(ts, &ts->client->dev, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);
#endif
	sec_input_register_vbus_notifier(&ts->client->dev);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->stm_input_nb, stm_touch_notify_call, 1);
#endif

	ts->probe_done = true;
	atomic_set(&ts->plat_data->enabled, 1);
	input_info(true, &ts->client->dev, "%s: done\n", __func__);
	input_log_fix();

	stm_ts_tool_proc_init(ts);

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(100));

	sec_cmd_send_event_to_user(&ts->sec, NULL, "RESULT=PROBE_DONE");
	return 0;

err_fn_init:
	stm_ts_irq_setting(ts, false);
err_enable_irq:
	ts->plat_data->enable = NULL;
	ts->plat_data->disable = NULL;
err_register_input:
	wakeup_source_unregister(ts->plat_data->sec_ws);

err_stm_ts_hw_init:
	mutex_destroy(&ts->sponge_mutex);
	mutex_destroy(&ts->device_mutex);
	mutex_destroy(&ts->i2c_mutex);
	mutex_destroy(&ts->eventlock);
	mutex_destroy(&ts->wait_for);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, false);
	if (ts->plat_data->pinctrl_configure)
		ts->plat_data->pinctrl_configure(&ts->client->dev, false);

	if (gpio_is_valid(ts->plat_data->irq_gpio))
		gpio_free(ts->plat_data->irq_gpio);

err_get_drv_data:
err_setup_drv_data:
	input_err(true, &client->dev, "%s: failed(%d)\n", __func__, retval);
	input_log_fix();
	return retval;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
static void stm_ts_remove(struct i2c_client *client)
#else
static int stm_ts_remove(struct i2c_client *client)
#endif
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int i = 0;
#endif

	input_info(true, &ts->client->dev, "%s\n", __func__);
	ts->plat_data->enable = NULL;
	ts->plat_data->disable = NULL;
	mutex_lock(&ts->plat_data->enable_mutex);
	atomic_set(&ts->plat_data->shutdown_called, 1);
	mutex_unlock(&ts->plat_data->enable_mutex);

	sec_input_unregister_vbus_notifier(&ts->client->dev);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&ts->stm_input_nb);
#endif
	sec_input_irq_disable_nosync(ts->plat_data);
	free_irq(ts->client->irq, ts);

	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->debug_work);
	sec_input_dumpkey_unregister(MULTI_DEV_NONE);
#endif

	wakeup_source_unregister(ts->plat_data->sec_ws);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	for (i = 0; i < (int)ARRAY_SIZE(attrs); i++) {
		sysfs_remove_file(&ts->plat_data->input_dev->dev.kobj,
				&attrs[i].attr);
	}
#endif

	stm_ts_fn_remove(ts);

	stm_ts_tool_proc_remove();

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, false);
	if (ts->plat_data->pinctrl_configure)
		ts->plat_data->pinctrl_configure(&ts->client->dev, false);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
	return 0;
#endif
}

static int stm_ts_enable(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int retval;

	if (!ts->probe_done) {
		input_dbg(true, &ts->client->dev, "%s: not probe\n", __func__);
		goto out;
	}

	if (!ts->info_work_done) {
		input_err(true, &ts->client->dev, "%s: not finished info work\n", __func__);
		goto out;
	}

	input_dbg(false, &ts->client->dev, "%s\n", __func__);

	retval = stm_ts_start_device(ts);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to start device\n", __func__);
		goto out;
	}

	atomic_set(&ts->plat_data->enabled, 1);

	if (ts->fix_active_mode)
		stm_ts_fix_active_mode(ts, true);

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);
out:
	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_work(&ts->work_print_info.work);

	sec_input_irq_enable(ts->plat_data);

	return 0;
}

static int stm_ts_disable(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	if (!ts->probe_done || atomic_read(&ts->plat_data->shutdown_called)) {
		input_dbg(false, &ts->client->dev, "%s: not probe\n", __func__);
		return 0;
	}

	if (!ts->info_work_done) {
		input_err(true, &ts->client->dev, "%s: not finished info work\n", __func__);
		return 0;
	}

	input_dbg(false, &ts->client->dev, "%s\n", __func__);

	cancel_delayed_work(&ts->reset_work);
	cancel_delayed_work(&ts->work_print_info);

	if (ts->touch_aging_mode) {
		ts->touch_aging_mode = false;
		stm_ts_reinit(ts, false);
	}

	sec_input_print_info(&ts->client->dev, ts->tdata);

	if (ts->plat_data->prox_power_off)
		stm_ts_stop_device(ts, 0);
	else
		stm_ts_stop_device(ts, ts->plat_data->lowpower_mode || ts->plat_data->fod_lp_mode);
	atomic_set(&ts->plat_data->enabled, 0);
	ts->plat_data->prox_power_off = 0;
	ts->fw_corruption = false;
	ts->osc_trim_error = false;

	return 0;
}

void stm_ts_reinit(struct stm_ts_data *ts, bool delay)
{
	u8 retry = 3;
	int rc = 0;

	if (delay) {
		rc = stm_ts_wait_for_ready(ts);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to wait for ready\n", __func__);
			return;
		}
		rc = stm_ts_read_chip_id(ts);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read chip id\n", __func__);
			return;
		}
	}

	do {
		rc = stm_ts_systemreset(ts, 0);
		if (rc < 0)
			stm_ts_reset(ts, 20);
		else
			break;
	} while (--retry);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: Failed to system reset\n", __func__);
		return;
	}

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	stm_ts_release_all_finger(ts);

	stm_ts_set_cover_type(ts, ts->flip_enable);

	if (ts->charger_mode)
		stm_ts_charger_mode(ts);

	stm_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

	/* because edge and dead zone will recover soon */
	sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);

	if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS) {
		stm_ts_set_fod_rect(ts);
		stm_ts_set_press_property(ts);
		stm_ts_set_fod_finger_merge(ts);
	}

	sec_delay(50);

	stm_ts_set_scanmode(ts, ts->scan_mode);
}

void stm_ts_release_all_finger(struct stm_ts_data *ts)
{
	sec_input_release_all_finger(&ts->client->dev);
}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
static void dump_tsp_rawdata(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			debug_work.work);
	int i;

	if (ts->rawdata_read_lock) {
		input_err(true, &ts->client->dev, "%s: ignored ## already checking..\n", __func__);
		return;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: ignored ## IC is power off\n", __func__);
		return;
	}

	ts->rawdata_read_lock = true;
	input_info(true, &ts->client->dev, "%s: ## run CX data ##, %d\n", __func__, __LINE__);
	run_cx_data_read((void *)&ts->sec);

	for (i = 0; i < 5; i++) {
		input_info(true, &ts->client->dev, "%s: ## run Raw Cap data ##, %d\n", __func__, __LINE__);
		run_rawcap_read((void *)&ts->sec);

		input_info(true, &ts->client->dev, "%s: ## run Delta ##, %d\n", __func__, __LINE__);
		run_delta_read((void *)&ts->sec);
		sec_delay(50);
	}
	input_info(true, &ts->client->dev, "%s: ## Done ##, %d\n", __func__, __LINE__);

	ts->rawdata_read_lock = false;
}

static void tsp_dump(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	if (!ts)
		return;

	input_info(true, dev, "%s: start\n", __func__);

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->debug_work, msecs_to_jiffies(100));
}
#endif

static void stm_ts_reset(struct stm_ts_data *ts, unsigned int ms)
{
	input_info(true, &ts->client->dev, "%s: Recover IC, discharge time:%d\n", __func__, ms);

	sec_input_irq_disable(ts->plat_data);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, false);

	sec_delay(ms);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, true);

	sec_delay(5);

	sec_input_irq_enable(ts->plat_data);
}

static void stm_ts_reset_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			reset_work.work);

	if (ts->debug_string & STM_TS_DEBUG_SEND_UEVENT)
		sec_cmd_send_event_to_user(&ts->sec, NULL, "RESULT=RESET");

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		input_err(true, &ts->client->dev, "%s: secure touch enabled\n",
				__func__);
		return;
	}
#endif

	input_info(true, &ts->client->dev, "%s: Call Power-Off to recover IC\n", __func__);
	ts->reset_is_on_going = true;

	mutex_lock(&ts->plat_data->enable_mutex);
	__pm_stay_awake(ts->plat_data->sec_ws);

	stm_ts_stop_device(ts, false);

	sec_delay(100);	/* Delay to discharge the IC from ESD or On-state.*/
	if (stm_ts_start_device(ts) < 0)
		input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);

	if (!atomic_read(&ts->plat_data->enabled)) {
		input_err(true, &ts->client->dev, "%s: call input_close\n", __func__);

		stm_ts_stop_device(ts, sec_input_need_ic_off(ts->plat_data));

		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			stm_ts_set_aod_rect(ts);
	}

	ts->reset_is_on_going = false;
	mutex_unlock(&ts->plat_data->enable_mutex);
	__pm_relax(ts->plat_data->sec_ws);
}

static void stm_ts_read_info_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			work_read_info.work);
	short minval = 0x7FFF;
	short maxval = 0x8000;
	int ret;

	input_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef TCLM_CONCEPT
	ret = sec_tclm_check_cal_case(ts->tdata);
	input_info(true, &ts->client->dev, "%s: sec_tclm_check_cal_case ret: %d\n", __func__, ret);
#endif

	ret = stm_ts_get_tsp_test_result(ts);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get result\n",
				__func__);

	input_raw_info(true, &ts->client->dev, "%s: fac test result %02X\n",
				__func__, ts->test_result.data[0]);

	stm_ts_panel_ito_test(ts, OPEN_SHORT_CRACK_TEST);

	stm_ts_read_frame(ts, TYPE_BASELINE_DATA, &minval, &maxval);
	stm_ts_get_hf_data(ts);
	stm_ts_get_miscal_data(ts, &minval, &maxval);

	ts->info_work_done = true;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
		return;
	}

	schedule_work(&ts->work_print_info.work);

	input_info(true, &ts->client->dev, "%s done\n", __func__);
}

void stm_ts_set_utc_sponge(struct stm_ts_data *ts)
{
	struct timespec64 current_time;
	u8 data[4];

	ktime_get_real_ts64(&current_time);
	data[0] = (u8)(current_time.tv_sec >> 0 & 0xFF);
	data[1] = (u8)(current_time.tv_sec >> 8 & 0xFF);
	data[2] = (u8)(current_time.tv_sec >> 16 & 0xFF);
	data[3] = (u8)(current_time.tv_sec >> 24 & 0xFF);

	stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_OFFSET_UTC, data, sizeof(data));
}

int stm_ts_stop_device(struct stm_ts_data *ts, bool lpmode)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#endif
	mutex_lock(&ts->device_mutex);

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(ts->tdata);
#endif

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	atomic_set(&ts->plat_data->touch_noise_status, 0);
	ts->plat_data->wet_mode = 0;

	if (lpmode) {
		input_info(true, &ts->client->dev, "%s: lowpower flag:0x%02X\n", __func__, ts->plat_data->lowpower_mode);

		if (ts->fix_active_mode)
			stm_ts_fix_active_mode(ts, false);

		enable_irq_wake(ts->plat_data->irq);

		stm_ts_set_opmode(ts, STM_TS_OPMODE_LOWPOWER);

		stm_ts_release_all_finger(ts);

		stm_ts_set_scanmode(ts, ts->scan_mode);

		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_LPM);

		stm_ts_set_utc_sponge(ts);

		ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_OFFSET_MODE,
				&ts->plat_data->lowpower_mode, sizeof(ts->plat_data->lowpower_mode));
	} else {
		sec_input_irq_disable(ts->plat_data);
		stm_ts_release_all_finger(ts);

		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_OFF);

		if (ts->plat_data->power)
			ts->plat_data->power(&ts->client->dev, false);
		if (ts->plat_data->pinctrl_configure)
			ts->plat_data->pinctrl_configure(&ts->client->dev, false);
	}
out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int stm_ts_start_device(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s%s\n", __func__,
			sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE) ? ": exit low power mode" : "");

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#endif
	mutex_lock(&ts->device_mutex);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWERON)) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	stm_ts_release_all_finger(ts);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		if (ts->plat_data->pinctrl_configure)
			ts->plat_data->pinctrl_configure(&ts->client->dev, true);
		if (ts->plat_data->power)
			ts->plat_data->power(&ts->client->dev, true);

		ts->reinit_done = false;
		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);
		stm_ts_reinit(ts, true);
		ts->reinit_done = true;
	} else {
		int ret;

		ret = stm_ts_set_opmode(ts, STM_TS_OPMODE_NORMAL);
		if (ret < 0) {
			ts->reinit_done = false;
			stm_ts_reinit(ts, false);
			ts->reinit_done = true;
		}

		disable_irq_wake(ts->plat_data->irq);
	}
	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);

	ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_OFFSET_MODE,
			&ts->plat_data->lowpower_mode, sizeof(ts->plat_data->lowpower_mode));

	stm_ts_charger_mode(ts);
out:
	mutex_unlock(&ts->device_mutex);

	return 0;
}

static void stm_ts_shutdown(struct i2c_client *client)
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	stm_ts_remove(client);
}

#if IS_ENABLED(CONFIG_PM)
static int stm_ts_pm_suspend(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	input_dbg(true, &ts->client->dev, "%s\n", __func__);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int stm_ts_pm_resume(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	input_dbg(true, &ts->client->dev, "%s\n", __func__);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif

static const struct i2c_device_id stm_ts_id[] = {
	{STM_TS_DRV_NAME, 0},
	{}
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops stm_ts_dev_pm_ops = {
	.suspend = stm_ts_pm_suspend,
	.resume = stm_ts_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id stm_ts_match_table[] = {
	{.compatible = "stm,stm_ts_fst1ba90a",},
	{},
};
#else
#define stm_ts_match_table NULL
#endif

static struct i2c_driver stm_ts_driver = {
	.driver = {
		.name = STM_TS_DRV_NAME,
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = stm_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &stm_ts_dev_pm_ops,
#endif
	},
	.probe = stm_ts_probe,
	.remove = stm_ts_remove,
	.shutdown = stm_ts_shutdown,
	.id_table = stm_ts_id,
};

static int __init stm_ts_driver_init(void)
{
	sec_input_init();
	return i2c_add_driver(&stm_ts_driver);
}

static void __exit stm_ts_driver_exit(void)
{
	sec_input_exit();
	i2c_del_driver(&stm_ts_driver);
}

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL v2");

module_init(stm_ts_driver_init);
module_exit(stm_ts_driver_exit);
