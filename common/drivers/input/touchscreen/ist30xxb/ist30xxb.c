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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <mach/gpio.h>

#include <linux/input/mt.h>
#include <mach/i2c-sprd.h>
#include <linux/input/ist30xxb.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif
#include "ist30xxb_update.h"
#include "ist30xxb_tracking.h"

#if IST30XX_CMCS_TEST
#include "ist30xxb_cmcs.h"
#endif
#if IST30XX_DEBUG
#include "ist30xxb_misc.h"
#endif
#if SEC_FACTORY_MODE
#include "ist30xxb_sec.h"
extern int sec_touch_sysfs(struct ist30xx_data *data);
extern int sec_fac_cmd_init(struct ist30xx_data *data);
#endif
#define TOUCH_BOOSTER	1
#define MAX_ERR_CNT             (100)

#if TOUCH_BOOSTER
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
extern int _store_cpu_num_min_limit(unsigned int input);
struct cpufreq_limit_handle *min_handle = NULL;
static const unsigned long touch_cpufreq_lock = 1200000;
#endif

#if IST30XX_USE_KEY
int ist30xx_key_code[] = { 0, KEY_RECENT, KEY_BACK };
#endif

DEFINE_MUTEX(ist30xx_mutex);

volatile bool ist30xx_irq_working = false;
static bool ist30xx_initialized = 0;
struct ist30xx_data *ts_data;
struct tsp_dev_info *ts_pdata;
EXPORT_SYMBOL(ts_data);
EXPORT_SYMBOL(ts_pdata);

#if IST30XX_DETECT_TA
static int ist30xx_ta_status = -1;
#endif
#if IST30XX_DETECT_CALLER
static int ist30xx_call_status = -1;
#endif
#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static struct delayed_work work_fw_update;
#endif

u32 event_ms = 0, timer_ms = 0;
int ist30xx_scan_retry = 0;

#if IST30XX_EVENT_MODE
int timer_period_ms = 500;                      // 0.5sec
#define EVENT_TIMER_INTERVAL     (HZ * timer_period_ms / 1000)
void timer_handler(unsigned long data);
#endif  // IST30XX_EVENT_MODE

#if IST30XX_DEBUG
extern TSP_INFO ist30xx_tsp_info;
extern TKEY_INFO ist30xx_tkey_info;
#endif

