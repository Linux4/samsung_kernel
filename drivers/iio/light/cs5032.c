/*
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/sensor/sensors_core.h>

#define I2C_M_WR                          0 /* for i2c Write */
#define LIGHT_LOG_TIME                    15

#define VENDOR_NAME                       "LITEON"
#define MODEL_NAME                        "CS5032"
#define MODULE_NAME                       "light_sensor"
#define CS5032_LIGHT_NAME                 "cs5032_light"
#define CS5032_DEFAULT_DELAY              200

#define CS5032_DEFAULT_ALTIME             0x30

#define CS5032_ALS_GAIN_1                 0x0
#define CS5032_ALS_GAIN_4                 0x1
#define CS5032_ALS_GAIN_16                0x2
#define CS5032_ALS_GAIN_64                0x3

#define CS5032_SYS_MGS_ENABLE             0x8
#define CS5032_SYS_RST_ENABLE             0x4
#define CS5032_SYS_PS_ENABLE              0x2
#define CS5032_SYS_ALS_ENABLE             0x1
#define CS5032_SYS_DEV_DOWN               0x0

#define CS5032_REG_SYS_CON                0x00
#define CS5032_REG_SYS_INTSTATUS          0x01
#define CS5032_REG_ALS_CON                0x07
#define CS5032_REG_ALS_ALGAIN_CON_MASK    0x03

#define CS5032_REG_ALS_TIME               0x0A
#define CS5032_REG_RED_DATA_LOW           0x28
#define CS5032_REG_RED_DATA_HIGH          0x29
#define CS5032_REG_GREEN_DATA_LOW         0x2A
#define CS5032_REG_GREEN_DATA_HIGH        0x2B
#define CS5032_REG_BLUE_DATA_LOW          0x2C
#define CS5032_REG_BLUE_DATA_HIGH         0x2D
#define CS5032_REG_COMP_DATA_LOW          0x2E
#define CS5032_REG_COMP_DATA_HIGH         0x2F

#define CS5032_REG_AL_ALTHL_LOW           0x32
#define CS5032_REG_AL_ALTHL_HIGH          0x33
#define CS5032_REG_AL_ALTHH_LOW           0x34
#define CS5032_REG_AL_ALTHH_HIGH          0x35
#define CS5032_REG_LUM_COEFR              0x54
#define CS5032_REG_LUM_COEFG              0x55
#define CS5032_REG_LUM_COEFB              0x56

enum {
	SENSOR_DISABLE = 0,
	SENSOR_ENABLE,
};

enum {
	CS5032_SCAN_LIGHT_CH_RED,
	CS5032_SCAN_LIGHT_CH_GREEN,
	CS5032_SCAN_LIGHT_CH_BLUE,
	CS5032_SCAN_LIGHT_CH_CLEAR,
	CS5032_SCAN_LIGHT_CH_LUX,
	CS5032_SCAN_LIGHT_TIMESTAMP,
};

#define CS5032_LIGHT_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define CS5032_LIGHT_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

#define CS5032_LIGHT_CHANNEL(color)					\
{									\
	.type = IIO_LIGHT,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_LIGHT_##color,				\
	.info_mask_separate = CS5032_LIGHT_INFO_SEPARATE_MASK,		\
	.info_mask_shared_by_type = CS5032_LIGHT_INFO_SHARED_MASK,	\
	.scan_index = CS5032_SCAN_LIGHT_CH_##color,			\
	.scan_type = IIO_ST('s', 32, 32, 0)				\
}

static const struct iio_chan_spec cs5032_light_channels[] = {
	CS5032_LIGHT_CHANNEL(RED),
	CS5032_LIGHT_CHANNEL(GREEN),
	CS5032_LIGHT_CHANNEL(BLUE),
	CS5032_LIGHT_CHANNEL(CLEAR),
	CS5032_LIGHT_CHANNEL(LUX),
	IIO_CHAN_SOFT_TIMESTAMP(CS5032_SCAN_LIGHT_TIMESTAMP)
};

struct cs5032_data {
	struct i2c_client *client;
	struct device *light_dev;
	struct delayed_work work;
	struct iio_trigger *trig;
	struct iio_dev *indio_dev;
	struct mutex lock;
	struct mutex data_lock;
	spinlock_t spin_lock;

	atomic_t pseudo_irq_enable_light;
	int delay;
	int count_log_time;
	int auto_gain;
	u16 color[4];
	int32_t lux;
	int16_t sampling_frequency_light;
	u8 enabled;
};

