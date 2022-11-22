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
#define PROXIMITY_CANCELATION
#ifdef PROXIMITY_CANCELATION
#define SUCCESS		1
#define FAIL		0
#define ERROR		-1
#define CAL_SKIP_ADC	8
#define CAL_FAIL_ADC	18
#define CANCELATION_FILE_PATH	"/efs/FactoryApp/prox_cal"
#endif

#define PROX_READ_NUM	40

 /* proximity sensor default value for register */
#define DEFAULT_HI_THD	0x0022
#define DEFAULT_LOW_THD	0x001E
#define CANCEL_HI_THD	0x0022
#define CANCEL_LOW_THD	0x001E

#ifdef CONFIG_MACH_J1_VZW
#define DEFAULT_CONF1	0x03A0
#else
#define DEFAULT_CONF1	0x0320
#endif

#define DEFAULT_CONF3	0x4200
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
* Since PS integration time and LED current would be different by HW rev or Project,
* we move the setting value to device tree. Please refer to the value below.
* PS_IT (CONF1, 0x03_L)
* 1T = 0, 1.5T = 1, 2T = 2, 2.5T = 3, 3T = 4, 3.5T = 5, 4T = 6, 8T = 7
*
* LED_I (CONF3, 0x04_H)
* 50mA = 0, 75mA = 1, 100mA = 2, 120mA = 3, 140mA = 4, 160mA = 5, 180mA = 6, 200mA = 7
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
	struct input_dev *proximity_input_dev;
	struct cm36672p_platform_data *pdata;
	struct mutex read_lock;
	struct hrtimer prox_timer;
	struct workqueue_struct *prox_wq;
	struct work_struct work_prox;
	struct device *proximity_dev;
	struct regulator *vdd;
	struct regulator *vio;
	ktime_t prox_poll_delay;
	atomic_t enable;
	int irq;
	int avg[3];
	unsigned int uProxCalResult;
#ifdef CONFIG_SENSORS_CM36672P_PMIC_LEDA
		struct regulator *leden_3p0;
#endif
};
static int proximity_regulator_onoff(struct device *dev, bool onoff);

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
	pr_err("%s, i2c transfer error ret=%d\n", __func__, err);
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
	pr_err("%s, i2c transfer error(%d)\n", __func__, err);
	return err;
}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
static int leden_gpio_onoff(struct cm36672p_data *data, bool onoff)
{
	struct cm36672p_platform_data *pdata = data->pdata;

	gpio_set_value(pdata->leden_gpio, onoff);
	if (onoff)
		msleep(20);

	return 0;
}
#elif defined(CONFIG_SENSORS_CM36672P_PMIC_LEDA)
static void cm36672p_pmic_leda_onoff(struct cm36672p_data *info, int onoff)
{
	int ret;

	pr_err("%s %s\n", __func__, (onoff) ? "on" : "off");

	if (info->leden_3p0 == NULL) {
		info->leden_3p0 = regulator_get(&info->i2c_client->dev,
			"PROX_LEDA_3.0V");
		if (IS_ERR(info->leden_3p0)) {
			pr_err("%s: regulator_get failed for PROX_LEDA_3.0V\n",
				__func__);
				return;
		}
	}

	if (onoff) {
		ret = regulator_enable(info->leden_3p0);
		if (ret)
			pr_err("%s: leda_3p3 enable failed (%d)\n",
				__func__, ret);
	} else {
		ret = regulator_disable(info->leden_3p0);
		if (ret)
			pr_err("%s: leden_3p0 disable failed (%d)\n",
				__func__, ret);
	}
	msleep(20);
	return;
}
#endif

