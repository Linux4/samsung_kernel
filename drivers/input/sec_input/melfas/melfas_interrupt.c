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

static void melfas_ts_coord_parsing(struct melfas_ts_data *ts, struct melfas_ts_event_coordinate *p_event_coord, u8 t_id)
{
	ts->plat_data->coord[t_id].id = t_id;
	ts->plat_data->coord[t_id].action = p_event_coord->tchsta;
	ts->plat_data->coord[t_id].x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p_event_coord->z & 0x3F;
	ts->plat_data->coord[t_id].ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
	ts->plat_data->coord[t_id].major = p_event_coord->major;
	ts->plat_data->coord[t_id].minor = p_event_coord->minor;

	if (!ts->plat_data->coord[t_id].palm && (ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_PALM))
		ts->plat_data->coord[t_id].palm_count++;

	ts->plat_data->coord[t_id].palm = (ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_PALM);
	if (ts->plat_data->coord[t_id].palm)
		ts->plat_data->palm_flag |= (1 << t_id);
	else
		ts->plat_data->palm_flag &= ~(1 << t_id);

	ts->plat_data->coord[t_id].left_event = p_event_coord->left_event;

	ts->plat_data->coord[t_id].noise_level = max(ts->plat_data->coord[t_id].noise_level,
							p_event_coord->noise_level);
	ts->plat_data->coord[t_id].max_strength = max(ts->plat_data->coord[t_id].max_strength,
							p_event_coord->max_strength);
	ts->plat_data->coord[t_id].hover_id_num = max(ts->plat_data->coord[t_id].hover_id_num,
							(u8)p_event_coord->hover_id_num);

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}

int melfas_ts_disable_esd_alert(struct melfas_ts_data *ts)
{
	int ret = 0;
	u8 wbuf[4];
	u8 rbuf[4];

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;

	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto ERROR;
	}

	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		goto ERROR;
	}

	if (rbuf[0] != 1) {
		input_info(true, &ts->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

static int melfas_ts_alert_handler_esd(struct melfas_ts_data *ts, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];

	input_info(true, &ts->client->dev, "%s [START] - frame_cnt[%d]\n",
		__func__, frame_cnt);

	if (frame_cnt == 0) {
		ts->esd_cnt++;
		input_info(true, &ts->client->dev, "%s - esd_cnt[%d]\n",
			__func__, ts->esd_cnt);

		if (ts->disable_esd == true) {
			melfas_ts_disable_esd_alert(ts);
		} else if (ts->esd_cnt > MELFAS_TS_ESD_COUNT_FOR_DISABLE) {
			//Disable ESD alert
			if (melfas_ts_disable_esd_alert(ts))
				input_err(true, &ts->client->dev,
					"%s - fail to disable esd alert\n", __func__);
			else
				ts->disable_esd = true;
		} else {
			//Reset chip
			if (!ts->reset_is_on_going)
				schedule_work(&ts->reset_work.work);
		}
	} else {
		//ESD detected
		//Reset chip
		if (!ts->reset_is_on_going)
			schedule_work(&ts->reset_work.work);
		ts->esd_cnt = 0;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return SEC_SUCCESS;
}

/*
 * Alert event handler - SRAM failure
 */
static int melfas_ts_alert_handler_sram(struct melfas_ts_data *ts, u8 *data)
{
	int i;

	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);

	ts->sram_addr_num = (unsigned int) (data[0] | (data[1] << 8));
	input_info(true, &ts->client->dev, "%s - sram_addr_num [%d]\n", __func__, ts->sram_addr_num);

	if (ts->sram_addr_num > 8) {
		input_err(true, &ts->client->dev, "%s [ERROR] sram_addr_num [%d]\n",
						__func__, ts->sram_addr_num);
		goto error;
	}

	for (i = 0; i < ts->sram_addr_num; i++) {
		ts->sram_addr[i] = data[2 + 4 * i] | (data[2 + 4 * i + 1] << 8) |
							(data[2 + 4 * i + 2] << 16) | (data[2 + 4 * i + 3] << 24);
		input_info(true, &ts->client->dev, "%s - sram_addr #%d [0x%08X]\n",
						__func__, i, ts->sram_addr[i]);
	}

	for (i = ts->sram_addr_num; i < 8; i++) {
		ts->sram_addr[i] = 0;
		input_info(true, &ts->client->dev, "%s - sram_addr #%d [0x%08X]\n",
						__func__, i, ts->sram_addr[i]);
	}

	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return SEC_SUCCESS;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return SEC_ERROR;
}

static void melfas_ts_gesture_event(struct melfas_ts_data *ts, u8 *event_buff)
{
	struct melfas_ts_gesture_status *p_gesture_status;
	int x, y;

	p_gesture_status = (struct melfas_ts_gesture_status *)&event_buff[2];

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->stype == MELFAS_TS_GESTURE_CODE_SWIPE) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SPAY, 0, 0);
	} else if (p_gesture_status->stype == MELFAS_TS_GESTURE_CODE_DOUBLE_TAP) {
		if (p_gesture_status->gesture_id == MELFAS_TS_GESTURE_ID_AOD) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
		} else if (p_gesture_status->gesture_id == MELFAS_TS_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			input_info(true, &ts->client->dev, "%s: AOT\n", __func__);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
			input_sync(ts->plat_data->input_dev);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
			input_sync(ts->plat_data->input_dev);
		}
	} else if (p_gesture_status->stype  == MELFAS_TS_GESTURE_CODE_SINGLE_TAP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
	} else if (p_gesture_status->stype  == MELFAS_TS_GESTURE_CODE_PRESS) {
		input_info(true, &ts->client->dev, "%s: FOD: %sPRESS\n",
				__func__, p_gesture_status->gesture_id ? "" : "LONG");
	} else if (p_gesture_status->stype  == MELFAS_TS_GESTURE_CODE_DUMPFLUSH) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
				if (p_gesture_status->gesture_id == MELFAS_TS_SPONGE_DUMP_0)
					melfas_ts_sponge_dump_flush(ts, MELFAS_TS_SPONGE_DUMP_0);
				if (p_gesture_status->gesture_id == MELFAS_TS_SPONGE_DUMP_1)
					melfas_ts_sponge_dump_flush(ts, MELFAS_TS_SPONGE_DUMP_1);
			} else {
				ts->sponge_dump_delayed_flag = true;
				ts->sponge_dump_delayed_area = p_gesture_status->gesture_id;
			}
		}
