/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/sensor/sensors_core.h>

///IIO additions...
//
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>


#define VENDOR_NAME	"CAPELLA"
#define MODEL_NAME	"CM3323"
#define MODULE_NAME	"light_sensor"

#define I2C_M_WR	0 /* for i2c Write */
#define I2c_M_RD	1 /* for i2c Read */

#define REL_RED         REL_HWHEEL
#define REL_GREEN       REL_DIAL
#define REL_BLUE        REL_WHEEL
#define REL_WHITE       REL_MISC

/* register addresses */
/* Ambient light sensor */
#define REG_CS_CONF1	0x00
#define REG_RED		0x08
#define REG_GREEN	0x09
#define REG_BLUE	0x0A
#define REG_WHITE	0x0B

#define LIGHT_LOG_TIME	15 /* 15 sec */
#define ALS_REG_NUM	2

#define CM3323_LIGHT_NAME   "cm3323_light"

enum {
	CM3323_SCAN_LIGHT_CH_RED,
	CM3323_SCAN_LIGHT_CH_GREEN,
	CM3323_SCAN_LIGHT_CH_BLUE,
	CM3323_SCAN_LIGHT_CH_CLEAR,
	CM3323_SCAN_LIGHT_TIMESTAMP,
};

#define CM3323_LIGHT_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define CM3323_LIGHT_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

#define CM3323_LIGHT_CHANNEL(color)					\
{								\
	.type = IIO_LIGHT,					\
	.modified = 1,						\
	.channel2 = IIO_MOD_LIGHT_##color,					\
	.info_mask_separate = CM3323_LIGHT_INFO_SEPARATE_MASK,	\
	.info_mask_shared_by_type = CM3323_LIGHT_INFO_SHARED_MASK,\
	.scan_index = CM3323_SCAN_LIGHT_CH_##color,			\
	.scan_type = IIO_ST('s', 32, 32, 0)			\
}

static const struct iio_chan_spec cm3323_light_channels[] = {
	CM3323_LIGHT_CHANNEL(RED),
	CM3323_LIGHT_CHANNEL(GREEN),
	CM3323_LIGHT_CHANNEL(BLUE),
	CM3323_LIGHT_CHANNEL(CLEAR),
	IIO_CHAN_SOFT_TIMESTAMP(CM3323_SCAN_LIGHT_TIMESTAMP)
};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

/* register settings */
static const u8 als_reg_setting[ALS_REG_NUM][2] = {
	{REG_CS_CONF1, 0x00},	/* enable */
	{REG_CS_CONF1, 0x01},	/* disable */
};

#define CM3323_DEFAULT_DELAY		200

/* driver data */
struct cm3323_p {
	struct i2c_client *i2c_client;
	struct input_dev *input;
	struct device *light_dev;
	struct delayed_work work;
	int delay;

	u8 power_state;
	u16 color[4];
	int irq;
	int time_count;
#ifdef CONFIG_SENSORS_ESD_DEFENCE
	int zero_cnt;
	int reset_cnt;
#endif
//	int light_delay;

	/* iio variables */
	struct iio_trigger *trig;
	int16_t sampling_frequency_light;
	atomic_t pseudo_irq_enable_light;
	struct mutex lock;
	spinlock_t spin_lock;
	struct mutex data_mutex;
	struct mutex light_mutex;
};

static int cm3323_i2c_read_word(struct cm3323_p *data,
		unsigned char reg_addr, u16 *buf)
{
	int ret;
	struct i2c_msg msg[2];
	unsigned char r_buf[2];

	msg[0].addr = data->i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = r_buf;

	ret = i2c_transfer(data->i2c_client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read error %d\n", __func__, ret);

		return ret;
	}

	*buf = (u16)r_buf[1];
	*buf = (*buf << 8) | (u16)r_buf[0];

	return 0;
}

