/* drivers/input/touchscreen/sec_ts.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/sec_sysfs.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <linux/of_gpio.h>

#include <linux/time.h>

#include "sec_ts.h"

#ifdef CONFIG_OF
#ifndef USE_OPEN_CLOSE
#define USE_OPEN_CLOSE
#undef CONFIG_HAS_EARLYSUSPEND
#undef CONFIG_PM
#endif
#endif

static struct device *sec_ts_dev;
EXPORT_SYMBOL(sec_ts_dev);

struct sec_ts_fw_file {
	u8* data;
	u32 pos;
	size_t size;
};

struct sec_ts_event_status{

	u8 tchsta:3;
	u8 ttype:3;
	u8 eid:2;

	u8 sid;

	u8 buff2;
	u8 buff3;
	u8 buff4;
	u8 buff5;
//	u8 buff6;
//	u8 buff7;
} __attribute__ ((packed));

struct sec_ts_gesture_status{
        u8 stype:6;
        u8 eid:2;
        u8 scode;
        u8 gesture;
        u8 y_4_2:3;
	u8 x:5;
	u8 h_4:1;
	u8 w:5;
	u8 y_1_0:2;
	u8 reserved:4;
	u8 h_3_0:4;
} __attribute__ ((packed));

#ifdef CONFIG_TOUCHSCREEN_SEC_TS_GLOVEMODE
struct sec_ts_exp_fn {
        int (*func_init)(void *device_data);
        void (*func_remove)(void);
};
#endif

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev);
static void sec_ts_input_close(struct input_dev *dev);
#ifdef USE_OPEN_DWORK
static void sec_ts_open_work(struct work_struct *work);
#endif
#endif

static int sec_ts_stop_device(struct sec_ts_data *ts);
static int sec_ts_start_device(struct sec_ts_data *ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_ts_early_suspend(struct early_suspend *h);
static void sec_ts_late_resume(struct early_suspend *h);
#endif
void sec_ts_release_all_finger(struct sec_ts_data *ts);

static int regshow_size = 0;
u8* fw_write_buffer;
u8 lv1cmd;
struct timespec eventtime_buf[256];
int eventtime_counter;
u8* read_lv1_buff;
static int lv1_readsize;
static int lv1_readremain;
static int lv1_readoffset;

static ssize_t sec_ts_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_bulk_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_fw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_setreadsize_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_setlv1cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static inline ssize_t sec_ts_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sec_ts_enter_recovery_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t sec_ts_id_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_status_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_selfraw_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_read_lv3_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_raw_channel_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_regread_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_touchtype_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_gesture_status_show(struct device *dev,
        struct device_attribute *attr, char *buf);
static ssize_t sec_ts_dd_version_show(struct device *dev,
        struct device_attribute *attr, char *buf);
static ssize_t sec_ts_read_bulk_show(struct device *dev,
        struct device_attribute *attr, char *buf);
static ssize_t sec_ts_read_eventtime_show(struct device *dev,
        struct device_attribute *attr, char *buf);
static inline ssize_t sec_ts_show_error(struct device *dev,
	struct device_attribute *attr, char *buf);


static DEVICE_ATTR(sec_ts_reg, 0666, NULL, sec_ts_reg_store);
static DEVICE_ATTR(sec_ts_bulk,0666, NULL, sec_ts_bulk_store);
static DEVICE_ATTR(sec_ts_fw, 0666, NULL, sec_ts_fw_store);
static DEVICE_ATTR(sec_ts_setreadsize, 0666, NULL, sec_ts_setreadsize_store);
static DEVICE_ATTR(sec_ts_setlv1cmd, 0666, NULL, sec_ts_setlv1cmd_store);
static DEVICE_ATTR(sec_ts_enter_recovery, 0666, NULL, sec_ts_enter_recovery_store);

static DEVICE_ATTR(sec_ts_id, 0666, sec_ts_id_show, NULL);
static DEVICE_ATTR(sec_ts_fw_ver, 0666, sec_ts_fw_ver_show, NULL);
static DEVICE_ATTR(sec_ts_status, 0666, sec_ts_status_show, NULL);
static DEVICE_ATTR(sec_ts_raw, 0666, sec_ts_raw_show, NULL);
static DEVICE_ATTR(sec_ts_selfraw, 0666, sec_ts_selfraw_show, NULL);
static DEVICE_ATTR(sec_ts_read_lv3, 0666, sec_ts_read_lv3_show, NULL);
static DEVICE_ATTR(sec_ts_raw_channel, 0666, sec_ts_raw_channel_show, NULL);
static DEVICE_ATTR(sec_ts_regread, 0666, sec_ts_regread_show, NULL);
static DEVICE_ATTR(sec_ts_touchtype, 0666, sec_ts_touchtype_show, NULL);
static DEVICE_ATTR(sec_ts_gesture_status, 0666, sec_ts_gesture_status_show, NULL);
static DEVICE_ATTR(sec_ts_dd_version, 0666, sec_ts_dd_version_show, NULL);
static DEVICE_ATTR(sec_ts_read_bulk, 0666, sec_ts_read_bulk_show, NULL);
static DEVICE_ATTR(sec_ts_read_eventtime, 0666, sec_ts_read_eventtime_show, NULL);

static struct attribute *cmd_attributes[] = {
        &dev_attr_sec_ts_reg.attr,
        &dev_attr_sec_ts_bulk.attr,
        &dev_attr_sec_ts_fw.attr,
        &dev_attr_sec_ts_setreadsize.attr,
	&dev_attr_sec_ts_setlv1cmd.attr,
	&dev_attr_sec_ts_enter_recovery.attr,
	&dev_attr_sec_ts_id.attr,
        &dev_attr_sec_ts_fw_ver.attr,
	&dev_attr_sec_ts_status.attr,
	&dev_attr_sec_ts_raw.attr,
	&dev_attr_sec_ts_selfraw.attr,
	&dev_attr_sec_ts_read_lv3.attr,
	&dev_attr_sec_ts_raw_channel.attr,
	&dev_attr_sec_ts_regread.attr,
	&dev_attr_sec_ts_touchtype.attr,
	&dev_attr_sec_ts_gesture_status.attr,
	&dev_attr_sec_ts_dd_version.attr,
	&dev_attr_sec_ts_read_bulk.attr,
	&dev_attr_sec_ts_read_eventtime.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
        .attrs = cmd_attributes,
};

static inline ssize_t sec_ts_show_error(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	tsp_debug_err(true, &ts->client->dev, "sec_ts :%s read only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

static inline ssize_t sec_ts_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	tsp_debug_err(true, &ts->client->dev, "sec_ts :%s write only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

int sec_ts_i2c_write(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[I2C_WRITE_BUFFER_SIZE + 1];
	int ret;
	unsigned char retry;
	struct i2c_msg msg;

	if (len > I2C_WRITE_BUFFER_SIZE)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_i2c_write len is larger than buffer size\n");
		return -1;
	}

	if(ts->power_status == SEC_TS_STATE_POWER_OFF)
        {
                tsp_debug_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF \n",__func__);
                goto err;
        }

	buf[0] = reg;
	memcpy(buf+1, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = buf;
	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < 10; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1) {
			break;
		}
		tsp_debug_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
	}
	mutex_unlock(&ts->i2c_mutex);
	if (retry == 10) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
	}

	if (ret == 1)
		return 0;
err:
	return -EIO;
}

int sec_ts_i2c_read(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[4];
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];

        if(ts->power_status == SEC_TS_STATE_POWER_OFF)
        {
                tsp_debug_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF \n",__func__);
                goto err;
        }

	buf[0] = reg;

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;
	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < 10; retry++) {
		if((ret = i2c_transfer(ts->client->adapter, msg, 2)) == 2){
			break;
		}
		tsp_debug_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
	}
	mutex_unlock(&ts->i2c_mutex);
	if (retry == 10) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
	}
	if (ret == 2)
		return 0;
err:
	return -EIO;
}


#ifdef SEC_TS_SUPPORT_STRINGLIB
static int sec_ts_read_from_string(struct sec_ts_data *ts,
					unsigned short *reg, unsigned char *data, int length)
{
	unsigned char string_reg[3];
	unsigned char *buf;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		tsp_debug_err(true, &ts->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	string_reg[0] = 0xD0;
	string_reg[1] = (*reg >> 8) & 0xFF;
	string_reg[2] = *reg & 0xFF;

	if (ts->digital_rev == FTS_DIGITAL_REV_1) {
		return fts_read_reg(ts, string_reg, 3, data, length);
	} else {
		int rtn;
		buf = kzalloc(length + 1, GFP_KERNEL);
		if (buf == NULL) {
			tsp_debug_info(true, &info->client->dev,
					"%s: kzalloc error.\n", __func__);
			return -1;
		}

		rtn = fts_read_reg(info, string_reg, 3, buf, length + 1);
		if (rtn >= 0)
			memcpy(data, &buf[1], length);

		kfree(buf);
		return rtn;
	}
}
/*
 * int sec_ts_write_to_string(struct fts_ts_info *, unsigned short *, unsigned char *, int)
 * send command or write specfic value to the string area.
 * string area means guest image or brane firmware.. etc..
 */
