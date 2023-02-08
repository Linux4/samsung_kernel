/*
 *  STMicroelectronics k2dh acceleration sensor driver
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/k2dh.h>
#include "k2dh_reg.h"

/* For Debugging */
#if 1
#define k2dh_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)
#endif
#define k2dh_infomsg(str, args...) pr_info("%s: " str, __func__, ##args)

#define VENDOR		"STM"
#define CHIP_ID		"K2DH"

/* The default settings when sensor is on is for all 3 axis to be enabled
 * and output data rate set to 400Hz.  Output is via a ioctl read call.
 */
#define DEFAULT_POWER_ON_SETTING (ODR400 | ENABLE_ALL_AXES)
#define ACC_DEV_MAJOR 241

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"
#define CAL_DATA_AMOUNT	20
#define Z_CAL_SIZE	1024

static const struct odr_delay {
	u8 odr; /* odr reg setting */
	s64 delay_ns; /* odr in ns */
} odr_delay_table[] = {
	{ ODR1344,     744047LL }, /* 1344Hz */
	{  ODR400,    2500000LL }, /*  400Hz */
	{  ODR200,    5000000LL }, /*  200Hz */
	{  ODR100,   10000000LL }, /*  100Hz */
	{   ODR50,   20000000LL }, /*   50Hz */
	{   ODR25,   40000000LL }, /*   25Hz */
	{   ODR10,  100000000LL }, /*   10Hz */
	{    ODR1, 1000000000LL }, /*    1Hz */
};

/* k2dh acceleration data */
struct k2dh_acc {
	s16 x;
	s16 y;
	s16 z;
};

struct k2dh_data {
	struct i2c_client *client;
	struct miscdevice k2dh_device;
	struct mutex read_lock;
	struct mutex write_lock;
	struct completion data_ready;
	struct class *acc_class;
	struct device *dev;
	struct k2dh_acc cal_data;
	struct k2dh_acc acc_xyz;
	u8 ctrl_reg1_shadow;
	atomic_t opened; /* opened implies enabled */
	struct accel_platform_data *acc_pdata;
};

static struct k2dh_data *g_k2dh;
static s64 k2dh_get_delay(struct k2dh_data *data);
static int k2dh_set_delay(struct k2dh_data *data, s64 delay_ns);


static void k2dh_xyz_position_adjust(struct k2dh_acc *acc,
		int position)
{

	const int position_map[][3][3] = {
	{{ 0, 1,  0}, { -1,  0,  0}, { 0,  0,  1} }, /* 0 top/upper-left */
	{{ -1,  0,  0}, { 0,  -1,  0}, { 0,  0,  1} }, /* 1 top/upper-right */
	{{ 0,  -1,  0}, {1,  0,  0}, { 0,  0,  1} }, /* 2 top/lower-right */
	{{1,  0,  0}, { 0, 1,  0}, { 0,  0,  1} }, /* 3 top/lower-left */
	{{ 0,  -1,  0}, { -1,  0,  0}, { 0,  0, -1} }, /* 4 bottom/upper-left */
	{{1,  0,  0}, { 0,  -1,  0}, { 0,  0, -1} }, /* 5 bottom/upper-right */
	{{ 0, 1,  0}, {1,  0,  0}, { 0,  0, -1} }, /* 6 bottom/lower-right */
	{{ -1,  0,  0}, { 0, 1,  0}, { 0,  0, -1} }, /* 7 bottom/lower-left*/
	};
	struct k2dh_acc xyz_adjusted = {0,};
	s16 raw[3] = {0,};
	int j;
	raw[0] = acc->x;
	raw[1] = acc->y;
	raw[2] = acc->z;
	for (j = 0; j < 3; j++) {
		xyz_adjusted.x +=
		(position_map[position][0][j] * raw[j]);
		xyz_adjusted.y +=
		(position_map[position][1][j] * raw[j]);
		xyz_adjusted.z +=
		(position_map[position][2][j] * raw[j]);
	}
	acc->x = xyz_adjusted.x;
	acc->y = xyz_adjusted.y;
	acc->z = xyz_adjusted.z;
}

#define BUF_DEPTH 12
/* Read X,Y and Z-axis acceleration raw data */
static int k2dh_read_accel_raw_xyz(struct k2dh_data *data,
				struct k2dh_acc *acc)
{
	int i;
	u8 acc_data[6];

