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
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>

#include <linux/sensor/sensors_core.h>
#include <linux/sensor/cm36672p.h>

/* For debugging */
#undef CM36672P_DEBUG

#define VENDOR		"CAPELLA"
#define CHIP_ID		"CM36672P"

#define I2C_M_WR	0 /* for i2c Write */
#define I2c_M_RD	1 /* for i2c Read */

/* register addresses */
/* Proximity sensor */
#define REG_PS_CONF1	0x03
#define REG_PS_CONF3	0x04
#define REG_PS_CANC	0x05
#define REG_PS_THD_LOW	0x06
#define REG_PS_THD_HIGH	0x07
#define REG_PS_DATA	0x08

/* Intelligent Cancelation*/
#define CAL_SKIP_ADC	8
#define CAL_FAIL_ADC	18
#define CANCELATION_FILE_PATH	"/efs/FactoryApp/prox_cal"

#define PROX_READ_NUM	40

 /* proximity sensor default value for register */
#define DEFAULT_HI_THD	0x0022
#define DEFAULT_LOW_THD	0x001E
#define CANCEL_HI_THD	0x0022
#define CANCEL_LOW_THD	0x001E

#define DEFAULT_CONF1	0x0320
#define DEFAULT_CONF3	0x4000
#define DEFAULT_TRIM	0x0000

enum {
	REG_ADDR = 0,
	CMD,
};

enum {
	OFF = 0,
	ON,
};

enum {
	PS_CONF1 = 0,
	PS_CONF3,
	PS_THD_LOW,
	PS_THD_HIGH,
	PS_CANCEL,
	PS_REG_NUM,
};

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
	{REG_PS_CONF1, DEFAULT_CONF1},
	{REG_PS_CONF3, DEFAULT_CONF3},
	{REG_PS_THD_LOW, DEFAULT_LOW_THD},
	{REG_PS_THD_HIGH, DEFAULT_HI_THD},
	{REG_PS_CANC, DEFAULT_TRIM},
};

/* driver data */
struct cm36672p_data {
	struct i2c_client *i2c_client;
	struct wake_lock prx_wake_lock;
#if !defined(CONFIG_SENSORS_IIO)
	struct input_dev *proximity_input_dev;
#endif
	struct cm36672p_platform_data *pdata;
	struct mutex read_lock;
	struct hrtimer prox_timer;
	struct workqueue_struct *prox_wq;
	struct work_struct work_prox;
	struct device *proximity_dev;
	ktime_t prox_poll_delay;
	atomic_t enable;
	int irq;
	int avg[3];
	unsigned int uProxCalResult;
	u8 val;
};

#if defined(CONFIG_SENSORS_IIO)
static const struct iio_chan_spec cm36672p_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.channel = -1,
		.scan_index = 0,
		.scan_type = IIO_ST('s', 8, 8, 0)
	}
};
#endif

int cm36672p_i2c_read_word(struct i2c_client *client, u8 command, u16 *val)
{
	int err = 0;
	int retry = 3;
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
	pr_err("[SENSOR] %s, i2c transfer error ret=%d\n", __func__, err);
	return err;
}

