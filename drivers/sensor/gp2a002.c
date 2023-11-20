/*
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#include <linux/sensor/sensors_core.h>
#include <linux/sensor/gp2a002.h>

#define REGS_PROX		0x0 /* Read  Only */
#define REGS_GAIN		0x1 /* Write Only */
#define REGS_HYS		0x2 /* Write Only */
#define REGS_CYCLE		0x3 /* Write Only */
#define REGS_OPMOD		0x4 /* Write Only */
#define REGS_CON		0x6 /* Write Only */

#if defined(CONFIG_MACH_COREPRIMEVE3G) || defined(CONFIG_MACH_GRANDPRIMEVE3G)
#define PROX_NONDETECT		0x2F
#define PROX_DETECT		0x0D
#else
#define PROX_NONDETECT		0x2F
#define PROX_DETECT		0x0F
#endif
#define PROX_NONDETECT_MODE1	0x43
#define PROX_DETECT_MODE1	0x28
#define PROX_NONDETECT_MODE2	0x48
#define PROX_DETECT_MODE2	0x42
#define OFFSET_FILE_PATH	"/efs/FactoryApp/prox_cal"

#define CHIP_DEV_NAME		"GP2AP002"
#define CHIP_DEV_VENDOR		"SHARP"

struct gp2a_data {
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev;
	struct iio_trigger *trig;
	spinlock_t spin_lock;
#else
	struct input_dev *input;
#endif
	struct device *dev;
	struct gp2a_platform_data *pdata;
	struct i2c_client *i2c_client;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	struct work_struct work_prox;

	int irq;
	int power_state;
	char val_state;
	char cal_mode;

	u8 detect;
	u8 nondetect;
#if defined(CONFIG_SENSORS_IIO)
	u64 timestamp;
#endif
};

enum {
	OFF = 0,
	ON,
};

#if defined(CONFIG_SENSORS_IIO)
#define IIO_BUFFER_1_BYTES	9 /* data 1 bytes + timestamp 8 bytes */

static const struct iio_chan_spec gp2a_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.channel = -1,
		.scan_index = 0,
		.scan_type = IIO_ST('s', IIO_BUFFER_1_BYTES * 8,
			IIO_BUFFER_1_BYTES * 8, 0)
	}
};

static inline s64 gp2a_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

irqreturn_t gp2a_iio_pollfunc_store_time(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = gp2a_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}
#endif

int gp2a_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int err = 0;
	unsigned char data[2] = {reg, 0};
	int retry = 10;
	struct i2c_msg msg[2] = {};

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = data;

	msg[1].addr = client->addr;
	msg[1].flags = 1;
	msg[1].len = 2;
	msg[1].buf = data;

	while (retry--) {
		data[0] = reg;

		err = i2c_transfer(client->adapter, msg, 2);

		if (err >= 0) {
			*val = data[1];
			return 0;
		}
	}

	pr_err("[SENSOR] %s, i2c transfer error ret = %d\n", __func__, err);

	return err;
}

int gp2a_i2c_write(struct i2c_client *client, u8 reg, u8 val)
{
	int err = -1;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 10;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		data[0] = reg;
		data[1] = val;

		msg->addr = client->addr;
		msg->flags = 0;
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(client->adapter, msg, 1);

		if (err >= 0)
			return 0;
	}

	pr_err("[SENSOR] %s, i2c transfer error ret= %d\n", __func__, err);

	return err;
}

static int gp2a_leda_onoff(struct gp2a_platform_data *pdata,
	struct device *dev, int power)
{
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	pr_info("[SENSOR] %s %s\n", __func__, (power) ? "on" : "off");

	gpio_set_value(pdata->power_en, power);
#else
	int ret;
	struct regulator *gp2a_leda;

	pr_info("[SENSOR] %s %s\n", __func__, (power) ? "on" : "off");

	gp2a_leda = devm_regulator_get(dev, "gp2a-leda");
	if (IS_ERR(gp2a_leda)) {
		pr_err("[SENSOR]: %s - cannot get gp2a_leda\n", __func__);
		return -ENOMEM;
	}

