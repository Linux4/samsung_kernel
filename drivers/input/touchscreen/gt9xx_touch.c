/*
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Author: andrew@goodix.com
 *
 * History:
 *	V1.0: 2012/08/31, first Release
 *	V1.2: 2012/10/15, modify gtp_reset_guitar,
 *			  slot report, tracking_id & 0x0F
 *	V1.4: 2012/12/12,
 *	V1.5: 2013/02/21, update format and style, remove errors and warnings
 *			  Modified by Albert Wang <twang13@marvell.com>
 *	V1.6: 2013/03/12, remove early suspend, add pm runtime
 *			  Modified by Albert Wang <twang13@marvell.com>
 *	V1.7: 2013/03/21, remove platform dependence from driver
 *			  Modified by Albert Wang <twang13@marvell.com>
 */

#include <linux/irq.h>
#include <linux/input/mt.h>
#include <linux/pm_runtime.h>
#include <linux/i2c/gt9xx_touch.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

static const char *goodix_ts_name = "goodix_gt9xx_ts";
static struct workqueue_struct *goodix_wq;
struct i2c_client *i2c_connect_client;
static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH] = {
	GTP_REG_CONFIG_DATA >> 8,
	GTP_REG_CONFIG_DATA & 0xff
};
static bool regulator_en;

#if GTP_HAVE_TOUCH_KEY
static const u16 touch_key_array[] = GTP_KEY_TAB;
#define GTP_MAX_KEY_NUM	 (sizeof(touch_key_array) / sizeof(touch_key_array[0]))
#endif

#if GTP_ESD_PROTECT
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
#endif

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	u8 retries = 0;
	s32 ret;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];

	while (retries < 3) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}

	if (retries >= 3) {
		GTP_DEBUG("I2C retry timeout, reset chip.");
		gtp_reset_guitar(client, 10);
	}

	return ret;
}

s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	u8 retries = 0;
	s32 ret;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}

	if (retries >= 5) {
		GTP_DEBUG("I2C retry timeout, reset chip.");
		gtp_reset_guitar(client, 10);
	}

	return ret;
}

s32 gtp_send_cfg(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	s32 ret = 0;

#if GTP_DRIVER_SEND_CFG
	u8 retry = 0;

	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, config,
			ts->gtp_cfg_len + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}
#endif
	return ret;
}

