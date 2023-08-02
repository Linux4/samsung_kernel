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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/stat.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"

#define TSP_CH_UNUSED               (0)
#define TSP_CH_SCREEN               (1)
#define TSP_CH_GTX                  (2)
#define TSP_CH_KEY                  (3)
#define TSP_CH_UNKNOWN              (-1)

#define TOUCH_NODE_PARSING_DEBUG    (0)

static u32 *ist30xxc_frame_buf;
static u32 *ist30xxc_frame_rawbuf;
static u32 *ist30xxc_frame_fltbuf;

int ist30xxc_check_valid_ch(struct ist30xxc_data *data, int ch_tx, int ch_rx)
{
	int i;
	TKEY_INFO *tkey = &data->tkey_info;
	TSP_INFO *tsp = &data->tsp_info;

	if (unlikely((ch_tx >= tsp->ch_num.tx) || (ch_rx >= tsp->ch_num.rx)))
		return TSP_CH_UNKNOWN;

	if ((ch_tx >= tsp->screen.tx) || (ch_rx >= tsp->screen.rx)) {
		if (tsp->gtx.num) {
			for (i = 0; i < tsp->gtx.num; i++) {
				if ((ch_tx == tsp->gtx.ch_num[i]) && (ch_rx < tsp->screen.rx))
					return TSP_CH_GTX;
            }
		}

		if (tkey->enable) {
			for (i = 0; i < tkey->key_num; i++) {
				if ((ch_tx == tkey->ch_num[i].tx) &&
				    (ch_rx == tkey->ch_num[i].rx))
					return TSP_CH_KEY;
			}
		}
	} else {
		return TSP_CH_SCREEN;
	}

	return TSP_CH_UNUSED;
}

int ist30xxc_parse_touch_node(struct ist30xxc_data *data, u8 flag, 
        struct TSP_NODE_BUF *node)
{
#if TOUCH_NODE_PARSING_DEBUG
	int j;
	TSP_INFO *tsp = &data->tsp_info;
#endif
	int i;
	u16 *raw = (u16 *)&node->raw;
	u16 *base = (u16 *)&node->base;
	u16 *filter = (u16 *)&node->filter;
    u32 *tmp_rawbuf = ist30xxc_frame_rawbuf;
	u32 *tmp_fltbuf = ist30xxc_frame_fltbuf;

	for (i = 0; i < node->len; i++) {
		if (flag & (NODE_FLAG_RAW | NODE_FLAG_BASE)) {
			*raw++ = *tmp_rawbuf & 0xFFF;
			*base++ = (*tmp_rawbuf >> 16) & 0xFFF;

			tmp_rawbuf++;
		}
		if (flag & NODE_FLAG_FILTER)
			*filter++ = *tmp_fltbuf++ & 0xFFF;
	}

#if TOUCH_NODE_PARSING_DEBUG
	ts_info("RAW - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->raw[(i * tsp->ch_num.rx) + j]);
	}

	ts_info("BASE - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->base[(i * tsp->ch_num.rx) + j]);
	}

	ts_info("FILTER - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->filter[(i * tsp->ch_num.rx) + j]);
	}
#endif

    return 0;
}

int print_ts_node(struct ist30xxc_data *data, u8 flag, 
        struct TSP_NODE_BUF *node, char *buf)
{
	int i, j;
	int count = 0;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &data->tsp_info;

    if (tsp->dir.swap_xy) {
        for (i = 0; i < tsp->ch_num.rx; i++) {
			for (j = 0; j < tsp->ch_num.tx; j++) {
				if (flag == NODE_FLAG_RAW)
					val = (int)node->raw[(j * tsp->ch_num.rx) + i];
				else if (flag == NODE_FLAG_BASE)
					val = (int)node->base[(j * tsp->ch_num.rx) + i];
				else if (flag == NODE_FLAG_FILTER)
					val = (int)node->filter[(j * tsp->ch_num.rx) + i];
				else if (flag == NODE_FLAG_DIFF)
					val = (int)(node->raw[(j * tsp->ch_num.rx) + i] 
                            - node->base[(j * tsp->ch_num.rx) + i]);
				else
					return 0;

				if (val < 0) val = 0;

                if (ist30xxc_check_valid_ch(data, j, i) == TSP_CH_UNUSED)
                    count += snprintf(msg, msg_len, "%4d ", 0);    				
                else
                    count += snprintf(msg, msg_len, "%4d ", val);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (flag == NODE_FLAG_RAW)
					val = (int)node->raw[(i * tsp->ch_num.rx) + j];
				else if (flag == NODE_FLAG_BASE)
					val = (int)node->base[(i * tsp->ch_num.rx) + j];
				else if (flag == NODE_FLAG_FILTER)
					val = (int)node->filter[(i * tsp->ch_num.rx) + j];
				else if (flag == NODE_FLAG_DIFF)
					val = (int)(node->raw[(i * tsp->ch_num.rx) + j] 
                            - node->base[(i * tsp->ch_num.rx) + j]);
				else
					return 0;

				if (val < 0) val = 0;

                if (ist30xxc_check_valid_ch(data, i, j) == TSP_CH_UNUSED)
                    count += snprintf(msg, msg_len, "%4d ", 0);    				
                else
                    count += snprintf(msg, msg_len, "%4d ", val);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	}

	return count;
}

int parse_ts_node(struct ist30xxc_data *data, u8 flag, 
        struct TSP_NODE_BUF *node, s16 *buf16, int mode)
{
	int i, j;
	s16 val = 0;
	TSP_INFO *tsp = &data->tsp_info;

	if (unlikely((flag != NODE_FLAG_RAW) && (flag != NODE_FLAG_BASE) &&
		     (flag != NODE_FLAG_FILTER) && (flag != NODE_FLAG_DIFF)))
		return -EPERM;

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			if (mode & TS_RAW_SCREEN) {
    			if (ist30xxc_check_valid_ch(data, i, j) != TSP_CH_SCREEN)
	    			continue;
            } else if (mode & TS_RAW_KEY) {
                if (ist30xxc_check_valid_ch(data, i, j) != TSP_CH_KEY)
	    			continue;
            }
			switch (flag) {
			case NODE_FLAG_RAW:
				val = (s16)node->raw[(i * tsp->ch_num.rx) + j];
				break;
			case NODE_FLAG_BASE:
				val = (s16)node->base[(i * tsp->ch_num.rx) + j];
				break;
			case NODE_FLAG_FILTER:
				val = (s16)node->filter[(i * tsp->ch_num.rx) + j];
				break;
			case NODE_FLAG_DIFF:
				val = (s16)(node->raw[(i * tsp->ch_num.rx) + j] 
                        - node->base[(i * tsp->ch_num.rx) + j]);
				break;
			}

			if (val < 0) val = 0;

            if (ist30xxc_check_valid_ch(data, i, j) == TSP_CH_UNUSED)
                val = 0;

			*buf16++ = val;
		}
	}

