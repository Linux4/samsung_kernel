/* driver/sensor/cm36686.c
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

#include "sensors_core.h"
#include <linux/sensor/cm36686.h>

/* For debugging */
#undef	cm36686_DEBUG

#define VENDOR		"CAPELLA"
#define CHIP_ID		"CM36686"

#define I2C_M_WR	0 /* for i2c Write */

/* register addresses */
/* Ambient light sensor */
#define REG_CS_CONF1	0x00
#define REG_ALS_DATA	0x09
#define REG_WHITE_DATA	0x0A

/* Proximity sensor */
#define REG_PS_CONF1		0x03
#define REG_PS_CONF3		0x04
#define REG_PS_CANC		0x05
#define REG_PS_THD_LOW		0x06
#define REG_PS_THD_HIGH		0x07
#define REG_PS_DATA		0x08

#define ALS_REG_NUM		2
#define PS_REG_NUM		5

#define MSK_L(x)		(x & 0xff)
#define MSK_H(x)		((x & 0xff00) >> 8)

/* Intelligent Cancelation*/
#define CANCELATION_FILE_PATH	"/efs/FactoryApp/prox_cal"
#define CAL_SKIP_ADC	5
#define CAL_FAIL_ADC	15

#define PROX_READ_NUM		40

 /* proximity sensor threshold */
#define DEFUALT_HI_THD		0x0022
#define DEFUALT_LOW_THD		0x001E
#define CANCEL_HI_THD		0x0022
#define CANCEL_LOW_THD		0x001E

 /*lightsnesor log time 6SEC 200mec X 30*/
#define LIGHT_LOG_TIME		30
#define LIGHT_ADD_STARTTIME		300000000

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

/* register settings */
static u16 als_reg_setting[ALS_REG_NUM][2] = {
	{REG_CS_CONF1, 0x0000},	/* enable */
	{REG_CS_CONF1, 0x0001},	/* disable */
};

/* Change threshold value on the midas-sensor.c */
enum {
	PS_CONF1 = 0,
	PS_CONF3,
	PS_THD_LOW,
	PS_THD_HIGH,
	PS_CANCEL,
};

enum {
	REG_ADDR = 0,
	CMD,
};

static u16 ps_reg_init_setting[PS_REG_NUM][2] = {
	{REG_PS_CONF1, 0x0304},	/* REG_PS_CONF1 */
	{REG_PS_CONF3, 0x4200},	/* REG_PS_CONF3 */
	{REG_PS_THD_LOW, DEFUALT_LOW_THD},	/* REG_PS_THD_LOW */
	{REG_PS_THD_HIGH, DEFUALT_HI_THD},	/* REG_PS_THD_HIGH */
	{REG_PS_CANC, 0x0000},	/* REG_PS_CANC */
};

/* driver data */
struct cm36686_data {
	struct i2c_client *i2c_client;
	struct wake_lock prx_wake_lock;
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct cm36686_platform_data *pdata;
	struct mutex power_lock;
	struct mutex read_lock;
	struct hrtimer light_timer;
	struct hrtimer prox_timer;
	struct workqueue_struct *light_wq;
	struct workqueue_struct *prox_wq;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct device *proximity_dev;
	struct device *light_dev;
	ktime_t light_poll_delay;
	ktime_t prox_poll_delay;
	int irq;
	u8 power_state;
	int avg[3];
	u16 als_data;
	u16 white_data;
	int count_log_time;
	unsigned int uProxCalResult;
	u8 val;
};

