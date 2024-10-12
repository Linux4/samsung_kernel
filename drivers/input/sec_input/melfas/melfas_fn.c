/* drivers/input/sec_input/melfas/melfas_fn.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "melfas_dev.h"
#include "melfas_reg.h"

#ifdef USE_TSP_TA_CALLBACKS
static bool ta_connected;
#endif

int melfas_ts_read_from_sponge(struct melfas_ts_data *ts, u8 *data, int len)
{	
	int ret = 0;
	u8 rbuf[2] = {0, };
	u16 mip4_addr = 0;

	mutex_lock(&ts->sponge_mutex);
	mip4_addr = data[0]  + (data[1] << 8);
	mip4_addr = MELFAS_TS_LIB_ADDR_START + mip4_addr;
	if (mip4_addr > MELFAS_TS_LIB_ADDR_END) {
		input_err(true, &ts->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	rbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	rbuf[1] = (u8)(mip4_addr & 0xFF);
	ret = ts->melfas_ts_i2c_read(ts, rbuf, 2, data, len);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);

exit:
	mutex_unlock(&ts->sponge_mutex);
	return ret;
}

int melfas_ts_write_to_sponge(struct melfas_ts_data *ts, u8 *data, int len)
{
	int ret = 0;
	u8 wbuf[3];
	u16 mip4_addr = 0;

	mutex_lock(&ts->sponge_mutex);
	mip4_addr = data[0]  + (data[1] << 8);
	mip4_addr = MELFAS_TS_LIB_ADDR_START + mip4_addr;
	if (mip4_addr > MELFAS_TS_LIB_ADDR_END) {
		input_err(true, &ts->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	wbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	wbuf[1] = (u8)(mip4_addr & 0xFF);
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &data[2], len - 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto exit;
	}

	wbuf[0] = (u8)((MELFAS_TS_LIB_ADDR_SYNC >> 8) & 0xFF);
	wbuf[1] = (u8)(MELFAS_TS_LIB_ADDR_SYNC & 0xFF);
	wbuf[2] = 1;

	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
exit:
	mutex_unlock(&ts->sponge_mutex);
	return ret;
}

void melfas_ts_set_utc_sponge(struct melfas_ts_data *ts)
{
	int ret;
	u8 data[6] = {SEC_TS_CMD_SPONGE_OFFSET_UTC, 0};
	struct timespec64 current_time;

	ktime_get_real_ts64(&current_time);
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[4] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[5] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	input_info(true, &ts->client->dev, "Write UTC to Sponge = %X\n", (int)(current_time.tv_sec));

	ret = ts->melfas_ts_write_sponge(ts, data, 6);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);
}

int melfas_ts_set_custom_library(struct melfas_ts_data *ts)
{
	u8 data[3] = { 0 };
	int ret;
	u8 force_fod_enable = 0;

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	/* enable FOD when LCD on state */
	if (ts->plat_data->support_fod && ts->plat_data->enabled)
		force_fod_enable = SEC_TS_MODE_SPONGE_PRESS;
#endif

	input_err(true, &ts->client->dev, "%s: Sponge (0x%02x)%s\n",
			__func__, ts->plat_data->lowpower_mode,
			force_fod_enable ? ", force fod enable" : "");

	if (ts->plat_data->prox_power_off) {
		data[2] = (ts->plat_data->lowpower_mode | force_fod_enable) &
						~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
		input_info(true, &ts->client->dev, "%s: prox off. disable AOT\n", __func__);
	} else {
		data[2] = ts->plat_data->lowpower_mode | force_fod_enable;
	}
	
	ret = ts->melfas_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

		/* read dump info */
	data[0] = SEC_TS_CMD_SPONGE_LP_DUMP;
	ret = ts->melfas_ts_read_sponge(ts, data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read dump_data\n", __func__);
		return ret;
	}

	ts->sponge_inf_dump = (data[0] & SEC_TS_SPONGE_DUMP_INF_MASK) >> SEC_TS_SPONGE_DUMP_INF_SHIFT;
	ts->sponge_dump_format = data[0] & SEC_TS_SPONGE_DUMP_EVENT_MASK;
	ts->sponge_dump_event = data[1];
	ts->sponge_dump_border = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT 
					+ (ts->sponge_dump_format * ts->sponge_dump_event);
	ts->sponge_dump_border_lsb = ts->sponge_dump_border & 0xFF;
	ts->sponge_dump_border_msb = (ts->sponge_dump_border & 0xFF00) >> 8;

	input_err(true, &ts->client->dev, "%s: sponge_inf_dump:%d sponge_dump_event:%d sponge_dump_format:%d\n",
			__func__, ts->sponge_inf_dump, ts->sponge_dump_event, ts->sponge_dump_format);

	return ret;
}

void melfas_ts_get_custom_library(struct melfas_ts_data *ts)
{
	u8 data[6] = { 0, };
	int ret;

	data[0] = SEC_TS_CMD_SPONGE_FOD_INFO;
	ret = ts->melfas_ts_read_sponge(ts, data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read fod info\n", __func__);
		return;
	}

	sec_input_set_fod_info(&ts->client->dev, data[0], data[1], data[2], data[3]);
}

int melfas_ts_set_press_property(struct melfas_ts_data *ts)
{
	u8 data[3] = { SEC_TS_CMD_SPONGE_PRESS_PROPERTY, 0 };
	int ret;

	if (!ts->plat_data->support_fod)
		return 0;

	data[2] = ts->plat_data->fod_data.press_prop;

	ret = ts->melfas_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->fod_data.press_prop);

	return ret;
}

int melfas_ts_set_fod_rect(struct melfas_ts_data *ts)
{
	u8 data[10] = {0x4b, 0};
	int ret, i;

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
		__func__, ts->plat_data->fod_data.rect_data[0], ts->plat_data->fod_data.rect_data[1],
		ts->plat_data->fod_data.rect_data[2], ts->plat_data->fod_data.rect_data[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->plat_data->fod_data.rect_data[i] & 0xFF;
		data[i * 2 + 3] = (ts->plat_data->fod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->melfas_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	return ret;
}

int melfas_ts_set_aod_rect(struct melfas_ts_data *ts)
{
	u8 data[10] = {0x02, 0};
	int i;
	int ret;

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 3] = (ts->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->melfas_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail set custom lib \n", __func__);

	return ret;
}

int melfas_ts_init_config(struct melfas_ts_data *ts)
{
	u8 wbuf[8];
	u8 rbuf[32];
	int ret = 0;

	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);

	/* read product name */
	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_PRODUCT_NAME;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 16);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to init config\n",
				__func__);
		return ret;
	}
	memcpy(ts->product_name, rbuf, 16);
	input_info(true, &ts->client->dev, "%s - product_name[%s]\n",
		__func__, ts->product_name);

	/* read fw version */
	melfas_ts_get_fw_version(ts, rbuf);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_NODE_NUM_X;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0)
		ts->plat_data->x_node_num = 15;
	else
		ts->plat_data->x_node_num = rbuf[0];

	if (!ts->plat_data->x_node_num)
		ts->plat_data->x_node_num = 15;

	wbuf[1] = MELFAS_TS_R1_INFO_NODE_NUM_Y;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0)
		ts->plat_data->y_node_num = 32;
	else
		ts->plat_data->y_node_num = rbuf[0];

	if (!ts->plat_data->y_node_num)
		ts->plat_data->y_node_num = 32;

	input_info(true, &ts->client->dev, "%s x_node_num:%d y_node_num:%d \n",
			__func__, ts->plat_data->x_node_num, ts->plat_data->y_node_num);

	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