static int cm3323_i2c_write(struct cm3323_p *data,
		unsigned char reg_addr, unsigned char buf)
{
	int ret = 0;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	/* send slave address & command */
	msg.addr = data->i2c_client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c write error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void cm3323_light_enable(struct cm3323_p *data)
{
	cm3323_i2c_write(data, REG_CS_CONF1, als_reg_setting[0][1]);
	schedule_delayed_work(&data->work,
		msecs_to_jiffies(100));
}

static void cm3323_light_disable(struct cm3323_p *data)
{
	/* disable setting */
	cm3323_i2c_write(data, REG_CS_CONF1, als_reg_setting[1][1]);
	cancel_delayed_work_sync(&data->work);
}

static inline s64 cm3323_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

static int cm3323_light_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct cm3323_p *st = iio_priv(indio_dev);
	unsigned long flags;
	spin_lock_irqsave(&st->spin_lock, flags);
	iio_trigger_poll(st->trig, cm3323_iio_get_boottime_ns());
	spin_unlock_irqrestore(&st->spin_lock, flags);
	return 0;
}

static void cm3323_work_func_light(struct work_struct *work)
{
	struct cm3323_p *data = container_of((struct delayed_work *)work,
			struct cm3323_p, work);
	struct iio_dev *indio_dev = iio_priv_to_dev(data);

	unsigned long delay = msecs_to_jiffies(data->delay);

	cm3323_i2c_read_word(data, REG_RED, &data->color[0]);
	cm3323_i2c_read_word(data, REG_GREEN, &data->color[1]);
	cm3323_i2c_read_word(data, REG_BLUE, &data->color[2]);
	cm3323_i2c_read_word(data, REG_WHITE, &data->color[3]);

/*
	input_report_rel(data->input, REL_RED, data->color[0] + 1);
	input_report_rel(data->input, REL_GREEN, data->color[1] + 1);
	input_report_rel(data->input, REL_BLUE, data->color[2] + 1);
	input_report_rel(data->input, REL_WHITE, data->color[3] + 1);
	input_sync(data->input);
*/
	cm3323_light_data_rdy_trig_poll(indio_dev);


#ifdef CONFIG_SENSORS_ESD_DEFENCE
	if ((data->color[0] == 0) && (data->color[1] == 0)
		&& (data->color[3] == 0) && (data->color[2] == 0)
		&& (data->reset_cnt < 20))
		data->zero_cnt++;
	else
		data->zero_cnt = 0;

	if (data->zero_cnt >= 25) {
		pr_info("[SENSOR]: %s - ESD Defence Reset!\n", __func__);
		cm3323_i2c_write(data, REG_CS_CONF1, als_reg_setting[1][1]);
		usleep_range(1000, 10000);
		cm3323_i2c_write(data, REG_CS_CONF1, als_reg_setting[0][1]);
		data->zero_cnt = 0;
		data->reset_cnt++;
	}
#endif

	if (((int64_t)data->delay * (int64_t)data->time_count)
		>= ((int64_t)LIGHT_LOG_TIME * MSEC_PER_SEC)) {
		pr_info("[SENSOR]: %s - r = %u g = %u b = %u w = %u\n",
			__func__, data->color[0], data->color[1],
			data->color[2], data->color[3]);
		data->time_count = 0;
	} else {
		data->time_count++;
	}

	schedule_delayed_work(&data->work, delay);
}

/* sysfs */
static ssize_t cm3323_poll_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->delay);
}

static ssize_t cm3323_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm3323_p *data = dev_get_drvdata(dev);
	int64_t new_delay;
	int ret;

	ret = kstrtoll(buf, 10, &new_delay);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	data->delay = new_delay;
	pr_info("[SENSOR]: %s - poll_delay = %lld\n", __func__, new_delay);

	return size;
}