static int sec_ts_write_to_string(struct sec_ts_data *ts,
					unsigned short *reg, unsigned char *data, int length)
{
	struct i2c_msg xfer_msg[3];
	unsigned char *regAdd;
	int ret;

	if (ts->touch_stopped) {
		   tsp_debug_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		   return 0;
	}

	regAdd = kzalloc(length + 6, GFP_KERNEL);
	if (regAdd == NULL) {
		tsp_debug_info(true, &ts->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -1;
	}

	mutex_lock(&ts->i2c_mutex);

/* msg[0], length 3*/
	regAdd[0] = 0xb3;
	regAdd[1] = 0x20;
	regAdd[2] = 0x01;

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = 3;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = &regAdd[0];
/* msg[0], length 3*/

/* msg[1], length 4*/
	regAdd[3] = 0xb1;
	regAdd[4] = (*reg >> 8) & 0xFF;
	regAdd[5] = *reg & 0xFF;

	memcpy(&regAdd[6], data, length);

/*regAdd[3] : B1 address, [4], [5] : String Address, [6]...: data */

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = 3 + length;
	xfer_msg[1].flags = 0;
	xfer_msg[1].buf = &regAdd[3];
/* msg[1], length 4*/

	ret = i2c_transfer(ts->client->adapter, xfer_msg, 2);
	if (ret == 2) {
		tsp_debug_info(true, &ts->client->dev,
				"%s: string command is OK.\n", __func__);

		regAdd[0] = FTS_CMD_NOTIFY;
		regAdd[1] = *reg & 0xFF;
		regAdd[2] = (*reg >> 8) & 0xFF;

		xfer_msg[0].addr = info->client->addr;
		xfer_msg[0].len = 3;
		xfer_msg[0].flags = 0;
		xfer_msg[0].buf = regAdd;

		ret = i2c_transfer(ts->client->adapter, xfer_msg, 1);
		if (ret != 1)
			tsp_debug_info(true, &ts->client->dev,
					"%s: string notify is failed.\n", __func__);
		else
			tsp_debug_info(true, &ts->client->dev,
					"%s: string notify is OK[%X].\n", __func__, *data);

	} else
		tsp_debug_info(true, &ts->client->dev,
				"%s: string command is failed. ret: %d\n", __func__, ret);

	mutex_unlock(&ts->i2c_mutex);
	kfree(regAdd);

	return ret;
}
#endif

static int sec_ts_i2c_read_bulk(struct sec_ts_data * ts, u8 * data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;

	msg.addr = ts->client->addr;
	msg.flags = I2C_M_RD;
	msg.len = len;
	msg.buf = data;

	mutex_lock(&ts->i2c_mutex);

	for (retry = 0; retry < 10; retry++) {
		if((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1){
			break;
		}
		tsp_debug_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == 10) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
	}

	if (ret == 1)
		return 0;

	return -EIO;
}

void sec_ts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

/*
static int sec_ts_i2c_readburst(struct sec_ts_data * ts, char * data, int len)
{
        int ret;
        struct i2c_msg msg;

        msg.addr = ts->client->addr;
        msg.flags = I2C_M_RD;
        msg.len = len;
        msg.buf = data;

        ret = i2c_transfer(ts->client->adapter, &msg, 1);
        if (ret == 1)
                return 0;

        return -EIO;
}*/

#define MAX_EVENT_COUNT 128
static void sec_sec_ts_read_event(struct sec_ts_data *ts)
{
	int ret;
	int is_event_remain;
	int t_id;
	int event_id;
	int read_event_count;
	u8 read_event_buff[SEC_TS_Event_Buff_Size];
#if defined (CONFIG_INPUT_BOOSTER)
	bool booster_restart = false;
#endif
	struct sec_ts_event_coordinate * p_event_coord;
	struct sec_ts_event_status * p_event_status;
	struct sec_ts_coordinate coordinate;

	is_event_remain = 0;
	read_event_count = 0;

	//repeat READ_ONE_EVENT until buffer is empty(No event)
	do
	{
		mutex_lock(&ts->device_mutex);
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, read_event_buff, SEC_TS_Event_Buff_Size);
		mutex_unlock(&ts->device_mutex);

		if (ret < 0)
		{
			tsp_debug_err(true, &ts->client->dev, "sec_sec_ts_read_event i2c read one event failed\n");
			return ;
		}

		if (read_event_count > MAX_EVENT_COUNT)
		{

			tsp_debug_err(true, &ts->client->dev, "sec_sec_ts_read_event : event buffer overflow\n");

			//write clear event stack command when read_event_count > MAX_EVENT_COUNT
			mutex_lock(&ts->device_mutex);
			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
			mutex_unlock(&ts->device_mutex);
			if (ret < 0)
			{
				tsp_debug_err(true, &ts->client->dev, "sec_sec_ts_read_event i2c write clear event failed\n");
			}

			return ;
		}

		event_id = read_event_buff[0] >> 6;
		switch (event_id)
		{
			case SEC_TS_Status_Event:
				p_event_status = (struct sec_ts_event_status *)read_event_buff;

				if (p_event_status->sid != 0x0)
				{
					if(((read_event_buff[0]&0x3) == 1) && (read_event_buff[1] == 0x14))
					{
						memcpy(ts->gesture_status, read_event_buff,SEC_TS_Event_Buff_Size);
						tsp_debug_info(true, &ts->client->dev, "sec_sec_ts_read_event GESTURE STATUS %x %x %x %x %x %x\n",
							ts->gesture_status[0], ts->gesture_status[1], ts->gesture_status[2],ts->gesture_status[3], ts->gesture_status[4], ts->gesture_status[5]);
					}

					tsp_debug_info(true, &ts->client->dev, "sec_sec_ts_read_event  STATUS %x %x %x %x %x %x\n",
						read_event_buff[0], read_event_buff[1], read_event_buff[2], read_event_buff[3], read_event_buff[4], read_event_buff[5]);
				}
				sec_ts_release_all_finger(ts);

				//No Event, end of event queue
				is_event_remain = 0;
				break;

			case SEC_TS_Coordinate_Event:
				p_event_coord = (struct sec_ts_event_coordinate *)read_event_buff;

				t_id = (p_event_coord->tid - 1);

				if (t_id < MAX_SUPPORT_TOUCH_COUNT+MAX_SUPPORT_HOVER_COUNT)
				{
					coordinate.id = t_id;
					coordinate.action = p_event_coord->tchsta;
					coordinate.ttype = p_event_coord->ttype;
					coordinate.x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
					coordinate.y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
					coordinate.touch_width = p_event_coord->z / 10;
					coordinate.ttype = p_event_coord->ttype & 0x7;

					if ((t_id == SEC_TS_EVENTID_HOVER) && coordinate.ttype == SEC_TS_TOUCHTYPE_PROXIMITY) {
						if ((coordinate.action == SEC_TS_Coordinate_Action_Release)) {
							input_mt_slot(ts->input_dev, 0);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
							tsp_debug_dbg(true, &ts->client->dev,
								"%s: Hover - Release - tid=%d, touch_count=%d\n",
								__func__, t_id, ts->touch_count);
							break;
						} else {
							input_mt_slot(ts->input_dev, 0);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);

							input_report_key(ts->input_dev, BTN_TOUCH, false);
							input_report_key(ts->input_dev, BTN_TOOL_FINGER, true);

							input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
							input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
							input_report_abs(ts->input_dev, ABS_MT_DISTANCE, coordinate.touch_width);

							if (coordinate.action == SEC_TS_Coordinate_Action_Press)
								tsp_debug_dbg(true, &ts->client->dev,
									"%s: Hover - Press - tid=%d, touch_count=%d\n",
									__func__, t_id, ts->touch_count);
							else if (coordinate.action == SEC_TS_Coordinate_Action_Move)
								tsp_debug_dbg(true, &ts->client->dev,
									"%s: Hover - Move - tid=%d, touch_count=%d\n",
									__func__, t_id, ts->touch_count);
							break;
						}
					}
					else if (coordinate.ttype == SEC_TS_TOUCHTYPE_NORMAL
							|| coordinate.ttype == SEC_TS_TOUCHTYPE_GLOVE) {
							if (coordinate.action == SEC_TS_Coordinate_Action_Release) {
								coordinate.touch_width = 0;
								coordinate.action = SEC_TS_Coordinate_Action_None;

								input_mt_slot(ts->input_dev, t_id);
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

								if (ts->touch_count > 0 )
									ts->touch_count--;
								if (ts->touch_count == 0) {
									input_report_key(ts->input_dev, BTN_TOUCH, 0);
									input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
								}

								break;
							} else if (coordinate.action == SEC_TS_Coordinate_Action_Press) {
								ts->touch_count++;
#if defined (CONFIG_INPUT_BOOSTER)
								booster_restart = true;
#endif
								input_mt_slot(ts->input_dev, t_id);
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
								input_report_key(ts->input_dev, BTN_TOUCH, 1);
								input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

								input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
								input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
								input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate.touch_width);
								input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate.touch_width);

								memcpy(&ts->coord[t_id], &coordinate,sizeof(struct sec_ts_coordinate));
								break;
							} else if (coordinate.action == SEC_TS_Coordinate_Action_Move) {
								if ((coordinate.ttype == SEC_TS_TOUCHTYPE_GLOVE) && !ts->touchkey_glove_mode_status) {
									ts->touchkey_glove_mode_status = true;
									input_report_switch(ts->input_dev, SW_GLOVE, 1);
								}

								input_mt_slot(ts->input_dev, t_id);
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
								input_report_key(ts->input_dev, BTN_TOUCH, 1);
								input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

								input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
								input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
								input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate.touch_width);
								input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate.touch_width);

								memcpy(&ts->coord[t_id], &coordinate, sizeof(struct sec_ts_coordinate));
								break;
							} else
								break;
					} else
						break;
				} else {
					tsp_debug_err(true, &ts->client->dev, "sec_ts_read_event  tid(%d) is  out of range\n", t_id);
				}

				is_event_remain = 1;
				break;

			case SEC_TS_Gesture_Event:
				tsp_debug_info(true, &ts->client->dev, "sec_ts_read_event GESTURE  %x %x %x %x %x %x\n",
					read_event_buff[0], read_event_buff[1], read_event_buff[2], read_event_buff[3], read_event_buff[4], read_event_buff[5]);

				is_event_remain = 1;
				break;

			default:
				tsp_debug_err(true, &ts->client->dev, "sec_ts_read_event unknown event  %x %x %x %x %x %x\n",
					read_event_buff[0], read_event_buff[1], read_event_buff[2], read_event_buff[3], read_event_buff[4], read_event_buff[5]);

				is_event_remain = 1;
				break;

		}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		if (coordinate.action == SEC_TS_Coordinate_Action_Press)
			tsp_debug_info(true, &ts->client->dev,
				"%s: [P] tID:%d,  x:%d, y:%d, w:%d, tc:%d\n",
				__func__, t_id, coordinate.x, coordinate.y, coordinate.touch_width, ts->touch_count);