int ist30xx_dbg_level = IST30XX_DEBUG_LEVEL;
void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r;

	if (ist30xx_dbg_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	r = printk("%s %pV", IST30XX_DEBUG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(void)
{
#if IST30XX_EVENT_MODE
	struct timespec t_current;               // ns
	ktime_get_ts(&t_current);

	return t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
#else
	return 0;
#endif  // IST30XX_EVENT_MODE
}

int ist30xx_intr_wait(long ms)                            // int ist30xx_intr_wait ߰
{
	long start_ms = get_milli_second();
	long curr_ms = 0;
	while (1) {
		if (!ist30xx_irq_working)
			break;
		curr_ms = get_milli_second();
		if ((curr_ms < 0) || (start_ms < 0) || (curr_ms - start_ms > ms)) {
			tsp_info("%s() timeout(%dms)\n", __func__, ms);
			return -EPERM;
		}
		msleep(2);
	}
	return 0;
}


void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (data->irq_enabled) {
		ist30xx_tracking(TRACK_INTR_DISABLE);
		disable_irq(data->client->irq);
		//printk("[TSP] ist30xx_disable_irq : %d\n", data->client->irq);
		data->irq_enabled = 0;
		data->status.event_mode = false;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (!data->irq_enabled) {
		ist30xx_tracking(TRACK_INTR_ENABLE);
		enable_irq(data->client->irq);
		//printk("[TSP] ist30xx_enable_irq : %d\n", data->client->irq);
		msleep(10);
		data->irq_enabled = 1;
		data->status.event_mode = true;
	}
}


int ist30xx_max_error_cnt = MAX_ERR_CNT;
int ist30xx_error_cnt = 0;
static void ist30xx_request_reset(void)
{
	ist30xx_error_cnt++;
	if (ist30xx_error_cnt >= ist30xx_max_error_cnt) {
		tsp_info("%s()\n", __func__);
		schedule_delayed_work(&ts_data->work_reset_check, 0);
		ist30xx_error_cnt = 0;
	}
}


void ist30xx_start(struct ist30xx_data *data)
{
#if IST30XX_DETECT_TA
	if (ist30xx_ta_status > -1) {
		ist30xx_tracking((ist30xx_ta_status ?
				  TRACK_CMD_TACON : TRACK_CMD_TADISCON));
		ist30xx_write_cmd(data->client, CMD_SET_TA_MODE, ist30xx_ta_status);
		tsp_info("%s(), ta_mode : %d\n", __func__, ist30xx_ta_status);
	}
#endif
#if IST30XX_DETECT_CALLER
	if (ist30xx_call_status > -1) {
		ist30xx_write_cmd(data->client, CMD_SET_CALL_MODE, ist30xx_call_status);
		tsp_info("%s(), call_mode : %d\n", __func__, ist30xx_call_status);
	}
#endif
#if IST30XX_REPORT_RATE
	if (data->ist30xx_report_rate >= 0) {
		ist30xx_write_cmd(data->client, CMD_SET_REPORT_RATE, data->ist30xx_report_rate);
		tsp_info(" reporting rate : %dus\n", data->ist30xx_report_rate);
	}

	if (data->ist30xx_idle_rate >= 0) {
		ist30xx_write_cmd(data->client, CMD_SET_IDLE_TIME, data->ist30xx_idle_rate);
		tsp_info(" reporting rate : %dus\n", data->ist30xx_idle_rate);
	}
#endif
	ist30xx_cmd_start_scan(data->client);
}


int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.prev_core_ver = data->fw.core_ver;
	data->fw.prev_param_ver = data->fw.param_ver;
	data->fw.core_ver = data->fw.param_ver = 0;

	ret = ist30xx_read_cmd(data->client, CMD_GET_FW_VER, &data->fw.core_ver);
	if (ret)
		return ret;

	ret = ist30xx_read_cmd(data->client, CMD_GET_PARAM_VER, &data->fw.param_ver);
	if (ret)
		return ret;
#if IST30XX_SUPPORT_SUB_VER
	ret = ist30xx_read_cmd(data->client, CMD_GET_SUB_VER, &data->fw.sub_ver);
	if (ret)
		return ret;

	tsp_info("IC version read core: %x, param: %x, sub: %x\n",
		 data->fw.core_ver, data->fw.param_ver, data->fw.sub_ver);
#else
	tsp_info("IC version read core: %x, param: %x\n",
		 data->fw.core_ver, data->fw.param_ver);
#endif
	return 0;
}


int ist30xx_init_touch_driver(struct ist30xx_data *data)
{
	int ret = 0;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client, true);
	if (ret)
		goto init_touch_end;

	ist30xx_get_ver_info(data);

init_touch_end:
	ist30xx_start(data);

	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


#if IST30XX_DEBUG
void ist30xx_print_info(void)
{
	TSP_INFO *tsp = &ist30xx_tsp_info;
	TKEY_INFO *tkey = &ist30xx_tkey_info;

	tsp_info("*** TSP/TKEY info ***\n");
	tsp_info("tscn dir swap: %d, flip x: %d, y: %d\n",
		  tsp->dir.swap_xy, tsp->dir.flip_x, tsp->dir.flip_y);
	tsp_info("tscn ch_num tx: %d, rx: %d\n",
		  tsp->ch_num.tx, tsp->ch_num.rx);
	tsp_info("tscn width: %d, height: %d\n",
		  tsp->width, tsp->height);
	tsp_info("tkey enable: %d, key num: %d, axis rx: %d \n",
		  tkey->enable, tkey->key_num, tkey->axis_rx);
	tsp_info("tkey ch_num[0] %d, [1] %d, [2] %d, [3] %d, [4] %d\n",
		  tkey->ch_num[0], tkey->ch_num[1], tkey->ch_num[2],
		  tkey->ch_num[3], tkey->ch_num[4]);
}
#endif

#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

#if IST30XX_INTERNAL_BIN
#if IST30XX_DEBUG
	ist30xx_get_tsp_info(data);
	ist30xx_get_tkey_info(data);
#endif  // IST30XX_DEBUG
#else
	ret = ist30xx_cmd_run_device(data->client, false);
	if (ret)
		goto get_info_end;

	ret = ist30xx_get_ver_info(data);
	if (ret)
		goto get_info_end;

# if IST30XX_DEBUG
	ret = ist30xx_tsp_update_info();
	if (ret)
		goto get_info_end;

	ret = ist30xx_tkey_update_info();
	if (ret)
		goto get_info_end;
# endif // IST30XX_DEBUG
#endif  // IST30XX_INTERNAL_BIN

#if IST30XX_DEBUG
	ist30xx_print_info();
	data->max_fingers = ist30xx_tsp_info.finger_num;
	data->max_keys = ist30xx_tkey_info.key_num;
#endif  // IST30XX_DEBUG

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_CALIB_RESULT, &calib_msg);
	if (ret == 0) {
		tsp_info("calib status: 0x%08x\n", calib_msg);
		ist30xx_tracking(calib_msg);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID ||
		    CALIB_TO_STATUS(calib_msg) > 0) {
			ist30xx_calibrate(IST30XX_FW_UPDATE_RETRY);

			ist30xx_cmd_run_device(data->client, true);
		}
	}
#if (IST30XX_EVENT_MODE && IST30XX_CHECK_CALIB)
	if (!data->status.update) {
		ret = ist30xx_cmd_check_calib(data->client);
		if (!ret) {
			data->status.calib = 1;
			event_ms = (u32)get_milli_second();
			data->status.event_mode = true;
		}
	}
#else
	ist30xx_start(ts_data);
#endif

#if !(IST30XX_INTERNAL_BIN)
get_info_end:
#endif
	if (ret == 0)
		ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x6)

