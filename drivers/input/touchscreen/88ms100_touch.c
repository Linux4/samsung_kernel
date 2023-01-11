/*
 * Marvell 88MS100 Touch Controller Driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/input/mt.h>
#include <linux/pm_runtime.h>
#include <linux/i2c/88ms100_touch.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/firmware.h>
#include <linux/configfs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#if MS100_USE_FW_HEX
#define Ascii2Hex(data) ((data >= '0' && data <= '9')?(data - '0'):((data >= \
			'A' && data <= 'F')?(data - 'A' + 10):((data >= \
				'a' && data <= 'f')?(data - 'a' + 10):0)))
u8 fw_buf[200] = {0};
#endif

static const char *ms100_ts_name = "MRVL 88MS100 TouchScreen";
static struct workqueue_struct *ms100_wq;
struct i2c_client *i2c_connect_client;
struct dentry *ms100_debugfs_root;
struct ms100_ts_data *ts;
static int use_update;
static int point_save;

#if MS100_HAVE_TOUCH_KEY
static const u16 touch_key_array[] = MS100_KEY_TAB;
#define MS100_MAX_KEY_NUM  (sizeof(touch_key_array) / \
	sizeof(touch_key_array[0]))
#endif

static s32 ms100_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	u8 retries = 0;
	s32 ret;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = MS100_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - MS100_ADDR_LENGTH;
	msgs[1].buf   = &buf[MS100_ADDR_LENGTH];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if (retries >= 5) {
		MS100_DEBUG("I2C retry timeout, reset chip.");
		ms100_reset_guitar(client, 10);
	}

	return ret;
}

static s32 ms100_i2c_read_buf(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	u8 retries = 0;
	s32 ret;

	msg.flags = I2C_M_RD;
	msg.addr = client->addr;
	msg.len = len;
	msg.buf = &buf[0];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	if (retries >= 5) {
		MS100_INFO("I2C retry timeout, reset chip.");
		ms100_reset_guitar(client, 10);
	}
	return ret;
}

static s32 ms100_i2c_read_byte(struct i2c_client *client, u8 *buf)
{
	return ms100_i2c_read_buf(client, buf, 1);
}


static s32 ms100_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	u8 retries = 0;
	s32 ret;

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
		MS100_DEBUG("I2C retry timeout, reset chip.");
		ms100_reset_guitar(client, 10);
	}

	return ret;
}

#if MS100_USE_FW_HEX
static u8 ascii2hex(u8 data)
{
	u8 temp = data;
	temp = Ascii2Hex(temp);
	return temp;
}

static s32 Hex2bin(const struct firmware *firmware, u8 *buffer, int *len)
{
	const u8 *src = firmware->data;
	u8 *begin = buffer;
	struct i2c_client *client = i2c_connect_client;
	int data_len, ret;
	u8 data_type, temp;
	int temp_len = 0;
	int i, n, m, j = 0, v = 0;
	unsigned int  addr;
	unsigned int enter_addr = 0;
	u8 sym, sym_1;
	u8 i2c_snd_buf[200];
	int count = 0;
	while (*src != '\0') {
		while (*src != ':')
			src++;
		src++;
		data_len = ascii2hex(*src++);
		data_len <<= 4;
		data_len += ascii2hex(*src++);
		src += 4;
		data_type = ascii2hex(*src++);
		data_type <<= 4;
		data_type += ascii2hex(*src++);

		switch (data_type) {
		case 0:
			for (i = 0; i < data_len; i++) {
				temp = ascii2hex(*src++);
				temp <<= 4;
				temp += ascii2hex(*src++);
				*buffer = temp;
				buffer++;
				temp_len++;

				if (temp_len == 116) {
					addr = 0x20000000 + j*116;
					if (v == 0)
						v = 0x40;
					else
						v = 0;
					i2c_snd_buf[0] = 127;
					i2c_snd_buf[1] = 0x03;
					i2c_snd_buf[2] = v;
					i2c_snd_buf[3] = 116 / 4;
					i2c_snd_buf[4] = 0xff;
					i2c_snd_buf[5] = 0xff;
					i2c_snd_buf[6] = 0xff;
					i2c_snd_buf[7] = 0xff;
					i2c_snd_buf[8] = addr & 0xff;
					i2c_snd_buf[9] = (addr >> 8) & 0xff;
					i2c_snd_buf[10] = (addr >> 16) & 0xff;
					i2c_snd_buf[11] = (addr >> 24) & 0xff;
					for (n = 0; n < 116; n++)
						i2c_snd_buf[n + 12] =
						    *(begin + n);
					ret = ms100_i2c_write(client,
						i2c_snd_buf, 128);
					if (ret < 0) {
						MS100_INFO("Exit%s,ret = %d",
							__func__, ret);
						return ret;
					}
					ms100_i2c_read_byte(client, &sym);
					for (m = 0; m < sym; m++)
						ms100_i2c_read_byte(client,
							&sym_1);
					if (count < 1) {
						enter_addr = *(begin + 7);
						enter_addr <<= 8;
						enter_addr += *(begin + 6);
						enter_addr <<= 8;
						enter_addr += *(begin + 5);
						enter_addr <<= 8;
						enter_addr += *(begin + 4);
						count++;
					}
					j++;
					temp_len = 0;
					buffer = begin;
				}
			}
			*len += data_len;
			break;
		case 1:
			addr = 0x20000000 + j*116;
			j = *len % 116;
			if (j) {
				if (v == 0)
					v = 0x40;
				else
					v = 0;
				i2c_snd_buf[0] = j + 11;
				i2c_snd_buf[1] = 0x03;
				i2c_snd_buf[2] = v;
				i2c_snd_buf[3] = j / 4;
				i2c_snd_buf[4] = 0xff;
				i2c_snd_buf[5] = 0xff;
				i2c_snd_buf[6] = 0xff;
				i2c_snd_buf[7] = 0xff;
				i2c_snd_buf[8] = addr & 0xff;
				i2c_snd_buf[9] = (addr >> 8) & 0xff;
				i2c_snd_buf[10] = (addr >> 16) & 0xff;
				i2c_snd_buf[11] = (addr >> 24) & 0xff;
				for (n = 0 ; n < 116; n++)
					i2c_snd_buf[n + 12] =
					    *(begin + n);
				ret = ms100_i2c_write(client,
					i2c_snd_buf, j + 12);
				if (ret < 0) {
					MS100_INFO("Exit%s, ret = %d",
						__func__, ret);
					return ret;
				}
				ms100_i2c_read_byte(client, &sym);
				for (m = 0; m < sym; m++)
					ms100_i2c_read_byte(client,
						&sym_1);
				buffer = begin;
			}
			{
				if (v == 0)
					v = 0x40;
				else
					v = 0;
				i2c_snd_buf[0] = 0x0b;
				i2c_snd_buf[1] = 0x05;
				i2c_snd_buf[2] = v;
				i2c_snd_buf[3] = 0x01;
				i2c_snd_buf[4] = 0xff;
				i2c_snd_buf[5] = 0xff;
				i2c_snd_buf[6] = 0xff;
				i2c_snd_buf[7] = 0xff;

				enter_addr -= 1;
				MS100_INFO("enter_addr = %x",
					enter_addr);
				i2c_snd_buf[8] = enter_addr;
				i2c_snd_buf[9] = enter_addr >> 8;
				i2c_snd_buf[10] = enter_addr >> 16;
				i2c_snd_buf[11] = enter_addr >> 24;

				ret = ms100_i2c_write(client,
					i2c_snd_buf, 12);
				if (ret < 0)
					return ret;
			}
			return 0;
		default:
			break;
		}
	}
	return -1;
}
#endif

static void ms100_irq_disable(struct ms100_ts_data *ts)
{
	unsigned long irqflags;

	MS100_DEBUG();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) {
		ts->irq_is_disable = 1;
		disable_irq_nosync(ts->client->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static void ms100_irq_enable(struct ms100_ts_data *ts)
{
	unsigned long irqflags;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) {
		enable_irq(ts->client->irq);
		ts->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static void ms100_touch_down(struct ms100_ts_data *ts, s32 id,
		s32 x, s32 y, s32 w, s32 p)
{
#if MS100_ICS_SLOT_REPORT
	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, p);
#else
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, p);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	input_mt_sync(ts->input_dev);
#endif
	MS100_DEBUG("ID:%d, X:%d, Y:%d, W:%d, P:%d", id, x, y, w, p);
}

static void ms100_touch_up(struct ms100_ts_data *ts, s32 id)
{
#if MS100_ICS_SLOT_REPORT
	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
	input_mt_sync_frame(ts->input_dev);
#else
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	input_mt_sync(ts->input_dev);
#endif
}

static void ms100_ts_work_func(struct work_struct *work)
{
	struct ms100_ts_data *ts = NULL;
	static u16 pre_touch, pre_touch_index;
	static u8 pre_key;
	u8 point_data[2 + 2 + 7 * MS100_MAX_TOUCH + 1] = {
		MS100_READ_COOR_ADDR >> 8,
		MS100_READ_COOR_ADDR & 0xFF
	};
	u8 touch_num = 0;
	u8 finger = 0;
	u8 key_value = 0;
	u8 have_key = 0;
	u8 bufferstatus = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 input_p = 0;
	s32 id = 0;
	s32 i, ret;

	ts = container_of(work, struct ms100_ts_data, work);
	ret = ms100_i2c_read(ts->client, point_data, 11);
	if (ret < 0) {
		MS100_ERROR("I2C transfer error. errno:%d\n ", ret);
		goto exit_work_func;
	}

	finger = point_data[MS100_ADDR_LENGTH];

	touch_num = finger & 0x0f;

	bufferstatus = finger & 0x20;
	if (touch_num > MS100_MAX_TOUCH || bufferstatus)
		goto exit_work_func;

	if (touch_num > 1) {
		u8 buf[7 * MS100_MAX_TOUCH] = {
			(MS100_READ_COOR_ADDR + 9) >> 8,
			(MS100_READ_COOR_ADDR + 9) & 0xff
		};
		ret = ms100_i2c_read(ts->client, buf, 2 + 7 * (touch_num - 1));
		memcpy(&point_data[11], &buf[2], 7 * (touch_num - 1));
	}

	have_key = point_data[3];

#if MS100_HAVE_TOUCH_KEY
	if (have_key || pre_key) {
		if (touch_num > 1) {
			key_value = point_data[2 + 1];
		} else {
			key_value = point_data[2 + 1];
			if (key_value || pre_key) {
				for (i = 0; i < MS100_MAX_KEY_NUM; i++)
					input_report_key(ts->input_dev,
						touch_key_array[i],
						key_value & (0x01 << i));
				touch_num = 0;
				pre_touch = 0;
			}
			if (have_key)
				pre_key = key_value;
			else if (pre_key)
				pre_key = 0;
		}
	}
#endif

#if  MS100_ICS_SLOT_REPORT
	if (pre_touch || touch_num) {
		s32 pos = 0;
		u16 touch_index = 0;

		coor_data = &point_data[4];
		if (touch_num) {
			int k = 0;
			for (; k < touch_num; k++) {
				id = coor_data[pos + 7*k] & 0x0F;
				if (id < MS100_MAX_TOUCH)
					touch_index |= (0x01 << id);
			}
			id = coor_data[pos] & 0x0F;
		}
		MS100_DEBUG("id=%d, touch_index=0x%x, pre_touch=0x%x\n",
				id, touch_index, pre_touch);

		for (i = 0; i < MS100_MAX_TOUCH; i++) {
			int st_len = 0;
			if (touch_index & (0x01<<i)) {
				input_x = coor_data[pos + 1]
					| coor_data[pos + 2] << 8;
				input_y = coor_data[pos + 3]
					| coor_data[pos + 4] << 8;
				input_p = coor_data[pos + 5];
				input_w = coor_data[pos + 6];
				MS100_SWAP(input_x, input_y);
				input_x = 12*1024 - input_x;
				input_x = (ts->abs_x_max*input_x)/(12*1024);
				input_y = (ts->abs_y_max*input_y)/(22*1024);
				input_x = (input_x > 720) ? 720 : input_x;
				input_y = (input_y > 1280) ? 1280 : input_y;
				input_w = (input_w > 12) ? 12 : input_w;
				input_p = (input_p > 255) ? 255 : input_p;

				ms100_touch_down(ts, id, input_x, input_y,
						input_w, input_p);
				pre_touch |= 0x01 << i;
				pos += 7;

				id = coor_data[pos] & 0x0F;
			} else {
				ms100_touch_up(ts, i);
				pre_touch &= ~(0x01 << i);
				pre_touch_index &= ~(0x01 << i);
				if (point_save == 1)
					st_len = 0;
			}
		}
		pre_touch_index = touch_index;
	}
#else
	if (touch_num) {
		for (i = 0; i < touch_num; i++) {
			coor_data = &point_data[i * 7 + 4];
			id = coor_data[0] & 0x0F;
			input_x = coor_data[1] | coor_data[2] << 8;
			input_y = coor_data[3] | coor_data[4] << 8;
			input_p = coor_data[5];
			input_w = coor_data[6];
			MS100_SWAP(input_x, input_y);

			input_x = 12*1024 - input_x;
			input_x = (ts->abs_x_max*input_x)/(12*1024);
			input_y = (ts->abs_y_max*input_y)/(22*1024);
			input_x = (input_x > 720) ? 1280 : input_x;
			input_y = (input_y > 720) ? 1280 : input_x;
			input_w = (input_w > 12) ? 12 : input_w;
			input_p = (input_p > 255) ? 255 : input_p;

			ms100_touch_down(ts, id, input_x, input_y,
					input_w, input_p);
		}
	} else if (pre_touch) {
		MS100_DEBUG("Touch Release!");
		ms100_touch_up(ts, 0);
	}

	pre_touch = touch_num;
	input_report_key(ts->input_dev, BTN_TOUCH, (touch_num || key_value));
#endif

	input_sync(ts->input_dev);

exit_work_func:
	if (ts->use_irq)
		ms100_irq_enable(ts);
}

static enum hrtimer_restart ms100_ts_timer_handler(struct hrtimer *timer)
{
	struct ms100_ts_data *ts =
		container_of(timer, struct ms100_ts_data, timer);

	queue_work(ms100_wq, &ts->work);
	hrtimer_start(&ts->timer,
		ktime_set(0, (MS100_POLL_TIME + 6) * 1000000),
		HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}


static irqreturn_t ms100_ts_irq_handler(int irq, void *dev_id)
{
	struct ms100_ts_data *ts = dev_id;

	ms100_irq_disable(ts);
	queue_work(ms100_wq, &ts->work);

	return IRQ_HANDLED;
}

static void ms100_int_sync(struct i2c_client *client, s32 ms)
{
	struct ms100_platform_data *ms100_data = client->dev.platform_data;

	gpio_direction_output(ms100_data->irq, 0);
	MS100_MSLEEP(ms);
	gpio_direction_input(ms100_data->irq);
}

void ms100_reset_guitar(struct i2c_client *client, s32 ms)
{
	struct ms100_platform_data *ms100_data = client->dev.platform_data;


	/* begin select I2C slave addr */
	gpio_direction_output(ms100_data->reset, 0);
	MS100_MSLEEP(ms);
	gpio_direction_output(ms100_data->irq,
		client->addr == 0x18);

	MS100_MSLEEP(2);
	gpio_direction_output(ms100_data->reset, 1);

	/* must > 3ms */
	MS100_MSLEEP(6);
	gpio_direction_input(ms100_data->reset);
	/* end select I2C slave addr */

	ms100_int_sync(client, 50);
}

