// SPDX-License-Identifier: GPL-2.0
/* drivers/input/sec_input/stm/stm_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"
#include "stm_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t secure_filter_interrupt(struct stm_ts_data *ts)
{
	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->plat_data->secure_pending_irqs));
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
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->plat_data->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
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
		if (atomic_read(&ts->reset_is_on_going)) {
			input_err(true, &ts->client->dev, "%s: reset is on going because i2c fail\n", __func__);
			return -EBUSY;
		}

		/* Enable Secure World */
		if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);

		/* synchronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		stm_ts_release_all_finger(ts);

		if (stm_pm_runtime_get_sync(ts) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->plat_data->secure_powerdown);
		reinit_completion(&ts->plat_data->secure_interrupt);

		atomic_set(&ts->plat_data->secure_enabled, 1);
		atomic_set(&ts->plat_data->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		stm_pm_runtime_put_sync(ts);
		atomic_set(&ts->plat_data->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		stm_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->plat_data->secure_interrupt);
		complete_all(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->plat_data->secure_pending_irqs));
	}

	complete(&ts->plat_data->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->plat_data->secure_interrupt);
	init_completion(&ts->plat_data->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct stm_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->plat_data->secure_enabled)) {
		atomic_set(&ts->plat_data->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR_RW(secure_touch_enable);
static DEVICE_ATTR_RO(secure_touch);
static DEVICE_ATTR_RO(secure_ownership);

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

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int stm_touch_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct stm_ts_data *ts = container_of(n, struct stm_ts_data, stm_input_nb);
	u8 reg[3];
	int ret = 0;

	if (!ts->info_work_done) {
		input_info(true, &ts->client->dev, "%s: info work is not done. skip\n", __func__);
		return ret;
	}

	if (atomic_read(&ts->plat_data->shutdown_called))
		return -ENODEV;

	reg[0] = STM_TS_CMD_SET_FUNCTION_ONOFF;

	switch (data) {
	case NOTIFIER_TSP_BLOCKING_REQUEST:
		if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE) {
			input_info(true, &ts->client->dev, "%s: Skip tsp block on wireless charger\n", __func__);
		} else {
			reg[1] = STM_TS_CMD_FUNCTION_SET_TSP_BLOCK;
			reg[2] = 1;
			ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
			input_info(true, &ts->client->dev, "%s: tsp block, ret=%d\n", __func__, ret);
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_BLOCK, 0, 0);
		}
		break;
	case NOTIFIER_TSP_BLOCKING_RELEASE:
		if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE) {
			input_info(true, &ts->client->dev, "%s: Skip tsp unblock on wireless charger\n", __func__);
		} else {
			reg[1] = STM_TS_CMD_FUNCTION_SET_TSP_BLOCK;
			reg[2] = 0;
			ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
			input_info(true, &ts->client->dev, "%s: tsp unblock, ret=%d\n", __func__, ret);
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
		}
		break;
	case NOTIFIER_WACOM_PEN_INSERT:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_DETECTION;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen in detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_REMOVE:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_DETECTION;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen out detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_SAVING_MODE_ON:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_SAVING_MODE;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen saving mode on, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_SAVING_MODE_OFF:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_SAVING_MODE;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen saving mode off, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		reg[1] = STM_TS_CMD_FUNCTION_SET_HOVER_DETECTION;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen hover in detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		reg[1] = STM_TS_CMD_FUNCTION_SET_HOVER_DETECTION;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen hover out detect, ret=%d\n", __func__, ret);
		break;
	default:
		break;
	}
	return ret;
}
#endif

void stm_ts_reinit(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = 0;
	int retry = 3;

	do {
		ret = stm_ts_systemreset(ts, 0);
		if (ret < 0)
			stm_ts_reset(ts, 20);
		else
			break;
	} while (--retry);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: Failed to system reset\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev,
			"%s: charger=0x%x, wc=0x%x, touch_functions=0x%x, Power mode=0x%x\n",
			__func__, ts->charger_mode, ts->plat_data->wirelesscharger_mode,
			ts->plat_data->touch_functions, atomic_read(&ts->plat_data->power_state));

	atomic_set(&ts->plat_data->touch_noise_status, 0);
	atomic_set(&ts->plat_data->touch_pre_noise_status, 0);
	ts->plat_data->wet_mode = 0;

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	stm_ts_release_all_finger(ts);

	if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE)
		stm_ts_set_wirelesscharger_mode(ts);

	if (ts->charger_mode != TYPE_WIRE_CHARGER_NONE)
		stm_ts_set_wirecharger_mode(ts);

	stm_ts_set_cover_type(ts, ts->plat_data->touch_functions & STM_TS_TOUCHTYPE_BIT_COVER);

	stm_ts_set_custom_library(ts);
	stm_ts_set_press_property(ts);
	stm_ts_set_fod_finger_merge(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		stm_ts_set_fod_rect(ts);

	/* Power mode */
	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) {
		stm_ts_set_opmode(ts, STM_TS_OPMODE_LOWPOWER);
		sec_delay(50);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			stm_ts_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(&ts->client->dev, GRIP_ALL_DATA);

		stm_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

		if (ts->plat_data->touchable_area) {
			ret = stm_ts_set_touchable_area(ts);
			if (ret < 0)
				goto out;
		}
	}

	if (ts->plat_data->ed_enable)
		stm_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ts->plat_data->pocket_mode)
		stm_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
	if (ts->sip_mode)
		stm_ts_sip_mode_enable(ts);
	if (ts->note_mode)
		stm_ts_note_mode_enable(ts);
	if (ts->game_mode)
		stm_ts_game_mode_enable(ts);
	if (ts->dead_zone)
		stm_ts_dead_zone_enable(ts);
	if (ts->fix_active_mode)
		stm_ts_fix_active_mode(ts, true);