#define TOUCH_DOWN_MESSAGE      ("p")
#define TOUCH_UP_MESSAGE        ("r")
#define TOUCH_MOVE_MESSAGE      (" ")
bool tsp_touched[IST30XX_MAX_MT_FINGERS] = { false, };

void print_tsp_event(finger_info *finger)
{
#if TOUCH_BOOSTER
	static finger_cnt = 0;
#endif
	int idx = finger->bit_field.id - 1;
	bool press = (finger->bit_field.udmg & PRESS_MSG_MASK) ? true : false;

	if(idx < 0){
		tsp_info("id value is minus! something is wrong...please check FW status!\n");
		return;
	}
	if (press) {
		if (tsp_touched[idx] == false) { // touch down
#if TOUCH_BOOSTER
			//if(!finger_cnt)
			{
				if(!min_handle)
			{
				min_handle = cpufreq_limit_min_freq(touch_cpufreq_lock, "TSP");
				if (IS_ERR(min_handle)) {
						printk(KERN_ERR "[TSP] cannot get cpufreq_min lock %lu(%d)\n",
					touch_cpufreq_lock, PTR_ERR(min_handle));
					min_handle = NULL;
				}
					_store_cpu_num_min_limit(2);
				tsp_info("cpu freq on\n");
			}
			}
			finger_cnt ++;
#endif
#if ENABLED_TSP_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_info("%s%d (%d, %d)\n",
				 TOUCH_DOWN_MESSAGE, finger->bit_field.id,
				 finger->bit_field.x, finger->bit_field.y);
#else
			tsp_info("touch down %d\n", finger->bit_field.id);
#endif
#endif
			tsp_touched[idx] = true;
		} else {                    // touch move
#if ENABLED_TSP_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_debug("%s%d (%d, %d)\n",
				  TOUCH_MOVE_MESSAGE, finger->bit_field.id,
				  finger->bit_field.x, finger->bit_field.y);
#else
			tsp_debug("touch move %d\n", finger->bit_field.id);
#endif
#endif
		}
	} else {
		if (tsp_touched[idx] == true) { // touch up
#if ENABLED_TSP_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_info("%s%d (%d, %d)\n",
				 TOUCH_UP_MESSAGE, finger->bit_field.id,
				 finger->bit_field.x, finger->bit_field.y);
#else
			tsp_info("touch up %d\n", finger->bit_field.id);
#endif
#endif
			tsp_touched[idx] = false;
#if TOUCH_BOOSTER
			finger_cnt --;
			if(!finger_cnt)
			{
			//printk(KERN_ERR "[TOUCH_BOOSTER]%s %d\n",__func__,__LINE__);
				cpufreq_limit_put(min_handle);
				min_handle = NULL;
				_store_cpu_num_min_limit(1);
				tsp_info("cpu freq off\n");
			}
#endif
		}
	}
}


static void release_finger(finger_info *finger)
{
	input_mt_slot(ts_data->input_dev, finger->bit_field.id - 1);
	input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);

	ist30xx_tracking(TRACK_POS_FINGER + finger->bit_field.id);
#if ENABLED_TSP_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	tsp_info("%s() %d(%d, %d)\n", __func__,
		 finger->bit_field.id, finger->bit_field.x, finger->bit_field.y);
#else
	tsp_info("%s\n",__func__);
#endif
#endif
	finger->bit_field.udmg &= ~(PRESS_MSG_MASK);
	print_tsp_event(finger);

	finger->bit_field.id = 0;

	input_sync(ts_data->input_dev);
}


#define CANCEL_KEY  (0xff)
#define RELEASE_KEY (0)
static void release_key(finger_info *key, u8 key_status)
{
	int id = key->bit_field.id;

	input_report_key(ts_data->input_dev, ist30xx_key_code[id], key_status);

	ist30xx_tracking(TRACK_POS_KEY + id);
#if ENABLED_TSK_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	tsp_info("%s() key%d, event: %d, status: %d\n", __func__,
		  id, key->bit_field.w, key_status);
#else
	tsp_info("%s\n",__func__);
#endif
#endif
	key->bit_field.id = 0;

	input_sync(ts_data->input_dev);
}

static void clear_input_data(struct ist30xx_data *data)
{
	int i;
	finger_info *fingers = (finger_info *)data->prev_fingers;
	finger_info *keys = (finger_info *)data->prev_keys;

	for (i = 0; i < data->num_fingers; i++) {
		if (fingers[i].bit_field.id == 0)
			continue;

		if (fingers[i].bit_field.udmg & PRESS_MSG_MASK)
			release_finger(&fingers[i]);
	}

	for (i = 0; i < data->num_keys; i++) {
		if (keys[i].bit_field.id == 0)
			continue;

		if (keys[i].bit_field.w == PRESS_MSG_KEY)
			release_key(&keys[i], RELEASE_KEY);
	}
}