struct cs5032_sensor_data {
	struct cs5032_data *cdata;
};

static inline s64 cs5032_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

static int cs5032_light_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;
	unsigned long flags;

	spin_lock_irqsave(&cs5032_iio->spin_lock, flags);
	iio_trigger_poll(cs5032_iio->trig, cs5032_iio_get_boottime_ns());
	spin_unlock_irqrestore(&cs5032_iio->spin_lock, flags);

	return 0;
}

static int cs5032_i2c_read(struct cs5032_data *cs5032_iio,
		unsigned char reg_addr, unsigned char *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = cs5032_iio->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = cs5032_iio->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(cs5032_iio->client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read(0x%x) error %d\n",
			__func__, reg_addr, ret);
		return ret;
	}

	return 0;
}


static int cs5032_i2c_read_block(struct cs5032_data *cs5032_iio,
		unsigned char reg_addr, unsigned char *buf, unsigned char len)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = cs5032_iio->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = cs5032_iio->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	ret = i2c_transfer(cs5032_iio->client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read(0x%x) error %d\n",
			__func__, reg_addr, ret);
		return ret;
	}

	return 0;
}

static int cs5032_i2c_write(struct cs5032_data *cs5032_iio,
		unsigned char reg_addr, unsigned char buf)
{
	int ret;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = cs5032_iio->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(cs5032_iio->client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c write(0x%x) error %d\n",
			__func__, reg_addr, ret);
		return ret;
	}

	return 0;
}

static int cs5032_set_algain(struct cs5032_data *cs5032_iio,
		unsigned char algain)
{
	int ret = 0;
	unsigned char buf, mask = CS5032_REG_ALS_ALGAIN_CON_MASK;

	ret = cs5032_i2c_read(cs5032_iio, CS5032_REG_ALS_CON, &buf);
	if (ret < 0)
		return ret;

	buf &= ~mask;
	buf |= algain;

	ret = cs5032_i2c_write(cs5032_iio, CS5032_REG_ALS_CON, buf);

	return ret;
}

static int cs5032_get_algain(struct cs5032_data *cs5032_iio)
{
	int ret = 0;
	unsigned char buf;

	ret = cs5032_i2c_read(cs5032_iio, CS5032_REG_ALS_CON, &buf);
	if (ret < 0)
		return ret;

	return (int)(buf & 0x03);
}

static int cs5032_set_altime(struct cs5032_data *cs5032_iio,
		unsigned char altime)
{
	int ret = 0;

	ret = cs5032_i2c_write(cs5032_iio, CS5032_REG_ALS_TIME, altime);

	return ret;
}
static int cs5032_set_mode(struct cs5032_data *cs5032_iio, unsigned char mode)
{
	int ret;

	pr_info("[SENSOR]: %s - mode = %x\n", __func__, mode);

	if (mode == CS5032_SYS_ALS_ENABLE) {
		cs5032_set_altime(cs5032_iio, CS5032_DEFAULT_ALTIME);

		/* Remove the initial delay (>200ms) by ALS GAIN setting
		 * cs5032_set_algain(cs5032_iio, CS5032_ALS_GAIN_1);
		 */
		cs5032_i2c_write(cs5032_iio, CS5032_REG_LUM_COEFR, 0);
		cs5032_i2c_write(cs5032_iio, CS5032_REG_LUM_COEFG, 0);
		cs5032_i2c_write(cs5032_iio, CS5032_REG_LUM_COEFB, 0);

		/*suspend*/
		cs5032_i2c_write(cs5032_iio, 0x02, 0xcc);
		cs5032_i2c_write(cs5032_iio, 0x08, 0x00);
		cs5032_i2c_write(cs5032_iio, 0x01, 0x00);
	}

	ret = cs5032_i2c_write(cs5032_iio, CS5032_REG_SYS_CON, mode);
	if (ret < 0)
		return ret;

	return ret;
}

#define FIXED_POINT 1024
static const int64_t MIN_THRES = 65535 >> 3;
static const int64_t MAX_THRES = (65535 * 3) >> 2;

static const int64_t Alpha_Y_Val = 0.405494566177106 * FIXED_POINT;
static const int64_t Beta_Y_Val =  4.61732789097300 * FIXED_POINT;
static const int64_t Gamma_Y_Val = 0.0777847281810487 * FIXED_POINT;
static const int64_t Delta_Y_Val = -7.98189792275903 * FIXED_POINT;

