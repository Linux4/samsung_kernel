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
#include <linux/power_supply.h>

#include <mach/gpio.h>
#include <mach/i2c-sprd.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_tracking.h"

#if IST30XXC_DEBUG
#include "ist30xx_misc.h"
#endif

#if IST30XXC_CMCS_TEST
#include "ist30xx_cmcs.h"
#endif

#define TOUCH_BOOSTER	1

#if TOUCH_BOOSTER
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
extern int _store_cpu_num_min_limit(unsigned int input);
struct cpufreq_limit_handle *min_handles = NULL;
static const unsigned long ts_cpufreq_lock = 1200000;
#endif

extern void (*tsp_charger_status_cb)(int);
u32 ist30xxc_event_ms = 0, ist30xxc_timer_ms = 0;
static struct timer_list ist30xxc_event_timer;
static struct timespec t_currents;               // ns
int timers_period_ms = 500;                      // 0.5sec
#define EVENT_TIMER_INTERVALS     	(HZ * timers_period_ms / 1000)
#define MAX_ERRS_CNT                (100)

#if IST30XXC_USE_KEY
int ist30xxc_key_code[] = { 0, KEY_RECENT, KEY_BACK };
#endif

struct ist30xxc_data *ts_datas;

DEFINE_MUTEX(ist30xxc_mutex);

int ist30xxc_dbg_level = IST30XXC_DEBUG_LEVEL;
void ts_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (ist30xxc_dbg_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", IST30XXC_DEBUG_TAG, &vaf);

	va_end(args);
}

long get_millisecond(void)
{
	ktime_get_ts(&t_currents);

	return t_currents.tv_sec * 1000 + t_currents.tv_nsec / 1000000;
}

int ist30xxc_intr_wait(struct ist30xxc_data *data, long ms)
{
	long start_ms = get_millisecond();
	long curr_ms = 0;

	while (1) {
		if (!data->irq_working)
			break;

		curr_ms = get_millisecond();
		if ((curr_ms < 0) || (start_ms < 0) || (curr_ms - start_ms > ms)) {
			ts_info("%s() timeout(%dms)\n", __func__, ms);
			return -EPERM;
		}

		msleep(2);
	}
	return 0;
}

void ist30xxc_disable_irq(struct ist30xxc_data *data)
{
	if (likely(data->irq_enabled)) {
		ist30xxc_tracking(TRACK_INTR_DISABLE);
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
		data->status.event_mode = false;
	}
}

void ist30xxc_enable_irq(struct ist30xxc_data *data)
{
	if (likely(!data->irq_enabled)) {
		ist30xxc_tracking(TRACK_INTR_ENABLE);
		enable_irq(data->client->irq);
		msleep(10);
		data->irq_enabled = 1;
		data->status.event_mode = true;
	}
}

void ist30xxc_scheduled_reset(struct ist30xxc_data *data)
{
	if (likely(data->initialized))
		schedule_delayed_work(&data->work_reset_check, 0);
}

static void ist30xxc_request_reset(struct ist30xxc_data *data)
{
	data->irq_err_cnt++;
	if (unlikely(data->irq_err_cnt >= data->max_irq_err_cnt)) {
		ts_info("%s()\n", __func__);
		ist30xxc_scheduled_reset(data);
		data->irq_err_cnt = 0;
	}
}

#define NOISE_MODE_TA       (0)
#define NOISE_MODE_CALL     (1)
#define NOISE_MODE_COVER    (2)
#define NOISE_MODE_POWER  	(8)
void ist30xxc_start(struct ist30xxc_data *data)
{
	if (data->initialized) {
        data->scan_count = 0;
        data->scan_retry = 0;
		mod_timer(&ist30xxc_event_timer, 
                get_jiffies_64() + EVENT_TIMER_INTERVALS * 2);
	}

    ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));

    ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_LOCAL_MODEL << 16) | (TS_LOCAL_CODE & 0xFFFF)));

	ts_info("%s(), local : %d, mode : 0x%x\n", __func__,
		 TS_LOCAL_CODE & 0xFFFF, data->noise_mode & 0xFFFF);

	if (data->report_rate >= 0) {
		ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_TIME_ACTIVE << 16) | (data->report_rate & 0xFFFF)));
		ts_info(" active rate : %dus\n", data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_TIME_IDLE << 16) | (data->idle_rate & 0xFFFF)));
		ts_info(" idle rate : %dus\n", data->idle_rate);
	}

#if IST30XXC_JIG_MODE
    if (data->jig_mode) {
        ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
               (eHCOM_SET_JIG_MODE << 16) | (IST30XXC_JIG_TOUCH & 0xFFFF));
        ts_info(" jig mode start\n");
    }

    if (data->debug_mode || data->jig_mode) {
#else
    if (data->debug_mode) {
#endif        
        ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
               (eHCOM_SLEEP_MODE_EN << 16) | (IST30XXC_DISABLE & 0xFFFF));
        ts_info(" debug mode start\n");
    }

	ist30xxc_cmd_start_scan(data);
}


