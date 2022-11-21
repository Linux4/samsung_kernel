/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "sensors_core.h"

#define VENDOR_NAME         "ROHM"
#define MODEL_NAME          "BH1733"
#define MODULE_NAME         "light_sensor"

#define SENSOR_BH1733_ADDR  0x29
#define SET_CMD             0x80
#define LUX_MIN_VALUE       0
#define LUX_MAX_VALUE       65528

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

enum BH1733_REG {
	REG_CON = 0x00,
	REG_TIME,
	REG_GAIN = 0x07,
	REG_ID = 0x12,
	REG_DATA0L = 0x14,
	REG_DATA0H,
	REG_DATA1L,
	REG_DATA1H,
};

enum BH1733_CMD {
	CMD_PWR_OFF = 0,
	CMD_PWR_ON,
	CMD_TIME_50MS,
	CMD_TIME_100MS,
	CMD_TIME_120MS,
	CMD_TIME_200MS,
	CMD_GAIN_X2,
	CMD_GAIN_X64,
	CMD_GAIN_X128,
};

enum BH1733_I2C_IF {
	REG = 0,
	CMD,
};

static const u8 cmds[9][2] = {
	{REG_CON, 0x00}, /* Power Down */
	{REG_CON, 0x03}, /* Power On */
	{REG_TIME, 0xED}, /* Timing 50ms */
	{REG_TIME, 0xDA}, /* Timing 100ms */
	{REG_TIME, 0xD3}, /* Timing 120ms */
	{REG_TIME, 0xB6}, /* Timing 200ms */
	{REG_GAIN, 0x01}, /* Gain x2 */
	{REG_GAIN, 0x02}, /* Gain x64 */
	{REG_GAIN, 0x03} /* Gain x128 */
};

static const u16 integ_time[4] = {50, 100, 120, 200};
static const u16 gain[3] = {2, 64, 128};

enum BH1733_GAIN {
	X2 = 0,
	X64,
	X128,
};

enum BH1733_TIME {
	T50MS = 0,
	T100MS,
	T120MS,
	T200MS,
};

enum BH1733_CASE {
	CASE_NORMAL = 1,
	CASE_LOW,
};

#define I2C_M_WR                  0 /* for i2c Write */
#define I2c_M_RD                  1 /* for i2c Read */
#define LIGHT_LOG_TIME            15 /* 15 sec */
#define BH1733_DEFAULT_DELAY      200000000LL

struct bh1733_p {
	struct i2c_client *i2c_client;
	struct input_dev *input;
	struct device *light_dev;
	struct delayed_work work;
	atomic_t delay;
	u8 power_state;
	u16 gain;
	u16 time;
	u16 raw_data[2];
	int irq;
	int time_count;
	int check_case;
};

static int bh1733_i2c_read_byte(struct bh1733_p *data,
		unsigned char reg_addr, u8 *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->i2c_client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read error %d\n", __func__, ret);

		return ret;
	}

	return 0;
}

static int bh1733_i2c_read_word(struct bh1733_p *data,
		unsigned char reg_addr, u16 *buf)
{
	int ret;
	struct i2c_msg msg[2];
	unsigned char r_buf[4];

	msg[0].addr = data->i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 4;
	msg[1].buf = r_buf;

	ret = i2c_transfer(data->i2c_client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - i2c read error %d\n", __func__, ret);

		return ret;
	}

	buf[0] = (u16)r_buf[1];
	buf[0] = (buf[0] << 8) | (u16)r_buf[0];

	buf[1] = (u16)r_buf[3];
	buf[1] = (buf[1] << 8) | (u16)r_buf[2];

	return 0;
}

static int bh1733_i2c_write(struct bh1733_p *data,
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

static void bh1733_set_gain_timing(struct bh1733_p *data, int num)
{
	switch (num) {
	case CASE_NORMAL:
		bh1733_i2c_write(data, SET_CMD | cmds[CMD_GAIN_X2][REG],
			cmds[CMD_GAIN_X2][CMD]);
		bh1733_i2c_write(data, SET_CMD | cmds[CMD_TIME_120MS][REG],
			cmds[CMD_TIME_120MS][CMD]);
		data->gain = gain[X2];
		data->time = integ_time[T120MS];
		break;
	case CASE_LOW:
		bh1733_i2c_write(data, SET_CMD | cmds[CMD_GAIN_X64][REG],
			cmds[CMD_GAIN_X64][CMD]);
		bh1733_i2c_write(data, SET_CMD | cmds[CMD_TIME_100MS][REG],
			cmds[CMD_TIME_100MS][CMD]);
		data->gain = gain[X64];
		data->time = integ_time[T100MS];
		break;
	default:
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, num);
	}
}