static ssize_t light_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct cm3323_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);
	if (enable && !(data->power_state & LIGHT_ENABLED)) {
		data->power_state |= LIGHT_ENABLED;
		cm3323_light_enable(data);
#ifdef CONFIG_SENSORS_ESD_DEFENCE
		data->zero_cnt = 0;
		data->reset_cnt = 0;
#endif
	} else if (!enable && (data->power_state & LIGHT_ENABLED)) {
		cm3323_light_disable(data);
		data->power_state &= ~LIGHT_ENABLED;
	}

	return size;
}

static ssize_t light_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(data->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		cm3323_poll_delay_show, cm3323_poll_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		light_enable_show, light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL,
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

/* light sysfs */
static ssize_t cm3323_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t cm3323_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
			data->color[0], data->color[1],
			data->color[2], data->color[3]);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
			data->color[0], data->color[1],
			data->color[2], data->color[3]);
}

static DEVICE_ATTR(name, S_IRUGO, cm3323_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, cm3323_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static ssize_t cm3323_light_sampling_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3323_p *st = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", (int)st->sampling_frequency_light);
}

static ssize_t cm3323_light_sampling_frequency_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm3323_p *st = iio_priv(indio_dev);
	int ret, data;
	int delay;
	ret = kstrtoint(buf, 10, &data);
	if (ret)
		return ret;
	if (data <= 0)
		return -EINVAL;
	mutex_lock(&st->lock);
	st->sampling_frequency_light = data;
	delay = MSEC_PER_SEC / st->sampling_frequency_light;
	// has to be checked about delay
	st->delay = delay;
	mutex_unlock(&st->lock);
	return count;
}

/* iio light sysfs - sampling frequency */
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUSR|S_IWUSR,
		cm3323_light_sampling_frequency_show,
		cm3323_light_sampling_frequency_store, 0);

static struct attribute *cm3323_iio_light_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL
};

static const struct attribute_group cm3323_iio_light_attribute_group = {
	.attrs = cm3323_iio_light_attributes,
};

static int cm3323_setup_reg(struct cm3323_p *data)
{
	int ret = 0;

	/* ALS initialization */
	ret = cm3323_i2c_write(data,
			als_reg_setting[0][0],
			als_reg_setting[0][1]);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - cm3323_als_reg is failed. %d\n",
			__func__, ret);
		return ret;
	}

	/* turn off */
	cm3323_i2c_write(data, REG_CS_CONF1, 0x01);
	return ret;
}

static int cm3323_input_init(struct cm3323_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate lightsensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_RED);
	input_set_capability(dev, EV_REL, REL_GREEN);
	input_set_capability(dev, EV_REL, REL_BLUE);
	input_set_capability(dev, EV_REL, REL_WHITE);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &light_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&dev->dev.kobj, dev->name);
		input_unregister_device(dev);
		return ret;
	}

	data->input = dev;
	return 0;
}

static irqreturn_t cm3323_light_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cm3323_p *st = iio_priv(indio_dev);
	int len = 0, i, j;
	int ret = 0;
	int32_t *data;

	data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)){
		j = 0;
		for (i = 0; i < 4; i++) {
			if (test_bit(i, indio_dev->active_scan_mask)) {
				data[j] = st->color[i];
				j++;
			}
		}
		len = j * 4;
	}

	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)data + ALIGN(len, sizeof(s64))) = pf->timestamp;
	ret = iio_push_to_buffers(indio_dev, (u8 *)data);
	if (ret < 0)
		pr_err("%s, iio_push buffer failed = %d\n",
			__func__, ret);
	kfree(data);
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}


static const struct iio_buffer_setup_ops cm3323_light_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

irqreturn_t cm3323_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = cm3323_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}

static int cm3323_light_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct cm3323_p *st = iio_priv(indio_dev);
	pr_err(" %s : START\n", __func__);

	if (!atomic_cmpxchg(&st->pseudo_irq_enable_light, 0, 1)) {
		pr_err("%s, enable routine\n", __func__);
		mutex_lock(&st->lock);
		cm3323_light_enable(st);
		mutex_unlock(&st->lock);
		schedule_delayed_work(&st->work, 0);
	}
	return 0;
}