void gtp_irq_disable(struct goodix_ts_data *ts)
{
	unsigned long irqflags;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) {
		ts->irq_is_disable = 1;
		disable_irq_nosync(ts->client->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

void gtp_irq_enable(struct goodix_ts_data *ts)
{
	unsigned long irqflags;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) {
		enable_irq(ts->client->irq);
		ts->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static void gtp_touch_down(struct goodix_ts_data *ts, s32 id,
					s32 x, s32 y, s32 w)
{
#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

#if GTP_ICS_SLOT_REPORT
	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
#else
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	input_mt_sync(ts->input_dev);
#endif
	GTP_DEBUG("ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

static void gtp_touch_up(struct goodix_ts_data *ts, s32 id)
{
#if GTP_ICS_SLOT_REPORT
	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
	input_mt_sync_frame(ts->input_dev);
	GTP_DEBUG("Touch id[%2d] release!", id);
#else
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	input_mt_sync(ts->input_dev);
#endif
}

static void goodix_ts_work_func(struct work_struct *work)
{
	struct goodix_ts_data *ts = NULL;
	static u16 pre_touch;
	static u8 pre_key;
	u8 end_cmd[3] = {
		GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF,
		0
	};
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {
		GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF
	};
	u8 touch_num = 0;
	u8 finger = 0;
	u8 key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i, ret;

	GTP_DEBUG_FUNC();

	ts = container_of(work, struct goodix_ts_data, work);
	if (ts->enter_update)
		return;

	ret = gtp_i2c_read(ts->client, point_data, 12);
	if (ret < 0) {
		GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
		goto exit_work_func;
	}

	finger = point_data[GTP_ADDR_LENGTH];
	if ((finger & 0x80) == 0)
		goto exit_work_func;

	touch_num = finger & 0x0f;
	if (touch_num > GTP_MAX_TOUCH)
		goto exit_work_func;

	if (touch_num > 1) {
		u8 buf[8 * GTP_MAX_TOUCH] = {
			(GTP_READ_COOR_ADDR + 10) >> 8,
			(GTP_READ_COOR_ADDR + 10) & 0xff
		};
		ret = gtp_i2c_read(ts->client, buf, 2 + 8 * (touch_num - 1));
		memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
	}

#if GTP_HAVE_TOUCH_KEY
	key_value = point_data[3 + 8 * touch_num];
	if (key_value || pre_key) {
		for (i = 0; i < GTP_MAX_KEY_NUM; i++)
			input_report_key(ts->input_dev, touch_key_array[i],
						key_value & (0x01 << i));

		touch_num = 0;
		pre_touch = 0;
	}
#endif
	pre_key = key_value;

	GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

#if GTP_ICS_SLOT_REPORT
	if (pre_touch || touch_num) {
		s32 pos = 0;
		u16 touch_index = 0;

		coor_data = &point_data[3];
		if (touch_num) {
			id = coor_data[pos] & 0x0F;
			touch_index |= (0x01 << id);
		}

		GTP_DEBUG("id=%d, touch_index=0x%x, pre_touch=0x%x\n",
					id, touch_index, pre_touch);
		for (i = 0; i < GTP_MAX_TOUCH; i++) {
			if (touch_index & (0x01<<i)) {
				input_x = coor_data[pos + 1]
						| coor_data[pos + 2] << 8;
				input_y = coor_data[pos + 3]
						| coor_data[pos + 4] << 8;
				input_w = coor_data[pos + 5]
						| coor_data[pos + 6] << 8;
				gtp_touch_down(ts, id, input_x, input_y,
								input_w);
				pre_touch |= 0x01 << i;
				pos += 8;
				id = coor_data[pos] & 0x0F;
				touch_index |= (0x01<<id);
			} else {
				gtp_touch_up(ts, i);
				pre_touch &= ~(0x01 << i);
			}
		}
	}
#else
	if (touch_num) {
		for (i = 0; i < touch_num; i++) {
			coor_data = &point_data[i * 8 + 3];
			id = coor_data[0] & 0x0F;
			input_x = coor_data[1] | coor_data[2] << 8;
			input_y = coor_data[3] | coor_data[4] << 8;
			input_w = coor_data[5] | coor_data[6] << 8;
			gtp_touch_down(ts, id, input_x, input_y, input_w);
		}
	} else if (pre_touch) {
		GTP_DEBUG("Touch Release!");
		gtp_touch_up(ts, 0);
	}

	pre_touch = touch_num;
	input_report_key(ts->input_dev, BTN_TOUCH, (touch_num || key_value));
#endif
	input_sync(ts->input_dev);
exit_work_func:
	if (!ts->gtp_rawdiff_mode) {
		ret = gtp_i2c_write(ts->client, end_cmd, 3);
		if (ret < 0)
			GTP_ERROR("I2C write end_cmd error!");
	}

	if (ts->use_irq)
		gtp_irq_enable(ts);
}

static enum hrtimer_restart goodix_ts_timer_handler(struct hrtimer *timer)
{
	struct goodix_ts_data *ts =
			container_of(timer, struct goodix_ts_data, timer);

	GTP_DEBUG_FUNC();

	queue_work(goodix_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, (GTP_POLL_TIME + 6) * 1000000),
						HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	GTP_DEBUG_FUNC();

	gtp_irq_disable(ts);
	queue_work(goodix_wq, &ts->work);

	return IRQ_HANDLED;
}

static void gtp_int_sync(struct i2c_client *client, s32 ms)
{
	struct gtp_platform_data *gtp_data = client->dev.platform_data;

	GTP_GPIO_OUTPUT(gtp_data->irq, 0);
	GTP_MSLEEP(ms);
	GTP_GPIO_AS_INT(gtp_data->irq);
}

void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	struct gtp_platform_data *gtp_data = client->dev.platform_data;

	GTP_DEBUG_FUNC();

	/* begin select I2C slave addr */
	GTP_GPIO_OUTPUT(gtp_data->reset, 0);
	GTP_MSLEEP(ms);
	GTP_GPIO_OUTPUT(gtp_data->irq, client->addr == 0x14);

	GTP_MSLEEP(2);
	GTP_GPIO_OUTPUT(gtp_data->reset, 1);

	/* must > 3ms */
	GTP_MSLEEP(6);
	GTP_GPIO_AS_INPUT(gtp_data->reset);
	/* end select I2C slave addr */

	gtp_int_sync(client, 50);
}

static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
	s32 ret;

#if GTP_DRIVER_SEND_CFG
	s32 i;
	u8 check_sum = 0;
	u8 rd_cfg_buf[16];

	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 *send_cfg_buf[3] = {
		cfg_info_group1,
		cfg_info_group2,
		cfg_info_group3
	};
	u8 cfg_info_len[3] = {
		sizeof(cfg_info_group1) / sizeof(cfg_info_group1[0]),
		sizeof(cfg_info_group2) / sizeof(cfg_info_group2[0]),
		sizeof(cfg_info_group3) / sizeof(cfg_info_group3[0])
	};

	for (i = 0; i < 3; i++) {
		if (cfg_info_len[i] > ts->gtp_cfg_len)
			ts->gtp_cfg_len = cfg_info_len[i];
	}
	GTP_DEBUG("len1=%d, len2=%d, len3=%d, send_len:%d", cfg_info_len[0],
			cfg_info_len[1], cfg_info_len[2], ts->gtp_cfg_len);

	if ((!cfg_info_len[1]) && (!cfg_info_len[2]))
		rd_cfg_buf[GTP_ADDR_LENGTH] = 0;
	else if (ts->data->gtp_cfg_group >= 0)
		rd_cfg_buf[GTP_ADDR_LENGTH] = ts->data->gtp_cfg_group;
	else {
		rd_cfg_buf[0] = GTP_REG_SENSOR_ID >> 8;
		rd_cfg_buf[1] = GTP_REG_SENSOR_ID & 0xff;
		ret = gtp_i2c_read(ts->client, rd_cfg_buf, 3);
		if (ret < 0) {
			GTP_ERROR("Read SENSOR ID failed, default use group1 config!");
			rd_cfg_buf[GTP_ADDR_LENGTH] = 0;
		}
		rd_cfg_buf[GTP_ADDR_LENGTH] &= 0x07;
	}
	GTP_DEBUG("SENSOR ID:%d", rd_cfg_buf[GTP_ADDR_LENGTH]);

	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH],
		send_cfg_buf[rd_cfg_buf[GTP_ADDR_LENGTH]], ts->gtp_cfg_len);

#if GTP_CUSTOM_CFG
	config[RESOLUTION_LOC] = (u8)ts->data->gtp_max_width;
	config[RESOLUTION_LOC + 1] = (u8)(ts->data->gtp_max_width>>8);
	config[RESOLUTION_LOC + 2] = (u8)ts->data->gtp_max_height;
	config[RESOLUTION_LOC + 3] = (u8)(ts->data->gtp_max_height>>8);

	if (GTP_INT_TRIGGER == 0)
		/* RISING */
		config[TRIGGER_LOC] &= 0xfe;
	else if (GTP_INT_TRIGGER == 1)
		/* FALLING */
		config[TRIGGER_LOC] |= 0x01;
#endif
	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++)
		check_sum += config[i];
	config[ts->gtp_cfg_len] = (~check_sum) + 1;
#else
	if (ts->gtp_cfg_len == 0)
		ts->gtp_cfg_len = GTP_CONFIG_MAX_LENGTH;

	ret = gtp_i2c_read(ts->client, config,
			ts->gtp_cfg_len + GTP_ADDR_LENGTH);
	if (ret < 0) {
		GTP_ERROR("GTP read resolution & max_touch_num failed, use default value!");
		ts->abs_x_max = ts->data->gtp_max_width;
		ts->abs_y_max = ts->data->gtp_max_height;
		ts->int_trigger_type = GTP_INT_TRIGGER;
		return ret;
	}
#endif
	GTP_DEBUG_FUNC();

	ts->abs_x_max = (config[RESOLUTION_LOC + 1] << 8)
						+ config[RESOLUTION_LOC];
	ts->abs_y_max = (config[RESOLUTION_LOC + 3] << 8)
						+ config[RESOLUTION_LOC + 2];
	ts->int_trigger_type = (config[TRIGGER_LOC]) & 0x03;
	if ((!ts->abs_x_max) || (!ts->abs_y_max)) {
		GTP_ERROR("GTP resolution & max_touch_num invalid, use default value!");
		ts->abs_x_max = ts->data->gtp_max_width;
		ts->abs_y_max = ts->data->gtp_max_height;
	}

	/* MUST delay >20ms before send cfg */
	GTP_MSLEEP(20);

	ret = gtp_send_cfg(ts->client);
	if (ret < 0) {
		GTP_ERROR("Send config error.");
		return ret;
	}

	GTP_DEBUG("X_MAX = %d,Y_MAX = %d,TRIGGER = 0x%02x",
			ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);
	GTP_MSLEEP(10);

	return 0;
}

