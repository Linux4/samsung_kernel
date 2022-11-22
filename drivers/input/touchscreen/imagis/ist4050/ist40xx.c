/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#include "ist40xx.h"
#include "ist40xx_misc.h"
#include "ist40xx_update.h"
#include "ist40xx_cmcs.h"

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/t-base-tui.h>
#endif

struct ist40xx_data *ts_data;

#ifdef CONFIG_DISPLAY_SAMSUNG
extern int get_lcd_attached(char *mode);
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
struct ist40xx_data *tui_tsp_info;
extern int tui_force_close(uint32_t arg);
#endif

int ist40xx_log_level = IST40XX_LOG_LEVEL;
void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (ist40xx_log_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", IST40XX_LOG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(struct ist40xx_data *data)
{
	ktime_get_ts(&data->t_current);

	return data->t_current.tv_sec * 1000 +
		data->t_current.tv_nsec / 1000000;
}

void ist40xx_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

int ist40xx_intr_wait(struct ist40xx_data *data, long ms)
{
	long start_ms = get_milli_second(data);
	long curr_ms = 0;

	while (1) {
		if (!data->irq_working)
			break;

		curr_ms = get_milli_second(data);
		if ((curr_ms < 0) || (start_ms < 0)
			|| (curr_ms - start_ms > ms)) {
			input_info(true, &data->client->dev, "%s: timeout(%dms)\n",
				   __func__, ms);
			return -EPERM;
		}

		ist40xx_delay(2);
	}
	return 0;
}

void ist40xx_disable_irq(struct ist40xx_data *data)
{
	if (data->irq_enabled) {
		disable_irq(data->client->irq);
		data->irq_enabled = false;
		data->status.event_mode = false;
	}
}

void ist40xx_enable_irq(struct ist40xx_data *data)
{
	if (!data->irq_enabled) {
		enable_irq(data->client->irq);
		ist40xx_delay(10);
		data->irq_enabled = true;
		data->status.event_mode = true;
	}
}

void ist40xx_scheduled_reset(struct ist40xx_data *data)
{
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s return, TUI is enabled!\n",
			  __func__);
		return;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
		if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
			input_err(true, &data->client->dev,
				  "%s: TSP no accessible from Linux, TUI is enabled!\n",
				  __func__);
			return;
		}
#endif

	if (data->initialized)
		schedule_delayed_work(&data->work_reset_check, 0);
}

static void ist40xx_request_reset(struct ist40xx_data *data)
{
	data->irq_err_cnt++;
	if (data->irq_err_cnt >= data->max_irq_err_cnt) {
		input_info(true, &data->client->dev, "%s\n", __func__);
		ist40xx_scheduled_reset(data);
		data->irq_err_cnt = 0;
	}
}

void ist40xx_start(struct ist40xx_data *data)
{
	u32 write_data;

	if (data->initialized) {
		data->scan_count = 0;
		data->scan_retry = 0;
		mod_timer(&data->event_timer,
				get_jiffies_64() + EVENT_TIMER_INTERVAL);
	}

	data->ignore_delay = true;

	/* TA mode */
	ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);

	if ((data->noise_mode >> NOISE_MODE_REJECTZONE) & 1) {
		ist40xx_write_buf(data->client, eHCOM_SET_REJECTZONE_TOP, (u32 *)&data->rejectzone_t, 1);
		ist40xx_write_buf(data->client, eHCOM_SET_REJECTZONE_BOTTOM, (u32 *)&data->rejectzone_b, 1);
		input_info(true, &data->client->dev, "%s: rejectzone T:%d, B:%d\n",
				__func__, data->rejectzone_t, data->rejectzone_b);
	}

	if (data->report_rate >= 0) {
		ist40xx_write_buf(data->client, eHCOM_SET_TIME_ACTIVE, (u32 *)&data->report_rate, 1);
		input_info(true, &data->client->dev, "%s: active rate : %dus\n",
				__func__, data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist40xx_write_buf(data->client, eHCOM_SET_TIME_IDLE, (u32 *)&data->idle_rate, 1);
		input_info(true, &data->client->dev, "%s: idle rate : %dus\n",
				__func__, data->idle_rate);
	}

	if (data->rec_mode) {
		write_data = IST40XX_ENABLE;
		ist40xx_write_buf(data->client, eHCOM_SET_REC_MODE, (u32 *)&write_data, 1);
		input_info(true, &data->client->dev, "%s: rec mode start\n", __func__);
	}

	if (data->debug_mode || data->rec_mode) {
		write_data = IST40XX_DISABLE;
		ist40xx_write_buf(data->client, eHCOM_SLEEP_MODE_EN, (u32 *)&write_data, 1);
		input_info(true, &data->client->dev, "%s: nosleep mode start\n", __func__);
	}

	ist40xx_cmd_start_scan(data);

	data->ignore_delay = false;

	if (data->rec_mode) {
		ist40xx_delay(100);
		write_data = IST40XX_START_SCAN;
		ist40xx_write_buf(data->client, eHCOM_SET_REC_MODE, (u32 *)&write_data, 1);
	}

	input_info(true, &data->client->dev, "%s: mode : 0x%x\n", __func__,
		   data->noise_mode & 0xFFFF);
}

int ist40xx_set_input_device(struct ist40xx_data *data)
{
	int ret;

	data->input_dev->name = "sec_touchscreen";
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &data->client->dev;

	input_mt_init_slots(data->input_dev, IST40XX_MAX_FINGER_ID, 0);

	set_bit(EV_SYN, data->input_dev->evbit);
	set_bit(EV_ABS, data->input_dev->evbit);
	set_bit(EV_KEY, data->input_dev->evbit);
	set_bit(BTN_TOUCH, data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, data->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, data->input_dev->propbit);
	set_bit(KEY_WAKEUP, data->input_dev->keybit);
	set_bit(KEY_INT_CANCEL, data->input_dev->keybit);

	input_set_abs_params(data->input_dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_X, 0,
				 data->tsp_info.width - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_Y, 0,
				 data->tsp_info.height - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_TOUCH_MAJOR, 0,
				 IST40XX_MAX_MAJOR, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_TOUCH_MINOR, 0,
				 IST40XX_MAX_MINOR, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_PRESSURE, 0, IST40XX_MAX_Z,
				 0, 0);

	input_set_capability(data->input_dev, EV_KEY, KEY_BLACK_UI_GESTURE);

	input_set_drvdata(data->input_dev, data);
	ret = input_register_device(data->input_dev);
	input_info(true, &data->client->dev, "%s: input register device:%d\n",
		   __func__, ret);
	if (ret)
		return ret;

	ret = input_register_device(data->input_dev_proximity);
	input_info(true, &data->client->dev, "%s: input proximity register device:%d\n",
		   __func__, ret);

	return ret;
}

int ist40xx_get_info(struct ist40xx_data *data)
{
	int ret;
	u32 calib_msg;
	u8 fod_info[4];
	u32 write_data;

	ist40xx_disable_irq(data);

	write_data = IST40XX_ENABLE;
	ist40xx_write_buf(data->client, eHCOM_FW_HOLD, (u32 *)&write_data, 1);
	ist40xx_delay(20);

	mutex_lock(&data->lock);
	ret = ist40xx_get_tsp_info(data);
	if (ret) {
		mutex_unlock(&data->lock);
		goto err_get_info;
	}
	mutex_unlock(&data->lock);

	ist40xx_print_info(data);

	ret = ist40xx_set_input_device(data);
	if (ret)
		goto err_get_info;

	ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_S, &calib_msg);
	input_info(true, &data->client->dev, "SLF calib result: 0x%08X\n", calib_msg);
	ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_M, &calib_msg);
	input_info(true, &data->client->dev, "MTL calib result: 0x%08X\n", calib_msg);
	ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_P, &calib_msg);
	input_info(true, &data->client->dev, "PROX calib result: 0x%08X\n", calib_msg);

	if (data->dt_data->support_fod) {
		ist40xx_read_buf(data->client, IST40XX_SPONGE_REG_BASE + IST40XX_SPONGE_FOD_INFO, (u32*)&fod_info, 1);

		data->fod_tx = fod_info[0];
		data->fod_rx = fod_info[1];
		data->fod_vi_size = fod_info[2];

		input_info(true, &data->client->dev, "%s fod_tx[%d] fod_rx[%d] fod_vi_size[%d] %02X%02X%02X%02X\n",
					__func__, data->fod_tx, data->fod_rx, data->fod_vi_size,
					fod_info[0], fod_info[1], fod_info[2], fod_info[3]); 
	}

	write_data = IST40XX_DISABLE;
	ret = ist40xx_write_buf(data->client, eHCOM_FW_HOLD, (u32 *)&write_data, 1);
	if (ret) {
		input_err(true, &data->client->dev, "%s: fail to disable hold\n",
			   __func__);
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		mutex_unlock(&data->lock);
	}

	ist40xx_enable_irq(data);

	return 0;

err_get_info:

	return ret;
}

#define SPECIAL_MAGIC_STRING		(0x4170CF00)
#define SPECIAL_MAGIC_MASK		(0xFFFFFF00)
#define SPECIAL_MESSAGE_MASK		(~SPECIAL_MAGIC_MASK)
#define PARSE_SPECIAL_MESSAGE(n)	((n & SPECIAL_MAGIC_MASK) == SPECIAL_MAGIC_STRING ? \
						(n & SPECIAL_MESSAGE_MASK) : -EINVAL)