static s32 ms100_init_panel(struct ms100_ts_data *ts)
{
	ts->abs_x_max = ts->data->ms100_max_width;
	ts->abs_y_max = ts->data->ms100_max_height;
	ts->int_trigger_type = MS100_INT_TRIGGER;
	MS100_DEBUG("X_MAX = %d,Y_MAX = %d,TRIGGER = 0x%02x", ts->abs_x_max,
		ts->abs_y_max, ts->int_trigger_type);
	MS100_MSLEEP(10);

	return 0;
}

static s8 ms100_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {
		MS100_READ_COOR_ADDR >> 8,
		MS100_READ_COOR_ADDR & 0xff
	};
	u8 retry = 0;
	s8 ret;

	while (retry++ < 5) {
		ret = ms100_i2c_read(client, test, 3);
		if (ret > 0) {
			MS100_INFO("MS100 0x0000: 0x%2x", test[2]);
			return ret;
		}

		MS100_ERROR("MS100 i2c test failed time %d.", retry);
		MS100_MSLEEP(10);
	}
	return ret;
}

static s8 ms100_request_io_port(struct ms100_ts_data *ts)
{
	s32 ret;

	ret = gpio_request(ts->data->irq, "MS100_INT_IRQ");
	if (ret < 0) {
		MS100_ERROR("Failed to request GPIO:%d, ERRNO:%d",
			(s32)ts->data->irq, ret);
		ret = -ENODEV;
	} else
		gpio_direction_input(ts->data->irq);

	ret = gpio_request(ts->data->reset, "MS100_RST_PORT");
	if (ret < 0) {
		MS100_ERROR("Failed to request GPIO:%d, ERRNO:%d",
			(s32)ts->data->reset, ret);
		ret = -ENODEV;
	}

	gpio_direction_input(ts->data->reset);
	ms100_reset_guitar(ts->client, 20);

	if (ret < 0) {
		gpio_free(ts->data->reset);
		gpio_free(ts->data->irq);
	}

	return ret;
}