#else
		if (coordinate.action == SEC_TS_Coordinate_Action_Press)
			tsp_debug_info(true, &ts->client->dev,
				"%s: [P] tID:%d,  tc:%d\n",
				__func__, t_id, ts->touch_count);
#endif
		else if (coordinate.action == SEC_TS_Coordinate_Action_Release) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			tsp_debug_info(true, &ts->client->dev,
				"%s: [R] tID:%d mc: %d tc:%d lx:%d ly:%d\n",
				__func__, t_id, ts->coord[t_id].mcount, ts->touch_count,
				ts->coord[t_id].x, ts->coord[t_id].y);
#else
			tsp_debug_info(true, &ts->client->dev,
				"%s: [R] tID:%d mc: %d tc:%d %d\n",
				__func__, t_id, ts->coord[t_id].mcount, ts->touch_count);
#endif
			ts->coord[t_id].mcount = 0;
		} else if (coordinate.action == SEC_TS_Coordinate_Action_Move)
			ts->coord[t_id].mcount++;

	} while(is_event_remain);

        input_sync(ts->input_dev);

#if defined (CONFIG_INPUT_BOOSTER)
	if ((coordinate.action == SEC_TS_Coordinate_Action_Press)
			|| (coordinate.action == SEC_TS_Coordinate_Action_Release)) {
		if (booster_restart)
			input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_ON);
		if (!ts->touch_count)
			input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_OFF);
	}
#endif
}

static irqreturn_t sec_ts_irq_thread(int irq, void *ptr)
{
	struct sec_ts_data * ts;

	ts = (struct sec_ts_data *)ptr;

	sec_sec_ts_read_event(ts);

	return IRQ_HANDLED;
}

int get_tsp_status(void)
{
	return 0;
}
EXPORT_SYMBOL(get_tsp_status);

#define NOISE_MODE_CMD 0x77
void sec_ts_set_charger(bool enable)
{
	return;
/*
	int ret;
	u8 noise_mode_on[] = {0x01};
	u8 noise_mode_off[] = {0x00};
	if (enable)
	{
		tsp_debug_info(true, &ts->client->dev, "sec_ts_set_charger : charger CONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, NOISE_MODE_CMD, noise_mode_on, sizeof(noise_mode_on));
	        if (ret < 0)
	                tsp_debug_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_ON\n");
	}
	else
	{
		tsp_debug_info(true, &ts->client->dev, "sec_ts_set_charger : charger DISCONNECTED!!\n");
                ret = sec_ts_i2c_write(ts, NOISE_MODE_CMD, noise_mode_off, sizeof(noise_mode_off));
                if (ret < 0)
                        tsp_debug_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_OFF\n");
	}
*/
}
EXPORT_SYMBOL(sec_ts_set_charger);