out:
	stm_ts_set_scanmode(ts, ts->scan_mode);
}
/*
 * don't need it in interrupt handler in reality, but, need it in vendor IC for requesting vendor IC.
 * If you are requested additional i2c protocol in interrupt handler by vendor.
 * please add it in stm_ts_external_func.
 */
static void stm_ts_external_func(struct stm_ts_data *ts)
{
	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_IN_IRQ);

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
	ts->plat_data->coord[t_id].hover_id_num = max_t(u8, ts->plat_data->coord[t_id].hover_id_num,
							p_event_coord->hover_id_num);
	ts->plat_data->coord[t_id].noise_status = p_event_coord->noise_status;
	ts->plat_data->coord[t_id].freq_id = p_event_coord->freq_id;
	if (ts->plat_data->freq_id != p_event_coord->freq_id)
		sec_cmd_send_status_uevent(&ts->sec, STATUS_TYPE_FREQ, p_event_coord->freq_id);
	ts->plat_data->freq_id = p_event_coord->freq_id;

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}

int stm_ts_fod_vi_event(struct stm_ts_data *ts)
{
	int ret = 0;

	ts->plat_data->fod_data.vi_data[0] = SEC_TS_CMD_SPONGE_FOD_POSITION;
	ret = ts->stm_ts_read_sponge(ts, ts->plat_data->fod_data.vi_data, ts->plat_data->fod_data.vi_size);
	if (ret < 0)
		input_info(true, &ts->client->dev, "%s: failed read fod vi\n", __func__);

	return ret;
}

static void stm_ts_gesture_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_gesture_status *p_gesture_status;
	int x, y;

	p_gesture_status = (struct stm_ts_gesture_status *)event_buff;

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->stype == STM_TS_SPONGE_EVENT_SWIPE_UP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SPAY, 0, 0);
	} else if (p_gesture_status->stype == STM_TS_GESTURE_CODE_DOUBLE_TAP) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_AOD) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			input_info(true, &ts->client->dev, "%s: AOT\n", __func__);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
			input_sync(ts->plat_data->input_dev);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
			input_sync(ts->plat_data->input_dev);
		}
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_SINGLETAP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_PRESS) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_LONG ||
			p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_NORMAL) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_PRESS, x, y);
			input_info(true, &ts->client->dev, "%s: FOD %sPRESS\n",
					__func__, p_gesture_status->gesture_id ? "" : "LONG");
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_RELEASE) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_RELEASE, x, y);
			input_info(true, &ts->client->dev, "%s: FOD RELEASE\n", __func__);
			memset(ts->plat_data->fod_data.vi_data, 0x0, ts->plat_data->fod_data.vi_size);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_OUT) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_OUT, x, y);
			input_info(true, &ts->client->dev, "%s: FOD OUT\n", __func__);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_VI) {
			if ((ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS) && ts->plat_data->support_fod_lp_mode)
				stm_ts_fod_vi_event(ts);
		} else {
			input_info(true, &ts->client->dev, "%s: invalid id %d\n",
					__func__, p_gesture_status->gesture_id);
		}
	} else if (p_gesture_status->stype  == STM_TS_GESTURE_CODE_DUMPFLUSH) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_LPM) {
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_0)
					stm_ts_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_0);
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_1)
					stm_ts_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_1);
			} else {
				ts->sponge_dump_delayed_flag = true;
				ts->sponge_dump_delayed_area = p_gesture_status->gesture_id;
			}
		}