	if (power) {
		ret = regulator_enable(gp2a_leda);
		if (ret) {
			pr_err("[SENSOR] %s: enable gp2a_leda failed (%d)\n",
				__func__, ret);
			return ret;
		}
	} else {
		ret = regulator_disable(gp2a_leda);
		if (ret) {
			pr_err("[SENSOR] %s: disable gp2a_leda failed (%d)\n",
				__func__, ret);
			return ret;
		}
	}

	devm_regulator_put(gp2a_leda);
#endif

	msleep(20);

	return 0;
}
static int gp2a_power_onoff(struct gp2a_data *gp2a, int power)
{
	u8 value;
	pr_info("[SENSOR] %s, status(%d)\n", __func__, power);

	if (power) {
		gp2a_leda_onoff(gp2a->pdata, &gp2a->i2c_client->dev, power);
		value = 0x18;
		gp2a_i2c_write(gp2a->i2c_client, REGS_CON, value);
		value = 0x08;
		gp2a_i2c_write(gp2a->i2c_client, REGS_GAIN, value);
		value = gp2a->nondetect;
		gp2a_i2c_write(gp2a->i2c_client, REGS_HYS, value);
		value = 0x04;
		gp2a_i2c_write(gp2a->i2c_client, REGS_CYCLE, value);
		value = 0x03;
		gp2a_i2c_write(gp2a->i2c_client, REGS_OPMOD, value);

		enable_irq_wake(gp2a->irq);
		enable_irq(gp2a->irq);

		value = 0x00;
		gp2a_i2c_write(gp2a->i2c_client, REGS_CON, value);
	} else {
		disable_irq_wake(gp2a->irq);
		disable_irq(gp2a->irq);

		value = 0x02;
		gp2a_i2c_write(gp2a->i2c_client, REGS_OPMOD, value);
		gp2a_leda_onoff(gp2a->pdata, &gp2a->i2c_client->dev, power);
	}
	return 0;
}

static ssize_t adc_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}

static ssize_t state_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}

static ssize_t name_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}

static ssize_t vendor_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}

static int gp2a_cal_mode_read_file(struct gp2a_data *gp2a)
{
	int err;
	mm_segment_t old_fs;
	struct file *cal_mode_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_mode_filp)) {
		err = PTR_ERR(cal_mode_filp);
		if (err != -ENOENT)
			pr_err("[SENSOR] %s, Can't open cal_mode file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = cal_mode_filp->f_op->read(cal_mode_filp,
		(char *)&gp2a->cal_mode,
		sizeof(u8), &cal_mode_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("[SENSOR] %s, Can't read the cal_mode from file\n",
			__func__);
		filp_close(cal_mode_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}

	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int gp2a_cal_mode_save_file(char mode)
{
	struct file *cal_mode_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH,
		O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_mode_filp)) {
		pr_err("[SENSOR] %s, Can't open cal_mode file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_mode_filp);
		pr_err("[SENSOR] %s, err = %d\n", __func__, err);
		return err;
	}

	err = cal_mode_filp->f_op->write(cal_mode_filp,
		(char *)&mode, sizeof(u8), &cal_mode_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("[SENSOR] %s, Can't read the cal_mode from file\n",
			__func__);
		err = -EIO;
	}

	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}

static ssize_t prox_cal_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->cal_mode);
}

static ssize_t prox_cal_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	int err;

	if (sysfs_streq(buf, "1")) {
		gp2a->cal_mode = 1;
		gp2a->nondetect = PROX_NONDETECT_MODE1;
		gp2a->detect = PROX_DETECT_MODE1;
	} else if (sysfs_streq(buf, "2")) {
		gp2a->cal_mode = 2;
		gp2a->nondetect = PROX_NONDETECT_MODE2;
		gp2a->detect = PROX_DETECT_MODE2;
	} else if (sysfs_streq(buf, "0")) {
		gp2a->cal_mode = 0;
		gp2a->nondetect = PROX_NONDETECT;
		gp2a->detect = PROX_DETECT;
	} else {
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (gp2a->power_state == 1) {
		gp2a_power_onoff(gp2a, OFF);
		msleep(20);
		gp2a_power_onoff(gp2a, ON);
	}

	err = gp2a_cal_mode_save_file(gp2a->cal_mode);
	if (err < 0) {
		pr_err("[SENSOR] %s, prox_cal_write() failed\n", __func__);
		return err;
	}

	return size;
}