int ist30xxc_get_ver_info(struct ist30xxc_data *data)
{
	int ret;

	data->fw.prev.core_ver = data->fw.cur.core_ver;
	data->fw.prev.fw_ver = data->fw.cur.fw_ver;
    data->fw.prev.project_ver = data->fw.cur.project_ver;
    data->fw.prev.test_ver = data->fw.cur.test_ver;
	data->fw.cur.core_ver = 0;
    data->fw.cur.fw_ver = 0;
    data->fw.cur.project_ver = 0;
    data->fw.cur.test_ver = 0;
    
    ret = ist30xxc_cmd_hold(data->client, 1);
    if (unlikely(ret))
		return ret;

   	ret = ist30xxc_read_reg(data->client, 
            IST30XXC_DA_ADDR(eHCOM_GET_VER_CORE), &data->fw.cur.core_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xxc_read_reg(data->client, 
            IST30XXC_DA_ADDR(eHCOM_GET_VER_FW), &data->fw.cur.fw_ver);
	if (unlikely(ret))
		goto err_get_ver;

    ret = ist30xxc_read_reg(data->client, 
            IST30XXC_DA_ADDR(eHCOM_GET_VER_PROJECT), &data->fw.cur.project_ver);
	if (unlikely(ret))
		goto err_get_ver;

    ret = ist30xxc_read_reg(data->client, 
            IST30XXC_DA_ADDR(eHCOM_GET_VER_TEST), &data->fw.cur.test_ver);
	if (unlikely(ret))
		goto err_get_ver;

    ret = ist30xxc_cmd_hold(data->client, 0);
    if (unlikely(ret))
		goto err_get_ver;

   	ts_info("IC version core: %x, fw: %x, test: %x, project: %x\n",
		 data->fw.cur.core_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver, 
         data->fw.cur.project_ver);

	return 0;

err_get_ver:
    ist30xxc_reset(data, false);

    return ret;
}

#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)
#define TRACKING_INTR_VALID     (0x127EA597)
u32 tracking_intr_value = TRACKING_INTR_VALID;
int ist30xxc_get_info(struct ist30xxc_data *data)
{
	int ret;
	u32 calib_msg;
	u32 ms;

	mutex_lock(&ist30xxc_mutex);
	ist30xxc_disable_irq(data);

#if IST30XXC_INTERNAL_BIN
#if IST30XXC_UPDATE_BY_WORKQUEUE
	ist30xxc_get_update_info(data, data->fw.buf, data->fw.buf_size);
#endif
	ist30xxc_get_tsp_info(data);
#else
	ret = ist30xxc_get_ver_info(data);
	if (unlikely(ret))
		goto get_info_end;

	ret = ist30xxc_get_tsp_info(data);
	if (unlikely(ret))
		goto get_info_end;
#endif  // IST30XXC_INTERNAL_BIN

	ist30xxc_print_info(data);    

  	ret = ist30xxc_read_cmd(data, eHCOM_GET_CAL_RESULT, &calib_msg);
    if (likely(ret == 0)) {
		ts_info("calib status: 0x%08x\n", calib_msg);
		ms = get_millisecond();
		ist30xxc_put_track_ms(ms);
		ist30xxc_put_track(&tracking_intr_value, 1);
		ist30xxc_put_track(&calib_msg, 1);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID ||
		    CALIB_TO_STATUS(calib_msg) > 0) {
			ist30xxc_calibrate(data, IST30XXC_MAX_RETRY_CNT);
		}
	}

#if IST30XXC_CHECK_CALIB
	if (likely(!data->status.update)) {
		ret = ist30xxc_cmd_check_calib(data->client);
		if (likely(!ret)) {
			data->status.calib = 1;
			data->status.calib_msg = 0;
			ist30xxc_event_ms = (u32)get_millisecond();
			data->status.event_mode = true;
		}
	}
#else
	ist30xxc_start(data);
#endif

#if !(IST30XXC_INTERNAL_BIN)
get_info_end:
#endif

    ist30xxc_enable_irq(data);
	mutex_unlock(&ist30xxc_mutex);

	return ret;
}