int melfas_ts_set_lowpowermode(void *data, u8 mode)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)data;
	u8 wbuf[3];
	u8 rbuf[1];
	int retry = 10;
	int ret = 0;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);
	input_err(true, &ts->client->dev, "%s: %s(%X)\n", __func__,
				mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", ts->plat_data->lowpower_mode);

	if (mode) {
		melfas_ts_set_utc_sponge(ts);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->sponge_dump_delayed_flag) {
				melfas_ts_sponge_dump_flush(ts, ts->sponge_dump_delayed_area);
				ts->sponge_dump_delayed_flag = false;
				input_info(true, &ts->client->dev, "%s : Sponge dump flush delayed work have procceed\n", __func__);
			}
		}
#endif
		ret = melfas_ts_set_custom_library(ts);
		if (ret < 0)
			return -EIO;
	}

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;

	do {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			return -EIO;
		}

		sec_delay(20);
		ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s [ERROR] read %x %x, rbuf %x\n",
					__func__, wbuf[0], wbuf[1], rbuf[0]);
			return -EIO;
		}

	} while ((retry--) && (rbuf[0] != mode));

	melfas_ts_locked_release_all_finger(ts);

	if (device_may_wakeup(&ts->client->dev)) {
		if (mode)
			enable_irq_wake(ts->client->irq);
		else
			disable_irq_wake(ts->client->irq);
	}

	if (mode == TO_LOWPOWER_MODE)
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
	else
		ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

	if (rbuf[0] != mode) {
		ts->plat_data->hw_param.mode_change_failed_count++;
		input_err(true, &ts->client->dev, "%s [ERROR] not changed to %s mode, rbuf %x\n",
				__func__, mode ? "LPM" : "normal", rbuf[0]);
		return -EIO;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;
}


void melfas_ts_reset(struct melfas_ts_data *ts, unsigned int ms)
{
	input_info(true, &ts->client->dev, "%s: Recover IC, discharge time:%d\n", __func__, ms);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, false);

	sec_delay(ms);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, true);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);
}

void melfas_ts_reset_work(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data,
			reset_work.work);
	int ret;
	char result[32];
	char test[32];

	ts->plat_data->hw_param.ic_reset_count++;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: secure touch enabled\n", __func__);
		return;
	}
#endif
	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: reset is ongoing\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);
	__pm_stay_awake(ts->plat_data->sec_ws);

	ts->reset_is_on_going = true;
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->stop_device(ts);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ret = ts->plat_data->start_device(ts);
	if (ret < 0) {
		/* for ACT i2c recovery fail test */
		snprintf(test, sizeof(test), "TEST=RECOVERY");
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(&ts->sec, test, result);

		input_err(true, &ts->client->dev, "%s: failed to reset, ret:%d\n", __func__, ret);
		ts->reset_is_on_going = false;
		cancel_delayed_work(&ts->reset_work);
		if (!ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
		mutex_unlock(&ts->modechange);

		snprintf(result, sizeof(result), "RESULT=RESET");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);

		__pm_relax(ts->plat_data->sec_ws);

		return;
	}

	if (!ts->plat_data->enabled) {
		input_err(true, &ts->client->dev, "%s: call input_close\n", __func__);

		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode) {
			ret = ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: failed to reset, ret:%d\n", __func__, ret);
				ts->reset_is_on_going = false;
				cancel_delayed_work(&ts->reset_work);
				if (!ts->plat_data->shutdown_called)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
				mutex_unlock(&ts->modechange);
				__pm_relax(ts->plat_data->sec_ws);
				return;
			}
			if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
				melfas_ts_set_aod_rect(ts);
		} else {
			ts->plat_data->stop_device(ts);
		}
	}

	ts->reset_is_on_going = false;
	mutex_unlock(&ts->modechange);

	snprintf(result, sizeof(result), "RESULT=RESET");
	sec_cmd_send_event_to_user(&ts->sec, NULL, result);

	__pm_relax(ts->plat_data->sec_ws);

}

void melfas_ts_read_info_work(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, 
			work_read_info.work);

	input_log_fix();
	melfas_ts_run_rawdata(ts, true);
}

void melfas_ts_print_info_work(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work_print_info.work);

	
	sec_input_print_info(&ts->client->dev, ts->tdata);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

void melfas_ts_unlocked_release_all_finger(struct melfas_ts_data *ts)
{
	sec_input_release_all_finger(&ts->client->dev);
}

void melfas_ts_locked_release_all_finger(struct melfas_ts_data *ts)
{
	mutex_lock(&ts->eventlock);

	melfas_ts_unlocked_release_all_finger(ts);

	mutex_unlock(&ts->eventlock);
}

int melfas_ts_set_cover_type(struct melfas_ts_data *ts, bool enable)
{
	u8 cover_cmd;
	u8 wbuf[4] = { 0,};
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %s, type:%d\n",
			__func__, enable ? "close" : "open", ts->plat_data->cover_type);

	cover_cmd = sec_input_check_cover_type(&ts->client->dev) & 0xFF;

	if (enable)
		ts->plat_data->touch_functions |= MELFAS_TS_BIT_SETFUNC_COVER;
	else
		ts->plat_data->touch_functions &= ~MELFAS_TS_BIT_SETFUNC_COVER;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: pwr off, close:%d, touch_fn:%x\n", __func__,
				enable, ts->plat_data->touch_functions);
		return -ENODEV;
	}

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_COVER_MODE;
	wbuf[2] = ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_COVER;
	wbuf[3] = cover_cmd;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s mms_i2c_write\n", __func__);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: cover state, %d %d\n",
		__func__, ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_COVER, cover_cmd);
	return 0;
}
EXPORT_SYMBOL(melfas_ts_set_cover_type);

int melfas_ts_set_glove_mode(struct melfas_ts_data *ts, bool enable)
{
	u8 wbuf[4] = { 0,};
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %s \n",
			__func__, enable ? "close" : "open");

	if (enable)
		ts->plat_data->touch_functions |= MELFAS_TS_BIT_SETFUNC_GLOVE;
	else
		ts->plat_data->touch_functions &= ~MELFAS_TS_BIT_SETFUNC_GLOVE;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: pwr off, close:%d, touch_fn:%x\n", __func__,
				enable, ts->plat_data->touch_functions);
		return -ENODEV;
	}

	input_info(true, &ts->client->dev, "%s: glove state, %d \n",
		__func__,ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_GLOVE);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_GLOVE_MODE;
	wbuf[2] = ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_GLOVE;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s mms_i2c_write\n", __func__);
		return -EIO;
	}

	input_info(true, &ts->client->dev, "%s: glove state, %d\n",
		__func__, ts->plat_data->touch_functions & MELFAS_TS_BIT_SETFUNC_GLOVE);
	return 0;
}