static int cs5032_get_als_value(struct cs5032_data *cs5032_iio)
{
	u8 buf[8];
	unsigned char error_flag;
	u16 r_val, g_val, b_val, c_val;
	int64_t max_val;
	int64_t luxValue;

	cs5032_i2c_read_block(cs5032_iio, CS5032_REG_RED_DATA_LOW, buf, 8);
	r_val = (u16)(buf[1] << 8 | buf[0]);
	g_val = (u16)(buf[3] << 8 | buf[2]);
	b_val = (u16)(buf[5] << 8 | buf[4]);
	c_val = (u16)(buf[7] << 8 | buf[6]);

	max_val = r_val;
	if (max_val < g_val) {
		max_val = g_val;
	}
	if (max_val < b_val) {
		max_val = b_val;
	}

	cs5032_iio->auto_gain = cs5032_get_algain(cs5032_iio);
	cs5032_i2c_read(cs5032_iio, CS5032_REG_SYS_INTSTATUS, &error_flag);
	error_flag = error_flag & 0x20;

	if((max_val <= MIN_THRES) && (error_flag != 0x20)) {
		if(cs5032_iio->auto_gain != CS5032_ALS_GAIN_64) {
			cs5032_set_algain(cs5032_iio, cs5032_iio->auto_gain + 1);
		}
	} else if((max_val >= MAX_THRES) || (error_flag == 0x20)) {
		if(cs5032_iio->auto_gain != CS5032_ALS_GAIN_1) {
			cs5032_set_algain(cs5032_iio, cs5032_iio->auto_gain - 1);
		}
	}

	cs5032_iio->color[0] = r_val;
	cs5032_iio->color[1] = g_val;
	cs5032_iio->color[2] = b_val;
	cs5032_iio->color[3] = c_val;

	luxValue = ((r_val * Alpha_Y_Val) + (g_val * Beta_Y_Val) \
		+ (b_val * Gamma_Y_Val) + (c_val * Delta_Y_Val))/FIXED_POINT;
	cs5032_iio->lux = luxValue >> (cs5032_iio->auto_gain * 2);

	if (cs5032_iio->lux < 0)
		cs5032_iio->lux = 0;

	/*suspend*/
	cs5032_i2c_write(cs5032_iio, 0x01, 0x00);

	return 0;
}

static int cs5032_lsensor_enable(struct cs5032_data *cs5032_iio)
{
	int ret = 0;

	pr_info("[SENSOR]: %s\n", __func__);
	if (cs5032_iio->enabled == SENSOR_DISABLE) {
		cs5032_iio->count_log_time = 1000;
		ret = cs5032_set_mode(cs5032_iio , CS5032_SYS_ALS_ENABLE);
		if (ret < 0) {
			pr_err("[SENSOR]: %s - can't enable light sensor\n",
				__func__);
			return ret;
		}
		schedule_delayed_work(&cs5032_iio->work, msecs_to_jiffies(200));
		cs5032_iio->enabled = SENSOR_ENABLE;
	}

	return 0;
}

static int cs5032_lsensor_disable(struct cs5032_data *cs5032_iio)
{
	int ret = 0;

	pr_info("[SENSOR]: %s\n", __func__);
	if (cs5032_iio->enabled == SENSOR_ENABLE) {
		ret = cs5032_set_mode(cs5032_iio, CS5032_SYS_DEV_DOWN);
		cancel_delayed_work_sync(&cs5032_iio->work);
		cs5032_iio->enabled = SENSOR_DISABLE;
	}

	return ret;
}

static ssize_t cs5032_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t cs5032_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t cs5032_rawdata_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cs5032_data *cs5032_iio = dev_get_drvdata(dev);

	if (cs5032_iio->enabled == SENSOR_DISABLE) {
	mutex_lock(&cs5032_iio->data_lock);
	cs5032_get_als_value(cs5032_iio);
	mutex_unlock(&cs5032_iio->data_lock);
	}
	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
			cs5032_iio->color[0], cs5032_iio->color[1],
			cs5032_iio->color[2], cs5032_iio->color[3]);
}

static DEVICE_ATTR(name, S_IRUGO, cs5032_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, cs5032_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, cs5032_rawdata_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, cs5032_rawdata_show, NULL);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static ssize_t cs5032_light_sampling_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	return sprintf(buf, "%d\n", (int)cs5032_iio->sampling_frequency_light);
}