int cm36672p_i2c_write_word(struct i2c_client *client, u8 command, u16 val)
{
	int err = 0;
	int retry = 3;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		err = i2c_smbus_write_word_data(client, command, val);
		if (err >= 0)
			return 0;
	}
	pr_err("[SENSOR] %s, i2c transfer error(%d)\n", __func__, err);
	return err;
}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
static int leden_gpio_onoff(struct cm36672p_platform_data *pdata, bool onoff)
{
	gpio_set_value(pdata->leden_gpio, onoff);
	if (onoff)
		msleep(20);

	return 0;
}
#endif

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
			pr_err("[SENSOR] %s, Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = cancel_filp->f_op->read(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("[SENSOR] %s, Can't read the cancel data from file\n",
			__func__);
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

	pr_info("[SENSOR] %s: cancel= 0x%x, high_thd= 0x%x, low_thd= 0x%x\n",
		__func__,
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
		cm36672p_i2c_read_word(data->i2c_client,
			REG_PS_DATA, &ps_data);
		mutex_unlock(&data->read_lock);

		if (ps_data < CAL_SKIP_ADC) {
			/*SKIP*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			pr_info("[SENSOR] %s:crosstalk <= %d\n", __func__,
				CAL_SKIP_ADC);
			data->uProxCalResult = 2;
		} else if (ps_data < CAL_FAIL_ADC) {
			/*CANCELATION*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				ps_data + data->pdata->default_trim;
			pr_info("[SENSOR] %s:crosstalk_offset = %u", __func__,
				ps_data);
			data->uProxCalResult = 1;
		} else {
			/*FAIL*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			pr_info("[SENSOR] %s:crosstalk > %d\n", __func__,
				CAL_FAIL_ADC);
			data->uProxCalResult = 0;
		}

		if (data->uProxCalResult == 1) {
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
		ps_reg_init_setting[PS_CANCEL][CMD] =
			data->pdata->default_trim;
		ps_reg_init_setting[PS_THD_HIGH][CMD] =
			data->pdata->default_hi_thd ?
			data->pdata->default_hi_thd :
			DEFAULT_HI_THD;
		ps_reg_init_setting[PS_THD_LOW][CMD] =
			data->pdata->default_low_thd ?
			data->pdata->default_low_thd :
			DEFAULT_LOW_THD;
	}

	if ((data->uProxCalResult == 1) || !do_calib) {
		err = cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_CANC,
			ps_reg_init_setting[PS_CANCEL][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s, ps_canc_reg is failed. %d\n",
				__func__, err);
		err = cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s: ps_high_reg is failed. %d\n",
				__func__, err);
		err = cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s: ps_low_reg is failed. %d\n",
				__func__, err);
	}

	pr_info("[SENSOR] %s: cancel= 0x%x, high_thd= 0x%x, low_thd= 0x%x\n",
		__func__,
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
		O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cancel_filp)) {
		pr_err("[SENSOR] %s, Can't open cancelation file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cancel_filp);
		return err;
	}

	err = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("[SENSOR] %s, Can't write the cancel data to file\n",
			__func__);
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
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	err = proximity_store_cancelation(dev, do_calib);
	if (err < 0) {
		pr_err("[SENSOR] %s, proximity_store_cancelation() failed\n",
			__func__);
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

	pr_info("[SENSOR] %s, %u\n", __func__, data->uProxCalResult);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxCalResult);
}

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SENSORS_IIO)
	struct cm36672p_data *data = iio_priv(dev_get_drvdata(dev));
#else
	struct cm36672p_data *data = dev_get_drvdata(dev);
#endif

	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&data->enable));
}

static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SENSORS_IIO)
	struct cm36672p_data *data = iio_priv(dev_get_drvdata(dev));
#else
	struct cm36672p_data *data = dev_get_drvdata(dev);
#endif
	bool new_value;
	int pre_enable;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pre_enable = atomic_read(&data->enable);
	pr_info("[SENSOR] %s, new_value = %d, pre_enable = %d\n",
		__func__, new_value, pre_enable);

	if (new_value && !pre_enable) {
		int i, ret;

		atomic_set(&data->enable, ON);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data->pdata, ON);
#endif
		/* open cancelation data */
		ret = proximity_open_cancelation(data);
		if (ret < 0 && ret != -ENOENT)
			pr_err("[SENSOR] %s, open_cancelation() failed\n",
				__func__);

		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++) {
			cm36672p_i2c_write_word(data->i2c_client,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);
		}

#if !defined(CONFIG_SENSORS_IIO)
		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(data->proximity_input_dev);
#endif

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else if (!new_value && pre_enable) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
		/* disable settings */
		cm36672p_i2c_write_word(data->i2c_client, REG_PS_CONF1,
			0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data->pdata, OFF);
#endif
		atomic_set(&data->enable, OFF);
	}

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

/* proximity sysfs */
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
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[SENSOR] %s, average enable = %d\n",
		__func__, new_value);
	if (new_value) {
		if (atomic_read(&data->enable) == OFF) {
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			leden_gpio_onoff(data->pdata, ON);
#endif
			cm36672p_i2c_write_word(data->i2c_client,
				REG_PS_CONF1,
				ps_reg_init_setting[PS_CONF1][CMD]);
		}
		hrtimer_start(&data->prox_timer, data->prox_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&data->prox_timer);
		cancel_work_sync(&data->work_prox);
		if (atomic_read(&data->enable) == OFF) {
			cm36672p_i2c_write_word(data->i2c_client,
				REG_PS_CONF1, 0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			leden_gpio_onoff(data->pdata, OFF);
#endif
		}
	}

	return size;
}