int melfas_ts_pocket_mode_enable(struct melfas_ts_data *ts, u8 enable)
{
	int ret;
	u8 address[2] = { 0, };
	u8 data = enable;

	input_info(true, &ts->client->dev, "%s: %s\n",
			__func__, data ? "enable" : "disable");

	address[0] = MELFAS_TS_R0_CTRL;
	address[1] = MELFAS_TS_R1_CTRL_POCKET_MODE;
	ret = ts->melfas_ts_i2c_write(ts, address, 2, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to set pocket mode enable\n", __func__);
	
	return ret;
}

int melfas_ts_ear_detect_enable(struct melfas_ts_data *ts, u8 enable)
{
	int ret;
	u8 address[2] = { 0, };
	u8 data = enable;

	input_info(true, &ts->client->dev, "%s: enable:%d\n", __func__, enable);

	/* 00: off, 01:Mutual, 10:Self, 11: Mutual+Self */
	address[0] = MELFAS_TS_R0_CTRL;
	address[1] = MELFAS_TS_R1_CTRL_PROXIMITY;
	ret = ts->melfas_ts_i2c_write(ts, address, 2, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to set ear_detect mode enable\n", __func__);
		return ret;

	return ret;
}

/*
 *	flag     1  :  set edge handler
 *		2  :  set (portrait, normal) edge zone data
 *		4  :  set (portrait, normal) dead zone data
 *		8  :  set landscape mode data
 *		16 :  mode clear
 *	data
 *		0xAA, FFF (y start), FFF (y end),  FF(direction)
 *		0xAB, FFFF (edge zone)
 *		0xAC, FF (up x), FF (down x), FFFF (up y), FF (bottom x), FFFF (down y)
 *		0xAD, FF (mode), FFF (edge), FFF (dead zone x), FF (dead zone top y), FF (dead zone bottom y)
 *	case
 *		edge handler set :  0xAA....
 *		booting time :  0xAA...  + 0xAB...
 *		normal mode : 0xAC...  (+0xAB...)
 *		landscape mode : 0xAD...
 *		landscape -> normal (if same with old data) : 0xAD, 0
 *		landscape -> normal (etc) : 0xAC....  + 0xAD, 0
 */

void melfas_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	u8 data[15] = { 0 };
	u8 address[2] = {MELFAS_TS_R0_CUSTOM,};

	input_info(true, &ts->client->dev, "%s: flag: %02X (clr,lan,nor,edg,han)\n", __func__, flag);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->plat_data->grip_data.edgehandler_direction == 0) {
			data[0] = 0x0;
			data[1] = 0x0;
			data[2] = 0x0;
			data[3] = 0x0;
		} else {
			data[0] = ts->plat_data->grip_data.edgehandler_direction & 0x3;
			data[1] = ts->plat_data->grip_data.edgehandler_start_y & 0xFF;
			data[2] = ts->plat_data->grip_data.edgehandler_end_y & 0xFF;
			data[3] = (((ts->plat_data->grip_data.edgehandler_end_y >> 8)  & 0xF) << 4) | ((ts->plat_data->grip_data.edgehandler_start_y >> 8) & 0xF);
		}
		address[1] = MELFAS_TS_R1_SEC_EDGE_HANDLER;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 4);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X\n",
				__func__, MELFAS_TS_R1_SEC_EDGE_HANDLER, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_EDGE_ZONE) {
		data[0] = ts->plat_data->grip_data.edge_range  & 0xFF;
		data[1] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		address[1] = MELFAS_TS_R1_SEC_EDGE_AREA;;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 2);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X\n",
				__func__, MELFAS_TS_R1_SEC_EDGE_AREA, data[0], data[1]);
	}

	if (flag & G_SET_NORMAL_MODE) {
		data[0] = ts->plat_data->grip_data.deadzone_y & 0xFF;
		data[1] = (ts->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		data[2] = ts->plat_data->grip_data.deadzone_up_x & 0xFF;
		data[3] = (ts->plat_data->grip_data.deadzone_up_x >> 8 & 0xFF);
		data[4] = ts->plat_data->grip_data.deadzone_dn_x & 0xFF;
		data[5] = (ts->plat_data->grip_data.deadzone_dn_x >> 8 & 0xFF);
		address[1] = MELFAS_TS_R1_SEC_DEAD_ZONE;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 6);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X,%02X,%02X\n",
				__func__, MELFAS_TS_R1_SEC_DEAD_ZONE, data[0], data[1], data[2], data[3], data[4], data[5]);

		address[1] = MELFAS_TS_R1_SEC_DEAD_ZONE_ENABLE;
		data[0] = 0x1;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 1);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X data:%d\n",
				__func__, MELFAS_TS_R1_SEC_DEAD_ZONE_ENABLE, address[0], address[1], data[0]);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		data[0] = ts->plat_data->grip_data.landscape_deadzone & 0xFF;
		data[1] = (ts->plat_data->grip_data.landscape_deadzone >> 8) & 0xFF;
		data[2] = ts->plat_data->grip_data.landscape_edge & 0xFF;
		data[3] = (ts->plat_data->grip_data.landscape_edge >> 8) & 0xFF;
		data[4] = ts->plat_data->grip_data.landscape_mode & 0x1;
		data[5] = 0x0;
		data[6] = 0x1;
		data[7] = ts->plat_data->grip_data.landscape_top_deadzone & 0xFF;
		data[8] = (ts->plat_data->grip_data.landscape_top_deadzone >> 8) & 0xFF;
		data[9] = ts->plat_data->grip_data.landscape_bottom_deadzone & 0xFF;
		data[10] = (ts->plat_data->grip_data.landscape_bottom_deadzone >> 8) & 0xFF;
		data[11] = ts->plat_data->grip_data.landscape_top_gripzone & 0xFF;
		data[12] = (ts->plat_data->grip_data.landscape_top_gripzone >> 8) & 0xFF;
		data[13] = ts->plat_data->grip_data.landscape_bottom_gripzone & 0xFF;
		data[14] = (ts->plat_data->grip_data.landscape_bottom_gripzone >> 8) & 0xFF;

		address[1] = MELFAS_TS_R1_SEC_LANDSCAPE_MODE;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 15);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X, %02X,%02X,%02X,%02X,"
				" %02X,%02X,%02X,%02X, %02X,%02X,%02X,\n",
				__func__, MELFAS_TS_R1_SEC_LANDSCAPE_MODE,
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7],
				data[8], data[9], data[10], data[11], 
				data[12], data[13], data[14]);
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		data[0] = ts->plat_data->grip_data.landscape_mode;
		data[1] = 0x0;
		data[2] = 0x0;
		address[1] = MELFAS_TS_R1_SEC_LANDSCAPE_MODE_CLR;;
		ts->melfas_ts_i2c_write(ts, address, 2, data, 7);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X\n",
				__func__, MELFAS_TS_R1_SEC_LANDSCAPE_MODE_CLR, data[0], data[1], data[2]);


	}
}

void minority_report_calculate_cmdata(struct melfas_ts_data *ts)
{
	ts->item_cmdata = (ts->test_diff_max / 100) > 0xF ? 0xF : (ts->test_diff_max / 100);
}

