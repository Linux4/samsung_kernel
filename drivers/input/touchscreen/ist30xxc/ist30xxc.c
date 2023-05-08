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
#include <linux/input/mt.h>
#include <linux/pm_runtime.h>
#include <soc/sprd/i2c-sprd.h>

#ifdef CONFIG_EXTCON_S2MM001B
#include <linux/extcon.h>
#endif

#include "ist30xxc.h"
#include "ist30xxc_update.h"
#include "ist30xxc_tracking.h"
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#if IST30XX_DEBUG
#include "ist30xxc_misc.h"
#endif

#if IST30XX_CMCS_TEST
#include "ist30xxc_cmcs.h"
#endif

#if IST30XX_CMCS_JIT_TEST
#include "ist30xxc_cmcs_jit.h"
#endif

#include <linux/power_supply.h>

#include <asm/sec/sec_debug.h>

extern void (*tsp_charger_status_cb)(int);

#define TOUCH_BOOSTER	0

#if TOUCH_BOOSTER
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
struct cpufreq_limit_handle *min_handle = NULL;
static const unsigned long touch_cpufreq_lock = 1200000;
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
struct cpu_num_min_limit_handle *cpu_num_limit_handle = NULL;
#endif
#endif

#define MAX_ERR_CNT			(100)
#define EVENT_TIMER_INTERVAL		(HZ * timer_period_ms / 1000)
u32 event_ms = 0, timer_ms = 0;
static struct timer_list event_timer;
static struct timespec t_current;	// ns
int timer_period_ms = 500;		// 0.5sec

#if IST30XX_USE_KEY
int ist30xx_key_code[IST30XX_MAX_KEYS + 1] = { 0, KEY_RECENT, KEY_BACK };
#endif

struct ist30xx_data *ts_data;

DEFINE_MUTEX(ist30xx_mutex);

#ifdef CONFIG_ARM64
struct class *sec_class;
EXPORT_SYMBOL(sec_class);
#endif

