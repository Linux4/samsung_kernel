/* driver/sensor/cm36672p.c
 * Copyright (c) 2011 SAMSUNG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/sensor/sensors_core.h>
#include "cm36672p.h"

///IIO additions...
//
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#undef PROXIMITY_FOR_TEST	/* for HW to tune up */

#define MODULE_NAME      "proximity_sensor"
#define VENDOR          "CAPELLA"
#define CHIP_ID         "CM36672P"

#define I2C_M_WR        0 /* for i2c Write */

enum {
	PS_CONF1 = 0,
	PS_CONF3,
	PS_THD_LOW,
	PS_THD_HIGH,
	PS_CANCEL,
	PS_REG_NUM,
};

enum {
	REG_ADDR = 0,
	CMD,
};

/* proximity sensor regsiter addresses */
#define REG_PS_CONF1    0x03
#define REG_PS_CONF3    0x04
#define REG_PS_CANC     0x05
#define REG_PS_THD_LOW  0x06
#define REG_PS_THD_HIGH 0x07
#define REG_PS_DATA     0x08

/* proximity sensor default value for register */
#define DEFAULT_HI_THD  0x0011
#define DEFAULT_LOW_THD 0x000d
#define CANCEL_HI_THD   0x000a
#define CANCEL_LOW_THD  0x0007

#define DEFAULT_CONF1   0x0320 /* PS_INT = (1:1), PS_PERS = (1:0) */
#if defined(CONFIG_SENSORS_CM36672P_SMART_PERS)
#define DEFAULT_CONF3   0x4010 /* PS_MS = 1, PS_SMART_PERS = 1 */
#else
#define DEFAULT_CONF3   0x4000 /* PS_MS = 1, PS_SMART_PERS = 0 */
#endif
#define DEFAULT_TRIM    0x0000

/*
* NOTE:
* Since PS Duty, PS integration time and LED current
* would be different by HW rev or Project,
* we move the setting value to device tree.
* Please refer to the value below.
*
* PS_DUTY (CONF1, 0x03_L)
* 1/40 = 0, 1/80 = 1, 1/160 = 2, 1/320 = 3
*
* PS_IT (CONF1, 0x03_L)
* 1T = 0, 1.5T = 1, 2T = 2, 2.5T = 3, 3T = 4, 3.5T = 5, 4T = 6, 8T = 7
*
* LED_I (CONF3, 0x04_H)
* 50mA = 0, 75mA = 1, 100mA = 2, 120mA = 3,
* 140mA = 4, 160mA = 5, 180mA = 6, 200mA = 7
*/
static u16 ps_reg_init_setting[PS_REG_NUM][2] = {
	{REG_PS_CONF1, DEFAULT_CONF1}, /* REG_PS_CONF1 */
	{REG_PS_CONF3, DEFAULT_CONF3}, /* REG_PS_CONF3 */
	{REG_PS_THD_LOW, DEFAULT_LOW_THD}, /* REG_PS_THD_LOW */
	{REG_PS_THD_HIGH, DEFAULT_HI_THD}, /* REG_PS_THD_HIGH */
	{REG_PS_CANC, DEFAULT_TRIM}, /* REG_PS_CANC */
};

/* Intelligent Cancelation*/
#define CM36672P_CANCELATION
#ifdef CM36672P_CANCELATION
#define CANCELATION_FILE_PATH   "/efs/FactoryApp/prox_cal"
#define CAL_SKIP_ADC    8 /* nondetect threshold *60% */
#define CAL_FAIL_ADC    20 /* detect threshold */
enum {
	CAL_FAIL = 0,
	CAL_CANCELATION,
	CAL_SKIP,
};
#endif

#define PROX_READ_NUM   40

enum {
	OFF = 0,
	ON,
};

#define CM36672P_PROX_INFO_SHARED_MASK		(BIT(IIO_CHAN_INFO_SCALE))
#define CM36672P_PROX_INFO_SEPARATE_MASK		(BIT(IIO_CHAN_INFO_RAW))

#define CM36672P_PROX_CHANNEL()						\
{									\
	.type = IIO_PROXIMITY,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_PROXIMITY,					\
	.info_mask_separate = CM36672P_PROX_INFO_SEPARATE_MASK,		\
	.info_mask_shared_by_type = CM36672P_PROX_INFO_SHARED_MASK,	\
	.scan_index = CM36672P_SCAN_PROX_CH,				\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 32,				\
		.storagebits = 32,				\
		.shift = 0,				\
	},							\
}

enum {
	CM36672P_SCAN_PROX_CH,
	CM36672P_SCAN_PROX_TIMESTAMP,
};

/* driver data */
struct cm36672p_data {
	struct i2c_client *i2c_client;
	struct wake_lock prox_wake_lock;
	struct input_dev *proximity_input_dev;
	struct cm36672p_platform_data *pdata;
	struct mutex read_lock;
	struct hrtimer prox_timer;
	struct workqueue_struct *prox_wq;
	struct work_struct work_prox;
	struct device *proximity_dev;
	struct regulator *vdd;
	struct regulator *vled;

	ktime_t prox_poll_delay;
	atomic_t enable;
	int irq;
	int avg[3];
	unsigned int prox_cal_result;

	int prox_delay;
	struct wake_lock prx_wake_lock;
	struct iio_trigger *prox_trig;
	int16_t sampling_frequency_prox;
	atomic_t pseudo_irq_enable_prox;
	struct mutex lock;
	spinlock_t spin_lock;
	struct iio_dev *indio_dev_prox;
	u8 proximity_detection;
};

static int proximity_vdd_onoff(struct device *dev, bool onoff);
static int proximity_vled_onoff(struct device *dev, bool onoff);