static ssize_t cs5032_light_sampling_frequency_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;
	int ret, data;
	int delay;

	ret = kstrtoint(buf, 10, &data);
	if (ret)
		return count;

	if (data <= 0)
		return count;

	mutex_lock(&cs5032_iio->lock);
	cs5032_iio->sampling_frequency_light = data;
	delay = MSEC_PER_SEC / cs5032_iio->sampling_frequency_light;

	if(delay < CS5032_DEFAULT_DELAY)
		delay = CS5032_DEFAULT_DELAY;

	cs5032_iio->delay = delay;
	pr_info("[SENSOR]: %s - %dms\n", __func__, cs5032_iio->delay);
	mutex_unlock(&cs5032_iio->lock);

	return count;
}

/* iio light sysfs - sampling frequency */
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUSR | S_IWUSR,
		cs5032_light_sampling_frequency_show,
		cs5032_light_sampling_frequency_store, 0);

static struct attribute *cs5032_iio_light_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL
};

static const struct attribute_group cs5032_iio_light_attribute_group = {
	.attrs = cs5032_iio_light_attributes,
};

static void cs5032_work_func_light(struct work_struct *work)
{
	struct cs5032_data *cs5032_iio =
			container_of((struct delayed_work *)work,
			struct cs5032_data, work);

	unsigned long delay = msecs_to_jiffies(cs5032_iio->delay);
	int ret;

	mutex_lock(&cs5032_iio->data_lock);
	ret = cs5032_get_als_value(cs5032_iio);
	if (ret < 0)
		pr_err("[SENSOR]: %s - could not get als data = %d\n",
			__func__, ret);
	mutex_unlock(&cs5032_iio->data_lock);

	if ((cs5032_iio->delay * cs5032_iio->count_log_time)
		>= LIGHT_LOG_TIME * MSEC_PER_SEC) {
		pr_info("[SENSOR]: %s - %u,%u,%u,%u : gain: %d, lux: %d\n",
			__func__, cs5032_iio->color[0], cs5032_iio->color[1],
			cs5032_iio->color[2], cs5032_iio->color[3],
			cs5032_iio->auto_gain, cs5032_iio->lux);
		cs5032_iio->count_log_time = 0;
	} else
		cs5032_iio->count_log_time++;

	cs5032_light_data_rdy_trig_poll(cs5032_iio->indio_dev);
	schedule_delayed_work(&cs5032_iio->work, delay);
}

static irqreturn_t cs5032_light_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	int len = 0, i, j;
	int ret = 0;
	int32_t *data;

	data = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		j = 0;
		for (i = 0; i < 4; i++) {
			if (test_bit(i, indio_dev->active_scan_mask)) {
				data[j] = cs5032_iio->color[i];
				j++;
			}
		}
		if (test_bit(4, indio_dev->active_scan_mask)) {
			data[4] = cs5032_iio->lux;
			j++;
		}

		len = j * 4;
	}

	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)data + ALIGN(len, sizeof(s64))) = pf->timestamp;
	ret = iio_push_to_buffers(indio_dev, (u8 *)data);

	if (ret < 0)
		pr_err("[SENSOR]: %s - iio_push buffer failed = %d\n",
			__func__, ret);
	kfree(data);
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops cs5032_light_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

irqreturn_t cs5032_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = cs5032_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}

static int cs5032_light_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	pr_info("[SENSOR]: %s\n", __func__);

	if (!atomic_cmpxchg(&cs5032_iio->pseudo_irq_enable_light, 0, 1)) {
		mutex_lock(&cs5032_iio->lock);
		cs5032_lsensor_enable(cs5032_iio);
		mutex_unlock(&cs5032_iio->lock);
	}

	return 0;
}

static int cs5032_light_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	pr_info("[SENSOR]: %s\n", __func__);

	if (atomic_cmpxchg(&cs5032_iio->pseudo_irq_enable_light, 1, 0)) {
		mutex_lock(&cs5032_iio->lock);
		cs5032_lsensor_disable(cs5032_iio);
		mutex_unlock(&cs5032_iio->lock);
	}
	return 0;
}

static int cs5032_data_light_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);

	if (state)
		cs5032_light_pseudo_irq_enable(indio_dev);
	else
		cs5032_light_pseudo_irq_disable(indio_dev);

	return 0;
}

static const struct iio_trigger_ops cs5032_light_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cs5032_data_light_rdy_trigger_set_state,
};

static int cs5032_read_raw_light(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val, int *val2, long mask)
{
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;
	int ret = -EINVAL;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	mutex_lock(&cs5032_iio->lock);

	switch (mask) {
	case 0:
		*val = cs5032_iio->color[chan->channel2 - IIO_MOD_LIGHT_RED];
		ret = IIO_VAL_INT;
		pr_info("[SENSOR]: %s : value = %d\n", __func__, *val);
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}

	mutex_unlock(&cs5032_iio->lock);

	return ret;
}