static int check_report_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int i, j;
	bool valid_id;
	finger_info *fingers = (finger_info *)data->fingers;
	finger_info *prev_fingers = (finger_info *)data->prev_fingers;

	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if ((fingers[i].bit_field.id == 0) ||
		    (fingers[i].bit_field.id > data->max_fingers) ||
		    (fingers[i].bit_field.x > IST30XX_MAX_X) ||
			     (fingers[i].bit_field.y > IST30XX_MAX_Y)) {
			tsp_warn("Invalid touch data - %d: %d(%d, %d), 0x%08x\n", i,
				 fingers[i].bit_field.id,
				 fingers[i].bit_field.x,
				 fingers[i].bit_field.y,
				 fingers[i].full_field);

			fingers[i].bit_field.id = 0;
			ist30xx_tracking(TRACK_POS_UNKNOWN);
			return -EPERM;
		}
	}

	/* previous finger info */
	if (data->num_fingers >= finger_counts) {
		for (i = 0; i < data->max_fingers; i++) { // prev_fingers
			if (prev_fingers[i].bit_field.id != 0 &&
			    (prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK)) {
				valid_id = false;
				for (j = 0; j < data->max_fingers; j++) { // fingers
					if ((prev_fingers[i].bit_field.id) ==
					    (fingers[j].bit_field.id)) {
						valid_id = true;
						break;
					}
				}
				if (valid_id == false)
					release_finger(&prev_fingers[i]);
			}
		}
	}

	return 0;
}

bool finger_on_screen(void)
{
	int i;

	for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++)
		if (tsp_touched[i]) return true;

	return false;
}

int key_press = 0;
int key_id = 0;
static void report_input_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int i, id;
	bool press = false;
	finger_info *fingers = (finger_info *)data->fingers;

	memset(data->prev_fingers, 0, sizeof(data->prev_fingers));

	for (i = 0; i < finger_counts; i++) {
		press = (fingers[i].bit_field.udmg & PRESS_MSG_MASK) ? true : false;

		print_tsp_event(&fingers[i]);

		input_mt_slot(data->input_dev, fingers[i].bit_field.id - 1);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		if (press) {
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					 fingers[i].bit_field.x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					 fingers[i].bit_field.y);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					 fingers[i].bit_field.w);
		}

		data->prev_fingers[i] = fingers[i];
	}

#if IST30XX_USE_KEY
	for (i = finger_counts; i < finger_counts + key_counts; i++) {
		id = fingers[i].bit_field.id;
		press = (fingers[i].bit_field.w == PRESS_MSG_KEY) ? true : false;
#if ENABLED_TSK_LOG
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		tsp_info("key(%08x) id: %d, press: %d, val: (%d, %d)\n",
			  fingers[i].full_field, id, press,
			  fingers[i].bit_field.x, fingers[i].bit_field.y);
#else
		tsp_info("key press: %d\n",press);
#endif
#endif
		input_report_key(data->input_dev, ist30xx_key_code[id], press);

		data->prev_keys[id - 1] = fingers[i];
	}
#endif  // IST30XX_USE_KEY

	data->num_fingers = finger_counts;
	data->num_keys = key_counts;
	ist30xx_error_cnt = 0;
	ist30xx_scan_retry = 0;

	input_sync(data->input_dev);
}

/*
 * CMD : CMD_GET_COORD
 *               [31:30]  [29:26]  [25:16]  [15:10]  [9:0]
 *   Multi(1st)  UDMG     Rsvd.    NumOfKey Rsvd.    NumOfFinger
 *    Single &   UDMG     ID       X        Area     Y
 *   Multi(2nd)
 *
 *   UDMG [31] 0/1 : single/multi
 *   UDMG [30] 0/1 : unpress/press
 */
u32 intr_debug_addr = 0;
u32 intr_debug_size = 0;
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = ts_data;
	u32 msg[IST30XX_MAX_MT_FINGERS];
	u32 ms;

	ist30xx_irq_working = true;

	if (!data->irq_enabled)
		goto irq_end;

	ms = get_milli_second();