#define SPECIAL_START_MASK		(0x80)
#define SPECIAL_SUCCESS_MASK		(0x0F)
void ist40xx_special_cmd(struct ist40xx_data *data, int cmd)
{
	gesture_msg g_msg;

	if (cmd == 0) {
		ist40xx_burst_read(data->client, IST40XX_HIB_GESTURE_MSG, g_msg.full,
				sizeof(g_msg.full) / IST40XX_DATA_LEN, true);

		input_info(true, &data->client->dev, "g_msg %d, %d, %d\n",
					g_msg.b.eid, g_msg.b.gtype, g_msg.b.gid);

		if (g_msg.b.eid == EID_GESTURE) {
			switch (g_msg.b.gtype) {
			case G_TYPE_SWIPE:
				if (g_msg.b.gid == G_ID_SWIPE_UP) {
					data->scrub_id = SPONGE_EVENT_TYPE_SPAY;
					data->scrub_x = 0;
					data->scrub_y = 0;
					data->all_spay_count++;

					input_info(true, &data->client->dev, "Swipe Up Trigger\n");

					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 true);
					input_sync(data->input_dev);
					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 false);
					input_sync(data->input_dev);
				}
				break;
			case G_TYPE_DOUBLETAP:
				if (g_msg.b.gid == G_ID_AOD_DOUBLETAP) {
					data->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
					data->scrub_x = (g_msg.b.gdata[0] << 4) |
									((g_msg.b.gdata[2] >> 4) & 0xF);
					data->scrub_y = (g_msg.b.gdata[1] << 4) |
									(g_msg.b.gdata[2] & 0xF);
					data->all_aod_tsp_count++;

					input_info(true, &data->client->dev,
						   "Double Tap Trigger (%d, %d)\n",
						   data->scrub_x, data->scrub_y);

					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 true);
					input_sync(data->input_dev);
					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 false);
					input_sync(data->input_dev);
				} else if (g_msg.b.gid == G_ID_DOUBLETAP_WAKEUP) {
					input_info(true, &data->client->dev,
							"AOT Double Tap Trigger\n");

					input_report_key(data->input_dev,
							 KEY_WAKEUP,
							 true);
					input_sync(data->input_dev);
					input_report_key(data->input_dev,
							 KEY_WAKEUP,
							 false);
					input_sync(data->input_dev);
					/* request from sensor team */
					input_report_abs(data->input_dev_proximity, 
							ABS_MT_CUSTOM2,
							true);
					input_sync(data->input_dev_proximity);
					input_report_abs(data->input_dev_proximity, 
							ABS_MT_CUSTOM2,
							false);
					input_sync(data->input_dev_proximity);
				}
				break;
			case G_TYPE_SINGLETAP:
				if (g_msg.b.gid == G_ID_SINGLETAP) {
					data->scrub_id = SPONGE_EVENT_TYPE_SINGLETAP;
					data->scrub_x = (g_msg.b.gdata[0] << 4) |
									((g_msg.b.gdata[2] >> 4) & 0xF);
					data->scrub_y = (g_msg.b.gdata[1] << 4) |
									(g_msg.b.gdata[2] & 0xF);
					data->all_singletap_count++;

					input_info(true, &data->client->dev,
						   "Single Tap Trigger (%d, %d)\n",
						   data->scrub_x, data->scrub_y);

					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 true);
					input_sync(data->input_dev);
					input_report_key(data->input_dev,
							 KEY_BLACK_UI_GESTURE,
							 false);
					input_sync(data->input_dev);
				}
				break;
			case G_TYPE_PRESSURE:
				break;
			case G_TYPE_PRESS:
				if (g_msg.b.gid == G_ID_FOD_LONG || g_msg.b.gid == G_ID_FOD_NORMAL) {
					data->scrub_id = SPONGE_EVENT_TYPE_FOD;
					input_info(true, &data->client->dev,
						"FOD %s\n", g_msg.b.gid ? "normal" : "long");
				} else if (g_msg.b.gid == G_ID_FOD_RELEASE) {
					data->scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;
					input_info(true, &data->client->dev,
						"FOD release\n");
				} else if (g_msg.b.gid == G_ID_FOD_OUT) {
					data->scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;
					input_info(true, &data->client->dev,
						"FOD out\n");
				}
				input_report_key(data->input_dev,
						 KEY_BLACK_UI_GESTURE,
						 true);
				input_sync(data->input_dev);
				input_report_key(data->input_dev,
						 KEY_BLACK_UI_GESTURE,
						 false);
				input_sync(data->input_dev);
				break;
			default:
				input_err(true, &data->client->dev,
					  "Not support gesture type\n");
				break;
			}
		} else {
			input_err(true, &data->client->dev, "Not Gesture Msg\n");
		}

		return;
	}

	input_err(true, &data->client->dev, "Not support gesture cmd: 0x%02X\n", cmd);
}

static void location_detect(struct ist40xx_data *data, char *loc, int x, int y)
{
	if (x < data->dt_data->area_edge)
		strncat(loc, "E.", 2);
	else if (x < (data->tsp_info.width - data->dt_data->area_edge))
		strncat(loc, "C.", 2);
	else
		strncat(loc, "e.", 2);

	if (y < data->dt_data->area_indicator)
		strncat(loc, "S", 1);
	else if (y < (data->tsp_info.height - data->dt_data->area_navigation))
		strncat(loc, "C", 1);
	else
		strncat(loc, "N", 1);
}

static void release_finger(struct ist40xx_data *data, int id)
{
	input_mt_slot(data->input_dev, id);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_report_key(data->input_dev, BTN_TOOL_FINGER, 0);
	input_sync(data->input_dev);

	input_info(true, &data->client->dev, "forced touch release: %d\n", id);

	data->tsp_touched[id] = false;
}

void clear_input_data(struct ist40xx_data *data)
{
	int i = 0;

	input_info(true, &data->client->dev, "%s\n", __func__);

	for (i = 0; i < IST40XX_MAX_FINGER_ID; i++) {
		if (data->tsp_touched[i] == true)
			release_finger(data, i);
	}

	data->touch_pressed_num = 0;
	data->check_multi = 0;

	data->print_info_cnt_release = 0;
}

static const char finger_mode[9] = { 'N', 'H', 'C', 'G', 'S', 'P', 'W', 'P', 'J' };
#define TOUCH_DOWN_MESSAGE		("[P]")
#define TOUCH_UP_MESSAGE		("[R]")
#define TOUCH_MOVE_MESSAGE		("[M]")
static void report_input_data(struct ist40xx_data *data)
{
	int i, id;
	int idx = 0;
	u8 ttype;
	u16 coord_x, coord_y;
	char location[4] = { 0 };

	for (i = 0; i < IST40XX_MAX_FINGER_ID; i++) {
		ttype = 0;
		memset(location, 0, sizeof(location));
        if (data->fingers[idx].EventId == EID_COORDNATE) {
		    id = data->fingers[idx].TouchId - 1;
		    if (i == id) {
			    ttype = (data->fingers[idx].TouchType_3_2 << 2) |
                    data->fingers[idx].TouchType_1_0;
                coord_x = (data->fingers[idx].X_11_4 << 4) |
                    data->fingers[idx].X_3_0;
                coord_y = (data->fingers[idx].Y_11_4 << 4) |
                    data->fingers[idx].Y_3_0;

                if (data->noise != data->fingers[idx].NoiseStatus) {
                    data->noise = data->fingers[idx].NoiseStatus;
                    input_info(true, &data->client->dev, "Noise Level %d\n",
                            data->noise);
                }

                if (data->fingers[idx].TouchStatus == TOUCH_STA_PRESS) {
					snprintf(data->pos, 5, TOUCH_DOWN_MESSAGE);
                    input_mt_slot(data->input_dev, id);
                    input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
                            true);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_X,
                            coord_x);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
                            coord_y);
                    if (ttype == TOUCH_TYPE_PALM)
                        input_report_abs(data->input_dev, ABS_MT_CUSTOM, true);
                    else
                        input_report_abs(data->input_dev, ABS_MT_CUSTOM, false);
                    input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
                            data->fingers[idx].Major);
                    input_report_abs(data->input_dev, ABS_MT_TOUCH_MINOR,
                            data->fingers[idx].Minor);
                    input_report_abs(data->input_dev, ABS_MT_PRESSURE,
                            data->fingers[idx].Z);

                    /*for getting coordinate of pressed point*/
                    data->p_x[id] = data->r_x[id] = coord_x;
                    data->p_y[id] = data->r_y[id] = coord_y;
                    location_detect(data, location, data->p_x[id],
                            data->p_y[id]);
/*
 * ToDo
 * PALM status
 */
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					input_info(true, &data->client->dev,
						"%s tID:%d.%d x:%d y:%d z:%d loc:%s major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
						TOUCH_DOWN_MESSAGE, id, (data->input_dev->mt->trkid - 1) & TRKID_MAX,
						coord_x, coord_y, data->fingers[idx].Z, location,
						data->fingers[idx].Major, data->fingers[idx].Minor,
						data->fingers[idx].NoiseLevel,
						data->fingers[idx].MaxStrength,
						data->fingers[idx].HoverIdNum, data->touch_pressed_num);
#else
					input_info(true, &data->client->dev,
						"%s tID:%d.%d z:%d loc:%s major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
						TOUCH_DOWN_MESSAGE, id, (data->input_dev->mt->trkid - 1) & TRKID_MAX,
						data->fingers[idx].Z, location,
						/*PARSE_PALM_STATUS(data->t_status),*/
						data->fingers[idx].Major, data->fingers[idx].Minor,
						data->fingers[idx].NoiseLevel,
						data->fingers[idx].MaxStrength,
						data->fingers[idx].HoverIdNum, data->touch_pressed_num);
#endif
					if (data->tsp_touched[id] == false) {
                        data->tsp_touched[id] = true;
                        data->touch_pressed_num++;
                        data->all_finger_count++;
                    }

                    data->move_count[id]++;
				} else if (data->fingers[idx].TouchStatus == TOUCH_STA_MOVE) {
					snprintf(data->pos, 5, TOUCH_MOVE_MESSAGE);

                    input_mt_slot(data->input_dev, id);
                    input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
                            true);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_X,
                            coord_x);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
                            coord_y);
                    if (ttype == TOUCH_TYPE_PALM)
                        input_report_abs(data->input_dev, ABS_MT_CUSTOM, true);
                    else
                        input_report_abs(data->input_dev, ABS_MT_CUSTOM, false);
                    input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
                            data->fingers[idx].Major);
                    input_report_abs(data->input_dev, ABS_MT_TOUCH_MINOR,
                            data->fingers[idx].Minor);
                    input_report_abs(data->input_dev, ABS_MT_PRESSURE,
                            data->fingers[idx].Z);

                    data->r_x[id] = coord_x;
                    data->r_y[id] = coord_y;
                    location_detect(data, location, data->r_x[id],
                            data->r_y[id]);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					tsp_debug("%s tID:%d x:%d y:%d z:%d loc:%s t:%d major:%d minor:%d NL:%d, MS:%d, HIN:%d\n",
                            TOUCH_MOVE_MESSAGE, id, coord_x, coord_y, data->fingers[idx].Z, location,
                            ttype, data->fingers[idx].Major,
                            data->fingers[idx].Minor, 
                            data->fingers[idx].NoiseLevel,
                            data->fingers[idx].MaxStrength,
                            data->fingers[idx].HoverIdNum);