int cm36686_i2c_read_word(struct cm36686_data *data, u8 command, u16 *val)
{
	struct i2c_client *client = data->i2c_client;
	struct i2c_msg msg[2];
	unsigned char tmp[2] = {0,};
	u16 value = 0;
	int err = 0;
	int retry = 3;

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

int cm36686_i2c_write_word(struct i2c_client *client, u8 command, u16 val)
{
	int retry = 3;
	int err = 0;

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
static int cm36686_setup_leden_gpio(struct cm36686_platform_data *pdata)
{
	int ret;

	ret = gpio_request(pdata->leden_gpio, "prox_en");
	if (ret < 0) {
		pr_err("[SENSOR] %s: gpio %d request failed (%d)\n",
			__func__, pdata->leden_gpio, ret);
		goto exit;
	}

	ret = gpio_direction_output(pdata->leden_gpio, 1);
	if (ret)
		pr_err("[SENSOR] %s: unable to setup for prox_en [%d]\n",
			__func__, pdata->leden_gpio);
exit:
	return ret;
}

static int cm36686_leden_gpio_onoff(struct cm36686_platform_data *pdata,
	bool onoff)
{
	pr_info("[SENSOR] %s, onoff = %d\n", __func__, onoff);
	gpio_set_value(pdata->leden_gpio, onoff);
	if (onoff)
		msleep(20);

	return 0;
}
#endif

static void cm36686_light_enable(struct cm36686_data *data)
{
	/* enable setting */
	cm36686_i2c_write_word(data->i2c_client, REG_CS_CONF1,
		als_reg_setting[0][1]);
	hrtimer_start(&data->light_timer, ns_to_ktime(200 * NSEC_PER_MSEC),
		HRTIMER_MODE_REL);
}

static void cm36686_light_disable(struct cm36686_data *data)
{
	/* disable setting */
	cm36686_i2c_write_word(data->i2c_client, REG_CS_CONF1,
		als_reg_setting[1][1]);
	hrtimer_cancel(&data->light_timer);
	cancel_work_sync(&data->work_light);
}

static int proximity_open_cancelation(struct cm36686_data *data)
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
			pr_err("[SENSOR] %s: Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = cancel_filp->f_op->read(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("[SENSOR] %s: Can't read the cancel data from file\n",
			__func__);
		err = -EIO;
	}

	/*If there is an offset cal data. */
	if (ps_reg_init_setting[PS_CANCEL][CMD] != 0) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] =
			data->pdata->cancel_hi_thd ?
			data->pdata->cancel_hi_thd :
			CANCEL_HI_THD;
		ps_reg_init_setting[PS_THD_LOW][CMD] =
			data->pdata->cancel_low_thd ?
			data->pdata->cancel_low_thd :
			CANCEL_LOW_THD;
	}

	pr_info("[SENSOR] %s: cancel = 0x%x, high = 0x%x, low = 0x%x\n",
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
	struct cm36686_data *data = dev_get_drvdata(dev);
	struct file *cancel_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;
	u16 ps_data = 0;

	if (do_calib) {
		mutex_lock(&data->read_lock);
		cm36686_i2c_read_word(data,
			REG_PS_DATA, &ps_data);
		ps_reg_init_setting[PS_CANCEL][CMD] = ps_data;
		mutex_unlock(&data->read_lock);

		if (ps_reg_init_setting[PS_CANCEL][CMD] < CAL_SKIP_ADC) {
			/*SKIP*/
			ps_reg_init_setting[PS_CANCEL][CMD] = 0;
			pr_info("[SENSOR] %s:crosstalk <= %d\n", __func__,
				CAL_SKIP_ADC);
			data->uProxCalResult = 2;
			err = 1;
		} else if (ps_reg_init_setting[PS_CANCEL][CMD] < CAL_FAIL_ADC) {
			/*CANCELATION*/
			ps_reg_init_setting[PS_CANCEL][CMD] =
				ps_reg_init_setting[PS_CANCEL][CMD];
			pr_info("[SENSOR] %s:crosstalk_offset = %u", __func__,
				ps_reg_init_setting[PS_CANCEL][CMD]);
			data->uProxCalResult = 1;
			err = 0;
		} else {
			/*FAIL*/
			ps_reg_init_setting[PS_CANCEL][CMD] = 0;
			pr_info("[SENSOR] %s:crosstalk > %d\n", __func__,
				CAL_FAIL_ADC);
			data->uProxCalResult = 0;
			err = 1;
		}

		if (err == 0) {
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
				DEFUALT_HI_THD;
			ps_reg_init_setting[PS_THD_LOW][CMD] =
				data->pdata->default_low_thd ?
				data->pdata->default_low_thd :
				DEFUALT_LOW_THD;
		}
	} else { /* reset */
		ps_reg_init_setting[PS_CANCEL][CMD] = 0;
		ps_reg_init_setting[PS_THD_HIGH][CMD] =
			data->pdata->default_hi_thd ?
			data->pdata->default_hi_thd :
			DEFUALT_HI_THD;
		ps_reg_init_setting[PS_THD_LOW][CMD] =
			data->pdata->default_low_thd ?
			data->pdata->default_low_thd :
			DEFUALT_LOW_THD;
	}

	err = cm36686_i2c_write_word(data->i2c_client, REG_PS_CANC,
		ps_reg_init_setting[PS_CANCEL][CMD]);
	if (err < 0)
		pr_err("[SENSOR] %s: cm36686_ps_canc_reg is failed. %d\n",
			__func__, err);
	err = cm36686_i2c_write_word(data->i2c_client, REG_PS_THD_HIGH,
		ps_reg_init_setting[PS_THD_HIGH][CMD]);
	if (err < 0)
		pr_err("[SENSOR] %s: cm36686_ps_high_reg is failed. %d\n",
			__func__, err);
	err = cm36686_i2c_write_word(data->i2c_client, REG_PS_THD_LOW,
		ps_reg_init_setting[PS_THD_LOW][CMD]);
	if (err < 0)
		pr_err("[SENSOR] %s: cm36686_ps_low_reg is failed. %d\n",
			__func__, err);

	pr_info("[SENSOR] %s: cancel = 0x%x, high = 0x%x, low = 0x%x\n",
		__func__,
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cancel_filp)) {
		pr_err("[SENSOR] %s: Can't open cancelation file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cancel_filp);
		return err;
	}

	err = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("[SENSOR] %s: Can't write the cancel data to file\n",
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
		pr_debug("[SENSOR] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	err = proximity_store_cancelation(dev, do_calib);
	if (err < 0) {
		pr_err("[SENSOR] %s: store_cancelation() failed\n",
			__func__);
		return err;
	}

	return size;
}

static ssize_t proximity_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n",
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD_HIGH][CMD],
		ps_reg_init_setting[PS_THD_LOW][CMD]);
}