#ifdef CONFIG_TOUCHSCREEN_SEC_TS_GLOVEMODE
int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode)
{
	int ret;
	u8 rdata[1];
	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF \n",__func__);
		goto glove_enable_err;
	}

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_SET_TOUCHFUNCTION, rdata, 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail to read glove status %x\n",__func__,rdata[0]);
		goto glove_enable_err;
	}

	if (mode) {
		//glove enable
		rdata[0] = rdata[0]|SEC_TS_BIT_SETFUNC_GLOVE;
		ts->glove_enables = (ts->glove_enables|SEC_TS_BIT_SETFUNC_GLOVE|SEC_TS_BIT_SETFUNC_MUTUAL);
	} else {
		//glove disconneted
		rdata[0] = rdata[0]&(~SEC_TS_BIT_SETFUNC_GLOVE);
		ts->glove_enables = ((ts->glove_enables&(~SEC_TS_BIT_SETFUNC_GLOVE))|SEC_TS_BIT_SETFUNC_MUTUAL);
	}
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, rdata, 1);

	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto glove_enable_err;
	} else {
		ts->glove_enables = rdata[0];
	}

	tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(mode)?"glove enable":"glove disable", ts->glove_enables);

	return 0;

glove_enable_err:
	(mode)? (ts->glove_enables = SEC_TS_BIT_SETFUNC_GLOVE)|SEC_TS_BIT_SETFUNC_MUTUAL:
	(ts->glove_enables = (ts->glove_enables&(~SEC_TS_BIT_SETFUNC_GLOVE))|SEC_TS_BIT_SETFUNC_MUTUAL);
		tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n",
			__func__, (mode) ? "glove enable" : "glove disable", ts->glove_enables);
	return -EIO;
	}
EXPORT_SYMBOL(sec_ts_glove_mode_enables);
#endif

int sec_ts_hover_enables(struct sec_ts_data *ts, int enables)
{
	int ret;
	u8 rdata[1];
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_SET_TOUCHFUNCTION, rdata, 1);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail to read glove status %x\n", __func__, rdata[0]);
		goto hover_enable_err;
	}

	if (enables) {
		rdata[0] = rdata[0] | SEC_TS_BIT_SETFUNC_HOVER;
		ts->glove_enables = (ts->glove_enables | SEC_TS_BIT_SETFUNC_HOVER | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	else {
		rdata[0] = rdata[0] & (~SEC_TS_BIT_SETFUNC_HOVER);
		ts->glove_enables = ((ts->glove_enables & (~SEC_TS_BIT_SETFUNC_HOVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, rdata, 1);

	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto hover_enable_err;
        }
        else
        {
                ts->glove_enables = rdata[0];
        }
	tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enables)?"hover enable":"hover disable", ts->glove_enables);
        return 0;
hover_enable_err:
	(enables)? (ts->glove_enables = (ts->glove_enables | SEC_TS_BIT_SETFUNC_HOVER)|SEC_TS_BIT_SETFUNC_MUTUAL):
		(ts->glove_enables = (ts->glove_enables&(~SEC_TS_BIT_SETFUNC_HOVER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enables)?"hover enable":"hover disable", ts->glove_enables);
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_hover_enables);

int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable)
{
	int ret;
	u8 rdata[1];

	tsp_debug_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->cover_type);

	switch (ts->cover_type) {
		case SEC_TS_VIEW_WIRELESS:
		case SEC_TS_VIEW_COVER:
			ret = sec_ts_i2c_read(ts, SEC_TS_READ_SET_TOUCHFUNCTION, rdata, 1);
			if (ret < 0)
			{
				tsp_debug_err(true, &ts->client->dev, "%s: fail to read clear_cover status %x\n", __func__, rdata[0]);
				goto clearcover_enable_err;
			}

			if (enable) {
				rdata[0] = rdata[0] | SEC_TS_BIT_SETFUNC_CLEARCOVER;
				ts->glove_enables = (ts->glove_enables | SEC_TS_BIT_SETFUNC_CLEARCOVER | SEC_TS_BIT_SETFUNC_MUTUAL);
			} else {
				rdata[0] = rdata[0] & (~SEC_TS_BIT_SETFUNC_CLEARCOVER);
				ts->glove_enables = ((ts->glove_enables & (~SEC_TS_BIT_SETFUNC_CLEARCOVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
			}

			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, rdata, 1);

			if (ret < 0) {
				tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
			    goto clearcover_enable_err;
			} else {
				ts->glove_enables = rdata[0];
			}

			tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enable)?"clearcover enable":"clearcover disable",ts->glove_enables);

			ret = sec_ts_i2c_read(ts, SEC_TS_READ_SET_TOUCHFUNCTION, rdata, 1);
			if (ret < 0)
			{
			        tsp_debug_err(true, &ts->client->dev, "%s: fail to read clear_cover status %x\n", __func__, rdata[0]);
			        goto clearcover_enable_err;
			}

			tsp_debug_info(true, &ts->client->dev, "%s: %x\n", __func__, rdata[0]);
			break;
		case SEC_TS_VIEW_WALLET:
		case SEC_TS_FLIP_WALLET:
		case SEC_TS_LED_COVER:
		case SEC_TS_MONTBLANC_COVER:
		case SEC_TS_CLEAR_FLIP_COVER :
		case SEC_TS_CHARGER_COVER:
		case SEC_TS_COVER_NOTHING1:
		case SEC_TS_COVER_NOTHING2:
		default:
			tsp_debug_err(true, &ts->client->dev, "%s: not change touch state, %d\n", __func__, ts->cover_type);
		break;
	}

	return 0;
clearcover_enable_err:
	(enable)? (ts->glove_enables = (ts->glove_enables | SEC_TS_BIT_SETFUNC_CLEARCOVER)|SEC_TS_BIT_SETFUNC_MUTUAL):
		(ts->glove_enables = (ts->glove_enables&(~SEC_TS_BIT_SETFUNC_CLEARCOVER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	tsp_debug_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enable)?"clearcover enable":"clearcover disable",ts->glove_enables);
        return -EIO;

}
EXPORT_SYMBOL(sec_ts_set_cover_type);

int sec_ts_i2c_write_burst(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	int retry;
	struct i2c_msg msg;

	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	//mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < 10; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1) {
			break;
		}
		tsp_debug_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
	}
	//mutex_unlock(&ts->i2c_mutex);
	if (retry == 10) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
	}

	if (ret == 1)
		return 0;

	return -EIO;
}

/* for debugging--------------------------------------------------------------------------------------*/

#define TX_COUNT 18
#define RX_COUNT 30
#define READ_RAW_DATA_SIZE (((TX_COUNT + 1) * RX_COUNT * 2))
static ssize_t sec_ts_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	int readSize = READ_RAW_DATA_SIZE;
	u8* read_raw_buff;

	ts->raw_status = 1;
	if(ts->power_status == SEC_TS_STATE_POWER_OFF)
		goto err_raw;

	read_raw_buff = (u8 *)kmalloc(sizeof(u8)*READ_RAW_DATA_SIZE,GFP_KERNEL);
	if(!read_raw_buff)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_raw_show kmalloc failed\n");
		return 0;
	}
	disable_irq(ts->client->irq);

	mutex_lock(&ts->device_mutex);
	//readSize = ts->tx_count * ts->rx_count * 2;
	//readSize = TX_COUNT * RX_COUNT * 2;
	readSize = READ_RAW_DATA_SIZE;

	if (readSize  == 0)
		readSize = READ_RAW_DATA_SIZE;

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_TOUCH_RAWDATA, read_raw_buff, readSize);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_raw_show i2c read raw data failed\n");
		return 0;
	}
	//clear event stack
	//sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);

	enable_irq(ts->client->irq);

	mutex_unlock(&ts->device_mutex);
	memcpy(buf, read_raw_buff, readSize);

	kfree(read_raw_buff);
	ts->raw_status = 0;
	return readSize;

err_raw:
	memset(buf, 0xff, readSize);
	return readSize;
}