void minority_report_sync_latest_value(struct melfas_ts_data *ts)
{
	u32 temp = 0;

	temp |= (ts->item_cmdata & 0xF);

	ts->defect_probability = temp;

	input_info(true, &ts->client->dev, "%s : defect_probability[%X]\n",
		__func__, ts->defect_probability);
}

int melfas_ts_proc_table_data(struct melfas_ts_data *ts, u8 data_type_size,
					u8 data_type_sign, u8 buf_addr_h, u8 buf_addr_l, u8 row_num,
					u8 col_num, u8 buf_col_num, u8 rotate)
{
	char data[16];
	int i_col, i_row, i_x, i_y;
	int max_x = 0;
	int max_y = 0;
	int sValue = 0;
	unsigned int uValue = 0;
	int value = 0;
	int size = 0;
	u8 wbuf[8];
	u8 rbuf[512];
	unsigned int buf_addr;
	int offset;
	int i, j, tmp;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	unsigned char *pStr = NULL;
	int lsize = (ts->plat_data->x_node_num + ts->plat_data->y_node_num) * CMD_RESULT_WORD_LEN;
	int ret = 0;

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		goto error_alloc_mem;

	memset(data, 0, 16);
	memset(pStr, 0, lsize);
	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	/* set axis */
	if (rotate == 0) {
		max_x = col_num;
		max_y = row_num;
	} else if (rotate == 1) {
		max_x = row_num;
		max_y = col_num;
	} else {
		input_err(true,&ts->client->dev, "%s [ERROR] rotate[%d]\n", __func__, rotate);
		goto error;
	}

	/* get table data */
	for (i_row = 0; i_row < row_num; i_row++) {
		/* get line data */
		offset = buf_col_num * data_type_size;
		size = col_num * data_type_size;
		buf_addr = (buf_addr_h << 8) | buf_addr_l | (offset * i_row);

		wbuf[0] = (buf_addr >> 8) & 0xFF;
		wbuf[1] = buf_addr & 0xFF;
		ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, size);
		if (ret < 0) {
			input_err(true,&ts->client->dev, "%s [ERROR] Read data buffer\n", __func__);
			goto error;
		}

		/* save data */
		for (i_col = 0; i_col < col_num; i_col++) {
			if (data_type_sign == 0) {
				/* unsigned */
				switch (data_type_size) {
				case 1:
					uValue = (u8)rbuf[i_col];
					break;
				case 2:
					uValue = (u16)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8));
					break;
				case 4:
					uValue = (u32)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8) | (rbuf[data_type_size * i_col + 2] << 16) | (rbuf[data_type_size * i_col + 3] << 24));
					break;
				default:
					input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
					goto error;
				}
				value = (int)uValue;
			} else {
				/* signed */
				switch (data_type_size) {
				case 1:
					sValue = (s8)rbuf[i_col];
					break;
				case 2:
					sValue = (s16)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8));
					break;
				case 4:
					sValue = (s32)(rbuf[data_type_size * i_col] | (rbuf[data_type_size * i_col + 1] << 8) | (rbuf[data_type_size * i_col + 2] << 16) | (rbuf[data_type_size * i_col + 3] << 24));
					break;
				default:
					input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
					goto error;
				}
				value = (int)sValue;
			}

			switch (rotate) {
			case 0:
				ts->image_buf[i_row * col_num + i_col] = value;
				break;
			case 1:
				ts->image_buf[i_col * row_num + (row_num - 1 - i_row)] = value;
				break;
			default:
				input_err(true,&ts->client->dev, "%s [ERROR] rotate[%d]\n", __func__, rotate);
				goto error;
			}
		}
	}

	/* min, max */
	for (i = 0; i < (row_num * col_num); i++) {
		if (i == 0)
			ts->test_min = ts->test_max = ts->image_buf[i];

		ts->test_min = min(ts->test_min, ts->image_buf[i]);
		ts->test_max = max(ts->test_max, ts->image_buf[i]);

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->image_buf[i]);
		strlcat(ts->print_buf, temp, PAGE_SIZE);
		memset(temp, 0x00, SEC_CMD_STR_LEN);		
	}

	/* max_diff without border*/
	for (i = 1; i < row_num - 1; i++) {
		for (j = 1; j < col_num - 2; j++) {
			if (i == 1 && j == 1) {
				ts->test_diff_max = 0;
			}
			tmp = ts->image_buf[i * col_num + j] - ts->image_buf[i * col_num + j + 1];
			ts->test_diff_max = max(ts->test_diff_max, abs(tmp));
			if (i < row_num - 2) {
				tmp = ts->image_buf[i * col_num + j] - ts->image_buf[(i + 1) * col_num + j];
				ts->test_diff_max = max(ts->test_diff_max, abs(tmp));
			}
		}
	}

	/* print table header */
	snprintf(data, sizeof(data), "    ");
	strlcat(pStr, data, lsize);
	memset(data, 0, 16);

	switch (data_type_size) {
	case 1:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%2d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	case 2:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%4d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	case 4:
		for (i_x = 0; i_x < max_x; i_x++) {
			snprintf(data, sizeof(data), "[%5d]", i_x);
			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}
		break;
	default:
		input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
		goto error;
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(data, 0, 16);
	memset(pStr, 0, lsize);

	/* print table */
	for (i_y = 0; i_y < max_y; i_y++) {
		/* print line header */
		snprintf(data, sizeof(data), "[%2d]", i_y);
		strlcat(pStr, data, lsize);
		memset(data, 0, 16);

		/* print line */
		for (i_x = 0; i_x < max_x; i_x++) {
			switch (data_type_size) {
			case 1:
				snprintf(data, sizeof(data), " %3d", ts->image_buf[i_y * max_x + i_x]);
				break;
			case 2:
				snprintf(data, sizeof(data), " %5d", ts->image_buf[i_y * max_x + i_x]);
				break;
			case 4:
				snprintf(data, sizeof(data), " %6d", ts->image_buf[i_y * max_x + i_x]);
				break;
			default:
				input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}

			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}

		input_raw_info(true, &ts->client->dev, "%s\n", pStr);

		memset(data, 0, 16);
		memset(pStr, 0, lsize);

	}

	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	kfree(pStr);

	return 0;

error:
	input_err(true,&ts->client->dev, "%s [ERROR]\n", __func__);
	kfree(pStr);
error_alloc_mem:	
	return 1;
}