#endif
	}
}

static int melfas_ts_alert_handler_mode_state(struct melfas_ts_data *ts, u8 data)
{
	if (data == ENTER_NOISE_MODE) {
		ts->plat_data->hw_param.noise_count++;
		input_info(true, &ts->client->dev, "%s: NOISE ON[%d]\n", __func__, data);
	} else if (data == EXIT_NOISE_MODE) {
		input_info(true, &ts->client->dev, "%s: NOISE OFF[%d]\n", __func__, data);
	} else if (data == ENTER_WET_MODE) {
		input_info(true, &ts->client->dev, "%s: WET MODE ON[%d]\n", __func__, data);
		ts->plat_data->wet_mode = 1;
		ts->plat_data->hw_param.wet_count++;
	} else if (data == EXIT_WET_MODE) {
		input_info(true, &ts->client->dev, "%s: WET MODE OFF[%d]\n", __func__, data);
		ts->plat_data->wet_mode = 0;
	} else if (data == MODE_STATE_VSYNC_ON) {
		input_info(true, &ts->client->dev, "%s: VSYNC ON[%d]\n", __func__, data);
	} else if (data == MODE_STATE_VSYNC_OFF) {
		input_info(true, &ts->client->dev, "%s: VSYNC OFF[%d]\n", __func__, data);
	} else {
		input_info(true, &ts->client->dev, "%s: NOT DEFINED[%d]\n", __func__, data);
		return 1;
	}

	return 0;
}

int melfas_ts_alert_handler_sponge(struct melfas_ts_data *ts, u8 *rbuf)
{
	input_dbg(true, &ts->client->dev, "%s [START]\n", __func__);

	if (rbuf[1] == 2)
		melfas_ts_gesture_event(ts, rbuf);
	else
		input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);

	return 0;
}

static int melfas_ts_alert_handler_proximity_state(struct melfas_ts_data *ts, u8 data)
{
	if ((ts->plat_data->support_ear_detect)) {
		ts->hover_event = data;
		input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM, data);
		input_sync(ts->plat_data->input_dev_proximity);
		input_info(true, &ts->client->dev, "%s: proximity: %d\n", __func__, data);
	}

	return 0;
}

static void melfas_ts_alert_event(struct melfas_ts_data *ts, u8 *event_buff)
{
	if (event_buff[0] == MELFAS_TS_ALERT_ESD) {
		if (melfas_ts_alert_handler_esd(ts, event_buff))
			goto ERROR;
	} else if (event_buff[0] == MELFAS_TS_ALERT_SPONGE_GESTURE) {
		if (melfas_ts_alert_handler_sponge(ts, event_buff))
			goto ERROR;
	} else if (event_buff[0] == MELFAS_TS_ALERT_SRAM_FAILURE) {
		if (melfas_ts_alert_handler_sram(ts, &event_buff[1]))
			goto ERROR;
	} else if (event_buff[0] == MELFAS_TS_ALERT_MODE_STATE) {
		if (melfas_ts_alert_handler_mode_state(ts, event_buff[1]))
			goto ERROR;
	} else if (event_buff[0] == MELFAS_TS_ALERT_POCKET_MODE_STATE) {
		if (melfas_ts_alert_handler_proximity_state(ts, event_buff[1]))
			goto ERROR;
	} else if (event_buff[0] == MELFAS_TS_ALERT_PROXIMITY_STATE) {
		if (melfas_ts_alert_handler_proximity_state(ts, event_buff[1]))
			goto ERROR;
	} else {
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown alert type [%d]\n",
			__func__, event_buff[0]);
		goto ERROR;
	}
	return;