int ist30xx_dbg_level = IST30XX_DEBUG_LEVEL;
void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (ist30xx_dbg_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", IST30XX_DEBUG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(void)
{
	ktime_get_ts(&t_current);

	return t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
}

int ist30xx_intr_wait(struct ist30xx_data *data, long ms)
{
	long start_ms = get_milli_second();
	long curr_ms = 0;

	while (true) {
		if (!data->irq_working)
			break;

		curr_ms = get_milli_second();
		if ((curr_ms < 0) || (start_ms < 0) || (curr_ms - start_ms > ms))
			return -EPERM;
		usleep_range(1500, 2500);
	}

	return 0;
}

void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (likely(data->irq_enabled)) {
		ist30xx_tracking(TRACK_INTR_DISABLE);
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
		data->status.event_mode = false;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (likely(!data->irq_enabled)) {
		ist30xx_tracking(TRACK_INTR_ENABLE);
		enable_irq(data->client->irq);
		msleep(10);
		data->irq_enabled = 1;
		data->status.event_mode = true;
	}
}

void ist30xx_scheduled_reset(struct ist30xx_data *data)
{
	if (likely(data->initialized))
		schedule_delayed_work(&data->work_reset_check, 0);
}

static void ist30xx_request_reset(struct ist30xx_data *data)
{
	data->irq_err_cnt++;
	if (unlikely(data->irq_err_cnt >= data->max_irq_err_cnt)) {
		ist30xx_scheduled_reset(data);
		data->irq_err_cnt = 0;
	}
}

#define NOISE_MODE_TA		(0)
#define NOISE_MODE_CALL		(1)
#define NOISE_MODE_COVER	(2)
#define NOISE_MODE_EDGE		(4)
#define NOISE_MODE_POWER	(8)
void ist30xx_start(struct ist30xx_data *data)
{
	if (data->initialized) {
		data->scan_count = 0;
		data->scan_retry = 0;
		mod_timer(&event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);
	}

	data->ignore_delay = true;

	ist30xx_write_cmd(data->client, IST30XX_HIB_CMD,
		((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));
	tsp_info("%s(), mode : 0x%x\n", __func__, data->noise_mode & 0xFFFF);

	if (data->report_rate >= 0) {
		ist30xx_write_cmd(data->client, IST30XX_HIB_CMD,
		((eHCOM_SET_TIME_ACTIVE << 16) | (data->report_rate & 0xFFFF)));
		tsp_info(" active rate : %dus\n", data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist30xx_write_cmd(data->client, IST30XX_HIB_CMD,
			((eHCOM_SET_TIME_IDLE << 16) | (data->idle_rate & 0xFFFF)));
		tsp_info(" idle rate : %dus\n", data->idle_rate);
	}

#if IST30XX_JIG_MODE
	if (data->jig_mode) {
		ist30xx_write_cmd(data->client, IST30XX_HIB_CMD,
			(eHCOM_SET_JIG_MODE << 16) | (IST30XX_JIG_TOUCH & 0xFFFF));
	}

	if (data->debug_mode || data->jig_mode) {
#else
	if (data->debug_mode) {
#endif
		ist30xx_write_cmd(data->client, IST30XX_HIB_CMD,
			(eHCOM_SLEEP_MODE_EN << 16) | (IST30XX_DISABLE & 0xFFFF));
	}

	ist30xx_cmd_start_scan(data);

	data->ignore_delay = false;
}


int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.prev.main_ver = data->fw.cur.main_ver;
	data->fw.prev.fw_ver = data->fw.cur.fw_ver;
	data->fw.prev.core_ver = data->fw.cur.core_ver;
	data->fw.prev.test_ver = data->fw.cur.test_ver;
	data->fw.cur.main_ver = 0;
	data->fw.cur.fw_ver = 0;
	data->fw.cur.core_ver = 0;
	data->fw.cur.test_ver = 0;

	ret = ist30xx_cmd_hold(data->client, 1);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_read_reg(data->client,
		IST30XX_DA_ADDR(eHCOM_GET_VER_MAIN), &data->fw.cur.main_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
		IST30XX_DA_ADDR(eHCOM_GET_VER_FW), &data->fw.cur.fw_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
		IST30XX_DA_ADDR(eHCOM_GET_VER_CORE), &data->fw.cur.core_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
		IST30XX_DA_ADDR(eHCOM_GET_VER_TEST), &data->fw.cur.test_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_cmd_hold(data->client, 0);
	if (unlikely(ret))
		goto err_get_ver;

	tsp_info("IC version main: %x, fw: %x, test: %x, core: %x\n",
		data->fw.cur.main_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver,
		data->fw.cur.core_ver);

	return 0;

err_get_ver:
	ist30xx_reset(data, false);

	return ret;
}

#define CALIB_MSG_MASK		(0xF0000FFF)
#define CALIB_MSG_VALID		(0x80000CAB)
#define TRACKING_INTR_VALID	(0x127EA597)
u32 tracking_intr_value = TRACKING_INTR_VALID;
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;
	u32 ms;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
	ist30xx_get_update_info(data, data->fw.buf, data->fw.buf_size);
#endif
	ist30xx_get_tsp_info(data);
#else
	ret = ist30xx_get_ver_info(data);
	if (unlikely(ret))
		goto get_info_end;

	ret = ist30xx_get_tsp_info(data);
	if (unlikely(ret))
		goto get_info_end;
#endif  /* IST30XX_INTERNAL_BIN */

	ist30xx_print_info(data);

	ret = ist30xx_read_cmd(data, eHCOM_GET_CAL_RESULT, &calib_msg);
	if (likely(!ret)) {
		tsp_info("calib status: 0x%08x\n", calib_msg);
		ms = get_milli_second();
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_value, 1);
		ist30xx_put_track(&calib_msg, 1);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID
						|| CALIB_TO_STATUS(calib_msg) > 0)
			ist30xx_calibrate(data, IST30XX_MAX_RETRY_CNT);
	}

#if IST30XX_CHECK_CALIB
	if (likely(!data->status.update)) {
		ret = ist30xx_cmd_check_calib(data->client);
		if (likely(!ret)) {
			data->status.calib = 1;
			data->status.calib_msg = 0;
			event_ms = (u32)get_milli_second();
			data->status.event_mode = true;
		}
	}
#else
	ist30xx_start(data);
#endif

#if !(IST30XX_INTERNAL_BIN)
get_info_end:
#endif

	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}

#if IST30XX_GESTURE
#define GESTURE_MAGIC_STRING		(0x4170CF00)
#define GESTURE_MAGIC_MASK		(0xFFFFFF00)
#define GESTURE_MESSAGE_MASK		(~GESTURE_MAGIC_MASK)
#define PARSE_GESTURE_MESSAGE(n) \
	((n & GESTURE_MAGIC_MASK) == GESTURE_MAGIC_STRING ? \
	(n & GESTURE_MESSAGE_MASK) : -EINVAL)
void ist30xx_gesture_cmd(struct ist30xx_data *data, int cmd)
{

	switch (cmd) {
	case 0x01:
		input_report_key(data->input_dev, KEY_VOLUMEUP, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_VOLUMEUP, 0);
		input_sync(data->input_dev);
	break;

	case 0x02:
		input_report_key(data->input_dev, KEY_VOLUMEDOWN, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_VOLUMEDOWN, 0);
		input_sync(data->input_dev);
	break;

	case 0x03:
		input_report_key(data->input_dev, KEY_PREVIOUSSONG, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_PREVIOUSSONG, 0);
		input_sync(data->input_dev);
	break;

	case 0x04:
		input_report_key(data->input_dev, KEY_NEXTSONG, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_NEXTSONG, 0);
		input_sync(data->input_dev);
	break;

	case 0x11:
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 0);
		input_sync(data->input_dev);
	break;

	case 0x12:
		input_report_key(data->input_dev, KEY_SCREENLOCK, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_SCREENLOCK, 0);
		input_sync(data->input_dev);
	break;

	case 0x13:
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 0);
		input_sync(data->input_dev);
	break;

	case 0x14:
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_PLAYPAUSE, 0);
		input_sync(data->input_dev);
	break;

	case 0x21:
		input_report_key(data->input_dev, KEY_MUTE, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_MUTE, 0);
		input_sync(data->input_dev);
	break;

	default:
		break;
	}
}
#endif