#else
					tsp_debug("%s tID:%d z:%d loc:%s t:%d major:%d minor:%d NL:%d, MS:%d, HIN:%d\n",
                            TOUCH_MOVE_MESSAGE, id, data->fingers[idx].Z, location,
                            ttype, data->fingers[idx].Major,
                            data->fingers[idx].Minor, 
                            data->fingers[idx].NoiseLevel,
                            data->fingers[idx].MaxStrength,
                            data->fingers[idx].HoverIdNum);
#endif
                    if (data->tsp_touched[id] == false) {
                        data->tsp_touched[id] = true;
                        data->touch_pressed_num++;
                        data->all_finger_count++;
                    }

                    data->move_count[id]++;
				} else if (data->fingers[idx].TouchStatus==TOUCH_STA_RELEASE) {
					snprintf(data->pos, 5, TOUCH_UP_MESSAGE);

                    if (data->tsp_touched[id] == true) {
                        input_mt_slot(data->input_dev, id);
                        input_mt_report_slot_state(data->input_dev,
                                MT_TOOL_FINGER, false);

                        data->r_x[id] = coord_x;
                        data->r_y[id] = coord_y;
                        location_detect(data, location, data->r_x[id],
                                data->r_y[id]);				
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					input_info(true, &data->client->dev,
							"%s tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
							TOUCH_UP_MESSAGE, id, location,
							data->r_x[id] - data->p_x[id],
							data->r_y[id] - data->p_y[id],
							data->move_count[id], data->touch_pressed_num,
							data->r_x[id], data->r_y[id],
							data->fingers[idx].Major, data->fingers[idx].Minor,
							data->fingers[idx].NoiseLevel,
							data->fingers[idx].MaxStrength,
							data->fingers[idx].HoverIdNum, data->touch_pressed_num);
#else
					input_info(true, &data->client->dev,
							"%s tID:%d loc:%s dd:%d,%d mc:%d tc:%d major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
							TOUCH_UP_MESSAGE, id, location,
							data->r_x[id] - data->p_x[id],
							data->r_y[id] - data->p_y[id],
							data->move_count[id], data->touch_pressed_num,
							data->fingers[idx].Major, data->fingers[idx].Minor,
							data->fingers[idx].NoiseLevel,
							data->fingers[idx].MaxStrength,
							data->fingers[idx].HoverIdNum, data->touch_pressed_num);

#endif
    					data->tsp_touched[id] = false;
	                    data->move_count[id] = 0;
	                    data->touch_pressed_num--;
						data->print_info_cnt_release = 0;
                    }

				}
    			idx++;            
#ifdef IST40XX_FORCE_RELEASE
            } else if (data->tsp_touched[i] == true) {
				snprintf(data->pos, 5, TOUCH_UP_MESSAGE);

				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
						false);

				location_detect(data, location, data->r_x[id], data->r_y[id]);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &data->client->dev,
						"%s tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
						TOUCH_UP_MESSAGE, id, location,
						data->r_x[id] - data->p_x[id],
						data->r_y[id] - data->p_y[id],
						data->move_count[id], data->touch_pressed_num,
						data->r_x[id], data->r_y[id],
						data->fingers[idx].Major, data->fingers[idx].Minor,
						data->fingers[idx].NoiseLevel,
						data->fingers[idx].MaxStrength,
						data->fingers[idx].HoverIdNum, data->touch_pressed_num);
#else
				input_info(true, &data->client->dev,
						"%s tID:%d loc:%s dd:%d,%d mc:%d tc:%d major:%d minor:%d nl:%d ms:%d hn:%d tc:%d\n",
						TOUCH_UP_MESSAGE, id, location,
						data->r_x[id] - data->p_x[id],
						data->r_y[id] - data->p_y[id],
						data->move_count[id], data->touch_pressed_num,
						data->fingers[idx].Major, data->fingers[idx].Minor,
						data->fingers[idx].NoiseLevel,
						data->fingers[idx].MaxStrength,
						data->fingers[idx].HoverIdNum, data->touch_pressed_num);

#endif

                data->tsp_touched[i] = false;
                data->touch_pressed_num--;
                data->move_count[i] = 0;
#endif
			}
		if (data->prev_type[id] != ttype)
			input_info(true, &data->client->dev, "tID: %d ttype: (%c -> %c): %s\n",
					id, finger_mode[data->prev_type[id]], finger_mode[ttype], data->pos);

		data->prev_type[id] = ttype;

		}
	}

	if (data->touch_pressed_num >= 1){
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_report_key(data->input_dev, BTN_TOOL_FINGER, 1);
	} else if (data->touch_pressed_num == 0){
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_key(data->input_dev, BTN_TOOL_FINGER, 0);
	}

	if ((data->touch_pressed_num >= 2) && (data->check_multi == 0)) {
		data->check_multi = 1;
		data->multi_count++;
	}

	if (data->touch_pressed_num < 2)
		data->check_multi = 0;

	data->irq_err_cnt = 0;
	data->scan_retry = 0;

	input_sync(data->input_dev);
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
void recording_data(struct ist40xx_data *data, bool idle)
{
	int ret;
	int i;
	u32 scancnt = 0;
	u32 addr = 0;
	u32 write_data;
	u32 *buf32;
	TSP_INFO *tsp = &data->tsp_info;

	if (idle && (data->rec_mode > 1))
		goto state_idle;

	ist40xx_delay(data->rec_delay);

	ret = ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS,
				   &scancnt);
	if (ret) {
		input_err(true, &data->client->dev, "%s failed to read scancnt\n",
			  __func__);
		goto state_idle;
	}

	if (data->recording_scancnt == scancnt) {
		input_err(true, &data->client->dev, "%s same recording scancnt\n",
			  __func__);
		goto state_idle;
	}

	data->recording_scancnt = scancnt;

	buf32 = kzalloc(data->rec_size +
			((tsp->node.self_len * 2) + tsp->node.len) * sizeof(u32),
			GFP_KERNEL);
	if (!buf32) {
		input_err(true, &data->client->dev, "failed to allocate %s %d\n",
			  __func__, __LINE__);
		goto state_idle;
	}

	if (data->rec_size > 0) {
		addr = IST40XX_DA_ADDR(data->rec_addr);
		ret = ist40xx_burst_read(data->client, addr, buf32,
					 data->rec_size / IST40XX_DATA_LEN,
					 true);
		if (ret)
			goto err_rec_fail;
	}

	if ((data->rec_type == 0) || (data->rec_type == 1) || (data->rec_type == 4)) {
		addr = IST40XX_DA_ADDR(data->self_cdc_addr);
		ret = ist40xx_burst_read(data->client, addr,
					buf32 + (data->rec_size / IST40XX_DATA_LEN),
					tsp->node.self_len, true);
		if (ret)
			goto err_rec_fail;
	}

	if ((data->rec_type == 0) || (data->rec_type == 2) || (data->rec_type == 4)) {
		addr = IST40XX_DA_ADDR(data->cdc_addr);
		ret = ist40xx_burst_read(data->client, addr,
					buf32 + (data->rec_size / IST40XX_DATA_LEN) +
					tsp->node.self_len, tsp->node.len, true);
		if (ret)
			goto err_rec_fail;
	}

	if ((data->rec_type == 3) || (data->rec_type == 4)) {
		addr = IST40XX_DA_ADDR(data->prox_cdc_addr);
		ret = ist40xx_burst_read(data->client, addr,
					buf32 + (data->rec_size / IST40XX_DATA_LEN) +
					tsp->node.self_len + tsp->node.len, tsp->node.self_len, true);
		if (ret)
			goto err_rec_fail;

		for (i = 0; i < tsp->node.self_len; i++)
			data->rec_prox[i] = buf32[(data->rec_size / IST40XX_DATA_LEN) +
				tsp->node.self_len + tsp->node.len + i];

	}

	ist40xx_recording_put_frame(data, buf32,
					(data->rec_size / IST40XX_DATA_LEN) +
					(tsp->node.self_len * 2) + tsp->node.len);

err_rec_fail:
	kfree(buf32);

state_idle:
	write_data = IST40XX_START_SCAN;
	ist40xx_write_buf(data->client, eHCOM_SET_REC_MODE, (u32 *)&write_data, 1);
}
#endif

/*
 * CMD : CMD_GET_COORD
 *
 *   1st  [31:24]   [23:21]   [20:16]   [15:12]   [11]  [10]   [9:0]
 *		Checksum  KeyCnt	KeyStatus FingerCnt Rsvd. Palm   FingerStatus
 *   2nd  [31:28]   [27:24]   [23:12]   [11:0]
 *		Major	 Minor	 X		 Y
 */
irqreturn_t ist40xx_irq_thread(int irq, void *ptr)
{
	int i, ret = 0;
	int read_cnt;
	int offset = 1;
	u32 msg[IST40XX_MAX_FINGER_ID * IST40XX_TOUCH_FRAME_CNT + offset];
	u32 *buf32;
	u32 ms;
	bool idle = false;
	struct ist40xx_data *data = (struct ist40xx_data *)ptr;

	data->irq_working = true;
	memset(msg, 0, sizeof(msg));

	if (!data->irq_enabled)
		goto irq_ignore;

#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (ist40xx_filter_interrupt(data) == IRQ_HANDLED) {
		ret = wait_for_completion_interruptible_timeout((&data->st_interrupt),
				msecs_to_jiffies(10 * MSEC_PER_SEC));
		return IRQ_HANDLED;
	}