	return 0;
}

int ist30xxc_read_touch_node(struct ist30xxc_data *data, u8 flag, 
        struct TSP_NODE_BUF *node)
{
    int i;
    int ret;
	u32 addr;
    u32 *tmp_rawbuf = ist30xxc_frame_rawbuf;
	u32 *tmp_fltbuf = ist30xxc_frame_fltbuf;

	ist30xxc_disable_irq(data);

	if (flag & NODE_FLAG_NO_CCP) {
        ist30xxc_reset(data, false);
		ret = ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_CP_CORRECT_EN << 16) | (IST30XXC_DISABLE & 0xFFFF)));
		if (unlikely(ret))
			goto read_tsp_node_end;

        msleep(1000);
	}

	ret = ist30xxc_cmd_hold(data->client, 1);
	if (unlikely(ret))
		goto read_tsp_node_end;

    addr = IST30XXC_DA_ADDR(data->tags.raw_base);
    if (flag & (NODE_FLAG_RAW | NODE_FLAG_BASE)) {
		ts_info("Reg addr: %x, size: %d\n", addr, node->len);
        for (i = 0; i < node->len; i++) {
    		ret = ist30xxc_read_buf(data->client, addr, tmp_rawbuf, 1);
		    if (unlikely(ret))
			    goto reg_access_end;

            addr += IST30XXC_DATA_LEN;
            tmp_rawbuf++;
        }
	}

    addr = IST30XXC_DA_ADDR(data->tags.filter_base);
	if (flag & NODE_FLAG_FILTER) {
		ts_info("Reg addr: %x, size: %d\n", addr, node->len);
        for (i = 0; i < node->len; i++) {
    		ret = ist30xxc_read_buf(data->client, addr, tmp_fltbuf, 1);
		    if (unlikely(ret))
			    goto reg_access_end;

            addr += IST30XXC_DATA_LEN;
            tmp_fltbuf++;
        }
	}

reg_access_end:
    ist30xxc_cmd_hold(data->client, 0);	

    if (flag & NODE_FLAG_NO_CCP) {
		ist30xxc_reset(data, false);
        ist30xxc_start(data);
	}	

read_tsp_node_end:
	ist30xxc_enable_irq(data);

	return ret;
}

/* sysfs: /sys/class/touch/node/refresh */
ssize_t ist30xxc_frame_refresh(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int ret = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER;

	ts_info("refresh\n");
   	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_read_touch_node(data, flag, &tsp->node);
	if (unlikely(ret)) {
    	mutex_unlock(&ist30xxc_mutex);
		ts_err("cmd 1frame raw update fail\n");
		return sprintf(buf, "FAIL\n");
	}
    mutex_unlock(&ist30xxc_mutex);

	ist30xxc_parse_touch_node(data, flag, &tsp->node);

	return sprintf(buf, "OK\n");
}


/* sysfs: /sys/class/touch/node/read_nocp */
ssize_t ist30xxc_frame_nocp(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER |
		  NODE_FLAG_NO_CCP;
    
    mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_read_touch_node(data, flag, &tsp->node);
	if (unlikely(ret)) {
		mutex_unlock(&ist30xxc_mutex);
		ts_err("cmd 1frame raw update fail\n");
		return sprintf(buf, "FAIL\n");
	}
   	mutex_unlock(&ist30xxc_mutex);

	ist30xxc_parse_touch_node(data, flag, &tsp->node);

	return sprintf(buf, "OK\n");
}


/* sysfs: /sys/class/touch/node/base */
ssize_t ist30xxc_base_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xx baseline(%d)\n", tsp->node.len);

	count += print_ts_node(data, NODE_FLAG_BASE, &tsp->node, buf);

	return count;
}


/* sysfs: /sys/class/touch/node/raw */
ssize_t ist30xxc_raw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	int count = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xx raw(%d)\n", tsp->node.len);

	count += print_ts_node(data, NODE_FLAG_RAW, &tsp->node, buf);

	return count;
}