#define SELF_TX_COUNT 39
#define SELF_RX_COUNT 26
#define READ_SELF_RAW_DATA_SIZE (((SELF_TX_COUNT + 1) * SELF_RX_COUNT * 2))
static ssize_t sec_ts_selfraw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	u8* read_raw_buff;

        ts->raw_status = 1;
        if(ts->power_status == SEC_TS_STATE_POWER_OFF)
                goto err_raw;

	read_raw_buff = (u8 *)kmalloc(sizeof(u8)*READ_SELF_RAW_DATA_SIZE,GFP_KERNEL);
	if(!read_raw_buff)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_selfraw_show kmalloc failed\n");
		return 0;
	}
	disable_irq(ts->client->irq);

	//readSize = ts->tx_count * ts->rx_count * 2;
	mutex_lock(&ts->device_mutex);
	if (regshow_size  == 0)
		regshow_size = READ_SELF_RAW_DATA_SIZE;
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_TOUCH_SELF_RAWDATA, read_raw_buff, regshow_size);

	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_raw_show i2c read raw data failed\n");
		return 0;
	}
	//clear event stack
	sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	enable_irq(ts->client->irq);
	mutex_unlock(&ts->device_mutex);
	memcpy(buf, read_raw_buff, regshow_size);

	kfree(read_raw_buff);

	return regshow_size;
err_raw:
        memset(buf, 0xff, regshow_size);
        return regshow_size;
}

static ssize_t sec_ts_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
	        return -EIO;
	}

	if (size > 0)
	{
		sec_ts_i2c_write(ts, (u8)buf[0], (u8*)(buf + 1), size -1);
	}

	tsp_debug_info(true, &ts->client->dev, "sec_ts_reg: 0x%x, 0x%x, size %d\n", buf[0], buf[1], (int)size);
	return size;
}

static ssize_t sec_ts_regread_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	//u8 *read_lv1_buff;
        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

#if 0
	if(lv1_readremain == 0)
	{
	read_lv1_buff = (u8 *)kmalloc(sizeof(u8)*lv1_readsize, GFP_KERNEL);
	if(!read_lv1_buff)
	{
		tsp_debug_err(true, &ts->client->dev, "%s kmalloc failed\n", __func__);
		return 0;
	}
	mutex_lock(&ts->device_mutex);
	ret = sec_ts_i2c_read(ts, lv1cmd, read_lv1_buff, lv1_readsize);
	mutex_unlock(&ts->device_mutex);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: i2c read %x command\n", __func__, lv1cmd);
		return 0;
	}
	lv1_readremain = lv1_readsize;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: lv1_readsize = %d \n", __func__, lv1_readsize);
	if((lv1_readsize - 1024) > 0)
	{
		memcpy(buf, read_lv1_buff + lv1_readoffset, 1024);
		lv1_readoffset += 1024;
		lv1_readsize -= 1024;
		lv1_readremain = lv1_readsize;
		return 1024;
	}
	else
	{
		memcpy(buf, read_lv1_buff + lv1_readoffset, lv1_readsize);
		kfree(read_lv1_buff);
		lv1_readremain = 0;
		return lv1_readsize;
	}
	return lv1_readremain;
#endif
	        //u8 *read_lv1_buff;
        read_lv1_buff = (u8 *)kmalloc(sizeof(u8)*lv1_readsize, GFP_KERNEL);
        if(!read_lv1_buff)
        {
                tsp_debug_err(true, &ts->client->dev, "%s kmalloc failed\n", __func__);
                return 0;
        }
        mutex_lock(&ts->device_mutex);
        ret = sec_ts_i2c_read(ts, lv1cmd, read_lv1_buff, lv1_readsize);
        mutex_unlock(&ts->device_mutex);
        if (ret < 0)
        {
                tsp_debug_err(true, &ts->client->dev, "%s: i2c read %x command\n", __func__, lv1cmd);
                return 0;
        }
        lv1_readremain = lv1_readsize;

	tsp_debug_info(true, &ts->client->dev, "%s: lv1_readsize = %d \n", __func__, lv1_readsize);
        memcpy(buf, read_lv1_buff + lv1_readoffset, lv1_readsize);
        kfree(read_lv1_buff);
        lv1_readremain = 0;
        return lv1_readsize;
}


static ssize_t sec_ts_read_bulk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
        int ret;

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

        mutex_lock(&ts->device_mutex);
        ret = sec_ts_i2c_read_bulk(ts, buf, lv1_readsize);
        mutex_unlock(&ts->device_mutex);
        if (ret < 0)
        {
                tsp_debug_err(true, &ts->client->dev, "%s: i2c read \n", __func__);
                return 0;
        }

        return lv1_readsize;
}

static ssize_t sec_ts_read_eventtime_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	buf = (char * )eventtime_buf;

        return sizeof(struct timespec)*eventtime_counter;
}


static ssize_t sec_ts_gesture_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

        mutex_lock(&ts->device_mutex);
        //buf = ts->gesture_status;
	memcpy(buf,ts->gesture_status,sizeof(ts->gesture_status));
	tsp_debug_info(true, &ts->client->dev, "sec_sec_ts_gesture_status_show GESTURE STATUS %x %x %x %x %x %x\n",
		ts->gesture_status[0], ts->gesture_status[1], ts->gesture_status[2],ts->gesture_status[3], ts->gesture_status[4], ts->gesture_status[5]);
	mutex_unlock(&ts->device_mutex);
        return sizeof(ts->gesture_status);
}

static ssize_t sec_ts_dd_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *version = SEC_TS_DRV_VERSION;
	ssize_t size = sizeof(version);
	//memcpy(buf,version,sizeof(version));
	memcpy(buf,version,size);
        return sizeof(version);
}

static ssize_t sec_ts_touchtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	memcpy(buf, &ts->touchtype, sizeof(ts->touchtype));

        return sizeof(ts->touchtype);
}

/*
static ssize_t sec_ts_regread_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

        if (regshow_size > 0)
        {
                sec_ts_i2c_readburst(ts, buf, regshow_size);
        }

        return regshow_size;
}
static DEVICE_ATTR(sec_ts_regread, 0666, sec_ts_regread_show, NULL);
*/
static ssize_t sec_ts_setreadsize_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (size >= 4)
	{
		regshow_size = ((unsigned int)buf[3] << 24) |
				((unsigned int)buf[2]<<16) |((unsigned int) buf[1]<<8) |((unsigned int)buf[0]<<0);
	}
	return size;
}

static ssize_t sec_ts_setlv1cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	lv1cmd = buf[0];
	lv1_readsize = ((unsigned int)buf[4] << 24) |
			((unsigned int)buf[3]<<16) |((unsigned int) buf[2]<<8) |((unsigned int)buf[1]<<0);
	lv1_readoffset = 0;
        lv1_readremain = 0;
	return size;
}

static ssize_t sec_ts_bulk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	struct i2c_msg msg;

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

	tsp_debug_info(true, &ts->client->dev, "%s: size = %d\n", __func__, (int)size);
	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = size;
	msg.buf = (u8*)buf;
	mutex_lock(&ts->device_mutex);
	ret = i2c_transfer(ts->client->adapter, &msg, 1);
	mutex_unlock(&ts->device_mutex);
	if (ret == 1)
		return size;

	tsp_debug_info(true, &ts->client->dev, "sec_ts_bulk: bulk error\n");
	return -EIO;
}

#define SEC_TS_Read_ID 0x52
#define READ_ID_BUFF_SIZE 3
static ssize_t sec_ts_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	u8 t_buff[READ_ID_BUFF_SIZE];

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

	mutex_lock(&ts->device_mutex);
	ret = sec_ts_i2c_read(ts, SEC_TS_Read_ID, t_buff, READ_ID_BUFF_SIZE);
	mutex_unlock(&ts->device_mutex);
	if (ret < 0)
	{
			tsp_debug_err(true, &ts->client->dev, "sec_ts_read_id_show i2c read id failed\n");
			return 0;
	}

	memcpy(buf, t_buff, READ_ID_BUFF_SIZE);
	return READ_ID_BUFF_SIZE;
}