#endif

	if (data->status.sys_mode == STATE_LPM) {
		pm_wakeup_event(data->input_dev->dev.parent, 2000);
#ifdef CONFIG_PM
		ret = wait_for_completion_interruptible_timeout(&data->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &data->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return IRQ_HANDLED;
		} else if (ret < 0) {
			input_err(true, &data->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return IRQ_HANDLED;
		}
#endif
	} else if (data->status.sys_mode == STATE_POWER_OFF)
		goto irq_ignore;

	ms = get_milli_second(data);

	if (data->intr_debug1_size > 0) {
		buf32 = kzalloc(data->intr_debug1_size * sizeof(u32), GFP_KERNEL);
		if (!buf32) {
			input_err(true, &data->client->dev, "failed to allocate %s %d\n",
				  __func__, __LINE__);
			goto irq_err;
		}

		tsp_debug("Intr_debug1 (addr: 0x%08x)\n", data->intr_debug1_addr);
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(data->intr_debug1_addr), buf32,
				data->intr_debug1_size, true);

		for (i = 0; i < data->intr_debug1_size; i++)
			tsp_debug(" %08x\n", buf32[i]);
		kfree(buf32);
		
		if (ret < 0)
			goto irq_ic_err;
	}

	if (data->intr_debug2_size > 0) {
		buf32 = kzalloc(data->intr_debug2_size * sizeof(u32), GFP_KERNEL);
		if (!buf32) {
			input_err(true, &data->client->dev, "failed to allocate %s %d\n",
				  __func__, __LINE__);
			goto irq_err;
		}

		tsp_debug("Intr_debug2 (addr: 0x%08x)\n", data->intr_debug2_addr);
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(data->intr_debug2_addr), buf32,
				data->intr_debug2_size, true);

		for (i = 0; i < data->intr_debug2_size; i++)
			tsp_debug(" %08x\n", buf32[i]);
		kfree(buf32);

		if (ret < 0)
			goto irq_ic_err;
	}

	ret = ist40xx_read_reg(data->client, IST40XX_HIB_INTR_MSG, msg);
	if (ret)
		goto irq_err;

	tsp_verb("intr msg: 0x%08x\n", *msg);

	/* TSP End Initial */
	if (*msg == IST40XX_INITIAL_VALUE) {
		tsp_debug("IC Ready~\n");
		goto irq_event;
	}

	/* TSP Recording */
	if (*msg == IST40XX_REC_VALUE) {
		idle = true;
		goto irq_end;
	}

	/* TSP IC Exception */
	if ((*msg & IST40XX_EXCEPT_MASK) == IST40XX_EXCEPT_VALUE) {
		input_err(true, &data->client->dev, "Occurred IC exception(0x%02X)\n",
			  *msg & 0xFF);
		ret = ist40xx_burst_read(data->client, IST40XX_HIB_COORD, &msg[offset],
				IST40XX_MAX_EXCEPT_SIZE, true);
		if (ret) {
			input_err(true, &data->client->dev,
				  " exception value read error(%d)\n", ret);
		} else {
			input_err(true, &data->client->dev,
				  " exception value : 0x%08X, 0x%08X\n", msg[1], msg[2]);
		}

		goto irq_ic_err;
	}

	if (*msg == 0 || *msg == 0xFFFFFFFF)	/* Unknown CMD */
		goto irq_err;

	if ((*msg & IST40XX_MISCALIB_MASK) == IST40XX_MISCALIB_MSG) {
		input_info(true, &data->client->dev, "Mis calibration End\n");
		data->status.miscalib_msg = *msg;
		data->status.miscalib_result = IST40XX_MISCALIB_VAL(*msg);
		goto irq_event;
	}

	if ((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
		ret = ist40xx_burst_read(data->client, IST40XX_HIB_INTR_MSG,
				data->status.calib_msg, IST40XX_MAX_CALIB_SIZE, true);
		if (ret < 0)
			goto irq_ic_err;

		input_info(true, &data->client->dev, "Auto calibration\n");
		input_info(true, &data->client->dev, "SLF calib status:0x%08X\n",
			   data->status.calib_msg[0]);
		input_info(true, &data->client->dev, "MTL calib status:0x%08X\n",
			   data->status.calib_msg[1]);
		input_info(true, &data->client->dev, "MAX CH status   :0x%08X\n",
			   data->status.calib_msg[2]);
		goto irq_event;
	}

	if ((CMCS_MSG(*msg) == CM_MSG_VALID) || (CMCS_MSG(*msg) == CS_MSG_VALID) ||
		(CMCS_MSG(*msg) == CMJIT_MSG_VALID) ||
		(CMCS_MSG(*msg) == CRJIT_MSG_VALID) ||
		(CMCS_MSG(*msg) == CRJIT2_MSG_VALID)) {
		data->status.cmcs = *msg;
		data->status.cmcs_result = CMCS_RESULT(*msg);
		input_info(true, &data->client->dev, "CMCS notify: 0x%08X\n", *msg);
		goto irq_event;
	}

	ret = PARSE_SPECIAL_MESSAGE(*msg);
	if (ret >= 0) {
		tsp_debug("special cmd: %d (0x%08X)\n", ret, *msg);
		ist40xx_special_cmd(data, ret);

		goto irq_event;
	}

    //----Touch Msg
	if ((PARSE_TOUCH_SIG(*msg) != IST40XX_TOUCH_SIG) ||
		!PARSE_ZERO_PREVENTION(*msg)) {
		tsp_err("Not Support Intr Msg~\n");
		goto irq_err;
	}

	memset(data->fingers, 0, sizeof(data->fingers));
	read_cnt = PARSE_TOUCH_CNT(*msg);

	if (data->hover != PARSE_HOVER_LEVEL(*msg)) {
		data->hover = PARSE_HOVER_LEVEL(*msg);
		input_report_abs(data->input_dev_proximity, ABS_MT_CUSTOM, data->hover);
		input_sync(data->input_dev_proximity);
		input_info(true, &data->client->dev, "Hover Level %d\n", data->hover);
	}

	if (data->pocket != PARSE_POCKET_STATUS(*msg) || data->force_pocket_event) {
		data->pocket = PARSE_POCKET_STATUS(*msg);
		data->force_pocket_event = false;
		input_info(true, &data->client->dev, "Pocket %s, %d\n",
			data->pocket ? "Detect" : "Undetect", data->pocket);
		input_report_abs(data->input_dev_proximity, ABS_MT_CUSTOM, data->pocket ? 11 : 10);
		input_sync(data->input_dev_proximity);
	}

	if (read_cnt <= 0)
		goto irq_end;

	ret = ist40xx_burst_read(data->client, IST40XX_HIB_COORD, &msg[offset],
			(read_cnt > 5 ? 5 : read_cnt) * IST40XX_TOUCH_FRAME_CNT, true);
	if (ret)
		goto irq_err;

	if (read_cnt > 5) {
		ret = ist40xx_burst_read(data->client, IST40XX_EXT_COORD,
					&msg[offset + (IST40XX_TOUCH_FRAME_CNT * 5)],
					(read_cnt - 5) * IST40XX_TOUCH_FRAME_CNT, true);
		if (ret)
			goto irq_err;
	}

	tsp_verb("Read Cnt:%d\n", read_cnt);
	for (i = 0; i < read_cnt; i++) {
		tsp_verb("%2d:0x%08X\n", i, msg[i * IST40XX_TOUCH_FRAME_CNT + offset]);
		tsp_verb("%2s 0x%08X\n", "  ", msg[i * IST40XX_TOUCH_FRAME_CNT + 1 + offset]);
		tsp_verb("%2s 0x%08X\n", "  ", msg[i * IST40XX_TOUCH_FRAME_CNT + 2 + offset]);
		tsp_verb("%2s 0x%08X\n", "  ", msg[i * IST40XX_TOUCH_FRAME_CNT + 3 + offset]);
	}

	data->t_status = *msg;
	memcpy(data->fingers, &msg[offset], sizeof(struct ts_event_coordinate) * read_cnt);

	report_input_data(data);

	if (data->intr_debug3_size > 0) {
		buf32 = kzalloc(data->intr_debug3_size * sizeof(u32), GFP_KERNEL);
		if (!buf32) {
			input_err(true, &data->client->dev, "failed to allocate %s %d\n",
				  __func__, __LINE__);
			goto irq_err;
		}
		tsp_debug("Intr_debug3 (addr: 0x%08x)\n", data->intr_debug3_addr);
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(data->intr_debug3_addr), buf32,
				data->intr_debug3_size, true);

		for (i = 0; i < data->intr_debug3_size; i++)
			tsp_debug(" %08x\n", buf32[i]);
		kfree(buf32);
		
		if (ret < 0)
			goto irq_ic_err;
	}

	goto irq_end;

irq_err:
	input_err(true, &data->client->dev, "intr msg: 0x%08x, ret: %d\n",
		  msg[0], ret);
	ist40xx_request_reset(data);
	goto irq_event;
irq_end:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (data->rec_mode)
		recording_data(data, idle);