static ssize_t proximity_cancel_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	pr_info("[SENSOR] %s, %u\n", __func__, data->uProxCalResult);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxCalResult);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", data->avg[0],
		data->avg[1], data->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	bool new_value = false;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[SENSOR] %s, average enable = %d\n", __func__, new_value);

	mutex_lock(&data->power_lock);
	if (new_value) {
		if (!(data->power_state & PROXIMITY_ENABLED)) {
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			cm36686_leden_gpio_onoff(data->pdata, true);
#endif
			cm36686_i2c_write_word(data->i2c_client,
				REG_PS_CONF1,
				ps_reg_init_setting[PS_CONF1][CMD]);
		}
		hrtimer_start(&data->prox_timer, data->prox_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&data->prox_timer);
		cancel_work_sync(&data->work_prox);
		if (!(data->power_state & PROXIMITY_ENABLED)) {
			cm36686_i2c_write_word(data->i2c_client,
				REG_PS_CONF1, 0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
			cm36686_leden_gpio_onoff(data->pdata, false);
#endif
		}
	}
	mutex_unlock(&data->power_lock);

	return size;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	u16 ps_data;

	mutex_lock(&data->power_lock);
	if (!(data->power_state & PROXIMITY_ENABLED)) {
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		cm36686_leden_gpio_onoff(data->pdata, true);
#endif
		cm36686_i2c_write_word(data->i2c_client, REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&data->read_lock);
	cm36686_i2c_read_word(data, REG_PS_DATA, &ps_data);
	mutex_unlock(&data->read_lock);

	if (!(data->power_state & PROXIMITY_ENABLED)) {
		cm36686_i2c_write_word(data->i2c_client, REG_PS_CONF1,
			0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		cm36686_leden_gpio_onoff(data->pdata, false);
#endif
	}
	mutex_unlock(&data->power_lock);

	return snprintf(buf, PAGE_SIZE, "%u\n", ps_data);
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
	struct cm36686_data *data = dev_get_drvdata(dev);
	u16 thresh_value = ps_reg_init_setting[PS_THD_HIGH][CMD];
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		pr_err("[SENSOR] %s, kstrtoint failed.", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_HIGH][CMD] = thresh_value;
		err = cm36686_i2c_write_word(data->i2c_client,
			REG_PS_THD_HIGH,
			ps_reg_init_setting[PS_THD_HIGH][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s: is failed. %d\n",
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
	struct cm36686_data *data = dev_get_drvdata(dev);
	u16 thresh_value = ps_reg_init_setting[PS_THD_LOW][CMD];
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		pr_err("[SENSOR] %s, kstrtoint failed.", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD_LOW][CMD] = thresh_value;
		err = cm36686_i2c_write_word(data->i2c_client,
			REG_PS_THD_LOW,
			ps_reg_init_setting[PS_THD_LOW][CMD]);
		if (err < 0)
			pr_err("[SENSOR] %s: is failed. %d\n",
				__func__, err);
		pr_info("[SENSOR] %s, new low threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("[SENSOR] %s, wrong low threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

/* sysfs for vendor & name */
static ssize_t cm36686_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t cm36686_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static struct device_attribute dev_attr_prox_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36686_vendor_show, NULL);
static struct device_attribute dev_attr_light_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36686_vendor_show, NULL);
static struct device_attribute dev_attr_prox_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36686_name_show, NULL);
static struct device_attribute dev_attr_light_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36686_name_show, NULL);

static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show, NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
static struct device_attribute dev_attr_prox_raw = __ATTR(raw_data,
	S_IRUGO, proximity_state_show, NULL);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_prox_sensor_vendor,
	&dev_attr_prox_sensor_name,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_avg,
	&dev_attr_state,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_prox_raw,
	NULL,
};

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u\n", data->als_data,
		data->white_data);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