static DEVICE_ATTR(adc, 0440, adc_read, NULL);
static DEVICE_ATTR(state, 0440, state_read, NULL);
static DEVICE_ATTR(name, 0440, name_read, NULL);
static DEVICE_ATTR(vendor, 0440, vendor_read, NULL);
static DEVICE_ATTR(prox_cal, 0664, prox_cal_read, prox_cal_write);

static struct device_attribute *proxi_attrs[] = {
	&dev_attr_adc,
	&dev_attr_state,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_prox_cal,
	NULL,
};

#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
static int gp2a_regulator_onoff(struct device *dev, bool onoff)
{
	struct regulator *gp2a_vdd;
	int ret;

	pr_info("[SENSOR] %s %s\n", __func__, (onoff) ? "on" : "off");

	gp2a_vdd = devm_regulator_get(dev, "gp2a-vdd");
	if (IS_ERR(gp2a_vdd)) {
		pr_err("[SENSOR]: %s - cannot get gp2a_vdd\n", __func__);
		return -ENOMEM;
	}

	regulator_set_voltage(gp2a_vdd, 2850000, 2850000);

	if (onoff) {
		ret = regulator_enable(gp2a_vdd);
		if (ret) {
			pr_err("[SENSOR] %s: enable vdd failed (%d)\n",
				__func__, ret);
			return ret;
		}
	} else {
		ret = regulator_disable(gp2a_vdd);
		if (ret) {
			pr_err("[SENSOR] %s: disable vdd failed (%d)\n",
				__func__, ret);
			return ret;
		}
	}

	devm_regulator_put(gp2a_vdd);
	msleep(20);

	return 0;
}
#endif

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SENSORS_IIO)
	struct gp2a_data *gp2a = iio_priv(dev_get_drvdata(dev));
#else
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
#endif

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->power_state);
}

static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SENSORS_IIO)
	struct gp2a_data *gp2a = iio_priv(dev_get_drvdata(dev));
#else
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
#endif
	int value = 0;
	int err = 0;

	err = kstrtoint(buf, 10, &value);
	if (err) {
		pr_err("[SENSOR] %s, kstrtoint failed.", __func__);
		return -EINVAL;
	}
	if (value != 0 && value != 1) {
		pr_err("[SENSOR] %s, wrong value(%d)\n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&gp2a->power_lock);

	if (gp2a->power_state != value) {
		pr_info("[SENSOR] %s, enable(%d)\n", __func__, value);
		if (value) {
			err = gp2a_cal_mode_read_file(gp2a);
			if (err < 0 && err != -ENOENT)
				pr_err("[SENSOR] %s, cal_mode read fail\n",
					__func__);

			pr_info("[SENSOR] %s, mode(%d)\n", __func__,
				gp2a->cal_mode);
			if (gp2a->cal_mode == 2) {
				gp2a->nondetect = PROX_NONDETECT_MODE2;
				gp2a->detect = PROX_DETECT_MODE2;
			} else if (gp2a->cal_mode == 1) {
				gp2a->nondetect = PROX_NONDETECT_MODE1;
				gp2a->detect = PROX_DETECT_MODE1;
			} else {
				gp2a->nondetect = PROX_NONDETECT;
				gp2a->detect = PROX_DETECT;
			}

#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
			gp2a_regulator_onoff(&gp2a->i2c_client->dev, ON);
#endif
			gp2a_power_onoff(gp2a, ON);
			gp2a->power_state = value;

			gp2a->val_state = value;
#if !defined(CONFIG_SENSORS_IIO)
			input_report_abs(gp2a->input, ABS_DISTANCE,
				gp2a->val_state);
			input_sync(gp2a->input);
#endif
		} else {
			gp2a_power_onoff(gp2a, OFF);
#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
			gp2a_regulator_onoff(&gp2a->i2c_client->dev, OFF);
#endif
			gp2a->power_state = value;
		}
	} else {
		pr_err("[SENSOR] %s, wrong cmd for enable\n", __func__);
	}

	mutex_unlock(&gp2a->power_lock);
	return size;
}

#if defined(CONFIG_SENSORS_IIO)
static IIO_DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_enable_show, proximity_enable_store, 0);