int melfas_ts_proc_vector_data(struct melfas_ts_data *ts, u8 data_type_size,
		u8 data_type_sign, u8 buf_addr_h, u8 buf_addr_l, u8 key_num, u8 vector_num, u16 *vector_id,
		u16 *vector_elem_num, int table_size)
{
	char data[16];
	int i, i_line, i_vector, i_elem;
	int sValue = 0;
	unsigned int uValue = 0;
	int value = 0;
	int size = 0;
	u8 wbuf[8];
	u8 rbuf[512];
	unsigned int buf_addr;
	int key_exist = 0;
	int total_len = 0;
	int elem_len = 0;
	int vector_total_len = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	unsigned char *pStr = NULL;
	int lsize = (ts->plat_data->x_node_num + ts->plat_data->y_node_num) * CMD_RESULT_WORD_LEN;
	int ret = 0;

	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		goto error_alloc_mem;

	memset(data, 0, 16);
	memset(pStr, 0, lsize);
	memset(ts->print_buf, 0, PAGE_SIZE);

	for (i = 0; i < vector_num; i++) {
		vector_total_len += vector_elem_num[i];
		input_dbg(true, &ts->client->dev, "%s - vector_elem_num(%d)[%d]\n", __func__, i, vector_elem_num[i]);		
	}
	
	total_len = key_num + vector_total_len;
	input_dbg(true, &ts->client->dev, "%s - key_num[%d] total_len[%d]\n", __func__, key_num, total_len);

	if (key_num > 0)
		key_exist = 1;
	else
		key_exist = 0;

	/* get line data */
	size = (key_num + vector_total_len) * data_type_size;
	buf_addr = (buf_addr_h << 8) | buf_addr_l;

	wbuf[0] = (buf_addr >> 8) & 0xFF;
	wbuf[1] = buf_addr & 0xFF;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, size);
	if (ret < 0) {
		input_err(true,&ts->client->dev, "%s [ERROR] Read data buffer\n", __func__);
		goto error;
	}

	/* save data */
	for (i = 0; i < total_len; i++) {
		if (data_type_sign == 0) {
			/* unsigned */
			switch (data_type_size) {
			case 1:
				uValue = (u8)rbuf[i];
				break;
			case 2:
				uValue = (u16)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8));
				break;
			case 4:
				uValue = (u32)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8) | (rbuf[data_type_size * i + 2] << 16) | (rbuf[data_type_size * i + 3] << 24));
				break;
			default:
				input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}
			value = (int)uValue;
		} else {
			/* signed */
			switch (data_type_size) {
			case 1:
				sValue = (s8)rbuf[i];
				break;
			case 2:
				sValue = (s16)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8));
				break;
			case 4:
				sValue = (s32)(rbuf[data_type_size * i] | (rbuf[data_type_size * i + 1] << 8) | (rbuf[data_type_size * i + 2] << 16) | (rbuf[data_type_size * i + 3] << 24));
				break;
			default:
				input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}
			value = (int)sValue;
		}

		ts->image_buf[table_size + i] = value;

		/* min, max */
		if (table_size + i == 0)
			ts->test_min = ts->test_max = ts->image_buf[table_size + i];

		ts->test_min = min(ts->test_min, ts->image_buf[table_size + i]);
		ts->test_max = max(ts->test_max, ts->image_buf[table_size + i]);

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->image_buf[i]);
		strlcat(ts->print_buf, temp, PAGE_SIZE);
		memset(temp, 0x00, SEC_CMD_STR_LEN);	
	}

	/* print header */
	i_vector = 0;
	i_elem = 0;
	for (i_line = 0; i_line < (key_exist + vector_num); i_line++) {
		if ((i_line == 0) && (key_exist == 1)) {
			elem_len = key_num;
			input_raw_info(true, &ts->client->dev, "%s", "[Key]\n");
		} else {
			elem_len = vector_elem_num[i_vector];

			if (elem_len <= 0) {
				i_vector++;
				continue;
			}
			switch (vector_id[i_vector]) {
			case VECTOR_ID_SCREEN_RX:
				input_raw_info(true, &ts->client->dev, "%s", "[Screen Rx]\n");
				break;
			case VECTOR_ID_SCREEN_TX:
				input_raw_info(true, &ts->client->dev, "%s", "[Screen Tx]\n");
				break;
			case VECTOR_ID_KEY_RX:
				input_raw_info(true, &ts->client->dev, "%s", "[Key Rx]\n");
				break;
			case VECTOR_ID_KEY_TX:
				input_raw_info(true, &ts->client->dev, "%s", "[Key Tx]\n");
				break;
			case VECTOR_ID_PRESSURE:
				input_raw_info(true, &ts->client->dev, "%s", "[Pressure]\n");
				break;
			case VECTOR_ID_OPEN_RESULT:
				input_raw_info(true, &ts->client->dev, "%s", "[Open Result]\n");
				break;
			case VECTOR_ID_OPEN_RX:
				input_raw_info(true, &ts->client->dev, "%s", "[Open Rx]\n");
				break;
			case VECTOR_ID_OPEN_TX:
				input_raw_info(true, &ts->client->dev, "%s", "[Open Tx\n");
				break;
			case VECTOR_ID_SHORT_RESULT:
				input_raw_info(true, &ts->client->dev, "%s", "[Short Result]\n");
				break;
			case VECTOR_ID_SHORT_RX:
				input_raw_info(true, &ts->client->dev, "%s", "[Short Rx]\n");
				break;
			case VECTOR_ID_SHORT_TX:
				input_raw_info(true, &ts->client->dev, "%s", "[Short Tx]\n");
				break;
			default:
				input_raw_info(true, &ts->client->dev, "[%d]\n", i_vector);
				break;
			}
			i_vector++;
		}

		memset(data, 0, 16);
		memset(pStr, 0, lsize);

		/* print line */
		for (i = i_elem; i < (i_elem + elem_len); i++) {
			switch (data_type_size) {
			case 1:
				snprintf(data, sizeof(data), " %3d", ts->image_buf[table_size + i]);
				break;
			case 2:
				snprintf(data, sizeof(data), " %5d", ts->image_buf[table_size + i]);
				break;
			case 4:
				snprintf(data, sizeof(data), " %6d", ts->image_buf[table_size + i]);
				break;
			default:
				input_err(true,&ts->client->dev, "%s [ERROR] data_type_size[%d]\n", __func__, data_type_size);
				goto error;
			}

			strlcat(pStr, data, lsize);
			memset(data, 0, 16);
		}

		input_raw_info(true, &ts->client->dev, "%s\n", pStr);

		memset(data, 0, 16);
		memset(pStr, 0x0, lsize);

		i_elem += elem_len;
	}

	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	kfree(pStr);
	return 0;

error:
	input_err(true,&ts->client->dev, "%s [ERROR]\n", __func__);
	kfree(pStr);
error_alloc_mem:		
	return 1;
}


