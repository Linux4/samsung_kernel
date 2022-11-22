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

#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/bma254.h>

#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <linux/sensors_core.h>

#define CHIP_DEV_NAME	"BMA254"
#define CHIP_DEV_VENDOR	"BOSCH"
#define CAL_PATH		"/efs/calibration_data"

struct bma254_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct device *dev;
	struct delayed_work work;
	int cal_data[3];
	int position;
	int delay;
	int enable;
};

static const int position_map[][3][3] = {
	{ {-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1} },
	{ { 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1} },
	{ { 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1} },
	{ { 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1} },
	{ { 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1} },
	{ { 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1} },
	{ {-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1} },
	{ { 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1} },
};

static int bma254_i2c_read(struct bma254_data *bma254, u8 reg, unsigned char *rbuf, int len)
{
	int ret = -1;
	struct i2c_msg msg[2];
	struct i2c_client *client = bma254->client;

	if (unlikely((client == NULL) || (!client->adapter)))
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = 1;
	msg[1].len = len;
	msg[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, &msg[0], 2);

	if (unlikely(ret < 0))
		pr_err("%s,i2c transfer error ret=%d\n", __func__, ret);

	return ret;
}

static int bma254_i2c_write(struct bma254_data *bma254, u8 reg, u8 val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 2;
	struct i2c_client *client = bma254->client;

	if (unlikely((client == NULL) || (!client->adapter)))
		return -ENODEV;

	do {
		data[0] = reg;
		data[1] = val;

		msg->addr = client->addr;
		msg->flags = 0;
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(client->adapter, msg, 1);

		if (err >= 0)
			return 0;
	} while (--retry > 0);

	pr_err("%s,i2c transfer error(%d)\n", __func__, err);
	return err;
}

static void bma254_activate(struct bma254_data *bma254, bool enable)
{
	pr_err("%s,acitve(%s)\n", __func__, enable ? "true" : "false");
	if (enable == true) {
		bma254_i2c_write(bma254, BMA254_REG14, SOFT_RESEET);
		msleep(5);

		bma254_i2c_write(bma254, BMA254_REG0F, BMA2X2_RANGE_SET);
		bma254_i2c_write(bma254, BMA254_REG10, BANDWIDTH_31_25);
		bma254_i2c_write(bma254, BMA254_REG11, BMA2X2_MODE_NORMAL);
	} else {
		bma254_i2c_write(bma254, BMA254_REG11, BMA2X2_MODE_SUSPEND);
	}
}

static int bma254_get_data(struct bma254_data *bma254, int *xyz, bool cal)
{

	int ret;
	int i, j;
	int data[3] = {0,};
	u8 sx[6] = {0,};

	ret = bma254_i2c_read(bma254, BMA254_XOUT, sx, 6);
	if (ret < 0)
		return ret;
	for (i = 0; i < 3; i++) {
		data[i] = ((int)((s8)sx[2 * i + 1] << 8 |
			((s8)sx[2 * i] & 0xfe))) >> 4;
	}

	for (i = 0; i < 3; i++) {
		xyz[i] = 0;
		for (j = 0; j < 3; j++)
			xyz[i] += data[j] * position_map[bma254->position][i][j];

		if (cal)
			xyz[i] = (xyz[i] - bma254->cal_data[i]);
	}
	return ret;
}

static void bma254_work_func(struct work_struct *work)
{
	struct bma254_data *bma254 = container_of((struct delayed_work *)work,
			struct bma254_data, work);
	int xyz[3] = {0,};
	int ret;

	if (bma254 == NULL) {
		pr_err("%s, NULL point\n", __func__);
		return;
	}

	ret = bma254_get_data(bma254, xyz, true);
	if (ret < 0) {
		pr_err("%s, data error(%d)\n", __func__, ret);
	} else {
#ifdef REPORT_ABS
		input_report_abs(bma254->input, ABS_X, xyz[0]);
		input_report_abs(bma254->input, ABS_Y, xyz[1]);
		input_report_abs(bma254->input, ABS_Z, xyz[2]);
#else
		input_report_rel(bma254->input, REL_X, xyz[0] < 0 ? xyz[0] : xyz[0] + 1);
		input_report_rel(bma254->input, REL_Y, xyz[1] < 0 ? xyz[1] : xyz[1] + 1);
		input_report_rel(bma254->input, REL_Z, xyz[2] < 0 ? xyz[2] : xyz[2] + 1);
#endif
		input_sync(bma254->input);
	}

	if (bma254->enable)
		schedule_delayed_work(&bma254->work, msecs_to_jiffies(bma254->delay));
}