static s8 ms100_request_irq(struct ms100_ts_data *ts)
{
	s32 ret;

	MS100_DEBUG("INT trigger type:%x", ts->int_trigger_type);
	ret  = request_irq(gpio_to_irq(ts->data->irq), ms100_ts_irq_handler,
			IRQ_TYPE_EDGE_FALLING, ts->client->name, ts);
	if (ret) {
		MS100_ERROR("Request IRQ failed!ERRNO:%d.", ret);
		gpio_direction_input(ts->data->irq);
		gpio_free(ts->data->irq);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = ms100_ts_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		ts->use_irq = 0;
		return -1;
	} else {
		ms100_irq_disable(ts);
		ts->use_irq = 1;
		return 0;
	}
}

static s8 ms100_request_input_dev(struct ms100_ts_data *ts)
{
	s8 phys[32];
	u8 index = 0;
	s8 ret;

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		MS100_ERROR("Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] =
		BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#if MS100_ICS_SLOT_REPORT
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	input_mt_init_slots(ts->input_dev, 255, INPUT_MT_DIRECT);
#else
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif
	for (index = 0; index < MS100_MAX_KEY_NUM; index++)
		input_set_capability(ts->input_dev, EV_KEY,
			touch_key_array[index]);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 12, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 12, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	sprintf(phys, "input/ts");
	ts->input_dev->name = ms100_ts_name;
	ts->input_dev->phys = phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAF;
	ts->input_dev->id.product = 0xBEEC;
	ts->input_dev->id.version = 10420;
	ts->input_dev->dev.parent = &ts->client->dev;

	ret = input_register_device(ts->input_dev);
	if (ret) {
		MS100_ERROR("Register %s input device failed",
			ts->input_dev->name);
		return -ENODEV;
	}
	if (ts->data->set_virtual_key)
		ret = ts->data->set_virtual_key(ts->input_dev);
	BUG_ON(ret != 0);
	return 0;
}

static s32 ms100_fw_shakehand(struct i2c_client *client,
	const struct firmware *firmware) {
	s32 ret, time = 0;
	u8 buf;
	u8 sym;
	do {
		ret = ms100_i2c_read_byte(client, &buf);
		if (ret < 0) {
			MS100_ERROR("I2C transfer error. errno:%d\n", ret);
			goto exit_fw_shakehand;
		}
		if (buf == '#') {
			ret = ms100_i2c_write(client, &buf, 1);
			if (ret < 0) {
				MS100_ERROR("I2C transfer error, errno:%d\n",
				       ret);
				goto exit_fw_shakehand;
			}
			ms100_i2c_read_byte(client, &sym);
			ms100_i2c_read_byte(client, &sym);
			ms100_i2c_read_byte(client, &sym);
			break;
		}
		MS100_INFO("t=%d", time);
		time++;
		msleep(100+time);
		if (time > 20)
			goto exit_fw_shakehand;
	} while (1);

	return 0;
exit_fw_shakehand:
	return -1;
}

static s32 ms100_fw_download(struct i2c_client *client,
	const struct firmware *firmware) {
	u8 i2c_snd_buf[2000];
#if READ
	u8 i2c_rcv_buf[2000];
#endif
	u8 sym, sym_1;
	int len, i, j, ret = 0;
	int v = 0, repeat_max = 10;
	unsigned int  addr;
	int m, n;
	len = firmware->size;
	i = len / 116;
	for (j = 0; j < i; j++) {
		addr = 0x20000000 + j*116;
#if READ
repeat:
		if (v == 0)
			v = 0x40;
		else
			v = 0;
		i2c_snd_buf[0] = 127;
		i2c_snd_buf[1] = 0x03;
		i2c_snd_buf[2] = v;
		i2c_snd_buf[3] = 116 / 4;
		i2c_snd_buf[4] = 0xff;
		i2c_snd_buf[5] = 0xff;
		i2c_snd_buf[6] = 0xff;
		i2c_snd_buf[7] = 0xff;

		i2c_snd_buf[8] = addr & 0xff;
		i2c_snd_buf[9] = (addr >> 8) & 0xff;
		i2c_snd_buf[10] = (addr >> 16) & 0xff;
		i2c_snd_buf[11] = (addr >> 24) & 0xff;

		for (n = 0; n < 116; n++)
			i2c_snd_buf[n + 12] = firmware->data[j*116 + n];
		ret = ms100_i2c_write(client, i2c_snd_buf, 128);
#endif
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}
		ms100_i2c_read_byte(client, &sym);
		for (m = 0; m < sym; m++)
			ms100_i2c_read_byte(client, &sym_1);
#if READ
		if (v == 0)
			v = 0x40;
		else
			v = 0;
		i2c_snd_buf[0] = 0x0b;
		i2c_snd_buf[1] = 0x02;
		i2c_snd_buf[2] = v;
		i2c_snd_buf[3] = 116 / 4;
		i2c_snd_buf[4] = 0xff;
		i2c_snd_buf[5] = 0xff;
		i2c_snd_buf[6] = 0xff;
		i2c_snd_buf[7] = 0xff;
		i2c_snd_buf[8] = addr & 0xff;
		i2c_snd_buf[9] = (addr >> 8) & 0xff;
		i2c_snd_buf[10] = (addr >> 16) & 0xff;
		i2c_snd_buf[11] = (addr >> 24) & 0xff;
		ret = ms100_i2c_write(client, i2c_snd_buf, 12);
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}

#if READ_ALL
		ret = ms100_i2c_read_buf(client, i2c_rcv_buf, 120);
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}
#endif