#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x06)
#define TOUCH_DOWN_MESSAGE      ("p")
#define TOUCH_UP_MESSAGE        ("r")
#define TOUCH_MOVE_MESSAGE      (" ")
bool ts_touched[IST30XXC_MAX_MT_FINGERS] = { false, };
bool tkey_pressed[IST30XXC_MAX_KEYS] = { false, };
void print_touch_event(struct ist30xxc_data *data, fingers_info *finger)
{
#if TOUCH_BOOSTER
	static int finger_cnts = 0;
#endif
	int idx = finger->bit_field.id - 1;
	bool press;

	press = PRESSED_FINGER(data->t_status, finger->bit_field.id);

	if (press) {
		if (ts_touched[idx] == false) { // touch down
#if TOUCH_BOOSTER
			//if(!finger_cnts)
			{
                if(!min_handles)
			    {
				    min_handles = cpufreq_limit_min_freq(ts_cpufreq_lock, "TSP");
				    if (IS_ERR(min_handles)) {
					    printk(KERN_ERR "[ TSP ] cannot get cpufreq_min lock %lu\n",
					    ts_cpufreq_lock);
					    min_handles = NULL;
				    }
                    _store_cpu_num_min_limit(2);
				    ts_info("cpu freq on\n");
                }
			}
			finger_cnts ++;
#endif
			ts_noti("%s%d (%d, %d)\n",
				 TOUCH_DOWN_MESSAGE, finger->bit_field.id,
				 finger->bit_field.x, finger->bit_field.y);
			ts_touched[idx] = true;
		} else {                    // touch move
			ts_debug("%s%d (%d, %d)\n",
				  TOUCH_MOVE_MESSAGE, finger->bit_field.id,
				  finger->bit_field.x, finger->bit_field.y);
		}
	} else {
		if (ts_touched[idx] == true) { // touch up
			ts_noti("%s%d\n", TOUCH_UP_MESSAGE, finger->bit_field.id);
			ts_touched[idx] = false;
#if TOUCH_BOOSTER
			finger_cnts --;
			if(!finger_cnts)
			{
				cpufreq_limit_put(min_handles);
				min_handles = NULL;
                _store_cpu_num_min_limit(1);
				ts_info("cpu freq off\n");
			}
#endif
		}
	}
}

void print_tkey_event(struct ist30xxc_data *data, int id)
{
	int idx = id - 1;
	bool press = PRESSED_KEY(data->t_status, id);

	if (press) {
		if (tkey_pressed[idx] == false) { // tkey down
			ts_noti("k %s%d\n", TOUCH_DOWN_MESSAGE, id);
			tkey_pressed[idx] = true;
		}
	} else {
		if (tkey_pressed[idx] == true) { // tkey up
			ts_noti("k %s%d\n", TOUCH_UP_MESSAGE, id);
			tkey_pressed[idx] = false;
		}
	}
}

static void release_finger(struct ist30xxc_data *data, int id)
{
	input_mt_slot(data->input_dev, id - 1);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

	ist30xxc_tracking(TRACK_POS_FINGER + id);
	ts_info("%s() %d\n", __func__, id);
	ts_touched[id - 1] = false;

	input_sync(data->input_dev);
}

#define CANCEL_KEY  (0xFF)
#define RELEASE_KEY (0)
static void release_key(struct ist30xxc_data *data, int id, u8 key_status)
{
	input_report_key(data->input_dev, ist30xxc_key_code[id], key_status);

	ist30xxc_tracking(TRACK_POS_KEY + id);
	ts_info("%s() key%d, status: %d\n", __func__, id, key_status);

	tkey_pressed[id - 1] = false;

	input_sync(data->input_dev);
}

static void clear_input_data(struct ist30xxc_data *data)
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

	id = 1;
	status = PARSE_KEY_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_key(data, id, RELEASE_KEY);
		status >>= 1;
		id++;
	}
	data->t_status = 0;
}

static int check_report_fingers(struct ist30xxc_data *data, int finger_counts)
{
	int i;
	fingers_info *fingers = (fingers_info *)data->fingers;

	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if (unlikely((fingers[i].bit_field.x >= IST30XXC_MAX_X) ||
			     (fingers[i].bit_field.y >= IST30XXC_MAX_Y))) {
			ts_warn("Invalid touch data - %d: %d(%d, %d), 0x%08x\n", i,
				 fingers[i].bit_field.id,
				 fingers[i].bit_field.x,
				 fingers[i].bit_field.y,
				 fingers[i].full_field);

			fingers[i].bit_field.id = 0;
			ist30xxc_tracking(TRACK_POS_UNKNOWN);
			return -EPERM;
		}
	}

    return 0;
}

static int check_valid_coord(u32 *msg, int cnt)
{
	int ret = 0;
	u8 *buf = (u8 *)msg;
	u8 chksum1 = msg[0] >> 24;
	u8 chksum2 = 0;

	msg[0] &= 0x00FFFFFF;

	cnt *= IST30XXC_DATA_LEN;

	while (cnt--)
		chksum2 += *buf++;

	if (chksum1 != chksum2) {
		ts_err("intr chksum: %02x, %02x\n", chksum1, chksum2);
		ret = -EPERM;
	}

	return (chksum1 == chksum2) ? 0 : -EPERM;
}

static void report_input_data(struct ist30xxc_data *data, int finger_counts,
			      int key_counts)
{
	int id;
	bool press = false;
	fingers_info *fingers = (fingers_info *)data->fingers;
#if IST30XXC_JIG_MODE    
    u32 *z_values = (u32 *)data->z_values;
#endif
	int idx = 0;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	for (id = 0; id < IST30XXC_MAX_MT_FINGERS; id++) {
		press = (status & (1 << id)) ? true : false;

		input_mt_slot(data->input_dev, id);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		fingers[idx].bit_field.id = id + 1;
		print_touch_event(data, &fingers[idx]);

		if (press == false)
			continue;

		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				 fingers[idx].bit_field.x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				 fingers[idx].bit_field.y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
				 fingers[idx].bit_field.area);
#if IST30XXC_JIG_MODE
        if (data->jig_mode)
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, z_values[idx]);
#endif
        idx++;
	}