int melfas_ts_get_ready_status(struct melfas_ts_data *ts)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_READY_STATUS;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	//check status
	if ((ret == MELFAS_TS_CTRL_STATUS_NONE) || (ret == MELFAS_TS_CTRL_STATUS_LOG)
		|| (ret == MELFAS_TS_CTRL_STATUS_READY)) {
		input_dbg(false, &ts->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	} else{
		input_err(true, &ts->client->dev,
			"%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if (ret == MELFAS_TS_CTRL_STATUS_LOG) {
		//skip log event
		wbuf[0] = MELFAS_TS_R0_LOG;
		wbuf[1] = MELFAS_TS_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

int melfas_ts_run_test(struct melfas_ts_data *ts, u8 test_type)
{
	int busy_cnt = 100;
	int wait_cnt = 0;
	int wait_num = 200;
	u8 wbuf[8];
	u8 rbuf[512];
	char data[16];
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 vector_num = 0;
	u16 vector_id[16];
	u16 vector_elem_num[16];
	u8 buf_addr_h;
	u8 buf_addr_l;
	u16 buf_addr;
	int table_size;
	int i;
	int ret = 0;

	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);
	input_dbg(true, &ts->client->dev, "%s - test_type[%d]\n", __func__, test_type);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return 1;
	}

	while (busy_cnt--) {
		if (ts->test_busy == false)
			break;

		msleep(10);
	}
	mutex_lock(&ts->device_mutex);
	ts->test_busy = true;
	mutex_unlock(&ts->device_mutex);

	memset(ts->print_buf, 0, PAGE_SIZE);

	disable_irq(ts->client->irq);
	melfas_ts_unlocked_release_all_finger(ts);

	/* disable touch event */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MELFAS_TS_TRIGGER_TYPE_NONE;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Disable event\n", __func__);
		goto ERROR;
	}

	/* check test type */
	switch (test_type) {
	case MELFAS_TS_TEST_TYPE_CM:
		input_raw_info(true, &ts->client->dev, "%s", "==== CM Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CM_ABS:
		input_raw_info(true, &ts->client->dev, "%s", "==== CM ABS Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CM_JITTER:
		input_raw_info(true, &ts->client->dev, "%s", "==== CM JITTER Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_SHORT:
		input_raw_info(true, &ts->client->dev, "%s", "==== SHORT Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_GPIO_LOW:
		input_raw_info(true, &ts->client->dev, "%s", "==== GPIO LOW Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_GPIO_HIGH:
		input_raw_info(true, &ts->client->dev, "%s", "==== GPIO HIGH Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CM_DIFF_HOR:
		input_raw_info(true, &ts->client->dev, "%s", "==== CM DIFF HOR Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CM_DIFF_VER:
		input_raw_info(true, &ts->client->dev, "%s", "==== CM DIFF VER Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CP:
		input_raw_info(true, &ts->client->dev, "%s", "==== CP Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_CP_SHORT:
		input_raw_info(true, &ts->client->dev, "%s", "==== CP SHORT Test ===\n");
		break;		
	case MELFAS_TS_TEST_TYPE_CP_LPM:
		input_raw_info(true, &ts->client->dev, "%s", "==== CP LPM Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_PANEL_CONN:
		input_raw_info(true, &ts->client->dev, "%s", "==== CONNECTION Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_OPEN_SHORT:
		input_raw_info(true, &ts->client->dev, "%s", "==== OPEN SHORT Test ===\n");
		break;
	case MELFAS_TS_TEST_TYPE_VSYNC:
		input_raw_info(true, &ts->client->dev, "%s", "==== V-SYNC Test ===\n");
		break;
	default:
		input_raw_info(true, &ts->client->dev, "%s [ERROR] Unknown test type\n", __func__);
		sprintf(ts->print_buf, "ERROR : Unknown test type");
		goto ERROR;
	}

	/* set test mode */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_MODE;
	wbuf[2] = MELFAS_TS_CTRL_MODE_TEST_CM;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Write test mode\n", __func__);
		goto ERROR;
	}

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (melfas_ts_get_ready_status(ts) == MELFAS_TS_CTRL_STATUS_READY)
			break;

		sec_delay(10);

		input_dbg(false, &ts->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - set control mode retry[%d]\n", __func__, wait_cnt);

	/* set test type */
	wbuf[0] = MELFAS_TS_R0_TEST;
	wbuf[1] = MELFAS_TS_R1_TEST_TYPE;
	wbuf[2] = test_type;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Write test type\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - set test type\n", __func__);

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (melfas_ts_get_ready_status(ts) == MELFAS_TS_CTRL_STATUS_READY)
			break;

		sec_delay(10);

		input_dbg(false, &ts->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - ready retry[%d]\n", __func__, wait_cnt);
	
	/*data format */
	wbuf[0] = MELFAS_TS_R0_TEST;
	wbuf[1] = MELFAS_TS_R1_TEST_DATA_FORMAT;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 7);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Read data format\n", __func__);
		goto ERROR;
	}

	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	vector_num = rbuf[6];

	input_dbg(true, &ts->client->dev, "%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n", __func__, row_num, col_num, buffer_col_num, rotate, key_num);
	input_dbg(true, &ts->client->dev, "%s - data_type[0x%02X] data_type_sign[%d] data_type_size[%d]\n", __func__, data_type, data_type_sign, data_type_size);
	input_dbg(true, &ts->client->dev, "%s - vector_num[%d]\n", __func__, vector_num);

	if (vector_num > 0) {
		wbuf[0] = MELFAS_TS_R0_TEST;
		wbuf[1] = MELFAS_TS_R1_TEST_VECTOR_INFO;
		ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, (vector_num * 4));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s [ERROR] Read vector info\n", __func__);
			goto ERROR;
		}
		for (i = 0; i < vector_num; i++) {
			vector_id[i] = rbuf[i * 4 + 0] | (rbuf[i * 4 + 1] << 8);
			vector_elem_num[i] = rbuf[i * 4 + 2] | (rbuf[i * 4 + 3] << 8);
			input_info(true, &ts->client->dev, "%s - vector[%d] : id[%d] elem_num[%d]\n", __func__, i, vector_id[i], vector_elem_num[i]);
		}
	}

	/* get buf addr */
	wbuf[0] = MELFAS_TS_R0_TEST;
	wbuf[1] = MELFAS_TS_R1_TEST_BUF_ADDR;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Read buf addr\n", __func__);
		goto ERROR;
	}

	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	buf_addr = (buf_addr_h << 8) | buf_addr_l;
	input_dbg(true, &ts->client->dev, "%s - buf_addr[0x%02X 0x%02X] [0x%04X]\n",
		__func__, buf_addr_h, buf_addr_l, buf_addr);

	/* print data */
	table_size = row_num * col_num;
	if (table_size > 0) {
		if (melfas_ts_proc_table_data(ts, data_type_size, data_type_sign, buf_addr_h, buf_addr_l,
							row_num, col_num, buffer_col_num, rotate)) {
			input_err(true, &ts->client->dev, "%s [ERROR] mms_proc_table_data\n", __func__);
			goto ERROR;
		}
	}

	if (test_type == MELFAS_TS_TEST_TYPE_CM) {
		input_info(true, &ts->client->dev, "%s : CM TEST : max_diff[%d]\n",
					__func__, ts->test_diff_max);
		minority_report_calculate_cmdata(ts);
	}

	if ((key_num > 0) || (vector_num > 0)) {
		if (table_size > 0)
			buf_addr += (row_num * buffer_col_num * data_type_size);

		buf_addr_l = buf_addr & 0xFF;
		buf_addr_h = (buf_addr >> 8) & 0xFF;
		input_dbg(true, &ts->client->dev, "%s - vector buf_addr[0x%02X 0x%02X][0x%04X]\n", __func__, buf_addr_h, buf_addr_l, buf_addr);

		if (melfas_ts_proc_vector_data(ts, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, key_num, vector_num, vector_id, vector_elem_num, table_size)) {
			input_err(true,&ts->client->dev, "%s [ERROR] mip4_ts_proc_vector_data\n", __func__);
			goto ERROR;
		}
	}

	/* open short test return */
	if (test_type == MELFAS_TS_TEST_TYPE_OPEN_SHORT) {
		int i_vector = 0;
		int i_elem = 0;
		int i_line, elem_len, temp;
		ts->open_short_result = 0;

		memset(ts->print_buf, 0, PAGE_SIZE);

		for (i_line = 0; i_line < vector_num; i_line++) {
			elem_len = vector_elem_num[i_vector];
			temp = elem_len;

			if (elem_len <= 0) {
				i_vector++;
				continue;
			}

			memset(data, 0, 16);

			if (ts->open_short_type == CHECK_ONLY_OPEN_TEST) {
				if (vector_id[i_vector] == VECTOR_ID_OPEN_RESULT) {
					if (ts->image_buf[table_size + i_elem] == 1) {
						snprintf(data, sizeof(data), "OK");
						strlcat(ts->print_buf, data, PAGE_SIZE);
						memset(data, 0, 16);
						ts->open_short_result = 1;
						break;
					} else {
						snprintf(data, sizeof(data), "NG,OPEN:");
						temp = -1;
					}
				} else if (vector_id[i_vector] == VECTOR_ID_OPEN_RX) {
					snprintf(data, sizeof(data), "RX:");
				} else if (vector_id[i_vector] == VECTOR_ID_OPEN_TX) {
					snprintf(data, sizeof(data), "TX:");
				} else {
					temp = -1;
				}
			} else if (ts->open_short_type == CHECK_ONLY_SHORT_TEST) {
				if (vector_id[i_vector] == VECTOR_ID_SHORT_RESULT) {
					if (ts->image_buf[table_size + i_elem] == 1) {
						snprintf(data, sizeof(data), "OK");
						strlcat(ts->print_buf, data, PAGE_SIZE);
						memset(data, 0, 16);
						ts->open_short_result = 1;
						break;
					} else {
						snprintf(data, sizeof(data), "NG,SHORT:");
						temp = -1;
					}
				} else if (vector_id[i_vector] == VECTOR_ID_SHORT_RX) {
					snprintf(data, sizeof(data), "RX:");
				} else if (vector_id[i_vector] == VECTOR_ID_SHORT_TX) {
					snprintf(data, sizeof(data), "TX:");
				} else {
					temp = -1;
				}
			}

			strlcat(ts->print_buf, data, PAGE_SIZE);
			memset(data, 0, 16);

			for (i = i_elem; i < (i_elem + temp); i++) {
				snprintf(data, sizeof(data), "%d,", ts->image_buf[table_size + i]);
				strlcat(ts->print_buf, data, PAGE_SIZE);
				memset(data, 0, 16);
			}
			i_vector++;
			i_elem += elem_len;				
		}
	} else if (test_type == MELFAS_TS_TEST_TYPE_GPIO_LOW | test_type == MELFAS_TS_TEST_TYPE_GPIO_HIGH) { 
		ts->image_buf[0] = gpio_get_value(ts->plat_data->irq_gpio);
		input_info(true, &ts->client->dev, "%s gpio value %d\n", __func__, ts->image_buf[0]);
	}

	/* set normal mode */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_MODE;
	wbuf[2] = MELFAS_TS_CTRL_MODE_NORMAL;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto ERROR;
	}

	/* wait ready status */
	wait_cnt = wait_num;
	while (wait_cnt--) {
		if (melfas_ts_get_ready_status(ts) == MELFAS_TS_CTRL_STATUS_READY)
			break;

		sec_delay(10);

		input_dbg(false, &ts->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - set normal mode %d\n", __func__, wait_cnt);

	goto EXIT;

ERROR:
	ret = 1;
EXIT:
	//enable touch event
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MELFAS_TS_TRIGGER_TYPE_INTR;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}

	if (ret)
		melfas_ts_reset(ts, 10);

	enable_irq(ts->client->irq);

	mutex_lock(&ts->device_mutex);
	ts->test_busy = false;
	mutex_unlock(&ts->device_mutex);

	if (ret)
		input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	else
		input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return ret;
}

int melfas_ts_get_image(struct melfas_ts_data *ts, u8 image_type)
{
	int busy_cnt = 500;
	int wait_cnt = 200;
	u8 wbuf[8];
	u8 rbuf[512];
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 vector_num = 0;
	u16 vector_id[16];
	u16 vector_elem_num[16];
	u8 buf_addr_h;
	u8 buf_addr_l;
	u16 buf_addr = 0;
	int i;
	int table_size;
	int ret = 0;

	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);
	input_dbg(true, &ts->client->dev, "%s - image_type[%d]\n", __func__, image_type);

	while (busy_cnt--) {
		if (ts->test_busy == false)
			break;

		sec_delay(10);
	}

	memset(ts->print_buf, 0, PAGE_SIZE);

	/* disable touch event */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MELFAS_TS_CTRL_TRIGGER_NONE;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Disable event\n", __func__);
		return 1;
	}

	mutex_lock(&ts->device_mutex);
	ts->test_busy = true;
	disable_irq(ts->client->irq);
	mutex_unlock(&ts->device_mutex);

	//check image type
	switch (image_type) {
	case MELFAS_TS_IMG_TYPE_INTENSITY:
		input_info(true, &ts->client->dev, "%s", "=== Intensity Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_RAWDATA:
		input_info(true, &ts->client->dev, "%s", "=== Rawdata Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_HSELF_RAWDATA:
		input_info(true, &ts->client->dev, "%s", "=== self Rawdata Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_HSELF_INTENSITY:
		input_info(true, &ts->client->dev, "%s", "=== self intensity Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_PROX_INTENSITY:
		input_info(true, &ts->client->dev, "%s", "=== PROX intensity Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_5POINT_INTENSITY:
		input_info(true, &ts->client->dev, "%s", "=== sensitivity Image ===\n");
		break;		
	case MELFAS_TS_IMG_TYPE_JITTER_DELTA:
		input_info(true, &ts->client->dev, "%s", "=== JITTER_DELTA Image ===\n");
		break;
	case MELFAS_TS_IMG_TYPE_SNR_DATA:
		input_info(true, &ts->client->dev, "%s", "=== SNR_DATA Image ===\n");
		break;
	default:
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown image type\n", __func__);
		goto ERROR;
	}

	//set image type
	wbuf[0] = MELFAS_TS_R0_IMAGE;
	wbuf[1] = MELFAS_TS_R1_IMAGE_TYPE;
	wbuf[2] = image_type;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Write image type\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - set image type\n", __func__);

	//wait ready status
	wait_cnt = 200;
	while (wait_cnt--) {
		if (melfas_ts_get_ready_status(ts) == MELFAS_TS_CTRL_STATUS_READY)
			break;

		sec_delay(10);

		input_dbg(true, &ts->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &ts->client->dev, "%s - ready\n", __func__);

	//data format
	wbuf[0] = MELFAS_TS_R0_IMAGE;
	wbuf[1] = MELFAS_TS_R1_IMAGE_DATA_FORMAT;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 7);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Read data format\n", __func__);
		goto ERROR;
	}

	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	vector_num = rbuf[6];

	input_dbg(true, &ts->client->dev,
		"%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n",
		__func__, row_num, col_num, buffer_col_num, rotate, key_num);
	input_dbg(true, &ts->client->dev,
		"%s - data_type[0x%02X] data_sign[%d] data_size[%d]\n",
		__func__, data_type, data_type_sign, data_type_size);

	if (vector_num > 0) {
		wbuf[0] = MELFAS_TS_R0_IMAGE;
		wbuf[1] = MELFAS_TS_R1_IMAGE_VECTOR_INFO;
		ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, (vector_num * 4));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s [ERROR] Read vector info\n", __func__);
			goto ERROR;
		}
		for (i = 0; i < vector_num; i++) {
			vector_id[i] = rbuf[i * 4 + 0] | (rbuf[i * 4 + 1] << 8);
			vector_elem_num[i] = rbuf[i * 4 + 2] | (rbuf[i * 4 + 3] << 8);
			input_dbg(true, &ts->client->dev, "%s - vector[%d] : id[%d] elem_num[%d]\n", __func__, i, vector_id[i], vector_elem_num[i]);
		}
	}

	//get buf addr
	wbuf[0] = MELFAS_TS_R0_IMAGE;
	wbuf[1] = MELFAS_TS_R1_IMAGE_BUF_ADDR;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Read buf addr\n", __func__);
		goto ERROR;
	}

	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	input_dbg(true, &ts->client->dev, "%s - buf_addr[0x%02X 0x%02X]\n",
		__func__, buf_addr_h, buf_addr_l);

	/* print data */
	table_size = row_num * col_num;
	if (table_size > 0) {
		if (melfas_ts_proc_table_data(ts, data_type_size, data_type_sign, buf_addr_h, buf_addr_l,
							row_num, col_num, buffer_col_num, rotate)) {
			input_err(true, &ts->client->dev, "%s [ERROR] mms_proc_table_data\n", __func__);
			goto ERROR;
		}
	}

	if ((key_num > 0) || (vector_num > 0)) {
		if (table_size > 0)
			buf_addr += (row_num * buffer_col_num * data_type_size);

		buf_addr_l = buf_addr & 0xFF;
		buf_addr_h = (buf_addr >> 8) & 0xFF;
		input_dbg(true, &ts->client->dev, "%s - vector buf_addr[0x%02X 0x%02X][0x%04X]\n", __func__, buf_addr_h, buf_addr_l, buf_addr);

		if (melfas_ts_proc_vector_data(ts, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, key_num, vector_num, vector_id, vector_elem_num, table_size)) {
			input_err(true,&ts->client->dev, "%s [ERROR] mip4_ts_proc_vector_data\n", __func__);
			goto ERROR;
		}
	}
	goto EXIT;

ERROR:
	ret = 1;
EXIT:
	/* clear image type */
	wbuf[0] = MELFAS_TS_R0_IMAGE;
	wbuf[1] = MELFAS_TS_R1_IMAGE_TYPE;
	wbuf[2] = MELFAS_TS_IMG_TYPE_NONE;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Clear image type\n", __func__);
		ret = 1;
	}

	/* enable touch event */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MELFAS_TS_CTRL_TRIGGER_INTR;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}

	if (ret)
		melfas_ts_reset(ts, 10);

	mutex_lock(&ts->device_mutex);
	ts->test_busy = false;
	enable_irq(ts->client->irq);
	mutex_unlock(&ts->device_mutex);

	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

void melfas_ts_run_rawdata(struct melfas_ts_data *ts, bool on_probe)
{
	ts->tsp_dump_lock = 1;
	input_raw_data_clear();
	input_raw_info(true, &ts->client->dev, "%s: start ##\n", __func__);

	if (!on_probe) {
		if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_INTENSITY)) {
			input_err(true, &ts->client->dev, "%s intensity error\n", __func__);
			goto out;
		}
	}

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM)) {
		input_err(true, &ts->client->dev, "%s cm error\n", __func__);
		goto out;
	}
	minority_report_sync_latest_value(ts);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP)) {
		input_err(true, &ts->client->dev, "%s cp error\n", __func__);
		goto out;
	}

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_SHORT)) {
		input_err(true, &ts->client->dev, "%s cp short error\n", __func__);
		goto out;
	}

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_LPM)) {
		input_err(true, &ts->client->dev, "%s cp lpm error\n", __func__);
		goto out;
	}

	if (on_probe) {
		if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_ABS)) {
			input_err(true, &ts->client->dev, "%s cm abs error\n", __func__);
			goto out;
		}

		if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_JITTER)) {
			input_err(true, &ts->client->dev, "%s cm_jitter error\n", __func__);
			goto out;
		}
	}

