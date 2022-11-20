/* drivers/input/sec_input/melfas/melfas_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "melfas_dev.h"
#include "melfas_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
irqreturn_t secure_filter_interrupt(struct melfas_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is on going because i2c fail\n", __func__);
			return -EBUSY;
		}

		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		melfas_ts_unlocked_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->adapter->dev.parent) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->melfas_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->adapter->dev.parent);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		melfas_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->melfas_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

int secure_touch_init(struct melfas_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct melfas_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

int melfas_stui_tsp_enter(void)
{
	struct melfas_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->melfas_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->client->irq);
	melfas_ts_unlocked_release_all_finger(ts);

	ret = stui_i2c_lock(ts->client->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->melfas_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		enable_irq(ts->client->irq);
		return -1;
	}

	return 0;
}

int melfas_stui_tsp_exit(void)
{
	struct melfas_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_i2c_unlock(ts->client->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	enable_irq(ts->client->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->melfas_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

int melfas_stui_tsp_type(void)
{
	struct stm_ts_data *ts = dev_get_drvdata(ptsp);
	int tsp_vendor = STUI_TSP_TYPE_MELFAS;

	return tsp_vendor;
}
#endif

int melfas_ts_i2c_write(struct melfas_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;
	int i;
	const int len_max = 0xffff;
	u8 *buf;
	u8 *buff;

	if (len + 1 > len_max) {
		input_err(true, &ts->client->dev,
				"%s: The i2c buffer size is exceeded.\n", __func__);
		return -ENOMEM;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, reg, cnum);

	buff = kzalloc(len + cnum, GFP_KERNEL);
	if (!buff) {
		kfree(buff);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	memcpy(buff, buf, cnum);
	memcpy(buff + cnum, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0 | I2C_M_DMA_SAFE;
	msg.len = len + cnum;
	msg.buf = buff;

	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		ret = i2c_transfer(ts->client->adapter, &msg, 1);
		if (ret == 1)
			break;

		if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}

		usleep_range(1 * 1000, 1 * 1000);

		if (retry > 1) {
			char result[32];
			input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n", __func__, retry + 1, ret);
			ts->plat_data->hw_param.comm_err_count++;

			snprintf(result, sizeof(result), "RESULT=I2C");
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		pr_info("sec_input:i2c_cmd: W:");
		for (i = 0; i < cnum; i++)
			pr_cont("%02X ", reg[i]);
		pr_cont(" | ");
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	if (ret == 1) {
		kfree(buf);
		return 0;
	}
err:
	kfree(buf);
	return -EIO;
}

int melfas_ts_i2c_read(struct melfas_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];
	int remain = len;
	int i;
	u8 *buff;
	u8 *buf;

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	
	memcpy(buf, reg, cnum);

	buff = kzalloc(len, GFP_KERNEL);
	if (!buff) {
		kfree(buf);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0 | I2C_M_DMA_SAFE;
	msg[0].len = cnum;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD | I2C_M_DMA_SAFE;
	msg[1].buf = buff;

	mutex_lock(&ts->i2c_mutex);
	if (len <= ts->plat_data->i2c_burstmax) {
		msg[1].len = len;
		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 2);
			if (ret == 2)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}

			if (retry > 1) {
				char result[32];
				input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
					__func__, retry + 1, ret);
				ts->plat_data->hw_param.comm_err_count++;

				snprintf(result, sizeof(result), "RESULT=I2C");
				sec_cmd_send_event_to_user(&ts->sec, NULL, result);
			}
		}
	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */
		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}

			if (retry > 1) {
				input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
					__func__, retry + 1, ret);
				ts->plat_data->hw_param.comm_err_count++;
			}
		}

		do {
			if (remain > ts->plat_data->i2c_burstmax)
				msg[1].len = ts->plat_data->i2c_burstmax;
			else
				msg[1].len = remain;

			remain -= ts->plat_data->i2c_burstmax;

			for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg[1], 1);
				if (ret == 1)
					break;
				usleep_range(1 * 1000, 1 * 1000);
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
					input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
					mutex_unlock(&ts->i2c_mutex);
					goto err;
				}

				if (retry > 1) {
					input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
						__func__, retry + 1, ret);
					ts->plat_data->hw_param.comm_err_count++;
				}
			}
			msg[1].buf += msg[1].len;
		} while (remain > 0);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	memcpy(data, buff, len);
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		pr_info("sec_input:i2c_cmd: R:");
		for (i = 0; i < cnum; i++)
			pr_cont("%02X ", reg[i]);
		pr_cont(" | ");
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	kfree(buf);
	kfree(buff);
	return ret;