static s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
	u8 buf[8] = {
		GTP_REG_VERSION >> 8,
		GTP_REG_VERSION & 0xff
	};
	s32 i, ret;

	GTP_DEBUG_FUNC();

	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		GTP_ERROR("GTP read version failed");
		return ret;
	}

	if (version)
		*version = (buf[7] << 8) | buf[6];

	for (i = 2; i < 6; i++) {
		if (!buf[i])
			buf[i] = 0x30;
	}
	GTP_DEBUG("IC VERSION:%c%c%c%c_%02x%02x",
			buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
	return ret;
}

static s8 gtp_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {
		GTP_REG_CONFIG_DATA >> 8,
		GTP_REG_CONFIG_DATA & 0xff
	};
	u8 retry = 0;
	s8 ret;

	GTP_DEBUG_FUNC();

	while (retry++ < 3) {
		ret = gtp_i2c_read(client, test, 3);
		if (ret > 0) {
			GTP_DEBUG("GTP CONFIG VERSION: 0x%2x", test[2]);
			return ret;
		}

		GTP_ERROR("GTP i2c test failed time %d.", retry);
		GTP_MSLEEP(10);
	}
	return ret;
}

static s8 gtp_request_io_port(struct goodix_ts_data *ts)
{
	s32 ret;

	ret = GTP_GPIO_REQUEST(ts->data->irq, "GTP_INT_IRQ");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d",
				(s32)ts->data->irq, ret);
		ret = -ENODEV;
	} else
		GTP_GPIO_AS_INT(ts->data->irq);

	ret = GTP_GPIO_REQUEST(ts->data->reset, "GTP_RST_PORT");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d",
				(s32)ts->data->reset, ret);
		ret = -ENODEV;
	}

	GTP_GPIO_AS_INPUT(ts->data->reset);
	gtp_reset_guitar(ts->client, 20);

	if (ret < 0) {
		GTP_GPIO_FREE(ts->data->reset);
		GTP_GPIO_FREE(ts->data->irq);
	}

	return ret;
}