struct cm36672p_sensor_data {
	struct cm36672p_data *cdata;
};

static inline s64 cm36672p_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

irqreturn_t cm36672p_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = cm36672p_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}

static int cm36672p_prox_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;
	unsigned long flags;

	spin_lock_irqsave(&cm36672p_iio->spin_lock, flags);
	iio_trigger_poll(cm36672p_iio->prox_trig);
	spin_unlock_irqrestore(&cm36672p_iio->spin_lock, flags);
	return 0;
}


int cm36672p_i2c_read_word(struct cm36672p_data *data, u8 command, u16 *val)
{
	int err = 0;
	int retry = 3;
	struct i2c_client *client = data->i2c_client;
	struct i2c_msg msg[2];
	unsigned char tmp[2] = {0,};
	u16 value = 0;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		/* send slave address & command */
		msg[0].addr = client->addr;
		msg[0].flags = I2C_M_WR;
		msg[0].len = 1;
		msg[0].buf = &command;

		/* read word data */
		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 2;
		msg[1].buf = tmp;

		err = i2c_transfer(client->adapter, msg, 2);

		if (err >= 0) {
			value = (u16)tmp[1];
			*val = (value << 8) | (u16)tmp[0];
			return err;
		}
	}
	SENSOR_ERR("i2c transfer error ret=%d\n", err);
	return err;
}

int cm36672p_i2c_write_word(struct cm36672p_data *data, u8 command,
	u16 val)
{
	int err = 0;
	struct i2c_client *client = data->i2c_client;
	int retry = 3;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		err = i2c_smbus_write_word_data(client, command, val);
		if (err >= 0)
			return 0;
	}
	SENSOR_ERR("i2c transfer error(%d)\n", err);
	return err;
}

#ifdef CM36672P_CANCELATION
static int proximity_open_cancelation(struct cm36672p_data *data)
{
	struct file *cancel_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(cancel_filp)) {
		err = PTR_ERR(cancel_filp);
		if (err != -ENOENT)
			SENSOR_ERR("Can't open cancelation file(%d)\n", err);
		set_fs(old_fs);
		return err;
	}

	err = cancel_filp->f_op->read(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		SENSOR_ERR("Can't read the cancel data from file(%d)\n", err);
		err = -EIO;
	}

	/*If there is an offset cal data. */
	if (ps_reg_init_setting[PS_CANCEL][CMD] != data->pdata->default_trim) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] =
			data->pdata->cancel_hi_thd ?
			data->pdata->cancel_hi_thd :
			CANCEL_HI_THD;
		ps_reg_init_setting[PS_THD_LOW][CMD] =
			data->pdata->cancel_low_thd ?
			data->pdata->cancel_low_thd :
			CANCEL_LOW_THD;
	}

	SENSOR_INFO("prox_cal = 0x%x, high_thd = 0x%x, low_thd = 0x%x\n",
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int proximity_store_cancelation(struct device *dev, bool do_calib)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	struct file *cancel_filp = NULL;
	mm_segment_t old_fs;
	int err;
	u16 ps_data = 0;

	if (do_calib) {
		mutex_lock(&data->read_lock);
		cm36672p_i2c_read_word(data, REG_PS_DATA, &ps_data);
		mutex_unlock(&data->read_lock);

		if (ps_data * 100 < (data->pdata->default_low_thd * 50)) {
			/* SKIP. CAL_SKIP_ADC */
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			SENSOR_INFO("crosstalk < %d/100\n",
				(data->pdata->default_low_thd * 50));
			data->prox_cal_result = CAL_SKIP;
		} else if (ps_data <= data->pdata->default_hi_thd) {
			/* CANCELATION. CAL_FAIL_ADC */
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim + ps_data;
			SENSOR_INFO("crosstalk_offset = %u", ps_data);
			data->prox_cal_result = CAL_CANCELATION;
		} else {
			/*FAIL*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			SENSOR_INFO("crosstalk > %d\n",
				data->pdata->default_hi_thd);
			data->prox_cal_result = CAL_FAIL;
		}

		if (data->prox_cal_result == CAL_CANCELATION) {
			ps_reg_init_setting[PS_THD_HIGH][CMD] =
				data->pdata->cancel_hi_thd ?
				data->pdata->cancel_hi_thd :
				CANCEL_HI_THD;
			ps_reg_init_setting[PS_THD_LOW][CMD] =
				data->pdata->cancel_low_thd ?
				data->pdata->cancel_low_thd :
				CANCEL_LOW_THD;
		} else {
			ps_reg_init_setting[PS_THD_HIGH][CMD] =
				data->pdata->default_hi_thd ?
				data->pdata->default_hi_thd :
				DEFAULT_HI_THD;
			ps_reg_init_setting[PS_THD_LOW][CMD] =
				data->pdata->default_low_thd ?
				data->pdata->default_low_thd :
				DEFAULT_LOW_THD;
		}
	} else { /* reset */
		ps_reg_init_setting[PS_CANCEL][CMD] = data->pdata->default_trim;
		ps_reg_init_setting[PS_THD_HIGH][CMD] =
			data->pdata->default_hi_thd ?
			data->pdata->default_hi_thd :
			DEFAULT_HI_THD;
		ps_reg_init_setting[PS_THD_LOW][CMD] =
			data->pdata->default_low_thd ?
			data->pdata->default_low_thd :
			DEFAULT_LOW_THD;
	}

	if ((data->prox_cal_result == CAL_CANCELATION) || !do_calib) {
		err = cm36672p_i2c_write_word(data, REG_PS_CANC,
			ps_reg_init_setting[PS_CANCEL][CMD]);
		if (err < 0)
			SENSOR_ERR("ps_canc_reg is failed. %d\n", err);
		err = cm36672p_i2c_write_word(data, REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			SENSOR_ERR("ps_high_reg is failed. %d\n", err);
		err = cm36672p_i2c_write_word(data, REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			SENSOR_ERR("ps_low_reg is failed. %d\n", err);
	}

	SENSOR_INFO("prox_cal = 0x%x, high_thd = 0x%x, low_thd = 0x%x\n",
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cancel_filp)) {
		set_fs(old_fs);
		err = PTR_ERR(cancel_filp);
		SENSOR_ERR("Can't open cancelation file(%d)\n", err);
		return err;
	}

	err = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		SENSOR_ERR("Can't write the cancel data to file(%d)\n", err);
		err = -EIO;
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	if (!do_calib) /* delay for clearing */
		msleep(150);

	return err;
}

static ssize_t proximity_cancel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1")) /* calibrate cancelation value */
		do_calib = true;
	else if (sysfs_streq(buf, "0")) /* reset cancelation value */
		do_calib = false;
	else {
		SENSOR_ERR("invalid value %d\n", *buf);
		return -EINVAL;
	}

	err = proximity_store_cancelation(dev, do_calib);
	if (err < 0) {
		SENSOR_ERR("proximity_store_cancelation() failed(%d)\n", err);
		return err;
	}

	return size;
}

static ssize_t proximity_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n",
		ps_reg_init_setting[PS_CANCEL][CMD] - data->pdata->default_trim,
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);
}