/* sysfs: /sys/class/touch/node/diff */
ssize_t ist30xxc_diff_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xx difference (%d)\n", tsp->node.len);

	count += print_ts_node(data, NODE_FLAG_DIFF, &tsp->node, buf);

	return count;
}


/* sysfs: /sys/class/touch/node/filter */
ssize_t ist30xxc_filter_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int count = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xx filter (%d)\n", tsp->node.len);

	count += print_ts_node(data, NODE_FLAG_FILTER, &tsp->node, buf);

	return count;
}

/* sysfs: /sys/class/touch/sys/debug_mode */
ssize_t ist30xxc_debug_mode_store(struct device *dev, 
				struct device_attribute *attr, const char *buf, size_t size)
{
    int ret = 0;
	int enable;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &enable);

	if (unlikely((enable != 0) && (enable != 1))) {
        ts_err("input data error(%d)\n", enable);
		return size;
    }
    
    if (data->status.power) {
        ret = ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
                   (eHCOM_SLEEP_MODE_EN << 16) | ((enable ? 0 : 1) & 0xFFFF));
    
        if (ret) {
            ts_info("failed to set debug mode(%d)\n", enable);
        } else {
            data->debug_mode = enable;
            ts_info("debug mode %s\n", enable ? "start" : "stop");
        }
    } else {
        data->debug_mode = enable;
        ts_info("debug mode %s\n", enable ? "start" : "stop");
    }

	return size;
}

#if IST30XXC_JIG_MODE
/* sysfs: /sys/class/touch/sys/jig_mode */
ssize_t ist30xxc_jig_mode_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &enable);

	if (unlikely((enable != 0) && (enable != 1))) {
        ts_err("input data error(%d)\n", enable);
		return size;
    }

    data->jig_mode = enable;
    ts_info("set jig mode: %s\n", enable ? "start" : "stop");

    mutex_lock(&ist30xxc_mutex);
    ist30xxc_reset(data, false);
    ist30xxc_start(data);
    mutex_unlock(&ist30xxc_mutex);

	return size;
}
#endif

extern int cal_ms_delay;
/* sysfs: /sys/class/touch/sys/clb_time */
ssize_t ist30xxc_calib_time_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int ms_delay;

	sscanf(buf, "%d", &ms_delay);

	if (ms_delay > 10 && ms_delay < 1000) // 1sec ~ 100sec
		cal_ms_delay = ms_delay;

	ts_info("Calibration wait time %dsec\n", cal_ms_delay / 10);

	return size;
}

/* sysfs: /sys/class/touch/sys/clb */
ssize_t ist30xxc_calib_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

    mutex_lock(&ist30xxc_mutex);
    ist30xxc_disable_irq(data);

    ist30xxc_reset(data, false);
	ist30xxc_calibrate(data, 1);

    mutex_unlock(&ist30xxc_mutex);
	ist30xxc_start(data);

	return 0;
}

/* sysfs: /sys/class/touch/sys/clb_result */
ssize_t ist30xxc_calib_result_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	int count = 0;
	u32 value;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	mutex_lock(&ist30xxc_mutex);
	ist30xxc_disable_irq(data);
	
	ret = ist30xxc_read_cmd(data, eHCOM_GET_CAL_RESULT, &value);
	if (unlikely(ret)) {
        ist30xxc_reset(data, false);
        ist30xxc_start(data);
        ts_warn("Error Read Calibration Result\n");
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	count = sprintf(buf,
			"Calibration Status : %d, Max raw gap : %d - (raw: %08x)\n",
			CALIB_TO_STATUS(value), CALIB_TO_GAP(value), value);

calib_show_end:	
	ist30xxc_enable_irq(data);
	mutex_unlock(&ist30xxc_mutex);

	return count;
}

/* sysfs: /sys/class/touch/sys/power_on */
ssize_t ist30xxc_power_on_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	ts_info("Power enable: %d\n", true);

	mutex_lock(&ist30xxc_mutex);
	ist30xxc_internal_resume(data);
	ist30xxc_enable_irq(data);
	mutex_unlock(&ist30xxc_mutex);

	ist30xxc_start(data);

	return 0;
}

/* sysfs: /sys/class/touch/sys/power_off */
ssize_t ist30xxc_power_off_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	ts_info("Power enable: %d\n", false);

	mutex_lock(&ist30xxc_mutex);
	ist30xxc_disable_irq(data);
	ist30xxc_internal_suspend(data);
	mutex_unlock(&ist30xxc_mutex);

	return 0;
}

/* sysfs: /sys/class/touch/sys/errcnt */
ssize_t ist30xxc_errcnt_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int err_cnt;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &err_cnt);

	if (unlikely(err_cnt < 0))
		return size;

	ts_info("Request reset error count: %d\n", err_cnt);

	data->max_irq_err_cnt = err_cnt;

	return size;
}

/* sysfs: /sys/class/touch/sys/scancnt */
ssize_t ist30xxc_scancnt_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int retry;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &retry);

	if (unlikely(retry < 0))
		return size;

	ts_info("Timer scan count retry: %d\n", retry);

	data->max_scan_retry = retry;

	return size;
}

extern int timer_period_ms;
/* sysfs: /sys/class/touch/sys/timerms */
ssize_t ist30xxc_timerms_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ms;

	sscanf(buf, "%d", &ms);

	if (unlikely((ms < 0) || (ms > 10000)))
		return size;

	ts_info("Timer period ms: %dms\n", ms);

	timer_period_ms = ms;

	return size;
}