#endif
irq_event:
irq_ignore:
	data->irq_working = false;
	data->event_ms = (u32) get_milli_second(data);
	if (data->initialized)
		mod_timer(&data->event_timer,
			  get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;

irq_ic_err:
	ist40xx_scheduled_reset(data);
	data->irq_working = false;
	data->event_ms = (u32) get_milli_second(data);
	if (data->initialized)
		mod_timer(&data->event_timer,
			  get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;
}

#ifdef IST40XX_PINCTRL
static int ist40xx_pinctrl_configure(struct ist40xx_data *data, bool active)
{
	struct pinctrl_state *set_state;

	int retval;

	input_info(true, &data->client->dev, "%s %s\n", __func__,
		   active ? "ACTIVE" : "SUSPEND");

	set_state = pinctrl_lookup_state(data->pinctrl,
					 active ? "on_state" : "off_state");
	if (IS_ERR(set_state)) {
		input_err(true, &data->client->dev, "%s cannot get active state\n",
			  __func__);
		return -EINVAL;
	}

	retval = pinctrl_select_state(data->pinctrl, set_state);
	if (retval) {
		input_err(true, &data->client->dev, "%s cannot set pinctrl %s state\n",
			__func__, active ? "active" : "suspend");
		return -EINVAL;
	}

	return 0;
}
#endif

static int ist40xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist40xx_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	msleep(100);
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s TUI cancel event call!\n",
			  __func__);
		tui_force_close(1);
		msleep(100);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			input_err(true, &data->client->dev, "%s TUI flag force clear!\n",
				  __func__);
			trustedui_clear_mask(
					TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif

	del_timer(&data->event_timer);
	cancel_delayed_work_sync(&data->work_reset_check);
#ifdef IST40XX_NOISE_MODE
	cancel_delayed_work_sync(&data->work_noise_protect);
#else
	cancel_delayed_work_sync(&data->work_force_release);
#endif

	mutex_lock(&data->lock);
	if (data->lpm_mode || data->ed_enable || data->pocket_enable ||
		data->fod_lp_mode) {

		if (data->prox_power_off) {
			u32 write_data;
			int ret;

			mutex_lock(&data->aod_lock);
			write_data = ((u32)data->fod_property << 16) | (u32)data->lpm_mode;
			write_data &= ~IST40XX_DOUBLETAP_WAKEUP;
			ret = ist40xx_write_buf(data->client, eHCOM_SET_SPONGE_FEATURE, (u32 *)&write_data, 1);
			if (ret)
				input_err(true, &data->client->dev, "%s: fail to write sponge reg: %d\n", __func__, ret);
			mutex_unlock(&data->aod_lock);
		}

		ist40xx_cmd_gesture(data, IST40XX_ENABLE);

		if (device_may_wakeup(&data->client->dev))
			enable_irq_wake(data->client->irq);

		data->status.sys_mode = STATE_LPM;
	} else {
		ist40xx_power_off(data);
		ist40xx_disable_irq(data);
		data->status.sys_mode = STATE_POWER_OFF;
#ifdef IST40XX_PINCTRL
		if (data->pinctrl) {
			int ret = ist40xx_pinctrl_configure(data, false);
			if (ret) {
				input_err(true, &data->client->dev,
					  "%s: cannot set pinctrl state\n", __func__);
			}
		}
#endif
	}
	clear_input_data(data);

	input_report_abs(data->input_dev_proximity, ABS_MT_CUSTOM, 0);
	if (data->pocket)
		data->force_pocket_event = true;

	if (data->prox_power_off) {
		input_report_key(data->input_dev, KEY_INT_CANCEL, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_INT_CANCEL, 0);
		input_sync(data->input_dev);
	}
	data->prox_power_off = 0;

	mutex_unlock(&data->lock);

	cancel_delayed_work_sync(&data->work_print_info);

	return 0;
}

static int ist40xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist40xx_data *data = i2c_get_clientdata(client);
	u32 write_data;
	int ret;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	msleep(100);
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s TUI cancel event call!\n",
			  __func__);
		tui_force_close(1);
		msleep(100);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			input_err(true, &data->client->dev, "%s TUI flag force clear!\n",
				  __func__);
			trustedui_clear_mask(
					TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif

	if (!data->initialized) {
		input_err(true, &data->client->dev, "IC initialize is not complete\n");
		return 0;
	}

	data->noise_mode |= (1 << NOISE_MODE_POWER);

	if (data->status.sys_mode == STATE_POWER_ON)
		return 0;

	mutex_lock(&data->lock);
	if (data->status.sys_mode == STATE_LPM) {
		ist40xx_cmd_gesture(data, IST40XX_DISABLE);
		mod_timer(&data->event_timer,
				get_jiffies_64() + EVENT_TIMER_INTERVAL);

		if (device_may_wakeup(&data->client->dev))
			disable_irq_wake(data->client->irq);

		data->status.sys_mode = STATE_POWER_ON;

		mutex_lock(&data->aod_lock);
		write_data = ((u32)data->fod_property << 16) | (u32)data->lpm_mode;
		ret = ist40xx_write_buf(data->client, eHCOM_SET_SPONGE_FEATURE, (u32 *)&write_data, 1);
		if (ret)
			input_err(true, &data->client->dev, "%s: fail to write sponge reg: %d\n", __func__, ret);
		mutex_unlock(&data->aod_lock);

	} else {
#ifdef IST40XX_PINCTRL
		if (data->pinctrl) {
			int ret = ist40xx_pinctrl_configure(data, true);
			if (ret) {
				input_err(true, &data->client->dev,
					  "%s: cannot set pinctrl state\n",
					  __func__);
			}
		}
#endif

		ist40xx_reset(data, false);
		ist40xx_enable_irq(data);
		ist40xx_start(data);
		data->status.sys_mode = STATE_POWER_ON;
	}
	mutex_unlock(&data->lock);

	cancel_delayed_work_sync(&data->work_print_info);
	data->print_info_cnt_open = 0;
	data->print_info_cnt_release = 0;
	schedule_work(&data->work_print_info.work);

	return 0;
}

#ifdef USE_OPEN_CLOSE
static void ist40xx_ts_close(struct input_dev *dev)
{
	struct ist40xx_data *data = input_get_drvdata(dev);

	if (!data->info_work_done) {
		input_err(true, &data->client->dev, "%s: not finished info work\n",
			  __func__);
		return;
	}

	input_info(true, &data->client->dev, "%s:\n", __func__);

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(data->tdata);
#endif
#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	ist40xx_secure_touch_stop(data, 1);
#endif

	ist40xx_suspend(&data->client->dev);
}

static int ist40xx_ts_open(struct input_dev *dev)
{
	struct ist40xx_data *data = input_get_drvdata(dev);

	if (!data->info_work_done) {
		input_err(true, &data->client->dev, "%s: not finished info work\n",
			  __func__);
		return 0;
	}

	input_info(true, &data->client->dev, "%s:\n", __func__);

	return ist40xx_resume(&data->client->dev);
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ist40xx_early_suspend(struct early_suspend *h)
{
	struct ist40xx_data *data = container_of(h, struct ist40xx_data,
			early_suspend);

	ist40xx_suspend(&data->client->dev);
}

static void ist40xx_late_resume(struct early_suspend *h)
{
	struct ist40xx_data *data = container_of(h, struct ist40xx_data,
			early_suspend);

	ist40xx_resume(&data->client->dev);
}
#endif

void ist40xx_set_ta_mode(bool mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_TA) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_TA);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_TA);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_ta_mode);

void ist40xx_set_edge_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_EDGE);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_EDGE);

	if (data->status.sys_mode != STATE_POWER_OFF)
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_edge_mode);

void ist40xx_set_call_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_CALL) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_CALL);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_CALL);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_call_mode);

void ist40xx_set_halfaod_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_HALFAOD) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_HALFAOD);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_HALFAOD);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_halfaod_mode);

void ist40xx_set_cover_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_COVER) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_COVER);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_COVER);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_cover_mode);

void ist40xx_set_glove_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_GLOVE) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_GLOVE);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_GLOVE);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_glove_mode);

int ist40xx_set_rejectzone_mode(int mode, u16 top, u16 bottom)
{
	struct ist40xx_data *data = ts_data;
	int ret;

	input_info(true, &data->client->dev, "%s: mode = %d (T:%d,B:%d)\n",
			__func__, mode, top, bottom);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_REJECTZONE);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_REJECTZONE);

	data->rejectzone_t = top;
	data->rejectzone_b = bottom;

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF)) {
		ret = ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
		if (ret)
			return ret;
		ret = ist40xx_write_buf(data->client, eHCOM_SET_REJECTZONE_TOP, (u32 *)&data->rejectzone_t, 1);
		if (ret)
			return ret;
		ret = ist40xx_write_buf(data->client, eHCOM_SET_REJECTZONE_BOTTOM, (u32 *)&data->rejectzone_b, 1);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ist40xx_set_rejectzone_mode);

void ist40xx_set_sensitivity_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_SENSITIVITY) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_SENSITIVITY);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_SENSITIVITY);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_sensitivity_mode);

void ist40xx_set_touchable_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_TOUCHABLE) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_TOUCHABLE);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_TOUCHABLE);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_touchable_mode);

void ist40xx_set_pocket_mode(int mode)
{
	struct ist40xx_data *data = ts_data;

	if (mode == ((data->noise_mode >> NOISE_MODE_POCKET) & 1))
		return;

	input_info(true, &data->client->dev, "%s: mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_POCKET);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_POCKET);

	if (data->initialized && (data->status.sys_mode != STATE_POWER_OFF))
		ist40xx_write_buf(data->client, eHCOM_SET_MODE_SPECIAL, (u32 *)&data->noise_mode, 1);
}
EXPORT_SYMBOL(ist40xx_set_pocket_mode);

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || defined(CONFIG_MUIC_NOTIFIER)
static int otg_flag = 0;
#endif

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
static int tsp_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{
	PD_NOTI_ATTACH_TYPEDEF usb_status =
	    *(PD_NOTI_ATTACH_TYPEDEF *) data;

	switch (usb_status.attach) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		otg_flag = 1;
		tsp_info("%s : otg_flag 1\n", __func__);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		otg_flag = 0;
		break;
	default:
		break;
	}

	return 0;
}
#else
#ifdef CONFIG_MUIC_NOTIFIER
static int tsp_muic_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		otg_flag = 0;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		if (attached_dev == ATTACHED_DEV_OTG_MUIC) {
			otg_flag = 1;
			tsp_info("%s : otg_flag 1\n", __func__);
		}
		break;
	default:
		break;
	}

	return 0;
}
#endif

#ifdef CONFIG_VBUS_NOTIFIER
static int tsp_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *) data;

	tsp_info("%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		tsp_info("%s : attach\n", __func__);
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || defined(CONFIG_MUIC_NOTIFIER)
		if (!otg_flag)
#endif
			ist40xx_set_ta_mode(true);
		break;
	case STATUS_VBUS_LOW:
		tsp_info("%s : detach\n", __func__);
		ist40xx_set_ta_mode(false);
		break;
	default:
		break;
	}
	return 0;
}
#endif
#endif