#if IST30XXC_USE_KEY
	status = PARSE_KEY_STATUS(data->t_status);
	for (id = 0; id < IST30XXC_MAX_KEYS; id++) {
		press = (status & (1 << id)) ? true : false;

		input_report_key(data->input_dev, ist30xxc_key_code[id + 1], press);

		print_tkey_event(data, id + 1);
	}
#endif  // IST30XXC_USE_KEY

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
u32 int_debug_addr, int_debug2_addr, int_debug3_addr = 0;
u32 int_debug_size, int_debug2_size, int_debug3_size = 0;
static irqreturn_t ist30xxc_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xxc_data *data = (struct ist30xxc_data *)ptr;
	int offset = 1;
#if IST30XXC_STATUS_DEBUG
    u32 touch_status;
#endif
	u32	t_status;
	u32 msg[IST30XXC_MAX_MT_FINGERS * 2 + offset];
	u32 ms;

	data->irq_working = true;

	if (unlikely(!data->irq_enabled))
		goto irq_end;

	ms = get_millisecond();

    if (int_debug_addr >= 0 && int_debug_size > 0) {
        ts_noti("Intr_debug (addr: 0x%08x)\n", int_debug_addr);
        ist30xxc_burst_read(data->client, IST30XXC_DA_ADDR(int_debug_addr), 
                &msg[0], int_debug_size, true);

        for (i = 0; i < int_debug_size; i++) {
            ts_noti("\t%08x\n", msg[i]);
        }
        ist30xxc_put_track(msg, int_debug_size);
    }

    if (int_debug2_addr >= 0 && int_debug2_size > 0) {
        ts_noti("Intr_debug2 (addr: 0x%08x)\n", int_debug2_addr);
        ist30xxc_burst_read(data->client, IST30XXC_DA_ADDR(int_debug2_addr), 
                &msg[0], int_debug2_size, true);

        for (i = 0; i < int_debug2_size; i++) {
            ts_noti("\t%08x\n", msg[i]);
        }
        ist30xxc_put_track(msg, int_debug2_size);
	}


   	memset(msg, 0, sizeof(msg));

	ret = ist30xxc_read_reg(data->client, IST30XXC_HIB_INTR_MSG, msg);
	if (unlikely(ret))
		goto irq_err;

    ts_verb("intr msg: 0x%08x\n", *msg);
		
    // TSP IC Exception
	if (unlikely((*msg & IST30XXC_EXCEPT_MASK) == IST30XXC_EXCEPT_VALUE)) {
       	ts_err("Occured IC exception(0x%02X)\n", *msg & 0xFF);        
#if I2C_BURST_MODE
		ret = ist30xxc_burst_read(data->client, 
                IST30XXC_HIB_COORD, &msg[offset], IST30XXC_MAX_EXCEPT_SIZE, true);
#else
		for (i = 0; i < IST30XXC_MAX_EXCEPT_SIZE; i++) {
			ret = ist30xxc_read_reg(data->client, 
                    IST30XXC_HIB_COORD + (i * 4), &msg[i + offset]);
		}
#endif
        if (unlikely(ret))
            ts_err(" exception value read error(%d)\n", ret);
        else
            ts_err(" exception value : 0x%08X, 0x%08X\n", msg[1], msg[2]);

	    goto irq_ic_err;
    }

	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF))  // Unknown CMD
		goto irq_err;

	ist30xxc_event_ms = ms;
	ist30xxc_put_track_ms(ist30xxc_event_ms);
	ist30xxc_put_track(&tracking_intr_value, 1);
	ist30xxc_put_track(msg, 1);

	if (unlikely((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID)) {
		data->status.calib_msg = *msg;
		ts_info("calib status: 0x%08x\n", data->status.calib_msg);

		goto irq_end;
	}

#if IST30XXC_CMCS_TEST   
    if (unlikely(*msg == IST30XXC_CMCS_MSG_VALID)) {
		data->status.cmcs = 1;
		ts_info("cmcs status: 0x%08x\n", *msg);

		goto irq_end;
	}
#endif

#if IST30XXC_STATUS_DEBUG
    ret = ist30xxc_read_reg(data->client, 
            IST30XXC_HIB_TOUCH_STATUS, &touch_status);

    if (ret == 0) {
        ts_debug("ALG : 0x%08X\n", touch_status);
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
		ts_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
			 finger_cnt, data->max_fingers, key_cnt, data->max_keys);
		goto irq_err;
	}

	if (finger_cnt > 0) {
#if I2C_BURST_MODE
		ret = ist30xxc_burst_read(data->client, 
                IST30XXC_HIB_COORD, &msg[offset], finger_cnt, true);
		if (unlikely(ret))
			goto irq_err;

		for (i = 0; i < finger_cnt; i++)
			data->fingers[i].full_field = msg[i + offset];
#else
		for (i = 0; i < finger_cnt; i++) {
			ret = ist30xxc_read_reg(data->client, 
                    IST30XXC_HIB_COORD + (i * 4), &msg[i + offset]);
			if (unlikely(ret))
				goto irq_err;

            data->fingers[i].full_field = msg[i + offset];
		}
#endif          // I2C_BURST_MODE

#if IST30XXC_JIG_MODE
        if (data->jig_mode) {
            // z-values ram address define
            ret = ist30xxc_burst_read(data->client, 
                    IST30XXC_DA_ADDR(data->tags.zvalue_base), 
                    data->z_values, finger_cnt, true);
        }
#endif
		ist30xxc_put_track(msg + offset, finger_cnt);
		for (i = 0; i < finger_cnt; i++) {
#if IST30XXC_JIG_MODE        
            if (data->jig_mode)
                ts_verb("intr msg(%d): 0x%08x, %d\n", 
                        i + offset, msg[i + offset], data->z_values[i]);
            else
#endif
                ts_verb("intr msg(%d): 0x%08x\n", i + offset, msg[i + offset]);
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

    if (int_debug3_addr >= 0 && int_debug3_size > 0) {
        ts_noti("Intr_debug3 (addr: 0x%08x)\n", int_debug3_addr);
        ist30xxc_burst_read(data->client, IST30XXC_DA_ADDR(int_debug3_addr), 
            &msg[0], int_debug3_size, true);

        for (i = 0; i < int_debug3_size; i++) {
            ts_noti("\t%08x\n", msg[i]);
        }
        ist30xxc_put_track(msg, int_debug3_size);
    }

	goto irq_end;

irq_err:
	ts_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xxc_request_reset(data);
irq_end:
	data->irq_working = false;
	ist30xxc_event_ms = (u32)get_millisecond();
	return IRQ_HANDLED;

irq_ic_err:
	ist30xxc_scheduled_reset(data);
	data->irq_working = false;
	ist30xxc_event_ms = (u32)get_millisecond();
	return IRQ_HANDLED;
}

int ist30xxc_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xxc_data *data = i2c_get_clientdata(client);

	del_timer(&ist30xxc_event_timer);
	cancel_delayed_work_sync(&data->work_noise_protect);
	cancel_delayed_work_sync(&data->work_reset_check);
	cancel_delayed_work_sync(&data->work_debug_algorithm);
	mutex_lock(&ist30xxc_mutex);
	ist30xxc_disable_irq(data);
	ist30xxc_internal_suspend(data);
	clear_input_data(data);

#if TOUCH_BOOSTER
	if(min_handles)
	{
		printk(KERN_ERR"[TSP] %s %d:: OOPs, Cpu was not in Normal Freq..\n", __func__, __LINE__);
		
		if (cpufreq_limit_put(min_handles) < 0) {
			printk(KERN_ERR "[TSP] Error in scaling down cpu frequency\n");
		}
		
		min_handles = NULL;
		ts_info("cpu freq off\n");
	}
#endif

	mutex_unlock(&ist30xxc_mutex);

	return 0;
}