#endif
	} else if (p_gesture_status->stype  == STM_TS_VENDOR_EVENT_LARGEPALM) {
		input_info(true, &ts->client->dev, "%s: LARGE PALM %s\n", __func__,
					p_gesture_status->gesture_id == 1 ? "RELEASED" : "PRESSED");
		if (p_gesture_status->gesture_id == 0x01)
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 0);
		else
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 1);
		input_sync(ts->plat_data->input_dev);
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_TWO_FINGER_DOUBLETAP) {
		input_info(true, &ts->client->dev, "%s: TWO FINGER DOUBLETAP\n", __func__);
		input_report_key(ts->plat_data->input_dev, KEY_WATCH, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, KEY_WATCH, 0);
		input_sync(ts->plat_data->input_dev);
	}
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

static void stm_ts_status_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_status *p_event_status;

	p_event_status = (struct stm_ts_event_status *)event_buff;

	if (p_event_status->stype > 0)
		input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);

	if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_ERROR) {
		if (p_event_status->status_id == STM_TS_ERR_EVENT_QUEUE_FULL) {
			input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
			stm_ts_release_all_finger(ts);
		} else if (p_event_status->status_id == STM_TS_ERR_EVENT_ESD) {
			input_err(true, &ts->client->dev, "%s: ESD detected\n", __func__);
			if (!atomic_read(&ts->reset_is_on_going))
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFO) {
		if (p_event_status->status_id == STM_TS_INFO_READY_STATUS) {
			if (p_event_status->status_data_1 == 0x10) {
				input_err(true, &ts->client->dev, "%s: IC Reset\n", __func__);
				/* if (!ts->reset_is_on_going)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
				*/
			}
		} else if (p_event_status->status_id == STM_TS_INFO_WET_MODE) {
			ts->plat_data->wet_mode = p_event_status->status_data_1;
			input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
				__func__, ts->plat_data->wet_mode);
			if (ts->plat_data->wet_mode)
				ts->plat_data->hw_param.wet_count++;
			sec_cmd_send_status_uevent(&ts->sec, STATUS_TYPE_WET, p_event_status->status_data_1);
		} else if (p_event_status->status_id == STM_TS_INFO_NOISE_MODE) {
			atomic_set(&ts->plat_data->touch_noise_status, (p_event_status->status_data_1 & 0x0f));
			input_info(true, &ts->client->dev, "%s: NOISE MODE %s[%02X]\n",
					__func__, atomic_read(&ts->plat_data->touch_noise_status) == 0 ? "OFF" : "ON",
					p_event_status->status_data_1);

			if (atomic_read(&ts->plat_data->touch_noise_status))
				ts->plat_data->hw_param.noise_count++;
			ts->plat_data->noise_mode = p_event_status->status_data_1;
			sec_cmd_send_status_uevent(&ts->sec, STATUS_TYPE_NOISE, p_event_status->status_data_1);
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_VENDORINFO) {
		if (ts->plat_data->support_ear_detect) {
			if (p_event_status->status_id == 0x6A) {
				ts->hover_event = p_event_status->status_data_1;
				sec_input_proximity_report(&ts->client->dev, p_event_status->status_data_1);
			}
		}
	}
}

