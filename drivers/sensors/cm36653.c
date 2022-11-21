/* driver/sensor/cm36653.c
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

#include <linux/module.h>
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
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>

#define	VENDOR                "CAPELLA"
#define	CHIP_ID               "CM36653"
#define MODULE_NAME_LIGHT     "light_sensor"
#define MODULE_NAME_PROX      "proximity_sensor"

/* Intelligent Cancelation*/
#define CANCELATION_FILE_PATH  "/efs/prox_cal"
#define CANCELATION_THRESHOLD  0x0806

#define I2C_M_WR	0 /* for i2c Write */
#define I2c_M_RD	1 /* for i2c Read */

#define REL_RED         REL_HWHEEL
#define REL_GREEN       REL_DIAL
#define REL_BLUE        REL_WHEEL
#define REL_WHITE       REL_MISC

/* register addresses */
/* Ambient light sensor */
#define REG_CS_CONF1    0x00
#define REG_RED         0x08
#define REG_GREEN       0x09
#define REG_BLUE        0x0A
#define REG_WHITE       0x0B

/* Proximity sensor */
#define REG_PS_CONF1    0x03
#define REG_PS_THD      0x05
#define REG_PS_CANC     0x06
#define REG_PS_DATA     0x07

#define ALS_REG_NUM     2
#define PS_REG_NUM      3

#define MSK_L(x)        (x & 0xff)
#define MSK_H(x)        ((x & 0xff00) >> 8)

#define PROX_READ_NUM   40
#define LIGHT_LOG_TIME  15

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

/* register settings */
static u8 als_reg_setting[ALS_REG_NUM][2] = {
	{REG_CS_CONF1, 0x00},	/* enable */
	{REG_CS_CONF1, 0x01},	/* disable */
};

/* Change threshold value on the midas-sensor.c */
enum {
	PS_CONF1 = 0,
	PS_THD,
	PS_CANCEL,
};
enum {
	REG_ADDR = 0,
	CMD,
};

static u16 ps_reg_init_setting[PS_REG_NUM][2] = {
	{REG_PS_CONF1, 0x430C},	/* REG_PS_CONF1 */
	{REG_PS_THD, 0x0A08},	/* REG_PS_THD */
	{REG_PS_CANC, 0x00},	/* REG_PS_CANC */
};

/* driver data */
struct cm36653_p {
	struct i2c_client *i2c_client;
	struct wake_lock prx_wake_lock;
	struct input_dev *prox_input_dev;
	struct input_dev *light_input_dev;
	struct mutex power_lock;
	struct mutex read_lock;
	struct hrtimer light_timer;
	struct hrtimer prox_timer;
	struct workqueue_struct *light_wq;
	struct workqueue_struct *prox_wq;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct device *cm36653_prox_dev;
	struct device *light_dev;
	ktime_t light_poll_delay;
	ktime_t prox_poll_delay;
	int irq;
	int prox_irq_gpio;
	int prox_led_gpio;
	bool is_led_on;
	u32 threshold;
	u8 power_state;
	int avg[3];
	u16 color[4];
	int time_count;
	u16 default_thresh;
};

int cm36653_i2c_read_byte(struct cm36653_p *data, u8 command, u8 *val)
{
	int ret = 0;
	int retry = 3;
	struct i2c_msg msg[1];
	struct i2c_client *client = data->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	/* send slave address & command */
	msg->addr = client->addr;
	msg->flags = I2C_M_RD;
	msg->len = 1;
	msg->buf = val;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0)
			return ret;
	}
	pr_err("[SENSOR]: %s - i2c read failed at addr 0x%x: %d\n", __func__,
		client->addr, ret);
	return ret;
}

int cm36653_i2c_read_word(struct cm36653_p *data, u8 command, u16 *val)
{
	int ret = 0;
	int retry = 3;
	struct i2c_client *client = data->i2c_client;
	struct i2c_msg msg[2];
	unsigned char buf[2] = {0,};
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
		msg[1].buf = buf;

		ret = i2c_transfer(client->adapter, msg, 2);

		if (ret >= 0) {
			value = (u16)buf[1];
			*val = (value << 8) | (u16)buf[0];
			return ret;
		}
	}
	pr_err("[SENSOR]: %s - i2c transfer error ret=%d\n", __func__, ret);
	return ret;
}