static s8 gtp_request_irq(struct goodix_ts_data *ts)
{
	const u8 irq_table[] = GTP_IRQ_TAB;
	s32 ret;

	GTP_DEBUG("INT trigger type:%x", ts->int_trigger_type);

	ret  = request_irq(ts->client->irq, goodix_ts_irq_handler,
			irq_table[ts->int_trigger_type], ts->client->name, ts);
	if (ret) {
		GTP_ERROR("Request IRQ failed!ERRNO:%d.", ret);
		GTP_GPIO_AS_INPUT(ts->data->irq);
		GTP_GPIO_FREE(ts->data->irq);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		ts->use_irq = 0;
		return -1;
	} else {
		gtp_irq_disable(ts);
		ts->use_irq = 1;
		return 0;
	}
}

static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
	s8 phys[32];
#if GTP_HAVE_TOUCH_KEY
	u8 index = 0;
#endif
	s8 ret;

	GTP_DEBUG_FUNC();

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		GTP_ERROR("Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] =
			BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#if GTP_ICS_SLOT_REPORT
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	input_mt_init_slots(ts->input_dev, 255, INPUT_MT_DIRECT);
#else
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif

#if GTP_HAVE_TOUCH_KEY
	for (index = 0; index < GTP_MAX_KEY_NUM; index++)
		input_set_capability(ts->input_dev, EV_KEY,
					touch_key_array[index]);
#endif

#if GTP_CHANGE_X2Y
	GTP_SWAP(ts->abs_x_max, ts->abs_y_max);
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,
							ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
							ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	sprintf(phys, "input/ts");
	ts->input_dev->name = goodix_ts_name;
	ts->input_dev->phys = phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;
	ts->input_dev->dev.parent = &ts->client->dev;

	ret = input_register_device(ts->input_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed",
					ts->input_dev->name);
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_OF
static int gtp_probe_dt(struct i2c_client *client)
{
	struct gtp_platform_data *gtp_data;
	struct device_node *np = client->dev.of_node;

	gtp_data = kzalloc(sizeof(*gtp_data), GFP_KERNEL);
	if (gtp_data == NULL) {
		GTP_ERROR("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	client->dev.platform_data = gtp_data;
	gtp_data->irq = of_get_named_gpio(np, "irq-gpios", 0);
	if (gtp_data->irq < 0) {
		GTP_ERROR("of_get_named_gpio irq failed.");
		return -EINVAL;
	}
	gtp_data->reset = of_get_named_gpio(np, "reset-gpios", 0);
	if (gtp_data->reset < 0) {
		GTP_ERROR("of_get_named_gpio reset failed.");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "goodix,cfg-group",
			 &gtp_data->gtp_cfg_group))
		GTP_INFO("cfg-group property not set, use default value 0\n");

#if GTP_CUSTOM_CFG
	if (of_property_read_u32(np, "goodix,max-height",
			 &gtp_data->gtp_max_height)) {
		GTP_ERROR("failed to get goodix,max-height property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "goodix,max-width",
			&gtp_data->gtp_max_width)) {
		GTP_ERROR("failed to get goodix,max-width property\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static struct of_device_id goodix_ts_dt_ids[] = {
	{ .compatible = "goodix,gt913-touch", },
	{}
};
#endif


static int goodix_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct goodix_ts_data *ts;
	u16 version_info;
	s32 ret = 0;

	GTP_DEBUG_FUNC();

	GTP_DEBUG("GTP Driver Version:%s", GTP_DRIVER_VERSION);
	GTP_DEBUG("GTP Driver build@%s,%s", __TIME__, __DATE__);
	GTP_DEBUG("GTP I2C Address:0x%02x", client->addr);

	i2c_connect_client = client;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		GTP_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		GTP_ERROR("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	memset(ts, 0, sizeof(*ts));
	INIT_WORK(&ts->work, goodix_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
#ifdef CONFIG_OF
	ret = gtp_probe_dt(client);
	if (ret == -ENOMEM) {
		GTP_ERROR("Probe device tree data failed.");
		kfree(ts);
		return ret;
	} else if (ret == -EINVAL) {
		GTP_ERROR("Probe device tree data failed.");
		kfree(client->dev.platform_data);
		kfree(ts);
		return ret;
	}
	ts->gtp_avdd = regulator_get(&client->dev, "avdd");
	ts->data = client->dev.platform_data;
#else
	ts->data = client->dev.platform_data;
	ts->gtp_avdd = regulator_get(NULL, ts->data->gtp_avdd_name);
#endif

	if (IS_ERR(ts->gtp_avdd)) {
		GTP_ERROR("goodix touch avdd power supply get failed\n");
		goto out;
	}
	regulator_set_voltage(ts->gtp_avdd, 2800000, 2800000);
	if (!regulator_en) {
		if (regulator_enable(ts->gtp_avdd)) {
			GTP_ERROR("gtp_vadd regulator enable failed\n");
			goto out_regulator;
		}
		regulator_en = true;
	}
	ts->gtp_rawdiff_mode = 0;
	spin_lock_init(&ts->irq_lock);
	ret = gtp_request_io_port(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request IO port failed.");
		goto out;
	}

	ret = gtp_i2c_test(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		goto out_io;
	}

	ret = gtp_read_version(client, &version_info);
	if (ret < 0) {
		GTP_ERROR("Read version failed.");
		goto out_i2c;
	}

	ret = gtp_init_panel(ts);
	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		goto out_i2c;
	}

	ret = gtp_request_input_dev(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request input dev failed");
		goto out_i2c;
	}

	ret = gtp_request_irq(ts);
	if (ret < 0)
		GTP_DEBUG("GTP works in polling mode.");
	else
		GTP_DEBUG("GTP works in interrupt mode.");

	gtp_irq_enable(ts);

#if GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	queue_delayed_work(gtp_esd_check_workqueue,
				&gtp_esd_check_work, GTP_ESD_CHECK_CIRCLE);
#endif
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif
	return 0;

out_i2c:
	i2c_set_clientdata(client, NULL);
out_io:
	GTP_GPIO_FREE(ts->data->reset);
	GTP_GPIO_FREE(ts->data->irq);
	if (regulator_en) {
		if (regulator_disable(ts->gtp_avdd))
			GTP_ERROR("gtp_vadd regulator disable failed\n");
		else
			regulator_en = false;
	}
out_regulator:
	regulator_put(ts->gtp_avdd);
out:
#ifdef CONFIG_OF
	kfree(ts->data);
#endif
	kfree(ts);
	return ret;
}

static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

	if (ts) {
		if (ts->use_irq) {
			GTP_GPIO_AS_INPUT(ts->data->irq);
			GTP_GPIO_FREE(ts->data->irq);
			free_irq(client->irq, ts);
		} else
			hrtimer_cancel(&ts->timer);
	}

	GTP_DEBUG("GTP driver is removing...");

#ifdef CONFIG_OF
	kfree(ts->data);
#endif
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);

	if (regulator_en) {
		if (regulator_disable(ts->gtp_avdd))
			GTP_ERROR("gtp_vadd regulator disable failed\n");
		else
			regulator_en = false;
	}
	regulator_put(ts->gtp_avdd);

	kfree(ts);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int gtp_enter_sleep(struct goodix_ts_data *ts)
{
	u8 i2c_control_buf[3] = {
		(u8)(GTP_REG_SLEEP >> 8),
		(u8)GTP_REG_SLEEP,
		5
	};
	u8 retry = 0;
	int ret = -1;

	GTP_DEBUG_FUNC();

	GTP_GPIO_OUTPUT(ts->data->irq, 0);
	GTP_MSLEEP(5);
	while (retry++ < 5) {
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			GTP_DEBUG("GTP enter sleep!");
			return ret;
		}
		GTP_MSLEEP(10);
	}
	GTP_ERROR("GTP send sleep cmd failed.");
	return ret;
}

static int gtp_wakeup_sleep(struct goodix_ts_data *ts)
{
	u8 retry = 0;
	int ret = -1;

	GTP_DEBUG_FUNC();

	/* MUST delay >20ms before send cfg */
	GTP_MSLEEP(20);

#if GTP_POWER_CTRL_SLEEP
	while (retry++ < 5) {
		gtp_reset_guitar(ts->client, 20);
		ret = gtp_send_cfg(ts->client);
		if (ret > 0) {
			GTP_DEBUG("Wakeup sleep send config success.");
			return ret;
		}
	}
#else
	while (retry++ < 10) {
		GTP_GPIO_OUTPUT(ts->data->irq, 1);
		GTP_MSLEEP(5);
		ret = gtp_i2c_test(ts->client);
		if (ret > 0) {
			GTP_DEBUG("GTP wakeup sleep.");
			gtp_int_sync(ts->client, 25);
			return ret;
		}
		gtp_reset_guitar(ts->client, 20);
	}
#endif

	GTP_ERROR("GTP wakeup sleep failed.");
	return ret;
}

static int goodix_runtime_suspend(struct device *dev)
{
	struct goodix_ts_data *ts;
	int ret = -1;

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);
#if GTP_ESD_PROTECT
	ts->gtp_is_suspend = 1;
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif
	if (ts->use_irq)
		gtp_irq_disable(ts);
	else
		hrtimer_cancel(&ts->timer);

	ret = gtp_enter_sleep(ts);
	if (ret < 0) {
		GTP_ERROR("GTP runtime suspend failed.");
		return ret;
	}

	if (regulator_en) {
		if (regulator_disable(ts->gtp_avdd))
			GTP_ERROR("gtp_vadd regulator disable failed\n");
		else
			regulator_en = false;
	}

	return 0;
}