static int stm_ts_get_event(struct stm_ts_data *ts, u8 *data, int *remain_event_count)
{
	int ret = 0;
	u8 address[2] = {0x00, STM_TS_READ_ONE_EVENT};

	ret = ts->stm_ts_read(ts, &address[1], 1, (u8 *)&data[0], STM_TS_EVENT_BUFF_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return ret;
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ONEEVENT)
		input_info(true, &ts->client->dev, "ONE: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1],
				data[2], data[3],
				data[4], data[5],
				data[6], data[7]);

	if (data[0] == 0) {
		input_info(true, &ts->client->dev, "%s: event buffer is empty(%d)\n", __func__, ts->irq_empty_count);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_SEC_FACTORY)
		ts->irq_empty_count++;
		if (ts->irq_empty_count >= 100) {
			ts->irq_empty_count = 0;
			sec_abc_send_event(GET_INT_ABC_TYPE(ts->multi_dev));
		}
#endif
		return SEC_ERROR;
	}

	*remain_event_count = data[7] & 0x1F;

	if (*remain_event_count > MAX_EVENT_COUNT - 1) {
		input_err(true, &ts->client->dev, "%s: event buffer overflow\n", __func__);
		address[0] = STM_TS_CMD_CLEAR_ALL_EVENT;
		ret = ts->stm_ts_write(ts, address, 1, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

		stm_ts_release_all_finger(ts);

		return SEC_ERROR;
	}

	if (*remain_event_count > 0) {
		address[0] = STM_TS_READ_ALL_EVENT;
		ret = ts->stm_ts_read(ts, &address[0], 1, &data[1 * STM_TS_EVENT_BUFF_SIZE],
				(STM_TS_EVENT_BUFF_SIZE) * (*remain_event_count));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return ret;
		}
	}

	return SEC_SUCCESS;
}

irqreturn_t stm_ts_irq_thread(int irq, void *ptr)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)ptr;
	int ret;
	u8 event_id;
	u8 read_event_buff[MAX_EVENT_COUNT * STM_TS_EVENT_BUFF_SIZE] = {0};
	u8 *event_buff;
	int curr_pos;
	int remain_event_count;
	char taas[32];
	char tresult[64];

	ret = event_id = curr_pos = remain_event_count = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled(%d)\n", __func__, ret);

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

	ret = stm_ts_get_event(ts, read_event_buff, &remain_event_count);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);

	do {
		event_buff = &read_event_buff[curr_pos * STM_TS_EVENT_BUFF_SIZE];
		event_id = event_buff[0] & 0x3;
		if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ALLEVENT)
			input_info(true, &ts->client->dev, "ALL: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					event_buff[0], event_buff[1], event_buff[2], event_buff[3],
					event_buff[4], event_buff[5], event_buff[6], event_buff[7]);

		if (event_id == STM_TS_STATUS_EVENT) {
			stm_ts_status_event(ts, event_buff);
		} else if (event_id == STM_TS_COORDINATE_EVENT) {
			stm_ts_coordinate_event(ts, event_buff);
		} else if (event_id == STM_TS_GESTURE_EVENT) {
			stm_ts_gesture_event(ts, event_buff);
		} else if (event_id == STM_TS_VENDOR_EVENT) {
			input_info(true, &ts->client->dev,
				"%s: %s event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				__func__, (event_buff[1] == 0x01 ? "echo" : ""), event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
				event_buff[12], event_buff[13], event_buff[14], event_buff[15]);

			if (event_buff[2] == STM_TS_CMD_SET_GET_OPMODE && event_buff[3] == STM_TS_OPMODE_NORMAL) {
				sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
				input_info(true, &ts->client->dev, "%s: Normal changed\n", __func__);
			} else if (event_buff[2] == STM_TS_CMD_SET_GET_OPMODE && event_buff[3] == STM_TS_OPMODE_LOWPOWER) {
				sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
				input_info(true, &ts->client->dev, "%s: lp changed\n", __func__);
			}

			if ((event_buff[0] == 0xF3) && ((event_buff[1] == 0x02) || (event_buff[1] == 0x46))) {
				if (!atomic_read(&ts->reset_is_on_going))
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
				break;
			}

			if ((event_buff[0] == 0x43) && (event_buff[1] == 0x05) && (event_buff[2] == 0x11)) {
				snprintf(taas, sizeof(taas), "TAAS=CASB");
				snprintf(tresult, sizeof(tresult), "RESULT=STM TSP Force Calibration Event");
				sec_cmd_send_event_to_user(&ts->sec, taas, tresult);
			}
		} else {
			input_info(true, &ts->client->dev,
					"%s: unknown event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
						event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
		}
		curr_pos++;
		remain_event_count--;
	} while (remain_event_count >= 0);

	sec_input_coord_event_sync_slot(&ts->client->dev);

	stm_ts_external_func(ts);

	mutex_unlock(&ts->eventlock);

	return IRQ_HANDLED;
}