extern int ist30xxc_dbg_level;
/* sysfs: /sys/class/touch/sys/printk */
ssize_t ist30xxc_printk_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int level;

	sscanf(buf, "%d", &level);

	if (unlikely((level < TS_DEV_ERR) || (level > TS_DEV_VERB)))
		return size;

	ts_info("prink log level: %d\n", level);

	ist30xxc_dbg_level = level;

	return size;
}

ssize_t ist30xxc_printk_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
    return sprintf(buf, "prink log level: %d\n", ist30xxc_dbg_level);
}

/* sysfs: /sys/class/touch/sys/printk5 */
ssize_t ist30xxc_printk5_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	ts_info("prink log level:%d\n", TS_DEV_DEBUG);

	ist30xxc_dbg_level = TS_DEV_DEBUG;

	return 0;
}

/* sysfs: /sys/class/touch/sys/printk6 */
ssize_t ist30xxc_printk6_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	ts_info("prink log level:%d\n", TS_DEV_VERB);

	ist30xxc_dbg_level = TS_DEV_VERB;

	return 0;
}

/* sysfs: /sys/class/touch/sys/max_touch */
ssize_t ist30xxc_touch_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int finger;
	int key;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d %d", &finger, &key);

	ts_info("fingers : %d, keys : %d\n", finger, key);

	data->max_fingers = finger;
	data->max_keys = key;

	return size;
}

/* sysfs: /sys/class/touch/sys/touch_rate */
ssize_t ist30xxc_touch_rate_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &rate);               // us

	if (unlikely(rate > 0xFFFF))            // over 65.5ms
		return size;

	data->report_rate = rate;
    ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_TIME_ACTIVE << 16) | (data->report_rate & 0xFFFF)));
    ts_info(" active rate : %dus\n", data->report_rate);

    return size;
}

/* sysfs: /sys/class/touch/sys/idle_rate */
ssize_t ist30xxc_idle_scan_rate_store(struct device *dev, 
                struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &rate);               // us

	if (unlikely(rate > 0xFFFF))            // over 65.5ms
		return size;

    data->idle_rate = rate;
    ist30xxc_write_cmd(data->client, IST30XXC_HIB_CMD, 
            ((eHCOM_SET_TIME_IDLE << 16) | (data->idle_rate & 0xFFFF)));
	ts_info(" idle rate : %dus\n", data->idle_rate);
	
	return size;
}

/* sysfs: /sys/class/touch/sys/mode_ta */
ssize_t ist30xxc_ta_mode_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xxc_set_ta_mode(mode);

	return size;
}

extern void ist30xxc_set_call_mode(int mode);
/* sysfs: /sys/class/touch/sys/mode_call */
ssize_t ist30xxc_call_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xxc_set_call_mode(mode);

	return size;
}

extern void ist30xxc_set_cover_mode(int mode);
/* sysfs: /sys/class/touch/sys/mode_cover */
ssize_t ist30xxc_cover_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xxc_set_cover_mode(mode);

	return size;
}

#define TUNES_CMD_WRITE         (1)
#define TUNES_CMD_READ          (2)
#define TUNES_CMD_REG_ENTER     (3)
#define TUNES_CMD_REG_EXIT      (4)
#define TUNES_CMD_UPDATE_PARAM  (5)
#define TUNES_CMD_UPDATE_FW     (6)

#define DIRECT_ADDR(n)          (IST30XXC_DA_ADDR(n))
#define DIRECT_CMD_WRITE        ('w')
#define DIRECT_CMD_READ         ('r')

#pragma pack(1)
typedef struct {
	u8	cmd;
	u32	addr;
	u16	len;
} TUNES_INFO;
#pragma pack()
#pragma pack(1)
typedef struct {
	char	cmd;
	u32	addr;
	u32	val;
} DIRECT_INFO;
#pragma pack()

static TUNES_INFO ist30xxc_tunes;
static DIRECT_INFO ist30xxc_direct;
static bool tunes_cmd_done = false;
static bool ist30xxc_reg_mode = false;

/* sysfs: /sys/class/touch/sys/direct */
ssize_t ist30xxc_direct_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int ret = -EPERM;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xxc_direct;

	sscanf(buf, "%c %x %x", &direct->cmd, &direct->addr, &direct->val);

	ts_debug("Direct cmd: %c, addr: %x, val: %x\n",
		  direct->cmd, direct->addr, direct->val);

	if (unlikely((direct->cmd != DIRECT_CMD_WRITE) &&
		     (direct->cmd != DIRECT_CMD_READ))) {
		ts_warn("Direct cmd is not correct!\n");
		return size;
	}

	if (ist30xxc_intr_wait(data, 30) < 0)
		return size;

	data->status.event_mode = false;
	if (direct->cmd == DIRECT_CMD_WRITE) {
        ist30xxc_cmd_hold(data->client, 1);
		ist30xxc_write_cmd(data->client, DIRECT_ADDR(direct->addr), 
                direct->val);
		ist30xxc_read_reg(data->client, DIRECT_ADDR(direct->addr), 
                &direct->val);
        ret = ist30xxc_cmd_hold(data->client, 0);
        if (ret) {
            ts_debug("Direct write fail\n");
            ist30xxc_reset(data, false);
            ist30xxc_start(data);
        } else {
            ts_debug("Direct write addr: %x, val: %x\n", 
                direct->addr, direct->val);
        }
	}
	data->status.event_mode = true;

	return size;
}