static ssize_t sec_ts_raw_channel_show(struct device *dev, struct device_attribute *attr, char * buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	buf[0] = ts->tx_count;
	buf[1] = ts->rx_count;
	return 2;
}

#define SEC_TS_Read_FW_Version 0xA2
#define READ_FW_VER_BUFF_SIZE 16
static ssize_t sec_ts_fw_ver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	u8 t_buff[READ_FW_VER_BUFF_SIZE];

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

	mutex_lock(&ts->device_mutex);
	ret = sec_ts_i2c_read(ts, SEC_TS_Read_FW_Version, t_buff, READ_FW_VER_BUFF_SIZE);
	mutex_unlock(&ts->device_mutex);
	if (ret < 0)
	{
			tsp_debug_err(true, &ts->client->dev, "sec_ts_read_ver_show i2c read ver failed\n");
			return 0;
	}
	memcpy(buf, t_buff, READ_FW_VER_BUFF_SIZE);
	return READ_FW_VER_BUFF_SIZE;
}

#define READ_STATUS_BUFF_SIZE 5
static ssize_t sec_ts_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	u8 read_status_buff[READ_STATUS_BUFF_SIZE];

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

	mutex_lock(&ts->device_mutex);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, read_status_buff, 1);
	if (ret < 0)
	{
			tsp_debug_err(true, &ts->client->dev, "%s i2c read Boot_Status failed\n",__func__);
			return 0;
	}

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_TS_STATUS, read_status_buff + 1, 4);
	if (ret < 0)
	{
			tsp_debug_err(true, &ts->client->dev, "%s i2c read TS_Status failed\n",__func__);
			return 0;
	}
	mutex_unlock(&ts->device_mutex);
	memcpy(buf, read_status_buff, READ_STATUS_BUFF_SIZE);
	return READ_STATUS_BUFF_SIZE;
}

#define READ_LV3_BUFF_SIZE 4
static ssize_t sec_ts_read_lv3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
        int ret;
        u8 read_lv3_buff[READ_LV3_BUFF_SIZE];

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                 tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

	mutex_lock(&ts->device_mutex);
        ret = sec_ts_i2c_read(ts, SEC_TS_READ_LV3, read_lv3_buff, READ_LV3_BUFF_SIZE);
	mutex_unlock(&ts->device_mutex);
        if (ret < 0)
        {
                tsp_debug_err(true, &ts->client->dev, "%s lv3 read  failed\n",__func__);
                return 0;
        }

        memcpy(buf, read_lv3_buff, READ_LV3_BUFF_SIZE);
        return READ_LV3_BUFF_SIZE;
}

static ssize_t sec_ts_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	tsp_debug_err(true, &ts->client->dev, "%s is not supported\n",__func__);
	return -1;
}

static ssize_t sec_ts_enter_recovery_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret;
	u8 on = (u8)buf[0];

	if(on == 1)
	{
		disable_irq(ts->client->irq);
		gpio_free(pdata->gpio);

		tsp_debug_info(true, &ts->client->dev, "%s: gpio free\n", __func__);
		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_OUT_INIT_LOW, "stm,tsp_int");
			tsp_debug_info(true, &ts->client->dev, "%s: gpio request one\n", __func__);
			if (ret) {
				tsp_debug_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			tsp_debug_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(100);
		pdata->power(ts, true);
	}
	else
	{
		gpio_free(pdata->gpio);

		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "stm,tsp_int");
			if (ret) {
				tsp_debug_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			tsp_debug_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(500);
		pdata->power(ts, true);
		sec_ts_delay(500);

		//AFE Calibration
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_AFE_CALIBRATION, NULL, 0);
		if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: fail to write AFE_CAL\n",__func__);
		}

		sec_ts_delay(1000);
		enable_irq(ts->client->irq);
	}

	return size;
}
//static DEVICE_ATTR(sec_ts_enter_recovery, 0666, NULL, sec_ts_enter_recovery_store);
static void sec_ts_charger_config(struct sec_ts_data * ts, int status)
{
	int ret;
	u8 rdata[1];

	if(ts->power_status == SEC_TS_STATE_POWER_OFF)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail to enalbe charger status, POWER_STATUS=OFF \n",__func__);
		goto charger_config_err;
	}
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_SET_TOUCHFUNCTION, rdata, 1);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail to read Charger status\n",__func__);
		goto charger_config_err;
	}
	else
	{
		tsp_debug_info(true, &ts->client->dev, "%s: Charger status read = %x\n",__func__, rdata[0]);
	}

	if (status == 0x01 || status == 0x03){
		//charger connected
		rdata[0] = (rdata[0]|SEC_TS_BIT_SETFUNC_CHARGER);
		ts->glove_enables = ts->glove_enables|SEC_TS_BIT_SETFUNC_CHARGER|SEC_TS_BIT_SETFUNC_MUTUAL;
	}
	else{
		//charger disconneted
		rdata[0] = (rdata[0]&(~(SEC_TS_BIT_SETFUNC_CHARGER)));
		ts->glove_enables = ((ts->glove_enables&(~SEC_TS_BIT_SETFUNC_CHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	}
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, rdata, 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command\n", __func__);
		goto charger_config_err;
	}
	tsp_debug_err(true, &ts->client->dev, "%s: charger inform : read status = %x\n", __func__, ts->glove_enables);
	return;

charger_config_err:
	if(status == 0x01 || status == 0x03)
		ts->glove_enables = ts->glove_enables|SEC_TS_BIT_SETFUNC_CHARGER|SEC_TS_BIT_SETFUNC_MUTUAL;
	else
		ts->glove_enables = ((ts->glove_enables&(~SEC_TS_BIT_SETFUNC_CHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	tsp_debug_err(true, &ts->client->dev, "%s: charger inform : touch function status = %x\n", __func__, ts->glove_enables);
}

static void sec_ts_ta_cb(struct sec_ts_callbacks *cb, int status)
{
	struct sec_ts_data *ts =
		container_of(cb, struct sec_ts_data, callbacks);
	tsp_debug_err(true, &ts->client->dev, "[TSP] %s: status : %x\n", __func__, status);

	ts->ta_status = status;
	/* if do not completed driver loading, ta_cb will not run. */
	/*if (!rmi4_data->init_done.done) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: until driver loading done.\n", __func__);
		return;
	}

	if (rmi4_data->touch_stopped || rmi4_data->doing_reflash) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: device is in suspend state or reflash.\n",
			__func__);
		return;
	}*/

	sec_ts_charger_config(ts,status);
}

static void sec_ts_raw_device_init(struct sec_ts_data *ts)
{
        int ret;

        sec_ts_dev = sec_device_create(ts, "sec_ts");

	ret = IS_ERR(sec_ts_dev);
        if (ret)     {
                tsp_debug_err(true, &ts->client->dev, "%s: fail - device_create\n", __func__);
                return;
        }

	ret = sysfs_create_group(&sec_ts_dev->kobj,&cmd_attr_group);
        if(ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail - sysfs_create_group\n",__func__);
		goto err_sysfs;
	}
#if 0
	ret = sysfs_create_link(&sec_ts_dev->kobj,
				&ts->input_dev->dev.kobj, "input");

	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto err_sysfs;
	}
#endif
	return;

err_sysfs:
	tsp_debug_err(true, &ts->client->dev, "%s: fail\n",__func__);
	return;
}

/* for debugging--------------------------------------------------------------------------------------*/
static int sec_ts_power(void *data, bool on)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)data;
	const struct sec_ts_plat_data *pdata = ts->plat_data;
	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;
	static bool enabled;
	int ret = 0;

	if (enabled == on)
		return ret;

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		ret = regulator_enable(regulator_dvdd);
		if (ret) {
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to enable vdd: %d\n", __func__, ret);
			return ret;
		}
		ret = regulator_enable(regulator_avdd);
		if (ret) {
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
			return ret;
		}

		ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_default);
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to configure tsp_attn pin\n", __func__);

		sec_ts_delay(5);
	} else {
		if (regulator_is_enabled(regulator_dvdd))
			regulator_disable(regulator_dvdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_sleep);
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to configure tsp_attn pin\n", __func__);
	}

	enabled = on;
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return ret;
}