static struct attribute *proximity_sysfs_attrs[] = {
	&iio_dev_attr_enable.dev_attr.attr,
	NULL
};
#else

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	NULL
};
#endif

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

#if defined(CONFIG_SENSORS_IIO)
static int gp2a_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan,
	int *val, int *val2, long mask)
{
	struct gp2a_data *data = iio_priv(indio_dev);
	int ret = -EINVAL;

	switch (chan->type) {
	case IIO_PROXIMITY:
		*val = data->val_state;
		break;
	default:
		pr_err("[SENSOR] %s, invalied channel\n", __func__);
		return ret;
	}

	return IIO_VAL_INT;
}

/*enable sensor called by iio buffer*/
static int gp2a_postenable(struct iio_dev *indio_dev)
{
	int ret=0;
	struct gp2a_data *gp2a = iio_priv(indio_dev);
	pr_info("[gp2a]in func %s will enable the sensor\n",__func__);
	mutex_lock(&gp2a->power_lock);

	if (gp2a->power_state != ON) {
		pr_info("[SENOSR] %s, enable(%d)\n", __func__, ON);
		if (ON) {
			ret = gp2a_cal_mode_read_file(gp2a);
			if (ret < 0 && ret != -ENOENT)
				pr_err("[SENOSR] %s, cal_mode read fail\n",
					__func__);

			pr_info("[SENOSR] %s, mode(%d)\n", __func__,
				gp2a->cal_mode);
			if (gp2a->cal_mode == 2) {
				gp2a->nondetect = PROX_NONDETECT_MODE2;
				gp2a->detect = PROX_DETECT_MODE2;
			} else if (gp2a->cal_mode == 1) {
				gp2a->nondetect = PROX_NONDETECT_MODE1;
				gp2a->detect = PROX_DETECT_MODE1;
			} else {
				gp2a->nondetect = PROX_NONDETECT;
				gp2a->detect = PROX_DETECT;
			}

#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
			gp2a_regulator_onoff(&gp2a->i2c_client->dev, ON);
#endif
			gp2a_power_onoff(gp2a, ON);
			gp2a->power_state = ON;

			gp2a->val_state = ON;

		}
	}

	mutex_unlock(&gp2a->power_lock);
	return 0;
}

/*disable sensor called by iio buffer*/
static int gp2a_predisable(struct iio_dev *indio_dev)
{
	struct gp2a_data *gp2a = iio_priv(indio_dev);
	pr_info("[gp2a]in func %s will cancel the work\n",__func__);

	mutex_lock(&gp2a->power_lock);
	if(gp2a->power_state != OFF){
		gp2a_power_onoff(gp2a, OFF);
#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
		gp2a_regulator_onoff(&gp2a->i2c_client->dev, OFF);
#endif
		gp2a->power_state = OFF;
	}
	mutex_unlock(&gp2a->power_lock);

	return 0;
}

static int gp2a_data_rdy_trigger_set_state(struct iio_trigger *trig,
	bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	pr_info("[gp2a]in func %s will enable the sensor\n",__func__);
	if(state)
		gp2a_postenable(indio_dev);
	else
		gp2a_predisable(indio_dev);
	pr_info("[gp2a]in func %s will enable the sensor\n",__func__);
	return 0;
}

static const struct iio_info gp2a_info = {
	.attrs = &proximity_attribute_group,
	.driver_module = THIS_MODULE,
	.read_raw = gp2a_read_raw,
};
static const struct iio_trigger_ops gp2a_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &gp2a_data_rdy_trigger_set_state,
};

static const struct iio_buffer_setup_ops gp2a_buffer_setup_ops = {
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int gp2a_probe_buffer(struct iio_dev *indio_dev)
{
	int ret = 0;
	struct iio_buffer *buffer;

	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		return ret;
	}
	indio_dev->buffer = buffer;

	/* setup ring buffer */
	buffer->scan_timestamp = true;
	buffer->bytes_per_datum = 16;
	indio_dev->setup_ops = &gp2a_buffer_setup_ops;

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	ret = iio_buffer_register(indio_dev,
				  gp2a_channels,
				  ARRAY_SIZE(gp2a_channels));
	if (ret) {
		pr_err("[gp2a]failed to initialize the ring\n");
	}
	return 0;
}