out:
	input_raw_info(true, &ts->client->dev, "%s: done ##\n", __func__);
	ts->tsp_dump_lock = 0;
}

int melfas_ts_get_fw_version(struct melfas_ts_data *ts, u8 *ver_buf)
{
	u8 rbuf[8] = {0,};
	u8 wbuf[2] = {0,};
	int i;
	int ret = 0;

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_VERSION_BOOT;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 8);
	if (ret < 0) {
		goto ERROR;
	};

	for (i = 0; i < (MELFAS_TS_FW_MAX_SECT_NUM * 2); i++) {
		ver_buf[i] = rbuf[i];
	}

	ts->plat_data->config_version_of_ic[0] = ver_buf[0];
	ts->plat_data->config_version_of_ic[1] = ver_buf[1];
	ts->plat_data->config_version_of_ic[2] = ver_buf[2];
	ts->plat_data->config_version_of_ic[3] = ver_buf[3];

	ts->plat_data->img_version_of_ic[0] = ver_buf[4];
	ts->plat_data->img_version_of_ic[1] = ver_buf[5];
	ts->plat_data->img_version_of_ic[2] = ver_buf[6];
	ts->plat_data->img_version_of_ic[3] = ver_buf[7];

	input_info(true, &ts->client->dev,
			"%s: boot:%x.%x core:%x.%x %x.%x version:%x.%x\n",
			__func__, ver_buf[0], ver_buf[1], ver_buf[2], ver_buf[3], ver_buf[4],
			ver_buf[5], ver_buf[6], ver_buf[7]);

	return 0;