#define DIRECT_BUF_COUNT        (4)
ssize_t ist30xxc_direct_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int i, ret, count = 0;
	int len;
	u32 addr;
	u32 buf32[DIRECT_BUF_COUNT];
	int max_len = DIRECT_BUF_COUNT;
	const int msg_len = 256;
	char msg[msg_len];
    struct ist30xxc_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xxc_direct;

	if (unlikely(direct->cmd != DIRECT_CMD_READ))
		return sprintf(buf, "ex) echo r addr len > direct\n");

	len = direct->val;
	addr = DIRECT_ADDR(direct->addr);

	if (ist30xxc_intr_wait(data, 30) < 0)
		return 0;

    data->status.event_mode = false;
    ist30xxc_cmd_hold(data->client, 1);

    while (len > 0) {
		if (len < max_len) max_len = len;

		memset(buf32, 0, sizeof(buf32));
        for (i = 0; i < max_len; i++) {
		    ret = ist30xxc_read_buf(data->client, addr, &buf32[i], 1);
    		if (unlikely(ret)) {
	    		count = sprintf(buf, "I2C Burst read fail, addr: %x\n", addr);
		    	break;
    		}

            addr += IST30XXC_DATA_LEN;
        }

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%08x ", buf32[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		len -= max_len;
	}

    ret = ist30xxc_cmd_hold(data->client, 0);
    if (ret) {
        ist30xxc_reset(data, false);
        ist30xxc_start(data);
    }
	data->status.event_mode = true;

	ts_debug("%s", buf);

	return count;
}

/* sysfs: /sys/class/touch/tunes/node_info */
ssize_t ist30xxc_node_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int size;
    struct ist30xxc_data *data = dev_get_drvdata(dev);
   	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;


	size = sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		       data->chip_id, tsp->dir.swap_xy, tsp->ch_num.tx, 
               tsp->ch_num.rx,tsp->screen.tx, tsp->screen.rx, 
               tsp->gtx.num, tsp->gtx.ch_num[0],
		       tsp->gtx.ch_num[1], tsp->gtx.ch_num[2], tsp->gtx.ch_num[3],
		       tsp->baseline, tkey->enable, tkey->baseline, tkey->key_num,
		       tkey->ch_num[0].tx, tkey->ch_num[0].rx, tkey->ch_num[1].tx,
		       tkey->ch_num[1].rx, tkey->ch_num[2].tx, tkey->ch_num[2].rx,
		       tkey->ch_num[3].tx, tkey->ch_num[3].rx, tkey->ch_num[4].tx,
		       tkey->ch_num[4].rx, data->tags.raw_base, data->tags.filter_base);

	return size;
}

/* sysfs: /sys/class/touch/tunes/regcmd */
ssize_t ist30xxc_regcmd_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret = -1;
	u32 *buf32;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	memcpy(&ist30xxc_tunes, buf, sizeof(ist30xxc_tunes));
	buf += sizeof(ist30xxc_tunes);
	buf32 = (u32 *)buf;

	tunes_cmd_done = false;

	switch (ist30xxc_tunes.cmd) {
	case TUNES_CMD_WRITE:
		break;
	case TUNES_CMD_READ:
		break;
	case TUNES_CMD_REG_ENTER:
		ist30xxc_disable_irq(data);
		
		/* enter reg access mode */
		ret = ist30xxc_cmd_hold(data->client, 1);
		if (unlikely(ret))
			goto regcmd_fail;

		ist30xxc_reg_mode = true;
		break;
	case TUNES_CMD_REG_EXIT:
		/* exit reg access mode */
		ret = ist30xxc_cmd_hold(data->client, 0);
		if (unlikely(ret)) {
            ist30xxc_reset(data, false);
            ist30xxc_start(data);
			goto regcmd_fail;
        }

		ist30xxc_reg_mode = false;
		ist30xxc_enable_irq(data);
		break;
	default:
		ist30xxc_enable_irq(data);
		return size;
	}
	tunes_cmd_done = true;

	return size;

regcmd_fail:
	ts_err("Tunes regcmd i2c_fail, ret=%d\n", ret);
	return size;
}

ssize_t ist30xxc_regcmd_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int size;

	size = sprintf(buf, "cmd: 0x%02x, addr: 0x%08x, len: 0x%04x\n",
		       ist30xxc_tunes.cmd, ist30xxc_tunes.addr, ist30xxc_tunes.len);

	return size;
}

#define MAX_WRITE_LEN   (1)
/* sysfs: /sys/class/touch/tunes/reg */
ssize_t ist30xxc_reg_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	u32 *buf32 = (u32 *)buf;
	int waddr, wcnt = 0, len = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (unlikely(ist30xxc_tunes.cmd != TUNES_CMD_WRITE)) {
		ts_err("error, IST30XXC_REG_CMD is not correct!\n");
		return size;
	}

	if (unlikely(!ist30xxc_reg_mode)) {
		ts_err("error, IST30XXC_REG_CMD is not ready!\n");
		return size;
	}

	if (unlikely(!tunes_cmd_done)) {
		ts_err("error, IST30XXC_REG_CMD is not ready!\n");
		return size;
	}

	waddr = ist30xxc_tunes.addr;
	if (ist30xxc_tunes.len >= MAX_WRITE_LEN)
		len = MAX_WRITE_LEN;
	else
		len = ist30xxc_tunes.len;

	while (wcnt < ist30xxc_tunes.len) {
		ret = ist30xxc_write_buf(data->client, waddr, buf32, len);
		if (unlikely(ret)) {
			ts_err("Tunes regstore i2c_fail, ret=%d\n", ret);
			return size;
		}

		wcnt += len;

		if ((ist30xxc_tunes.len - wcnt) < MAX_WRITE_LEN)
			len = ist30xxc_tunes.len - wcnt;

		buf32 += MAX_WRITE_LEN;
		waddr += MAX_WRITE_LEN * IST30XXC_DATA_LEN;
	}

	tunes_cmd_done = false;

	return size;
}