static ssize_t proximity_cancel_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	SENSOR_INFO("%u\n", data->prox_cal_result);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->prox_cal_result);
}
#endif
#if 0
static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	bool new_value;
	int pre_enable;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		SENSOR_ERR("invalid value %d\n", *buf);
		return -EINVAL;
	}

	pre_enable = atomic_read(&data->enable);
	SENSOR_INFO("new_value = %d, pre_enable = %d\n",
		new_value, pre_enable);

	if (new_value && !pre_enable) {
		int i, ret;

		if (!data->pdata->vdd_always_on)
			proximity_vdd_onoff(dev, ON);
		proximity_vled_onoff(dev, ON);

		atomic_set(&data->enable, ON);
#ifdef CM36672P_CANCELATION
		/* open cancelation data */
		ret = proximity_open_cancelation(data);
		if (ret < 0 && ret != -ENOENT)
			SENSOR_ERR("proximity_open_cancelation() failed\n");

#endif
		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++)
			cm36672p_i2c_write_word(data,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);

		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(data->proximity_input_dev);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else if (!new_value && pre_enable) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
		/* disable settings */
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
		atomic_set(&data->enable, OFF);
		proximity_vled_onoff(dev, OFF);
		if (!data->pdata->vdd_always_on)
			proximity_vdd_onoff(dev, OFF);
	}
	SENSOR_INFO("enable = %d\n", atomic_read(&data->enable));

	return size;
}

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&data->enable));
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};
#endif

/* sysfs for vendor & name */
static ssize_t cm36672p_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t cm36672p_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t proximity_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->pdata->default_trim);
}

#if defined(PROXIMITY_FOR_TEST)
static ssize_t proximity_trim_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	u16 trim_value;
	int err;

	err = kstrtou16(buf, 10, &trim_value);
	if (err < 0) {
		SENSOR_ERR("kstrtoint failed.\n");
		return size;
	}
	SENSOR_INFO("trim_value: %u\n", trim_value);

	if (trim_value > -1) {
		data->pdata->default_trim = trim_value;
		ps_reg_init_setting[PS_CANCEL][CMD] = trim_value;
		data->pdata->default_trim = trim_value;
		err = cm36672p_i2c_write_word(data, REG_PS_CANC,
			ps_reg_init_setting[PS_CANCEL][CMD]);
		if (err < 0)
			SENSOR_ERR("cm36672p_ps_canc is failed. %d\n", err);
		SENSOR_INFO("new trim_value = %u\n", trim_value);
		msleep(150);
	} else
		SENSOR_ERR("wrong trim_value (%u)!!\n", trim_value);

	return size;
}
#endif

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", data->avg[0],
		data->avg[1], data->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	bool new_value = false;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		SENSOR_ERR("invalid value %d\n", *buf);
		return -EINVAL;
	}

	SENSOR_INFO("average enable = %d\n", new_value);
	if (new_value) {
		if (atomic_read(&data->enable) == OFF) {
			if (!data->pdata->vdd_always_on)
				proximity_vdd_onoff(dev, ON);
			proximity_vled_onoff(dev, ON);

			cm36672p_i2c_write_word(data, REG_PS_CONF1,
				ps_reg_init_setting[PS_CONF1][CMD]);
		}
		hrtimer_start(&data->prox_timer, data->prox_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&data->prox_timer);
		cancel_work_sync(&data->work_prox);
		if (atomic_read(&data->enable) == OFF) {
			cm36672p_i2c_write_word(data, REG_PS_CONF1,
				0x0001);
			proximity_vled_onoff(dev, OFF);
			if (!data->pdata->vdd_always_on)
				proximity_vdd_onoff(dev, OFF);
		}
	}

	return size;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	u16 ps_data;

	if (atomic_read(&data->enable) == OFF) {
		if (!data->pdata->vdd_always_on)
			proximity_vdd_onoff(dev, ON);
		proximity_vled_onoff(dev, ON);

		cm36672p_i2c_write_word(data, REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&data->read_lock);
	cm36672p_i2c_read_word(data, REG_PS_DATA, &ps_data);
	mutex_unlock(&data->read_lock);

	if (atomic_read(&data->enable) == OFF) {
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);

		proximity_vled_onoff(dev, OFF);
		if (!data->pdata->vdd_always_on)
			proximity_vdd_onoff(dev, OFF);
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", ps_data);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SENSOR_INFO("%u,%u\n",
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);
	return snprintf(buf, PAGE_SIZE, "%u,%u\n",
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	u16 thresh_value = ps_reg_init_setting[PS_THD_HIGH][CMD];
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		SENSOR_ERR("kstrtoint failed\n");

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data, REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			SENSOR_ERR("cm36672_ps_high_reg is failed. %d\n", err);
		SENSOR_INFO("new high threshold = 0x%x\n", thresh_value);
		msleep(150);
	} else
		SENSOR_ERR("wrong high threshold value(0x%x)\n", thresh_value);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	SENSOR_INFO("%u,%u\n",
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	return snprintf(buf, PAGE_SIZE, "%u,%u\n",
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	u16 thresh_value = ps_reg_init_setting[PS_THD_LOW][CMD];
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		SENSOR_ERR("kstrtoint failed\n");

	SENSOR_INFO("thresh_value:%u\n", thresh_value);
	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_LOW][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data, REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			SENSOR_ERR("cm36672_ps_low_reg is failed. %d\n", err);
		SENSOR_INFO("new low threshold = 0x%x\n", thresh_value);
		msleep(150);
	} else
		SENSOR_ERR("wrong low threshold value(0x%x)\n", thresh_value);

	return size;
}