int ist30xxc_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xxc_data *data = i2c_get_clientdata(client);

    data->noise_mode |= (1 << NOISE_MODE_POWER);

    mutex_lock(&ist30xxc_mutex);
	ist30xxc_internal_resume(data);
	ist30xxc_start(data);
	ist30xxc_enable_irq(data);
	mutex_unlock(&ist30xxc_mutex);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ist30xxc_early_suspend(struct early_suspend *h)
{
	struct ist30xxc_data *data = container_of(h, struct ist30xxc_data,
						 early_suspend);

	ist30xxc_suspend(&data->client->dev);
}
static void ist30xxc_late_resume(struct early_suspend *h)
{
	struct ist30xxc_data *data = container_of(h, struct ist30xxc_data,
						 early_suspend);

    ist30xxc_resume(&data->client->dev);
}
#endif

void ist30xxc_set_ta_mode(int status)
{
	if(ts_datas->initialized){
   		ts_info("%s(), mode = %d\n", __func__, status);

		if (status == POWER_SUPPLY_TYPE_BATTERY ||
	        status == POWER_SUPPLY_TYPE_UNKNOWN)
        	status = 0;
		else
			status = 1;

        if (unlikely(status == ((ts_datas->noise_mode >> NOISE_MODE_TA) & 1)))
	    	return;

        if (status)
            ts_datas->noise_mode |= (1 << NOISE_MODE_TA);        
        else
        	ts_datas->noise_mode &= ~(1 << NOISE_MODE_TA);

#if IST30XXC_TA_RESET
        if (unlikely(ts_datas->initialized))
        	ist30xxc_scheduled_reset(ts_datas);
#else
        ist30xxc_write_cmd(ts_datas->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_MODE_SPECIAL << 16) | (ts_datas->noise_mode & 0xFFFF)));
#endif

        ist30xxc_tracking(status ? TRACK_CMD_TACON : TRACK_CMD_TADISCON);
    }
}

void ist30xxc_set_call_mode(int mode)
{
	ts_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_datas->noise_mode >> NOISE_MODE_CALL) & 1)))
		return;

    if (mode)
		ts_datas->noise_mode |= (1 << NOISE_MODE_CALL);
    else
    	ts_datas->noise_mode &= ~(1 << NOISE_MODE_CALL);