ssize_t ist30xxc_reg_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
    int ret;
	int size;
#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (unlikely(ist30xxc_tunes.cmd != TUNES_CMD_READ)) {
		ts_err("error, IST30XXC_REG_CMD is not correct!\n");
		return 0;
	}

	if (unlikely(!tunes_cmd_done)) {
		ts_err("error, IST30XXC_REG_CMD is not ready!\n");
		return 0;
	}

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);          // Activated only when the GPIO I2C is used
#endif
    ret = ist30xxc_burst_read(data->client,  ist30xxc_tunes.addr, 
            (u32 *)buf, ist30xxc_tunes.len, false);
#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);       // Activated only when the GPIO I2C is used
#endif
	if (unlikely(ret)) {
		ts_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = ist30xxc_tunes.len * IST30XXC_DATA_LEN;

	tunes_cmd_done = false;

	return size;
}

/* sysfs: /sys/class/touch/tunes/adb */
ssize_t ist30xxc_adb_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	char *tmp, *ptr;
	char token[9];
	u32 cmd, addr, len, val;
	int write_len;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %x %x", &cmd, &addr, &len);
  
	switch (cmd) {
	case TUNES_CMD_WRITE:   /* write cmd */
		write_len = 0;
		ptr = (char *)(buf + 15);

		while (write_len < len) {
			memcpy(token, ptr, 8);
			token[8] = 0;
			val = simple_strtoul(token, &tmp, 16);
			ret = ist30xxc_write_buf(data->client, addr, &val, 1);
			if (unlikely(ret)) {
				ts_err("Tunes regstore i2c_fail, ret=%d\n", ret);
				return size;
			}

			ptr += 8;
			write_len++;
			addr += 4;
		}
		break;

	case TUNES_CMD_READ:   /* read cmd */
		ist30xxc_tunes.cmd = cmd;
		ist30xxc_tunes.addr = addr;
		ist30xxc_tunes.len = len;
		break;

	case TUNES_CMD_REG_ENTER:   /* enter */
		ist30xxc_disable_irq(data);

		ret = ist30xxc_cmd_hold(data->client, 1);
		if (unlikely(ret < 0)) {
			goto cmd_fail;
        }
		
        ist30xxc_reg_mode = true;
		break;

	case TUNES_CMD_REG_EXIT:   /* exit */
		if (ist30xxc_reg_mode == true) {
			ret = ist30xxc_cmd_hold(data->client, 0);
			if (unlikely(ret < 0)) {
                ist30xxc_reset(data, false);
                ist30xxc_start(data);
			    goto cmd_fail;
            }

			ist30xxc_reg_mode = false;
			ist30xxc_enable_irq(data);
		}
		break;

	default:
		break;
	}

	return size;

cmd_fail:
	ts_err("Tunes adb i2c_fail\n");
	return size;
}

ssize_t ist30xxc_adb_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret;
	int i, len, size = 0;
	char reg_val[10];
#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif
    struct ist30xxc_data *data = dev_get_drvdata(dev);

    ts_info("tunes_adb_show,%08x,%d\n", ist30xxc_tunes.addr, ist30xxc_tunes.len);

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);          // Activated only when the GPIO I2C is used
#endif
    ret = ist30xxc_burst_read(data->client, ist30xxc_tunes.addr, 
            ist30xxc_frame_buf, ist30xxc_tunes.len, false);
    if (unlikely(ret)) {
	  	ts_err("Tunes adbshow i2c_fail, ret=%d\n", ret);
	    return size;
    }    

#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);       // Activated only when the GPIO I2C is used
#endif

	size = 0;
	buf[0] = 0;
	len = sprintf(reg_val, "%08x", ist30xxc_tunes.addr);
	strcat(buf, reg_val);
	size += len;
	for (i = 0; i < ist30xxc_tunes.len; i++) {
		len = sprintf(reg_val, "%08x", ist30xxc_frame_buf[i]);
		strcat(buf, reg_val);
		size += len;
	}

	return size;
}

#if IST30XXC_ALGORITHM_MODE
/* sysfs: /sys/class/touch/tunes/algorithm */
extern u32 ist30xxc_algr_addr, ist30xxc_algr_size;
ssize_t ist30xxc_algr_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &ist30xxc_algr_addr, &ist30xxc_algr_size);
    
	ts_info("Algorithm addr: 0x%x, count: %d\n",
		 ist30xxc_algr_addr, ist30xxc_algr_size);

	ist30xxc_algr_addr |= IST30XXC_DIRECT_ACCESS;

	return size;
}