/* _store_cpu_num_min_limit() faunction can make 50~70ms delay, so remove below code*/
/*	if ((ms >= event_ms) && (ms - event_ms < 6)) // Noise detect
		goto irq_end;*/

	memset(msg, 0, sizeof(msg));

	ret = ist30xx_get_position(data->client, msg, 1);
	if (ret)
		goto irq_err;

	tsp_verb("ist30xx_irq_thread msg: 0x%08x\n", *msg);

	if (*msg == 0xE11CE970)     // TSP IC Exception
		goto irq_ic_err;

	if (*msg == 0 || *msg == 0xFFFFFFFF || *msg == 0x2FFF03FF ||
	    *msg == 0x30003000 || *msg == 0x300B300B)   // Unknown CMD
		goto irq_err;

	event_ms = ms;
	ist30xx_put_track_ms(event_ms);
	ist30xx_put_track(msg, 1);

	if ((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
		data->status.calib_msg = *msg;
		tsp_info("calib status: 0x%08x\n", data->status.calib_msg);

		goto irq_end;
	}

	memset(data->fingers, 0, sizeof(data->fingers));

	data->fingers[0].full_field = *msg;
	key_cnt = 0;
	finger_cnt = 1;
	read_cnt = 1;

	if (data->fingers[0].bit_field.udmg & MULTI_MSG_MASK) {
		key_cnt = data->fingers[0].bit_field.x;
		finger_cnt = data->fingers[0].bit_field.y;
		read_cnt = finger_cnt + key_cnt;

		if (finger_cnt > data->max_fingers ||
		    key_cnt > data->max_keys) {
			tsp_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
				 finger_cnt, data->max_fingers,
				 key_cnt, data->max_keys);
			goto irq_err;
		}

		if (read_cnt > 0) {
#if I2C_BURST_MODE
			ret = ist30xx_get_position(data->client, msg, read_cnt);
			if (ret)
				goto irq_err;

			for (i = 0; i < read_cnt; i++)
				data->fingers[i].full_field = msg[i];
#else
			for (i = 0; i < read_cnt; i++) {
				ret = ist30xx_get_position(data->client, &msg[i], 1);
				if (ret)
					goto irq_err;

				data->fingers[i].full_field = msg[i];
			}
#endif                  // I2C_BURST_MODE

			ist30xx_put_track(msg, read_cnt);
		}
	}

	if (check_report_data(data, finger_cnt, key_cnt))
		goto irq_end;

	if (read_cnt > 0)
		report_input_data(data, finger_cnt, key_cnt);

	if (intr_debug_addr > 0 && intr_debug_size > 0) {
		ret = ist30xxb_burst_read(ts_data->client,
					  intr_debug_addr, &msg[0], intr_debug_size);
		for (i = 0; i < intr_debug_size; i++)
			tsp_debug("\t%08x\n", msg[i]);
	}

	goto irq_end;

irq_err:
	tsp_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xx_request_reset();
irq_end:
	ist30xx_irq_working = false;
	event_ms = (u32)get_milli_second();
	return IRQ_HANDLED;

irq_ic_err:
	tsp_err("Occured IC exception\n");
	schedule_delayed_work(&data->work_reset_check, 0);
	ist30xx_irq_working = false;
	event_ms = (u32)get_milli_second();
	return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
#define ist30xx_suspend NULL
#define ist30xx_resume  NULL
static void ist30xx_early_suspend(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);
#if IST30XX_EVENT_MODE
	unsigned long flags;
#endif
#if IST30XX_EVENT_MODE
	spin_lock_irqsave(&data->timer_lock, flags);
	del_singleshot_timer_sync(&data->event_timer);
	spin_unlock_irqrestore(&data->timer_lock, flags);
#endif
	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
	
#if TOUCH_BOOSTER
	if(min_handle)
	{
		printk(KERN_ERR"[TSP] %s %d:: OOPs, Cpu was not in Normal Freq..\n", __func__, __LINE__);
		
		if (cpufreq_limit_put(min_handle) < 0) {
			printk(KERN_ERR "[TSP] Error in scaling down cpu frequency\n");
		}
		
		min_handle = NULL;
		tsp_info("cpu freq off\n");
	}
#endif
	
	mutex_unlock(&ist30xx_mutex);
}
static void ist30xx_late_resume(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);
#if IST30XX_EVENT_MODE
	unsigned long flags;
#endif
	mutex_lock(&ist30xx_mutex);
	ist30xx_internal_resume(data);
	ist30xx_start(data);
#if IST30XX_EVENT_MODE
	spin_lock_irqsave(&data->timer_lock, flags);
	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
	spin_unlock_irqrestore(&data->timer_lock, flags);
#endif
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);
}
#else
static int ist30xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);
	ist30xx_internal_suspend(data);
#if TOUCH_BOOSTER
	if(min_handle)
	{
		printk(KERN_ERR"[TSP] %s %d:: OOPs, Cpu was not in Normal Freq..\n", __func__, __LINE__);

		if (cpufreq_limit_put(min_handle) < 0) {
			printk(KERN_ERR "[TSP] Error in scaling down cpu frequency\n");
		}

		min_handle = NULL;
		tsp_info("cpu freq off\n");
	}
#endif
	
	return 0;
}
static int ist30xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	return ist30xx_internal_resume(data);
}
#endif // CONFIG_HAS_EARLYSUSPEND

static void ist30xx_shutdown(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);
#if IST30XX_EVENT_MODE
	unsigned long flags;