ERROR:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

#ifdef CONFIG_VBUS_NOTIFIER
int melfas_ts_charger_attached(struct melfas_ts_data *ts, bool status)
{
	u8 wbuf[4];
	int ret = 0;

	input_info(true, &ts->client->dev, "%s [START] %s\n", __func__, status ? "connected" : "disconnected");

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_CHARGER_MODE;
	wbuf[2] = status;

	if ((status == 0) || (status == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
		if (ret)
			input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
	} else {
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, status);
	}
	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

int melfas_ts_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct melfas_ts_data *ts = container_of(nb, struct melfas_ts_data, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	input_info(true, &ts->client->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &ts->client->dev, "%s : attach\n", __func__);
		ts->ta_stsatus = true;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &ts->client->dev, "%s : detach\n", __func__);
		ts->ta_stsatus = false;
		break;
	default:
		break;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s tsp power off\n", __func__);
		return 0;
	}

	melfas_ts_charger_attached(ts, ts->ta_stsatus);
	return 0;
}
#endif

#ifdef USE_TSP_TA_CALLBACKS
void melfas_ts_charger_status_cb(struct tsp_callbacks *cb, int status)
{
	pr_info("%s: TA %s\n",
		__func__, status ? "connected" : "disconnected");

	if (status)
		ta_connected = true;
	else
		ta_connected = false;

	/* not yet defined functions */
}

void melfas_ts_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_info("%s\n", __func__);
}
#endif


MODULE_LICENSE("GPL");