static void gp2a_remove_buffer(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
}

static irqreturn_t gp2a_trigger_handler(int irq, void *p)
{
	char *tmp;
	int d_index=0;
	s64 tmp_buf[2]={0};
	s64 timestamp = gp2a_iio_get_boottime_ns();
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct gp2a_data *gp2a = iio_priv(indio_dev);

	tmp = (unsigned char *)tmp_buf;
	memcpy(tmp,&gp2a->val_state,sizeof(gp2a->val_state));
	d_index+=sizeof(gp2a->val_state);
	if (indio_dev->buffer->scan_timestamp){
		tmp_buf[(d_index + 7)/8] = timestamp;
	}
	iio_push_to_buffers(indio_dev, tmp);
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int gp2a_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct gp2a_data *gp2a = iio_priv(indio_dev);
	indio_dev->pollfunc = iio_alloc_pollfunc(&gp2a_iio_pollfunc_store_time,
		&gp2a_trigger_handler, IRQF_ONESHOT, indio_dev,
		"%s_consumer%d", indio_dev->name, indio_dev->id);
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	gp2a->trig = iio_trigger_alloc("%s-dev%d", indio_dev->name,
		indio_dev->id);
	if (!gp2a->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	gp2a->trig->dev.parent = indio_dev->dev.parent;
	gp2a->trig->ops = &gp2a_trigger_ops;
	iio_trigger_set_drvdata(gp2a->trig, indio_dev);
	indio_dev->trig = gp2a->trig;
	ret = iio_trigger_register(gp2a->trig);
	if (ret)
		goto error_free_trig;
	return 0;

error_free_trig:
	iio_trigger_free(gp2a->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}


static void gp2a_remove_trigger(struct iio_dev *indio_dev)
{
	iio_trigger_unregister(indio_dev->trig);
	iio_trigger_free(indio_dev->trig);
}
#endif

static void gp2a_prox_work_func(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of(work,
		struct gp2a_data, work_prox);
#if defined(CONFIG_SENSORS_IIO)
	unsigned long flags;
#endif
	u8 vo, value;

	gp2a_i2c_read(gp2a->i2c_client, REGS_PROX, &vo);
	vo = 0x01 & vo;

	value = 0x18;
	gp2a_i2c_write(gp2a->i2c_client, REGS_CON, value);

	if (!vo) {
		gp2a->val_state = 0x01;
		value = gp2a->nondetect;
	} else {
		gp2a->val_state = 0x00;
		value = gp2a->detect;
	}
	gp2a_i2c_write(gp2a->i2c_client, REGS_HYS, value);

	pr_info("[SENSOR] %s, %d\n", __func__, gp2a->val_state);

#if defined(CONFIG_SENSORS_IIO)
	spin_lock_irqsave(&gp2a->spin_lock, flags);
	iio_trigger_poll(gp2a->trig, gp2a_iio_get_boottime_ns());
	spin_unlock_irqrestore(&gp2a->spin_lock, flags);
#else
	input_report_abs(gp2a->input, ABS_DISTANCE, gp2a->val_state);
	input_sync(gp2a->input);
#endif
	msleep(20);

	value = 0x00;
	gp2a_i2c_write(gp2a->i2c_client, REGS_CON, value);
}

irqreturn_t gp2a_irq_handler(int irq, void *data)
{
	struct gp2a_data *gp2a = data;
	pr_info("[SENSOR] %s, %d\n", __func__, irq);

	schedule_work((struct work_struct *)&gp2a->work_prox);
	wake_lock_timeout(&gp2a->prx_wake_lock, 3*HZ);

	return IRQ_HANDLED;
}

static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	struct gp2a_platform_data *pdata = gp2a->pdata;
	int ret;
	u8 value;

	ret = gpio_request(pdata->p_out, "gpio_proximity_out");
	if (ret < 0) {
		pr_err("[SENSOR] %s, gpio %d request failed (%d)\n",
			__func__, pdata->p_out, ret);
		return ret;
	}

	ret = gpio_direction_input(pdata->p_out);
	if (ret < 0) {
		pr_err("[SENSOR] %s, failed gpio %d as input (%d)\n",
			__func__, pdata->p_out, ret);
		goto err_gpio_direction_input;
	}

	value = 0x18;
	gp2a_i2c_write(gp2a->i2c_client, REGS_CON, value);

	gp2a->irq = gpio_to_irq(pdata->p_out);
	ret = request_irq(gp2a->irq, gp2a_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
		"proximity_int", gp2a);
	if (ret < 0) {
		pr_err("[SENSOR] %s, request_irq(%d) failed for gpio %d (%d)\n",
			__func__, gp2a->irq, pdata->p_out, ret);
		goto err_request_irq;
	}

	pr_info("[SENSOR] %s, request_irq(%d) success for gpio %d\n",
		__func__, gp2a->irq, pdata->p_out);

	disable_irq(gp2a->irq);

	value = 0x02;
	gp2a_i2c_write(gp2a->i2c_client, REGS_OPMOD, value);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->p_out);
done:
	return ret;
}