#endif
	if(ist30xx_initialized){
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(data);
		ist30xx_internal_suspend(data);
		mutex_unlock(&ist30xx_mutex);
#if IST30XX_EVENT_MODE
/*make deadlock if do shutdown while timer expired*/
//		spin_lock_irqsave(&data->timer_lock, flags);
		del_singleshot_timer_sync(&data->event_timer);
//		spin_unlock_irqrestore(&data->timer_lock, flags);
#endif
	}
}
void ist30xx_set_ta_mode(bool charging)
{
	if(ist30xx_initialized){
		tsp_info("%s(), charging = %d\n", __func__, charging);
#if IST30XX_DETECT_TA
		if (charging == ist30xx_ta_status)
			return;

		if (ist30xx_ta_status == -1) {
			ist30xx_ta_status = charging ? 1 : 0;
			return;
		}

		ist30xx_ta_status = charging ? 1 : 0;

		tsp_info("%s(), status = %d\n", __func__, ist30xx_ta_status);

		schedule_delayed_work(&ts_data->work_reset_check, 0);
#endif
	}
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_call_mode(int mode)
{
	if(ist30xx_initialized){
		tsp_info("%s(), mode = %d\n", __func__, mode);

#if IST30XX_DETECT_CALLER
		if (mode == ist30xx_ta_status)
			return;

		if (ist30xx_ta_status == -1) {
			ist30xx_ta_status = mode ? 1 : 0;
			return;
		}

		ist30xx_call_status = mode;

		tsp_info("%s(), call = %d\n", __func__, ist30xx_call_status);

		schedule_delayed_work(&ts_data->work_reset_check, 0);
#endif
	}
}
EXPORT_SYMBOL(ist30xx_set_call_mode);


void charger_enable(int enable)
{
	bool charging = enable ? true : false;

	ist30xx_set_ta_mode(charging);
}

static void reset_work_func(struct work_struct *work)
{
#if IST30XX_EVENT_MODE
	unsigned long flags;
#endif
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	tsp_info("Request reset function\n");

	if ((ist30xx_initialized == 1) && (ts_data->status.power == 1) &&
	    (ts_data->status.update != 1) && (ts_data->status.calib != 1)) {
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(ts_data);

		clear_input_data(ts_data);

		ist30xx_cmd_run_device(ts_data->client, true);

		ist30xx_start(ts_data);
#if IST30XX_EVENT_MODE
		spin_lock_irqsave(&ts_data->timer_lock, flags);
		mod_timer(&ts_data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
		spin_unlock_irqrestore(&ts_data->timer_lock, flags);
#endif
		ist30xx_enable_irq(ts_data);
		mutex_unlock(&ist30xx_mutex);
	}
}

#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	tsp_info("FW update function\n");

	if (ist30xx_auto_bin_update(ts_data))
		ist30xx_disable_irq(ts_data);
}
#endif // IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE


#if IST30XX_EVENT_MODE
u32 ist30xx_max_scan_retry = 2;
u32 ist30xx_scan_count = 0;
u32 ist30xx_algr_addr = 0, ist30xx_algr_size = 0;

#define SCAN_STATUS_MAGIC   (0x3C000000)
#define SCAN_STATUS_MASK    (0xFF000000)
#define FINGER_CNT_MASK     (0x00F00000)
#define SCAN_CNT_MASK       (0x000FFFFF)
#define GET_FINGER_CNT(k)   ((k & FINGER_CNT_MASK) >> 20)
#define GET_SCAN_CNT(k)     (k & SCAN_CNT_MASK)

static void noise_work_func(struct work_struct *work)
{
#if IST30XX_NOISE_MODE
	int i, ret;
	u32 scan_status = 0;

	ret = ist30xx_read_cmd(ts_data->client, IST30XXB_MEM_COUNT, &scan_status);
	if (ret) {
		tsp_warn("Mem scan count read fail!\n");
		goto retry_timer;
	}

	ist30xx_put_track_ms(timer_ms);
	ist30xx_put_track(&scan_status, 1);

	tsp_verb("scan status: 0x%x\n", scan_status);

	/* Check valid scan count */
	if ((scan_status & SCAN_STATUS_MASK) != SCAN_STATUS_MAGIC) {
		tsp_warn("Scan status is not corrected! (0x%08x)\n", scan_status);
		goto retry_timer;
	}

	/* Status of IC is idle */
	if (GET_FINGER_CNT(scan_status) == 0) {
		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
			if (ts_data->prev_fingers[i].bit_field.id == 0)
				continue;

			if (ts_data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK) {
				tsp_warn("prev_fingers: 0x%08x\n",
					 ts_data->prev_fingers[i].full_field);
				release_finger(&ts_data->prev_fingers[i]);
			}
		}

		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
			if (ts_data->prev_keys[i].bit_field.id == 0)
				continue;

			if (ts_data->prev_keys[i].bit_field.w == PRESS_MSG_KEY) {
				tsp_warn("prev_keys: 0x%08x\n",
					 ts_data->prev_keys[i].full_field);
				release_key(&ts_data->prev_keys[i], RELEASE_KEY);
			}
		}
	}

	scan_status &= SCAN_CNT_MASK;

	/* Status of IC is lock-up */
	if (scan_status == ist30xx_scan_count) {
		tsp_warn("TSP IC is not responded! (0x%08x)\n", scan_status);
		goto retry_timer;
	}
	else
		ist30xx_scan_retry = 0;

	ist30xx_scan_count = scan_status;

	return;

retry_timer:
	ist30xx_scan_retry++;
	tsp_warn("Retry scan status!(%d)\n", ist30xx_scan_retry);

	if (ist30xx_scan_retry == ist30xx_max_scan_retry) {
		schedule_delayed_work(&ts_data->work_reset_check, 0);
		ist30xx_scan_retry = 0;
	}