int cm36653_i2c_write_byte(struct cm36653_p *data, u8 command, u8 val)
{
	int ret = 0;
	struct i2c_client *client = data->i2c_client;
	struct i2c_msg msg[1];
	unsigned char buf[2];
	int retry = 3;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		buf[0] = command;
		buf[1] = val;

		/* send slave address & command */
		msg->addr = client->addr;
		msg->flags = I2C_M_WR;
		msg->len = 2;
		msg->buf = buf;

		ret = i2c_transfer(client->adapter, msg, 1);

		if (ret >= 0)
			return 0;
	}
	pr_err("[SENSOR]: %s - i2c transfer error(%d)\n", __func__, ret);
	return ret;
}

int cm36653_i2c_write_word(struct cm36653_p *data, u8 command, u16 val)
{
	int ret = 0;
	struct i2c_client *client = data->i2c_client;
	int retry = 3;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		ret = i2c_smbus_write_word_data(client, command, val);
		if (ret >= 0)
			return 0;
	}
	pr_err("[SENSOR]: %s - i2c transfer error(%d)\n", __func__, ret);
	return ret;
}

void cm36653_prox_led_gpiooff(struct cm36653_p *data, bool onoff)
{
	if (onoff) {
		gpio_set_value(data->prox_led_gpio, 1);
		msleep(20);
	} else
		gpio_set_value(data->prox_led_gpio, 0);
	data->is_led_on = onoff;

	usleep_range(1000, 1100);
	pr_info("[SENSOR]: %s - onoff = %s, led_on_gpio = %s\n",
		__func__, onoff ? "on" : "off",
		gpio_get_value(data->prox_led_gpio) ? "high" : "low");
}

static void cm36653_light_enable(struct cm36653_p *data)
{
	/* enable setting */
	cm36653_i2c_write_byte(data, REG_CS_CONF1,
		als_reg_setting[0][1]);
	hrtimer_start(&data->light_timer, data->light_poll_delay,
		HRTIMER_MODE_REL);
}

static void cm36653_light_disable(struct cm36653_p *data)
{
	/* disable setting */
	cm36653_i2c_write_byte(data, REG_CS_CONF1, als_reg_setting[1][1]);
	hrtimer_cancel(&data->light_timer);
	cancel_work_sync(&data->work_light);
}

/* sysfs */
static ssize_t cm36653_poll_delay_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(data->light_poll_delay));
}

static ssize_t cm36653_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	int64_t new_delay;
	int ret;

	ret = strict_strtoll(buf, 10, &new_delay);
	if (ret < 0)
		return ret;

	mutex_lock(&data->power_lock);
	if (new_delay != ktime_to_ns(data->light_poll_delay)) {
		data->light_poll_delay = ns_to_ktime(new_delay);
		if (data->power_state & LIGHT_ENABLED) {
			cm36653_light_disable(data);
			cm36653_light_enable(data);
		}
		pr_info("[SENSOR]: %s - poll_delay = %lld\n",
			__func__, new_delay);
	}
	mutex_unlock(&data->power_lock);

	return size;
}

static ssize_t cm36653_light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->power_lock);
	pr_info("[SENSOR]: %s - new_value = %d\n", __func__, new_value);
	if (new_value && !(data->power_state & LIGHT_ENABLED)) {
		data->power_state |= LIGHT_ENABLED;
		cm36653_light_enable(data);
	} else if (!new_value && (data->power_state & LIGHT_ENABLED)) {
		cm36653_light_disable(data);
		data->power_state &= ~LIGHT_ENABLED;
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static ssize_t cm36653_light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
			(data->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static void cm36653_prox_open_cancelation(struct cm36653_p *data)
{
	struct file *cancel_filp = NULL;
	int ret = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cancel_filp)) {
		ret = PTR_ERR(cancel_filp);
		if (ret != -ENOENT)
			pr_err("[SENSOR]: %s - Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		return;
	}

	ret = cancel_filp->f_op->read(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u8), &cancel_filp->f_pos);
	if (ret != sizeof(u8))
		pr_err("[SENSOR]: %s - Can't read the cancel data %d\n",
			__func__, ret);

	/*If there is an offset cal data. */
	if (ps_reg_init_setting[PS_CANCEL][CMD] != 0)
		ps_reg_init_setting[PS_THD][CMD] = CANCELATION_THRESHOLD;

	pr_info("[SENSOR]: %s - proximity ps_data = %d, ps_thresh = 0x%x\n",
		__func__, ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD][CMD]);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);
}