static int bma254_open_calibration(struct bma254_data *bma254)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CAL_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&bma254->cal_data, 3 * sizeof(s32), &cal_filp->f_pos);
	if (err != 3 * sizeof(s32)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d,%d,%d)\n", __func__,
		bma254->cal_data[0], bma254->cal_data[1], bma254->cal_data[2]);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static ssize_t bma254_enable_store(struct device *dev,
			   struct device_attribute *attr,
		       const char *buf, size_t size)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);

	int value = 0;
	int err = 0;

	err = kstrtoint(buf, 10, &value);
	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	if (value != 0 && value != 1) {
		pr_err("%s,wrong value(%d)\n", __func__, value);
		goto done;
	}

	if (bma254->enable != value) {
		pr_info("%s, enable(%d)\n", __func__, value);
		if (value) {
			bma254_activate(bma254, true);
			bma254_open_calibration(bma254);
			schedule_delayed_work(&bma254->work, msecs_to_jiffies(bma254->delay));
		} else {
			cancel_delayed_work_sync(&bma254->work);
			bma254_activate(bma254, false);
		}
		bma254->enable = value;
	} else {
		pr_err("%s, wrong cmd for enable\n", __func__);
	}
done:
	return size;
}

static ssize_t bma254_enable_show(struct device *dev,
			  struct device_attribute *attr,
		      char *buf)
{
	struct bma254_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->enable);
}

static ssize_t bma254_delay_store(struct device *dev,
			   struct device_attribute *attr,
		       const char *buf, size_t size)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);

	int value = 0;
	int err = 0;

	err = kstrtoint(buf, 10, &value);
	if (err) {
		pr_err("%s, kstrtoint failed\n", __func__);
		goto done;
	}
	if (value < 0 || 200 < value) {
		pr_err("%s,wrong value(%d)\n", __func__, value);
		goto done;
	}

	if (bma254->delay != value) {
		if (bma254->enable)
			cancel_delayed_work_sync(&bma254->work);

		bma254->delay = value;

		if (bma254->enable)
			schedule_delayed_work(&bma254->work, msecs_to_jiffies(bma254->delay));
	} else {
		pr_err("%s, same delay\n", __func__);
	}
	pr_info("%s,delay %d\n", __func__, bma254->delay);
done:
	return size;
}

static ssize_t bma254_delay_show(struct device *dev,
			  struct device_attribute *attr,
		      char *buf)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", bma254->delay);
}

static DEVICE_ATTR(enable,
		   S_IRUGO|S_IWUSR|S_IWGRP,
		   bma254_enable_show,
		   bma254_enable_store
		   );
static DEVICE_ATTR(delay,
		   S_IRUGO|S_IWUSR|S_IWGRP,
		   bma254_delay_show,
		   bma254_delay_store
		   );

static struct attribute *bma254_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	NULL
};

static struct attribute_group bma254_attribute_group = {
	.attrs = bma254_attributes
};

static int bma254_do_calibrate(struct bma254_data *bma254, int enable)
{
	struct file *cal_filp;
	int sum[3] = { 0, };
	int err;
	mm_segment_t old_fs;

	if (enable) {
                int data[3] = { 0, };
                int i;
		for (i= 0; i < 100; i++) {
			err = bma254_get_data(bma254, data, false);
			if (err < 0) {
				pr_err("%s : failed in the %dth loop\n", __func__, i);
				return err;
			}

			sum[0] += data[0];
			sum[1] += data[1];
			sum[2] += data[2];
		}

		bma254->cal_data[0] = (sum[0] / 100);
		bma254->cal_data[1] = (sum[1] / 100);
		bma254->cal_data[2] = ((sum[2] / 100) - 1024);
	} else {
		bma254->cal_data[0] = 0;
		bma254->cal_data[1] = 0;
		bma254->cal_data[2] = 0;
	}

	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
			bma254->cal_data[0], bma254->cal_data[1],
				bma254->cal_data[2]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CAL_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&bma254->cal_data, 3 * sizeof(s32), &cal_filp->f_pos);
	if (err != 3 * sizeof(s32)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}


