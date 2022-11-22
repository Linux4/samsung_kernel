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

#if IST30XX_CHKIC
#include "ist30xx_chkic_bin.h"
#endif

#define TSP_CH_UNUSED       (0)
#define TSP_CH_SCREEN       (1)
#define TSP_CH_GTX          (2)
#define TSP_CH_KEY          (3)

#define TOUCH_NODE_PARSING_DEBUG    (0)

extern struct ist30xx_data *ts_data;

static u32 *ist30xx_frame_buf;
static u32 *ist30xx_frame_rawbuf;
static u32 *ist30xx_frame_fltbuf;

TSP_INFO ist30xx_tsp_info;
TKEY_INFO ist30xx_tkey_info;

#define TSP_INFO_SWAP_XY    (1 << 0)
#define TSP_INFO_FLIP_X     (1 << 1)
#define TSP_INFO_FLIP_Y     (1 << 2)
int ist30xx_tsp_update_info(void)
{
	int ret = 0;
	u32 tsp_lcd, tsp_swap, tsp_scr, tsp_gtx, tsp_ch_num;
	u32 tkey_info1, tkey_info2, tkey_info3;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	TKEY_INFO *tkey = &ist30xx_tkey_info;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_LCD_INFO, &tsp_lcd);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_TSP_SWAP_INFO, &tsp_swap);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_SCR_INFO, &tsp_scr);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_GTX_INFO, &tsp_gtx);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_TSP_INFO, &tsp_ch_num);
	if (unlikely(ret || !tsp_ch_num)) return ret;

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_KEY_INFO1, &tkey_info1);
	if (unlikely(ret))
		return ret;
	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_KEY_INFO2, &tkey_info2);
	if (unlikely(ret))
		return ret;
	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_KEY_INFO3, &tkey_info3);
	if (unlikely(ret))
		return ret;

	tsp->ch_num.rx = (tsp_ch_num >> 16) & 0xFFFF;
	tsp->ch_num.tx = tsp_ch_num & 0xFFFF;

	tsp->width = (tsp_lcd >> 16) & 0xFFFF;
	tsp->height = tsp_lcd & 0xFFFF;

	tsp->gtx.num = (tsp_gtx >> 24) & 0xFF;
	tsp->gtx.ch_num[0] = (tsp_gtx >> 16) & 0xFF;
	tsp->gtx.ch_num[1] = (tsp_gtx >> 8) & 0xFF;
	tsp->gtx.ch_num[2] = 0xFF;
	tsp->gtx.ch_num[3] = 0xFF;

	tsp->finger_num = IST30XX_MAX_MT_FINGERS;
	tsp->dir.swap_xy = (tsp_swap & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (tsp_swap & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (tsp_swap & TSP_INFO_FLIP_Y ? true : false);

	tsp->screen.rx = (tsp_scr >> 16) & 0xFFFF;
	tsp->screen.tx = tsp_scr & 0xFFFF;

	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;

	tkey->enable = (((tkey_info1 >> 24) & 0xFF) ? true : false);
	tkey->key_num = (tkey_info1 >> 16) & 0xFF;
	tkey->ch_num[0].tx = tkey_info1 & 0xFF;
	tkey->ch_num[0].rx = (tkey_info1 >> 8) & 0xFF;
	tkey->ch_num[1].tx = (tkey_info2 >> 16) & 0xFF;
	tkey->ch_num[1].rx = (tkey_info2 >> 24) & 0xFF;
	tkey->ch_num[2].tx = tkey_info2 & 0xFF;
	tkey->ch_num[2].rx = (tkey_info2 >> 8) & 0xFF;
	tkey->ch_num[3].tx = (tkey_info3 >> 16) & 0xFF;
	tkey->ch_num[3].rx = (tkey_info3 >> 24) & 0xFF;
	tkey->ch_num[4].tx = tkey_info3 & 0xFF;
	tkey->ch_num[4].rx = (tkey_info3 >> 8) & 0xFF;

	return ret;
}

int ist30xx_check_valid_ch(int ch_tx, int ch_rx)
{
	int i;
	TKEY_INFO *tkey = &ist30xx_tkey_info;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	if (unlikely((ch_tx >= tsp->ch_num.tx) || (ch_rx >= tsp->ch_num.rx)))
		return TSP_CH_UNUSED;

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


int ist30xx_parse_touch_node(u8 flag, struct TSP_NODE_BUF *node)
{
#if TOUCH_NODE_PARSING_DEBUG
	int j;
	TSP_INFO *tsp = &ist30xx_tsp_info;
#endif
	int i;
	u16 *raw = (u16 *)&node->raw;
	u16 *base = (u16 *)&node->base;
	u16 *filter = (u16 *)&node->filter;
	u32 *tmp_rawbuf = ist30xx_frame_rawbuf;
	u32 *tmp_fltbuf = ist30xx_frame_fltbuf;

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
	tsp_info("RAW - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->raw[i][j]);
	}

	tsp_info("BASE - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->base[i][j]);
	}

	tsp_info("FILTER - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ TSP ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->filter[i][j]);
	}
#endif

	return 0;
}

int print_touch_node(u8 flag, struct TSP_NODE_BUF *node, char *buf, bool ch_tsp)
{
	int i, j;
	int count = 0;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &ist30xx_tsp_info;

	if (tsp->dir.swap_xy) {
        for (i = 0; i < tsp->ch_num.rx; i++) {
			for (j = 0; j < tsp->ch_num.tx; j++) {
				if (ch_tsp && (ist30xx_check_valid_ch(i, j) != TSP_CH_SCREEN))
					continue;

				if (flag == NODE_FLAG_RAW)
					val = (int)node->raw[j][i];
				else if (flag == NODE_FLAG_BASE)
					val = (int)node->base[j][i];
				else if (flag == NODE_FLAG_FILTER)
					val = (int)node->filter[j][i];
				else if (flag == NODE_FLAG_DIFF)
					val = (int)(node->raw[j][i] - node->base[j][i]);
				else
					return 0;

				if (val < 0) val = 0;

                if (ist30xx_check_valid_ch(j, i) == TSP_CH_UNUSED)
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
				if (ch_tsp && (ist30xx_check_valid_ch(i, j) != TSP_CH_SCREEN))
					continue;

				if (flag == NODE_FLAG_RAW)
					val = (int)node->raw[i][j];
				else if (flag == NODE_FLAG_BASE)
					val = (int)node->base[i][j];
				else if (flag == NODE_FLAG_FILTER)
					val = (int)node->filter[i][j];
				else if (flag == NODE_FLAG_DIFF)
					val = (int)(node->raw[i][j] - node->base[i][j]);
				else
					return 0;

				if (val < 0) val = 0;

                if (ist30xx_check_valid_ch(i, j) == TSP_CH_UNUSED)
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

int parse_tsp_node(u8 flag, struct TSP_NODE_BUF *node, s16 *buf16, int mode)
{
	int i, j;
	s16 val = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	int tsp_ch_type = TSP_CH_UNUSED;

	if (unlikely((flag != NODE_FLAG_RAW) && (flag != NODE_FLAG_BASE) &&
		     (flag != NODE_FLAG_FILTER) && (flag != NODE_FLAG_DIFF)))
		return -EPERM;

    if (mode == TSP_RAW_SCREEN) {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				tsp_ch_type = ist30xx_check_valid_ch(i, j); 
				if (tsp_ch_type != TSP_CH_SCREEN && tsp_ch_type != TSP_CH_GTX)
					continue;

				switch ((int)flag) {
				case NODE_FLAG_RAW:
					val = (s16)node->raw[i][j];
					break;
				case NODE_FLAG_BASE:
					val = (s16)node->base[i][j];
					break;
				case NODE_FLAG_FILTER:
					val = (s16)node->filter[i][j];
					break;
				case NODE_FLAG_DIFF:
					val = (s16)(node->raw[i][j] - node->base[i][j]);
					break;
				}

				if (val < 0) val = 0;

				*buf16++ = val;
			}
        }
	} else if (mode == TSP_RAW_KEY) {
        for (i = 0; i < tsp->ch_num.tx; i++) {
	    	for (j = 0; j < tsp->ch_num.rx; j++) {
		    	if (ist30xx_check_valid_ch(i, j) != TSP_CH_KEY)
			    	continue;

    			switch ((int)flag) {
	    		case NODE_FLAG_RAW:
		    		val = (s16)node->raw[i][j];
			    	break;
    			case NODE_FLAG_BASE:
	    			val = (s16)node->base[i][j];
		    		break;
    			case NODE_FLAG_FILTER:
	    			val = (s16)node->filter[i][j];
		    		break;
    			case NODE_FLAG_DIFF:
	    			val = (s16)(node->raw[i][j] - node->base[i][j]);
		    		break;
			    }

    			if (val < 0) val = 0;

	    		*buf16++ = val;
		    }
		}
	}

	return 0;
}

int ist30xx_read_touch_node(u8 flag, struct TSP_NODE_BUF *node)
{
	int ret = 0;
	u32 addr = IST30XXB_RAW_ADDR;

	if (ts_data->chip_id == IST3038_CHIP_ID)
		addr += 0xD4;

	ist30xx_disable_irq(ts_data);

	if (flag & NODE_FLAG_NO_CCP) {
		ist30xx_reset(false);

		ret = ist30xx_write_cmd(ts_data->client, CMD_USE_CORRECT_CP, 0);
		if (unlikely(ret))
			goto read_tsp_node_end;
	}

	ret = ist30xx_cmd_reg(ts_data->client, CMD_ENTER_REG_ACCESS);
	if (unlikely(ret))
		goto read_tsp_node_end;

	ret = ist30xx_write_cmd(ts_data->client, IST30XX_RX_CNT_ADDR, node->len);
	if (unlikely(ret))
		goto reg_access_end;

	if (flag & (NODE_FLAG_RAW | NODE_FLAG_BASE)) {
		tsp_info("Reg addr: %x, size: %d\n", addr, node->len);
		ret = ist30xx_read_buf(ts_data->client, addr,
				       ist30xx_frame_rawbuf, node->len);
		if (unlikely(ret))
			goto reg_access_end;
	}

	if (flag & NODE_FLAG_FILTER) {
		tsp_info("Reg addr: %x, size: %d\n", IST30XXB_FILTER_ADDR, node->len);
		ret = ist30xx_read_buf(ts_data->client, IST30XXB_FILTER_ADDR,
				       ist30xx_frame_fltbuf, node->len);
		if (unlikely(ret))
			goto reg_access_end;
	}

reg_access_end:
	ist30xx_cmd_reg(ts_data->client, CMD_EXIT_REG_ACCESS);
	ist30xx_cmd_start_scan(ts_data->client);

read_tsp_node_end:
	ist30xx_enable_irq(ts_data);

	return ret;
}


/* sysfs: /sys/class/touch/node/refresh */
ssize_t ist30xx_frame_refresh(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int ret = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER;

	tsp_info("refresh\n");
   	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_read_touch_node(flag, &tsp->node);
	if (unlikely(ret)) {
        	mutex_unlock(&ist30xx_mutex);
		tsp_err("cmd 1frame raw update fail\n");
		return sprintf(buf, "FAIL\n");
	}
    	mutex_unlock(&ist30xx_mutex);

	ist30xx_parse_touch_node(flag, &tsp->node);

	return sprintf(buf, "OK\n");
}


/* sysfs: /sys/class/touch/node/read_nocp */
ssize_t ist30xx_frame_nocp(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER |
		  NODE_FLAG_NO_CCP;

    	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_read_touch_node(flag, &tsp->node);
	if (unlikely(ret)) {
		mutex_unlock(&ist30xx_mutex);
		tsp_err("cmd 1frame raw update fail\n");
		return sprintf(buf, "FAIL\n");
	}
   	mutex_unlock(&ist30xx_mutex);

	ist30xx_parse_touch_node(flag, &tsp->node);

	return sprintf(buf, "OK\n");
}


/* sysfs: /sys/class/touch/node/base */
ssize_t ist30xx_base_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb baseline(%d)\n", tsp->node.len);

	count += print_touch_node(NODE_FLAG_BASE, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/raw */
ssize_t ist30xx_raw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	int count = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb raw(%d)\n", tsp->node.len);

	count += print_touch_node(NODE_FLAG_RAW, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/diff */
ssize_t ist30xx_diff_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb difference (%d)\n", tsp->node.len);

	count += print_touch_node(NODE_FLAG_DIFF, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/filter */
ssize_t ist30xx_filter_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int count = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb filter (%d)\n", tsp->node.len);

	count += print_touch_node(NODE_FLAG_FILTER, &tsp->node, buf, false);

	return count;
}


extern int calib_ms_delay;
/* sysfs: /sys/class/touch/sys/clb_time */
ssize_t ist30xx_calib_time_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int ms_delay;

	sscanf(buf, "%d", &ms_delay);

	if (ms_delay > 10 && ms_delay < 1000) // 1sec ~ 100sec
		calib_ms_delay = ms_delay;

	tsp_info("Calibration wait time %dsec\n", calib_ms_delay / 10);

	return size;
}

/* sysfs: /sys/class/touch/sys/clb */
ssize_t ist30xx_calib_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
    	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);

	ist30xx_reset(false);

	ist30xx_calibrate(1);

	ist30xx_start(ts_data);
    	mutex_unlock(&ist30xx_mutex);

	return 0;
}

/* sysfs: /sys/class/touch/sys/clb_result */
ssize_t ist30xx_calib_result_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	int count = 0;
	u32 value;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);
	ist30xx_reset(false);

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_CALIB_RESULT, &value);
	if (unlikely(ret)) {
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	count = sprintf(buf,
			"Calibration Status : %d, Max raw gap : %d - (raw: %08x)\n",
			CALIB_TO_STATUS(value), CALIB_TO_GAP(value), value);

calib_show_end:
	ist30xx_start(ts_data);
	ist30xx_enable_irq(ts_data);
	mutex_unlock(&ist30xx_mutex);

	return count;
}

/* sysfs: /sys/class/touch/sys/power_on */
ssize_t ist30xx_power_on_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	tsp_info("Power enable: %d\n", true);

	mutex_lock(&ist30xx_mutex);
	ist30xx_internal_resume(ts_data);
	ist30xx_enable_irq(ts_data);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(ts_data);

	return 0;
}