	int total_x = 0, total_y = 0, total_z = 0;
	static int pre_x, pre_y, pre_z;
	static int buff_x[BUF_DEPTH];
	static int buff_y[BUF_DEPTH];
	static int buff_z[BUF_DEPTH];
	static int count;
	static s16 delay;

	for (i = 0; i < 6; i++)
		acc_data[i] = i2c_smbus_read_byte_data(data->client,
					OUT_X_L + i);

	acc->x = (acc_data[1] << 8) | acc_data[0];
	acc->y = (acc_data[3] << 8) | acc_data[2];
	acc->z = (acc_data[5] << 8) | acc_data[4];

	acc->x = acc->x >> 4;
	acc->y = acc->y >> 4;
	acc->z = acc->z >> 4;

	if(*(g_k2dh->acc_pdata->vibrator_on) > 0) {
		if (delay == 0) {
			delay = k2dh_get_delay(data);
			pr_info("%s : vibrator_on = 1, delay = %d\n", __func__,
				delay);
			k2dh_set_delay(data, 10000000);
			for (i = 0; i < BUF_DEPTH; i++) {
				buff_x[i] = pre_x;
				buff_y[i] = pre_y;
				buff_z[i] = pre_z;
			}
			count = 0;
		}

		buff_x[count] = acc->x;
		buff_y[count] = acc->y;
		buff_z[count] = acc->z;

		if(count++ >= BUF_DEPTH)
			count = 0;
	
		for (i = 0; i < BUF_DEPTH; i++) {
			total_x += buff_x[i];
			total_y += buff_y[i];
			total_z += buff_z[i];
		}

		acc->x = total_x / BUF_DEPTH;
		acc->y = total_y / BUF_DEPTH;
		acc->z = total_z / BUF_DEPTH;
	} else {
		if (delay) {
			pr_info("%s : vibrator_on = 0, delay = %d\n", __func__,
				delay);
			k2dh_set_delay(data, delay);
			delay = 0;
		}
		pre_x = acc->x;
		pre_y = acc->y;
		pre_z = acc->z;
	}

	if (g_k2dh->acc_pdata && g_k2dh->acc_pdata->axis_adjust)
		k2dh_xyz_position_adjust(acc,
			g_k2dh->acc_pdata->accel_get_position());
	return 0;
}

static int k2dh_read_accel_xyz(struct k2dh_data *data,
				struct k2dh_acc *acc)
{
	int err = 0;

	mutex_lock(&data->read_lock);
	err = k2dh_read_accel_raw_xyz(data, acc);
	mutex_unlock(&data->read_lock);
	if (err < 0) {
		pr_err("%s: k2dh_read_accel_raw_xyz() failed\n", __func__);
		return err;
	}

	acc->x -= data->cal_data.x;
	acc->y -= data->cal_data.y;
	acc->z -= data->cal_data.z;

	return err;
}