static void reset_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist40xx_data *data = container_of(delayed_work, struct ist40xx_data,
			work_reset_check);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s: return, TUI is enabled!\n",
			  __func__);
		return;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &data->client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return;
	}
#endif

	if ((data == NULL) || (data->client == NULL))
		return;

	input_info(true, &data->client->dev, "Request reset function\n");

	if ((data->initialized == 1) &&
	    (data->status.sys_mode != STATE_POWER_OFF) &&
	    (data->status.update != 1) && (data->status.calib < 1) &&
	    (data->status.miscalib < 1)) {
		mutex_lock(&data->lock);
		ist40xx_disable_irq(data);
		ist40xx_reset(data, false);
		clear_input_data(data);
		ist40xx_enable_irq(data);
		ist40xx_start(data);
		mutex_unlock(&data->lock);
	}
}

#ifdef IST40XX_NOISE_MODE
static void noise_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
	u32 scan_count = 0;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist40xx_data *data = container_of(delayed_work, struct ist40xx_data,
			work_noise_protect);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s: return, TUI is enabled!\n", __func__);
		return;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &data->client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return;
	}
#endif

	ret = ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS,
			&touch_status);
	if (ret) {
		input_err(true, &data->client->dev, "Touch status read fail!\n");
		goto retry_timer;
	}

	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	/* Check valid scan count */
	if ((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC) {
		input_err(true, &data->client->dev, "%s: Touch status is not corrected! (0x%08x)\n",
			__func__, touch_status);
		goto retry_timer;
	}

	/* Status of IC is idle */
	if (GET_FINGER_ENABLE(touch_status) == 0) {
		if (data->touch_pressed_num > 0)
			clear_input_data(data);
	}

	scan_count = GET_SCAN_CNT(touch_status);

	/* Status of IC is lock-up */
	if (scan_count == data->scan_count) {
		input_err(true, &data->client->dev, "TSP IC is not responded! (0x%08x, %8d)\n",
			  touch_status, scan_count);
		goto retry_timer;
	}

	data->scan_retry = 0;
	data->scan_count = scan_count;
	return;

retry_timer:
	data->scan_retry++;
	input_info(true, &data->client->dev, "Retry touch status!(%d)\n", data->scan_retry);

	if (data->scan_retry >= data->max_scan_retry) {
		ist40xx_scheduled_reset(data);
		data->scan_retry = 0;
	} else {
		mod_timer(&data->event_timer,
				get_jiffies_64() + EVENT_TIMER_INTERVAL / 10);
	}
}
#else
static void release_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist40xx_data *data =
		container_of(delayed_work, struct ist40xx_data,
			 work_force_release);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev, "%s: return, TUI is enabled!\n", __func__);
		return;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &data->client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return;
	}
#endif

	if (data->touch_pressed_num == 0)
		return;

	ret = ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS,
			&touch_status);
	if (ret) {
		input_err(true, &data->client->dev, "Touch status read fail!\n");
		return;
	}

	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	/* Check valid scan count */
	if ((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC) {
		input_err(true, &data->client->dev, "%s: Touch status is not corrected! (0x%08x)\n",
			__func__, touch_status);
		return;
	}

	/* Status of IC is idle */
	if (GET_FINGER_ENABLE(touch_status) == 0)
		clear_input_data(data);
}
#endif

void timer_handler(unsigned long timer_data)
{
	struct ist40xx_data *data = (struct ist40xx_data *)timer_data;
	struct ist40xx_status *status = &data->status;

	if (data->irq_working || !data->initialized || data->rec_mode ||
			!status->event_mode)
		goto restart_timer;

	if ((status->sys_mode == STATE_POWER_ON) && (status->update != 1) &&
			(status->calib < 1) && (status->miscalib < 1)) {
		data->timer_ms = (u32) get_milli_second(data);

		if (status->noise_mode) {
			/* 100ms after last interrupt */
			if (data->timer_ms - data->event_ms > 100) {
#ifdef IST40XX_NOISE_MODE
				schedule_delayed_work(&data->work_noise_protect, 0);
#else
				schedule_delayed_work(&data->work_force_release, 0);
#endif
			}
		}
	}

restart_timer:
	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}

static void ist40xx_print_info_work(struct work_struct *work)
{
	struct ist40xx_data *data = container_of(work, struct ist40xx_data, work_print_info.work);

	data->print_info_cnt_open++;
	if (data->print_info_cnt_open > 0xFFF0)
		data->print_info_cnt_open = 0;

	if (data->touch_pressed_num == 0) {
#ifdef TCLM_CONCEPT
		input_info(true, &data->client->dev, "mode:%04X, tc:%d, noise:%x,%x, wet:%d, wc:%x, lp:%x, D%05X, fn:%04X/%04X // v:%04X%04X%04X, cal:%02X(%02X) C%02XT%04X.%4s%s Cal_Flag:%s,%d // %d %d\n",
				0/*mode*/, data->touch_pressed_num, data->noise, 0/*noise*/, 0/*data->wet*/, 0/*wc*/, data->lpm_mode,
				0/*defect probability*/, 0/*touch function*/, 0/*ic status*/,
				data->fw.cur.main_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver,
				0/*cal*/, data->test_result.data[0]/*test result*/,
				data->tdata->nvdata.cal_count, data->tdata->nvdata.tune_fix_ver,
				data->tdata->tclm_string[data->tdata->nvdata.cal_position].f_name,
				(data->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ", " ", 0,
				data->print_info_cnt_open, data->print_info_cnt_release);
#else
		input_info(true, &data->client->dev, "mode:%04X, tc:%d, noise:%x,%x, wet:%d, wc:%x, lp:%x, D%05X, fn:%04X/%04X // v:%04X%04X%04X // %d %d\n",
				0/*mode*/, data->touch_pressed_num, data->noise, 0/*noise*/, 0/*data->wet*/, 0/*wc*/, data->lpm_mode,
				0/*defect probability*/, 0/*touch function*/, 0/*ic status*/,
				data->fw.cur.main_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver,
				data->print_info_cnt_open, data->print_info_cnt_release);
#endif
		data->print_info_cnt_release++;
	}
	schedule_delayed_work(&data->work_print_info, msecs_to_jiffies(30 * MSEC_PER_SEC));
}

#ifdef CONFIG_OF
static int ist40xx_parse_dt(struct device *dev, struct ist40xx_data *data)
{
	struct device_node *np = dev->of_node;
	u32 px_zone[3];
	u32 cm_spec[3];

	data->dt_data->irq_gpio = of_get_named_gpio(np, "imagis,irq-gpio", 0);

	data->dt_data->is_power_by_gpio = of_property_read_bool(np,
			"imagis,power-gpioen");
	if (data->dt_data->is_power_by_gpio) {
		data->dt_data->power_gpio = of_get_named_gpio(np, "imagis,power-gpio", 0);
	} else {
		data->dt_data->power_gpio = -1;
		if (of_property_read_string(np, "imagis,regulator_avdd",
					&data->dt_data->regulator_avdd)) {
			input_err(true, dev, "%s Failed to get regulator_avdd name property\n",
				  __func__);
		}
	}

	data->dt_data->fw_det_gpio = of_get_named_gpio(np, "imagis,gpio-tsp_fw_det", 0);
	if (gpio_is_valid(data->dt_data->fw_det_gpio)) {
		gpio_request(data->dt_data->fw_det_gpio, "TSP_FW_DET");
		input_info(true, dev, "%s: fw_det_gpio: %d, %s\n",
				__func__, data->dt_data->fw_det_gpio,
				gpio_get_value(data->dt_data->fw_det_gpio) ? "HIGH" : "LOW");
	}

	if (of_property_read_u32(np, "imagis,fw-bin", &data->dt_data->fw_bin) >= 0) {
		input_info(true, dev, "%s: fw-bin: %d\n", __func__, data->dt_data->fw_bin);
	}

	if (of_property_read_string(np, "imagis,ic-version",
				&data->dt_data->ic_version) >= 0) {
		input_info(true, dev, "%s: ic_version: %s\n", __func__,
			   data->dt_data->ic_version);
	}

	if (of_property_read_string(np, "imagis,project-name",
				&data->dt_data->project_name) >= 0) {
		input_info(true, dev, "%s: project_name: %s\n", __func__,
			   data->dt_data->project_name);
	}

	if (data->dt_data->ic_version && data->dt_data->project_name) {
		snprintf(data->dt_data->fw_path, FIRMWARE_PATH_LENGTH, "%s%s_%s.bin",
			 FIRMWARE_PATH, data->dt_data->ic_version,
			 data->dt_data->project_name);
		input_info(true, dev, "%s: firm path: %s\n", __func__,
			   data->dt_data->fw_path);

		snprintf(data->dt_data->cmcs_path, FIRMWARE_PATH_LENGTH,
			 "%s%s_%s_cmcs.bin", FIRMWARE_PATH,
			 data->dt_data->ic_version, data->dt_data->project_name);
		input_info(true, dev, "%s: cmcs bin path: %s\n", __func__,
				data->dt_data->cmcs_path);
	}

	if (of_property_read_u32(np, "imagis,octa-hw",
				&data->dt_data->octa_hw) >= 0) {
		input_info(true, dev, "%s: octa-hw: %d\n", __func__,
			   data->dt_data->octa_hw);
	}

	if (of_property_read_u32_array(np, "imagis,area-size", px_zone, 3)) {
		input_err(true, dev, "%s: Failed to get zone's size\n", __func__);
		data->dt_data->area_indicator = 63;
		data->dt_data->area_navigation = 126;
		data->dt_data->area_edge = 60;
	} else {
		data->dt_data->area_indicator = px_zone[0];
		data->dt_data->area_navigation = px_zone[1];
		data->dt_data->area_edge = px_zone[2];
	}

	if (of_property_read_u32(np, "imagis,bringup",
				 &data->dt_data->bringup) < 0)
		data->dt_data->bringup = 0;

	if (of_property_read_u32(np, "imagis,factory_item_version",
				 &data->dt_data->item_version) < 0)
		data->dt_data->item_version = 0;

	data->dt_data->enable_settings_aot = of_property_read_bool(np, "enable_settings_aot");
	data->dt_data->support_fod = of_property_read_bool(np, "support_fod");
	data->dt_data->enable_fpcb_noise_test = of_property_read_bool(np, "enable_fpcb_noise_test");
	data->dt_data->support_open_short_test = of_property_read_bool(np, "support_open_short_test");
	data->dt_data->support_mis_calibration_test = of_property_read_bool(np, "support_mis_calibration_test");

	if (of_property_read_u32_array(np, "imagis,cm_spec", cm_spec, 3)) {
		input_err(true, dev, "%s: Failed to get zone's size\n", __func__);
		data->dt_data->cm_min_spec = CM_MIN_SPEC;
		data->dt_data->cm_max_spec = CM_MAX_SPEC;
		data->dt_data->cm_spec_gap = SPEC_GAP;
	} else {
		data->dt_data->cm_min_spec = cm_spec[0];
		data->dt_data->cm_max_spec = cm_spec[1];
		data->dt_data->cm_spec_gap = cm_spec[2];
	}

	input_info(true, dev, "%s: irq:%d, tsp_ldo: %s\n", __func__,
		   data->dt_data->irq_gpio, data->dt_data->regulator_avdd);

	input_info(true, dev, "%s: power source by gpio:%d, power_gpio: %d\n",
		   __func__, data->dt_data->is_power_by_gpio, data->dt_data->power_gpio);

	return 0;
}

void sec_tclm_parse_dt(struct i2c_client *client, struct sec_tclm_data *tdata)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "imagis,tclm_level", &tdata->tclm_level) < 0) {
		tdata->tclm_level = 0;
		input_err(true, &client->dev, "%s Failed to get tclm_level property\n",
			  __func__);
	}

	if (of_property_read_u32(np, "imagis,afe_base", &tdata->afe_base) < 0) {
		tdata->afe_base = 0;
		input_err(true, &client->dev, "%s Failed to get afe_base property\n",
			  __func__);
	}

	tdata->support_tclm_test = of_property_read_bool(np, "support_tclm_test");

	input_err(true, &client->dev, "%s tclm_level %d, afe_base %04X\n", __func__,
		  tdata->tclm_level, tdata->afe_base);
}