/* sysfs: /sys/class/touch/sys/power_off */
ssize_t ist30xx_power_off_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	tsp_info("Power enable: %d\n", false);

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);
	ist30xx_internal_suspend(ts_data);
	mutex_unlock(&ist30xx_mutex);

	return 0;
}

extern int ist30xx_max_error_cnt;
/* sysfs: /sys/class/touch/sys/errcnt */
ssize_t ist30xx_errcnt_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int err_cnt;

	sscanf(buf, "%d", &err_cnt);

	if (unlikely(err_cnt < 0))
		return size;

	tsp_info("Request reset error count: %d\n", err_cnt);

	ist30xx_max_error_cnt = err_cnt;

	return size;
}

#if IST30XX_EVENT_MODE
extern int ist30xx_max_scan_retry;
/* sysfs: /sys/class/touch/sys/scancnt */
ssize_t ist30xx_scancnt_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int retry;

	sscanf(buf, "%d", &retry);

	if (unlikely(retry < 0))
		return size;

	tsp_info("Timer scan count retry: %d\n", retry);

	ist30xx_max_scan_retry = retry;

	return size;
}

extern int timer_period_ms;
/* sysfs: /sys/class/touch/sys/timerms */
ssize_t ist30xx_timerms_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ms;

	sscanf(buf, "%d", &ms);

	if (unlikely((ms < 0) || (ms > 10000)))
		return size;

	tsp_info("Timer period ms: %dms\n", ms);

	timer_period_ms = ms;

	return size;
}
#endif