static ssize_t proximity_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
		data->pdata->default_trim);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[SENSOR] %s = %u,%u\n", __func__,
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
		pr_err("[SENSOR] %s, kstrtoint failed.\n", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s, ps_high_reg is failed. %d\n",
				__func__, err);
		pr_info("[SENSOR] %s, new high threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("[SENSOR] %s, wrong high threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[SENSOR] %s = %u,%u\n", __func__,
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
		pr_err("[SENSOR] %s, kstrtoint failed.", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_LOW][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s, ps_low_reg is failed. %d\n",
				__func__, err);
		pr_info("[SENSOR] %s, new low threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("[SENSOR] %s, wrong low threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36672p_data *data = dev_get_drvdata(dev);
	u16 ps_data;

	if (atomic_read(&data->enable) == OFF) {
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data->pdata, ON);
#endif
		cm36672p_i2c_write_word(data->i2c_client,
			REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&data->read_lock);
	cm36672p_i2c_read_word(data->i2c_client, REG_PS_DATA, &ps_data);
	mutex_unlock(&data->read_lock);

	if (atomic_read(&data->enable) == OFF) {
		cm36672p_i2c_write_word(data->i2c_client, REG_PS_CONF1,
			0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data->pdata, OFF);
#endif
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", ps_data);
}

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


static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show,
	NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(prox_trim, S_IRUSR | S_IRGRP,
	proximity_trim_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(vendor, S_IRUSR | S_IRGRP, cm36672p_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUSR | S_IRGRP, cm36672p_name_show, NULL);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_avg,
	&dev_attr_prox_trim,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_state,
	&dev_attr_raw_data,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};

#if defined(CONFIG_SENSORS_IIO)
static int cm36672p_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan,
	int *val, int *val2, long mask)
{
	struct cm36672p_data *data = iio_priv(indio_dev);
	int ret = -EINVAL;

	switch (chan->type) {
	case IIO_PROXIMITY:
		*val = data->val;
		break;
	default:
		pr_err("[SENSOR] %s, invalied channel\n", __func__);
		return ret;
	}

	return IIO_VAL_INT;
}

static const struct iio_info cm36672p_info = {
	.attrs = &proximity_attribute_group,
	.driver_module = THIS_MODULE,
	.read_raw = cm36672p_read_raw,
};
#endif

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t proximity_irq_thread_fn(int irq, void *user_data)
{
	struct cm36672p_data *data = user_data;
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev = iio_priv_to_dev(data);
	u8 *buf;
#endif
	u16 ps_data = 0;
	int enabled;
#ifdef CM36672P_DEBUG
	static int count;
	pr_info("[SENSOR] %s\n", __func__);
#endif

	enabled = atomic_read(&data->enable);
	data->val = gpio_get_value(data->pdata->irq);
	cm36672p_i2c_read_word(data->i2c_client, REG_PS_DATA, &ps_data);
#ifdef CM36672P_DEBUG
	pr_info("[SENSOR] %s, enabled = %d, count = %d\n",
		__func__, enabled, count++);
#endif

	if (enabled) {
#if defined(CONFIG_SENSORS_IIO)
		memcpy(&buf, &data->val, sizeof(data->val));

		iio_push_to_buffers(indio_dev, &buf);
#else
		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev, ABS_DISTANCE,
			data->val);
		input_sync(data->proximity_input_dev);
#endif
	}

	wake_lock_timeout(&data->prx_wake_lock, 3 * HZ);

	pr_info("[SENSOR] %s, val = %u, ps_data = %u (close:0, far:1)\n",
		__func__, data->val, ps_data);

	return IRQ_HANDLED;
}