#ifdef cm36686_DEBUG
	pr_info("[SENSOR] %s = %u,%u\n", __func__, data->als_data,
		data->white_data);
#endif
	return snprintf(buf, PAGE_SIZE, "%u,%u\n", data->als_data,
		data->white_data);
}

static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_light_sensor_vendor,
	&dev_attr_light_sensor_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static ssize_t cm36686_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%lld\n",
		ktime_to_ns(data->light_poll_delay));
}

static ssize_t cm36686_poll_delay_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	mutex_lock(&data->power_lock);
	if (new_delay != ktime_to_ns(data->light_poll_delay)) {
		data->light_poll_delay = ns_to_ktime(new_delay);
		if (data->power_state & LIGHT_ENABLED) {
			cm36686_light_disable(data);
			cm36686_light_enable(data);
		}
		pr_info("[SENSOR] %s, poll_delay = %lld\n",
			__func__, new_delay);
	}
	mutex_unlock(&data->power_lock);

	return size;
}

static ssize_t light_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[SENSOR] %s,new_value=%d\n", __func__, new_value);

	mutex_lock(&data->power_lock);
	if (new_value && !(data->power_state & LIGHT_ENABLED)) {
		data->power_state |= LIGHT_ENABLED;
		cm36686_light_enable(data);
	} else if (!new_value && (data->power_state & LIGHT_ENABLED)) {
		cm36686_light_disable(data);
		data->power_state &= ~LIGHT_ENABLED;
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static ssize_t light_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(data->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->power_lock);
	pr_info("[SENSOR] %s, new_value = %d\n", __func__, new_value);
	if (new_value && !(data->power_state & PROXIMITY_ENABLED)) {
		int i;
		int err = 0;

		data->power_state |= PROXIMITY_ENABLED;
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		cm36686_leden_gpio_onoff(data->pdata, true);
#endif

		/* open cancelation data */
		err = proximity_open_cancelation(data);
		if (err < 0 && err != -ENOENT)
			pr_err("[SENSOR] %s: proximity_open_cancelation() failed\n",
				__func__);

		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++)
			cm36686_i2c_write_word(data->i2c_client,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);

		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev,
			ABS_DISTANCE, 1);
		input_sync(data->proximity_input_dev);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else if (!new_value && (data->power_state & PROXIMITY_ENABLED)) {
		data->power_state &= ~PROXIMITY_ENABLED;

		disable_irq_wake(data->irq);
		disable_irq(data->irq);
		/* disable settings */
		cm36686_i2c_write_word(data->i2c_client,
			REG_PS_CONF1, 0x0001);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
		cm36686_leden_gpio_onoff(data->pdata, false);
#endif
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(data->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}


static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36686_poll_delay_show, cm36686_poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	light_enable_show, light_enable_store);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_enable_show, proximity_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};