static int cs5032_light_probe_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	buffer->scan_timestamp = true;
	indio_dev->buffer = buffer;
	indio_dev->setup_ops = &cs5032_light_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
		indio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(indio_dev, indio_dev->buffer,
		CS5032_SCAN_LIGHT_CH_RED);
	iio_scan_mask_set(indio_dev, indio_dev->buffer,
		CS5032_SCAN_LIGHT_CH_GREEN);
	iio_scan_mask_set(indio_dev, indio_dev->buffer,
		CS5032_SCAN_LIGHT_CH_BLUE);
	iio_scan_mask_set(indio_dev, indio_dev->buffer,
		CS5032_SCAN_LIGHT_CH_CLEAR);
	iio_scan_mask_set(indio_dev, indio_dev->buffer,
		CS5032_SCAN_LIGHT_CH_LUX);

	return 0;

error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	pr_err("[SENSOR]: %s failed!!\n", __func__);
	return ret;
}

static int cs5032_light_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	indio_dev->pollfunc =
			iio_alloc_pollfunc(&cs5032_iio_pollfunc_store_boottime,
			&cs5032_light_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	cs5032_iio->trig = iio_trigger_alloc("%s-dev%d",
				indio_dev->name, indio_dev->id);
	if (!cs5032_iio->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	cs5032_iio->trig->dev.parent = &cs5032_iio->client->dev;
	cs5032_iio->trig->ops = &cs5032_light_trigger_ops;
	iio_trigger_set_drvdata(cs5032_iio->trig, indio_dev);
	ret = iio_trigger_register(cs5032_iio->trig);

	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(cs5032_iio->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	pr_err("[SENSOR]: %s failed!!\n", __func__);
	return ret;
}

static void cs5032_light_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static void cs5032_light_remove_trigger(struct iio_dev *indio_dev)
{
	struct cs5032_sensor_data *sdata = iio_priv(indio_dev);
	struct cs5032_data *cs5032_iio = sdata->cdata;

	iio_trigger_unregister(cs5032_iio->trig);
	iio_trigger_free(cs5032_iio->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_info cs5032_light_info = {
	.read_raw = &cs5032_read_raw_light,
	.attrs = &cs5032_iio_light_attribute_group,
	.driver_module = THIS_MODULE,
};

static int cs5032_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct cs5032_data *cs5032_iio = NULL;
	struct cs5032_sensor_data *cm5032_p;
	int ret = 0;

	pr_info("[SENSOR]: %s - start!!\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -EIO;
		goto exit_free_gpio;
	}

	cs5032_iio = kzalloc(sizeof(struct cs5032_data), GFP_KERNEL);
	if (!cs5032_iio) {
		ret = -ENOMEM;
		goto exit_free_gpio;
	}

	/* iio device register - light*/
	cs5032_iio->indio_dev = iio_device_alloc(sizeof(*cm5032_p));
	if (!cs5032_iio->indio_dev) {
		pr_err("%s, iio_dev_light alloc failed\n", __func__);
		goto error_free_data;
	}

	cm5032_p = iio_priv(cs5032_iio->indio_dev);
	cm5032_p->cdata = cs5032_iio;

	cs5032_iio->indio_dev->name = CS5032_LIGHT_NAME;
	cs5032_iio->indio_dev->dev.parent = &client->dev;
	cs5032_iio->indio_dev->info = &cs5032_light_info;
	cs5032_iio->indio_dev->channels = cs5032_light_channels;
	cs5032_iio->indio_dev->num_channels = ARRAY_SIZE(cs5032_light_channels);
	cs5032_iio->indio_dev->modes = INDIO_DIRECT_MODE;

	cs5032_iio->sampling_frequency_light = 5;
	cs5032_iio->delay = CS5032_DEFAULT_DELAY;
	cs5032_iio->enabled = SENSOR_DISABLE;
	cs5032_iio->auto_gain = CS5032_ALS_GAIN_1;

	i2c_set_clientdata(client, cs5032_iio);
	cs5032_iio->client = client;

	ret = cs5032_light_probe_buffer(cs5032_iio->indio_dev);
	if (ret) {
		pr_err("%s, light probe buffer failed\n", __func__);
		goto error_free_light_dev;
	}

	ret = cs5032_light_probe_trigger(cs5032_iio->indio_dev);
	if (ret) {
		pr_err("%s, light probe trigger failed\n", __func__);
		goto error_remove_light_buffer;
	}

	ret = iio_device_register(cs5032_iio->indio_dev);
	if (ret) {
		pr_err("%s, light iio register failed\n", __func__);
		goto error_remove_light_trigger;
	}

	/* initialize the cs5032 chip */
	ret = cs5032_set_mode(cs5032_iio, CS5032_SYS_RST_ENABLE);
	if (ret)
		goto error_init_chip;

	INIT_DELAYED_WORK(&cs5032_iio->work, cs5032_work_func_light);
	spin_lock_init(&cs5032_iio->spin_lock);
	mutex_init(&cs5032_iio->lock);
	mutex_init(&cs5032_iio->data_lock);

	/* set sysfs for light sensor */
	sensors_register(cs5032_iio->light_dev, cs5032_iio,
		sensor_attrs, MODULE_NAME);
	sensors_create_symlink(&cs5032_iio->indio_dev->dev.kobj,
			"light_sensor");
	pr_info("[SENSOR]: %s - success!!\n", __func__);

	return 0;

error_init_chip:
	iio_device_unregister(cs5032_iio->indio_dev);
error_remove_light_trigger:
	cs5032_light_remove_trigger(cs5032_iio->indio_dev);
error_remove_light_buffer:
	cs5032_light_remove_buffer(cs5032_iio->indio_dev);
error_free_light_dev:
	iio_device_free(cs5032_iio->indio_dev);
error_free_data:
	kfree(cs5032_iio);
exit_free_gpio:
	pr_err("[SENSOR]: %s failed!!\n", __func__);
	return ret;
}

static int cs5032_remove(struct i2c_client *client)
{
	struct cs5032_data *cs5032_iio = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (cs5032_iio->enabled) {
		cancel_delayed_work_sync(&cs5032_iio->work);
		cs5032_set_mode(cs5032_iio, CS5032_SYS_DEV_DOWN);
	}
	mutex_destroy(&cs5032_iio->lock);
	mutex_destroy(&cs5032_iio->data_lock);
	sensors_unregister(cs5032_iio->light_dev, sensor_attrs);
	iio_device_unregister(cs5032_iio->indio_dev);
	cs5032_light_remove_trigger(cs5032_iio->indio_dev);
	cs5032_light_remove_buffer(cs5032_iio->indio_dev);
	iio_device_free(cs5032_iio->indio_dev);
	kfree(cs5032_iio);

	return 0;
}

static void cs5032_shutdown(struct i2c_client *client)
{
	struct cs5032_data *cs5032_iio = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (cs5032_iio->enabled) {
		cancel_delayed_work_sync(&cs5032_iio->work);
		cs5032_set_mode(cs5032_iio, CS5032_SYS_DEV_DOWN);
	}
}

static int cs5032_resume(struct device *dev)
{
	struct cs5032_data *cs5032_iio = dev_get_drvdata(dev);

	if (cs5032_iio->enabled)
		schedule_delayed_work(&cs5032_iio->work,
			msecs_to_jiffies(cs5032_iio->delay));

	return 0;
}

static int cs5032_suspend(struct device *dev)
{
	struct cs5032_data *cs5032_iio = dev_get_drvdata(dev);

	if (cs5032_iio->enabled)
		cancel_delayed_work_sync(&cs5032_iio->work);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cs5032_match_table[] = {
	{ .compatible = "cs5032",},
	{},
};
#else
#define cs5032_match_table NULL
#endif

static const struct i2c_device_id cs5032_id[] = {
	{ "cs5032_match_table", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, cs5032_id);

static const struct dev_pm_ops cs5032_pm_ops = {
	.suspend = cs5032_suspend,
	.resume = cs5032_resume
};

static struct i2c_driver cs5032_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.pm = &cs5032_pm_ops,
		.of_match_table = cs5032_match_table,
	},
	.probe	= cs5032_probe,
	.remove	= cs5032_remove,
	.shutdown = cs5032_shutdown,
	.id_table = cs5032_id,
};

static int __init cs5032_init(void)
{
	int ret;

	ret = i2c_add_driver(&cs5032_driver);
	return ret;

}

static void __exit cs5032_exit(void)
{
	i2c_del_driver(&cs5032_driver);
}

module_init(cs5032_init);
module_exit(cs5032_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("RGB Sensor device driver for cs5302");
MODULE_LICENSE("GPL");