err:
	kfree(buf);
	kfree(buff);
	return -EIO;
}

void melfas_ts_reinit(void *data) 
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)data;

	input_info(true, &ts->client->dev,
		"%s: charger=0x%x, touch_functions=0x%x, Power mode=0x%x\n",
		__func__, ts->plat_data->wirelesscharger_mode, ts->plat_data->touch_functions, ts->plat_data->power_state);

	ts->plat_data->touch_noise_status = 0;
	ts->plat_data->touch_pre_noise_status = 0;
	ts->plat_data->wet_mode = 0;

	melfas_ts_unlocked_release_all_finger(ts);

	if (ts->disable_esd == true)
		melfas_ts_disable_esd_alert(ts);

#ifdef CONFIG_VBUS_NOTIFIER
	if (ts->ta_stsatus)
		melfas_ts_charger_attached(ts, true);
#endif
	melfas_ts_set_cover_type(ts, ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_COVER);

	melfas_ts_set_custom_library(ts);
	melfas_ts_set_press_property(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		melfas_ts_set_fod_rect(ts);

	/* Power mode */
	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		melfas_ts_set_lowpowermode(ts, SEC_INPUT_STATE_LPM);
		sec_delay(50);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			melfas_ts_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
	}

	if (ts->plat_data->ed_enable)
		melfas_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ts->plat_data->pocket_mode)
		melfas_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
	if (ts->glove_enable)
		melfas_ts_set_glove_mode(ts, ts->glove_enable);	
}

int melfas_ts_input_open(struct input_dev *dev)
{
	struct melfas_ts_data *ts = input_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&ts->work_read_info);

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = true;
	ts->plat_data->prox_power_off = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_TOUCH_MODE);
#ifdef CONFIG_VBUS_NOTIFIER
		if (ts->ta_stsatus)
			melfas_ts_charger_attached(ts, true);
#endif
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
	} else {
		ret = ts->plat_data->start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

	mutex_unlock(&ts->modechange);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!ts->plat_data->shutdown_called)
		schedule_work(&ts->work_print_info.work);
	return 0;
}

void melfas_ts_input_close(struct input_dev *dev)
{
	struct melfas_ts_data *ts = input_get_drvdata(dev);

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = false;
	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, ts->tdata);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_cancel_session();
#endif

	cancel_delayed_work(&ts->reset_work);

	if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode)
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
	else
		ts->plat_data->stop_device(ts);

	mutex_unlock(&ts->modechange);
}

int melfas_ts_stop_device(void *data)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)data;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->client->irq);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;

	melfas_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->pinctrl_configure(&ts->client->dev, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int melfas_ts_start_device(void *data)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)data;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		ret = -1;
		goto out;
	}

	melfas_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, true);

	sec_delay(90);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	ts->plat_data->touch_noise_status = 0;

	ts->plat_data->init(ts);

	enable_irq(ts->client->irq);
out:
	mutex_unlock(&ts->device_mutex);
	return ret;
}

static int melfas_ts_hw_init(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);
	ts->plat_data->power(&ts->client->dev, true);
	if (!ts->plat_data->regulator_boot_on)
		sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

	ret = melfas_ts_firmware_update_on_probe(ts, false, true);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to firmware update\n",
				__func__);
		return ret;
	}

	ret = melfas_ts_init_config(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to init config\n",
				__func__);
		return ret;
	}

	ts->print_buf = devm_kzalloc(&client->dev, sizeof(u8) * 4096, GFP_KERNEL);
	ts->image_buf =	devm_kzalloc(&client->dev, sizeof(int) * ((ts->plat_data->x_node_num * ts->plat_data->y_node_num)),
			GFP_KERNEL);

#ifdef USE_TSP_TA_CALLBACKS
	ts->register_cb = melfas_ts_register_callback;
	ts->callbacks.inform_charger = melfas_ts_charger_status_cb;
	if (ts->register_cb)
		ts->register_cb(&ts->callbacks);
#endif
#ifdef CONFIG_VBUS_NOTIFIER
	vbus_notifier_register(&ts->vbus_nb, melfas_ts_vbus_notification,
				VBUS_NOTIFY_DEV_CHARGER);
#endif

	input_info(true, &ts->client->dev, "%s: Initialized\n", __func__);

	return ret;
}