#define PRESS_MSG_MASK		(0x01)
#define MULTI_MSG_MASK		(0x02)
#define TOUCH_DOWN_MESSAGE	("p")
#define TOUCH_UP_MESSAGE	("r")
#define TOUCH_MOVE_MESSAGE	(" ")
bool tsp_touched[IST30XX_MAX_MT_FINGERS] = { false, };
void print_tsp_event(struct ist30xx_data *data, finger_info *finger)
{
#if TOUCH_BOOSTER
	static int fingers_cnt = 0;
#endif
	int idx = finger->bit_field.id - 1;
	bool press;

	press = PRESSED_FINGER(data->t_status, finger->bit_field.id);

	if (press) {
		if (tsp_touched[idx] == false) { /* touch down */
#if TOUCH_BOOSTER
			if(!min_handle)
			{
				min_handle = cpufreq_limit_min_freq(touch_cpufreq_lock, "TSP");
				if (IS_ERR(min_handle)) {
					printk(KERN_ERR "[TSP] cannot get cpufreq_min lock %lu(%d)\n",
						touch_cpufreq_lock, PTR_ERR(min_handle));
					min_handle = NULL;
				}
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
				cpu_num_limit_handle = _store_cpu_num_min_limit(2, "TSP");
				if (IS_ERR(cpu_num_limit_handle)) {
					printk(KERN_ERR "[TSP] cannot get cpuf_num_min limit %lu(%ld)\n",
						2, PTR_ERR(cpu_num_limit_handle));
					cpu_num_limit_handle = NULL;
				}
#endif
				tsp_info("cpu freq on\n");
			}
			fingers_cnt++;
#endif
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_noti("%s%d (%d, %d)\n",
				TOUCH_DOWN_MESSAGE, finger->bit_field.id,
				finger->bit_field.x, finger->bit_field.y);
#else
			tsp_noti("touch down\n");
#endif
			tsp_touched[idx] = true;
		} else { /* touch move */
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_debug("%s%d (%d, %d)\n",
				TOUCH_MOVE_MESSAGE, finger->bit_field.id,
				finger->bit_field.x, finger->bit_field.y);
#else
			tsp_debug("touch move\n");
#endif
		}
	} else {
		if (tsp_touched[idx] == true) { /* touch up */
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_noti("%s%d\n", TOUCH_UP_MESSAGE, finger->bit_field.id);
#else
			tsp_noti("touch up\n");
#endif
			tsp_touched[idx] = false;
#if TOUCH_BOOSTER
			fingers_cnt--;
			if(!fingers_cnt)
			{
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
				cpu_num_min_limit_put(cpu_num_limit_handle);
				cpu_num_limit_handle = NULL;
#endif
				cpufreq_limit_put(min_handle);
				min_handle = NULL;
				tsp_info("cpu freq off\n");
			}
#endif
		}
	}
}
#if IST30XX_USE_KEY
#define PRESS_MSG_KEY		(0x06)
bool tkey_pressed[IST30XX_MAX_KEYS] = { false, };
void print_tkey_event(struct ist30xx_data *data, int id)
{
	int idx = id - 1;
	bool press = PRESSED_KEY(data->t_status, id);

	if (press) {
		if (tkey_pressed[idx] == false) { /* tkey down */
			tsp_noti("k %s%d\n", TOUCH_DOWN_MESSAGE, id);
			tkey_pressed[idx] = true;
		}
	} else {
		if (tkey_pressed[idx] == true) { /* tkey up */
			tsp_noti("k %s%d\n", TOUCH_UP_MESSAGE, id);
			tkey_pressed[idx] = false;
		}
	}
}
#endif
static void release_finger(struct ist30xx_data *data, int id)
{
	input_mt_slot(data->input_dev, id - 1);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

	ist30xx_tracking(TRACK_POS_FINGER + id);

	tsp_touched[id - 1] = false;

	input_sync(data->input_dev);
}

#if IST30XX_USE_KEY
#define CANCEL_KEY		(0xFF)
#define RELEASE_KEY		(0)
static void release_key(struct ist30xx_data *data, int id, u8 key_status)
{
	input_report_key(data->input_dev, ist30xx_key_code[id], key_status);

	ist30xx_tracking(TRACK_POS_KEY + id);

	tkey_pressed[id - 1] = false;

	input_sync(data->input_dev);
}
#endif
static void clear_input_data(struct ist30xx_data *data)
{
	int id = 1;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_finger(data, id);
		status >>= 1;
		id++;
	}
#if IST30XX_USE_KEY
	id = 1;
	status = PARSE_KEY_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_key(data, id, RELEASE_KEY);
		status >>= 1;
		id++;
	}
#endif
	data->t_status = 0;
}

static int check_report_fingers(struct ist30xx_data *data, int finger_counts)
{
	int i;
	finger_info *fingers = (finger_info *)data->fingers;

	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if (unlikely((fingers[i].bit_field.x >= data->pdata->max_x) ||
			(fingers[i].bit_field.y >= data->pdata->max_y))) {
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

    return 0;
}

static int check_valid_coord(u32 *msg, int cnt)
{
	u8 *buf = (u8 *)msg;
	u8 chksum1 = msg[0] >> 24;
	u8 chksum2 = 0;
	u32 tmp = msg[0];

	msg[0] &= 0x00FFFFFF;

	cnt *= IST30XX_DATA_LEN;

	while (cnt--)
		chksum2 += *buf++;

	msg[0] = tmp;

	if (chksum1 != chksum2) {
		tsp_err("intr chksum: %02x, %02x\n", chksum1, chksum2);
		return -EPERM;
	}

	return 0;
}

static void report_input_data(struct ist30xx_data *data, int finger_counts,
						int key_counts)
{
	int id;
	bool press = false;
	finger_info *fingers = (finger_info *)data->fingers;
#if IST30XX_JIG_MODE
	u32 *z_values = (u32 *)data->z_values;
#endif
	int idx = 0;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	for (id = 0; id < IST30XX_MAX_MT_FINGERS; id++) {
		press = (status & (1 << id)) ? true : false;

		input_mt_slot(data->input_dev, id);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		fingers[idx].bit_field.id = id + 1;
		print_tsp_event(data, &fingers[idx]);

		if (press == false)
			continue;

		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					fingers[idx].bit_field.x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					fingers[idx].bit_field.y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					fingers[idx].bit_field.area);
#if IST30XX_JIG_MODE
		if (data->jig_mode)
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, z_values[idx]);
#endif
		idx++;
	}