static int cm36653_prox_store_cancelation(struct device *dev, bool do_calib)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	struct file *cancel_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;
	u16 ps_data = 0;

	if (do_calib) {
		mutex_lock(&data->read_lock);
		cm36653_i2c_read_word(data,
			REG_PS_DATA, &ps_data);
		ps_reg_init_setting[PS_CANCEL][CMD] = 0xff & ps_data;
		mutex_unlock(&data->read_lock);

		ps_reg_init_setting[PS_THD][CMD] = CANCELATION_THRESHOLD;
	} else { /* reset */
		ps_reg_init_setting[PS_CANCEL][CMD] = 0;
		ps_reg_init_setting[PS_THD][CMD] = data->default_thresh;
	}
	cm36653_i2c_write_word(data, REG_PS_THD,
		ps_reg_init_setting[PS_THD][CMD]);
	cm36653_i2c_write_byte(data, REG_PS_CANC,
		ps_reg_init_setting[PS_CANCEL][CMD]);
	pr_info("[SENSOR]: %s - prox_cal = 0x%x, prox_thresh = 0x%x\n",
		__func__, ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cancel_filp)) {
		pr_err("[SENSOR]: %s - Can't open cancelation file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cancel_filp);
		return ret;
	}

	ret = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u8), &cancel_filp->f_pos);
	if (ret != sizeof(u8)) {
		pr_err("[SENSOR]: %s - Can't write the cancel data to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	if (!do_calib) /* delay for clearing */
		msleep(150);

	return ret;
}

static ssize_t cm36653_prox_cancel_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	bool do_calib;
	int ret;

	if (sysfs_streq(buf, "1")) /* calibrate cancelation value */
		do_calib = true;
	else if (sysfs_streq(buf, "0")) /* reset cancelation value */
		do_calib = false;
	else {
		pr_debug("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ret = cm36653_prox_store_cancelation(dev, do_calib);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - cm36653_prox_store_cancelation() failed\n",
			__func__);
		return ret;
	}

	return size;
}

static ssize_t cm36653_prox_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d\n", ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD][CMD] >> 8 & 0xff);
}

static ssize_t cm36653_prox_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->power_lock);
	pr_info("[SENSOR]: %s - new_value = %d, threshold = %d\n", __func__,
		new_value, ps_reg_init_setting[PS_THD][CMD] >> 8 & 0xff);
	if (new_value && !(data->power_state & PROXIMITY_ENABLED)) {
		u8 val = 1;
		int i;

		data->power_state |= PROXIMITY_ENABLED;

		if (data->prox_led_gpio)
			cm36653_prox_led_gpiooff(data, true);

		/* open cancelation data */
		cm36653_prox_open_cancelation(data);

		/* enable settings */
		for (i = 0; i < PS_REG_NUM; i++) {
			cm36653_i2c_write_word(data,
				ps_reg_init_setting[i][REG_ADDR],
				ps_reg_init_setting[i][CMD]);
		}

		val = gpio_get_value(data->prox_irq_gpio);
		/* 0 is close, 1 is far */
		input_report_abs(data->prox_input_dev,
			ABS_DISTANCE, val);
		input_sync(data->prox_input_dev);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else if (!new_value && (data->power_state & PROXIMITY_ENABLED)) {
		data->power_state &= ~PROXIMITY_ENABLED;

		disable_irq_wake(data->irq);
		disable_irq(data->irq);

		/* disable settings */
		cm36653_i2c_write_word(data, REG_PS_CONF1, 0x0001);

		if (data->prox_led_gpio)
			cm36653_prox_led_gpiooff(data, false);
	}
	mutex_unlock(&data->power_lock);
	return size;
}

static ssize_t cm36653_prox_enable_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       (data->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   cm36653_poll_delay_show, cm36653_poll_delay_store);

static struct device_attribute dev_attr_light_enable =
__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36653_light_enable_show, cm36653_light_enable_store);

static struct device_attribute dev_attr_cm36653_prox_enable =
__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36653_prox_enable_show, cm36653_prox_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute *cm36653_prox_sysfs_attrs[] = {
	&dev_attr_cm36653_prox_enable.attr,
	NULL
};