#if READ_ONEBYTE
		for (n = 0; n < 120; n++) {
			ret = ms100_i2c_read_byte(client, &i2c_rcv_buf[n]);
			if (ret < 0) {
				MS100_INFO("Exit%s,ret = %d", __func__, ret);
				return ret;
			}
		}
#endif

		for (n = 0; n < 116; n++) {
			if (i2c_snd_buf[n + 12] != i2c_rcv_buf[n + 4]) {
				repeat_max--;
				if (repeat_max) {
					if (v == 0)
						v = 0x40;
					else
						v = 0;
					MS100_INFO("goto repeqt!");
					goto repeat;
				} else {
					MS100_INFO("Exit%s,ret = %d",
						__func__, ret);
					return -1;
				}
			}
		}
#endif
	}
	addr = 0x20000000 + i*116;
	j = len % 116;
	if (j) {
		repeat_max = 10;

		if (v == 0)
			v = 0x40;
		else
			v = 0;
		i2c_snd_buf[0] = j + 11;
		i2c_snd_buf[1] = 0x03;
		i2c_snd_buf[2] = v;
		i2c_snd_buf[3] = j / 4;
		i2c_snd_buf[4] = 0xff;
		i2c_snd_buf[5] = 0xff;
		i2c_snd_buf[6] = 0xff;
		i2c_snd_buf[7] = 0xff;
		i2c_snd_buf[8] = addr & 0xff;
		i2c_snd_buf[9] = (addr >> 8) & 0xff;
		i2c_snd_buf[10] = (addr >> 16) & 0xff;
		i2c_snd_buf[11] = (addr >> 24) & 0xff;
		for (n = 0 ; n < 116; n++)
			i2c_snd_buf[n + 12] = firmware->data[i*116 + n];
		ret = ms100_i2c_write(client, i2c_snd_buf, j + 12);
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}
		ms100_i2c_read_byte(client, &sym);
		for (m = 0; m < sym; m++)
			ms100_i2c_read_byte(client, &sym_1);