static ssize_t bma254_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}

static ssize_t bma254_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}

static ssize_t bma254_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);
	int xyz[3] = {0, };
	int ret;

	if (!bma254->enable) {
		bma254_activate(bma254, true);
		schedule_delayed_work(&bma254->work, msecs_to_jiffies(bma254->delay));
	}

	ret = bma254_get_data(bma254, xyz, true);
	if (ret < 0) {
		pr_err("%s, data error(%d)\n", __func__, ret);
	}
	pr_info("bma254_raw_data_show %d %d %d\n", xyz[0], xyz[1], xyz[2]);
	if (!bma254->enable) {
		cancel_delayed_work_sync(&bma254->work);
		bma254_activate(bma254, false);
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		xyz[0], xyz[1], xyz[2]);
}

static ssize_t bma254_calibartion_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);
	int ret;

	ret = bma254_open_calibration(bma254);
	if (ret < 0) {
		pr_err("%s, bma254_open_calibration(%d)\n", __func__, ret);
	}
	if (bma254->cal_data[0] == 0 &&
		bma254->cal_data[1] == 0 &&
		bma254->cal_data[2] == 0)
		ret = 0;
	else
		ret = 1;
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", ret,
		bma254->cal_data[0], bma254->cal_data[1], bma254->cal_data[2]);
}

static ssize_t bma254_calibartion_store(struct device *dev,
	struct device_attribute *attr,const char *buf, size_t count)
{
	struct bma254_data *bma254 = dev_get_drvdata(dev);
	int value;
	int err;
	char tmp[64];

	err = kstrtoint(buf, 10, &value);
	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		return -EINVAL;
	}

	err = bma254_do_calibrate(bma254, value);
	if (err < 0)
		pr_err("%s, bma254_do_calibrate(%d)\n", __func__, err);
	else
		err = 0;
	count = sprintf(tmp, "%d\n", err);
	return count;
}

static DEVICE_ATTR(name, 0440, bma254_name_show, NULL);
static DEVICE_ATTR(vendor, 0440, bma254_vendor_show, NULL);
static DEVICE_ATTR(raw_data, 0440, bma254_raw_data_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	bma254_calibartion_show, bma254_calibartion_store);