static int bh1733_get_luxvalue(struct bh1733_p *data)
{
	int ret;

	ret = bh1733_i2c_read_word(data, SET_CMD | REG_DATA0L, data->raw_data);
	if (ret < 0)
		return ret;

	switch (data->check_case) {
	case CASE_NORMAL:
		if (data->raw_data[0] < 80 &&
			data->raw_data[1] < 1000)
			data->check_case = CASE_LOW;
		else if (data->raw_data[0] < 1000 &&
			data->raw_data[1] < 80)
			data->check_case = CASE_LOW;
		else
			return 0;
		break;
	case CASE_LOW:
		if (data->raw_data[0] < 32000 && data->raw_data[1] < 32000)
			return 0;
		else
			data->check_case = CASE_NORMAL;
		break;
	default:
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__,
			data->check_case);
		break;
	}

	bh1733_set_gain_timing(data, data->check_case);
	return 0;
}

static void bh1733_work_func_light(struct work_struct *work)
{
	struct bh1733_p *data = container_of((struct delayed_work *)work,
			struct bh1733_p, work);
	unsigned long delay = nsecs_to_jiffies(atomic_read(&data->delay));
	u32 result = (data->gain << 16) | data->time;
	int ret;

	ret = bh1733_get_luxvalue(data);
	if (ret >= 0) {
		input_report_rel(data->input, REL_MISC, data->raw_data[0] + 1);
		input_report_rel(data->input, REL_WHEEL, data->raw_data[1] + 1);
		input_report_rel(data->input, REL_HWHEEL, result);
		input_sync(data->input);
	} else
		pr_err("[SENSOR]: %s - read word failed! (ret=%d)\n",
			__func__, ret);

	if (((int64_t)atomic_read(&data->delay) * (int64_t)data->time_count)
		>= ((int64_t)LIGHT_LOG_TIME * NSEC_PER_SEC)) {
		pr_info("[SENSOR]: %s - raw_data0 = %u raw_data1 = %u\n",
			__func__, data->raw_data[0], data->raw_data[1]);
		data->time_count = 0;
	} else {
		data->time_count++;
	}

	schedule_delayed_work(&data->work, delay);
}

static void bh1733_light_enable(struct bh1733_p *data)
{
	bh1733_i2c_write(data, SET_CMD | cmds[CMD_PWR_ON][REG],
		cmds[CMD_PWR_ON][CMD]);

	data->check_case = CASE_NORMAL;
	bh1733_set_gain_timing(data, CASE_NORMAL);

	schedule_delayed_work(&data->work,
		nsecs_to_jiffies(atomic_read(&data->delay)));
}

static void bh1733_light_disable(struct bh1733_p *data)
{
	cancel_delayed_work_sync(&data->work);
	bh1733_i2c_write(data, SET_CMD | cmds[CMD_PWR_OFF][REG],
		cmds[CMD_PWR_OFF][CMD]);
}

/* sysfs */
static ssize_t bh1733_poll_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->delay));
}

static ssize_t bh1733_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct bh1733_p *data = dev_get_drvdata(dev);
	int64_t new_delay;
	int ret;

	ret = kstrtoll(buf, 10, &new_delay);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	atomic_set(&data->delay, new_delay);
	pr_info("[SENSOR]: %s - poll_delay = %lld\n", __func__, new_delay);

	return size;
}

static ssize_t bh1733_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct bh1733_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);
	if (enable && !(data->power_state & LIGHT_ENABLED)) {
		data->power_state |= LIGHT_ENABLED;
		bh1733_light_enable(data);
	} else if (!enable && (data->power_state & LIGHT_ENABLED)) {
		bh1733_light_disable(data);
		data->power_state &= ~LIGHT_ENABLED;
	}

	return size;
}

static ssize_t bh1733_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(data->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		bh1733_poll_delay_show, bh1733_poll_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		bh1733_enable_show, bh1733_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL,
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

/* light sysfs */
static ssize_t bh1733_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t bh1733_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u\n",
			data->raw_data[0], data->raw_data[1]);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u\n",
			data->raw_data[0], data->raw_data[1]);
}