static struct attribute_group cm36653_prox_attribute_group = {
	.attrs = cm36653_prox_sysfs_attrs,
};

/* sysfs for vendor & name */
static ssize_t cm36653_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t cm36653_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static struct device_attribute dev_attr_prox_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36653_vendor_show, NULL);
static struct device_attribute dev_attr_light_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36653_vendor_show, NULL);
static struct device_attribute dev_attr_prox_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36653_name_show, NULL);
static struct device_attribute dev_attr_light_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36653_name_show, NULL);

/* proximity sysfs */
static ssize_t cm36653_prox_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d,%d,%d\n", data->avg[0],
		data->avg[1], data->avg[2]);
}

static ssize_t cm36653_prox_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	bool new_value = false;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[SENSOR]: %s - average enable = %d\n", __func__, new_value);
	mutex_lock(&data->power_lock);
	if (new_value) {
		if (!(data->power_state & PROXIMITY_ENABLED)) {
			if (data->prox_led_gpio)
				cm36653_prox_led_gpiooff(data, true);

			cm36653_i2c_write_word(data, REG_PS_CONF1,
				ps_reg_init_setting[PS_CONF1][CMD]);
		}
		hrtimer_start(&data->prox_timer, data->prox_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&data->prox_timer);
		cancel_work_sync(&data->work_prox);
		if (!(data->power_state & PROXIMITY_ENABLED)) {
			cm36653_i2c_write_word(data, REG_PS_CONF1,
				0x0001);
			if (data->prox_led_gpio)
				cm36653_prox_led_gpiooff(data, false);
		}
	}
	mutex_unlock(&data->power_lock);

	return size;
}

static ssize_t cm36653_prox_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	u16 ps_data;

	mutex_lock(&data->power_lock);
	if (!(data->power_state & PROXIMITY_ENABLED)) {
		if (data->prox_led_gpio)
			cm36653_prox_led_gpiooff(data, true);

		cm36653_i2c_write_word(data, REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&data->read_lock);
	cm36653_i2c_read_word(data, REG_PS_DATA,
		&ps_data);
	mutex_unlock(&data->read_lock);

	if (!(data->power_state & PROXIMITY_ENABLED)) {
		cm36653_i2c_write_word(data, REG_PS_CONF1, 0x0001);
		if (data->prox_led_gpio)
			cm36653_prox_led_gpiooff(data, false);
	}
	mutex_unlock(&data->power_lock);

	return sprintf(buf, "%d\n", (u8)(0xff & ps_data));
}

static ssize_t cm36653_prox_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "prox_threshold = %d\n",
		ps_reg_init_setting[PS_THD][CMD] >> 8 & 0xff);
}

static ssize_t cm36653_prox_thresh_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36653_p *data = dev_get_drvdata(dev);
	u8 thresh_value = ps_reg_init_setting[PS_THD][CMD] >> 8 & 0xff;
	int ret;

	ret = kstrtou8(buf, 10, &thresh_value);
	if (ret < 0)
		pr_err("[SENSOR]: %s - kstrtou8 failed.", __func__);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD][CMD] =
			(u16)(thresh_value << 8 | (thresh_value - 2));
		ret = cm36653_i2c_write_word(data, REG_PS_THD,
			ps_reg_init_setting[PS_THD][CMD]);
		if (ret < 0)
			pr_err("[SENSOR]: %s - cm36653_ps_reg is failed. %d\n",
				__func__, ret);
		pr_info("[SENSOR]: %s - new threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("[SENSOR]: %s - wrong threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36653_prox_cancel_show, cm36653_prox_cancel_store);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36653_prox_avg_show, cm36653_prox_avg_store);
static DEVICE_ATTR(state, S_IRUGO,
	cm36653_prox_state_show, NULL);
static DEVICE_ATTR(prox_thresh, S_IRUGO | S_IWUSR | S_IWGRP,
	cm36653_prox_thresh_show, cm36653_prox_thresh_store);
static struct device_attribute dev_attr_prox_raw =
	__ATTR(raw_data, S_IRUGO, cm36653_prox_state_show, NULL);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_prox_sensor_vendor,
	&dev_attr_prox_sensor_name,
	&dev_attr_prox_cal,
	&dev_attr_prox_avg,
	&dev_attr_state,
	&dev_attr_prox_thresh,
	&dev_attr_prox_raw,
	NULL,
};