#if READ
		if (v == 0)
			v = 0x40;
		else
			v = 0;
		i2c_snd_buf[0] = 0x0b;
		i2c_snd_buf[1] = 0x02;
		i2c_snd_buf[2] = v;
		i2c_snd_buf[3] = j / 4;
		i2c_snd_buf[4] = 0xff;
		i2c_snd_buf[5] = 0xff;
		i2c_snd_buf[6] = 0xff;
		i2c_snd_buf[7] = 0xff;
		i2c_snd_buf[8] = addr & 0xff;
		i2c_snd_buf[9] = (addr >> 8) & 0xff;
		i2c_snd_buf[10] = (addr >> 16) & 0xff;
		i2c_snd_buf[11] = (addr >> 24) & 0xff;
		ret = ms100_i2c_write(client, i2c_snd_buf, 12);
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}
#if READ_ALL
		ret = ms100_i2c_read_buf(client, i2c_rcv_buf, j + 4);
		if (ret < 0) {
			MS100_INFO("Exit%s,ret = %d", __func__, ret);
			return ret;
		}
#endif
#if READ_ONEBYTE
		for (n = 0; n < j + 4; n++) {
			ret = ms100_i2c_read_byte(client, &i2c_rcv_buf[n]);
			if (ret < 0) {
				MS100_INFO("Exit%s,ret = %d", __func__, ret);
				return ret;
			}
		}