static int sec_ts_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	u32 coords[2], lines[2];
	int ret = 0;

	pdata->gpio = of_get_named_gpio(np, "sec,irq_gpio", 0);
	if (gpio_is_valid(pdata->gpio)) {
		ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
		if (ret) {
			tsp_debug_err(true, &client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
			return -EINVAL;
		}
	} else {
		tsp_debug_err(true, &client->dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}
	client->irq = gpio_to_irq(pdata->gpio);

	if (of_property_read_u32(np, "sec,irq_type", &pdata->irq_type)) {
		tsp_debug_err(true, &client->dev, "Failed to get irq_type property\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "sec,max_coords", coords, 2)) {
		tsp_debug_err(true, &client->dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pdata->max_x = coords[0];
	pdata->max_y = coords[1];

	if (of_property_read_u32_array(np, "sec,num_lines", lines, 2))
		tsp_debug_info(true, &client->dev, "skipped to get num_lines property\n");
	else {
		pdata->num_rx = lines[0];
		pdata->num_tx = lines[1];
		tsp_debug_info(true, &client->dev, "num_of[rx,tx]: [%d,%d]\n",
			pdata->num_rx, pdata->num_tx);
	}

	if (of_property_read_string(np, "sec,regulator_dvdd", &pdata->regulator_dvdd)) {
		tsp_debug_err(true, &client->dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}
	if (of_property_read_string(np, "sec,regulator_avdd", &pdata->regulator_avdd)) {
		tsp_debug_err(true, &client->dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}
	pdata->power = sec_ts_power;

	of_property_read_string(np, "sec,firmware_name", &pdata->firmware_name);
	of_property_read_string(np, "sec,parameter_name", &pdata->parameter_name);

	if (of_property_read_string_index(np, "sec,project_name", 0, &pdata->project_name))
		tsp_debug_info(true, &client->dev, "skipped to get project_name property\n");
	if (of_property_read_string_index(np, "sec,project_name", 1, &pdata->model_name))
		tsp_debug_info(true, &client->dev, "skipped to get model_name property\n");

	return ret;
}


static int sec_ts_setup_drv_data(struct i2c_client *client)
{
	int ret = 0;
	struct sec_ts_data *ts;
	struct sec_ts_plat_data *pdata;

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			tsp_debug_err(true, &client->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		ret = sec_ts_parse_dt(client);
		if (ret) {
			tsp_debug_err(true, &client->dev, "Failed to parse dt\n");
			return ret;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		tsp_debug_err(true, &client->dev, "No platform data found\n");
			return -EINVAL;
	}
	if (!pdata->power) {
		tsp_debug_err(true, &client->dev, "No power contorl found\n");
			return -EINVAL;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) {
		tsp_debug_err(true, &client->dev, "could not get pinctrl\n");
		return PTR_ERR(pdata->pinctrl);
	}

	pdata->pins_default = pinctrl_lookup_state(pdata->pinctrl, "on_state");
	if (IS_ERR(pdata->pins_default))
		tsp_debug_err(true, &client->dev, "could not get default pinstate\n");

	pdata->pins_sleep = pinctrl_lookup_state(pdata->pinctrl, "off_state");
	if (IS_ERR(pdata->pins_sleep))
		tsp_debug_err(true, &client->dev, "could not get sleep pinstate\n");

	ts = kzalloc(sizeof(struct sec_ts_data), GFP_KERNEL);
	if (!ts) {
		tsp_debug_err(true, &client->dev, "%s: Failed to alloc mem for info\n", __func__);
		return -ENOMEM;
	}

	ts->client = client;
	ts->plat_data = pdata;
	ts->crc_addr = 0x0001FE00;
	ts->fw_addr = 0x00002000;
	ts->para_addr = 0x18000;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_write_burst = sec_ts_i2c_write_burst;

#ifdef USE_OPEN_DWORK
	INIT_DELAYED_WORK(&ts->open_work, sec_ts_open_work);
#endif

	i2c_set_clientdata(client, ts);

	return ret;
}

#define T_BUFF_SIZE 4
static int sec_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct sec_ts_data *ts;
#ifdef CONFIG_FW_UPDATE_ON_PROBE
	u8 tBuff[T_BUFF_SIZE];
#endif
	static char sec_ts_phys[64] = { 0 };

	int ret = 0;

	tsp_debug_info(true, &client->dev, "SEC_TS Driver [%s] %s %s\n", 
		SEC_TS_DRV_VERSION, __DATE__, __TIME__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		tsp_debug_err(true, &client->dev, "%s : EIO err!\n", __func__);
		return -EIO;
	}

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		tsp_debug_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}
#endif

	ret = sec_ts_setup_drv_data(client);
	if (ret < 0) {
		tsp_debug_err(true, &client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}

	ts = (struct sec_ts_data *)i2c_get_clientdata(client);
	if (!ts) {
		tsp_debug_err(true, &client->dev, "%s: Failed to get driver data\n", __func__);
		ret = -ENODEV;
		goto err_get_drv_data;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);

	//Enable Power
	ts->plat_data->power(ts, true);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_delay(1000);

#ifndef CONFIG_FW_UPDATE_ON_PROBE
	tsp_debug_info(true, &ts->client->dev, "%s: fw update on probe disabled!\n", __func__);
#endif

#ifdef CONFIG_FW_UPDATE_ON_PROBE
	ret = sec_ts_firmware_update_on_probe(ts);
	if (ret < 0)
		goto err_init;

	//Read Raw Channel Info
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_RAW_CHANNEL, tBuff, 2);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: fail to read raw channel info\n");
		goto err_init;
	}
	else
	{
		ts->tx_count = tBuff[0];
		ts->rx_count = tBuff[1];
		tsp_debug_info(true, &ts->client->dev, "sec_ts_probe: S6SSEC_TS Tx : %d, Rx : %d\n", ts->tx_count, ts->rx_count);
	}

	//AFE Calibration
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_AFE_CALIBRATION, NULL, 0);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: fail to write AFE_CAL\n");
		goto err_init;
	}
	sec_ts_delay(1000);

	//Sense_on
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");
		goto err_init;
	}
#endif

	ts->pFrame = kzalloc(ts->tx_count * ts->rx_count * 2, GFP_KERNEL);
	if (!ts->pFrame) {
		tsp_debug_err(true, &ts->client->dev, "%s: allocate pFrame err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_frame;
	}

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		tsp_debug_err(true, &ts->client->dev, "%s: allocate device err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_device;
	}

	ts->input_dev->name = "sec_touchscreen";
	snprintf(sec_ts_phys, sizeof(sec_ts_phys), "%s/input1",
		ts->input_dev->name);
	ts->input_dev->phys = sec_ts_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &client->dev;
	ts->touch_count = 0;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
#ifdef USE_OPEN_CLOSE
	ts->input_dev->open = sec_ts_input_open;
	ts->input_dev->close = sec_ts_input_close;
#endif

#ifdef CONFIG_TOUCHSCREN_SEC_TS_GLOVEMODE
        input_set_capability(ts->input_dev, EV_SW, SW_GLOVE);
#endif
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, ts->input_dev->keybit);
#ifdef SEC_TS_SUPPORT_STRINGLIB
	set_bit(KEY_BLACK_UI_GESTURE, ts->input_dev->keybit);
#endif
#ifdef SEC_TS_SUPPORT_TOUCH_KEY
	if (ts->plat_data->support_mskey) {
		for (i = 0 ; i < ts->plat_data->num_touchkey ; i++)
			set_bit(ts->plat_data->touchkey[i].keycode, ts->input_dev->keybit);

		set_bit(EV_LED, ts->input_dev->evbit);
		set_bit(LED_MISC, ts->input_dev->ledbit);
	}
#endif
#ifdef SEC_TS_SUPPORT_SIDE_GESTURE
	if (ts->plat_data->support_sidegesture) {
		set_bit(KEY_SIDE_GESTURE, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_RIGHT, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_LEFT, ts->input_dev->keybit);
	}
#endif
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
        set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	input_mt_init_slots(ts->input_dev, MAX_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->plat_data->max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->plat_data->max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
#ifdef SEC_TS_SUPPORT_SEC_SWIPE
	input_set_abs_params(ts->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
#endif
#if defined(SEC_TS_SUPPORT_SIDE_GESTURE)
	if (ts->plat_data->support_sidegesture)
		input_set_abs_params(ts->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_DISTANCE, 0, 255, 0, 0);

	input_set_drvdata(ts->input_dev, ts);
	i2c_set_clientdata(client, ts);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		tsp_debug_err(true, &ts->client->dev, "%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device;
	}

	tsp_debug_info(true, &ts->client->dev, "sec_ts_probe request_irq = %d\n" , client->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_ts_irq_thread,
		ts->plat_data->irq_type, SEC_TS_I2C_NAME, ts);

	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: Unable to request threaded irq\n");
		goto err_irq;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#if 0
	trustedui_set_tsp_irq(ts->irq);
	tsp_debug_info(true, &client->dev, "%s[%d] called!\n",
		__func__, ts->irq);
#endif
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = sec_ts_early_suspend;
	ts->early_suspend.resume = sec_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

//#ifdef SEC_TS_SUPPORT_TA_MODE
	ts->callbacks.inform_charger = sec_ts_ta_cb;
	if(ts->plat_data->register_cb)
		ts->plat_data->register_cb(&ts->callbacks);
//#endif

	sec_ts_raw_device_init(ts);
	sec_ts_fn_init(ts);

	return 0;

err_irq:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device:
	if (ts->input_dev)
		input_free_device(ts->input_dev);
err_allocate_device:
	kfree(ts->pFrame);
err_allocate_frame:
#ifdef CONFIG_FW_UPDATE_ON_PROBE
err_init:
#endif
	ts->plat_data->power(ts, false);
err_get_drv_data:
	kfree(ts);
err_setup_drv_data:
	return ret;
}

void sec_ts_release_all_finger(struct sec_ts_data *ts)
{
	int i;

	for (i=0; i < MAX_SUPPORT_TOUCH_COUNT; i++)
	{
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		ts->coord[i].mcount = 0;
	}

	input_report_key(ts->input_dev, BTN_TOUCH, false);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, false);
	ts->touch_count = 0;
#ifdef CONFIG_TOUCHSCREEN_SEC_TS_GLOVEMODE
        input_report_switch(ts->input_dev,
                SW_GLOVE, false);
        ts->touchkey_glove_mode_status = false;
#endif
#if defined (CONFIG_INPUT_BOOSTER)
	input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_OFF);
#endif

	input_sync(ts->input_dev);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	return;
}
#endif

#ifdef USE_OPEN_CLOSE
#ifdef USE_OPEN_DWORK
static void sec_ts_open_work(struct work_struct *work)
{
	int ret;
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
						open_work.work);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	ret = sec_ts_start_device(ts);
	if (ret < 0)
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
}
#endif
static int sec_ts_input_open(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);
	int ret;

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	schedule_delayed_work(%ts->open_work,
				msecs_to_jiffies(TOUCH_OPEN_DWORK_TIME));