/* light sysfs */
static ssize_t cm36653_light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u\n",
		data->color[0]+1, data->color[1]+1,
		data->color[2]+1, data->color[3]+1);
}

static ssize_t cm36653_light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36653_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u\n",
		data->color[0]+1, data->color[1]+1,
		data->color[2]+1, data->color[3]+1);
}

static DEVICE_ATTR(lux, S_IRUGO, cm36653_light_lux_show, NULL);
static struct device_attribute dev_attr_light_raw =
	__ATTR(raw_data, S_IRUGO, cm36653_light_data_show, NULL);

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_light_sensor_vendor,
	&dev_attr_light_sensor_name,
	&dev_attr_lux,
	&dev_attr_light_raw,
	NULL,
};

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t cm36653_irq_thread_fn(int irq, void *cm36653_data_p)
{
	struct cm36653_p *data = cm36653_data_p;
	u8 val;
	u16 ps_data = 0;

	val = gpio_get_value(data->prox_irq_gpio);
	cm36653_i2c_read_word(data, REG_PS_DATA, &ps_data);

	if (data->power_state & PROXIMITY_ENABLED) {
		/* 0 is close, 1 is far */
		input_report_abs(data->prox_input_dev,
			ABS_DISTANCE, val);
		input_sync(data->prox_input_dev);
	}

	wake_lock_timeout(&data->prx_wake_lock, 3 * HZ);

	pr_info("[SENSOR]: %s - val = %d, ps_data = %d (close:0, far:1)\n",
		__func__, val, (u8)(0xff & ps_data));

	return IRQ_HANDLED;
}

static int cm36653_setup_reg(struct cm36653_p *data)
{
	int ret = 0, i = 0;
	u16 tmp = 0;

	/* ALS initialization */
	ret = cm36653_i2c_write_byte(data,
			als_reg_setting[0][0],
			als_reg_setting[0][1]);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - cm36653_als_reg is failed. %d\n",
			__func__, ret);
		return ret;
	}
	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		ret = cm36653_i2c_write_word(data,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
		if (ret < 0) {
			pr_err("[SENSOR]: %s - cm36653_ps_reg is failed. %d\n",
				__func__, ret);
			return ret;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	mutex_lock(&data->read_lock);
	ret = cm36653_i2c_read_word(data, REG_PS_DATA, &tmp);
	mutex_unlock(&data->read_lock);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - read ps_data failed\n", __func__);
		ret = -EIO;
	}
	pr_err("[SENSOR]: %s - initial proximity value = %d\n",
		__func__, (u8)(0xff & tmp));

	/* turn off */
	cm36653_i2c_write_byte(data, REG_CS_CONF1, 0x01);
	cm36653_i2c_write_word(data, REG_PS_CONF1, 0x0001);

	pr_info("[SENSOR] %s is success.", __func__);
	return ret;
}

static int cm36653_setup_gpio(struct cm36653_p *data)
{
	int ret = -EIO;

	ret = gpio_request(data->prox_led_gpio, "gpio_prox_led_gpio");
	if (ret < 0) {
		pr_err("[SENSOR]: %s - gpio %d request failed (%d)\n",
		       __func__, data->prox_led_gpio, ret);
		return ret;
	}
	gpio_direction_output(data->prox_led_gpio, 0);

	ret = gpio_request(data->prox_irq_gpio, "gpio_cm36653_prox_out");
	if (ret < 0) {
		pr_err("[SENSOR]: %s - gpio %d request failed (%d)\n",
		       __func__, data->prox_irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(data->prox_irq_gpio);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - failed to set gpio %d as input (%d)\n",
		       __func__, data->prox_irq_gpio, ret);
		goto err_gpio_direction_input;
	}

	data->irq = gpio_to_irq(data->prox_irq_gpio);
	ret = request_threaded_irq(data->irq, NULL, cm36653_irq_thread_fn,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT, "cm36653_prox_int", data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - request irq(%d) failed %d (%d)\n",
		       __func__, data->irq, data->prox_irq_gpio, ret);
		goto err_request_irq;
	}

	/* start with intretupts disabled */
	disable_irq(data->irq);

	pr_err("[SENSOR]: %s - success\n", __func__);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(data->prox_irq_gpio);
	gpio_free(data->prox_led_gpio);