#if IST30XX_USE_KEY
	status = PARSE_KEY_STATUS(data->t_status);
	for (id = 0; id < IST30XX_MAX_KEYS; id++) {
		press = (status & (1 << id)) ? true : false;

		input_report_key(data->input_dev, ist30xx_key_code[id + 1], press);

		print_tkey_event(data, id + 1);
	}
#endif

	data->irq_err_cnt = 0;
	data->scan_retry = 0;

	input_sync(data->input_dev);
}

/*
 * CMD : CMD_GET_COORD
 *
 *   1st  [31:24]   [23:21]   [20:16]   [15:12]   [11:10]   [9:0]
 *        Checksum  KeyCnt    KeyStatus FingerCnt Rsvd.     FingerStatus
 *   2nd  [31:28]   [27:24]   [23:12]   [11:0]
 *        ID        Area      X         Y
 */
#define TRACKING_INTR_DEBUG1_VALID	(0x127A6E81)
#define TRACKING_INTR_DEBUG2_VALID	(0x127A6E82)
#define TRACKING_INTR_DEBUG3_VALID	(0x127A6E83)
u32 tracking_intr_debug_value = 0;
u32 intr_debug_addr, intr_debug2_addr, intr_debug3_addr = 0;
u32 intr_debug_size, intr_debug2_size, intr_debug3_size = 0;
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = (struct ist30xx_data *)ptr;
	int offset = 1;
#if IST30XX_STATUS_DEBUG
	u32 touch_status;
#endif
	u32 t_status;
	u32 msg[IST30XX_MAX_MT_FINGERS * 2 + offset];
	u32 ms;

	data->irq_working = true;

	if (unlikely(!data->irq_enabled))
		goto irq_end;

	ms = get_milli_second();

	if (intr_debug_size > 0) {
		tsp_noti("Intr_debug (addr: 0x%08x)\n", intr_debug_addr);
		ist30xx_burst_read(data->client, IST30XX_DA_ADDR(intr_debug_addr),
			&msg[0], intr_debug_size, true);

		for (i = 0; i < intr_debug_size; i++) {
			tsp_noti("\t%08x\n", msg[i]);
		}

		tracking_intr_debug_value = TRACKING_INTR_DEBUG1_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug_size);
	}

	if (intr_debug2_size > 0) {
		tsp_noti("Intr_debug2 (addr: 0x%08x)\n", intr_debug2_addr);
		ist30xx_burst_read(data->client, IST30XX_DA_ADDR(intr_debug2_addr),
			&msg[0], intr_debug2_size, true);

		for (i = 0; i < intr_debug2_size; i++) {
			tsp_noti("\t%08x\n", msg[i]);
		}

		tracking_intr_debug_value = TRACKING_INTR_DEBUG2_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug2_size);
	}

	memset(msg, 0, sizeof(msg));

	ret = ist30xx_read_reg(data->client, IST30XX_HIB_INTR_MSG, msg);
	if (unlikely(ret))
		goto irq_err;

	// TSP IC Exception
	if (unlikely((*msg & IST30XX_EXCEPT_MASK) == IST30XX_EXCEPT_VALUE)) {
		tsp_err("Occured IC exception(0x%02X)\n", *msg & 0xFF);
#if I2C_BURST_MODE
		ret = ist30xx_burst_read(data->client,
			IST30XX_HIB_COORD, &msg[offset], IST30XX_MAX_EXCEPT_SIZE, true);
#else
		for (i = 0; i < IST30XX_MAX_EXCEPT_SIZE; i++) {
			ret = ist30xx_read_reg(data->client,
				IST30XX_HIB_COORD + (i * 4), &msg[i + offset]);
		}
#endif
		if (unlikely(ret))
			tsp_err(" exception value read error(%d)\n", ret);
		else
			tsp_err(" exception value : 0x%08X, 0x%08X\n", msg[1], msg[2]);

		goto irq_ic_err;
	}

	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF))  /* Unknown CMD */
		goto irq_err;

	event_ms = ms;
	ist30xx_put_track_ms(event_ms);
	ist30xx_put_track(&tracking_intr_value, 1);
	ist30xx_put_track(msg, 1);

	if (unlikely((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID)) {
		data->status.calib_msg = *msg;
		tsp_info("calib status: 0x%08x\n", data->status.calib_msg);

		goto irq_end;
	}

#if IST30XX_CMCS_TEST
	if (unlikely(*msg == IST30XX_CMCS_MSG_VALID)) {
		data->status.cmcs = 1;
		tsp_info("cmcs status: 0x%08x\n", *msg);

		goto irq_end;
	}
#endif

#if IST30XX_CMCS_JIT_TEST
	if (((*msg & CMCS_MSG_MASK) == CM_MSG_VALID) || ((*msg & CMCS_MSG_MASK) == CS_MSG_VALID)) {
		data->status.cmcs = *msg;
		tsp_info("cmcs status: 0x%08x\n", *msg);

		goto irq_end;
	}
#endif

#if IST30XX_GESTURE
	ret = PARSE_GESTURE_MESSAGE(*msg);
	if (unlikely(ret > 0)) {
		tsp_info("Gesture ID: %d (0x%08x)\n", ret, *msg);
		ist30xx_gesture_cmd(data, ret);

		goto irq_end;
	}
#endif

#if IST30XX_STATUS_DEBUG
	ret = ist30xx_read_reg(data->client,
		IST30XX_HIB_TOUCH_STATUS, &touch_status);

	if (ret == 0) {
		tsp_debug("ALG : 0x%08X\n", touch_status);
	}

	memset(data->fingers, 0, sizeof(data->fingers));
#endif

	/* Unknown interrupt data for extend coordinate */
	if (unlikely(!CHECK_INTR_STATUS(*msg)))
		goto irq_err;

	t_status = *msg;
	key_cnt = PARSE_KEY_CNT(t_status);
	finger_cnt = PARSE_FINGER_CNT(t_status);

	if (unlikely((finger_cnt > data->max_fingers) ||
			(key_cnt > data->max_keys))) {
		tsp_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
			 finger_cnt, data->max_fingers, key_cnt, data->max_keys);
		goto irq_err;
	}

	if (finger_cnt > 0) {
#if I2C_BURST_MODE
		ret = ist30xx_burst_read(data->client,
			IST30XX_HIB_COORD, &msg[offset], finger_cnt, true);
		if (unlikely(ret))
			goto irq_err;

		for (i = 0; i < finger_cnt; i++)
			data->fingers[i].full_field = msg[i + offset];
#else
		for (i = 0; i < finger_cnt; i++) {
			ret = ist30xx_read_reg(data->client,
				IST30XX_HIB_COORD + (i * 4), &msg[i + offset]);
			if (unlikely(ret))
				goto irq_err;

			data->fingers[i].full_field = msg[i + offset];
		}
#endif

#if IST30XX_JIG_MODE
		if (data->jig_mode) {
			ret = ist30xx_burst_read(data->client,
				IST30XX_DA_ADDR(data->tags.zvalue_base),
				data->z_values, finger_cnt, true);
		}
#endif
		ist30xx_put_track(msg + offset, finger_cnt);
		for (i = 0; i < finger_cnt; i++) {
#if IST30XX_JIG_MODE
			if (data->jig_mode)
				tsp_verb("intr msg(%d): 0x%08x, %d\n",
					i + offset, msg[i + offset], data->z_values[i]);
			else
#endif
				tsp_verb("intr msg(%d): 0x%08x\n", i + offset, msg[i + offset]);
		}
	}

	read_cnt = finger_cnt + 1;

	ret = check_valid_coord(&msg[0], read_cnt);
	if (unlikely(ret))
		goto irq_err;

	ret = check_report_fingers(data, finger_cnt);
	if (unlikely(ret))
		goto irq_err;

	data->t_status = t_status;
	report_input_data(data, finger_cnt, key_cnt);

	if (intr_debug3_size > 0) {
		tsp_noti("Intr_debug3 (addr: 0x%08x)\n", intr_debug3_addr);
		ist30xx_burst_read(data->client, IST30XX_DA_ADDR(intr_debug3_addr),
			&msg[0], intr_debug3_size, true);

		for (i = 0; i < intr_debug3_size; i++)
			tsp_noti("\t%08x\n", msg[i]);

		tracking_intr_debug_value = TRACKING_INTR_DEBUG3_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug3_size);
	}

	goto irq_end;