extern int ist30xx_dbg_level;
/* sysfs: /sys/class/touch/sys/printk */
ssize_t ist30xx_printk_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int level;

	sscanf(buf, "%d", &level);

	if (unlikely((level < DEV_ERR) || (level > DEV_VERB)))
		return size;

	tsp_info("prink log level: %d\n", level);

	ist30xx_dbg_level = level;

	return size;
}

ssize_t ist30xx_printk_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "prink log level: %d\n", ist30xx_dbg_level);
}

/* sysfs: /sys/class/touch/sys/printk5 */
ssize_t ist30xx_printk5_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	tsp_info("prink log level:%d\n", DEV_DEBUG);

	ist30xx_dbg_level = DEV_DEBUG;

	return 0;
}

/* sysfs: /sys/class/touch/sys/printk6 */
ssize_t ist30xx_printk6_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	tsp_info("prink log level:%d\n", DEV_VERB);

	ist30xx_dbg_level = DEV_VERB;

	return 0;
}

/* sysfs: /sys/class/touch/sys/max_touch */
ssize_t ist30xx_touch_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int finger;
	int key;

	sscanf(buf, "%d %d", &finger, &key);

	tsp_info("fingers : %d, keys : %d\n", finger, key);

	ts_data->max_fingers = finger;
	ts_data->max_keys = key;

	return size;
}

extern void ist30xx_scheduled_reset(void);
extern int ist30xx_report_rate;
/* sysfs: /sys/class/touch/sys/touch_rate */
ssize_t ist30xx_touch_rate_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;

	sscanf(buf, "%d", &rate);               // us

	if (unlikely(rate > 0xFFFF))            // over 65.5ms
		return size;

	tsp_info("touch reporting rate: %d\n", rate);

	ist30xx_report_rate = rate;

	ist30xx_scheduled_reset();

	return size;
}

extern int ist30xx_idle_rate;
/* sysfs: /sys/class/touch/sys/idle_rate */
ssize_t ist30xx_idle_scan_rate_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;

	sscanf(buf, "%d", &rate);               // us

	if (unlikely(rate > 0xFFFF))            // over 65.5ms
		return size;

	tsp_info("touch idle scan rate: %d\n", rate);

	ist30xx_idle_rate = rate;

	ist30xx_scheduled_reset();

	return size;
}

extern void ist30xx_set_ta_mode(bool charging);
/* sysfs: /sys/class/touch/sys/mode_ta */
ssize_t ist30xx_ta_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xx_set_ta_mode(mode);

	return size;
}

extern void ist30xx_set_call_mode(int mode);
/* sysfs: /sys/class/touch/sys/mode_call */
ssize_t ist30xx_call_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xx_set_call_mode(mode);

	return size;
}

extern void ist30xx_set_cover_mode(int mode);
/* sysfs: /sys/class/touch/sys/mode_cover */
ssize_t ist30xx_cover_mode_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1)))   // enable/disable
		return size;

	ist30xx_set_cover_mode(mode);

	return size;
}

#if IST30XX_CHECK_BATT_TEMP
extern int ist30xx_batt_chk_max_cnt;
/* sysfs: /sys/class/touch/sys/check_temp */
ssize_t ist30xx_check_temp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int count;

	sscanf(buf, "%d", &count);

    ist30xx_batt_chk_max_cnt = count;

    tsp_info("check temp max time: %dmsec\n", count * 500);
 
	return size;
}
#endif