static DEVICE_ATTR(name, S_IRUGO, bh1733_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, bh1733_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static int bh1733_chip_id(struct bh1733_p *data)
{
	int ret = 0;
	u8 reg;

	pr_info("[SENSOR]: %s - POWER ON %x, %x\n", __func__,
		cmds[CMD_PWR_ON][REG], cmds[CMD_PWR_ON][CMD]);
	ret = bh1733_i2c_write(data, SET_CMD | cmds[CMD_PWR_ON][REG],
		cmds[CMD_PWR_ON][CMD]);
	if (ret < 0)
		goto err_exit;

	ret = bh1733_i2c_read_byte(data, SET_CMD | REG_ID, &reg);
	if (ret < 0) {
		pr_err("%s : failed to read\n", __func__);
		ret = -EIO;
		goto err_exit;
	}

	pr_info("[SENSOR]: %s - PART_ID_REG = %x\n", __func__, reg);
	ret = bh1733_i2c_write(data, SET_CMD | cmds[CMD_PWR_OFF][REG],
		cmds[CMD_PWR_OFF][CMD]);
	if (ret < 0)
		pr_err("[SENSOR]: %s - Failed to write byte (CMD_PWR_OFF)\n",
			__func__);
err_exit:
	return ret;
}

static int bh1733_input_init(struct bh1733_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate lightsensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_capability(dev, EV_REL, REL_WHEEL);
	input_set_capability(dev, EV_REL, REL_HWHEEL);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(data->input);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &light_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&data->input->dev.kobj,
			data->input->name);
		input_unregister_device(dev);
		return ret;
	}

	data->input = dev;
	return 0;
}

static int bh1733_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct bh1733_p *data = NULL;

	pr_info("[SENSOR]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	data = kzalloc(sizeof(struct bh1733_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SENSOR]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	data->i2c_client = client;
	i2c_set_clientdata(client, data);

	/* Check if the device is there or not. */
	ret = bh1733_chip_id(data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - bh1733_chip_id failed\n", __func__);
		goto exit_i2c_failed;
	}

	/* input device init */
	ret = bh1733_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	atomic_set(&data->delay, BH1733_DEFAULT_DELAY);
	data->time_count = 0;
	data->check_case = CASE_NORMAL;

	INIT_DELAYED_WORK(&data->work, bh1733_work_func_light);

	/* set sysfs for light sensor */
	sensors_register(data->light_dev, data, sensor_attrs, MODULE_NAME);
	pr_info("[SENSOR]: %s - Probe done!\n", __func__);

	return 0;

exit_input_init:
exit_i2c_failed:
	kfree(data);
#ifdef CONFIG_SEC_RUBENS_PROJECT
	sensor_power_on_vdd(data,0);
#endif
exit_kzalloc:
exit:
	pr_err("[SENSOR]: %s - Probe fail!\n", __func__);
	return ret;
}

static void bh1733_shutdown(struct i2c_client *client)
{
	struct bh1733_p *data = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (data->power_state & LIGHT_ENABLED)
		bh1733_light_disable(data);
}

static int bh1733_remove(struct i2c_client *client)
{
	struct bh1733_p *data = i2c_get_clientdata(client);

	/* device off */
	if (data->power_state & LIGHT_ENABLED)
		bh1733_light_disable(data);

	/* sysfs destroy */
	sensors_unregister(data->light_dev, sensor_attrs);
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);

	/* input device destroy */
	sysfs_remove_group(&data->input->dev.kobj, &light_attribute_group);
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}

static int bh1733_suspend(struct device *dev)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		bh1733_light_disable(data);
	}

	return 0;
}

static int bh1733_resume(struct device *dev)
{
	struct bh1733_p *data = dev_get_drvdata(dev);

	if (data->power_state & LIGHT_ENABLED) {
		pr_info("[SENSOR]: %s\n", __func__);
		bh1733_light_enable(data);
	}

	return 0;
}

static struct of_device_id bh1733_match_table[] = {
	{ .compatible = "bh1733-i2c",},
	{},
};

static const struct i2c_device_id bh1733_device_id[] = {
	{ "bh1733_match_table", 0 },
	{}
};

static const struct dev_pm_ops bh1733_pm_ops = {
	.suspend	= bh1733_suspend,
	.resume		= bh1733_resume,
};

static struct i2c_driver bh1733_i2c_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = bh1733_match_table,
		.pm = &bh1733_pm_ops
	},
	.probe		= bh1733_probe,
	.shutdown	= bh1733_shutdown,
	.remove		= bh1733_remove,
	.id_table	= bh1733_device_id,
};

static int __init bh1733_init(void)
{
	return i2c_add_driver(&bh1733_i2c_driver);
}

static void __exit bh1733_exit(void)
{
	i2c_del_driver(&bh1733_i2c_driver);
}

module_init(bh1733_init);
module_exit(bh1733_exit);

MODULE_DESCRIPTION("Light Sensor device driver for bh1733");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