irq_err:
	tsp_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xx_request_reset(data);
irq_end:
	data->irq_working = false;
	event_ms = (u32)get_milli_second();
	return IRQ_HANDLED;

irq_ic_err:
	ist30xx_scheduled_reset(data);
	data->irq_working = false;
	event_ms = (u32)get_milli_second();
	return IRQ_HANDLED;
}

static int ist30xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	del_timer(&event_timer);
	cancel_delayed_work_sync(&data->work_noise_protect);
	cancel_delayed_work_sync(&data->work_reset_check);
	cancel_delayed_work_sync(&data->work_debug_algorithm);
	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
#if IST30XX_GESTURE
	if (data->gesture) {
		ist30xx_start(data);
		data->status.noise_mode = false;
		ist30xx_enable_irq(data);
	}
#endif
#if TOUCH_BOOSTER
	if (min_handle) {
		printk(KERN_ERR"[TSP] %s %d:: OOPs, Cpu was not in Normal Freq..\n",
									__func__, __LINE__);

		if (cpufreq_limit_put(min_handle) < 0)
			printk(KERN_ERR "[TSP] Error in scaling down cpu frequency\n");

		min_handle = NULL;
		tsp_info("cpu freq off\n");
	}
#endif
	mutex_unlock(&ist30xx_mutex);

	return 0;
}

static int ist30xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	data->noise_mode |= (1 << NOISE_MODE_POWER);

	mutex_lock(&ist30xx_mutex);
	ist30xx_internal_resume(data);
	ist30xx_start(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ist30xx_early_suspend(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	ist30xx_suspend(&data->client->dev);
}
static void ist30xx_late_resume(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	ist30xx_resume(&data->client->dev);
}
#endif

void ist30xx_set_ta_mode(int status)
{
	if (ts_data->initialized) {
		if (status == POWER_SUPPLY_TYPE_BATTERY || status == POWER_SUPPLY_TYPE_UNKNOWN)
			status = 0;
		else
			status = 1;

		if (unlikely(status == ((ts_data->noise_mode >> NOISE_MODE_TA) & 1)))
			return;

		if (status)
			ts_data->noise_mode |= (1 << NOISE_MODE_TA);
		else
			ts_data->noise_mode &= ~(1 << NOISE_MODE_TA);

#if IST30XX_TA_RESET
		if (unlikely(ts_data->initialized))
			ist30xx_scheduled_reset(ts_data);
#else
		ist30xx_write_cmd(ts_data->client, IST30XX_HIB_CMD,
			((eHCOM_SET_MODE_SPECIAL << 16) | (ts_data->noise_mode & 0xFFFF)));
#endif

		ist30xx_tracking(status ? TRACK_CMD_TACON : TRACK_CMD_TADISCON);
	}
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_call_mode(int mode)
{
	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_CALL) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_CALL);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_CALL);

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data->client, IST30XX_HIB_CMD,
		((eHCOM_SET_MODE_SPECIAL << 16) | (ts_data->noise_mode & 0xFFFF)));