#if defined(PROXIMITY_FOR_TEST)
static ssize_t proximity_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int regist = 0, val = 0;
	struct cm36672p_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%x,%x", &regist, &val) != 2) {
		SENSOR_ERR("The number of data are wrong\n");
		return -EINVAL;
	}

	cm36672p_i2c_write_word(data, regist, val);
	SENSOR_INFO("Register(0x%2x) 16:data(0x%4x) 10:%d\n",
			regist, val, val);

	return count;
}

static ssize_t proximity_register_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 val[10], i;
	struct cm36672p_data *data = dev_get_drvdata(dev);

	for (i = 0; i < 10; i++) {
		cm36672p_i2c_read_word(data, i, &val[i]);
		SENSOR_INFO("Register(0x%2x) data(0x%4x)\n", i, val[i]);
	}

	return snprintf(buf, PAGE_SIZE,
		"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		val[0], val[1], val[2], val[3], val[4],
		val[5], val[6], val[7], val[8], val[9]);
}
#endif

static DEVICE_ATTR(vendor, S_IRUGO, cm36672p_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, cm36672p_name_show, NULL);
#ifdef CM36672P_CANCELATION
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show,
	NULL);
#endif
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
#if defined(PROXIMITY_FOR_TEST)
static DEVICE_ATTR(prox_trim, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_trim_show, proximity_trim_store);
static DEVICE_ATTR(prox_register, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_register_read_show, proximity_register_write_store);
#else
static DEVICE_ATTR(prox_trim,  S_IRUSR | S_IRGRP,
	proximity_trim_show, NULL);
#endif

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_avg,
	&dev_attr_state,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_raw_data,
	&dev_attr_prox_trim,
#if defined(PROXIMITY_FOR_TEST)
	&dev_attr_prox_register,
#endif
#ifdef CM36672P_CANCELATION
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
#endif
	NULL,
};

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t proximity_irq_thread_fn(int irq, void *user_data)
{
	struct cm36672p_data *data = user_data;
	u8 val;
	u16 ps_data = 0;
	int enabled;

	enabled = atomic_read(&data->enable);
	val = gpio_get_value(data->pdata->irq);
	cm36672p_i2c_read_word(data, REG_PS_DATA, &ps_data);

	if (enabled) {
		data->proximity_detection = val;
		cm36672p_prox_data_rdy_trig_poll(data->indio_dev_prox);
	}
	wake_lock_timeout(&data->prox_wake_lock, 3 * HZ);

	SENSOR_INFO("val = %u, ps_data = %u (close:0, far:1)\n", val, ps_data);

	return IRQ_HANDLED;
}

static void proximity_get_avg_val(struct cm36672p_data *data)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u16 ps_data = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		msleep(40);
		cm36672p_i2c_read_word(data, REG_PS_DATA,
			&ps_data);
		avg += ps_data;

		if (!i)
			min = ps_data;
		else if (ps_data < min)
			min = ps_data;

		if (ps_data > max)
			max = ps_data;
	}
	avg /= PROX_READ_NUM;

	data->avg[0] = min;
	data->avg[1] = avg;
	data->avg[2] = max;
}

static void cm36672_work_func_prox(struct work_struct *work)
{
	struct cm36672p_data *data = container_of(work,
		struct cm36672p_data, work_prox);
	proximity_get_avg_val(data);
}

static enum hrtimer_restart cm36672_prox_timer_func(struct hrtimer *timer)
{
	struct cm36672p_data *data = container_of(timer,
		struct cm36672p_data, prox_timer);
	queue_work(data->prox_wq, &data->work_prox);
	hrtimer_forward_now(&data->prox_timer, data->prox_poll_delay);
	return HRTIMER_RESTART;
}