int stm_ts_enable(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: probe is not done\n", __func__);
		return 0;
	}

	cancel_delayed_work_sync(&ts->work_read_info);

	atomic_set(&ts->plat_data->enabled, 1);
	ts->plat_data->prox_power_off = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) {
		ts->plat_data->lpmode(ts, TO_TOUCH_MODE);
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
	} else {
		ret = ts->plat_data->start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

	if (ts->fix_active_mode)
		stm_ts_fix_active_mode(ts, true);

	if (ts->plat_data->low_sensitivity_mode) {
		u8 reg[3];

		reg[0] = STM_TS_CMD_SET_FUNCTION_ONOFF;
		reg[1] = STM_TS_CMD_FUNCTION_SET_LOW_SENSITIVITY_MODE;
		reg[2] = ts->plat_data->low_sensitivity_mode;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to set low sensitivity mode\n", __func__);
	}

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);

#ifdef USE_FOD_FEATURE
retry_fodmode:
	if (ts->plat_data->support_fod && (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)) {
		data[0] = STM_TS_CMD_SPONGE_OFFSET_MODE;
		ret = ts->stm_ts_read_sponge(ts, data, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read sponge offset data\n", __func__);
			retrycnt++;
			if (retrycnt < 5)
				goto retry_fodmode;

		}
		input_info(true, &ts->client->dev, "%s: read offset data(0x%x)\n", __func__, data[0]);

		if ((data[0] & SEC_TS_MODE_SPONGE_PRESS) != (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)) {
			input_err(true, &ts->client->dev, "%s: fod data not matched(%d,%d) retry %d\n",
					__func__, (data[0] & SEC_TS_MODE_SPONGE_PRESS),
					(ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS), retrycnt);
			retrycnt++;
			/*rewrite*/
			stm_ts_set_custom_library(ts);
			stm_ts_set_press_property(ts);
			stm_ts_set_fod_finger_merge(ts);
			if (retrycnt < 5)
				goto retry_fodmode;
		}
	}
#endif
	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_work(&ts->work_print_info.work);

	if (atomic_read(&ts->plat_data->power_state) > SEC_INPUT_STATE_POWER_OFF)
		sec_input_forced_enable_irq(ts->irq);

	return 0;
}

int stm_ts_disable(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: probe is not done\n", __func__);
		return 0;
	}

	cancel_delayed_work_sync(&ts->work_read_info);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	atomic_set(&ts->plat_data->enabled, 0);

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(ts->tdata);
#endif
	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, ts->tdata);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	if (atomic_read(&ts->plat_data->pvm->trusted_touch_enabled)) {
		input_info(true, &ts->client->dev, "%s wait for disabling trusted touch\n", __func__);
		wait_for_completion_interruptible(&ts->plat_data->pvm->trusted_touch_powerdown);
	}
#endif
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_cancel_session();
#endif
	cancel_delayed_work(&ts->reset_work);

	if (sec_input_need_ic_off(ts->plat_data))
		ts->plat_data->stop_device(ts);
	else
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);

	return 0;
}

int stm_ts_stop_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	struct irq_desc *desc = irq_to_desc(ts->irq);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (ts->sec.fac_dev)
		get_lp_dump_show(ts->sec.fac_dev, NULL, NULL);

	mutex_lock(&ts->device_mutex);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->irq);
	while (desc->wake_depth > 0)
		disable_irq_wake(ts->irq);

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_OFF);

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->pinctrl_configure(&ts->client->dev, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int stm_ts_start_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = 0;
	u8 address = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	mutex_lock(&ts->device_mutex);

	if (atomic_read(&ts->plat_data->power_state) != SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, true);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);
	atomic_set(&ts->plat_data->touch_noise_status, 0);
	ts->plat_data->init(ts);

	stm_ts_read_chip_id(ts);

	/* Sense_on */
	address = STM_TS_CMD_SENSE_ON;
	ret = ts->stm_ts_write(ts, &address, 1, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);

	enable_irq(ts->irq);
out:
	mutex_unlock(&ts->device_mutex);
	return ret;
}