ssize_t ist30xxc_algr_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret;
	u32 algr_addr;
	int count = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	ret = ist30xxc_read_cmd(data, eHCOM_GET_ALGO_BASE, &algr_addr);
	if (unlikely(ret)) {
        ist30xxc_reset(data, false);
        ist30xxc_start(data);
		ts_warn("Algorithm mem addr read fail!\n");
		return 0;
	}

	ts_info("algr_addr(0x%x): 0x%x\n", eHCOM_GET_ALGO_BASE, algr_addr);

	count = sprintf(buf, "Algorithm addr : 0x%x\n", algr_addr);

	return count;
}
#endif // IST30XXC_ALGORITHM_MODE

/* sysfs: /sys/class/touch/tunes/intr_debug */
extern u32 int_debug_addr, int_debug_size;
ssize_t ist30xxc_intr_debug_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &int_debug_addr, &int_debug_size);
	ts_info("Interrupt debug addr: 0x%x, count: %d\n",
		 int_debug_addr, int_debug_size);

	int_debug_addr |= IST30XXC_DIRECT_ACCESS;

	return size;
}

ssize_t ist30xxc_intr_debug_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
	int count = 0;

	ts_info("intr_debug_addr(0x%x): %d\n", int_debug_addr, int_debug_size);

	count = sprintf(buf, "intr_debug_addr(0x%x): %d\n",
			int_debug_addr, int_debug_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug2 */
extern u32 int_debug2_addr, int_debug2_size;
ssize_t ist30xxc_intr_debug2_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &int_debug2_addr, &int_debug2_size);
	ts_info("Interrupt debug2 addr: 0x%x, count: %d\n",
		    int_debug2_addr, int_debug2_size);

	int_debug2_addr |= IST30XXC_DIRECT_ACCESS;

	return size;
}

ssize_t ist30xxc_intr_debug2_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
	int count = 0;

	ts_info("intr_debug2_addr(0x%x): %d\n", 
            int_debug2_addr, int_debug2_size);

	count = sprintf(buf, "intr_debug2_addr(0x%x): %d\n",
			int_debug2_addr, int_debug2_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug3 */
extern u32 int_debug3_addr, int_debug3_size;
ssize_t ist30xxc_intr_debug3_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &int_debug3_addr, &int_debug3_size);
	ts_info("Interrupt debug3 addr: 0x%x, count: %d\n",
		    int_debug3_addr, int_debug3_size);

	int_debug3_addr |= IST30XXC_DIRECT_ACCESS;

	return size;
}

ssize_t ist30xxc_intr_debug3_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
	int count = 0;

	ts_info("intr_debug3_addr(0x%x): %d\n", 
            int_debug3_addr, int_debug3_size);

	count = sprintf(buf, "intr_debug3_addr(0x%x): %d\n",
			int_debug3_addr, int_debug3_size);

	return count;
}

/* sysfs : node */
static DEVICE_ATTR(refresh, S_IRUGO, ist30xxc_frame_refresh, NULL);
static DEVICE_ATTR(nocp, S_IRUGO, ist30xxc_frame_nocp, NULL);
static DEVICE_ATTR(filter, S_IRUGO, ist30xxc_filter_show, NULL);
static DEVICE_ATTR(raw, S_IRUGO, ist30xxc_raw_show, NULL);
static DEVICE_ATTR(base, S_IRUGO, ist30xxc_base_show, NULL);
static DEVICE_ATTR(diff, S_IRUGO, ist30xxc_diff_show, NULL);

/* sysfs : sys */
static DEVICE_ATTR(debug_mode, S_IWUSR | S_IWGRP, NULL, ist30xxc_debug_mode_store);
#if IST30XXC_JIG_MODE
static DEVICE_ATTR(jig_mode, S_IWUSR | S_IWGRP , NULL, ist30xxc_jig_mode_store);
#endif
static DEVICE_ATTR(printk, S_IRUGO | S_IWUSR | S_IWGRP,
		   ist30xxc_printk_show, ist30xxc_printk_store);
static DEVICE_ATTR(printk5, S_IRUGO, ist30xxc_printk5_show, NULL);
static DEVICE_ATTR(printk6, S_IRUGO, ist30xxc_printk6_show, NULL);
static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR | S_IWGRP,
		   ist30xxc_direct_show, ist30xxc_direct_store);
static DEVICE_ATTR(clb_time, S_IWUSR | S_IWGRP, NULL, ist30xxc_calib_time_store);
static DEVICE_ATTR(clb, S_IRUGO, ist30xxc_calib_show, NULL);
static DEVICE_ATTR(clb_result, S_IRUGO, ist30xxc_calib_result_show, NULL);
static DEVICE_ATTR(tsp_power_on, S_IRUGO, ist30xxc_power_on_show, NULL);
static DEVICE_ATTR(tsp_power_off, S_IRUGO, ist30xxc_power_off_show, NULL);
static DEVICE_ATTR(errcnt, S_IWUSR | S_IWGRP, NULL, ist30xxc_errcnt_store);
static DEVICE_ATTR(scancnt, S_IWUSR | S_IWGRP, NULL, ist30xxc_scancnt_store);
static DEVICE_ATTR(timerms, S_IWUSR | S_IWGRP, NULL, ist30xxc_timerms_store);
static DEVICE_ATTR(report_rate, S_IWUSR | S_IWGRP, NULL, ist30xxc_touch_rate_store);
static DEVICE_ATTR(idle_rate, S_IWUSR | S_IWGRP, NULL, ist30xxc_idle_scan_rate_store);
static DEVICE_ATTR(mode_ta, S_IWUSR | S_IWGRP, NULL, ist30xxc_ta_mode_store);
static DEVICE_ATTR(mode_call, S_IWUSR | S_IWGRP, NULL, ist30xxc_call_mode_store);
static DEVICE_ATTR(mode_cover, S_IWUSR | S_IWGRP, NULL, ist30xxc_cover_mode_store);
static DEVICE_ATTR(max_touch, S_IWUSR | S_IWGRP, NULL, ist30xxc_touch_store);