/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t cm36686_irq_thread_fn(int irq, void *user_data)
{
	struct cm36686_data *data = user_data;
	u16 ps_data = 0;

#ifdef cm36686_DEBUG
	static int count;
	pr_info("[SENSOR] %s\n", __func__);
#endif

	data->val = gpio_get_value(data->pdata->irq);
	cm36686_i2c_read_word(data, REG_PS_DATA, &ps_data);
#ifdef cm36686_DEBUG
	pr_info("[SENSOR] %s: count = %d\n", __func__, count++);
#endif

	if (data->power_state & PROXIMITY_ENABLED) {
		/* 0 is close, 1 is far */
		input_report_abs(data->proximity_input_dev, ABS_DISTANCE,
			data->val);
		input_sync(data->proximity_input_dev);
	}

	wake_lock_timeout(&data->prx_wake_lock, 3 * HZ);

	pr_info("[SENSOR] %s: val = %u, ps_data = %u (close:0, far:1)\n",
		__func__, data->val, ps_data);

	return IRQ_HANDLED;
}

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart cm36686_light_timer_func(struct hrtimer *timer)
{
	struct cm36686_data *data
		= container_of(timer, struct cm36686_data, light_timer);
	queue_work(data->light_wq, &data->work_light);
	hrtimer_forward_now(&data->light_timer, data->light_poll_delay);
	return HRTIMER_RESTART;
}

static void cm36686_work_func_light(struct work_struct *work)
{
	struct cm36686_data *data = container_of(work,
		struct cm36686_data, work_light);

	mutex_lock(&data->read_lock);
	cm36686_i2c_read_word(data, REG_ALS_DATA, &data->als_data);
	cm36686_i2c_read_word(data, REG_WHITE_DATA, &data->white_data);
	mutex_unlock(&data->read_lock);

	input_report_rel(data->light_input_dev, REL_DIAL,
		data->als_data + 1);
	input_report_rel(data->light_input_dev, REL_WHEEL,
		data->white_data + 1);
	input_sync(data->light_input_dev);

	if (data->count_log_time >= LIGHT_LOG_TIME) {
		pr_info("[SENSOR] %s, %u,%u\n", __func__,
			data->als_data, data->white_data);
		data->count_log_time = 0;
	} else
		data->count_log_time++;
}