#endif  // IST30XX_NOISE_MODE
}

static void debug_work_func(struct work_struct *work)
{
#if IST30XX_ALGORITHM_MODE
	int ret = -EPERM;
	ALGR_INFO algr;
	u32 *buf32 = (u32 *)&algr;

	ret = ist30xxb_burst_read(ts_data->client,
				  ist30xx_algr_addr, (u32 *)&algr, ist30xx_algr_size);
	if (ret) {
		tsp_warn("Algorithm mem addr read fail!\n");
		return;
	}

	ist30xx_put_track(buf32, ist30xx_algr_size);

	tsp_debug(" 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		  buf32[0], buf32[1], buf32[2], buf32[3], buf32[4]);

	tsp_debug("  Scanstatus: %x\n", algr.scan_status);
	tsp_debug("  TouchCnt: %d\n", algr.touch_cnt);
	tsp_debug("  IntlTouchCnt: %d\n", algr.intl_touch_cnt);
	tsp_debug("  StatusFlag: %d\n", algr.status_flag);
	tsp_debug("  RAWPeakMax: %d\n", algr.raw_peak_max);
	tsp_debug("  RAWPeakMin: %d\n", algr.raw_peak_min);
	tsp_debug("  FLTPeakMax: %d\n", algr.flt_peak_max);
	tsp_debug("  AdptThreshold: %d\n", algr.adpt_threshold);
	tsp_debug("  KeyRawData0: %d\n", algr.key_raw_data[0]);
	tsp_debug("  KeyRawData1: %d\n", algr.key_raw_data[1]);
#endif // IST30XX_ALGORITHM_MODE
}

void timer_handler(unsigned long data)
{
	struct ist30xx_status *status = &ts_data->status;
	unsigned long flags;

	if (ist30xx_irq_working)
		goto restart_timer;

	if (status->event_mode) {
		if ((status->power == 1) && (status->update != 1)) {
			timer_ms = (u32)get_milli_second();
			if (status->calib == 1) {
				if (timer_ms - event_ms >= 3000) {   // 3second
					tsp_debug("calibration timeout over 3sec\n");
					schedule_delayed_work(&ts_data->work_reset_check, 0);
					status->calib = 0;
				}
			} else if (status->noise_mode) {
				if (timer_ms - event_ms > 100)
					schedule_delayed_work(&ts_data->work_noise_protect, 0);
			}

#if IST30XX_ALGORITHM_MODE
			if ((ist30xx_algr_addr >= IST30XXB_ACCESS_ADDR) &&
			    (ist30xx_algr_size > 0))
				schedule_delayed_work(&ts_data->work_debug_algorithm, 0);
#endif                 // IST30XX_ALGORITHM_MODE
		}
	}

restart_timer:
	spin_lock_irqsave(&ts_data->timer_lock, flags);
	mod_timer(&ts_data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
	spin_unlock_irqrestore(&ts_data->timer_lock, flags);
}
#endif // IST30XX_EVENT_MODE


static int ist30xx_probe(struct i2c_client *		client,
				   const struct i2c_device_id * id)
{
	int ret;
	int retry = 3;
	struct ist30xx_data *data;
	struct input_dev *input_dev;
        int ist_gpio = 0;
	struct tsp_dev_info *pdata = client->dev.platform_data;
#if IST30XX_EVENT_MODE
	unsigned long flags;
#endif
	tsp_info("[TSP] %s(), the i2c addr=0x%x", __func__, client->addr);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

#ifdef CONFIG_OF

        struct device_node *np = client->dev.of_node;

        printk("[TSP] the client->dev.of_node=0x%x\n", client->dev.of_node);
        
    if (client->dev.of_node){
            ist_gpio = of_get_gpio(np, 0);              
            printk("[TSP] gpio id=%d", ist_gpio);
        }
#endif

	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		ret = -ENOMEM;
		tsp_err("%s(), input_allocate_device failed (%d)\n", __func__, ret);
		goto err_alloc_dev;
	}

	data->max_fingers = data->max_keys = IST30XX_MAX_MT_FINGERS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS, 0);

	input_dev->name = "ist30xx_ts_input";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, IST30XX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, IST30XX_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, IST30XX_MAX_W, 0, 0);

#if IST30XX_USE_KEY
	{
		int i;
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(EV_SYN, input_dev->evbit);
		for (i = 1; i < ARRAY_SIZE(ist30xx_key_code); i++)
			set_bit(ist30xx_key_code[i], input_dev->keybit);
	}
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

	ts_data = data;
	ts_pdata = pdata;
#ifdef TSP_USE_GPIO_CONTROL
	ret = gpio_request(TSP_PWR_GPIO_EN, "ist30xx_ldo_en_gpio");
	if(ret)
		printk("ist30xx_ldo_en_gpio gpio request fail!\n");