#if IST30XXC_TA_RESET
    if (unlikely(ts_datas->initialized))
    	ist30xxc_scheduled_reset(ts_datas);
#else
    ist30xxc_write_cmd(ts_datas->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_MODE_SPECIAL << 16) | (ts_datas->noise_mode & 0xFFFF)));
#endif

    ist30xxc_tracking(mode ? TRACK_CMD_CALL : TRACK_CMD_NOTCALL);
}

void ist30xxc_set_cover_mode(int mode)
{
	ts_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_datas->noise_mode >> NOISE_MODE_COVER) & 1)))
		return;

    if (mode)
		ts_datas->noise_mode |= (1 << NOISE_MODE_COVER);
    else
	    ts_datas->noise_mode &= ~(1 << NOISE_MODE_COVER);
	
#if IST30XXC_TA_RESET
    if (unlikely(ts_datas->initialized))
    	ist30xxc_scheduled_reset(ts_datas);
#else
    ist30xxc_write_cmd(ts_datas->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_MODE_SPECIAL << 16) | (ts_datas->noise_mode & 0xFFFF)));
#endif

    ist30xxc_tracking(mode ? TRACK_CMD_COVER : TRACK_CMD_NOTCOVER);
}

static void reset_work_func(struct work_struct *work)
{
    struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xxc_data *data = container_of(delayed_work, 
            struct ist30xxc_data, work_reset_check);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	ts_info("Request reset function\n");

	if (likely((data->initialized == 1) && (data->status.power == 1) &&
		   (data->status.update != 1) && (data->status.calib != 1))) {
		mutex_lock(&ist30xxc_mutex);
		ist30xxc_disable_irq(data);
		clear_input_data(data);
		ist30xxc_reset(data, false);
		ist30xxc_start(data);
		ist30xxc_enable_irq(data);
		mutex_unlock(&ist30xxc_mutex);
	}
}

#if IST30XXC_INTERNAL_BIN && IST30XXC_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
    struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xxc_data *data = container_of(delayed_work, 
            struct ist30xxc_data, work_fw_update);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	ts_info("FW update function\n");

	if (likely(ist30xxc_auto_bin_update(data)))
		ist30xxc_disable_irq(data);
}
#endif // IST30XXC_INTERNAL_BIN && IST30XXC_UPDATE_BY_WORKQUEUE

#define TOUCH_STATUS_MAGIC      (0x00000075)
#define TOUCH_STATUS_MASK       (0x000000FF)
#define FINGER_ENABLE_MASK      (0x00100000)
#define SCAN_CNT_MASK           (0xFFE00000)
#define GET_FINGER_ENABLE(n)    ((n & FINGER_ENABLE_MASK) >> 20)
#define GET_SCAN_CNT(n)         ((n & SCAN_CNT_MASK) >> 21)
u32 ist30xxc_algr_addr = 0, ist30xxc_algr_size = 0;
static void noise_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
    u32 scan_count = 0;
    struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xxc_data *data = container_of(delayed_work, 
            struct ist30xxc_data, work_noise_protect);

    ret = ist30xxc_read_reg(data->client, 
            IST30XXC_HIB_TOUCH_STATUS, &touch_status);
	if (unlikely(ret)) {
		ts_warn("Touch status read fail!\n");
		goto retry_timer;
	}

	ist30xxc_put_track_ms(ist30xxc_timer_ms);
	ist30xxc_put_track(&touch_status, 1);

	ts_verb("Touch Info: 0x%08x\n", touch_status);

	/* Check valid scan count */
	if (unlikely((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC)) {
		ts_warn("Touch status is not corrected! (0x%08x)\n", touch_status);
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
		ts_warn("TSP IC is not responded! (0x%08x)\n", scan_count);
		goto retry_timer;
	}

    data->scan_retry = 0;
	data->scan_count = scan_count;
	return;

retry_timer:
	data->scan_retry++;
	ts_warn("Retry touch status!(%d)\n", data->scan_retry);

	if (unlikely(data->scan_retry == data->max_scan_retry)) {
		ist30xxc_scheduled_reset(data);
		data->scan_retry = 0;
	}
}