static void proxsensor_get_avg_val(struct cm36686_data *data)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u16 ps_data = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		msleep(40);
		cm36686_i2c_read_word(data, REG_PS_DATA,
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

static void cm36686_work_func_prox(struct work_struct *work)
{
	struct cm36686_data *data = container_of(work,
		struct cm36686_data, work_prox);
	proxsensor_get_avg_val(data);
}

static enum hrtimer_restart cm36686_prox_timer_func(struct hrtimer *timer)
{
	struct cm36686_data *data = container_of(timer,
		struct cm36686_data, prox_timer);
	queue_work(data->prox_wq, &data->work_prox);
	hrtimer_forward_now(&data->prox_timer, data->prox_poll_delay);
	return HRTIMER_RESTART;
}

static int cm36686_setup_reg(struct cm36686_data *data)
{
	int err = 0, i = 0;
	u16 tmp = 0;

	/* ALS initialization */
	err = cm36686_i2c_write_word(data->i2c_client,
			als_reg_setting[0][0],
			als_reg_setting[0][1]);
	if (err < 0) {
		pr_err("[SENSOR] %s: cm36686_als_reg is failed. %d\n", __func__,
			err);
		return err;
	}
	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		err = cm36686_i2c_write_word(data->i2c_client,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
		if (err < 0) {
			pr_err("[SENSOR] %s: cm36686_ps_reg is failed. %d\n",
				__func__, err);
			return err;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	mutex_lock(&data->read_lock);
	err = cm36686_i2c_read_word(data, REG_PS_DATA, &tmp);
	mutex_unlock(&data->read_lock);
	if (err < 0) {
		pr_err("[SENSOR] %s: read ps_data failed\n", __func__);
		err = -EIO;
	}

	/* turn off */
	cm36686_i2c_write_word(data->i2c_client, REG_CS_CONF1, 0x0001);
	cm36686_i2c_write_word(data->i2c_client, REG_PS_CONF1, 0x0001);
	cm36686_i2c_write_word(data->i2c_client, REG_PS_CONF3, 0x0000);

	return err;
}

static int cm36686_setup_irq(struct cm36686_data *data)
{
	int rc;
	struct cm36686_platform_data *pdata = data->pdata;

	rc = gpio_request(pdata->irq, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("[SENSOR] %s: gpio %d request failed (%d)\n",
			__func__, pdata->irq, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->irq);
	if (rc < 0) {
		pr_err("[SENSOR] %s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->irq, rc);
		goto err_gpio_direction_input;
	}

	data->irq = gpio_to_irq(pdata->irq);
	rc = request_threaded_irq(data->irq, NULL,
		cm36686_irq_thread_fn,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_int", data);
	if (rc < 0) {
		pr_err("[SENSOR] %s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, data->irq, pdata->irq, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(data->irq);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->irq);
done:
	return rc;
}


/* device tree parsing function */
static int cm36686_parse_dt(struct device *dev,
	struct cm36686_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;

	if (pdata == NULL)
		return -ENODEV;

	pdata->irq = of_get_named_gpio_flags(np, "cm36686,irq_gpio", 0, &flags);
	if (pdata->irq < 0) {
		pr_err("[SENSOR]: %s - get prox_int error\n", __func__);
		return -ENODEV;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	pdata->leden_gpio = of_get_named_gpio_flags(np,
		"cm36686,leden_gpio", 0, &flags);
	if (pdata->leden_gpio < 0) {
		pr_err("[SENSOR] %s: get prox_leden_gpio error\n", __func__);
		return -ENODEV;
	}
#endif

	ret = of_property_read_u32(np, "cm36686,default_hi_thd",
		&pdata->default_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s: Cannot set default_hi_thd\n",
			__func__);
		pdata->default_hi_thd = DEFUALT_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36686,default_low_thd",
		&pdata->default_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s: Cannot set default_low_thd\n",
			__func__);
		pdata->default_low_thd = DEFUALT_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36686,cancel_hi_thd",
		&pdata->cancel_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s: Cannot set cancel_hi_thd\n",
			__func__);
		pdata->cancel_hi_thd = CANCEL_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36686,cancel_low_thd",
		&pdata->cancel_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR] %s: Cannot set cancel_low_thd\n",
			__func__);
		pdata->cancel_low_thd = CANCEL_LOW_THD;
	}

	ps_reg_init_setting[2][CMD] = pdata->default_low_thd;
	ps_reg_init_setting[3][CMD] = pdata->default_hi_thd;

	return 0;
}

static int prox_regulator_onoff(struct device *dev, bool onoff)
{
	struct regulator *vdd;
	int ret;

	pr_info("[SENSOR] %s %s\n", __func__, (onoff) ? "on" : "off");

	vdd = devm_regulator_get(dev, "cm36686,vdd");
	if (IS_ERR(vdd)) {
		pr_err("[SENSOR] %s: cannot get vdd\n", __func__);
		return -ENOMEM;
	}

	regulator_set_voltage(vdd, 3000000, 3000000);
	if (onoff) {
		ret = regulator_enable(vdd);
		if (ret)
			pr_err("[SENSOR] %s: Failed to enable vdd.\n",
				__func__);
	} else {
		ret = regulator_disable(vdd);
		if (ret)
			pr_err("[SENSOR] %s: Failed to disable vdd.\n",
				__func__);
	}
	msleep(20);

	devm_regulator_put(vdd);
	msleep(20);

	return 0;
}

static int cm36686_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct cm36686_data *data;
	struct cm36686_platform_data *pdata;

	pr_info("[SENSOR] %s: Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR] %s: i2c functionality check failed!\n",
			__func__);
		ret = -ENODEV;
		goto exit;
	}

	prox_regulator_onoff(&client->dev, true);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct cm36686_platform_data), GFP_KERNEL);
		ret = cm36686_parse_dt(&client->dev, pdata);
		if (ret)
			goto err_parse_dt;
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("[SENSOR] %s: missing pdata!\n", __func__);
		goto err_no_pdata;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	/* setup leden_gpio */
	ret = cm36686_setup_leden_gpio(pdata);
	if (ret) {
		pr_err("%s: could not setup leden_gpio\n", __func__);
		goto err_setup_leden_gpio;
	}
	cm36686_leden_gpio_onoff(pdata, true);
#endif

	/* Check if the device is there or not. */
	ret = cm36686_i2c_write_word(client, REG_CS_CONF1, 0x0001);
	if (ret < 0) {
		pr_err("[SENSOR] %s: cm36686 is not connected.(%d)\n",
			__func__, ret);
		goto err_check_device;
	}

	/* Allocate memory for driver data */
	data = kzalloc(sizeof(struct cm36686_data), GFP_KERNEL);
	if (!data) {
		pr_err("[SENSOR] %s: failed to alloc memory\n",
			__func__);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	data->pdata = pdata;
	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->power_lock);
	mutex_init(&data->read_lock);

	/* wake lock init for proximity sensor */
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");

	/* setup initial registers */
	ret = cm36686_setup_reg(data);
	if (ret < 0) {
		pr_err("[SENSOR] %s: could not setup regs\n", __func__);
		goto err_setup_reg;
	}

#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	cm36686_leden_gpio_onoff(data->pdata, false);
#endif

	/* setup irq */
	ret = cm36686_setup_irq(data);
	if (ret) {
		pr_err("[SENSOR] %s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* allocate proximity input_device */
	data->proximity_input_dev = input_allocate_device();
	if (!data->proximity_input_dev) {
		pr_err("[SENSOR] %s: could not alloc proximity input device\n",
			__func__);
		goto err_input_allocate_device_proximity;
	}

	input_set_drvdata(data->proximity_input_dev, data);
	data->proximity_input_dev->name = "proximity_sensor";
	input_set_capability(data->proximity_input_dev, EV_ABS,
		ABS_DISTANCE);
	input_set_abs_params(data->proximity_input_dev, ABS_DISTANCE,
		0, 1, 0, 0);

	ret = input_register_device(data->proximity_input_dev);
	if (ret < 0) {
		pr_err("[SENSOR] %s: could not register input device\n",
			__func__);
		goto err_input_register_device_proximity;
	}

	ret = sensors_create_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);
	if (ret < 0) {
		pr_err("[SENSOR] %s: create_symlink error\n", __func__);
		goto err_sensors_create_symlink_prox;
	}

	ret = sysfs_create_group(&data->proximity_input_dev->dev.kobj,
		 &proximity_attribute_group);
	if (ret) {
		pr_err("[SENSOR] %s: could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group_proximity;
	}

	/* For factory test mode, we use timer to get average proximity data. */
	/* prox_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);/*2 sec*/
	data->prox_timer.function = cm36686_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->prox_wq = create_singlethread_workqueue("cm36686_prox_wq");
	if (!data->prox_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR] %s: could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox, cm36686_work_func_prox);

	/* allocate lightsensor input_device */
	data->light_input_dev = input_allocate_device();
	if (!data->light_input_dev) {
		pr_err("[SENSOR] %s: could not allocate light input device\n",
			__func__);
		goto err_input_allocate_device_light;
	}

	input_set_drvdata(data->light_input_dev, data);
	data->light_input_dev->name = "light_sensor";
	input_set_capability(data->light_input_dev, EV_REL, REL_MISC);
	input_set_capability(data->light_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->light_input_dev, EV_REL, REL_WHEEL);

	ret = input_register_device(data->light_input_dev);
	if (ret < 0) {
		pr_err("[SENSOR] %s: could not register input device\n",
			__func__);
		goto err_input_register_device_light;
	}

	ret = sensors_create_symlink(&data->light_input_dev->dev.kobj,
		data->light_input_dev->name);
	if (ret < 0) {
		pr_err("[SENSOR] %s: create_symlink error\n", __func__);
		goto err_sensors_create_symlink_light;
	}

	ret = sysfs_create_group(&data->light_input_dev->dev.kobj,
		&light_attribute_group);
	if (ret) {
		pr_err("[SENSOR] %s: could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group_light;
	}

	/* light_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->light_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	data->light_timer.function = cm36686_light_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->light_wq = create_singlethread_workqueue("cm36686_light_wq");
	if (!data->light_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR] %s: could not create light workqueue\n",
			__func__);
		goto err_create_light_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_light, cm36686_work_func_light);

	/* set sysfs for proximity sensor */
	ret = sensors_register(data->proximity_dev,
		data, prox_sensor_attrs, "proximity_sensor");
	if (ret) {
		pr_err("[SENSOR] %s: could not create proximity_dev (%d)\n",
			__func__, ret);
		goto prox_sensor_register_failed;
	}

	/* set sysfs for light sensor */
	ret = sensors_register(data->light_dev,
		data, light_sensor_attrs, "light_sensor");
	if (ret) {
		pr_err("[SENSOR] %s: could not create light_dev (%d)\n",
			__func__, ret);
		goto light_sensor_register_failed;
	}

	pr_info("[SENSOR] %s is success.\n", __func__);
	return ret;

/* error, unwind it all */
light_sensor_register_failed:
	sensors_unregister(data->proximity_dev, prox_sensor_attrs);
prox_sensor_register_failed:
	destroy_workqueue(data->light_wq);
err_create_light_workqueue:
	sysfs_remove_group(&data->light_input_dev->dev.kobj,
		&light_attribute_group);
err_sysfs_create_group_light:
	sensors_remove_symlink(&data->light_input_dev->dev.kobj,
		data->light_input_dev->name);
err_sensors_create_symlink_light:
	input_unregister_device(data->light_input_dev);
err_input_register_device_light:
	input_free_device(data->light_input_dev);
err_input_allocate_device_light:
	destroy_workqueue(data->prox_wq);
err_create_prox_workqueue:
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);
err_sensors_create_symlink_prox:
	input_unregister_device(data->proximity_input_dev);
err_input_register_device_proximity:
	input_free_device(data->proximity_input_dev);
err_input_allocate_device_proximity:
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);
err_setup_irq:
err_setup_reg:
	wake_lock_destroy(&data->prx_wake_lock);
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->power_lock);
err_mem_alloc:
err_check_device:
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	cm36686_leden_gpio_onoff(pdata, false);
	gpio_free(pdata->leden_gpio);