static int cm3323_light_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct cm3323_p *st = iio_priv(indio_dev);
	if (atomic_cmpxchg(&st->pseudo_irq_enable_light, 1, 0)) {
		mutex_lock(&st->lock);
		cm3323_light_disable(st);
		mutex_unlock(&st->lock);
	}
	return 0;
}


static int cm3323_light_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		cm3323_light_pseudo_irq_enable(indio_dev);
	else
		cm3323_light_pseudo_irq_disable(indio_dev);
	return 0;
}


static int cm3323_data_light_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	pr_info("%s, called state = %d\n", __func__, state);
	cm3323_light_set_pseudo_irq(indio_dev, state);
	return 0;
}


static const struct iio_trigger_ops cm3323_light_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cm3323_data_light_rdy_trigger_set_state,
};

static int cm3323_read_raw_light(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val,
		int *val2,
		long mask) {
	struct cm3323_p  *st = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	mutex_lock(&st->lock);

	switch (mask) {
	case 0:
		*val = st->color[chan->channel2 - IIO_MOD_LIGHT_RED] ;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		/* Gain : counts / uT = 1000 [nT] */
		/* Scaling factor : 1000000 / Gain = 1000 */
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}

	mutex_unlock(&st->lock);

	return ret;
}


static int cm3323_light_probe_buffer(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &cm3323_light_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM3323_SCAN_LIGHT_CH_RED);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM3323_SCAN_LIGHT_CH_GREEN);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM3323_SCAN_LIGHT_CH_BLUE);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM3323_SCAN_LIGHT_CH_CLEAR);
	pr_err("%s, successed \n", __func__);
	return 0;

error_free_buf:
	pr_err("%s, failed \n", __func__);
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int cm3323_light_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct cm3323_p *st = iio_priv(indio_dev);
	indio_dev->pollfunc = iio_alloc_pollfunc(&cm3323_iio_pollfunc_store_boottime,
			&cm3323_light_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);

	pr_err("%s is called\n", __func__);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	st->trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!st->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	st->trig->dev.parent = &st->i2c_client->dev;
	st->trig->ops = &cm3323_light_trigger_ops;
	iio_trigger_set_drvdata(st->trig, indio_dev);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error_free_trig;
	pr_err("%s is success\n", __func__);
	return 0;