static void debug_work_func(struct work_struct *work)
{
#if IST30XXC_ALGORITHM_MODE
	int ret = -EPERM;
    int i;
	u32 *buf32;

    struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xxc_data *data = container_of(delayed_work, 
            struct ist30xxc_data, work_debug_algorithm);

    buf32 = kzalloc(ist30xxc_algr_size, GFP_KERNEL);

    for (i = 0; i < ist30xxc_algr_size; i++) {
        ret = ist30xxc_read_buf(data->client, 
                ist30xxc_algr_addr + IST30XXC_DATA_LEN * i, &buf32[i], 1);
        if (ret) {
		    ts_warn("Algorithm mem addr read fail!\n");
    		return;
	    }
    }

    ist30xxc_put_track(buf32, ist30xxc_algr_size);

	ts_debug(" 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		  buf32[0], buf32[1], buf32[2], buf32[3], buf32[4]);
#endif
}

void ist30xxc_timer_handler(unsigned long timer_data)
{
    struct ist30xxc_data *data = (struct ist30xxc_data *)timer_data;
	struct ist30xxc_status *status = &data->status;

	if (data->irq_working)
		goto restart_timer;

	if (status->event_mode) {
		if (likely((status->power == 1) && (status->update != 1))) {
			ist30xxc_timer_ms = (u32)get_millisecond();
			if (unlikely(status->calib == 1)) { // Check calibration
				if ((status->calib_msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
					ts_info("Calibration check OK!!\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				} else if (ist30xxc_timer_ms - ist30xxc_event_ms >= 3000) { 
                    // 3second timeout
					ts_info("calibration timeout over 3sec\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				}
			} else if (likely(status->noise_mode)) {
                // 100ms after last interrupt
				if (ist30xxc_timer_ms - ist30xxc_event_ms > 100)
					schedule_delayed_work(&data->work_noise_protect, 0);
			}

#if IST30XXC_ALGORITHM_MODE
            if ((ist30xxc_algr_addr >= IST30XXC_DIRECT_ACCESS) &&
                (ist30xxc_algr_size > 0)) {
                // 100ms after last interrupt
                if (ist30xxc_timer_ms - ist30xxc_event_ms > 100)
                    schedule_delayed_work(&data->work_debug_algorithm, 0);
            }
#endif
		}
	}

restart_timer:
	mod_timer(&ist30xxc_event_timer, get_jiffies_64() + EVENT_TIMER_INTERVALS);
}

#if IST30XXC_SEC_FACTORY
extern int sec_ts_sysfs(struct ist30xxc_data *data);
extern int sec_factory_cmd_init(struct ist30xxc_data *data);
#endif
int ist30xxc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int retry = 3;
	struct ist30xxc_data *data;
	struct input_dev *input_dev;
	int ist_gpio = 0;

    ts_info("### IMAGIS probe(ver:%s, protocol:%X, addr:0x%02X) ###\n", 
            IMAGIS_DD_VERSION, IMAGIS_PROTOCOL_B, client->addr);

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
        ts_err("input_allocate_device failed\n");        
		goto err_alloc_dev;
	}

    data->max_fingers = IST30XXC_MAX_MT_FINGERS;
    data->max_keys = IST30XXC_MAX_KEYS;
	data->irq_enabled = 1;    
	data->client = client;
    data->input_dev = input_dev;
    i2c_set_clientdata(client, data);
    
    input_mt_init_slots(input_dev, IST30XXC_MAX_MT_FINGERS, 0);

	input_dev->name = "ist30xx_ts_input";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    input_set_abs_params(data->input_dev, ABS_MT_POSITION_X, 
            0, IST30XXC_MAX_X - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_Y, 
            0, IST30XXC_MAX_Y - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,	0, IST30XXC_MAX_W, 0, 0);
#if IST30XXC_JIG_MODE    
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0, 0, 0);
#endif

#if IST30XXC_USE_KEY
    {
	    int i;
	    set_bit(EV_KEY, input_dev->evbit);
	    set_bit(EV_SYN, input_dev->evbit);
	    for (i = 1; i < ARRAY_SIZE(ist30xxc_key_code); i++)
		    set_bit(ist30xxc_key_code[i], input_dev->keybit);
    }
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (unlikely(ret)) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xxc_early_suspend;
	data->early_suspend.resume = ist30xxc_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	ts_datas = data;

#ifdef TSP_USE_GPIO_CONTROL
	printk("TSP gpio use control .\n");
	ret = gpio_request(TSP_PWR_GPIO_EN, "ist30xx_ldo_en_gpio");
	if(ret)
		printk("ist30xx_ldo_en_gpio gpio request fail!\n");
#endif

	ret = ist30xxc_init_system(data);
	printk("[ TSP ] init system .\n");
	if (unlikely(ret)) {
		ts_err("chip initialization failed\n");
		goto err_init_drv;
	}

    while ((data->chip_id != IST30XXC_CHIP_ID) && 
            (data->chip_id != IST30XXC_DEFAULT_CHIP_ID) && 
            (data->chip_id != IST3048C_DEFAULT_CHIP_ID)) {
		ret = ist30xxc_read_cmd(data, IST30XXC_REG_CHIPID, &data->chip_id);
		data->chip_id &= 0xFFFF;
        if (unlikely(ret))
            ist30xxc_reset(data, false);

		if (retry-- == 0)
			goto err_init_drv;
	}
    ts_info("ChipID: %x\n", data->chip_id);

	ret = ist30xxc_init_update_sysfs(data);
	printk("TSP update sysfs .\n");
	if (unlikely(ret))
		goto err_init_drv;

#if IST30XXC_DEBUG
	ret = ist30xxc_init_misc_sysfs(data);
	if (unlikely(ret))
		goto err_init_drv;
#endif

#if IST30XXC_CMCS_TEST
	ret = ist30xxc_init_cmcs_sysfs(data);
	if (unlikely(ret))
		goto err_init_drv;
#endif

#if IST30XXC_SEC_FACTORY
	ret = sec_factory_cmd_init(data);
	if (unlikely(ret))
		goto err_init_drv;

	ret = sec_ts_sysfs(data);
	if (unlikely(ret))
		goto err_init_drv;
#endif

#if IST30XXC_TRACKING_MODE
	ret = ist30xxc_init_tracking_sysfs();
	if (unlikely(ret))
		goto err_init_drv;
#endif
#ifndef CONFIG_OF
    ist_gpio = 82;
#endif
    /* configure touchscreen interrupt gpio */
	printk("TSP GPIO REQUEST .\n");
	ret = gpio_request(ist_gpio, "ist30xx_irq_gpio");
	printk("TSP gpio request done .\n");
	if (ret) {
		ts_err("unable to request gpio.(%s)\r\n",input_dev->name);
		goto err_init_drv;
	}
	gpio_direction_input(ist_gpio);
	printk("TSP GPIO direction set .\n");

	client->irq = gpio_to_irq(ist_gpio);
	printk("TSP- CLIENT IRQ .\n");
    ts_info("client->irq : %d\n", client->irq);
   	ret = request_threaded_irq(client->irq, NULL, ist30xxc_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist30xx_ts", data);
	if (unlikely(ret))
		goto err_irq;

	ist30xxc_disable_irq(data);
    sprd_i2c_ctl_chg_clk(1, 400000); // up h/w i2c 1 400k

#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
    retry = 3;
	data->tsp_type = TSP_TYPE_UNKNOWN;
	while (retry-- > 0) {
		ret = ist30xxc_read_cmd(data, IST30XXC_REG_TSPTYPE, &data->tsp_type);
        if (likely(ret == 0)) {
        	data->tsp_type = IST30XXC_PARSE_TSPTYPE(data->tsp_type);
            ts_info("tsptype: %x\n", data->tsp_type);
            break;
        }	

		if (unlikely(retry == 0))
			goto err_irq;
	}

	ts_info("TSP IC: %x, TSP Vendor: %x\n", data->chip_id, data->tsp_type);
#else
    ts_info("TSP IC: %x\n", data->chip_id);
#endif
   	data->status.event_mode = false;

#if IST30XXC_INTERNAL_BIN
#if IST30XXC_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&data->work_fw_update, fw_update_func);
	schedule_delayed_work(&data->work_fw_update, IST30XXC_UPDATE_DELAY);
#else
	ret = ist30xxc_auto_bin_update(data);
	if (unlikely(ret != 0))
		goto err_irq;
#endif
#endif  // IST30XXC_INTERNAL_BIN

    // INIT VARIABLE (DEFAULT)
    data->irq_working = false;
    data->max_scan_retry = 2;
    data->max_irq_err_cnt = MAX_ERRS_CNT;
    data->report_rate = -1;
    data->idle_rate = -1;

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);

    init_timer(&ist30xxc_event_timer);
    ist30xxc_event_timer.data = (unsigned long)data;
	ist30xxc_event_timer.function = ist30xxc_timer_handler;
	ist30xxc_event_timer.expires = jiffies_64 + (EVENT_TIMER_INTERVALS);
	mod_timer(&ist30xxc_event_timer, 
            get_jiffies_64() + EVENT_TIMER_INTERVALS * 2);

	ret = ist30xxc_get_info(data);
	ts_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));
	
	data->initialized = true;
	tsp_charger_status_cb = ist30xxc_set_ta_mode;
    ts_info("### IMAGIS probe success ###\n");

	return 0;

err_irq:
	ts_info("ChipID: %x\n", data->chip_id);
	ist30xxc_disable_irq(data);
	free_irq(client->irq, data);
err_init_drv:
	data->status.event_mode = false;
	ts_err("Error, ist30xx init driver\n");
	ist30xxc_power_off(data);
#ifdef CONFIG_HAS_EARLYSUSPEND	
	unregister_early_suspend(&data->early_suspend);
#endif
	input_unregister_device(input_dev);
err_reg_dev:
err_alloc_dev:
    ts_err("Error, ist30xx mem free\n");
	kfree(data);
	return -1;
}


int ist30xxc_remove(struct i2c_client *client)
{
	struct ist30xxc_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

    ist30xxc_disable_irq(data);
	free_irq(client->irq, data);
	ist30xxc_power_off(data);

	input_unregister_device(data->input_dev);
	tsp_charger_status_cb = NULL;
	kfree(data);

	return 0;
}

void ist30xxc_shutdown(struct i2c_client *client)
{
	struct ist30xxc_data *data = i2c_get_clientdata(client);

    if(data->initialized){
        del_timer(&ist30xxc_event_timer);
    	cancel_delayed_work_sync(&data->work_noise_protect);
	    cancel_delayed_work_sync(&data->work_reset_check);
    	cancel_delayed_work_sync(&data->work_debug_algorithm);
    	mutex_lock(&ist30xxc_mutex);
    	ist30xxc_disable_irq(data);
    	ist30xxc_internal_suspend(data);
		mutex_unlock(&ist30xxc_mutex);
	}
}