static int proximity_vdd_onoff(struct device *dev, bool onoff)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	int ret;

	SENSOR_INFO("%s\n", (onoff) ? "on" : "off");

	if (!data->vdd) {
		SENSOR_INFO("VDD get regulator\n");
		data->vdd = devm_regulator_get(dev, "cm36672p,vdd");
		if (IS_ERR(data->vdd)) {
			SENSOR_ERR("cannot get vdd\n");
			data->vdd = NULL;
			return -ENOMEM;
		}

		if (!regulator_get_voltage(data->vdd))
			regulator_set_voltage(data->vdd, 3000000, 3300000);
	}

	if (onoff) {
		if (regulator_is_enabled(data->vdd)) {
			SENSOR_INFO("Regulator already enabled\n");
			return 0;
		}

		ret = regulator_enable(data->vdd);
		if (ret)
			SENSOR_ERR("Failed to enable vdd.\n");
		usleep_range(10000, 11000);
	} else {
		ret = regulator_disable(data->vdd);
		if (ret)
			SENSOR_ERR("Failed to disable vdd.\n");
	}

	SENSOR_INFO("end\n");
	return 0;
}

static int proximity_vled_onoff(struct device *dev, bool onoff)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	int ret;

	SENSOR_INFO("%s, ldo:%d\n",
		(onoff) ? "on" : "off", data->pdata->vled_ldo);

	/* ldo control */
	if (data->pdata->vled_ldo) {
		gpio_set_value(data->pdata->vled_ldo, onoff);
		if (onoff)
			msleep(20);
		return 0;
	}

	/* regulator(PMIC) control */
	if (!data->vled) {
		SENSOR_INFO("VLED get regulator\n");
		data->vled = devm_regulator_get(dev, "cm36672p,vled");
		if (IS_ERR(data->vled)) {
			SENSOR_ERR("cannot get vled\n");
			data->vled = NULL;
			return -ENOMEM;
		}
	}

	if (onoff) {
		if (regulator_is_enabled(data->vled)) {
			SENSOR_INFO("Regulator already enabled\n");
			return 0;
		}

		ret = regulator_enable(data->vled);
		if (ret)
			SENSOR_ERR("Failed to enable vled.\n");
		usleep_range(10000, 11000);
	} else {
		ret = regulator_disable(data->vled);
		if (ret)
			SENSOR_ERR("Failed to disable vled.\n");
	}

	return 0;
}

static int setup_reg_cm36672p(struct cm36672p_data *data)
{
	int ret, i;
	u16 tmp;

	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		ret = cm36672p_i2c_write_word(data,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
		if (ret < 0) {
			SENSOR_ERR("cm36672_ps_reg is failed. %d\n", ret);
			return ret;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	mutex_lock(&data->read_lock);
	ret = cm36672p_i2c_read_word(data, REG_PS_DATA, &tmp);
	mutex_unlock(&data->read_lock);
	if (ret < 0) {
		SENSOR_ERR("read ps_data failed\n");
		ret = -EIO;
	}

	/* turn off */
	cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
	cm36672p_i2c_write_word(data, REG_PS_CONF3, 0x0000);

	return ret;
}

static int setup_irq_cm36672p(struct cm36672p_data *data)
{
	int ret;
	struct cm36672p_platform_data *pdata = data->pdata;

	ret = gpio_request(pdata->irq, "gpio_proximity_out");
	if (ret < 0) {
		SENSOR_ERR("gpio %d request failed(%d)\n", pdata->irq, ret);
		return ret;
	}

	ret = gpio_direction_input(pdata->irq);
	if (ret < 0) {
		SENSOR_ERR("failed to set gpio %d as input(%d)\n",
						pdata->irq, ret);
		gpio_free(pdata->irq);
		return ret;
	}

	data->irq = gpio_to_irq(pdata->irq);
	/* add IRQF_NO_SUSPEND option in case of Spreadtrum AP */
	ret = request_threaded_irq(data->irq, NULL, proximity_irq_thread_fn,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_int", data);
	if (ret < 0) {
		SENSOR_ERR("request_irq(%d) failed for gpio %d (%d)\n",
			data->irq, pdata->irq, ret);
		gpio_free(pdata->irq);
		return ret;
	}

	/* start with interrupts disabled */
	disable_irq(data->irq);

	SENSOR_ERR("success\n");
	return ret;
}

static const struct iio_chan_spec cm36672p_prox_channels[] = {
	CM36672P_PROX_CHANNEL(),
	IIO_CHAN_SOFT_TIMESTAMP(CM36672P_SCAN_PROX_TIMESTAMP)
};

static int cm36672p_read_raw_prox(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;
	int ret = -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	pr_info(" %s\n", __func__);
	mutex_lock(&cm36672p_iio->lock);

	switch (mask) {
	case 0:
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}

	mutex_unlock(&cm36672p_iio->lock);

	return ret;
}

static const struct iio_info cm36672p_prox_info= {

	.read_raw = &cm36672p_read_raw_prox,
	.driver_module = THIS_MODULE,

};

static enum hrtimer_restart cm36672p_prox_timer_func(struct hrtimer *timer)
{
	struct cm36672p_data *cm36672p_iio
			= container_of(timer, struct cm36672p_data, prox_timer);
	queue_work(cm36672p_iio->prox_wq, &cm36672p_iio->work_prox);
	hrtimer_forward_now(&cm36672p_iio->prox_timer,
			cm36672p_iio->prox_poll_delay);
	return HRTIMER_RESTART;
}

static const struct iio_buffer_setup_ops cm36672p_prox_buffer_setup_ops = {
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};


static int cm36672p_prox_probe_buffer(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &cm36672p_prox_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36672P_SCAN_PROX_CH);

	pr_err("%s is success\n", __func__);
	return 0;

error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int cm36672p_prox_enable(struct cm36672p_data *data, int enable)
{
	bool new_value;
	int pre_enable;

	new_value = enable;
	pre_enable = atomic_read(&data->enable);
	SENSOR_INFO("new_value = %d, pre_enable = %d\n",
		new_value, pre_enable);

	if (new_value && !pre_enable) {
		int i, ret;

		atomic_set(&data->enable, ON);
#ifdef CM36672P_CANCELATION
		/* open cancelation data */
		ret = proximity_open_cancelation(data);
		if (ret < 0 && ret != -ENOENT)
			SENSOR_ERR("proximity_open_cancelation() failed\n");

#endif
		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++)
			cm36672p_i2c_write_word(data,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);

		data->proximity_detection = 1;
		cm36672p_prox_data_rdy_trig_poll(data->indio_dev_prox);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else if (!new_value && pre_enable) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
		/* disable settings */
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
		atomic_set(&data->enable, OFF);
	}
	SENSOR_INFO("enable = %d\n", atomic_read(&data->enable));

	return 0;
}


static int cm36672p_prox_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;

	if (!atomic_cmpxchg(&cm36672p_iio->pseudo_irq_enable_prox, 0, 1)) {
		mutex_lock(&cm36672p_iio->lock);
		cm36672p_prox_enable(cm36672p_iio, ON);
		mutex_unlock(&cm36672p_iio->lock);
	}

	return 0;
}

static int cm36672p_prox_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;

	if (atomic_cmpxchg(&cm36672p_iio->pseudo_irq_enable_prox, 1, 0)) {
		mutex_lock(&cm36672p_iio->lock);
		cm36672p_prox_enable(cm36672p_iio, OFF);
		mutex_unlock(&cm36672p_iio->lock);
	}
	return 0;
}