static struct device_attribute *bma254_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_raw_data,
	&dev_attr_calibration,
	NULL,
};
#ifdef CONFIG_SENSORS_POWERCONTROL
static int bma254_regulator_onoff(struct device *dev, bool onoff)
{
	struct regulator *bma_vdd;

	pr_info("%s %s\n", __func__, (onoff) ? "on" : "off");

	bma_vdd = devm_regulator_get(dev, "bma254-vdd");
	if (IS_ERR(bma_vdd)) {
		pr_err("%s: cannot get bma_vdd\n", __func__);
		return -ENOMEM;
	}

	if (onoff) {
		regulator_enable(bma_vdd);
	} else {
		regulator_disable(bma_vdd);
	}

	devm_regulator_put(bma_vdd);
	msleep(10);

	return 0;
}
#endif
static int bma254_parse_dt(struct bma254_data *data, struct device *dev)
{
	struct device_node *this_node= dev->of_node;
	u32 temp;

	if (this_node == NULL) {
		pr_err("%s,this_node is empty\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_u32(this_node, "bma254,position", &temp) < 0) {
		pr_err("%s,get position(%d) error\n", __func__, temp);
		return -ENODEV;
	} else {
		data->position = (u8)temp;
		pr_info("%s, position(%d)\n", __func__, data->position);
		return 0;
	}
}

static int bma254_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct bma254_data *bma254;
	struct input_dev *dev;

	pr_info("%s, is called\n", __func__);

	if (client == NULL) {
		pr_err("%s, client doesn't exist\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
#ifdef CONFIG_SENSORS_POWERCONTROL
	ret = bma254_regulator_onoff(&client->dev, true);
	if (ret) {
		pr_err("%s, Power Up Failed\n", __func__);
		return ret;
	}
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s,client not i2c capable\n", __func__);
		return -ENOMEM;
	}

	ret = i2c_smbus_read_word_data(client, BMA254_CHIP_ID);
	if ((ret & 0x00ff) != 0xfa) {
		pr_err("%s,i2c failed(%x)\n", __func__, ret & 0x00ff);
		ret = -ENOMEM;
		return ret;
	} else {
		pr_err("%s,chip id %x\n", __func__, ret & 0x00ff);
	}

	bma254 = kzalloc(sizeof(struct bma254_data), GFP_KERNEL);
	if (!bma254) {
		pr_err("%s, kzalloc error\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	ret = bma254_parse_dt(bma254, &client->dev);
	if (ret) {
		pr_err("%s, get gpio is failed\n", __func__);
		goto parse_dt_err;
	}

	i2c_set_clientdata(client, bma254);
	bma254->client = client;

	bma254_activate(bma254, false);

	dev = input_allocate_device();
	if (!dev){
		goto input_allocate_device_err;
	}
	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;
#ifdef REPORT_ABS
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);
#else
	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
#endif
	input_set_drvdata(dev, bma254);

	ret = input_register_device(dev);
	if (ret < 0) {
		pr_err("%s,sysfs_create_group failed\n", __func__);
		goto input_register_device_err;
	}
	bma254->input = dev;

	ret = sysfs_create_group(&bma254->input->dev.kobj,
			&bma254_attribute_group);
	if (ret < 0) {
		pr_err("%s,sysfs_create_group failed\n", __func__);
		goto sysfs_create_group_err;
	}

	INIT_DELAYED_WORK(&bma254->work, bma254_work_func);
	bma254->delay = MAX_DELAY;
	bma254->enable = 0;

	ret = sensors_register(bma254->dev, bma254,
		bma254_attrs, "accelerometer_sensor");
	if (ret < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto sensors_register_err;
	}

	goto exit;
sensors_register_err:
	sysfs_remove_group(&bma254->input->dev.kobj,
			&bma254_attribute_group);
sysfs_create_group_err:
	input_unregister_device(bma254->input);
input_register_device_err:
	input_free_device(dev);
input_allocate_device_err:
parse_dt_err:
	kfree(bma254);
#ifdef CONFIG_SENSORS_POWERCONTROL
	bma254_regulator_onoff(&client->dev, false);
#endif
exit:
	return ret;
}

static int bma254_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma254_data *bma254 = i2c_get_clientdata(client);

	pr_info("%s, enable(%d)\n", __func__, bma254->enable);
	if (bma254->enable) {
		cancel_delayed_work_sync(&bma254->work);
		bma254_activate(bma254, false);
	}

	return 0;
}

static int bma254_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma254_data *bma254 = i2c_get_clientdata(client);

	pr_info("%s, enable(%d)\n", __func__, bma254->enable);
	if (bma254->enable) {
		bma254_activate(bma254, true);
		schedule_delayed_work(&bma254->work, msecs_to_jiffies(bma254->delay));
	}

	return 0;
}

static const struct dev_pm_ops bma254_dev_pm_ops = {
	.suspend = bma254_suspend,
	.resume = bma254_resume,
};

static const u16 normal_i2c[] = { I2C_CLIENT_END };

static const struct i2c_device_id bma254_device_id[] = {
	{"bma254", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, bma254_device_id);

static struct of_device_id bma254_match_table[] = {
	{ .compatible = "bma254",},
	{},
};

MODULE_DEVICE_TABLE(of, bma254_match_table);

static struct i2c_driver bma254_i2c_driver = {
	.driver = {
		.name = "bma254",
		.owner = THIS_MODULE,
		.of_match_table = bma254_match_table,
		.pm = &bma254_dev_pm_ops,
	},
	.probe		= bma254_probe,
	.id_table	= bma254_device_id,
	.address_list = normal_i2c,
};

module_i2c_driver(bma254_i2c_driver);

MODULE_AUTHOR("daehan.wi@samsung.com");
MODULE_DESCRIPTION("G-Sensor Driver for BMA254");
MODULE_LICENSE("GPL");