static int gp2a_request_gpio(struct gp2a_platform_data *pdata)
{
	int ret = 0;

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	ret = gpio_request(pdata->power_en, "prox_en");
	if (ret) {
		pr_err("[SENSOR] %s: gpio request fail\n", __func__);
		return ret;
	}

	ret = gpio_direction_output(pdata->power_en, 0);
	if (ret) {
		pr_err("[SENSOR] %s: unable to set_direction [%d]\n",
			__func__, pdata->power_en);
		return ret;
	}
#endif
	return ret;
}

static int gp2a_parse_dt(struct device *dev, struct gp2a_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;

	if (pdata == NULL)
		return -ENODEV;

	pdata->p_out = of_get_named_gpio_flags(np, "gp2a-i2c,irq-gpio", 0,
		&flags);
	if (pdata->p_out < 0) {
		pr_err("[SENSOR] %s : get irq_gpio(%d) error\n", __func__,
			pdata->p_out);
		return -ENODEV;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	pdata->power_en = of_get_named_gpio_flags(np, "gp2a-i2c,en-gpio", 0,
		&flags);
	if (pdata->power_en < 0) {
		pr_err("[SENSOR] %s : get power_en(%d) error\n", __func__,
			pdata->power_en);
		return -ENODEV;
	}
#endif

	return 0;
}


static int gp2a_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct gp2a_data *gp2a;
	struct gp2a_platform_data *pdata = client->dev.platform_data;
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev;
#endif
	int ret;

	pr_info("[SENSOR] %s, start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR] %s, i2c functionality failed\n", __func__);
		return -ENOMEM;
	}

#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
	ret = gp2a_regulator_onoff(&client->dev, ON);
	if (ret) {
		pr_err("[SENSOR] %s, Power Up Failed\n", __func__);
		return ret;
	}
#endif

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct gp2a_platform_data), GFP_KERNEL);
		ret = gp2a_parse_dt(&client->dev, pdata);
		if (ret < 0)
			return ret;
	}

	ret = gp2a_request_gpio(pdata);
	if (ret < 0)
		goto err_request_gpio;

	gp2a_leda_onoff(pdata, &client->dev, ON);

	/* Check if the device is there or not. */
	ret = gp2a_i2c_write(client, REGS_CON, 0x00);
	if (ret < 0) {
		pr_err("[SENSOR] %s, gp2a is not connected.(%d)\n", __func__,
			ret);
		goto err_setup_dev;
	}

#if defined(CONFIG_SENSORS_IIO)
	/* Configure IIO device */
	indio_dev = iio_device_alloc(sizeof(*gp2a));
	if (!indio_dev) {
		pr_err("[SENSOR] %s, iio_device_alloc failed\n", __func__);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	gp2a = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);

	/* Init IIO device */
	indio_dev->name = "proximity_sensor";
	indio_dev->channels = gp2a_channels;
	indio_dev->num_channels = ARRAY_SIZE(gp2a_channels);
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &gp2a_info;
	ret = gp2a_probe_buffer(indio_dev);
	if (ret) {
		pr_err("[gp2a]failed to probe the buffer\n");
		goto exit_iio_buffer_failed;
	}

	ret = gp2a_probe_trigger(indio_dev);
	if (ret) {
		pr_err("[gp2a]failed to gp2a_probe_trigger\n");
		goto exit_iio_trigger_failed;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		pr_err("[SENSOR] %s: iio_device_register failed(%d)\n",
			__func__, ret);
		goto exit_iio_register_failed;
	}

	gp2a->pdata = pdata;
	gp2a->i2c_client = client;