#ifdef PROXIMITY_CANCELATION
static int proximity_open_cancelation(struct cm36672p_data *data)
{
	struct file *cancel_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cancel_filp)) {
		err = PTR_ERR(cancel_filp);
		if (err != -ENOENT)
			pr_err("%s, Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = cancel_filp->f_op->read(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("%s, Can't read the cancel data from file\n", __func__);
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

	pr_info("%s, prox_cal= 0x%x, high_thresh= 0x%x, low_thresh= 0x%x\n",
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
		cm36672p_i2c_read_word(data,
			REG_PS_DATA, &ps_data);
		mutex_unlock(&data->read_lock);

		if (ps_data < CAL_SKIP_ADC) {
			/*SKIP*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			pr_info("%s:crosstalk <= %d\n", __func__, CAL_SKIP_ADC);
			data->uProxCalResult = 2;
		} else if (ps_data < CAL_FAIL_ADC) {
			/*CANCELATION*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				ps_data + data->pdata->default_trim;
			pr_info("%s:crosstalk_offset = %u", __func__, ps_data);
			data->uProxCalResult = 1;
		} else {
			/*FAIL*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				data->pdata->default_trim;
			pr_info("%s:crosstalk > %d\n", __func__, CAL_FAIL_ADC);
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
		err = cm36672p_i2c_write_word(data, REG_PS_CANC,
			ps_reg_init_setting[PS_CANCEL][CMD]);
		if (err < 0)
			pr_err("%s, ps_canc_reg is failed. %d\n", __func__,
				err);
		err = cm36672p_i2c_write_word(data, REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			pr_err("%s: ps_high_reg is failed. %d\n", __func__,
				err);
		err = cm36672p_i2c_write_word(data, REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			pr_err("%s: ps_low_reg is failed. %d\n", __func__,
				err);
	}

	pr_info("%s: prox_cal = 0x%x, ps_high_thresh = 0x%x, ps_low_thresh = 0x%x\n",
		__func__,
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
		O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cancel_filp)) {
		pr_err("%s, Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cancel_filp);
		return err;
	}

	err = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("%s, Can't write the cancel data to file\n", __func__);
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
		pr_err("%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	err = proximity_store_cancelation(dev, do_calib);
	if (err < 0) {
		pr_err("%s, proximity_store_cancelation() failed\n",
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

	pr_info("%s, %u\n", __func__, data->uProxCalResult);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxCalResult);
}
#endif

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
		pr_err("%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pre_enable = atomic_read(&data->enable);
	pr_info("%s, new_value = %d, pre_enable = %d\n",
		__func__, new_value, pre_enable);

	if (new_value && !pre_enable) {
		int i, ret;

		proximity_regulator_onoff(dev, ON);
		atomic_set(&data->enable, ON);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data, ON);
#endif
#ifdef PROXIMITY_CANCELATION
		/* open cancelation data */
		ret = proximity_open_cancelation(data);
		if (ret < 0 && ret != -ENOENT)
			pr_err("%s, proximity_open_cancelation() failed\n",
				__func__);
#endif
		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++) {
			cm36672p_i2c_write_word(data,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);
		}

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
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data, OFF);
#endif
		atomic_set(&data->enable, OFF);
		proximity_regulator_onoff(dev, OFF);
	}
	pr_info("%s, enable = %d\n", __func__, atomic_read(&data->enable));

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
		pr_err("%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("%s, average enable = %d\n", __func__, new_value);
	if (new_value) {
		if (atomic_read(&data->enable) == OFF) {
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			leden_gpio_onoff(data, ON);
#endif
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
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			leden_gpio_onoff(data, OFF);
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
	pr_info("%s = %u,%u\n", __func__,
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
		pr_err("%s, kstrtoint failed.\n", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data, REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			pr_err("%s, cm36672_ps_high_reg is failed. %d\n",
				__func__, err);
		pr_info("%s, new high threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("%s, wrong high threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s = %u,%u\n", __func__,
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
		pr_err("%s, kstrtoint failed.", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_LOW][CMD] = thresh_value;
		err = cm36672p_i2c_write_word(data, REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			pr_err("%s, cm36672_ps_low_reg is failed. %d\n",
				__func__, err);
		pr_info("%s, new low threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("%s, wrong low threshold value(0x%x)!!\n",
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
		leden_gpio_onoff(data, ON);
#endif
		cm36672p_i2c_write_word(data, REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&data->read_lock);
	cm36672p_i2c_read_word(data, REG_PS_DATA, &ps_data);
	mutex_unlock(&data->read_lock);

	if (atomic_read(&data->enable) == OFF) {
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		leden_gpio_onoff(data, OFF);
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


#ifdef PROXIMITY_CANCELATION
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show,
	NULL);
#endif
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
#ifdef PROXIMITY_CANCELATION
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
#endif
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

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t proximity_irq_thread_fn(int irq, void *user_data)
{
	struct cm36672p_data *data = user_data;
	u8 val;
	u16 ps_data = 0;
	int enabled;
#ifdef CM36672P_DEBUG
	static int count;
	pr_info("%s\n", __func__);
#endif

	enabled = atomic_read(&data->enable);
	val = gpio_get_value(data->pdata->irq);
	cm36672p_i2c_read_word(data, REG_PS_DATA, &ps_data);
#ifdef CM36672P_DEBUG
	pr_info("%s, enabled = %d, count = %d\n", __func__, enabled, count++);
#endif

	if (enabled) {
		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev, ABS_DISTANCE,
			val);
		input_sync(data->proximity_input_dev);
	}

	wake_lock_timeout(&data->prx_wake_lock, 3 * HZ);

	pr_info("%s, val = %u, ps_data = %u (close:0, far:1)\n",
		__func__, val, ps_data);

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

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
static int setup_leden_gpio(struct cm36672p_data *data)
{
	int ret;
	struct cm36672p_platform_data *pdata = data->pdata;

	ret = gpio_request(pdata->leden_gpio, "prox_en");
	if (ret < 0)
		pr_err("%s, gpio %d request failed (%d)\n",
			__func__, pdata->leden_gpio, ret);

	ret = gpio_direction_output(pdata->leden_gpio, 1);
	if (ret)
		pr_err("%s: unable to set_direction for prox_en [%d]\n",
			__func__, pdata->leden_gpio);

	return ret;
}
#endif

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
			pr_err("%s, cm36672_ps_reg is failed. %d\n",
				__func__, ret);
			return ret;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	mutex_lock(&data->read_lock);
	ret = cm36672p_i2c_read_word(data, REG_PS_DATA, &tmp);
	mutex_unlock(&data->read_lock);
	if (ret < 0) {
		pr_err("%s, read ps_data failed\n", __func__);
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
		pr_err("%s, gpio %d request failed (%d)\n",
			__func__, pdata->irq, ret);
		return ret;
	}

	ret = gpio_direction_input(pdata->irq);
	if (ret < 0) {
		pr_err("%s, failed to set gpio %d as input (%d)\n",
			__func__, pdata->irq, ret);
		gpio_free(pdata->irq);
		return ret;
	}

	data->irq = gpio_to_irq(pdata->irq);
	ret = request_threaded_irq(data->irq, NULL, proximity_irq_thread_fn,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_int", data);
	if (ret < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
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
	struct cm36672p_data *data = dev_get_drvdata(dev);
	int ret;

	pr_info("%s %s\n", __func__, (onoff) ? "on" : "off");

	if (!data->vdd) {
		pr_info("%s VDD get regulator\n", __func__);
		data->vdd = devm_regulator_get(dev, "cm36672p,vdd");
		if (IS_ERR(data->vdd)) {
			pr_err("%s, cannot get vdd\n", __func__);
			return -ENOMEM;
		}
		regulator_set_voltage(data->vdd, 2850000, 2850000);
	}

	if (!data->vio) {
		pr_info("%s VIO get regulator\n", __func__);
		data->vio = devm_regulator_get(dev, "cm36672p,vio");
		if (IS_ERR(data->vio)) {
			pr_err("%s, cannot get vio\n", __func__);
			devm_regulator_put(data->vdd);
			return -ENOMEM;
		}
		regulator_set_voltage(data->vio, 1800000, 1800000);
	}

	if (onoff) {
		ret = regulator_enable(data->vdd);
		if (ret)
			pr_err("%s: Failed to enable vdd.\n", __func__);
		msleep(20);
		ret = regulator_enable(data->vio);
		if (ret)
			pr_err("%s: Failed to enable vio.\n", __func__);
		msleep(20);
	} else {
		ret = regulator_disable(data->vdd);
		if (ret)
			pr_err("%s: Failed to disable vdd.\n", __func__);
		msleep(20);
		ret = regulator_disable(data->vio);
		if (ret)
			pr_err("%s: Failed to disable vio.\n", __func__);
		msleep(20);
	}

	return 0;
}


#ifdef CONFIG_OF
/* device tree parsing function */
static int cm36672p_parse_dt(struct device *dev,
	struct cm36672p_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	u32 temp;

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	pdata->leden_gpio = of_get_named_gpio_flags(np, "cm36672p,leden_gpio",
		0, &flags);
	if (pdata->leden_gpio < 0) {
		pr_err("%s, get prox_leden_gpio error\n", __func__);
		return -ENODEV;
	}
#endif

	pdata->irq = of_get_named_gpio_flags(np, "cm36672p,irq_gpio", 0,
		&flags);
	if (pdata->irq < 0) {
		pr_err("%s, get prox_int error\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "cm36672p,default_hi_thd",
		&pdata->default_hi_thd);
	if (ret < 0) {
		pr_err("%s, Cannot set default_hi_thd\n", __func__);
		pdata->default_hi_thd = DEFAULT_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,default_low_thd",
		&pdata->default_low_thd);
	if (ret < 0) {
		pr_err("%s, Cannot set default_low_thd\n", __func__);
		pdata->default_low_thd = DEFAULT_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_hi_thd",
		&pdata->cancel_hi_thd);
	if (ret < 0) {
		pr_err("%s, Cannot set cancel_hi_thd\n", __func__);
		pdata->cancel_hi_thd = CANCEL_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,cancel_low_thd",
		&pdata->cancel_low_thd);
	if (ret < 0) {
		pr_err("%s, Cannot set cancel_low_thd\n", __func__);
		ps_reg_init_setting[PS_THD_LOW][CMD] = CANCEL_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36672p,ps_it", &temp);
	if (ret < 0) {
		pr_err("%s, Cannot set ps_it\n", __func__);
		ps_reg_init_setting[PS_CONF1][CMD] = DEFAULT_CONF1;
	} else {
		temp = temp << 1;
		ps_reg_init_setting[PS_CONF1][CMD] |= temp;
	}

	ret = of_property_read_u32(np, "cm36672p,led_current", &temp);
	if (ret < 0) {
		pr_err("%s, Cannot set led_current\n", __func__);
		ps_reg_init_setting[PS_CONF3][CMD] = DEFAULT_CONF3;
	} else {
		temp = temp << 8;
		ps_reg_init_setting[PS_CONF3][CMD] |= temp;
	}

	ret = of_property_read_u32(np, "cm36672p,default_trim",
		&pdata->default_trim);
	if (ret < 0) {
		pr_err("%s, Cannot set default_trim\n", __func__);
		pdata->default_trim = DEFAULT_TRIM;
	}
	ps_reg_init_setting[PS_THD_LOW][CMD] = pdata->default_low_thd;
	ps_reg_init_setting[PS_THD_HIGH][CMD] = pdata->default_hi_thd;
	ps_reg_init_setting[PS_CANCEL][CMD] = pdata->default_trim;

	return 0;
}
#else
static int cm36672p_parse_dt(struct device *dev, struct cm36672p_platform_data)
{
	return -ENODEV;
}
#endif

static int cm36672p_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct cm36672p_data *data = NULL;
	struct cm36672p_platform_data *pdata = NULL;

	pr_info("%s, Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s, i2c functionality check failed!\n",
			__func__);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct cm36672p_data), GFP_KERNEL);
	if (!data) {
		pr_err("%s, failed to alloc memory for RGB sensor module data\n",
			__func__);
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
		sizeof(struct cm36672p_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto exit;
		}
		ret = cm36672p_parse_dt(&client->dev, pdata);
		if (ret)
			goto exit;
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s, missing pdata!\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}


	data->pdata = pdata;
	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->read_lock);
	/* wake lock init for proximity sensor */
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");
	proximity_regulator_onoff(&client->dev, ON);

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	/* setup led_en_gpio */
	ret = setup_leden_gpio(data);
	if (ret) {
		pr_err("%s, could not setup leden_gpio\n", __func__);
		goto err_setup_leden_gpio;
	}
	leden_gpio_onoff(data, ON);
#elif defined(CONFIG_SENSORS_CM36672P_PMIC_LEDA)
	cm36672p_pmic_leda_onoff(data, ON);
#endif

	/* Check if the device is there or not. */
	ret = cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);
	if (ret < 0) {
		pr_err("%s, cm36672 is not connected.(%d)\n", __func__,
			ret);
		goto err_setup_dev;
	}

	/* setup initial registers */
	ret = setup_reg_cm36672p(data);
	if (ret < 0) {
		pr_err("%s, could not setup regs\n", __func__);
		goto err_setup_dev;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(data, OFF);
#endif

	/* allocate proximity input_device */
	data->proximity_input_dev = input_allocate_device();
	if (!data->proximity_input_dev) {
		pr_err("%s, could not allocate proximity input device\n",
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
		pr_err("%s, could not register input device\n",
			__func__);
		goto err_input_register_device;
	}

	ret = sensors_create_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);
	if (ret < 0) {
		pr_err("%s, create_symlink error\n", __func__);
		goto err_sensors_create_symlink_prox;
	}

	ret = sysfs_create_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	if (ret) {
		pr_err("%s, could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* setup irq */
	ret = setup_irq_cm36672p(data);
	if (ret) {
		pr_err("%s, could not setup irq\n", __func__);
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
		pr_err("%s, could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox, cm36672_work_func_prox);

	/* set sysfs for proximity sensor */
	ret = sensors_register(data->proximity_dev,
		data, prox_sensor_attrs,
			"proximity_sensor");
	if (ret) {
		pr_err("%s, cound not register proximity sensor device(%d).\n",
			__func__, ret);
		goto prox_sensor_register_failed;
	}

	proximity_regulator_onoff(&client->dev, OFF);
	pr_info("%s is success.\n", __func__);
	return ret;

/* error, unwind it all */
prox_sensor_register_failed:
	destroy_workqueue(data->prox_wq);
err_create_prox_workqueue:
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);
err_setup_irq:
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
err_setup_dev:
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(data, OFF);
	gpio_free(data->pdata->leden_gpio);
err_setup_leden_gpio:
#endif
	wake_lock_destroy(&data->prx_wake_lock);
	mutex_destroy(&data->read_lock);
	proximity_regulator_onoff(&client->dev, OFF);
exit:
	kfree(data);

	pr_err("%s is FAILED (%d)\n", __func__, ret);
	return ret;
}

static int cm36672p_i2c_remove(struct i2c_client *client)
{
	struct cm36672p_data *data = i2c_get_clientdata(client);

	/* free irq */
	if (atomic_read(&data->enable)) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
	}
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);

	/* device off */
	if (atomic_read(&data->enable))
		cm36672p_i2c_write_word(data, REG_PS_CONF1, 0x0001);

	if (data->vdd) {
		pr_info("%s VDD put\n", __func__);
		devm_regulator_put(data->vdd);
	}

	if (data->vio) {
		pr_info("%s VIO put\n", __func__);
		devm_regulator_put(data->vio);
	}
	/* destroy workqueue */
	destroy_workqueue(data->prox_wq);

	/* sysfs destroy */
	sensors_unregister(data->proximity_dev, prox_sensor_attrs);
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);

	/* input device destroy */
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	input_unregister_device(data->proximity_input_dev);

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	leden_gpio_onoff(data, OFF);
	gpio_free(data->pdata->leden_gpio);
#endif

	/* lock destroy */
	mutex_destroy(&data->read_lock);
	wake_lock_destroy(&data->prx_wake_lock);

	kfree(data);

	return 0;
}

static int cm36672p_suspend(struct device *dev)
{
	pr_info("%s is called.\n", __func__);
	return 0;
}

static int cm36672p_resume(struct device *dev)
{
	pr_info("%s is called.\n", __func__);
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