static int stm_ts_hw_init(struct stm_ts_data *ts)
{
	int ret = 0;
	int retry = 3;
	u8 reg[3] = { 0 };
	u8 data[STM_TS_EVENT_BUFF_SIZE] = { 0 };

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	ts->plat_data->power(&ts->client->dev, true);
	if (!ts->plat_data->regulator_boot_on)
		sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);

	ret = ts->stm_ts_read(ts, &reg[0], 1, data, STM_TS_EVENT_BUFF_SIZE);
	if (ret == -ENOTCONN) {
		input_err(true, &ts->client->dev, "%s: not connected\n", __func__);
		return ret;
	}

	do {
		ret = stm_ts_fw_corruption_check(ts);
		if (ret == -STM_TS_ERROR_FW_CORRUPTION) {
			ts->plat_data->hw_param.checksum_result = 1;
			break;
		} else if (ret < 0) {
			if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM)
				break;
			else if (ts->plat_data->hw_param.checksum_result)
				break;
			else if (ret == -STM_TS_ERROR_TIMEOUT_ZERO)
				stm_ts_read_chip_id_hw(ts);

			stm_ts_systemreset(ts, 20);
		} else {
			break;
		}
	} while (--retry);

	stm_ts_get_version_info(ts);

	if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
		ret = stm_ts_osc_trim_recovery(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
	}

	if (ts->plat_data->hw_param.checksum_result) {
		ts->fw_version_of_ic = 0;
		ts->config_version_of_ic = 0;
		ts->fw_main_version_of_ic = 0;
	}

	stm_ts_read_chip_id(ts);

	ret = stm_ts_fw_update_on_probe(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to firmware update\n",
				__func__);
		return -STM_TS_ERROR_FW_UPDATE_FAIL;
	}

	ret = stm_ts_get_channel_info(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
		return ret;
	}

	ts->pFrame = kzalloc(ts->rx_count * ts->tx_count * 2 + 1, GFP_KERNEL);
	if (!ts->pFrame)
		return -ENOMEM;

	ts->cx_data = kzalloc(ts->rx_count * ts->tx_count + 1, GFP_KERNEL);
	if (!ts->cx_data) {
		kfree(ts->pFrame);
		return -ENOMEM;
	}

	ts->ito_result = kzalloc(STM_TS_ITO_RESULT_PRINT_SIZE, GFP_KERNEL);
	if (!ts->ito_result) {
		kfree(ts->cx_data);
		kfree(ts->pFrame);
		return -ENOMEM;
	}

	/* fts driver set functional feature */
	ts->plat_data->touch_count = 0;
	ts->touch_opmode = STM_TS_OPMODE_NORMAL;
	ts->charger_mode = STM_TS_BIT_CHARGER_MODE_NORMAL;
	ts->irq_empty_count = 0;

#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif

	ts->plat_data->touch_functions = STM_TS_TOUCHTYPE_DEFAULT_ENABLE;
	stm_ts_set_touch_function(ts);
	sec_delay(10);

	stm_ts_command(ts, STM_TS_CMD_FORCE_CALIBRATION, true);
	stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	ts->scan_mode = STM_TS_SCAN_MODE_DEFAULT;
	stm_ts_set_scanmode(ts, ts->scan_mode);

	input_info(true, &ts->client->dev, "%s: Initialized\n", __func__);

	return ret;
}