static int k2dh_open_calibration(struct k2dh_data *data)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		err = PTR_ERR(cal_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	k2dh_dbgmsg("%s: (%u,%u,%u)\n", __func__,
		data->cal_data.x, data->cal_data.y, data->cal_data.z);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int k2dh_do_calibrate(struct device *dev, bool do_calib)
{
	struct k2dh_data *acc_data = dev_get_drvdata(dev);
	struct k2dh_acc data = { 0, };
	struct file *cal_filp = NULL;
	int sum[3] = { 0, };
	int err = 0;
	int i;
	s16 z_cal;
	mm_segment_t old_fs;

	if (do_calib) {
		for (i = 0; i < CAL_DATA_AMOUNT; i++) {
			mutex_lock(&acc_data->read_lock);
			err = k2dh_read_accel_raw_xyz(acc_data, &data);
			mutex_unlock(&acc_data->read_lock);
			if (err < 0) {
				pr_err("%s: k2dh_read_accel_raw_xyz() "
					"failed in the %dth loop\n",
					__func__, i);
				return err;
			}

			sum[0] += data.x;
			sum[1] += data.y;
			sum[2] += data.z;
		}

		acc_data->cal_data.x = sum[0] / CAL_DATA_AMOUNT;
		acc_data->cal_data.y = sum[1] / CAL_DATA_AMOUNT;
		z_cal = sum[2] / CAL_DATA_AMOUNT;

		if (z_cal > 0)
			acc_data->cal_data.z = z_cal - Z_CAL_SIZE;
		else
			acc_data->cal_data.z = z_cal + Z_CAL_SIZE;

	} else {
		acc_data->cal_data.x = 0;
		acc_data->cal_data.y = 0;
		acc_data->cal_data.z = 0;
	}

	printk(KERN_INFO "%s: cal data (%d,%d,%d)\n", __func__,
	      acc_data->cal_data.x, acc_data->cal_data.y, acc_data->cal_data.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&acc_data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int k2dh_accel_enable(struct k2dh_data *data)
{
	int err = 0;

	if (atomic_read(&data->opened) == 0) {
		err = k2dh_open_calibration(data);
		if (err < 0 && err != -ENOENT)
			pr_err("%s: k2dh_open_calibration() failed\n",
				__func__);
		data->ctrl_reg1_shadow = DEFAULT_POWER_ON_SETTING;
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
						DEFAULT_POWER_ON_SETTING);
		if (err)
			pr_err("%s: i2c write ctrl_reg1 failed\n", __func__);

		err = i2c_smbus_write_byte_data(data->client, CTRL_REG4,
				CTRL_REG4_HR | CTRL_REG4_BDU);

		if (err)
			pr_err("%s: i2c write ctrl_reg4 failed\n", __func__);
	}

	atomic_add(1, &data->opened);

	return err;
}

static int k2dh_accel_disable(struct k2dh_data *data)
{
	int err = 0;

	atomic_sub(1, &data->opened);
	if (atomic_read(&data->opened) == 0) {
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
								PM_OFF);
		data->ctrl_reg1_shadow = PM_OFF;
	}

	return err;
}

/*  open command for k2dh device file  */
static int k2dh_open(struct inode *inode, struct file *file)
{
	k2dh_infomsg("is called.\n");
	return 0;
}

/*  release command for k2dh device file */
static int k2dh_close(struct inode *inode, struct file *file)
{
	k2dh_infomsg("is called.\n");
	return 0;
}

static s64 k2dh_get_delay(struct k2dh_data *data)
{
	int i;
	u8 odr;
	s64 delay = -1;

	odr = data->ctrl_reg1_shadow & ODR_MASK;
	for (i = 0; i < ARRAY_SIZE(odr_delay_table); i++) {
		if (odr == odr_delay_table[i].odr) {
			delay = odr_delay_table[i].delay_ns;
			break;
		}
	}
	return delay;
}

static int k2dh_set_delay(struct k2dh_data *data, s64 delay_ns)
{
	int odr_value = ODR1;
	int res = 0;
	int i;

	/* round to the nearest delay that is less than
	 * the requested value (next highest freq)
	 */
	for (i = 0; i < ARRAY_SIZE(odr_delay_table); i++) {
		if (delay_ns < odr_delay_table[i].delay_ns)
			break;
	}
	if (i > 0)
		i--;
	odr_value = odr_delay_table[i].odr;
	delay_ns = odr_delay_table[i].delay_ns;

	k2dh_infomsg("old=%lldns, new=%lldns, odr=0x%x/0x%x\n",
		k2dh_get_delay(data), delay_ns, odr_value,
			data->ctrl_reg1_shadow);
	mutex_lock(&data->write_lock);
	if (odr_value != (data->ctrl_reg1_shadow & ODR_MASK)) {
		u8 ctrl = (data->ctrl_reg1_shadow & ~ODR_MASK);
		ctrl |= odr_value;
		data->ctrl_reg1_shadow = ctrl;
		res = i2c_smbus_write_byte_data(data->client, CTRL_REG1, ctrl);
	}
	mutex_unlock(&data->write_lock);
	return res;
}

/*  ioctl command for k2dh device file */
static long k2dh_ioctl(struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct k2dh_data *data = container_of(file->private_data,
		struct k2dh_data, k2dh_device);
	s64 delay_ns;
	int enable = 0;

	/* cmd mapping */
	switch (cmd) {
	case K2DH_IOCTL_SET_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg,
					sizeof(enable)))
			return -EFAULT;
		k2dh_infomsg("opened = %d, enable = %d\n",
			atomic_read(&data->opened), enable);
		if (enable)
			err = k2dh_accel_enable(data);
		else
			err = k2dh_accel_disable(data);
		break;
	case K2DH_IOCTL_SET_DELAY:
		if (copy_from_user(&delay_ns, (void __user *)arg,
					sizeof(delay_ns)))
			return -EFAULT;
		err = k2dh_set_delay(data, delay_ns);
		break;
	case K2DH_IOCTL_GET_DELAY:
		delay_ns = k2dh_get_delay(data);
		if (put_user(delay_ns, (s64 __user *)arg))
			return -EFAULT;
		break;
	case K2DH_IOCTL_READ_ACCEL_XYZ:
		err = k2dh_read_accel_xyz(data, &data->acc_xyz);
		if (err)
			break;
		if (copy_to_user((void __user *)arg,
			&data->acc_xyz, sizeof(data->acc_xyz)))
			return -EFAULT;
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int k2dh_suspend(struct device *dev)
{
	int res = 0;
	struct k2dh_data *data = dev_get_drvdata(dev);

	if (atomic_read(&data->opened) > 0)
		res = i2c_smbus_write_byte_data(data->client,
						CTRL_REG1, PM_OFF);
	return res;
}

static int k2dh_resume(struct device *dev)
{
	int res = 0;
	struct k2dh_data *data = dev_get_drvdata(dev);

	if (atomic_read(&data->opened) > 0)
		res = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
						data->ctrl_reg1_shadow);
	return res;
}