error_free_trig:
	iio_trigger_free(st->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void cm3323_light_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static void cm3323_light_remove_trigger(struct iio_dev *indio_dev)
{
	struct cm3323_p *st = iio_priv(indio_dev);
	iio_trigger_unregister(st->trig);
	iio_trigger_free(st->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_info cm3323_light_info= {
	.read_raw = &cm3323_read_raw_light,
	.attrs = &cm3323_iio_light_attribute_group,
	.driver_module = THIS_MODULE,
};

static int cm3323_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct cm3323_p *data = NULL;
	struct cm3323_p *data_iio = NULL;
	struct iio_dev *indio_dev;

	pr_info("[SENSOR]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	data = kzalloc(sizeof(struct cm3323_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SENSOR]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	/* iio device register - light*/
	indio_dev = iio_device_alloc(sizeof(*data_iio));
	if (!indio_dev) {
		pr_err("%s, iio_dev_light alloc failed\n", __func__);
		goto exit_of_node;
	}
	i2c_set_clientdata(client, indio_dev);

	indio_dev->name = CM3323_LIGHT_NAME;
	indio_dev->dev.parent = &client->dev;
	indio_dev->info = &cm3323_light_info;
	indio_dev->channels = cm3323_light_channels;
	indio_dev->num_channels = ARRAY_SIZE(cm3323_light_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;

	data_iio = iio_priv(indio_dev);
	data_iio->i2c_client = client;
	data_iio->sampling_frequency_light = 5;
	data_iio->delay = MSEC_PER_SEC / data_iio->sampling_frequency_light;

	spin_lock_init(&data_iio->spin_lock);
	mutex_init(&data_iio->lock);
	mutex_init(&data_iio->data_mutex);
	mutex_init(&data_iio->light_mutex);

	pr_err("%s, before light probe buffer\n", __func__);
	ret = cm3323_light_probe_buffer(indio_dev);
	if (ret) {
		pr_err("%s, light probe buffer failed\n", __func__);
		goto error_free_light_dev;
	}

	ret = cm3323_light_probe_trigger(indio_dev);
	if (ret) {
		pr_err("%s, light probe trigger failed\n", __func__);
		goto error_remove_light_buffer;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		pr_err("%s, light iio register failed\n", __func__);
		goto error_remove_light_trigger;
	}

	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	INIT_DELAYED_WORK(&data_iio->work, cm3323_work_func_light);

	/* Check if the device is there or not. */
	ret = cm3323_setup_reg(data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - could not setup regs\n", __func__);
		goto exit_setup_reg;
	}

	/* input device init */
	ret = cm3323_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	data->delay = CM3323_DEFAULT_DELAY;
	data->time_count = 0;

	/* set sysfs for light sensor */
	sensors_register(data->light_dev, data, sensor_attrs, MODULE_NAME);
	pr_info("[SENSOR]: %s - Probe done!\n", __func__);

	return 0;

exit_input_init:
exit_setup_reg:
	iio_device_unregister(indio_dev);
error_remove_light_trigger:
	cm3323_light_remove_trigger(indio_dev);
error_remove_light_buffer:
	cm3323_light_remove_buffer(indio_dev);
error_free_light_dev:
	iio_device_free(indio_dev);
exit_of_node:
exit_kzalloc:
	kfree(data);
exit:
	pr_err("[SENSOR]: %s - Probe fail!\n", __func__);
	return ret;
}

static void cm3323_shutdown(struct i2c_client *client)
{
	struct cm3323_p *data = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (data->power_state & LIGHT_ENABLED)
		cm3323_light_disable(data);
}

static int __devexit cm3323_remove(struct i2c_client *client)
{
	struct cm3323_p *data = i2c_get_clientdata(client);

	/* device off */
	if (data->power_state & LIGHT_ENABLED)
		cm3323_light_disable(data);

	/* sysfs destroy */
	sensors_unregister(data->light_dev, sensor_attrs);
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);

	/* input device destroy */
	sysfs_remove_group(&data->input->dev.kobj, &light_attribute_group);
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}

static int cm3323_suspend(struct device *dev)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		cm3323_light_disable(data);
	}

	return 0;
}

static int cm3323_resume(struct device *dev)
{
	struct cm3323_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		cm3323_light_enable(data);
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cm3323_match_table[] = {
	{ .compatible = "light_sensor,cm3323",},
	{ },
};
#else
#define cm3323_match_table	NULL
#endif

static const struct i2c_device_id cm3323_device_id[] = {
	{ "cm3323_match_table", 0 },
	{ }
};

static const struct dev_pm_ops cm3323_pm_ops = {
	.suspend = cm3323_suspend,
	.resume = cm3323_resume
};

static struct i2c_driver cm3323_i2c_driver = {
	.driver = {
		.name = MODEL_NAME,
		.owner = THIS_MODULE,
		.pm = &cm3323_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cm3323_match_table),
#endif
	},
	.probe = cm3323_probe,
	.shutdown = cm3323_shutdown,
	.remove = __devexit_p(cm3323_remove),
	.id_table = cm3323_device_id,
};

static int __init cm3323_init(void)
{
	return i2c_add_driver(&cm3323_i2c_driver);
}

static void __exit cm3323_exit(void)
{
	i2c_del_driver(&cm3323_i2c_driver);
}

module_init(cm3323_init);
module_exit(cm3323_exit);

MODULE_DESCRIPTION("RGB Sensor device driver for cm3323");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