#endif
		for (n = 0; n < j; n++) {
			if (i2c_snd_buf[n + 12] != i2c_rcv_buf[n + 4]) {
				repeat_max--;
				if (repeat_max) {
					if (v == 0)
						v = 0x40;
					else
						v = 0;
					goto repeat;
				} else {
					MS100_INFO("Exit%s,ret = %d",
						__func__, ret);
					return -1;
				}
			}
		}
#endif
	}
	{
		unsigned int enter_addr;
		if (v == 0)
			v = 0x40;
		else
			v = 0;
		i2c_snd_buf[0] = 0x0b;
		i2c_snd_buf[1] = 0x05;
		i2c_snd_buf[2] = v;
		i2c_snd_buf[3] = 0x01;
		i2c_snd_buf[4] = 0xff;
		i2c_snd_buf[5] = 0xff;
		i2c_snd_buf[6] = 0xff;
		i2c_snd_buf[7] = 0xff;

		enter_addr = firmware->data[7];
		enter_addr <<= 8;
		enter_addr += firmware->data[6];
		enter_addr <<= 8;
		enter_addr += firmware->data[5];
		enter_addr <<= 8;
		enter_addr += firmware->data[4];

		enter_addr -= 1;
		i2c_snd_buf[8] = enter_addr;
		i2c_snd_buf[9] = enter_addr >> 8;
		i2c_snd_buf[10] = enter_addr >> 16;
		i2c_snd_buf[11] = enter_addr >> 24;

		ret = ms100_i2c_write(client, i2c_snd_buf, 12);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static void ms100_request_fw_callback(const struct firmware *firmware,
	void *context)
{
	struct ms100_ts_data *ts = context;
	s32 ret;
#if MS100_USE_FW_HEX
	int len = 0;
	struct firmware fw;
#endif
	if (firmware) {
#if MS100_USE_FW_HEX
		ret = ms100_fw_shakehand(ts->client, &fw);
		Hex2bin(firmware, fw_buf, &len);
		fw.size = len;
#else
		ret = ms100_fw_shakehand(ts->client, firmware);
#endif
		if (ret < 0)
			goto exit_fw_callback;
#if MS100_USE_FW_HEX
		ret = ms100_fw_download(ts->client, &fw);
#else
		ret = ms100_fw_download(ts->client, firmware);
#endif
		if (ret < 0)
			goto exit_fw_callback;
	}
exit_fw_callback:
	if (!use_update)
		release_firmware(firmware);
}

static const struct firmware *fw;

static int ms100_probe_dt(struct i2c_client *client)
{
	struct ms100_platform_data *ms100_data;
	struct device_node *np = client->dev.of_node;

	ms100_data = kzalloc(sizeof(*ms100_data), GFP_KERNEL);
	if (ms100_data == NULL) {
		MS100_ERROR("Alloc GFP_KENNEL memory failed");
		return -ENOMEM;
	}

	client->dev.platform_data = ms100_data;

	ms100_data->irq = of_get_named_gpio(np, "irq-gpios", 0);
	if (ms100_data->irq < 0) {
		MS100_ERROR("of_get_named_gpio irq failed");
		return -EINVAL;
	}

	ms100_data->reset = of_get_named_gpio(np, "reset-gpios", 0);
	if (ms100_data->reset < 0) {
		MS100_ERROR("of_get_named_gpio reset failed");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "marvell,max-height",
		    &ms100_data->ms100_max_height)) {
		MS100_ERROR("failed to get marvell,max-height property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "marvell,max-width",
		    &ms100_data->ms100_max_width)) {
		MS100_ERROR("failed to get marvell,max-width property\n");
		return -EINVAL;
	}

	return 0;
}