/* sysfs : tunes */
static DEVICE_ATTR(node_info, S_IRUGO, ist30xxc_node_info_show, NULL);
static DEVICE_ATTR(regcmd, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_regcmd_show, 
        ist30xxc_regcmd_store);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR | S_IWGRP , ist30xxc_reg_show, ist30xxc_reg_store);
static DEVICE_ATTR(adb, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_adb_show, ist30xxc_adb_store);
#if IST30XXC_ALGORITHM_MODE
static DEVICE_ATTR(algorithm, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_algr_show, ist30xxc_algr_store);
#endif
static DEVICE_ATTR(intr_debug, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_intr_debug_show, 
        ist30xxc_intr_debug_store);
static DEVICE_ATTR(intr_debug2, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_intr_debug2_show, 
        ist30xxc_intr_debug2_store);
static DEVICE_ATTR(intr_debug3, S_IRUGO | S_IWUSR | S_IWGRP, ist30xxc_intr_debug3_show, 
        ist30xxc_intr_debug3_store);

static struct attribute *node_attributes[] = {
	&dev_attr_refresh.attr,
	&dev_attr_nocp.attr,
	&dev_attr_filter.attr,
	&dev_attr_raw.attr,
	&dev_attr_base.attr,
	&dev_attr_diff.attr,
	NULL,
};

static struct attribute *sys_attributes[] = {
    &dev_attr_debug_mode.attr,
#if IST30XXC_JIG_MODE
    &dev_attr_jig_mode.attr,
#endif
	&dev_attr_printk.attr,
	&dev_attr_printk5.attr,
	&dev_attr_printk6.attr,
	&dev_attr_direct.attr,
	&dev_attr_clb_time.attr,
	&dev_attr_clb.attr,
	&dev_attr_clb_result.attr,
	&dev_attr_tsp_power_on.attr,
	&dev_attr_tsp_power_off.attr,
	&dev_attr_errcnt.attr,
	&dev_attr_scancnt.attr,
	&dev_attr_timerms.attr,
	&dev_attr_report_rate.attr,
	&dev_attr_idle_rate.attr,
	&dev_attr_mode_ta.attr,
	&dev_attr_mode_call.attr,
	&dev_attr_mode_cover.attr,
	&dev_attr_max_touch.attr,
	NULL,
};

static struct attribute *tunes_attributes[] = {
	&dev_attr_node_info.attr,
	&dev_attr_regcmd.attr,
	&dev_attr_reg.attr,
	&dev_attr_adb.attr,
#if IST30XXC_ALGORITHM_MODE
	&dev_attr_algorithm.attr,
#endif
	&dev_attr_intr_debug.attr,
    &dev_attr_intr_debug2.attr,
    &dev_attr_intr_debug3.attr,
	NULL,
};

static struct attribute_group node_attr_group = {
	.attrs	= node_attributes,
};

static struct attribute_group sys_attr_group = {
	.attrs	= sys_attributes,
};

static struct attribute_group tunes_attr_group = {
	.attrs	= tunes_attributes,
};

extern struct class *ist30xxc_class;
struct device *ist30xxc_sys_dev;
struct device *ist30xxc_tunes_dev;
struct device *ist30xxc_node_dev;

int ist30xxc_init_misc_sysfs(struct ist30xxc_data *data)
{
    int frame_buf_size = IST30XXC_NODE_TOTAL_NUM * IST30XXC_DATA_LEN;

	/* /sys/class/touch/sys */
	ist30xxc_sys_dev = device_create(ist30xxc_class, NULL, 0, data, "sys");

	/* /sys/class/touch/sys/... */
	if (unlikely(sysfs_create_group(&ist30xxc_sys_dev->kobj,
					&sys_attr_group)))
		ts_err("Failed to create sysfs group(%s)!\n", "sys");

	/* /sys/class/touch/tunes */
	ist30xxc_tunes_dev = device_create(ist30xxc_class, NULL, 0, data, "tunes");

	/* /sys/class/touch/tunes/... */
	if (unlikely(sysfs_create_group(&ist30xxc_tunes_dev->kobj,
					&tunes_attr_group)))
		ts_err("Failed to create sysfs group(%s)!\n", "tunes");

	/* /sys/class/touch/node */
	ist30xxc_node_dev = device_create(ist30xxc_class, NULL, 0, data, "node");

	/* /sys/class/touch/node/... */
	if (unlikely(sysfs_create_group(&ist30xxc_node_dev->kobj,
					&node_attr_group)))
		ts_err("Failed to create sysfs group(%s)!\n", "node");

    ist30xxc_frame_buf = kmalloc(frame_buf_size, GFP_KERNEL);
	ist30xxc_frame_rawbuf = kmalloc(frame_buf_size, GFP_KERNEL);
	ist30xxc_frame_fltbuf = kmalloc(frame_buf_size, GFP_KERNEL);
	if (!ist30xxc_frame_buf || !ist30xxc_frame_rawbuf || !ist30xxc_frame_fltbuf)
		return -ENOMEM;

	return 0;
}