static int melfas_ts_init(struct i2c_client *client)
{
	struct melfas_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			ret = -ENOMEM;
			goto error_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			ret = -ENOMEM;
			input_err(true, &client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "%s: could not get pinctrl\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}

	client->irq = gpio_to_irq(pdata->irq_gpio);

	ts->client = client;
	ts->plat_data = pdata;
	ts->tdata = NULL;
	ts->melfas_ts_i2c_read = melfas_ts_i2c_read;
	ts->melfas_ts_i2c_write = melfas_ts_i2c_write;
	ts->melfas_ts_read_sponge = melfas_ts_read_from_sponge;
	ts->melfas_ts_write_sponge = melfas_ts_write_to_sponge;

	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->start_device = melfas_ts_start_device;
	ts->plat_data->stop_device = melfas_ts_stop_device;
	ts->plat_data->init = melfas_ts_reinit;
	ts->plat_data->lpmode = melfas_ts_set_lowpowermode;
	ts->plat_data->set_grip_data = melfas_set_grip_data_to_ic;

	ptsp = &client->dev;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = melfas_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = melfas_stui_tsp_exit;	
	ts->plat_data->stui_tsp_type = melfas_stui_tsp_type;
#endif

	INIT_DELAYED_WORK(&ts->reset_work, melfas_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, melfas_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, melfas_ts_print_info_work);

	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->modechange);
	mutex_init(&ts->sponge_mutex);

	ts->plat_data->sec_ws = wakeup_source_register(&ts->client->dev, "tsp");
	device_init_wakeup(&client->dev, true);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	i2c_set_clientdata(client, ts);

	ret = sec_input_device_register(&client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ts->plat_data->input_dev->open = melfas_ts_input_open;
	ts->plat_data->input_dev->close = melfas_ts_input_close;

	ret = melfas_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);
#endif
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = melfas_ts_dump_tsp_log;
	INIT_DELAYED_WORK(&ts->check_rawdata, melfas_ts_check_rawdata);
#endif
	input_info(true, &client->dev, "%s: init resource\n", __func__);

	return 0;

err_fn_init:
err_register_input_device:
	wakeup_source_unregister(ts->plat_data->sec_ws);
error_allocate_mem:
	regulator_put(pdata->dvdd);
	regulator_put(pdata->avdd);
error_allocate_pdata:
	input_err(true, &client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void melfas_ts_release(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->reset_work);
	flush_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->check_rawdata);
	dump_callbacks.inform_dump = NULL;
#endif
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	melfas_ts_sysfs_remove(ts);
#endif
	melfas_ts_fn_remove(ts);
	device_init_wakeup(&client->dev, false);
	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;
	ts->probe_done = false;

	ts->plat_data->power(&ts->client->dev, false);

	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
}


int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: EIO err!\n", __func__);
		return -EIO;
	}

	ret = melfas_ts_init(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ts = i2c_get_clientdata(client);

	ret = melfas_ts_hw_init(client);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		melfas_ts_release(client);
		return ret;
	}

	melfas_ts_get_custom_library(ts);
	melfas_ts_set_custom_library(ts);

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, client->irq);
	ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, MELFAS_TS_I2C_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		melfas_ts_release(client);
		return ret;
	}

	ts->probe_done = true;
	ts->plat_data->enabled = true;


	input_err(true, &ts->client->dev, "%s: done\n", __func__);
	input_log_fix();

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

	return 0;
}

int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->modechange);
	ts->plat_data->shutdown_called = true;
	mutex_unlock(&ts->modechange);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	melfas_ts_release(client);

	return 0;
}

void melfas_ts_shutdown(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	melfas_ts_remove(client);
}


#if IS_ENABLED(CONFIG_PM)
static int melfas_ts_pm_suspend(struct device *dev)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int melfas_ts_pm_resume(struct device *dev)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif


static const struct i2c_device_id melfas_ts_id[] = {
	{ MELFAS_TS_I2C_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops melfas_ts_dev_pm_ops = {
	.suspend = melfas_ts_pm_suspend,
	.resume = melfas_ts_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id melfas_ts_match_table[] = {
	{ .compatible = "melfas,melfas_ts",},
	{ },
};
#else
#define melfas_ts_match_table NULL
#endif

static struct i2c_driver melfas_ts_driver = {
	.probe		= melfas_ts_probe,
	.remove		= melfas_ts_remove,
	.shutdown	= melfas_ts_shutdown,
	.id_table	= melfas_ts_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MELFAS_TS_I2C_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = melfas_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &melfas_ts_dev_pm_ops,
#endif
	},
};

module_i2c_driver(melfas_ts_driver);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_DESCRIPTION("melfas TouchScreen driver");
MODULE_LICENSE("GPL");