static const struct dev_pm_ops k2dh_pm_ops = {
	.suspend = k2dh_suspend,
	.resume = k2dh_resume,
};

static const struct file_operations k2dh_fops = {
	.owner = THIS_MODULE,
	.open = k2dh_open,
	.release = k2dh_close,
	.unlocked_ioctl = k2dh_ioctl,
};

static ssize_t k2dh_fs_read(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct k2dh_data *data = dev_get_drvdata(dev);

	int err = 0;
	int on;

	mutex_lock(&data->write_lock);
	on = atomic_read(&data->opened);
	if (on == 0) {
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
						DEFAULT_POWER_ON_SETTING);
	}
	mutex_unlock(&data->write_lock);

	if (err < 0) {
		pr_err("%s: i2c write ctrl_reg1 failed\n", __func__);
		return err;
	}
	usleep_range(10000, 11000); /* need loading time */
	err = k2dh_read_accel_xyz(data, &data->acc_xyz);
	if (err < 0) {
		pr_err("%s: k2dh_read_accel_xyz failed\n", __func__);
		return err;
	}

	if (on == 0) {
		mutex_lock(&data->write_lock);
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
								PM_OFF);
		mutex_unlock(&data->write_lock);
		if (err)
			pr_err("%s: i2c write ctrl_reg1 failed\n", __func__);
	}
	return sprintf(buf, "%d,%d,%d\n",
		data->acc_xyz.x, data->acc_xyz.y, data->acc_xyz.z);
}

static ssize_t k2dh_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int err;
	struct k2dh_data *data = dev_get_drvdata(dev);

	err = k2dh_open_calibration(data);
	if (err < 0)
		pr_err("%s: k2dh_open_calibration() failed\n", __func__);

	if (!data->cal_data.x && !data->cal_data.y && !data->cal_data.z)
		err = -1;

	return sprintf(buf, "%d %d %d %d\n",
		err, data->cal_data.x, data->cal_data.y, data->cal_data.z);
}

static ssize_t k2dh_calibration_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct k2dh_data *data = dev_get_drvdata(dev);
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1"))
		do_calib = true;
	else if (sysfs_streq(buf, "0"))
		do_calib = false;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (atomic_read(&data->opened) == 0) {
		/* if off, turn on the device.*/
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
			DEFAULT_POWER_ON_SETTING);
		if (err) {
			pr_err("%s: i2c write ctrl_reg1 failed(err=%d)\n",
				__func__, err);
		}
	}

	err = k2dh_do_calibrate(dev, do_calib);
	if (err < 0) {
		pr_err("%s: k2dh_do_calibrate() failed\n", __func__);
		return err;
	}

	if (atomic_read(&data->opened) == 0) {
		/* if off, turn on the device.*/
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
			PM_OFF);
		if (err) {
			pr_err("%s: i2c write ctrl_reg1 failed(err=%d)\n",
				__func__, err);
		}
	}

	return count;
}

//static DEVICE_ATTR(acc_file, 0664, k2dh_fs_read, NULL);
static ssize_t k2dh_accel_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t k2dh_accel_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(name, 0664, k2dh_accel_name_show, NULL);
static DEVICE_ATTR(vendor, 0664, k2dh_accel_vendor_show, NULL);
static DEVICE_ATTR(raw_data, 0664, k2dh_fs_read, NULL);

static DEVICE_ATTR(calibration, 0664,
		   k2dh_calibration_show, k2dh_calibration_store);

void k2dh_shutdown(struct i2c_client *client)
{
	int res = 0;
	struct k2dh_data *data = i2c_get_clientdata(client);

	k2dh_infomsg("is called.\n");

	res = i2c_smbus_write_byte_data(data->client,
					CTRL_REG1, PM_OFF);
	if (res < 0)
		pr_err("%s: pm_off failed %d\n", __func__, res);
}