#define TUNES_CMD_WRITE         (1)
#define TUNES_CMD_READ          (2)
#define TUNES_CMD_REG_ENTER     (3)
#define TUNES_CMD_REG_EXIT      (4)
#define TUNES_CMD_UPDATE_PARAM  (5)
#define TUNES_CMD_UPDATE_FW     (6)

#define DIRECT_ADDR(n)          (IST30XXB_DA_ADDR(n))
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

static TUNES_INFO ist30xx_tunes;
static DIRECT_INFO ist30xx_direct;
static bool tunes_cmd_done = false;
static bool ist30xx_reg_mode = false;

/* sysfs: /sys/class/touch/sys/direct */
ssize_t ist30xxb_direct_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret = -EPERM;
	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xx_direct;

	sscanf(buf, "%c %x %x", &direct->cmd, &direct->addr, &direct->val);

	tsp_debug("Direct cmd: %c, addr: %x, val: %x\n",
		  direct->cmd, direct->addr, direct->val);

	if (unlikely((direct->cmd != DIRECT_CMD_WRITE) &&
		     (direct->cmd != DIRECT_CMD_READ))) {
		tsp_warn("Direct cmd is not correct!\n");
		return size;
	}

	if (ist30xx_intr_wait(30) < 0)
		return size;

	ts_data->status.event_mode = false;
	if (direct->cmd == DIRECT_CMD_WRITE) {
		ret = ist30xx_write_cmd(ts_data->client, DIRECT_ADDR(direct->addr),
					direct->val);
		ret = ist30xx_read_cmd(ts_data->client, DIRECT_ADDR(direct->addr),
				       &direct->val);
		tsp_debug("Direct write addr: %x, val: %x\n",
			  direct->addr, direct->val);
	}
	ts_data->status.event_mode = true;

	return size;
}

#define DIRECT_BUF_COUNT        (4)
ssize_t ist30xxb_direct_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int i, ret, count = 0;
	int len;
	u32 addr;
	u32 buf32[DIRECT_BUF_COUNT];
	int max_len = DIRECT_BUF_COUNT;
	const int msg_len = 256;
	char msg[msg_len];

	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xx_direct;

	if (unlikely(direct->cmd != DIRECT_CMD_READ))
		return sprintf(buf, "ex) echo r addr len > direct\n");

	len = direct->val;
	addr = DIRECT_ADDR(direct->addr);

	if (ist30xx_intr_wait(30) < 0)
		return 0;
	ts_data->status.event_mode = false;
	while (len > 0) {
		if (len < max_len) max_len = len;

		memset(buf32, 0, sizeof(buf32));
		ret = ist30xxb_burst_read(ts_data->client, addr, buf32, max_len);
		if (unlikely(ret)) {
			count = sprintf(buf, "I2C Burst read fail, addr: %x\n", addr);
			break;
		}

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%08x ", buf32[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		addr += max_len * IST30XX_DATA_LEN;
		len -= max_len;
	}
	ts_data->status.event_mode = true;

	tsp_debug("%s", buf);

	return count;
}

/* sysfs: /sys/class/touch/tunes/node_info */
ssize_t tunes_node_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int size;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	TKEY_INFO *tkey = &ist30xx_tkey_info;

	size = sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
		       ts_data->chip_id, tsp->dir.swap_xy, tsp->ch_num.tx, 
               tsp->ch_num.rx, tsp->screen.tx, tsp->screen.rx, 
               tsp->gtx.num, tsp->gtx.ch_num[0],
		       tsp->gtx.ch_num[1], tsp->gtx.ch_num[2], tsp->gtx.ch_num[3],
		       tsp->baseline, tkey->enable, tkey->baseline, tkey->key_num,
		       tkey->ch_num[0].tx, tkey->ch_num[0].rx, tkey->ch_num[1].tx,
		       tkey->ch_num[1].rx, tkey->ch_num[2].tx, tkey->ch_num[2].rx,
		       tkey->ch_num[3].tx, tkey->ch_num[3].rx, tkey->ch_num[4].tx,
		       tkey->ch_num[4].rx);

	return size;
}

/* sysfs: /sys/class/touch/tunes/regcmd */
ssize_t tunes_regcmd_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret = -1;
	u32 *buf32;

	memcpy(&ist30xx_tunes, buf, sizeof(ist30xx_tunes));
	buf += sizeof(ist30xx_tunes);
	buf32 = (u32 *)buf;

	tunes_cmd_done = false;

	switch (ist30xx_tunes.cmd) {
	case TUNES_CMD_WRITE:
		break;
	case TUNES_CMD_READ:
		break;
	case TUNES_CMD_REG_ENTER:
        	mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(ts_data);
		ist30xx_reset(false);

		/* enter reg access mode */
		ret = ist30xx_cmd_reg(ts_data->client, CMD_ENTER_REG_ACCESS);
		if (unlikely(ret)) {
           		mutex_unlock(&ist30xx_mutex);
			goto regcmd_fail;
      		}

        	mutex_unlock(&ist30xx_mutex);
		ist30xx_reg_mode = true;

		break;
	case TUNES_CMD_REG_EXIT:
		/* exit reg access mode */
		ret = ist30xx_cmd_reg(ts_data->client, CMD_EXIT_REG_ACCESS);
		if (unlikely(ret))
			goto regcmd_fail;

		ret = ist30xx_cmd_start_scan(ts_data->client);
		if (unlikely(ret))
			goto regcmd_fail;

		ist30xx_reg_mode = false;

		ist30xx_enable_irq(ts_data);
		break;
	default:
		ist30xx_enable_irq(ts_data);
		return size;
	}
	tunes_cmd_done = true;

	return size;

regcmd_fail:
	tsp_err("Tunes regcmd i2c_fail, ret=%d\n", ret);
	return size;
}

ssize_t tunes_regcmd_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int size;

	size = sprintf(buf, "cmd: 0x%02x, addr: 0x%08x, len: 0x%04x\n",
		       ist30xx_tunes.cmd, ist30xx_tunes.addr, ist30xx_tunes.len);

	return size;
}