static struct of_device_id marvell_ts_dt_ids[] = {
	{ .compatible = "marvell,88ms100-touch"},
	{}
};

static int ms100_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	s32 ret = 0;

#if MS100_USE_FW_BIN
	const char *fw_name = "mtouch1_fw.bin";
#else
	const char *fw_name = "mtouch1_fw.hex";
#endif

	i2c_connect_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		MS100_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		MS100_ERROR("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	memset(ts, 0, sizeof(*ts));
	ts->client = client;
	i2c_set_clientdata(client, ts);

	ret = ms100_probe_dt(client);

	if (ret == -ENOMEM) {
		MS100_ERROR("probe device tree source failed");
		kfree(ts);
		return ret;
	} else if (ret == -EINVAL) {
		MS100_ERROR("probe device tree source failed");
		kfree(client->dev.platform_data);
		kfree(ts);
		return ret;
	}

	ts->data = client->dev.platform_data;

	if (ts->data->set_power)
		ts->data->set_power(1);
	spin_lock_init(&ts->irq_lock);
	ret = ms100_request_io_port(ts);
	if (ret < 0) {
		MS100_ERROR("MS100 request IO port failed.");
		goto out;
	}

	gpio_direction_output(ts->data->reset, 0);
	MS100_MSLEEP(2);
	gpio_direction_output(ts->data->reset, 1);
	MS100_INFO("begin download fw!");
	ret = request_firmware(&fw, fw_name, &client->dev);
	if (ret < 0)
		MS100_ERROR("request firmware error %d !\n", ret);
	ms100_request_fw_callback(fw, ts);
	MS100_INFO("download fw end!");
	use_update = 0;
	INIT_WORK(&ts->work, ms100_ts_work_func);
	ret = ms100_i2c_test(client);
	if (ret < 0) {
		MS100_ERROR("I2C communication ERROR!");
		goto out_io;
	}

	ret = ms100_init_panel(ts);
	if (ret < 0) {
		MS100_ERROR("MS100 init panel failed.");
		goto out_i2c;
	}

	ret = ms100_request_input_dev(ts);
	if (ret < 0) {
		MS100_ERROR("MS100 request input dev failed");
		goto out_i2c;
	}

	ret = ms100_request_irq(ts);
	if (ret < 0)
		MS100_INFO("MS100 works in polling mode.");
	else
		MS100_INFO("MS100 works in interrupt mode.");

	ms100_irq_enable(ts);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
#endif

	return 0;
out_i2c:
	i2c_set_clientdata(client, NULL);
out_io:
	gpio_free(ts->data->reset);
	gpio_free(ts->data->irq);
out:
	if (ts->data->set_power)
		ts->data->set_power(0);
	kfree(ts);
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int ms100_enter_sleep(struct ms100_ts_data *ts)
{
	u8 i2c_control_buf[3] = {
		MS100_REG_SLEEP >> 8,
		MS100_REG_SLEEP & 0xff,
		1
	};
	u8 retry = 0;
	int ret = -1;

	MS100_INFO("ENter func %s.", __func__);

	gpio_direction_output(ts->data->irq, 0);
	MS100_MSLEEP(5);
	while (retry++ < 5) {
		ret = ms100_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			MS100_INFO("MS100 enter sleep!");
			return ret;
		}
		MS100_MSLEEP(10);
	}
	MS100_ERROR("MS100 send sleep cmd failed.");
	return ret;
}