static int cm36672p_prox_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		cm36672p_prox_pseudo_irq_enable(indio_dev);
	else
		cm36672p_prox_pseudo_irq_disable(indio_dev);
	return 0;
}

static int cm36672p_data_prox_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	cm36672p_prox_set_pseudo_irq(indio_dev, state);
	return 0;
}

static const struct iio_trigger_ops cm36672p_prox_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cm36672p_data_prox_rdy_trigger_set_state,
};

static irqreturn_t cm36672p_prox_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;

	int len = 0;
	int ret = 0;
	int32_t *data;

	data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		/* TODO : data update */
		if (!cm36672p_iio->proximity_detection)
			*data = 0;
		else
			*data = 1;
	}

	len = 4;

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

static int cm36672p_prox_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;

	indio_dev->pollfunc = iio_alloc_pollfunc(&cm36672p_iio_pollfunc_store_boottime,
			&cm36672p_prox_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	cm36672p_iio->prox_trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!cm36672p_iio->prox_trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	cm36672p_iio->prox_trig->dev.parent = &cm36672p_iio->i2c_client->dev;
	cm36672p_iio->prox_trig->ops = &cm36672p_prox_trigger_ops;
	iio_trigger_set_drvdata(cm36672p_iio->prox_trig, indio_dev);
	ret = iio_trigger_register(cm36672p_iio->prox_trig);
	if (ret)
		goto error_free_trig;

	pr_err("%s is success\n", __func__);
	return 0;

error_free_trig:
	iio_trigger_free(cm36672p_iio->prox_trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	pr_info("%s is failed\n", __func__);
	return ret;
}


static void cm36672p_prox_remove_trigger(struct iio_dev *indio_dev)
{
	struct cm36672p_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36672p_data *cm36672p_iio = sdata->cdata;

	iio_trigger_unregister(cm36672p_iio->prox_trig);
	iio_trigger_free(cm36672p_iio->prox_trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void cm36672p_prox_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static int cm36672p_prox_register(struct cm36672p_data *cm36672p_iio)
{
	struct cm36672p_sensor_data *prox_p;
	int err;
	int ret = -ENODEV;

	/* iio device register - prox*/
	cm36672p_iio->indio_dev_prox  = iio_device_alloc(sizeof(*prox_p));
	if (!cm36672p_iio->indio_dev_prox ) {
		pr_err("%s, iio_dev_prox alloc failed\n", __func__);
		err = -ENOMEM;
		goto err_prox_register_failed;
	}

	prox_p = iio_priv(cm36672p_iio->indio_dev_prox );
	prox_p->cdata = cm36672p_iio;

	cm36672p_iio->indio_dev_prox->name = CHIP_ID;
	cm36672p_iio->indio_dev_prox->dev.parent = &cm36672p_iio->i2c_client->dev;
	cm36672p_iio->indio_dev_prox->info = &cm36672p_prox_info;
	cm36672p_iio->indio_dev_prox->channels = cm36672p_prox_channels;
	cm36672p_iio->indio_dev_prox->num_channels = ARRAY_SIZE(cm36672p_prox_channels);
	cm36672p_iio->indio_dev_prox->modes = INDIO_DIRECT_MODE;

	cm36672p_iio->sampling_frequency_prox = 5;
	cm36672p_iio->prox_delay = MSEC_PER_SEC / cm36672p_iio->sampling_frequency_prox;

	/* wake lock init for proximity sensor */
	wake_lock_init(&cm36672p_iio->prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	/* probe buffer */
	err = cm36672p_prox_probe_buffer(cm36672p_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox probe buffer failed\n", __func__);
		goto error_free_prox_dev;
	}

	/* probe trigger */
	err = cm36672p_prox_probe_trigger(cm36672p_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox probe trigger failed\n", __func__);
		goto error_remove_prox_buffer;
	}
	/* iio device register */
	err = iio_device_register(cm36672p_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox iio register failed\n", __func__);
		goto error_remove_prox_trigger;
	}

	/* For factory test mode, we use timer to get average proximity data. */
	/* prox_timer settings. we poll for light values using a timer. */
	hrtimer_init(&cm36672p_iio->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cm36672p_iio->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);/*2 sec*/
	cm36672p_iio->prox_timer.function = cm36672p_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	cm36672p_iio->prox_wq = create_singlethread_workqueue("cm36672p_prox_wq");
	if (!cm36672p_iio->prox_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR] %s: could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&cm36672p_iio->work_prox, cm36672_work_func_prox);

	return 0;

err_create_prox_workqueue:
	iio_device_unregister(cm36672p_iio->indio_dev_prox);
error_remove_prox_trigger:
	cm36672p_prox_remove_trigger(cm36672p_iio->indio_dev_prox);
error_remove_prox_buffer:
	cm36672p_prox_remove_buffer(cm36672p_iio->indio_dev_prox);
error_free_prox_dev:
	iio_device_free(cm36672p_iio->indio_dev_prox);
	wake_lock_destroy(&cm36672p_iio->prx_wake_lock);
err_prox_register_failed:
	return err;
}


/* device tree parsing function */
static int cm36672p_parse_dt(struct device *dev,
	struct cm36672p_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	u32 temp;

	if (!pdata) {
		SENSOR_ERR("missing pdata\n");
		return -ENOMEM;
	}

	pdata->vled_ldo = of_get_named_gpio_flags(np, "cm36672p,vled_ldo",
		0, &flags);
	if (pdata->vled_ldo < 0) {
		SENSOR_ERR("fail to get vled_ldo: means to use regulator as vLED\n");
		pdata->vled_ldo = 0;
	} else {
		ret = gpio_request(pdata->vled_ldo, "prox_vled_en");
		if (ret < 0)
			SENSOR_ERR("gpio %d request failed (%d)\n",
				pdata->vled_ldo, ret);
		else
			gpio_direction_output(pdata->vled_ldo, 0);
	}

	ret = of_property_read_u32(np, "cm36672p,vdd_always_on",
		&pdata->vdd_always_on);

	pdata->irq = of_get_named_gpio_flags(np, "cm36672p,irq_gpio", 0,
		&flags);
	if (pdata->irq < 0) {
		SENSOR_ERR("get prox_int error\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "cm36672p,default_hi_thd",
		&pdata->default_hi_thd);
	if (ret < 0) {
		SENSOR_ERR("Cannot set default_hi_thd\n");
		pdata->default_hi_thd = DEFAULT_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,default_low_thd",
		&pdata->default_low_thd);
	if (ret < 0) {
		SENSOR_ERR("Cannot set default_low_thd\n");
		pdata->default_low_thd = DEFAULT_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_hi_thd",
		&pdata->cancel_hi_thd);
	if (ret < 0) {
		SENSOR_ERR("Cannot set cancel_hi_thd\n");
		pdata->cancel_hi_thd = CANCEL_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_low_thd",
		&pdata->cancel_low_thd);
	if (ret < 0) {
		SENSOR_ERR("Cannot set cancel_low_thd\n");
		pdata->cancel_low_thd = CANCEL_LOW_THD;
	}

	/* Proximity Duty ratio Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_duty", &temp);
	if (ret < 0) {
		SENSOR_ERR("Cannot set ps_duty\n");
		ps_reg_init_setting[PS_CONF1][CMD] |= DEFAULT_CONF1;
	} else {
		temp = temp << 6;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	/* Proximity Interrupt Persistence Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_pers", &temp);
	if (ret < 0) {
		SENSOR_ERR("Cannot set ps_pers\n");
		ps_reg_init_setting[PS_CONF1][CMD] |= DEFAULT_CONF1;
	} else {
		temp = temp << 4;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	/* Proximity Integration Time Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_it", &temp);
	if (ret < 0) {
		SENSOR_ERR("Cannot set ps_it\n");
		ps_reg_init_setting[PS_CONF1][CMD] |= DEFAULT_CONF1;
	} else {
		temp = temp << 1;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	/* Proximity LED Current Register Setting */
	ret = of_property_read_u32(np, "cm36672p,led_current", &temp);
	if (ret < 0) {
		SENSOR_ERR("Cannot set led_current\n");
		ps_reg_init_setting[PS_CONF3][CMD] |= DEFAULT_CONF3;
	} else {
		temp = temp << 8;
		ps_reg_init_setting[PS_CONF3][CMD] |= temp;
	}

	/* Proximity Smart Persistence Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_smart_pers", &temp);
	if (ret < 0) {
		SENSOR_ERR("Cannot set ps_smart_pers\n");
		ps_reg_init_setting[PS_CONF3][CMD] |= DEFAULT_CONF3;
	} else {
		temp = temp << 4;
		ps_reg_init_setting[PS_CONF3][CMD] |= temp;
	}


	ret = of_property_read_u32(np, "cm36672p,default_trim",
		&pdata->default_trim);
	if (ret < 0) {
		SENSOR_ERR("Cannot set default_trim\n");
		pdata->default_trim = DEFAULT_TRIM;
	}

	ps_reg_init_setting[PS_THD_LOW][CMD] = pdata->default_low_thd;
	ps_reg_init_setting[PS_THD_HIGH][CMD] = pdata->default_hi_thd;
	ps_reg_init_setting[PS_CANCEL][CMD] = pdata->default_trim;

	SENSOR_INFO("initial CONF1 = 0x%X, CONF3 = 0x%X, vdd_alwayson_on: %d, vled_ldo: %d\n",
		ps_reg_init_setting[PS_CONF1][CMD],
		ps_reg_init_setting[PS_CONF3][CMD],
		pdata->vdd_always_on, pdata->vled_ldo);

	return 0;
}

static int cm36672p_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct cm36672p_data *data = NULL;
	struct cm36672p_platform_data *pdata = NULL;

	SENSOR_INFO("start\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c functionality check failed!\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct cm36672p_data), GFP_KERNEL);
	if (!data) {
		SENSOR_ERR("failed to alloc memory for sensor drv data\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct cm36672p_platform_data), GFP_KERNEL);
		ret = cm36672p_parse_dt(&client->dev, pdata);
		if (ret)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		SENSOR_ERR("missing pdata!\n");
		ret = -ENOMEM;
		goto err_no_pdata;
	}

	data->pdata = pdata;
	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->read_lock);
	/* wake lock init for proximity sensor */
	wake_lock_init(&data->prox_wake_lock, WAKE_LOCK_SUSPEND,
		"prox_wake_lock");

	proximity_vdd_onoff(&client->dev, ON);
	proximity_vled_onoff(&client->dev, ON);

	/* Check if the device is there or not. */
	ret = cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
	if (ret < 0) {
		SENSOR_ERR("cm36672 is not connected(%d)\n", ret);
		goto err_setup_dev;
	}

	/* setup initial registers */
	ret = setup_reg_cm36672p(data);
	if (ret < 0) {
		SENSOR_ERR("could not setup regs\n");
		goto err_setup_dev;
	}

	/* allocate proximity input_device */
	data->proximity_input_dev = input_allocate_device();
	if (!data->proximity_input_dev) {
		SENSOR_ERR("could not allocate proximity input device\n");
		goto err_input_alloc_device;
	}

	ret = cm36672p_prox_register(data);
	if (ret < 0)
		goto err_input_alloc_device;

	spin_lock_init(&data->spin_lock);
	mutex_init(&data->lock);

	sensors_create_symlink(&data->indio_dev_prox->dev.kobj,
				"proximity_sensor");

	/* setup irq */
	ret = setup_irq_cm36672p(data);
	if (ret) {
		SENSOR_ERR("could not setup irq\n");
		goto err_setup_irq;
	}

	/* For factory test mode, we use timer to get average proximity data. */
	/* prox_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);/*2 sec*/
	data->prox_timer.function = cm36672_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->prox_wq = create_singlethread_workqueue("cm36672_prox_wq");
	if (!data->prox_wq) {
		ret = -ENOMEM;
		SENSOR_ERR("could not create prox workqueue\n");
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox, cm36672_work_func_prox);

	/* set sysfs for proximity sensor */
	ret = sensors_register(data->proximity_dev,
		data, prox_sensor_attrs, MODULE_NAME);
	if (ret) {
		SENSOR_ERR("failed to register proximity dev(%d)\n", ret);
		goto err_prox_sensor_register;
	}

	proximity_vled_onoff(&client->dev, OFF);
	if (!data->pdata->vdd_always_on)
		proximity_vdd_onoff(&client->dev, OFF);

	SENSOR_INFO("success\n");
	return ret;

/* error, unwind it all */
err_prox_sensor_register:
	destroy_workqueue(data->prox_wq);
err_create_prox_workqueue:
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);
err_setup_irq:
err_input_alloc_device:
err_setup_dev:
	proximity_vled_onoff(&client->dev, OFF);
	if (data->pdata->vled_ldo)
		gpio_free(data->pdata->vled_ldo);
	proximity_vdd_onoff(&client->dev, OFF);
	wake_lock_destroy(&data->prox_wake_lock);
	mutex_destroy(&data->read_lock);
err_no_pdata:
err_parse_dt:
	kfree(data);

	SENSOR_ERR("failed (%d)\n", ret);
	return ret;
}

static int cm36672p_i2c_remove(struct i2c_client *client)
{
	SENSOR_INFO("\n");
	return 0;
}

static void cm36672p_i2c_shutdown(struct i2c_client *client)
{
	struct cm36672p_data *data = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&data->enable);

	SENSOR_INFO("pre_enable = %d\n", pre_enable);

	if (pre_enable == 1) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
		proximity_vled_onoff(&client->dev, OFF);
	}
	proximity_vdd_onoff(&client->dev, OFF);
	SENSOR_INFO("done\n");
}

static int cm36672p_suspend(struct device *dev)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	int enable;

	SENSOR_INFO("is called.\n");

	enable = atomic_read(&data->enable);

	if (enable)
		disable_irq(data->irq);

	return 0;
}