static int k2dh_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct k2dh_data *data;
	struct accel_platform_data *pdata;
	int err;

	k2dh_infomsg("is started.\n");

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(struct k2dh_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit;
	}

	/* Checking device */
	err = i2c_smbus_write_byte_data(client, CTRL_REG1,
		PM_OFF);
	if (err) {
		pr_err("%s: there is no such device, err = %d\n",
							__func__, err);
		goto err_read_reg;
	}

	data->client = client;
	g_k2dh = data;
	i2c_set_clientdata(client, data);

	init_completion(&data->data_ready);
	mutex_init(&data->read_lock);
	mutex_init(&data->write_lock);
	atomic_set(&data->opened, 0);

	/* sensor HAL expects to find /dev/accelerometer */
	data->k2dh_device.minor = MISC_DYNAMIC_MINOR;
	data->k2dh_device.name = ACC_DEV_NAME;
	data->k2dh_device.fops = &k2dh_fops;

	err = misc_register(&data->k2dh_device);
	if (err) {
		pr_err("%s: misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	pdata = client->dev.platform_data;
	data->acc_pdata = pdata;
	/* accelerometer position set */
	if (!pdata) {
		pr_err("using defualt position\n");
	} else {
		pr_info("successful, position = %d\n",
			pdata->accel_get_position());
	}

	/* creating device for test & calibration */
	data->dev = sensors_classdev_register("accelerometer_sensor");
	if (IS_ERR(data->dev)) {
		pr_err("%s: class create failed(accelerometer_sensor)\n",
			__func__);
		err = PTR_ERR(data->dev);
		goto err_acc_device_create;
	}

	err = device_create_file(data->dev, &dev_attr_raw_data);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_raw_data.attr.name);
		goto err_acc_device_create_file;
	}

	err = device_create_file(data->dev, &dev_attr_calibration);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_calibration.attr.name);
		goto err_cal_device_create_file;
	}

	err = device_create_file(data->dev, &dev_attr_vendor);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_vendor.attr.name);
		goto err_vendor_device_create_file;
	}

	err = device_create_file(data->dev, &dev_attr_name);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_name.attr.name);
		goto err_name_device_create_file;
	}

	dev_set_drvdata(data->dev, data);

	k2dh_infomsg("is successful.\n");

	return 0;

err_name_device_create_file:
	device_remove_file(data->dev, &dev_attr_vendor);
err_vendor_device_create_file:
	device_remove_file(data->dev, &dev_attr_calibration);
err_cal_device_create_file:
	device_remove_file(data->dev, &dev_attr_raw_data);
err_acc_device_create_file:
	sensors_classdev_unregister(data->dev);
err_acc_device_create:
	misc_deregister(&data->k2dh_device);
err_misc_register:
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->write_lock);
err_read_reg:
	kfree(data);
exit:
	return err;
}

static int k2dh_remove(struct i2c_client *client)
{
	struct k2dh_data *data = i2c_get_clientdata(client);
	int err = 0;

	if (atomic_read(&data->opened) > 0) {
		err = i2c_smbus_write_byte_data(data->client,
					CTRL_REG1, PM_OFF);
		if (err < 0)
			pr_err("%s: pm_off failed %d\n", __func__, err);
	}

	device_destroy(sec_class, 0);
	device_destroy(data->acc_class, MKDEV(ACC_DEV_MAJOR, 0));
	class_destroy(data->acc_class);
	misc_deregister(&data->k2dh_device);
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->write_lock);
	kfree(data);

	return 0;
}

static const struct i2c_device_id k2dh_id[] = {
	{ "k2dh", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, k2dh_id);

static struct i2c_driver k2dh_driver = {
	.probe = k2dh_probe,
	.shutdown = k2dh_shutdown,
	.remove = __devexit_p(k2dh_remove),
	.id_table = k2dh_id,
	.driver = {
		.pm = &k2dh_pm_ops,
		.owner = THIS_MODULE,
		.name = "k2dh",
	},
};

static int __init k2dh_init(void)
{
	return i2c_add_driver(&k2dh_driver);
}

static void __exit k2dh_exit(void)
{
	i2c_del_driver(&k2dh_driver);
}

module_init(k2dh_init);
module_exit(k2dh_exit);

MODULE_DESCRIPTION("k2dh accelerometer driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