static void proximity_get_avg_val(struct cm36672p_data *data)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u16 ps_data = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		msleep(40);
		cm36672p_i2c_read_word(data->i2c_client, REG_PS_DATA,
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

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
static int setup_leden_gpio(struct cm36672p_platform_data *pdata)
{
	int ret;

	ret = gpio_request(pdata->leden_gpio, "prox_en");
	if (ret < 0)
		pr_err("[SENSOR] %s, gpio %d request failed (%d)\n",
			__func__, pdata->leden_gpio, ret);

	ret = gpio_direction_output(pdata->leden_gpio, 1);
	if (ret)
		pr_err("[SENSOR] %s: fail to set_direction for %d\n",
			__func__, pdata->leden_gpio);

	return ret;
}
#endif

static int setup_reg_cm36672p(struct i2c_client *client)
{
	int ret, i;
	u16 tmp;

	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		ret = cm36672p_i2c_write_word(client,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
		if (ret < 0) {
			pr_err("[SENSOR] %s, cm36672_ps_reg is failed. %d\n",
				__func__, ret);
			return ret;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	ret = cm36672p_i2c_read_word(client, REG_PS_DATA, &tmp);
	if (ret < 0) {
		pr_err("[SENSOR] %s, read ps_data failed\n", __func__);
		ret = -EIO;
	}

	/* turn off */
	cm36672p_i2c_write_word(client, REG_PS_CONF1, 0x0001);
	cm36672p_i2c_write_word(client, REG_PS_CONF3, 0x0000);

	return ret;
}


static int setup_irq_cm36672p(struct cm36672p_data *data)
{
	int ret;
	struct cm36672p_platform_data *pdata = data->pdata;

	ret = gpio_request(pdata->irq, "gpio_proximity_out");
	if (ret < 0) {
		pr_err("[SENSOR] %s, gpio %d request failed (%d)\n",
			__func__, pdata->irq, ret);
		return ret;
	}

	ret = gpio_direction_input(pdata->irq);
	if (ret < 0) {
		pr_err("[SENSOR] %s, failed to set gpio %d as input (%d)\n",
			__func__, pdata->irq, ret);
		gpio_free(pdata->irq);
		return ret;
	}

	data->irq = gpio_to_irq(pdata->irq);
	ret = request_threaded_irq(data->irq, NULL, proximity_irq_thread_fn,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_int", data);
	if (ret < 0) {
		pr_err("[SENSOR] %s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, data->irq, pdata->irq, ret);
		gpio_free(pdata->irq);
		return ret;
	}

	/* start with interrupts disabled */
	disable_irq(data->irq);

	return ret;
}


static int proximity_regulator_onoff(struct device *dev, bool onoff)
{
	struct regulator *vdd;
	int ret;

	pr_info("[SENSOR] %s %s\n", __func__, (onoff) ? "on" : "off");

	vdd = devm_regulator_get(dev, "cm36672p,vdd");
	if (IS_ERR(vdd)) {
		pr_err("[SENSOR] %s, cannot get vdd\n", __func__);
		return -ENOMEM;
	}

	regulator_set_voltage(vdd, 2850000, 3300000);

	if (onoff) {
		ret = regulator_enable(vdd);
		msleep(20);
	} else {
		ret = regulator_disable(vdd);
		msleep(20);
	}
	devm_regulator_put(vdd);
	msleep(20);

	return 0;
}


/* device tree parsing function */
static int cm36672p_parse_dt(struct device *dev,
	struct cm36672p_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	u32 temp;

	if (pdata == NULL)
		return -ENODEV;

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	pdata->leden_gpio = of_get_named_gpio_flags(np, "cm36672p,leden_gpio",
		0, &flags);
	if (pdata->leden_gpio < 0) {
		pr_err("[SENSOR] %s, get prox_leden_gpio error\n", __func__);
		return -ENODEV;
	}
#endif

	pdata->irq = of_get_named_gpio_flags(np, "cm36672p,irq_gpio", 0,
		&flags);
	if (pdata->irq < 0) {
		pr_err("[SENSOR] %s, get prox_int error\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "cm36672p,default_hi_thd",
		&pdata->default_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set default_hi_thd\n", __func__);
		pdata->default_hi_thd = DEFAULT_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,default_low_thd",
		&pdata->default_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set default_low_thd\n", __func__);
		pdata->default_low_thd = DEFAULT_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_hi_thd",
		&pdata->cancel_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set cancel_hi_thd\n", __func__);
		pdata->cancel_hi_thd = CANCEL_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_low_thd",
		&pdata->cancel_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set cancel_low_thd\n", __func__);
		pdata->cancel_low_thd = CANCEL_LOW_THD;
	}

	/* Proximity Duty ratio Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_duty", &temp);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set ps_duty\n", __func__);
		ps_reg_init_setting[PS_CONF1][CMD] = DEFAULT_CONF1;
	} else {
		temp = temp << 6;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	/* Proximity Integration Time Register Setting */
	ret = of_property_read_u32(np, "cm36672p,ps_it", &temp);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set ps_it\n", __func__);
		ps_reg_init_setting[PS_CONF1][CMD] |= DEFAULT_CONF1;
	} else {
		temp = temp << 1;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	/* Proximity LED Current Register Setting */
	ret = of_property_read_u32(np, "cm36672p,led_current", &temp);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set led_current\n", __func__);
		ps_reg_init_setting[PS_CONF3][CMD] = DEFAULT_CONF3;
	} else {
		temp = temp << 8;
		ps_reg_init_setting[PS_CONF3][CMD] |= temp;
	}

	ret = of_property_read_u32(np, "cm36672p,default_trim",
		&pdata->default_trim);
	if (ret < 0) {
		pr_err("[SENSOR] %s, Cannot set default_trim\n", __func__);
		pdata->default_trim = DEFAULT_TRIM;
	}

	ps_reg_init_setting[PS_THD_LOW][CMD] = pdata->default_low_thd;
	ps_reg_init_setting[PS_THD_HIGH][CMD] = pdata->default_hi_thd;
	ps_reg_init_setting[PS_CANCEL][CMD] = pdata->default_trim;

	pr_info("[SENSOR] %s, initial CONF1 = 0x%X, CONF3 = 0x%X\n", __func__,
		ps_reg_init_setting[PS_CONF1][CMD],
		ps_reg_init_setting[PS_CONF3][CMD]);

	return 0;
}

static int cm36672p_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct cm36672p_data *data = NULL;
	struct cm36672p_platform_data *pdata = NULL;
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev;
#endif

	pr_info("[SENSOR] %s, Probe Start!\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR] %s, i2c functionality check failed!\n",
			__func__);
		ret = -ENODEV;
		goto exit;
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
		pr_err("[SENSOR] %s, missing pdata!\n", __func__);
		ret = -ENOMEM;
		goto err_no_pdata;
	}

	proximity_regulator_onoff(&client->dev, ON);

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	/* setup led_en_gpio */
	ret = setup_leden_gpio(pdata);
	if (ret) {
		pr_err("[SENSOR] %s, could not setup leden_gpio\n", __func__);
		goto err_setup_leden_gpio;
	}
	leden_gpio_onoff(pdata, ON);
#endif

	/* Check if the device is there or not. */
	ret = cm36672p_i2c_write_word(client, REG_PS_CONF1, 0x0001);
	if (ret < 0) {
		pr_err("[SENSOR] %s, cm36672 is not connected.(%d)\n", __func__,
			ret);
		goto err_setup_dev;
	}

	/* setup initial registers */
	ret = setup_reg_cm36672p(client);
	if (ret < 0) {
		pr_err("[SENSOR] %s, could not setup regs\n", __func__);
		goto err_setup_dev;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(pdata, OFF);
#endif

#if defined(CONFIG_SENSORS_IIO)
	/* Configure IIO device */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev) {
		pr_err("[SENSOR] %s, iio_device_alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_mem_alloc;
	}

	data = iio_priv(indio_dev);
	dev_set_name(&indio_dev->dev, "proximity_sensor");
	i2c_set_clientdata(client, indio_dev);

	indio_dev->name = client->name;
	indio_dev->channels = cm36672p_channels;
	indio_dev->num_channels = ARRAY_SIZE(cm36672p_channels);
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &cm36672p_info;

	ret = sensors_iio_configure_buffer(indio_dev);
	if (ret) {
		pr_err("[SENSOR] %s, configure ring buffer failed\n",
			__func__);
		goto err_iio_configure_buffer_failed;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		pr_err("[SENSOR] %s: iio_device_register failed(%d)\n",
			__func__, ret);
		goto err_iio_register_failed;
	}

	data->i2c_client = client;
	data->pdata = pdata;

#else	/* !defined(CONFIG_SENSORS_IIO)*/

	/* Allocate memory for driver data */
	data = kzalloc(sizeof(struct cm36672p_data), GFP_KERNEL);
	if (!data) {
		pr_err("[SENSOR] %s, device_alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_mem_alloc;
	}

	i2c_set_clientdata(client, data);
	data->i2c_client = client;
	data->pdata = pdata;

	/* allocate proximity input_device */
	data->proximity_input_dev = input_allocate_device();
	if (!data->proximity_input_dev) {
		pr_err("[SENSOR] %s, could not allocate proximity input device\n",
			__func__);
		goto err_input_alloc_device;
	}

	input_set_drvdata(data->proximity_input_dev, data);
	data->proximity_input_dev->name = "proximity_sensor";
	input_set_capability(data->proximity_input_dev, EV_ABS,
		ABS_DISTANCE);
	input_set_abs_params(data->proximity_input_dev, ABS_DISTANCE, 0, 1,
		0, 0);

	ret = input_register_device(data->proximity_input_dev);
	if (ret < 0) {
		pr_err("[SENSOR] %s, could not register input device\n",
			__func__);
		goto err_input_register_device;
	}

	ret = sensors_create_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);
	if (ret < 0) {
		pr_err("[SENSOR] %s, create_symlink error\n", __func__);
		goto err_sensors_create_symlink_prox;
	}

	ret = sysfs_create_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	if (ret) {
		pr_err("[SENSOR] %s, could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}
#endif

	mutex_init(&data->read_lock);
	/* wake lock init for proximity sensor */
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");

	/* setup irq */
	ret = setup_irq_cm36672p(data);
	if (ret) {
		pr_err("[SENSOR] %s, could not setup irq\n", __func__);
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
		pr_err("[SENSOR] %s, could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox, cm36672_work_func_prox);

	/* set sysfs for proximity sensor */
	ret = sensors_register(data->proximity_dev,
		data, prox_sensor_attrs, "proximity_sensor");
	if (ret) {
		pr_err("[SENSOR] %s, failed to register proximity dev(%d).\n",
			__func__, ret);
		goto err_prox_sensor_register;
	}

	pr_info("[SENSOR] %s is success.\n", __func__);
	return ret;

/* error, unwind it all */
err_prox_sensor_register:
	destroy_workqueue(data->prox_wq);
err_create_prox_workqueue:
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);
err_setup_irq:
	wake_lock_destroy(&data->prx_wake_lock);
	mutex_destroy(&data->read_lock);
#if defined(CONFIG_SENSORS_IIO)
	iio_device_unregister(indio_dev);
err_iio_register_failed:
	sensors_iio_unconfigure_buffer(indio_dev);
err_iio_configure_buffer_failed:
#else
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);
err_sensors_create_symlink_prox:
	input_unregister_device(data->proximity_input_dev);
err_input_register_device:
	input_free_device(data->proximity_input_dev);
err_input_alloc_device:
	kfree(data);
#endif
exit_mem_alloc:
err_setup_dev:
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(pdata, OFF);
	gpio_free(pdata->leden_gpio);
err_setup_leden_gpio:
#endif
	proximity_regulator_onoff(&client->dev, OFF);
err_no_pdata:
err_parse_dt:
exit:
	pr_err("[SENSOR] %s is FAILED (%d)\n", __func__, ret);
	return ret;
}

static int cm36672p_i2c_remove(struct i2c_client *client)
{
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct cm36672p_data *data = iio_priv(indio_dev);
#else
	struct cm36672p_data *data = i2c_get_clientdata(client);
#endif

	/* free irq */
	if (atomic_read(&data->enable)) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
	}
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);

	/* device off */
	if (atomic_read(&data->enable))
		cm36672p_i2c_write_word(data->i2c_client, REG_PS_CONF1,
			0x0001);

	/* destroy workqueue */
	destroy_workqueue(data->prox_wq);

	/* lock destroy */
	mutex_destroy(&data->read_lock);
	wake_lock_destroy(&data->prx_wake_lock);

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(data->pdata, OFF);
	gpio_free(data->pdata->leden_gpio);
#endif

	/* sysfs destroy */
	sensors_unregister(data->proximity_dev, prox_sensor_attrs);

#if defined(CONFIG_SENSORS_IIO)
	iio_device_unregister(indio_dev);
	sensors_iio_unconfigure_buffer(indio_dev);
#else
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);

	/* input device destroy */
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	input_unregister_device(data->proximity_input_dev);
	kfree(data);
#endif

	proximity_regulator_onoff(&client->dev, OFF);

	return 0;
}

static int cm36672p_suspend(struct device *dev)
{
	pr_info("[SENSOR] %s is called.\n", __func__);
	return 0;
}

static int cm36672p_resume(struct device *dev)
{
	pr_info("[SENSOR] %s is called.\n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cm36672p_match_table[] = {
	{ .compatible = "cm36672p",},
	{},
};
#else
#define cm36672p_match_table NULL
#endif


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