#define MAX_WRITE_LEN   (1)
/* sysfs: /sys/class/touch/tunes/reg */
ssize_t tunes_reg_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	u32 *buf32 = (u32 *)buf;
	int waddr, wcnt = 0, len = 0;

	if (unlikely(ist30xx_tunes.cmd != TUNES_CMD_WRITE)) {
		tsp_err("error, IST30XX_REG_CMD is not correct!\n");
		return size;
	}

	if (unlikely(!ist30xx_reg_mode)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return size;
	}

	if (unlikely(!tunes_cmd_done)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return size;
	}

	waddr = ist30xx_tunes.addr;
	if (ist30xx_tunes.len >= MAX_WRITE_LEN)
		len = MAX_WRITE_LEN;
	else
		len = ist30xx_tunes.len;

	while (wcnt < ist30xx_tunes.len) {
		ret = ist30xx_write_buf(ts_data->client, waddr, buf32, len);
		if (unlikely(ret)) {
			tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
			return size;
		}

		wcnt += len;

		if ((ist30xx_tunes.len - wcnt) < MAX_WRITE_LEN)
			len = ist30xx_tunes.len - wcnt;

		buf32 += MAX_WRITE_LEN;
		waddr += MAX_WRITE_LEN * IST30XX_DATA_LEN;
	}

	tunes_cmd_done = false;

	return size;
}

ssize_t tunes_reg_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret;
	int size;
	u32 *buf32 = (u32 *)buf;

#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif

	if (unlikely(ist30xx_tunes.cmd != TUNES_CMD_READ)) {
		tsp_err("error, IST30XX_REG_CMD is not correct!\n");
		return 0;
	}

	if (unlikely(!tunes_cmd_done)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return 0;
	}

	size = ist30xx_tunes.len;
	ret = ist30xx_write_cmd(ts_data->client, IST30XX_RX_CNT_ADDR, size);
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return 0;
	}

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);          // Activated only when the GPIO I2C is used
#endif
	ret = ist30xx_read_buf(ts_data->client, ist30xx_tunes.addr, buf32, size);
#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);       // Activated only when the GPIO I2C is used
#endif
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = ist30xx_tunes.len * IST30XX_DATA_LEN;

	tunes_cmd_done = false;

	return size;
}

/* sysfs: /sys/class/touch/tunes/adb */
ssize_t tunes_adb_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	char *tmp, *ptr;
	char token[9];
	u32 cmd, addr, len, val;
	int write_len;

	sscanf(buf, "%x %x %x", &cmd, &addr, &len);

	switch (cmd) {
	case TUNES_CMD_WRITE:   /* write cmd */
		write_len = 0;
		ptr = (char *)(buf + 15);

		while (write_len < len) {
			memcpy(token, ptr, 8);
			token[8] = 0;
			val = simple_strtoul(token, &tmp, 16);
			ret = ist30xx_write_buf(ts_data->client, addr, &val, 1);
			if (unlikely(ret)) {
				tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
				return size;
			}

			ptr += 8;
			write_len++;
			addr += 4;
		}
		break;

	case TUNES_CMD_READ:   /* read cmd */
		ist30xx_tunes.cmd = cmd;
		ist30xx_tunes.addr = addr;
		ist30xx_tunes.len = len;
		break;

	case TUNES_CMD_REG_ENTER:   /* enter */
                mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(ts_data);
		ist30xx_reset(false);

		ret = ist30xx_cmd_reg(ts_data->client, CMD_ENTER_REG_ACCESS);
		if (unlikely(ret < 0)) {
                        mutex_unlock(&ist30xx_mutex);
			goto cmd_fail;
                }
                mutex_unlock(&ist30xx_mutex);
		ist30xx_reg_mode = true;
		break;

	case TUNES_CMD_REG_EXIT:   /* exit */
		if (ist30xx_reg_mode == true) {
			ret = ist30xx_cmd_reg(ts_data->client, CMD_EXIT_REG_ACCESS);
			if (unlikely(ret < 0))
				goto cmd_fail;

			ret = ist30xx_cmd_start_scan(ts_data->client);
			if (unlikely(ret < 0))
				goto cmd_fail;
			ist30xx_reg_mode = false;
			ist30xx_enable_irq(ts_data);
		}
		break;

	default:
		break;
	}

	return size;

cmd_fail:
	tsp_err("Tunes adb i2c_fail\n");
	return size;
}

ssize_t tunes_adb_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret;
	int i, len, size = 0;
	char reg_val[10];

#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif

	ret = ist30xx_write_cmd(ts_data->client, IST30XX_RX_CNT_ADDR,
				ist30xx_tunes.len);
	if (unlikely(ret)) {
		tsp_err("Tunes adbshow i2c_fail, ret=%d\n", ret);
		return size;
	}

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);
#endif
	ret = ist30xx_read_buf(ts_data->client, ist30xx_tunes.addr,
			       ist30xx_frame_buf, ist30xx_tunes.len);
#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);
#endif
	if (unlikely(ret)) {
		tsp_err("Tunes adbshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = 0;
	buf[0] = 0;
	len = sprintf(reg_val, "%08x", ist30xx_tunes.addr);
	strcat(buf, reg_val);
	size += len;
	for (i = 0; i < ist30xx_tunes.len; i++) {
		len = sprintf(reg_val, "%08x", ist30xx_frame_buf[i]);
		strcat(buf, reg_val);
		size += len;
	}

	return size;
}

#if IST30XX_ALGORITHM_MODE
/* sysfs: /sys/class/touch/tunes/algorithm */
extern u32 ist30xx_algr_addr, ist30xx_algr_size;
ssize_t ist30xx_algr_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &ist30xx_algr_addr, &ist30xx_algr_size);
    
	tsp_info("Algorithm addr: 0x%x, count: %d\n",
		 ist30xx_algr_addr, ist30xx_algr_size);

	ist30xx_algr_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t ist30xx_algr_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret;
	u32 algr_addr;
	int count = 0;

	ret = ist30xx_read_cmd(ts_data->client, IST30XXB_MEM_ALGORITHM, &algr_addr);
	if (unlikely(ret)) {
		tsp_warn("Algorithm mem addr read fail!\n");
		return 0;
	}

	tsp_info("algr_addr(0x%x): 0x%x\n", IST30XXB_MEM_ALGORITHM, algr_addr);

	count = sprintf(buf, "Algorithm addr : 0x%x\n", algr_addr);

	return count;
}
#endif // IST30XX_ALGORITHM_MODE