done:
	return ret;
}

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart cm36653_light_timer_func(struct hrtimer *timer)
{
	struct cm36653_p *data
	    = container_of(timer, struct cm36653_p, light_timer);
	queue_work(data->light_wq, &data->work_light);
	hrtimer_forward_now(&data->light_timer, data->light_poll_delay);
	return HRTIMER_RESTART;
}

static void cm36653_work_func_light(struct work_struct *work)
{
	struct cm36653_p *data = container_of(work, struct cm36653_p,
						    work_light);

	mutex_lock(&data->read_lock);
	cm36653_i2c_read_word(data, REG_RED, &data->color[0]);
	cm36653_i2c_read_word(data, REG_GREEN, &data->color[1]);
	cm36653_i2c_read_word(data, REG_BLUE, &data->color[2]);
	cm36653_i2c_read_word(data, REG_WHITE, &data->color[3]);
	mutex_unlock(&data->read_lock);

	input_report_rel(data->light_input_dev, REL_RED,
		data->color[0]+1);
	input_report_rel(data->light_input_dev, REL_GREEN,
		data->color[1]+1);
	input_report_rel(data->light_input_dev, REL_BLUE,
		data->color[2]+1);
	input_report_rel(data->light_input_dev, REL_WHITE,
		data->color[3]+1);
	input_sync(data->light_input_dev);

	if ((ktime_to_ms(data->light_poll_delay) * (int64_t)data->time_count)
		>= ((int64_t)LIGHT_LOG_TIME * MSEC_PER_SEC)) {
		pr_info("[SENSOR]: %s - r = %u, g = %u, b = %u, w = %u\n",
			__func__, data->color[0]+1, data->color[1]+1,
			data->color[2]+1, data->color[3]+1);
		data->time_count = 0;
	} else
		data->time_count++;
}

static void cm36653_proxsensor_get_avg_val(struct cm36653_p *data)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u16 ps_data = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		msleep(40);
		cm36653_i2c_read_word(data, REG_PS_DATA, &ps_data);
		avg += (u8)(0xff & ps_data);

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

static void cm36653_work_func_prox(struct work_struct *work)
{
	struct cm36653_p *data = container_of(work, struct cm36653_p,
						  work_prox);
	cm36653_proxsensor_get_avg_val(data);
}

static enum hrtimer_restart cm36653_prox_timer_func(struct hrtimer *timer)
{
	struct cm36653_p *data
			= container_of(timer, struct cm36653_p, prox_timer);
	queue_work(data->prox_wq, &data->work_prox);
	hrtimer_forward_now(&data->prox_timer, data->prox_poll_delay);
	return HRTIMER_RESTART;
}

static int cm36653_parse_dt(struct cm36653_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->prox_irq_gpio = of_get_named_gpio_flags(dNode,
		"cm36653-i2c,prox_irq_gpio", 0, &flags);
	if (data->prox_irq_gpio < 0) {
		pr_err("[SENSOR]: %s - get prox_irq_gpio error\n", __func__);
		return -ENODEV;
	}

	data->prox_led_gpio = of_get_named_gpio_flags(dNode,
		"cm36653-i2c,prox_led_gpio", 0, &flags);
	if (data->prox_led_gpio < 0) {
		pr_err("[SENSOR]: %s - get prox_led_gpio error\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_u32(dNode,
			"cm36653-i2c,threshold", &data->threshold) < 0)
		data->threshold = 0x0806;

	pr_info("[SENSOR] %s success, threshold = 0x%X\n",
		__func__, data->threshold);
	return 0;
}