static void ist40xx_request_gpio(struct i2c_client *client,
		struct ist40xx_data *data)
{
	int ret;

	input_info(true, &client->dev, "%s\n", __func__);

	if (gpio_is_valid(data->dt_data->irq_gpio)) {
		ret = gpio_request(data->dt_data->irq_gpio, "imagis,irq_gpio");
		if (ret) {
			input_err(true, &client->dev,
				  "%s unable to request irq_gpio [%d]\n", __func__,
				  data->dt_data->irq_gpio);
			return;
		}

		ret = gpio_direction_input(data->dt_data->irq_gpio);
		if (ret) {
			input_err(true, &client->dev,
				  "%s unable to set direction for gpio [%d]\n",
				  __func__, data->dt_data->irq_gpio);
		}
		client->irq = gpio_to_irq(data->dt_data->irq_gpio);
	}

	if (gpio_is_valid(data->dt_data->power_gpio)) {
		ret = gpio_request(data->dt_data->power_gpio, "imagis,power_gpio");
		if (ret) {
			input_err(true, &client->dev,
				  "%s unable to request power_gpio [%d]\n", __func__,
				  data->dt_data->power_gpio);
			return;
		}

		ret = gpio_direction_output(data->dt_data->power_gpio, 1);
		if (ret) {
			input_err(true, &client->dev,
				  "%s unable to set direction for gpio [%d]\n",
				  __func__, data->dt_data->power_gpio);
		}
	}
}

static void ist40xx_free_gpio(struct ist40xx_data *data)
{
	input_info(true, &data->client->dev, "%s\n", __func__);

	if (gpio_is_valid(data->dt_data->irq_gpio))
		gpio_free(data->dt_data->irq_gpio);

	if (gpio_is_valid(data->dt_data->power_gpio))
		gpio_free(data->dt_data->power_gpio);
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_ist_on(void)
{
	struct ist40xx_data *data = ts_data;

	input_info(true, &data->client->dev, "%s release all finger..", __func__);
	clear_input_data(tui_tsp_info);

	del_timer(&tui_tsp_info->event_timer);

	cancel_delayed_work_sync(&tui_tsp_info->work_reset_check);
#ifdef IST40XX_NOISE_MODE
	cancel_delayed_work_sync(&tui_tsp_info->work_noise_protect);
#else
	cancel_delayed_work_sync(&tui_tsp_info->work_force_release);
#endif

	tui_tsp_info->status.noise_mode = false;
}
EXPORT_SYMBOL(trustedui_mode_ist_on);

void trustedui_mode_ist_off(void)
{
	struct ist40xx_data *data = ts_data;

	input_info(true, &data->client->dev, "%s ", __func__);

	tui_tsp_info->status.noise_mode = true;

	//EVENT_TIMER_INTERVAL
	mod_timer(&tui_tsp_info->event_timer,
		  get_jiffies_64() + (HZ * tui_tsp_info->timer_period_ms / 1000));
}
EXPORT_SYMBOL(trustedui_mode_ist_off);
#endif

static void ist40xx_run_rawdata(struct ist40xx_data *data)
{
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	data->tsp_dump_lock = 1;
#endif
	input_raw_data_clear();
	input_raw_info(true, &data->client->dev, "%s start ##\n", __func__);
	ist40xx_display_booting_dump_log(data);
	input_raw_info(true, &data->client->dev, "%s done ##\n", __func__);
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	data->tsp_dump_lock = 0;
#endif
}

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include <linux/sec_ts_common.h>
extern struct tsp_dump_callbacks dump_callbacks;
static struct delayed_work *p_ghost_check;

static void ist40xx_check_rawdata(struct work_struct *work)
{
	struct ist40xx_data *data = container_of(work, struct ist40xx_data,
						 ghost_check.work);

	if (data->tsp_dump_lock == 1) {
		input_info(true, &data->client->dev,
			"%s: ignored ## already checking..\n", __func__);
		return;
	}
	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_info(true, &data->client->dev,
			"%s: ignored ## IC is power off\n", __func__);
		return;
	}

	ist40xx_display_key_dump_log(data);
}

static void dump_tsp_log(void)
{
	tsp_info("%s start\n", __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		tsp_info("%s: ignored ## lpm charging Mode!!\n",__func__);
		return;
	}
#endif

	if (p_ghost_check == NULL) {
		tsp_info("%s: ignored ## tsp probe fail!!\n", __func__);
		return;
	}

	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#endif

static void ist40xx_read_info_work(struct work_struct *work)
{
	struct ist40xx_data *data = container_of(work, struct ist40xx_data,
			work_read_info.work);
#ifdef TCLM_CONCEPT
	u32 buff;
	int ret;

	ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, &buff, 1);
	data->test_result.data[0] = buff & 0xff;

	/*this function will call ist40xx_tclm_data_read*/
	ret = sec_tclm_check_cal_case(data->tdata);
	input_info(true, &data->client->dev,
		   "%s sec_tclm_check_cal_case result %d; test result %X\n", __func__,
		   ret, data->test_result.data[0]);
#endif
#ifdef IST40XX_USE_CMCS
	run_cmcs_full_test((void *)&data->sec);
#endif
	input_log_fix();

	// for rawdata
	ist40xx_run_rawdata(data);
	data->info_work_done = true;
}