static int goodix_runtime_resume(struct device *dev)
{
	struct goodix_ts_data *ts;
	int ret = -1;

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);

	if (!regulator_en) {
		if (regulator_enable(ts->gtp_avdd))
			GTP_ERROR("gtp_vadd regulator enable failed\n");
		else
			regulator_en = true;
	}

	ret = gtp_wakeup_sleep(ts);
	if (ret < 0) {
		GTP_ERROR("GTP runtime resume failed.");
		return ret;
	}

	if (ts->use_irq)
		gtp_irq_enable(ts);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

#if GTP_ESD_PROTECT
	ts->gtp_is_suspend = 0;
	queue_delayed_work(gtp_esd_check_workqueue,
				&gtp_esd_check_work, GTP_ESD_CHECK_CIRCLE);
#endif
	return 0;
}

static const struct dev_pm_ops goodix_ts_pmops = {
	SET_RUNTIME_PM_OPS(goodix_runtime_suspend, goodix_runtime_resume, NULL)
};
#endif

#if GTP_ESD_PROTECT
static void gtp_esd_check_func(struct work_struct *work)
{
	struct goodix_ts_data *ts = NULL;
	u8 test[3] = {
		GTP_REG_CONFIG_DATA >> 8,
		GTP_REG_CONFIG_DATA & 0xff
	};
	s32 i, ret;

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);

	if (ts->gtp_is_suspend)
		return;

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read(i2c_connect_client, test, 3);
		if (ret >= 0)
			break;
	}

	if (i >= 3)
		gtp_reset_guitar(ts->client, 50);

	if (!ts->gtp_is_suspend)
		queue_delayed_work(gtp_esd_check_workqueue,
			&gtp_esd_check_work, GTP_ESD_CHECK_CIRCLE);

	return;
}
#endif

static const struct i2c_device_id goodix_ts_id[] = {
	{ GTP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe		= goodix_ts_probe,
	.remove		= goodix_ts_remove,
	.id_table	= goodix_ts_id,
	.driver = {
		.name	= GTP_I2C_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm	= &goodix_ts_pmops,
#endif
		.of_match_table = of_match_ptr(goodix_ts_dt_ids),
	},
};

static int __init goodix_ts_init(void)
{
	s32 ret;

	GTP_DEBUG_FUNC();
	GTP_DEBUG("GTP driver install.");
	goodix_wq = create_singlethread_workqueue("goodix_wq");
	if (!goodix_wq) {
		GTP_ERROR("Creat workqueue failed.");
		return -ENOMEM;
	}

	ret = i2c_add_driver(&goodix_ts_driver);
	return ret;
}

static void __exit goodix_ts_exit(void)
{
	GTP_DEBUG_FUNC();
	GTP_DEBUG("GTP driver exited.");
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq)
		destroy_workqueue(goodix_wq);
}

late_initcall(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("GTP Series Driver");
MODULE_LICENSE("GPL");