#else	/* !defined(CONFIG_SENSORS_IIO)*/

	/* Allocate memory for driver data */
	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("[SENSOR] %s, failed memory alloc\n", __func__);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}
	i2c_set_clientdata(client, gp2a);

	gp2a->pdata = pdata;
	gp2a->i2c_client = client;

	gp2a->input = input_allocate_device();
	if (!gp2a->input) {
		pr_err("[SENSOR] %s, could not allocate input device\n",
			__func__);
		goto err_input_allocate_device_proximity;
	}

	input_set_drvdata(gp2a->input, gp2a);
	gp2a->input->name = "proximity_sensor";
	input_set_capability(gp2a->input, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(gp2a->input, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(gp2a->input);
	if (ret < 0) {
		pr_err("[SENSOR] %s, could not register input device\n",
			__func__);
		goto err_input_register_device_proximity;
	}
	ret = sensors_create_symlink(&gp2a->input->dev.kobj,
		gp2a->input->name);
	if (ret < 0) {
		pr_err("[SENSOR] %s, create sysfs symlink error\n", __func__);
		goto err_sysfs_create_symlink_proximity;
	}

	ret = sysfs_create_group(&gp2a->input->dev.kobj,
		&proximity_attribute_group);
	if (ret) {
		pr_err("[SENSOR] %s, create sysfs group error\n", __func__);
		goto err_sysfs_create_group_proximity;
	}
#endif

	wake_lock_init(&gp2a->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");
	mutex_init(&gp2a->power_lock);

	INIT_WORK(&gp2a->work_prox, gp2a_prox_work_func);

	ret = gp2a_setup_irq(gp2a);
	if (ret) {
		pr_err("[SENSOR] %s, could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	ret = sensors_register(gp2a->dev, gp2a, proxi_attrs,
		"proximity_sensor");
	if (ret < 0) {
		pr_info("[SENSOR] %s, could not sensors_register\n",
			__func__);
		goto err_sensors_register;
	}

	gp2a_leda_onoff(pdata, &client->dev, OFF);

	pr_info("[SENSOR] %s done\n", __func__);
	goto done;

err_sensors_register:
	free_irq(gp2a->irq, gp2a);
	gpio_free(gp2a->pdata->p_out);
err_setup_irq:
	mutex_destroy(&gp2a->power_lock);
	wake_lock_destroy(&gp2a->prx_wake_lock);
#if defined(CONFIG_SENSORS_IIO)
	iio_device_unregister(indio_dev);
exit_iio_register_failed:
	gp2a_remove_trigger(indio_dev);
exit_iio_trigger_failed:
	gp2a_remove_buffer(indio_dev);
exit_iio_buffer_failed:
	i2c_set_clientdata(client, NULL);
#else
	sysfs_remove_group(&gp2a->input->dev.kobj,
		&proximity_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&gp2a->input->dev.kobj,
		gp2a->input->name);
err_sysfs_create_symlink_proximity:
	input_unregister_device(gp2a->input);
err_input_register_device_proximity:
	input_free_device(gp2a->input);
err_input_allocate_device_proximity:
	kfree(gp2a);
#endif
err_mem_alloc:
err_setup_dev:
err_request_gpio:
	gp2a_leda_onoff(pdata, &client->dev, OFF);
done:
#if defined(CONFIG_SENSORS_GP2A_HAS_REGULATOR)
	gp2a_regulator_onoff(&client->dev, OFF);
#endif
	return ret;
}

static const struct i2c_device_id gp2a_device_id[] = {
	{"gp2a", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, gp2a_device_id);

static struct of_device_id gp2a_i2c_match_table[] = {
	{ .compatible = "gp2a-i2c",},
	{},
};

MODULE_DEVICE_TABLE(of, gp2a_i2c_match_table);

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner = THIS_MODULE,
		.of_match_table = gp2a_i2c_match_table,

	},
	.probe		= gp2a_i2c_probe,
	.id_table	= gp2a_device_id,
};

module_i2c_driver(gp2a_i2c_driver);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for gp2ap002");
MODULE_LICENSE("GPL");