#else
	ret = sec_ts_start_device(ts);
	if (ret < 0)
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
#endif
	return 0;
}

static void sec_ts_input_close(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	cancel_delayed_work(&ts->open_work);
#endif

	sec_ts_stop_device(ts);
}
#endif
static int sec_ts_remove(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

	free_irq(client->irq, ts);

	input_mt_destroy_slots(ts->input_dev);
	input_unregister_device(ts->input_dev);

	ts->input_dev = NULL;
	ts->plat_data->power(ts, false);

	kfree(ts);
	return 0;
}

static void sec_ts_shutdown(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_stop_device(ts);
}

static int sec_ts_stop_device(struct sec_ts_data *ts)
{
	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->client->irq);
	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, false);
	ts->power_status = SEC_TS_STATE_POWER_OFF;

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

static int sec_ts_start_device(struct sec_ts_data *ts)
{
	int ret;
	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_ON) {
		tsp_debug_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, true);
	sec_ts_delay(100);
	ts->power_status = SEC_TS_STATE_POWER_ON;

	if (ts->plat_data->enable_sync)
                ts->plat_data->enable_sync(true);

	if(ts->ta_status)
		sec_ts_charger_config(ts, ts->ta_status);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->glove_enables, 1);
        if(ret <0){
                tsp_debug_err(true, &ts->client->dev, "%s: Failed to send glove_mode command", __func__);
        }

	enable_irq(ts->client->irq);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

#if (!defined(CONFIG_HAS_EARLYSUSPEND)) && (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
static int sec_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_stop_device(ts);

	return 0;
}

static int sec_ts_resume(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_start_device(ts);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_ts_early_suspend(struct early_suspend *h)
{
	struct sec_ts_data *ts;
	ts = container_of(h, struct sec_ts_data, early_suspend);

	sec_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void sec_ts_late_resume(struct early_suspend *h)
{
	struct sec_ts_data *ts;
	ts = container_of(h, struct sec_ts_data, early_suspend);

	sec_ts_resume(ts->client);
}
#endif

#ifdef CONFIG_PM
static int sec_ts_pm_suspend(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->input_dev->mutex);

	if (ts->input_dev->users)
		sec_ts_stop_device(ts);

	mutex_unlock(&ts->input_dev->mutex);

	return 0;
}

static int sec_ts_pm_resume(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->input_dev->mutex);

	if (ts->input_dev->users)
		sec_ts_start_device(ts);

	mutex_unlock(&ts->input_dev->mutex);

	return 0;
}
#endif

static const struct i2c_device_id sec_ts_id[] = {
	{ SEC_TS_I2C_NAME, 0 },
	{ },
};

//MODULE_DEVICE_TABLE(i2c, sec_ts_id);

#ifdef CONFIG_PM
static const struct dev_pm_ops sec_ts_dev_pm_ops = {
	.suspend = sec_ts_pm_suspend,
	.resume = sec_ts_pm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id sec_ts_match_table[] = {
	{ .compatible = "sec,sec_ts",},
	{ },
};
#else
#define sec_ts_match_table NULL
#endif

static struct i2c_driver sec_ts_driver = {
	.probe		= sec_ts_probe,
	.remove		= sec_ts_remove,
	.shutdown	= sec_ts_shutdown,
#if (!defined(CONFIG_HAS_EARLYSUSPEND)) && (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
	.suspend	= sec_ts_suspend,
	.resume		= sec_ts_resume,
#endif
	.id_table	= sec_ts_id,
	.driver = {
		.owner    = THIS_MODULE,
		.name	= SEC_TS_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = sec_ts_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &sec_ts_dev_pm_ops,
#endif
	},
};

static int __init sec_ts_init(void)
{
	return i2c_add_driver(&sec_ts_driver);
}

static void __exit sec_ts_exit(void)
{
	i2c_del_driver(&sec_ts_driver);
}

MODULE_AUTHOR("Hyobae, Ahn<hyobae.ahn@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics TouchScreen driver");
MODULE_LICENSE("GPL");

module_init(sec_ts_init);
module_exit(sec_ts_exit);