/* sysfs: /sys/class/touch/tunes/intr_debug */
extern u32 intr_debug_addr, intr_debug_size;
ssize_t intr_debug_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &intr_debug_addr, &intr_debug_size);
	tsp_info("Interrupt debug addr: 0x%x, count: %d\n",
		 intr_debug_addr, intr_debug_size);

	intr_debug_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t intr_debug_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int count = 0;

	tsp_info("intr_debug_addr(0x%x): %d\n", intr_debug_addr, intr_debug_size);

	count = sprintf(buf, "intr_debug_addr(0x%x): %d\n",
			intr_debug_addr, intr_debug_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug2 */
extern u32 intr_debug2_addr, intr_debug2_size;
ssize_t intr_debug2_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &intr_debug2_addr, &intr_debug2_size);
	tsp_info("Interrupt debug2 addr: 0x%x, count: %d\n",
		    intr_debug2_addr, intr_debug2_size);

	intr_debug2_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t intr_debug2_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int count = 0;

	tsp_info("intr_debug2_addr(0x%x): %d\n", 
            intr_debug2_addr, intr_debug2_size);

	count = sprintf(buf, "intr_debug2_addr(0x%x): %d\n",
			intr_debug2_addr, intr_debug2_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug3 */
extern u32 intr_debug3_addr, intr_debug3_size;
ssize_t intr_debug3_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	sscanf(buf, "%x %d", &intr_debug3_addr, &intr_debug3_size);
	tsp_info("Interrupt debug3 addr: 0x%x, count: %d\n",
		    intr_debug3_addr, intr_debug3_size);

	intr_debug3_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t intr_debug3_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int count = 0;

	tsp_info("intr_debug3_addr(0x%x): %d\n", 
            intr_debug3_addr, intr_debug3_size);

	count = sprintf(buf, "intr_debug3_addr(0x%x): %d\n",
			intr_debug3_addr, intr_debug3_size);

	return count;
}

#if IST30XX_CHKIC
u8 *ts_chkic_bin = NULL;
u32 ts_chkic_bin_size = 0;
u32 ts_chkic_result = 0;

int chkic_ms_delay = IST30XX_CHKIC_WAIT;
int ist30xx_chkic_wait(void)
{
	int cnt = chkic_ms_delay;

	ts_data->status.chkic_msg = 0;
	while (cnt-- > 0) {
		msleep(100);

		if (ts_data->status.chkic_msg) {
			if (ts_data->status.chkic_msg == IST30XX_CHKIC_END)
				return 0;
			else
				return -EAGAIN;
		}
	}
	tsp_warn("Fault test time out\n");

	return -EPERM;
}

#define chkic_next_step(ret)   { if (unlikely(ret)) goto end; msleep(5); }
int ist30xx_chkic_test(const u8 *buf, int size)
{
	int ret;
	int len;
	u32 chksum = 0;
	u32 chkic_chksum = 0;
	u32 *buf32;
	struct i2c_client *client = (struct i2c_client *)ts_data->client;

	tsp_info("*** Check IC ***\n");

	/* Result initial */
	ts_chkic_result = 0;

	/* Parse chkicsum */
	chkic_chksum = *((u32 *)&buf[size - IST30XX_CHECKSUM_SIZE]);
	tsp_verb("Read binary info - chksum: 0x%08x size: 0x%x\n",
		 chkic_chksum, size);

	ist30xx_disable_irq(ts_data);

	ist30xx_reset(false);

	/* Load IC chkic test code */
	ret = ist30xx_write_cmd(client, CMD_EXEC_MEM_CODE, 0);
	chkic_next_step(ret);

	buf32 = (u32 *)buf;
	len = size / IST30XX_DATA_LEN;
	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	ret = ist30xx_write_buf(client, len, buf32, len);
	chkic_next_step(ret);

	/* Check load end */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	chkic_next_step(ret);
	if (chksum != IST30XX_CHKIC_LOAD_END)
		goto end;
	tsp_info("Check ic code ready!!\n");

	/* Check chksum */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	chkic_next_step(ret);
	tsp_info("Check ic chksum: %08x, %08x\n", chksum, chkic_chksum);

	ist30xx_enable_irq(ts_data);
	/* Wait IC chkic result */
	if (ist30xx_chkic_wait() != 0)
		tsp_info("Check ic timeout!!\n");

	ist30xx_disable_irq(ts_data);

	/* Read IC chkic result */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &ts_chkic_result);
	chkic_next_step(ret);
	tsp_info("Read result value: %08x\n", ts_chkic_result);

	ist30xx_reset(false);

	ist30xx_start(ts_data);

end:
	if (unlikely(ret)) {
		tsp_warn("Check ic error, ret=%d\n", ret);
	} else if (unlikely(chksum != chkic_chksum)) {
		tsp_warn("Chksum error, chksum=%x(%x)\n", chksum, chkic_chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(ts_data);

	return ret;
}

/* sysfs: /sys/class/touch/chkic/chkic */
static ssize_t ist30xx_chkic_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;

	if ((ts_chkic_bin == NULL) || (ts_chkic_bin_size == 0))
		return sprintf(buf, "Binary is not correct(%d)\n", ts_chkic_bin_size);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_chkic_test(ts_chkic_bin, ts_chkic_bin_size);
	mutex_unlock(&ist30xx_mutex);

	if (ts_chkic_result == IST30XX_CHKIC_OK)
		return sprintf(buf, "Check IC : OK");
	else if (ts_chkic_result == IST30XX_CHKIC_FAIL)
		return sprintf(buf, "Check IC : FAIL");
	else
		return sprintf(buf, "Check IC : ERROR");
}
#endif