static void ist_set_input_prop_proximity(struct ist40xx_data *data, struct input_dev *dev)
{
	static char ist_phys[64] = { 0 };

	snprintf(ist_phys, sizeof(ist_phys), "%s/input1", dev->name);
	dev->phys = ist_phys;
	dev->id.bustype = BUS_I2C;
	dev->dev.parent = &data->client->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_SW, dev->evbit);

	set_bit(INPUT_PROP_DIRECT, dev->propbit);

	input_set_abs_params(dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_abs_params(dev, ABS_MT_CUSTOM2, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(dev, data);
}

static int ist40xx_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	int i;
	struct ist40xx_data *data;
	struct sec_tclm_data *tdata = NULL;
	struct input_dev *input_dev;

	input_info(true, &client->dev, "### IMAGIS probe(Slave Addr:0x%02X) ###\n",
		   client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	data = kzalloc(sizeof(struct ist40xx_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

#ifdef CONFIG_OF
	// TODO: Load device tree.
	data->dt_data = NULL;
	if (client->dev.of_node) {
		data->dt_data = kzalloc(sizeof(struct ist40xx_dt_data), GFP_KERNEL);
		if (!data->dt_data) {
			input_err(true, &client->dev, "failed to allocate dt data\n");
			goto err_alloc_dev;
		}

		ret = ist40xx_parse_dt(&client->dev, data);
		if (ret)
			goto err_alloc_dev;

		tdata = kzalloc(sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata)
			goto error_alloc_tdata;

		sec_tclm_parse_dt(client, tdata);
	} else {
		data->dt_data = NULL;
		input_err(true, &client->dev, "don't exist of_node\n");
		goto err_alloc_dev;
	}

	ist40xx_request_gpio(client, data);

	data->client = client;
#ifdef IST40XX_PINCTRL
	/* Get pinctrl if target uses pinctrl */
	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(data->pinctrl)) {
		if (PTR_ERR(data->pinctrl) == -EPROBE_DEFER)
			goto err_pinctrl;

		input_err(true, &client->dev, "%s Target does not use pinctrl\n",
			  __func__);
		data->pinctrl = NULL;
	}

	if (data->pinctrl) {
		ret = ist40xx_pinctrl_configure(data, true);
		if (ret) {
			input_err(true, &client->dev,
				  "%s cannot set pinctrl state\n", __func__);
		}
	}
#endif
#endif

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev, "input_allocate_device failed\n");
		goto err_alloc_dt;
	}

	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	data->tdata = tdata;
	if (!data->tdata)
		goto err_null_data;

	data->input_dev_proximity = input_allocate_device();
	if (!data->input_dev_proximity) {
		input_err(true, &client->dev, "%s: allocate input_dev_proximity err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_input_dev_proximity;
	}

	data->input_dev_proximity->name = "sec_touchproximity";
	ist_set_input_prop_proximity(data, data->input_dev_proximity);

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(data->tdata);
	data->tdata->client = data->client;
	data->tdata->tclm_read = ist40xx_tclm_data_read;
	data->tdata->tclm_write = ist40xx_tclm_data_write;
	data->tdata->tclm_execute_force_calibration = ist40xx_execute_force_calibration;
	data->tdata->external_factory = false;
	data->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif
	INIT_DELAYED_WORK(&data->work_read_info, ist40xx_read_info_work);
	INIT_DELAYED_WORK(&data->work_print_info, ist40xx_print_info_work);
#ifdef USE_OPEN_CLOSE
	input_dev->open = ist40xx_ts_open;
	input_dev->close = ist40xx_ts_close;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist40xx_early_suspend;
	data->early_suspend.resume = ist40xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	data->irq_enabled = false;
	data->status.event_mode = false;
	mutex_init(&data->lock);
	mutex_init(&data->aod_lock);
	mutex_init(&data->i2c_lock);
	mutex_init(&data->i2c_wc_lock);

	ts_data = data;

	/* initialize data variable */
	data->ignore_delay = false;
	data->irq_working = false;
	data->max_scan_retry = 2;
	data->max_irq_err_cnt = IST40XX_MAX_ERR_CNT;
	data->report_rate = -1;
	data->idle_rate = -1;
	data->timer_period_ms = 5000;
	data->status.sys_mode = STATE_POWER_OFF;
	data->rec_mode = 0;
	data->rec_type = 0;
	data->rec_file_name = kzalloc(IST40XX_REC_FILENAME_SIZE, GFP_KERNEL);
	data->debug_mode = 0;
	data->checksum_result = 0;
	data->multi_count = 0;
	data->all_finger_count = 0;
	data->all_spay_count = 0;
	data->all_aod_tsp_count = 0;
	data->all_singletap_count = 0;
	data->lpm_mode = 0;
	data->info_work_done = false;
	data->rejectzone_t = 0;
	data->rejectzone_b = 0;
	data->hover = 0;
	data->noise = 0;
	data->pocket = 0;
	data->fod_property = 0;

	for (i = 0; i < IST40XX_MAX_FINGER_ID; i++)
		data->tsp_touched[i] = false;

	for (i = 0; i < 4; i++)
		data->rect_data[i] = 0;

	/* system power init */
	ret = ist40xx_init_system(data);
	if (ret) {
		input_err(true, &client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

	/* init irq thread */
	input_info(true, &client->dev, "client->irq : %d\n", client->irq);
	ret = request_threaded_irq(client->irq, NULL, ist40xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist40xx_ts",
				   data);
	if (ret)
		goto err_init_drv;

	data->irq_enabled = true;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	trustedui_set_tsp_irq(client->irq);
	input_info(true, &client->dev, "%s[%d] called!\n", __func__, client->irq);
#endif

	ret = ist40xx_auto_bin_update(data);
	if (ret == 0)
		goto err_irq;

	ret = ist40xx_get_info(data);
	input_info(true, &client->dev, "Get info: %s\n", (ret == 0 ? "success" : "fail"));
	if (ret)
		goto err_read_info;

	ret = ist40xx_init_update_sysfs(data);
	if (ret)
		goto err_sysfs;

	ret = ist40xx_init_misc_sysfs(data);
	if (ret)
		goto err_sysfs;

	ret = ist40xx_init_cmcs_sysfs(data);
	if (ret)
		goto err_sysfs;

#ifdef SEC_FACTORY_MODE
	ret = sec_touch_sysfs(data);
	if (ret)
		goto err_sec_sysfs;
#endif

	input_info(true, &client->dev, "Create sysfs!!\n");

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
#ifdef IST40XX_NOISE_MODE
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
#else
	INIT_DELAYED_WORK(&data->work_force_release, release_work_func);
#endif

	init_timer(&data->event_timer);
	data->event_timer.data = (unsigned long)data;
	data->event_timer.function = timer_handler;
	data->event_timer.expires = jiffies_64 + EVENT_TIMER_INTERVAL;
	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	manager_notifier_register(&data->ccic_nb, tsp_ccic_notification,
			MANAGER_NOTIFY_PDIC_USB);
#else
#ifdef CONFIG_MUIC_NOTIFIER
	muic_notifier_register(&data->muic_nb, tsp_muic_notification,
			MUIC_NOTIFY_DEV_CHARGER);
#endif
#endif
#ifdef CONFIG_VBUS_NOTIFIER
	vbus_notifier_register(&data->vbus_nb, tsp_vbus_notification,
			VBUS_NOTIFY_DEV_CHARGER);
#endif
	device_init_wakeup(&client->dev, true);

	ist40xx_start(data);
	data->initialized = true;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = data;
#endif

	schedule_delayed_work(&data->work_read_info, msecs_to_jiffies(500));
	schedule_work(&data->work_print_info.work);

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = dump_tsp_log;
	INIT_DELAYED_WORK(&data->ghost_check, ist40xx_check_rawdata);
	p_ghost_check = &data->ghost_check;
#endif

#ifdef CONFIG_PM
	init_completion(&data->resume_done);
	complete_all(&data->resume_done);
#endif

	input_info(true, &client->dev, "### IMAGIS probe success ###\n");

	return 0;

#ifdef SEC_FACTORY_MODE
err_sec_sysfs:
	sec_touch_sysfs_remove(data);
	sec_cmd_exit(&data->sec, SEC_CLASS_DEVT_TSP);
#endif
err_sysfs:
	class_destroy(ist40xx_class);
err_read_info:
err_irq:
	ist40xx_disable_irq(data);
	free_irq(client->irq, data);
err_init_drv:
	input_free_device(input_dev);
	data->status.event_mode = false;
	ist40xx_power_off(data);
#if (defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
	unregister_early_suspend(&data->early_suspend);
#endif
err_allocate_input_dev_proximity:
err_null_data:
err_alloc_dt:
#ifdef IST40XX_PINCTRL
err_pinctrl:
#endif
	if (tdata)
		kfree(tdata);
	if (data->dt_data)
		ist40xx_free_gpio(data);
error_alloc_tdata:
	if (data->dt_data) {
		input_err(true, &client->dev, "Error, ist40xx mem free\n");
		kfree(data->dt_data);
	}
err_alloc_dev:
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	p_ghost_check = NULL;
#endif
	kfree(data);
	input_err(true, &client->dev, "Error, ist40xx init driver\n");

	return -ENODEV;
}

static int ist40xx_remove(struct i2c_client *client)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);

#if (defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
	unregister_early_suspend(&data->early_suspend);
#endif
	cancel_delayed_work_sync(&data->work_print_info);

	ist40xx_disable_irq(data);
	free_irq(client->irq, data);
	ist40xx_power_off(data);

#ifdef SEC_FACTORY_MODE
	sec_touch_sysfs_remove(data);
	sec_cmd_exit(&data->sec, SEC_CLASS_DEVT_TSP);
#endif

#ifdef CONFIG_OF
	if (data->dt_data) {
		ist40xx_free_gpio(data);
		kfree(data->dt_data);
	}
#endif

	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	input_unregister_device(data->input_dev_proximity);
	input_free_device(data->input_dev_proximity);	
	kfree(data);

	return 0;
}

static void ist40xx_shutdown(struct i2c_client *client)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&data->work_print_info);

	del_timer(&data->event_timer);
	cancel_delayed_work_sync(&data->work_reset_check);
#ifdef IST40XX_NOISE_MODE
	cancel_delayed_work_sync(&data->work_noise_protect);
#else
	cancel_delayed_work_sync(&data->work_force_release);
#endif
	mutex_lock(&data->lock);
	ist40xx_disable_irq(data);
	ist40xx_power_off(data);
	clear_input_data(data);
	mutex_unlock(&data->lock);
}

static struct i2c_device_id ist40xx_idtable[] = {
	{IST40XX_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ist40xx_idtable);

#ifdef CONFIG_OF
static struct of_device_id ist40xx_match_table[] = {
	{.compatible = "imagis,ist40xx-ts",},
	{},
};
#else
#define ist40xx_match_table NULL
#endif

#if (!defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
static const struct dev_pm_ops ist40xx_pm_ops = {
	.suspend = ist40xx_suspend,
	.resume = ist40xx_resume,
};
#else
#ifdef CONFIG_PM
static int ist40xx_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist40xx_data *data = i2c_get_clientdata(client);

	reinit_completion(&data->resume_done);
#ifndef USE_OPEN_CLOSE
	ist40xx_suspend(dev);
#endif
	return 0;
}

static int ist40xx_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist40xx_data *data = i2c_get_clientdata(client);

	complete_all(&data->resume_done);
#ifndef USE_OPEN_CLOSE
	ist40xx_resume(dev);
#endif
	return 0;
}

static const struct dev_pm_ops ist40xx_pm_ops = {
	.suspend = ist40xx_pm_suspend,
	.resume = ist40xx_pm_resume,
};
#endif
#endif

static struct i2c_driver ist40xx_i2c_driver = {
	.id_table = ist40xx_idtable,
	.probe = ist40xx_probe,
	.remove = ist40xx_remove,
	.shutdown = ist40xx_shutdown,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = IST40XX_DEV_NAME,
		   .of_match_table = ist40xx_match_table,
#if (!defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
		   .pm = &ist40xx_pm_ops,
#else
#ifdef CONFIG_PM
		   .pm = &ist40xx_pm_ops,
#endif
#endif
		   },
};

static int __init ist40xx_init(void)
{
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		tsp_info("%s: Do not load driver due to : lpm %d\n", __func__,
			 lpcharge);
		return -ENODEV;
	}
#endif

	return i2c_add_driver(&ist40xx_i2c_driver);
}

static void __exit ist40xx_exit(void)
{
	i2c_del_driver(&ist40xx_i2c_driver);
}

module_init(ist40xx_init);
module_exit(ist40xx_exit);

MODULE_DESCRIPTION("Imagis IST40XX touch driver");
MODULE_LICENSE("GPL");