#endif

	ist30xx_tracking(mode ? TRACK_CMD_CALL : TRACK_CMD_NOTCALL);
}
EXPORT_SYMBOL(ist30xx_set_call_mode);

void ist30xx_set_cover_mode(int mode)
{
	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_COVER) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_COVER);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_COVER);

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data->client, IST30XX_HIB_CMD,
		((eHCOM_SET_MODE_SPECIAL << 16) | (ts_data->noise_mode & 0xFFFF)));
#endif

	ist30xx_tracking(mode ? TRACK_CMD_COVER : TRACK_CMD_NOTCOVER);
}
EXPORT_SYMBOL(ist30xx_set_cover_mode);

void ist30xx_set_edge_mode(int mode)
{
	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_EDGE);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_EDGE);

	ist30xx_write_cmd(ts_data->client, IST30XX_HIB_CMD,
		((eHCOM_SET_MODE_SPECIAL << 16) | (ts_data->noise_mode & 0xFFFF)));
}
EXPORT_SYMBOL(ist30xx_set_edge_mode);

static void reset_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
	struct ist30xx_data, work_reset_check);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	if (likely((data->initialized == 1) && (data->status.power == 1) &&
		(data->status.update != 1) && (data->status.calib != 1))) {
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(data);
#if IST30XX_GESTURE
		if (data->suspend)
			ist30xx_internal_suspend(data);
		else
#endif
			ist30xx_reset(data, false);

		clear_input_data(data);
		ist30xx_start(data);
#if IST30XX_GESTURE
		if (data->gesture && data->suspend)
			data->status.noise_mode = false;
#endif
		ist30xx_enable_irq(data);
		mutex_unlock(&ist30xx_mutex);
	}
}

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
					struct ist30xx_data, work_fw_update);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	if (likely(ist30xx_auto_bin_update(data)))
		ist30xx_disable_irq(data);
}
#endif // IST30XX_UPDATE_BY_WORKQUEUE
#endif // IST30XX_INTERNAL_BIN

#define TOUCH_STATUS_MAGIC	(0x00000075)
#define TOUCH_STATUS_MASK	(0x000000FF)
#define FINGER_ENABLE_MASK	(0x00100000)
#define SCAN_CNT_MASK		(0xFFE00000)
#define GET_FINGER_ENABLE(n)	((n & FINGER_ENABLE_MASK) >> 20)
#define GET_SCAN_CNT(n)		((n & SCAN_CNT_MASK) >> 21)
u32 ist30xx_algr_addr;
u32 ist30xx_algr_size;
static void noise_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status;
	u32 scan_count;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
				struct ist30xx_data, work_noise_protect);

	ret = ist30xx_read_reg(data->client,
			IST30XX_HIB_TOUCH_STATUS, &touch_status);
	if (unlikely(ret))
		goto retry_timer;

	ist30xx_put_track_ms(timer_ms);
	ist30xx_put_track(&touch_status, 1);

	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	/* Check valid scan count */
	if (unlikely((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC)) {
		tsp_warn("Touch status is not corrected! (0x%08x)\n", touch_status);
		goto retry_timer;
	}

	/* Status of IC is idle */
	if (GET_FINGER_ENABLE(touch_status) == 0) {
		if ((PARSE_FINGER_CNT(data->t_status) > 0) ||
			(PARSE_KEY_CNT(data->t_status) > 0))
			clear_input_data(data);
	}

	scan_count = GET_SCAN_CNT(touch_status);

	/* Status of IC is lock-up */
	if (unlikely(scan_count == data->scan_count)) {
		tsp_warn("TSP IC is not responded! (0x%08x)\n", scan_count);
		goto retry_timer;
	}

	data->scan_retry = 0;
	data->scan_count = scan_count;
	return;

retry_timer:
	data->scan_retry++;
	tsp_warn("Retry touch status!(%d)\n", data->scan_retry);

	if (unlikely(data->scan_retry == data->max_scan_retry)) {
		ist30xx_scheduled_reset(data);
		data->scan_retry = 0;
	}
}

static void debug_work_func(struct work_struct *work)
{
#if IST30XX_ALGORITHM_MODE
	int ret = -EPERM;
	int i;
	u32 *buf32;

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
	struct ist30xx_data, work_debug_algorithm);

	buf32 = kzalloc(ist30xx_algr_size, GFP_KERNEL);
	if (!buf32) {
		tsp_warn("Failed to allocate buffer\n");
		return;
	}

	for (i = 0; i < ist30xx_algr_size; i++) {
		ret = ist30xx_read_buf(data->client,
		ist30xx_algr_addr + IST30XX_DATA_LEN * i, &buf32[i], 1);
		if (ret) {
			tsp_warn("Algorithm mem addr read fail!\n");
			return;
		}
	}

	ist30xx_put_track(buf32, ist30xx_algr_size);

	tsp_debug(" 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			buf32[0], buf32[1], buf32[2], buf32[3], buf32[4]);
	kfree(buf32);
#endif
}