static int cm36653_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct cm36653_p *data = NULL;

	pr_info("[SENSOR]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR]: %s - i2c functionality check failed!\n",
			__func__);
		return ret;
	}

	data = kzalloc(sizeof(struct cm36653_p), GFP_KERNEL);
	if (!data) {
		pr_err("[SENSOR]: %s - failed to alloc memory\n", __func__);
		return -ENOMEM;
	}

	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->power_lock);
	mutex_init(&data->read_lock);

	ret = cm36653_parse_dt(data, &client->dev);
		if (ret < 0) {
		pr_err("[SENSOR]: %s - - of_node error\n", __func__);
		ret = -ENODEV;
		goto err_of_node;
	}

	/* setup gpio */
	ret = cm36653_setup_gpio(data);
	if (ret) {
		pr_err("[SENSOR]: %s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	cm36653_prox_led_gpiooff(data, true);

	/* wake lock init for proximity sensor */
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");

	/* Check if the device is there or not. */
	ret = cm36653_i2c_write_byte(data, REG_CS_CONF1, 0x01);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - cm36653 is not connected.(%d)\n",
			__func__, ret);
		goto err_setup_reg;
	}
	/* setup initial registers */
	ps_reg_init_setting[PS_THD][CMD] = data->threshold;
	data->default_thresh = ps_reg_init_setting[PS_THD][CMD];

	ret = cm36653_setup_reg(data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - could not setup regs\n", __func__);
		goto err_setup_reg;
	}

	cm36653_prox_led_gpiooff(data, false);

	/* allocate proximity input_device */
	data->prox_input_dev = input_allocate_device();
	if (!data->prox_input_dev) {
		pr_err("[SENSOR]: %s - could not allocate proximity input\n",
		       __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_proximity;
	}

	input_set_drvdata(data->prox_input_dev, data);
	data->prox_input_dev->name = MODULE_NAME_PROX;
	input_set_capability(data->prox_input_dev, EV_ABS,
			     ABS_DISTANCE);
	input_set_abs_params(data->prox_input_dev, ABS_DISTANCE, 0, 1,
			     0, 0);

	ret = input_register_device(data->prox_input_dev);
	if (ret < 0) {
		input_free_device(data->prox_input_dev);
		pr_err("[SENSOR]: %s - could not register input device\n",
			__func__);
		goto err_input_register_device_proximity;
	}

	ret = sensors_create_symlink(&data->prox_input_dev->dev.kobj,
					data->prox_input_dev->name);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink_prox;
	}

	ret = sysfs_create_group(&data->prox_input_dev->dev.kobj,
				 &cm36653_prox_attribute_group);
	if (ret) {
		pr_err("[SENSOR]: %s - could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group_proximity;
	}

	/* For factory test mode, we use timer to get average proximity data. */
	/* prox_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);/*2 sec*/
	data->prox_timer.function = cm36653_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->prox_wq = create_singlethread_workqueue("cm36653_prox_wq");
	if (!data->prox_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR]: %s - could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_prox, cm36653_work_func_prox);

	/* allocate lightsensor input_device */
	data->light_input_dev = input_allocate_device();
	if (!data->light_input_dev) {
		pr_err("[SENSOR]: %s - could not allocate light input device\n",
			__func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}

	input_set_drvdata(data->light_input_dev, data);
	data->light_input_dev->name = MODULE_NAME_LIGHT;
	input_set_capability(data->light_input_dev, EV_REL, REL_RED);
	input_set_capability(data->light_input_dev, EV_REL, REL_GREEN);
	input_set_capability(data->light_input_dev, EV_REL, REL_BLUE);
	input_set_capability(data->light_input_dev, EV_REL, REL_WHITE);

	ret = input_register_device(data->light_input_dev);
	if (ret < 0) {
		input_free_device(data->light_input_dev);
		pr_err("%s: could not register input device\n", __func__);
		goto err_input_register_device_light;
	}

	ret = sensors_create_symlink(&data->light_input_dev->dev.kobj,
					data->light_input_dev->name);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink_light;
	}

	ret = sysfs_create_group(&data->light_input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err("[SENSOR]: %s - could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group_light;
	}

	/* light_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->light_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	data->light_timer.function = cm36653_light_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->light_wq = create_singlethread_workqueue("cm36653_light_wq");
	if (!data->light_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR]: %s - could not create light workqueue\n",
			__func__);
		goto err_create_light_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_light, cm36653_work_func_light);

	/* set sysfs for proximity sensor */
	ret = sensors_register(data->cm36653_prox_dev, data, prox_sensor_attrs,
			MODULE_NAME_PROX);
	if (ret) {
		pr_err("[SENSOR]: %s - cound not register\
			proximity sensor device(%d).\n",
			__func__, ret);
		goto prox_sensor_register_failed;
	}

	/* set sysfs for light sensor */
	ret = sensors_register(data->light_dev, data, light_sensor_attrs,
			MODULE_NAME_LIGHT);
	if (ret) {
		pr_err("[SENSOR]: %s - cound not register\
			light sensor device(%d).\n",
			__func__, ret);
		goto light_sensor_register_failed;
	}

	pr_info("[SENSOR] %s is success.\n", __func__);
	goto done;

/* error, unwind it all */
light_sensor_register_failed:
	sensors_unregister(data->cm36653_prox_dev, prox_sensor_attrs);
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
err_input_allocate_device_light:
	destroy_workqueue(data->prox_wq);
err_create_prox_workqueue:
	sysfs_remove_group(&data->prox_input_dev->dev.kobj,
			   &cm36653_prox_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&data->prox_input_dev->dev.kobj,
			data->prox_input_dev->name);
err_sensors_create_symlink_prox:
	input_unregister_device(data->prox_input_dev);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
err_setup_reg:
	wake_lock_destroy(&data->prx_wake_lock);
	cm36653_prox_led_gpiooff(data, false);
	free_irq(data->irq, data);
	gpio_free(data->prox_irq_gpio);
	gpio_free(data->prox_led_gpio);
err_setup_gpio:
err_of_node:
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->power_lock);
	kfree(data);