#endif

	ret = ist30xx_init_system();
	if (ret) {
		dev_err(&client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

	

#ifndef CONFIG_OF
      ist_gpio = pdata->gpio;
#endif
	/* configure touchscreen interrupt gpio */
	ret = gpio_request(ist_gpio, "ist30xx_irq_gpio");
        printk("[TSP] pdata->gpio : %d\n", ist_gpio);
	if (ret) {
		tsp_err("unable to request gpio.(%s)\r\n",input_dev->name);
		goto err_init_drv;
	}
	gpio_direction_input(ist_gpio);

	client->irq = gpio_to_irq(ist_gpio);
	printk("[TSP] client->irq : %d\n", client->irq);
	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist30xx_ts", data);
	if (ret)
		goto err_irq;
/* I2C fail sometime due to high frequency */
ist30xx_disable_irq(data);
sprd_i2c_ctl_chg_clk(1, 400000);

	while (data->chip_id != IST30XXB_CHIP_ID) {
		ret = ist30xx_read_cmd(data->client, IST30XXB_REG_CHIPID, &data->chip_id);
		if (ret){
			tsp_warn("[TSP]reg chip id read fail!\n");
			ist30xx_reset();
			ret = ist30xx_read_cmd(data->client, IST30XXB_REG_CHIPID, &data->chip_id);
			if(ret)tsp_warn("[TSP]reg chip id read fail again!\n");
		}
		if (data->chip_id == 0x3000B)
			data->chip_id = IST30XXB_CHIP_ID;
		printk("TSP in probe : read  IST30XXB_REG_CHIPID end : data->chip_id : %x \n", data->chip_id);

		if (retry-- == 0)
			goto err_init_drv;
	}

	retry = 3;
	data->tsp_type = TSP_TYPE_UNKNOWN;
	while (retry-- > 0) {
		ret = ist30xx_read_cmd(data->client, IST30XXB_REG_TSPTYPE,
				       &data->tsp_type);
		if (ret) continue;

		tsp_info("tsptype: %x\n", data->tsp_type);
		data->tsp_type = IST30XXB_PARSE_TSPTYPE(data->tsp_type);

		if (ret == 0)
			break;
	}

	tsp_info("TSP IC: %x, TSP Vendor: %x\n", data->chip_id, data->tsp_type);

	data->status.event_mode = false;

ret = ist30xx_init_update_sysfs();
	if (ret)
		goto err_init_drv;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs();
	if (ret)
		goto err_init_drv;
#endif

#if IST30XX_CMCS_TEST
	ret = ist30xx_init_cmcs_sysfs();
	if(ret)
		goto err_init_drv;
#endif

#if SEC_FACTORY_MODE
	ret = sec_fac_cmd_init(data);
	if (ret)
		goto err_init_drv;
	ret = sec_touch_sysfs(data);
	if (ret)
		goto err_init_drv;
#endif

#if IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs();
	if (ret)
		goto err_init_drv;
#endif
	
	
#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&work_fw_update, fw_update_func);
	schedule_delayed_work(&work_fw_update, IST30XX_UPDATE_DELAY);
#else
	ret = ist30xx_auto_bin_update(data);
	if (ret < 0)
		goto err_irq;
#endif
#endif  // IST30XX_INTERNAL_BIN
#if IST30XX_REPORT_RATE
	data->ist30xx_report_rate = data->ist30xx_idle_rate = -1;
#endif
	if (ist30xx_ta_status < 0)
		ist30xx_ta_status = 0;

	ret = ist30xx_get_info(data);
	tsp_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));
	if (ret)
		goto err_irq;

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);

#if IST30XX_EVENT_MODE
	spin_lock_init(&data->timer_lock);
	spin_lock_irqsave(&data->timer_lock, flags);
	init_timer(&data->event_timer);
	data->event_timer.function = timer_handler;
	data->event_timer.expires = jiffies_64 + (EVENT_TIMER_INTERVAL);
	add_timer(&data->event_timer);
	spin_unlock_irqrestore(&data->timer_lock, flags);
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	ist30xx_initialized = 1;
	printk("[TSP] Probe end\n");
	return 0;

err_irq:
	tsp_debug("ChipID: %x\n", data->chip_id);
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
err_init_drv:
	data->status.event_mode = false;
	tsp_err("Error, ist30xx init driver\n");
	ist30xx_power_off();
	input_unregister_device(input_dev);
	return 0;

err_reg_dev:
err_alloc_dev:
	tsp_err("Error, ist30xx mem free\n");
	kfree(data);
	return 0;
}


static int ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	free_irq(client->irq, data);
	ist30xx_power_off();
	input_unregister_device(data->input_dev);
	kfree(data);
	return 0;
}


static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};


MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops ist30xx_pm_ops = {
	.suspend	= ist30xx_suspend,
	.resume		= ist30xx_resume,
};
#endif


static struct i2c_driver ist30xx_i2c_driver = {
	.id_table	= ist30xx_idtable,
	.probe		= ist30xx_probe,
	.remove		= ist30xx_remove,
	.shutdown	= ist30xx_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= IST30XX_DEV_NAME,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.pm	= &ist30xx_pm_ops,
#endif
	},
};


static int __init ist30xx_init(void)
{
	return i2c_add_driver(&ist30xx_i2c_driver);
}

static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");