void timer_handler(unsigned long timer_data)
{
	struct ist30xx_data *data = (struct ist30xx_data *)timer_data;
	struct ist30xx_status *status = &data->status;

	if (data->irq_working)
		goto restart_timer;

	if (status->event_mode) {
		if (likely((status->power == 1) && (status->update != 1))) {
			timer_ms = (u32)get_milli_second();
			if (unlikely(status->calib == 1)) { /* Check calibration */
				if ((status->calib_msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
					tsp_info("Calibration check OK!!\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				} else if (timer_ms - event_ms >= 3000) { /* 3s */
					tsp_info("calibration timeout over 3sec\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				}
			} else if (likely(status->noise_mode)) {
				if (timer_ms - event_ms > 100) /* 100ms after last interrupt */
					schedule_delayed_work(&data->work_noise_protect, 0);
			}

#if IST30XX_ALGORITHM_MODE
			if ((ist30xx_algr_addr >= IST30XX_DIRECT_ACCESS) &&
				(ist30xx_algr_size > 0)) {
				if (timer_ms - event_ms > 100) /* 100ms after last interrupt */
					schedule_delayed_work(&data->work_debug_algorithm, 0);
			}
#endif
		}
	}

restart_timer:
	mod_timer(&event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}

extern struct class *ist30xx_class;
#if SEC_FACTORY_MODE
extern int sec_touch_sysfs(struct ist30xx_data *data);
extern int sec_fac_cmd_init(struct ist30xx_data *data);
#endif

static struct ist30xx_platform_data *ist30xx_parse_dt(struct device *dev)
{
	struct ist30xx_platform_data *pdata;
	struct device_node *np = dev->of_node;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(!pdata)) {
		dev_err(dev, "Failed to allocate platform data\n");
		return NULL;
	}

	if (of_property_read_u32(np, "imagis,max-x", &pdata->max_x)) {
		tsp_err("failed to get max_x\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,max-y", &pdata->max_y)) {
		tsp_err("failed to get max_y\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,max-w", &pdata->max_w)) {
		tsp_err("failed to get max_w\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,avdd-volt", &pdata->avdd_volt)) {
		tsp_err("failed to get avdd_volt\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,chip-id", &pdata->chip_id)) {
		tsp_err("failed to get chip_id\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,max-node", &pdata->max_node)) {
		tsp_err("failed to get max_node\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,chip-code", &pdata->chip_code)) {
		tsp_err("failed to get chip code\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,tkey", &pdata->tkey)) {
		tsp_err("failed to get tkey\n");
		goto out;
	}

	if (of_property_read_u32(np, "imagis,octa-hw", &pdata->octa_hw)) {
		tsp_err("failed to get octa hw\n");
		goto out;
	}

	if (of_property_read_string(np, "imagis,chip-name", &pdata->chip_name)) {
		tsp_err("failed to get chip name\n");
		goto out;
	}

	tsp_debug("##### Device tree #####\n");
	tsp_debug("avdd volt: %d\n", pdata->avdd_volt);
	tsp_debug("chip name: %s\n", pdata->chip_name);
	tsp_debug("chip id: 0x%X\n", pdata->chip_id);
	tsp_debug("chip code: %d\n", pdata->chip_code);
	tsp_debug("max x: %d\n", pdata->max_x);
	tsp_debug("max y: %d\n", pdata->max_y);
	tsp_debug("max w: %d\n", pdata->max_w);
	tsp_debug("max node: %d\n", pdata->max_node);
	tsp_debug("touch key: %d\n", pdata->tkey);
	tsp_debug("octa hw: %d\n", pdata->octa_hw);

	return pdata;

out:
	dev_err(dev, "Failed to get property\n");

	return NULL;
}

static int ist30xx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int retry = 3;
#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;
	struct ist30xx_platform_data *pdata;
#endif
	struct ist30xx_data *data;
	struct input_dev *input_dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		return -EIO;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = ist30xx_parse_dt(&client->dev);
	if (unlikely(!pdata)) {
		dev_err(&client->dev, "Failed to obtain platform data\n");
		kfree(data);
		return -EINVAL;
	}
#endif

	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		tsp_err("input_allocate_device failed\n");
		goto err_alloc_dev;
	}

	data->pdata = pdata;
	data->max_fingers = IST30XX_MAX_MT_FINGERS;
	data->max_keys = IST30XX_MAX_KEYS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS, 0);

	input_dev->name = "sec_touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(data->input_dev, ABS_MT_POSITION_X,
			0, data->pdata->max_x - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_Y,
			0, data->pdata->max_y - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			0, data->pdata->max_w, 0, 0);
#if IST30XX_JIG_MODE
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0, 0, 0);
#endif

#if IST30XX_USE_KEY
	{
		int i;
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(EV_SYN, input_dev->evbit);
		for (i = 1; i < ARRAY_SIZE(ist30xx_key_code); i++)
			set_bit(ist30xx_key_code[i], input_dev->keybit);
	}