static int ms100_wakeup_sleep(struct ms100_ts_data *ts)
{
	u8 retry = 0;
	int ret = -1;

	MS100_INFO("Enter func %s.", __func__);

	while (retry++ < 10) {
		gpio_direction_output(ts->data->irq, 1);
		MS100_MSLEEP(5);
		ret = ms100_i2c_test(ts->client);
		if (ret > 0) {
			MS100_INFO("MS100 wakeup sleep.");
			ms100_int_sync(ts->client, 25);
			return ret;
		}
		ms100_reset_guitar(ts->client, 20);
	}

	MS100_ERROR("MS100 wakeup sleep failed.");
	return ret;
}

static int ms100_runtime_suspend(struct device *dev)
{
	struct ms100_ts_data *ts;
	int ret = -1;

	ts = i2c_get_clientdata(i2c_connect_client);
	if (ts->use_irq)
		ms100_irq_disable(ts);
	else
		hrtimer_cancel(&ts->timer);
	MS100_INFO("Enter func %s.", __func__);

	ret = ms100_enter_sleep(ts);
	if (ret < 0) {
		MS100_ERROR("MS100 runtime suspend failed.");
		return ret;
	}
#if MS100_LOSE_POWER_SLEEP
	if (ts->data->set_power)
		ts->data->set_power(0);
#endif
	return 0;
}

static int ms100_runtime_resume(struct device *dev)
{
	struct ms100_ts_data *ts;
	int ret = -1;

	MS100_INFO("Enter func %s.", __func__);
	ts = i2c_get_clientdata(i2c_connect_client);
#if MS100_LOSE_POWER_SLEEP
	if (ts->data->set_power)
		ts->data->set_power(1);
#endif

#if MS100_LOSE_POWER_SLEEP
	gpio_direction_output(ts->data->reset, 0);
	MS100_MSLEEP(20);
	gpio_direction_output(ts->data->reset, 1);
	MS100_MSLEEP(20);
#if MS100_USE_FW_HEX
	request_firmware(&fw, "mtouch1_fw.hex", dev);
#else
	request_firmware(&fw, "mtouch1_fw.bin", dev);
#endif
	ms100_request_fw_callback(fw, ts);
	use_update = 0;

	ret = ms100_wakeup_sleep(ts);
#endif
	if (ts->use_irq)
		ms100_irq_enable(ts);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;
}

static const struct dev_pm_ops ms100_ts_pmops = {
	SET_RUNTIME_PM_OPS(ms100_runtime_suspend, ms100_runtime_resume, NULL)
};
#endif

static int ms100_ts_remove(struct i2c_client *client)
{
	struct ms100_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&client->dev);
#endif
	if (ts) {
		if (ts->use_irq) {
			gpio_direction_input(ts->data->irq);
			gpio_free(ts->data->irq);
			free_irq(client->irq, ts);
		} else
			hrtimer_cancel(&ts->timer);
	}

	MS100_INFO("MS100 driver is removing...");

	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);

	if (ts->data->set_power)
		ts->data->set_power(0);

	kfree(ts);
	return 0;
}


static const struct i2c_device_id ms100_ts_id[] = {
	{ MS100_I2C_NAME, 0 },
	{}
};

static struct i2c_driver ms100_ts_driver = {
	.probe          = ms100_ts_probe,
	.remove         = ms100_ts_remove,
	.id_table       = ms100_ts_id,
	.driver = {
		.name   = MS100_I2C_NAME,
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm     = &ms100_ts_pmops,
#endif
		.of_match_table = of_match_ptr(marvell_ts_dt_ids),
	},
};
static int __init ms100_ts_init(void)
{
	s32 ret;

	MS100_INFO("88MS100 touch driver install.");
	ms100_wq = create_singlethread_workqueue("ms100_wq");
	if (!ms100_wq) {
		MS100_ERROR("Creat workqueue failed.");
		return -ENOMEM;
	}

	ret = i2c_add_driver(&ms100_ts_driver);
	return ret;
}

static void __exit ms100_ts_exit(void)
{
	MS100_INFO("Marvell touch driver exited.");
	i2c_del_driver(&ms100_ts_driver);
	if (ms100_wq)
		destroy_workqueue(ms100_wq);
}

late_initcall(ms100_ts_init);
module_exit(ms100_ts_exit);

MODULE_AUTHOR("Yi Zeng <zengy@marvell.com>");
MODULE_DESCRIPTION("88MS100 Touch Controller Driver");
MODULE_LICENSE("GPL");