err_setup_leden_gpio:
#endif
err_no_pdata:
err_parse_dt:
	prox_regulator_onoff(&client->dev, false);
exit:
	pr_err("[SENSOR] %s is FAILED (%d)\n", __func__, ret);
	return ret;
}

static int cm36686_i2c_remove(struct i2c_client *client)
{
	struct cm36686_data *data = i2c_get_clientdata(client);

	/* free irq */
	if (data->power_state & PROXIMITY_ENABLED) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
	}
	free_irq(data->irq, data);
	gpio_free(data->pdata->irq);

	/* device off */
	if (data->power_state & LIGHT_ENABLED)
		cm36686_light_disable(data);
	if (data->power_state & PROXIMITY_ENABLED)
		cm36686_i2c_write_word(data->i2c_client,
			REG_PS_CONF1, 0x0001);

	/* destroy workqueue */
	destroy_workqueue(data->light_wq);
	destroy_workqueue(data->prox_wq);

	/* sysfs destroy */
	sensors_unregister(data->light_dev, light_sensor_attrs);
	sensors_unregister(data->proximity_dev, prox_sensor_attrs);
	sensors_remove_symlink(&data->light_input_dev->dev.kobj,
		data->light_input_dev->name);
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
		data->proximity_input_dev->name);

	/* input device destroy */
	sysfs_remove_group(&data->light_input_dev->dev.kobj,
		&light_attribute_group);
	input_unregister_device(data->light_input_dev);
	sysfs_remove_group(&data->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	input_unregister_device(data->proximity_input_dev);
#if defined(CONFIG_SENSORS_LEDA_EN_GPIO)
	cm36686_leden_gpio_onoff(data->pdata, false);
	gpio_free(data->pdata->leden_gpio);
#endif

	/* lock destroy */
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->power_lock);
	wake_lock_destroy(&data->prx_wake_lock);

	kfree(data);
	prox_regulator_onoff(&client->dev, false);

	return 0;
}