static int stm_ts_init(struct stm_ts_data *ts)
{
	int ret = 0;

	if (ts->client->dev.of_node) {
		ret = sec_input_parse_dt(&ts->client->dev);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
#ifdef TCLM_CONCEPT
		sec_tclm_parse_dt(&ts->client->dev, ts->tdata);
#endif
		sec_input_multi_device_create(&ts->client->dev);
		ts->multi_dev = ts->plat_data->multi_dev;
	} else {
		ts->plat_data = ts->client->dev.platform_data;
		if (!ts->plat_data) {
			ret = -ENOMEM;
			input_err(true, &ts->client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	ts->plat_data->pinctrl = devm_pinctrl_get(&ts->client->dev);
	if (IS_ERR(ts->plat_data->pinctrl))
		input_err(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);

	ts->client->irq = gpio_to_irq(ts->plat_data->irq_gpio);

	ts->irq = ts->client->irq;
	ts->stm_ts_systemreset = stm_ts_systemreset;
	ts->stm_ts_command = stm_ts_command;
	ts->plat_data->dev =  &ts->client->dev;
	ts->plat_data->bus_master = &ts->client->adapter->dev;
	ts->plat_data->irq = ts->irq;
	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->start_device = stm_ts_start_device;
	ts->plat_data->stop_device = stm_ts_stop_device;
	ts->plat_data->init = stm_ts_reinit;
	ts->plat_data->lpmode = stm_ts_set_lowpowermode;
	ts->plat_data->set_grip_data = stm_set_grip_data_to_ic;
	ts->plat_data->set_temperature = stm_ts_set_temperature;

	ptsp = &ts->client->dev;

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
	INIT_DELAYED_WORK(&ts->work_read_functions, stm_ts_get_touch_function);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	INIT_DELAYED_WORK(&ts->plat_data->interrupt_notify_work, stm_ts_interrupt_notify);
#endif

	mutex_init(&ts->device_mutex);
	mutex_init(&ts->read_write_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->sponge_mutex);
	mutex_init(&ts->fn_mutex);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	ret = sec_input_device_register(&ts->client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	mutex_init(&ts->plat_data->enable_mutex);
	ts->plat_data->enable = stm_ts_enable;
	ts->plat_data->disable = stm_ts_disable;

	ret = stm_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

	if (IS_FOLD_DEV(ts->multi_dev)) {
		char dev_name[16];

		snprintf(dev_name, sizeof(dev_name), "TSP-%s: ", ts->multi_dev->name);
		ts->plat_data->sec_ws = wakeup_source_register(ts->sec.fac_dev, dev_name);
	} else {
		ts->plat_data->sec_ws = wakeup_source_register(ts->sec.fac_dev, "TSP");
	}

	ret = device_init_wakeup(ts->sec.fac_dev, true);
	input_info(true, &ts->client->dev, "%s: device_init_wakeup: fac_dev: ret: %d\n", __func__, ret);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	sec_input_dumpkey_register(GET_DEV_COUNT(ts->multi_dev), stm_ts_dump_tsp_log, &ts->client->dev);
	INIT_DELAYED_WORK(&ts->check_rawdata, stm_ts_check_rawdata);
#endif
	input_info(true, &ts->client->dev, "%s: init resource\n", __func__);

	return 0;

err_fn_init:
	ts->plat_data->enable = NULL;
	ts->plat_data->disable = NULL;
err_register_input_device:
error_allocate_pdata:
error_allocate_mem:
	input_err(true, &ts->client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void stm_ts_release(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->enable = NULL;
	ts->plat_data->disable = NULL;

	sec_input_multi_device_remove(ts->multi_dev);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (ts->stm_input_nb.notifier_call)
		sec_input_unregister_notify(&ts->stm_input_nb);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&ts->vbus_nb);
#endif
	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_functions);
	cancel_delayed_work_sync(&ts->reset_work);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	cancel_delayed_work_sync(&ts->plat_data->interrupt_notify_work);
#endif
	flush_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->check_rawdata);
	sec_input_dumpkey_unregister(GET_DEV_COUNT(ts->multi_dev));
#endif
	device_init_wakeup(ts->sec.fac_dev, false);
	stm_ts_fn_remove(ts);

	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;
	ts->probe_done = false;

	ts->plat_data->power(&ts->client->dev, false);
}


int stm_ts_probe(struct stm_ts_data *ts)
{
	int ret = 0;
	static int deferred_flag;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &ts->client->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = stm_ts_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ret = stm_ts_hw_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		stm_ts_release(ts);
		return ret;
	}

	stm_ts_get_custom_library(ts);
	stm_ts_set_custom_library(ts);

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, ts->client->irq);
	ret = request_threaded_irq(ts->client->irq, NULL, stm_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, STM_TS_I2C_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		stm_ts_release(ts);
		return ret;
	}

	ts->probe_done = true;
	atomic_set(&ts->plat_data->enabled, 1);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	ret = sec_trusted_touch_init(&ts->client->dev);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to init trusted touch\n", __func__);
	}
#endif

	sec_secure_touch_register(ts, ts->dev, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->stm_input_nb, stm_touch_notify_call, 2);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&ts->vbus_nb, stm_ts_vbus_notification,
						VBUS_NOTIFY_DEV_CHARGER);
#endif

	input_err(true, &ts->client->dev, "%s: done\n", __func__);
	input_log_fix();

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

	return 0;
}

int stm_ts_remove(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->plat_data->enable_mutex);
	atomic_set(&ts->plat_data->shutdown_called, 1);
	mutex_unlock(&ts->plat_data->enable_mutex);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	stm_ts_release(ts);

	return 0;
}

void stm_ts_shutdown(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	stm_ts_remove(ts);
}


#if IS_ENABLED(CONFIG_PM)
int stm_ts_pm_suspend(struct stm_ts_data *ts)
{
	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

int stm_ts_pm_resume(struct stm_ts_data *ts)
{
	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif

MODULE_LICENSE("GPL");