done:
	return ret;
}

static int cm36653_i2c_remove(struct i2c_client *client)
{
	struct cm36653_p *data = i2c_get_clientdata(client);

	/* free irq */
	if (data->power_state & PROXIMITY_ENABLED) {
		disable_irq_wake(data->irq);
		disable_irq(data->irq);
	}
	free_irq(data->irq, data);
	gpio_free(data->prox_irq_gpio);

	/* device off */
	if (data->power_state & LIGHT_ENABLED)
		cm36653_light_disable(data);
	if (data->power_state & PROXIMITY_ENABLED) {
		cm36653_i2c_write_byte(data, REG_PS_CONF1, 0x01);
		if (data->prox_led_gpio)
			cm36653_prox_led_gpiooff(data, false);
	}

	/* destroy workqueue */
	destroy_workqueue(data->light_wq);
	destroy_workqueue(data->prox_wq);

	/* sysfs destroy */
	sensors_unregister(data->light_dev, light_sensor_attrs);
	sensors_unregister(data->cm36653_prox_dev, prox_sensor_attrs);
	sensors_remove_symlink(&data->light_input_dev->dev.kobj,
			data->light_input_dev->name);
	sensors_remove_symlink(&data->prox_input_dev->dev.kobj,
			data->prox_input_dev->name);

	/* input device destroy */
	sysfs_remove_group(&data->light_input_dev->dev.kobj,
			   &light_attribute_group);
	input_unregister_device(data->light_input_dev);
	sysfs_remove_group(&data->prox_input_dev->dev.kobj,
			   &cm36653_prox_attribute_group);
	input_unregister_device(data->prox_input_dev);

	/* lock destroy */
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->power_lock);
	wake_lock_destroy(&data->prx_wake_lock);

	kfree(data);

	return 0;
}

static int cm36653_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   data->power_state because we use that state in resume.
	 */
	struct cm36653_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED)
		cm36653_light_disable(data);

	return 0;
}

static int cm36653_resume(struct device *dev)
{
	struct cm36653_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED)
		cm36653_light_enable(data);

	return 0;
}

static struct of_device_id cm36653_match_table[] = {
	{ .compatible = "cm36653-i2c",},
	{},
};

static const struct i2c_device_id cm36653_device_id[] = {
	{"cm36653_match_table", 0},
	{}
};

static const struct dev_pm_ops cm36653_pm_ops = {
	.suspend = cm36653_suspend,
	.resume = cm36653_resume
};

static struct i2c_driver cm36653_i2c_driver = {
	.driver = {
		   .name = CHIP_ID,
		   .owner = THIS_MODULE,
		   .of_match_table = cm36653_match_table,
		   .pm = &cm36653_pm_ops
	},
	.probe = cm36653_i2c_probe,
	.remove = cm36653_i2c_remove,
	.id_table = cm36653_device_id,
};

static int __init cm36653_init(void)
{
	return i2c_add_driver(&cm36653_i2c_driver);
}

static void __exit cm36653_exit(void)
{
	i2c_del_driver(&cm36653_i2c_driver);
}

module_init(cm36653_init);
module_exit(cm36653_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("RGB Sensor device driver for cm36653");
MODULE_LICENSE("GPL");