static int cm36686_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   cm36686->power_state because we use that state in resume.
	 */
	struct cm36686_data *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED)
		cm36686_light_disable(data);

	return 0;
}

static int cm36686_resume(struct device *dev)
{
	struct cm36686_data *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED)
		cm36686_light_enable(data);

	return 0;
}

static struct of_device_id cm36686_match_table[] = {
	{ .compatible = "cm36686",},
	{},
};

static const struct i2c_device_id cm36686_device_id[] = {
	{"cm36686", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm36686_device_id);

static const struct dev_pm_ops cm36686_pm_ops = {
	.suspend = cm36686_suspend,
	.resume = cm36686_resume
};

static struct i2c_driver cm36686_i2c_driver = {
	.driver = {
		   .name = "cm36686",
		   .owner = THIS_MODULE,
		   .of_match_table = cm36686_match_table,
		   .pm = &cm36686_pm_ops
	},
	.probe = cm36686_i2c_probe,
	.remove = cm36686_i2c_remove,
	.id_table = cm36686_device_id,
};

static int __init cm36686_init(void)
{
	return i2c_add_driver(&cm36686_i2c_driver);
}

static void __exit cm36686_exit(void)
{
	i2c_del_driver(&cm36686_i2c_driver);
}

module_init(cm36686_init);
module_exit(cm36686_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("RGB Sensor device driver for cm36686");
MODULE_LICENSE("GPL");