#if IST30XX_GESTURE
	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, KEY_PLAYPAUSE);
	input_set_capability(input_dev, EV_KEY, KEY_NEXTSONG);
	input_set_capability(input_dev, EV_KEY, KEY_PREVIOUSSONG);
	input_set_capability(input_dev, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(input_dev, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(input_dev, EV_KEY, KEY_MUTE);
#endif
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (unlikely(ret)) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	ts_data = data;

	ret = ist30xx_init_system(data);
	if (unlikely(ret)) {
		tsp_err("chip initialization failed\n");
		goto err_init_drv;
	}

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist30xx_ts", data);
	if (unlikely(ret))
		goto err_init_drv;

	ist30xx_disable_irq(data);
	sprd_i2c_ctl_chg_clk(1, 400000);	// up h/w i2c 1 400k

	while (retry-- > 0) {
		ret = ist30xx_read_cmd(data, IST30XX_REG_CHIPID, &data->chip_id);
		data->chip_id &= 0xFFFF;
		if (unlikely(!ret)) {
			if ((data->chip_id == data->pdata->chip_id) ||
				(data->chip_id == IST30XXC_DEFAULT_CHIP_ID) ||
				(data->chip_id == IST3048C_DEFAULT_CHIP_ID)) {
				break;
			}
		} else if (unlikely(!retry))
			goto err_irq;

		ist30xx_reset(data, false);
	}

	if (pdata->chip_code < IMAGIS_IST3038C) {
		retry = 3;
		data->tsp_type = TSP_TYPE_UNKNOWN;
		while (retry-- > 0) {
			ret = ist30xx_read_cmd(data, IST30XX_REG_TSPTYPE, &data->tsp_type);
			if (likely(!ret)) {
				data->tsp_type = IST30XX_PARSE_TSPTYPE(data->tsp_type);
				tsp_info("tsptype: %x\n", data->tsp_type);
				break;
			}

			if (unlikely(!retry))
				goto err_irq;
		}
		tsp_info("TSP IC: %x, TSP Vendor: %x\n", data->chip_id, data->tsp_type);
	} else
		tsp_info("TSP IC: %x\n", data->chip_id);

	data->status.event_mode = false;

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&data->work_fw_update, fw_update_func);
	schedule_delayed_work(&data->work_fw_update, IST30XX_UPDATE_DELAY);
#else
	ret = ist30xx_auto_bin_update(data);
	if (unlikely(ret != 0))
		goto err_irq;
#endif
#endif

	ret = ist30xx_init_update_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if IST30XX_CMCS_TEST
	ret = ist30xx_init_cmcs_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if IST30XX_CMCS_JIT_TEST
	ret = ist30xx_init_cmcs_jit_sysfs(data);
	if (unlikely(ret)) {
		tsp_err("%s: do not init cmcs jitter\n",__func__);
		goto err_sysfs;
	}
#endif

#if IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs();
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if SEC_FACTORY_MODE
	ret = sec_fac_cmd_init(data);
	if (unlikely(ret))
		goto err_sysfs;

	ret = sec_touch_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if IST30XX_GESTURE
	data->suspend = false;
	data->gesture = false;
#endif
	data->irq_working = false;
	data->max_scan_retry = 2;
	data->max_irq_err_cnt = MAX_ERR_CNT;
	data->report_rate = -1;
	data->idle_rate = -1;

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);

	init_timer(&event_timer);
	event_timer.data = (unsigned long)data;
	event_timer.function = timer_handler;
	event_timer.expires = jiffies_64 + (EVENT_TIMER_INTERVAL);
	mod_timer(&event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);

	ret = ist30xx_get_info(data);
	tsp_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));

	data->initialized = true;

	pm_runtime_enable(&client->dev);

	tsp_charger_status_cb = ist30xx_set_ta_mode;

	tsp_info("### IMAGIS probe success ###\n");

	return 0;

err_sysfs:
	class_destroy(ist30xx_class);
err_irq:
	tsp_info("ChipID: %x\n", data->chip_id);
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
err_init_drv:
	data->status.event_mode = false;
	ist30xx_power_off(data);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	input_unregister_device(input_dev);
err_reg_dev:
err_alloc_dev:
	kfree(data);
	tsp_err("Error, ist30xx init driver\n");
	return -ENODEV;
}


static int ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
	ist30xx_power_off(data);

	input_unregister_device(data->input_dev);
	tsp_charger_status_cb = NULL;
	kfree(data);

	return 0;
}

static void ist30xx_shutdown(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

	del_timer(&event_timer);
	cancel_delayed_work_sync(&data->work_noise_protect);
	cancel_delayed_work_sync(&data->work_reset_check);
	cancel_delayed_work_sync(&data->work_debug_algorithm);
	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
	mutex_unlock(&ist30xx_mutex);
}

static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_OF
static struct of_device_id ist30xx_match_table[] = {
	{ .compatible = "imagis,ist30xxc", },
	{ },
};
#else
#define ist30xx_match_table NULL
#endif

#ifndef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops ist30xx_pm_ops = {
	SET_RUNTIME_PM_OPS(ist30xx_suspend, ist30xx_resume, NULL)
};
#endif

static struct i2c_driver ist30xx_i2c_driver = {
	.id_table		= ist30xx_idtable,
	.probe		= ist30xx_probe,
	.remove		= ist30xx_remove,
	.shutdown		= ist30xx_shutdown,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= IST30XX_DEV_NAME,
		.of_match_table	= ist30xx_match_table,
#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm			= &ist30xx_pm_ops,
#endif
	},
};

extern unsigned int lpcharge;

static int __init ist30xx_init(void)
{
#ifdef CONFIG_ARM64
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		printk("Failed create class \n");
		return PTR_ERR(sec_class);
	}
#endif
	if (!lpcharge)
		return i2c_add_driver(&ist30xx_i2c_driver);
	else
		return 0;
}


static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

#ifdef CONFIG_EXTCON_S2MM001B
static int extcon_notifier(struct notifier_block *nb,
	unsigned long state, void *edev)
{
	ist30xx_set_ta_mode(extcon_get_state(edev));

	return NOTIFY_OK;
}

static int ta_init_detect(void)
{
	struct extcon_dev *edev = NULL;
	static struct notifier_block nb;

	if (!ts_data)
		return -EPERM;

	edev = extcon_get_extcon_dev("s2mm001b");
	if (!edev) {
		pr_err("Failed to get extcon dev\n");
		return -ENXIO;
	}

	nb.notifier_call = extcon_notifier;

	extcon_register_notifier(edev, &nb);

	return 0;
}
late_initcall(ta_init_detect);
#endif

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");