#if IST30XX_NODE_CAP
#define MAX_CAP_TH		150
static ssize_t ist30xx_check_node_cap(char* msgbuf, u32* rawbuf, size_t rawsize)
{
	int			ret = -1;
	int			i, j; 
	char		msg[128] = {0,};
	u32			max_cap = MAX_CAP_TH;
	u32			value;
	ssize_t		size = 0;
	TSP_INFO*	tsp = &ist30xx_tsp_info;
	bool		success = true;
	u32			addr = IST30XXB_RAW_ADDR;
	
	if (ts_data->chip_id == IST3038_CHIP_ID)
		addr += 0xD4;

	ist30xx_reset(false);
	ret = ist30xx_write_cmd(ts_data->client, CMD_MODE_FOR_JIG, 0x41);
	if (ret != 0) {
		tsp_err("%s:%d failed\n", __FUNCTION__, __LINE__);
		goto check_node_cap_err;
	}

	msleep(6000);

	ret = ist30xxb_burst_read(ts_data->client, IST30XXB_DA_ADDR(addr), rawbuf, rawsize);
	if (ret != 0) {
		tsp_err("%s:%d failed\n", __FUNCTION__, __LINE__);
		goto check_node_cap_err;
	}

	for (i = 0; i < tsp->screen.tx; i++) {
		for (j = 0; j < tsp->screen.rx; j++) {
			value = *(rawbuf + (tsp->ch_num.rx * i + j));

			if (value > max_cap) {
				success = false;
				max_cap = value;
			}
		}
	}	

	if (success == true)
		size += sprintf(msg, "\nPass\n");
	else	
		size += sprintf(msg, "\nFail Cap(%d)\n", max_cap);

	strcat(msgbuf, msg);
	return size;

check_node_cap_err:
	size += sprintf(msg, "\n%s\n", "abnormal error occurred");
	strcpy(msgbuf, msg);
	return size;
}

static ssize_t ist30xx_print_node_cap(char* msgbuf, u32* rawbuf, size_t rawsize)
{
	int			ret = -1;
	int			i, j, max_rx, max_tx;
	char		msg[128] = {0,};
	u32			max_cap = 0;
	u32			value;
	ssize_t		size = 0;
	TSP_INFO*	tsp = &ist30xx_tsp_info;
	u32			addr = IST30XXB_RAW_ADDR;
	
	if (ts_data->chip_id == IST3038_CHIP_ID)
		addr += 0xD4;

	ist30xx_reset(false);
	ret = ist30xx_write_cmd(ts_data->client, CMD_MODE_FOR_JIG, 0x41);
	if (ret != 0) {
		tsp_err("%s:%d failed\n", __FUNCTION__, __LINE__);
		goto print_node_cap_err;
	}

	msleep(6000);

	ret = ist30xxb_burst_read(ts_data->client, IST30XXB_DA_ADDR(addr), rawbuf, rawsize);
	if (ret != 0) {
		tsp_err("%s:%d failed\n", __FUNCTION__, __LINE__);
		goto print_node_cap_err;
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			value = *(rawbuf + (tsp->ch_num.rx * i + j));
			if (value > max_cap) {
				max_cap = value;
				max_rx = j;
				max_tx = i;
			}

			size += sprintf(msg, "%4d", *(rawbuf + (tsp->ch_num.rx * i + j)));
			strcat(msgbuf, msg);	
		}
		strcat(msgbuf, "\n");
		size++;
	}	

	size += sprintf(msg, "\nMax Cap Value = %d CH (%d,%d)\n", max_cap, max_tx, max_rx);
	strcat(msgbuf, msg);
	return size;

print_node_cap_err:
	size += sprintf(msg, "\n%s\n", "abnormal error occurred");
	strcpy(msgbuf, msg);
	return size;
}

/* sysfs: /sys/class/touch/node/chk_cap */
static ssize_t ist30xx_chk_cap_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t size;

	ist30xx_disable_irq(ts_data);

	mutex_lock(&ist30xx_mutex);
	size = ist30xx_check_node_cap(buf, ist30xx_frame_rawbuf, NODE_TOTAL_NUM);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_reset(false);
	ist30xx_start(ts_data);
	ist30xx_enable_irq(ts_data);
	
	return size;
}


/* sysfs: /sys/class/touch/node/pri_cap */
static ssize_t ist30xx_pri_cap_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t size;

	ist30xx_disable_irq(ts_data);

	mutex_lock(&ist30xx_mutex);
	size = ist30xx_print_node_cap(buf, ist30xx_frame_rawbuf, NODE_TOTAL_NUM);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_reset(false);
	ist30xx_start(ts_data);
	ist30xx_enable_irq(ts_data);
	
	return size;
}
#endif
/* sysfs : node */
static DEVICE_ATTR(refresh, S_IRUGO | S_IWUSR, ist30xx_frame_refresh, NULL);
static DEVICE_ATTR(nocp, S_IRUGO | S_IWUSR, ist30xx_frame_nocp, NULL);
static DEVICE_ATTR(filter, S_IRUGO | S_IWUSR, ist30xx_filter_show, NULL);
static DEVICE_ATTR(raw, S_IRUGO | S_IWUSR, ist30xx_raw_show, NULL);
static DEVICE_ATTR(base, S_IRUGO | S_IWUSR, ist30xx_base_show, NULL);
static DEVICE_ATTR(diff, S_IRUGO | S_IWUSR, ist30xx_diff_show, NULL);

/* sysfs : sys */
static DEVICE_ATTR(printk, S_IRUGO | S_IWUSR,
		   ist30xx_printk_show, ist30xx_printk_store);
static DEVICE_ATTR(printk5, S_IRUGO | S_IWUSR, ist30xx_printk5_show, NULL);
static DEVICE_ATTR(printk6, S_IRUGO | S_IWUSR, ist30xx_printk6_show, NULL);
static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR,
		   ist30xxb_direct_show, ist30xxb_direct_store);
static DEVICE_ATTR(clb_time, S_IRUGO | S_IWUSR, NULL, ist30xx_calib_time_store);
static DEVICE_ATTR(clb, S_IRUGO | S_IWUSR, ist30xx_calib_show, NULL);
static DEVICE_ATTR(clb_result, S_IRUGO | S_IWUSR, ist30xx_calib_result_show, NULL);
static DEVICE_ATTR(tsp_power_on, S_IRUGO | S_IWUSR, ist30xx_power_on_show, NULL);
static DEVICE_ATTR(tsp_power_off, S_IRUGO | S_IWUSR, ist30xx_power_off_show, NULL);
static DEVICE_ATTR(errcnt, S_IRUGO | S_IWUSR, NULL, ist30xx_errcnt_store);
#if IST30XX_EVENT_MODE
static DEVICE_ATTR(scancnt, S_IRUGO | S_IWUSR, NULL, ist30xx_scancnt_store);
static DEVICE_ATTR(timerms, S_IRUGO | S_IWUSR, NULL, ist30xx_timerms_store);
#endif
static DEVICE_ATTR(report_rate, S_IRUGO | S_IWUSR, NULL, ist30xx_touch_rate_store);
static DEVICE_ATTR(idle_rate, S_IRUGO | S_IWUSR, NULL, ist30xx_idle_scan_rate_store);
static DEVICE_ATTR(mode_ta, S_IRUGO | S_IWUSR, NULL, ist30xx_ta_mode_store);
static DEVICE_ATTR(mode_call, S_IRUGO | S_IWUSR, NULL, ist30xx_call_mode_store);
static DEVICE_ATTR(mode_cover, S_IRUGO | S_IWUSR, NULL, ist30xx_cover_mode_store);
#if IST30XX_CHECK_BATT_TEMP
static DEVICE_ATTR(check_temp, S_IRUGO | S_IWUSR, NULL, ist30xx_check_temp_store);
#endif
static DEVICE_ATTR(max_touch, S_IRUGO | S_IWUSR, NULL, ist30xx_touch_store);