ERROR:
	if (!ts->reset_is_on_going)
		schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
	return;
}

static void melfas_ts_coordinate_event(struct melfas_ts_data *ts, u8 *event_buff)
{
	struct melfas_ts_event_coordinate *p_event_coord;
	u8 t_id = 0;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev,
				"%s: device is closed %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
		return;
	}

	p_event_coord = (struct melfas_ts_event_coordinate *)event_buff;

	t_id = p_event_coord->tid - 1;

	if (t_id < SEC_TS_SUPPORT_TOUCH_COUNT) {
		ts->plat_data->prev_coord[t_id] = ts->plat_data->coord[t_id];
		melfas_ts_coord_parsing(ts, p_event_coord, t_id);

		if ((ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_NORMAL)
				|| (ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_PALM)
				|| (ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_WET)
				|| (ts->plat_data->coord[t_id].ttype == MELFAS_TS_TOUCHTYPE_GLOVE)) {
			sec_input_coord_event(&ts->client->dev, t_id);
		} else {
			input_err(true, &ts->client->dev,
					"%s: do not support coordinate type(%d)\n",
					__func__, ts->plat_data->coord[t_id].ttype);
		}
	} else {
		input_err(true, &ts->client->dev, "%s: tid(%d) p_event_coord->tid(%d) is out of range\n", __func__, t_id, p_event_coord->tid);
		input_err(true, &ts->client->dev, "%s: %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
	}
}

static int melfas_ts_get_event(struct melfas_ts_data *ts, u8 *data, int *remain_event_count)
{
	int ret = 0;
	u8 address[2] = { 0,};
	u8 one_event_data = 0;
	int size = 0;

	address[0] = MELFAS_TS_R0_EVENT;
	address[1] = MELFAS_TS_R1_EVENT_PACKET_INFO;
	ret = ts->melfas_ts_i2c_read(ts, address, 2, (u8 *)&one_event_data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return ret;
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ONEEVENT)
		input_info(true, &ts->client->dev, "%s: one event %02X \n", __func__, one_event_data);

	ts->event_id = ((one_event_data >> 7) & 0x1);
	*remain_event_count = one_event_data & 0x7F;
	if (ts->event_id == 0)
		size = (one_event_data & 0x7F) * MELFAS_TS_EVENT_BUFF_SIZE;
	else
		size = one_event_data & 0x7F;

	if ((size > MAX_FINGER_NUM * MELFAS_TS_EVENT_BUFF_SIZE) || (size == 0)) {
		input_info(true, &ts->client->dev, "%s event_id:%d, packet size = %d\n", __func__, ts->event_id, size);
		size = MAX_FINGER_NUM * MELFAS_TS_EVENT_BUFF_SIZE;
		address[0] = MELFAS_TS_R0_EVENT;
		address[1] = MELFAS_TS_R1_EVENT_PACKET_DATA;
		ts->melfas_ts_i2c_read(ts, address, 2, data, size);
		melfas_ts_unlocked_release_all_finger(ts);
		return SEC_ERROR;
	}

	address[0] = MELFAS_TS_R0_EVENT;
	address[1] = MELFAS_TS_R1_EVENT_PACKET_DATA;
	ret = ts->melfas_ts_i2c_read(ts, address, 2, data, size);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return ret;
	}

	return SEC_SUCCESS;
}

irqreturn_t melfas_ts_irq_thread(int irq, void *ptr)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)ptr;
	int ret;
	u8 read_event_buff[MAX_FINGER_NUM * MELFAS_TS_EVENT_BUFF_SIZE] = {0,};
	u8 *event_buff;
	int curr_pos;
	int remain_event_count;
	ret = curr_pos = remain_event_count = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;

	ret = melfas_ts_get_event(ts, read_event_buff, &remain_event_count);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);
	if (ts->event_id == MELFAS_TS_COORDINATE_EVENT) {
		do {
			event_buff = &read_event_buff[curr_pos * MELFAS_TS_EVENT_BUFF_SIZE];
			melfas_ts_coordinate_event(ts, event_buff);
			curr_pos++;
			remain_event_count--;
		} while (remain_event_count > 0);
	} else if (ts->event_id == MELFAS_TS_ALERT_EVENT) {
		event_buff = read_event_buff;
		melfas_ts_alert_event(ts, event_buff);
	} else {
		input_info(true, &ts->client->dev,
				"%s: unknown event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
					event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
					event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
	}
	mutex_unlock(&ts->eventlock);

	return IRQ_HANDLED;
}

MODULE_LICENSE("GPL");