static int cm36672p_resume(struct device *dev)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	int enable;

	SENSOR_INFO("is called.\n");

	enable = atomic_read(&data->enable);

	if (enable)
		enable_irq(data->irq);

	return 0;
}

static struct of_device_id cm36672p_match_table[] = {
	{ .compatible = "cm36672p",},
	{},
};

static const struct i2c_device_id cm36672p_device_id[] = {
	{"cm36672p", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm36672p_device_id);

static const struct dev_pm_ops cm36672p_pm_ops = {
	.suspend = cm36672p_suspend,
	.resume = cm36672p_resume
};

static struct i2c_driver cm36672p_i2c_driver = {
	.driver = {
		   .name = "cm36672p",
		   .owner = THIS_MODULE,
		   .of_match_table = cm36672p_match_table,
		   .pm = &cm36672p_pm_ops
	},
	.probe = cm36672p_i2c_probe,
	.remove = cm36672p_i2c_remove,
	.shutdown = cm36672p_i2c_shutdown,
	.id_table = cm36672p_device_id,
};

static int __init cm36672p_init(void)
{
	return i2c_add_driver(&cm36672p_i2c_driver);
}

static void __exit cm36672p_exit(void)
{
	i2c_del_driver(&cm36672p_i2c_driver);
}

module_init(cm36672p_init);
module_exit(cm36672p_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Proximity Sensor device driver for CM36672P");
MODULE_LICENSE("GPL");