/* sysfs : tunes */
static DEVICE_ATTR(node_info, S_IRUGO | S_IWUSR, tunes_node_info_show, NULL);
static DEVICE_ATTR(regcmd, S_IRUGO | S_IWUSR, tunes_regcmd_show, tunes_regcmd_store);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR, tunes_reg_show, tunes_reg_store);
static DEVICE_ATTR(adb, S_IRUGO | S_IWUSR, tunes_adb_show, tunes_adb_store);
#if IST30XX_ALGORITHM_MODE
static DEVICE_ATTR(algorithm, S_IRUGO | S_IWUSR, ist30xx_algr_show, ist30xx_algr_store);
#endif
static DEVICE_ATTR(intr_debug, S_IRUGO | S_IWUSR, intr_debug_show, intr_debug_store);
static DEVICE_ATTR(intr_debug2, S_IRUGO | S_IWUSR, intr_debug2_show, intr_debug2_store);
static DEVICE_ATTR(intr_debug3, S_IRUGO | S_IWUSR, intr_debug3_show, intr_debug3_store);

#if IST30XX_CHKIC
/* sysfs : chkic */
static DEVICE_ATTR(chkic, S_IRUGO | S_IWUSR, ist30xx_chkic_show, NULL);
#endif

#if IST30XX_NODE_CAP
static DEVICE_ATTR(pri_cap, S_IRUGO | S_IWUSR, ist30xx_pri_cap_show, NULL);
static DEVICE_ATTR(chk_cap, S_IRUGO | S_IWUSR, ist30xx_chk_cap_show, NULL);
#endif

static struct attribute *node_attributes[] = {
	&dev_attr_refresh.attr,
	&dev_attr_nocp.attr,
	&dev_attr_filter.attr,
	&dev_attr_raw.attr,
	&dev_attr_base.attr,
	&dev_attr_diff.attr,
#if IST30XX_NODE_CAP
	&dev_attr_pri_cap.attr,
	&dev_attr_chk_cap.attr,
#endif
	NULL,
};

static struct attribute *sys_attributes[] = {
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
#if IST30XX_EVENT_MODE
	&dev_attr_scancnt.attr,
	&dev_attr_timerms.attr,
#endif
	&dev_attr_report_rate.attr,
	&dev_attr_idle_rate.attr,
	&dev_attr_mode_ta.attr,
	&dev_attr_mode_call.attr,
	&dev_attr_mode_cover.attr,
#if IST30XX_CHECK_BATT_TEMP
    &dev_attr_check_temp.attr,
#endif
	&dev_attr_max_touch.attr,
	NULL,
};

static struct attribute *tunes_attributes[] = {
	&dev_attr_node_info.attr,
	&dev_attr_regcmd.attr,
	&dev_attr_reg.attr,
	&dev_attr_adb.attr,
#if IST30XX_ALGORITHM_MODE
	&dev_attr_algorithm.attr,
#endif
	&dev_attr_intr_debug.attr,
    &dev_attr_intr_debug2.attr,
    &dev_attr_intr_debug3.attr,
	NULL,
};

#if IST30XX_CHKIC
static struct attribute *chkic_attributes[] = {
	&dev_attr_chkic.attr,
	NULL,
};
#endif

static struct attribute_group node_attr_group = {
	.attrs	= node_attributes,
};

static struct attribute_group sys_attr_group = {
	.attrs	= sys_attributes,
};

static struct attribute_group tunes_attr_group = {
	.attrs	= tunes_attributes,
};

#if IST30XX_CHKIC
static struct attribute_group chkic_attr_group = {
	.attrs	= chkic_attributes,
};
#endif

extern struct class *ist30xx_class;
struct device *ist30xx_sys_dev;
struct device *ist30xx_tunes_dev;
struct device *ist30xx_node_dev;
#if IST30XX_CHKIC
struct device *ist30xx_chkic_dev;
#endif

int ist30xx_init_misc_sysfs(void)
{
	/* /sys/class/touch/sys */
	ist30xx_sys_dev = device_create(ist30xx_class, NULL, 0, NULL, "sys");

	/* /sys/class/touch/sys/... */
	if (unlikely(sysfs_create_group(&ist30xx_sys_dev->kobj,
					&sys_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "sys");

	/* /sys/class/touch/tunes */
	ist30xx_tunes_dev = device_create(ist30xx_class, NULL, 0, NULL, "tunes");

	/* /sys/class/touch/tunes/... */
	if (unlikely(sysfs_create_group(&ist30xx_tunes_dev->kobj,
					&tunes_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "tunes");

	/* /sys/class/touch/node */
	ist30xx_node_dev = device_create(ist30xx_class, NULL, 0, NULL, "node");

	/* /sys/class/touch/node/... */
	if (unlikely(sysfs_create_group(&ist30xx_node_dev->kobj,
					&node_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "node");

#if IST30XX_CHKIC
	/* /sys/class/touch/chkic */
	ist30xx_chkic_dev = device_create(ist30xx_class, NULL, 0, NULL, "chkic");

	/* /sys/class/touch/chkic/... */
	if (unlikely(sysfs_create_group(&ist30xx_chkic_dev->kobj,
					&chkic_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "chkic");

	ts_chkic_bin = (u8 *)ist30xxb_chkic;
	ts_chkic_bin_size = sizeof(ist30xxb_chkic);
#endif

	ist30xx_frame_buf = kmalloc(4096, GFP_KERNEL);
	ist30xx_frame_rawbuf = kmalloc(4096, GFP_KERNEL);
	ist30xx_frame_fltbuf = kmalloc(4096, GFP_KERNEL);
	if (!ist30xx_frame_buf || !ist30xx_frame_rawbuf || !ist30xx_frame_fltbuf)
		return -ENOMEM;

	return 0;
}
