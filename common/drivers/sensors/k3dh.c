/*
 *  STMicroelectronics k3dh acceleration sensor driver
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
#include <linux/sensors_core.h>
#include <linux/k3dh.h>
#include "k3dh_reg.h"
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
#include <linux/input.h>
#endif
#ifdef USES_MOVEMENT_RECOGNITION
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void k3dh_early_suspend(struct early_suspend *h);
static void k3dh_late_resume(struct early_suspend *h);
#endif

/* For Debugging */
#if 1
#define k3dh_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)
#endif
#define k3dh_infomsg(str, args...) pr_info("%s: " str, __func__, ##args)

#define VENDOR		"STM"
#define CHIP_ID		"K2DH"

/* The default settings when sensor is on is for all 3 axis to be enabled
 * and output data rate set to 400Hz.  Output is via a ioctl read call.
 */
#define DEFAULT_POWER_ON_SETTING (ODR400 | ENABLE_ALL_AXES)
#define ACC_DEV_MAJOR 241

#define CALIBRATION_FILE_PATH	"/efs/sensor/acc_cal"
#define CAL_DATA_AMOUNT	20

#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH		9806550
#define ABSMAX_2G		(GRAVITY_EARTH * 2)
#define ABSMIN_2G		(-GRAVITY_EARTH * 2)
#define MIN_DELAY	5
#define MAX_DELAY	200
#endif

#ifdef USES_MOVEMENT_RECOGNITION
//#define DEBUG_REACTIVE_ALERT
#define DEFAULT_CTRL3_SETTING		0x60 /* INT enable */
#define DEFAULT_INTERRUPT_SETTING	0x0A /* INT1 XH,YH : enable */
#define DEFAULT_INTERRUPT2_SETTING	0x20 /* INT2 ZH enable */
#define DEFAULT_THRESHOLD		0x7F /* 2032mg (16*0x7F) */
#define DYNAMIC_THRESHOLD		300	/* mg */
#define DYNAMIC_THRESHOLD2		700	/* mg */
#define MOVEMENT_DURATION		0x00 /*INT1 (DURATION/odr)ms*/
enum {
	OFF = 0,
	ON = 1
};
#define ABS(a)		(((a) < 0) ? -(a) : (a))
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))

#define K3DH_RETRY_COUNT	3
#define ACC_ENABLED 1
#endif

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

/* K3DH acceleration data */
struct k3dh_acc {
	s16 x;
	s16 y;
	s16 z;
};

struct k3dh_data {
	struct i2c_client *client;
	struct miscdevice k3dh_device;
	struct mutex read_lock;
	struct mutex write_lock;
	struct completion data_ready;
#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
	struct class *acc_class;
#else
	struct device *dev;
#endif
	struct k3dh_acc cal_data;
	struct k3dh_acc acc_xyz;
	u8 ctrl_reg1_shadow;
	atomic_t opened; /* opened implies enabled */
	struct accel_platform_data *acc_pdata;
	int position;
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	struct input_dev *input;
	struct delayed_work work;
	atomic_t delay;
	atomic_t enable;
#endif
#ifdef USES_MOVEMENT_RECOGNITION
	u8 state;
	int movement_recog_flag;
	unsigned char interrupt_state;
	//struct wake_lock reactive_wake_lock;
	struct k3dh_platform_data *pdata;
	int irq;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	atomic_t suspended; //system suspend or not .
#endif
};

static struct k3dh_data *g_k3dh;

extern struct class *sensor_class;

#ifdef USES_MOVEMENT_RECOGNITION
#ifdef DEBUG_REACTIVE_ALERT
static int k3dh_acc_i2c_read(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msgs[] = {
		{
			.addr	= g_k3dh->client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= g_k3dh->client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < K3DH_RETRY_COUNT; i++) {
		if (i2c_transfer(g_k3dh->client->adapter, msgs, 2) >= 0) {
			break;
		}
		msleep(10);
	}

	if (i >= K3DH_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __func__, K3DH_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}
#endif /*DEBUG_REACTIVE_ALERT*/

static int k3dh_acc_i2c_write(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msg[] = {
		{
			.addr	= g_k3dh->client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < K3DH_RETRY_COUNT; i++) {
		if (i2c_transfer(g_k3dh->client->adapter, msg, 1) >= 0) {
			break;
		}
		msleep(10);
	}

	if (i >= K3DH_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __func__, K3DH_RETRY_COUNT);
		return -EIO;
	}
	return 0;
}
#endif

static void k3dh_xyz_position_adjust(struct k3dh_acc *acc,
		int position)
{
	const int position_map[][3][3] = {
	{{ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1} }, /* 0 top/upper-left */
	{{-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1} }, /* 1 top/upper-right */
	{{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1} }, /* 2 top/lower-right */
	{{ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1} }, /* 3 top/lower-left */
	{{ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1} }, /* 4 bottom/upper-left */
	{{ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1} }, /* 5 bottom/upper-right */
	{{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1} }, /* 6 bottom/lower-right */
	{{-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1} }, /* 7 bottom/lower-left*/
	};

	struct k3dh_acc xyz_adjusted = {0,};
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

/* Read X,Y and Z-axis acceleration raw data */
static int k3dh_read_accel_raw_xyz(struct k3dh_data *data,
				struct k3dh_acc *acc)
{
	int err;
	s8 reg = OUT_X_L | AC; /* read from OUT_X_L to OUT_Z_H by auto-inc */
	u8 acc_data[6];

	err = i2c_smbus_read_i2c_block_data(data->client, reg,
					    sizeof(acc_data), acc_data);
	if (err != sizeof(acc_data)) {
		pr_err("%s : failed to read 6 bytes for getting x/y/z\n",
		       __func__);
		return -EIO;
	}

	acc->x = (acc_data[1] << 8) | acc_data[0];
	acc->y = (acc_data[3] << 8) | acc_data[2];
	acc->z = (acc_data[5] << 8) | acc_data[4];

	acc->x = acc->x >> 4;
	acc->y = acc->y >> 4;
	acc->z = acc->z >> 4;

	k3dh_xyz_position_adjust(acc, data->position);

	return 0;
}

static int k3dh_read_accel_xyz(struct k3dh_data *data,
				struct k3dh_acc *acc)
{
	int err;

	mutex_lock(&data->read_lock);
	err = k3dh_read_accel_raw_xyz(data, acc);
	mutex_unlock(&data->read_lock);
	if (err < 0) {
		pr_err("%s: k3dh_read_accel_raw_xyz() failed\n", __func__);
		return err;
	}

	acc->x -= data->cal_data.x;
	acc->y -= data->cal_data.y;
	acc->z -= data->cal_data.z;

	return err;
}

static int k3dh_open_calibration(struct k3dh_data *data)
{
	struct file *cal_filp;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		err = PTR_ERR(cal_filp);
		if (err != -ENOENT)
			printk("K3DH] No calibration file yet\n");
		set_fs(old_fs);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		printk("K3DH] K3dh Can't read the cal data from file\n");
		err = -EIO;
	}

	printk("K3DH] K3dh Open calbration (%d,%d,%d)\n",
		data->cal_data.x, data->cal_data.y, data->cal_data.z);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int k3dh_do_calibrate(struct device *dev, bool do_calib)
{
	struct k3dh_data *acc_data = dev_get_drvdata(dev);
	struct k3dh_acc data = { 0, };
	struct file *cal_filp = NULL;
	int sum[3] = { 0, };
	int err = 0;
	s16 i = 0;
	mm_segment_t old_fs;


	if (do_calib) {
		for (i = 0; i < CAL_DATA_AMOUNT; i++) {
			mutex_lock(&acc_data->read_lock);
			err = k3dh_read_accel_raw_xyz(acc_data, &data);
			mutex_unlock(&acc_data->read_lock);
			if (err < 0) {
				pr_err("%s: k3dh_read_accel_raw_xyz() "
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
		acc_data->cal_data.z = (sum[2] / CAL_DATA_AMOUNT) - 1024;
	} else {
		acc_data->cal_data.x = 0;
		acc_data->cal_data.y = 0;
		acc_data->cal_data.z = 0;
	}

	printk(KERN_INFO "K3DH] %s: cal data (%d,%d,%d)\n", __func__,
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

static int k3dh_accel_enable(struct k3dh_data *data)
{
	int err = 0;

	if (atomic_read(&data->opened) == 0) {
		err = k3dh_open_calibration(data);
		if (err < 0 && err != -ENOENT)
			pr_err("%s: k3dh_open_calibration() failed\n",
				__func__);
		data->ctrl_reg1_shadow = DEFAULT_POWER_ON_SETTING;
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
						DEFAULT_POWER_ON_SETTING);
		if (err)
			pr_err("%s: i2c write ctrl_reg1 failed\n", __func__);

		err = i2c_smbus_write_byte_data(data->client, CTRL_REG4,
						CTRL_REG4_HR);
		if (err)
			pr_err("%s: i2c write ctrl_reg4 failed\n", __func__);
	}

	atomic_add(1, &data->opened);
#ifdef USES_MOVEMENT_RECOGNITION
	g_k3dh->state |= ACC_ENABLED;
#endif
	return err;
}

static int k3dh_accel_disable(struct k3dh_data *data)
{
	int err = 0;

	atomic_sub(1, &data->opened);
	if (atomic_read(&data->opened) == 0) {
#ifdef USES_MOVEMENT_RECOGNITION
		if (g_k3dh->movement_recog_flag == ON) {
			unsigned char acc_data[2] = {0};

			printk(KERN_INFO "K3DH] [%s] LOW_PWR_MODE.\n", __FUNCTION__);
			acc_data[0] = CTRL_REG1;
			acc_data[1] = LOW_PWR_MODE;
			if(k3dh_acc_i2c_write(acc_data, 2) !=0)
				printk(KERN_ERR "K3DH] [%s] Change to Low Power Mode is failed\n",__FUNCTION__);
		} else
#endif
		{
			err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,PM_OFF);
			data->ctrl_reg1_shadow = PM_OFF;
		}
	}
#ifdef USES_MOVEMENT_RECOGNITION
	g_k3dh->state=0;
#endif

	return err;
}

/*  open command for K3DH device file  */
static int k3dh_open(struct inode *inode, struct file *file)
{
	k3dh_infomsg("is called.\n");
	return 0;
}

/*  release command for K3DH device file */
static int k3dh_close(struct inode *inode, struct file *file)
{
	k3dh_infomsg("is called.\n");
	return 0;
}

static s64 k3dh_get_delay(struct k3dh_data *data)
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

static int k3dh_set_delay(struct k3dh_data *data, s64 delay_ns)
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

	k3dh_infomsg("old=%lldns, new=%lldns, odr=0x%x/0x%x\n",
		k3dh_get_delay(data), delay_ns, odr_value,
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

/*  ioctl command for K3DH device file */
static long k3dh_ioctl(struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct k3dh_data *data = container_of(file->private_data,
		struct k3dh_data, k3dh_device);
	s64 delay_ns;
	int enable = 0;

	/* cmd mapping */
	switch (cmd) {
	case K3DH_IOCTL_SET_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg,
					sizeof(enable)))
			return -EFAULT;
		k3dh_infomsg("opened = %d, enable = %d\n",
			atomic_read(&data->opened), enable);
		if (enable)
			err = k3dh_accel_enable(data);
		else
			err = k3dh_accel_disable(data);
		break;
	case K3DH_IOCTL_SET_DELAY:
		if (copy_from_user(&delay_ns, (void __user *)arg,
					sizeof(delay_ns)))
			return -EFAULT;
		err = k3dh_set_delay(data, delay_ns);
		break;
	case K3DH_IOCTL_GET_DELAY:
		delay_ns = k3dh_get_delay(data);
		if (put_user(delay_ns, (s64 __user *)arg))
			return -EFAULT;
		break;
	case K3DH_IOCTL_READ_ACCEL_XYZ:
		err = k3dh_read_accel_xyz(data, &data->acc_xyz);
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


#ifdef CONFIG_HAS_EARLYSUSPEND
static void k3dh_early_suspend(struct early_suspend *h)
{
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	atomic_set(&g_k3dh->suspended,1);
	if (atomic_read(&g_k3dh->enable))
		cancel_delayed_work_sync(&g_k3dh->work);
#endif
	if (atomic_read(&g_k3dh->opened) > 0)
		 i2c_smbus_write_byte_data(g_k3dh->client,
						CTRL_REG1, PM_OFF);

}
static void k3dh_late_resume(struct early_suspend *h)
{
	if (atomic_read(&g_k3dh->opened) > 0)
		i2c_smbus_write_byte_data(g_k3dh->client, CTRL_REG1,
						g_k3dh->ctrl_reg1_shadow);
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	atomic_set(&g_k3dh->suspended,0);
		if (atomic_read(&g_k3dh->enable))
			schedule_delayed_work(&g_k3dh->work,
				msecs_to_jiffies(5));
#endif

}
#endif

#ifndef CONFIG_HAS_EARLYSUSPEND
static int k3dh_suspend(struct device *dev)
{
	int res = 0;
	struct k3dh_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	if (atomic_read(&data->enable))
		cancel_delayed_work_sync(&data->work);
#endif
	if (atomic_read(&data->opened) > 0)
		res = i2c_smbus_write_byte_data(data->client,
						CTRL_REG1, PM_OFF);

	return res;
}

static int k3dh_resume(struct device *dev)
{
	int res = 0;
	struct k3dh_data *data = dev_get_drvdata(dev);

	if (atomic_read(&data->opened) > 0)
		res = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
						data->ctrl_reg1_shadow);
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
		if (atomic_read(&data->enable))
			schedule_delayed_work(&data->work,
				msecs_to_jiffies(5));
#endif
	return res;
}

static const struct dev_pm_ops k3dh_pm_ops = {
	.suspend = k3dh_suspend,
	.resume = k3dh_resume,
};
#endif

static const struct file_operations k3dh_fops = {
	.owner = THIS_MODULE,
	.open = k3dh_open,
	.release = k3dh_close,
	.unlocked_ioctl = k3dh_ioctl,
};

#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
static ssize_t k3dh_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct k3dh_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", atomic_read(&data->enable));
}

static ssize_t k3dh_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct k3dh_data *data = input_get_drvdata(input);
	unsigned long enable = 0;
	int err;

	if (strict_strtoul(buf, 10, &enable))
		return -EINVAL;
	k3dh_open_calibration(data);

	if (enable) {
		err = k3dh_accel_enable(data);
		if (err < 0)
			goto done;
#ifdef CONFIG_HAS_EARLYSUSPEND
		if(atomic_read(&data->suspended)==0)//schedule work after device late_resume.  if not, it will call input function and set wakelock, it will cause device can not sleep
		{
#endif
			schedule_delayed_work(&data->work,
				msecs_to_jiffies(5));
#ifdef CONFIG_HAS_EARLYSUSPEND
		}
#endif
	} else {
		cancel_delayed_work_sync(&data->work);
		err = k3dh_accel_disable(data);
		if (err < 0)
			goto done;
	}
	atomic_set(&data->enable, enable);
	pr_info("K3DH] %s, enable = %ld, suspended = %ld \n", __func__, enable, data->suspended);
done:
	return count;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP, k3dh_enable_show, k3dh_enable_store);

static ssize_t k3dh_delay_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct k3dh_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", atomic_read(&data->delay));
}

static ssize_t k3dh_delay_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct k3dh_data *data = input_get_drvdata(input);
	unsigned long delay = 0;
	if (strict_strtoul(buf, 10, &delay))
		return -EINVAL;

	if (delay > MAX_DELAY)
		delay = MAX_DELAY;
	if (delay < MIN_DELAY)
		delay = MIN_DELAY;
	atomic_set(&data->delay, delay);
	pr_info("K3DH] %s, delay = %ld\n", __func__, delay);
	return count;
}
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   k3dh_delay_show, k3dh_delay_store);

static struct attribute *k3dh_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	NULL
};

static struct attribute_group k3dh_attribute_group = {
	.attrs = k3dh_attributes
};
#endif

static ssize_t k3dh_fs_read(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct k3dh_data *data = dev_get_drvdata(dev);

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
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

	err = k3dh_read_accel_xyz(data, &data->acc_xyz);
	if (err < 0) {
		pr_err("%s: k3dh_read_accel_xyz failed\n", __func__);
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
#endif
	return sprintf(buf, "%d,%d,%d\n",
		data->acc_xyz.x, data->acc_xyz.y, data->acc_xyz.z);
}

static ssize_t k3dh_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int err;
	struct k3dh_data *data = dev_get_drvdata(dev);
	printk("K3DH calibration_show\n");

	err = k3dh_open_calibration(data);
	if (err < 0)
		printk("k3dh_open_calibration() failed\n");

	if (!data->cal_data.x && !data->cal_data.y && !data->cal_data.z)
		err = -1;

	return sprintf(buf, "%d %d %d %d\n",
		err, data->cal_data.x, data->cal_data.y, data->cal_data.z);
}

static ssize_t k3dh_calibration_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct k3dh_data *data = dev_get_drvdata(dev);
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
	printk("K3DH calibration_store :%d\n",do_calib);

	if (atomic_read(&data->opened) == 0) {
		/* if off, turn on the device.*/
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG1,
			DEFAULT_POWER_ON_SETTING);
		if (err) {
			pr_err("%s: i2c write ctrl_reg1 failed(err=%d)\n",
				__func__, err);
		}
	}

	err = k3dh_do_calibrate(dev, do_calib);
	if (err < 0) {
		pr_err("%s: k3dh_do_calibrate() failed\n", __func__);
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

#ifdef USES_MOVEMENT_RECOGNITION
static ssize_t k3dh_accel_reactive_alert_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int onoff = OFF, err = 0, ctrl_reg = 0;
	bool factory_test = false;
	struct k3dh_acc raw_data;
	u8 thresh1 = 0, thresh2 = 0;
	unsigned char acc_data[2] = {0};

	if (sysfs_streq(buf, "1"))
		onoff = ON;
	else if (sysfs_streq(buf, "0"))
		onoff = OFF;
	else if (sysfs_streq(buf, "2")) {
		onoff = ON;
		factory_test = true;
		printk(KERN_INFO "[K3DH] [%s] factory_test = %d\n", __func__, factory_test);
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (onoff == ON && g_k3dh->movement_recog_flag == OFF) {
		printk(KERN_INFO "[K3DH] [%s] reactive alert is on.\n", __func__);
		g_k3dh->interrupt_state = 0; /* Init interrupt state.*/

		if (!(g_k3dh->state & ACC_ENABLED)) {
			acc_data[0] = CTRL_REG1;
			acc_data[1] = FASTEST_MODE;
			if(k3dh_acc_i2c_write(acc_data, 2) !=0)
			{
				printk(KERN_ERR "[%s] Change to Fastest Mode is failed\n",__FUNCTION__);
				ctrl_reg = CTRL_REG1;
				goto err_i2c_write;
			}

			/* trun on time, T = 7/odr ms */
			usleep_range(10000, 10000);
		}
		enable_irq(g_k3dh->irq);
		//if (device_may_wakeup(&g_k3dh->client->dev))
			enable_irq_wake(g_k3dh->irq);
		/* Get x, y, z data to set threshold1, threshold2. */
		err = k3dh_read_accel_xyz(g_k3dh, &raw_data);
		printk(KERN_INFO "[K3DH] [%s] raw x = %d, y = %d, z = %d\n",
			__FUNCTION__, raw_data.x, raw_data.y, raw_data.z);
		if (err < 0) {
			pr_err("%s: k3dh_accel_read_xyz failed\n",
				__func__);
			goto exit;
		}
		if (!(g_k3dh->state & ACC_ENABLED)) {
			acc_data[0] = CTRL_REG1;
			acc_data[1] = LOW_PWR_MODE; /* Change to 50Hz*/
			if(k3dh_acc_i2c_write(acc_data, 2) !=0){
				printk(KERN_ERR "[%s] Change to Low Power Mode is failed\n",__FUNCTION__);
				ctrl_reg = CTRL_REG1;
				goto err_i2c_write;
			}
		}
		/* Change raw data to threshold value & settng threshold */
		thresh1 = (MAX(ABS(raw_data.x), ABS(raw_data.y))
				+ DYNAMIC_THRESHOLD)/16;
		if (factory_test == true)
			thresh2 = 0; /* for z axis */
		else
			thresh2 = (ABS(raw_data.z) + DYNAMIC_THRESHOLD2)/16;
		printk(KERN_INFO "[K3DH] [%s] threshold1 = 0x%x, threshold2 = 0x%x\n",__func__, thresh1, thresh2);
		acc_data[0] = INT1_THS;
		acc_data[1] = thresh1;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			ctrl_reg = INT1_THS;
			goto err_i2c_write;
		}
		acc_data[0] = INT2_THS;
		acc_data[1] = thresh2;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			ctrl_reg = INT2_THS;
			goto err_i2c_write;
		}

		/* INT enable */
		#if defined USE_INT1_PIN  /*int1 pin*/
		acc_data[0] = CTRL_REG3;
		acc_data[1] = DEFAULT_CTRL3_SETTING;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			ctrl_reg = CTRL_REG3;
			goto err_i2c_write;
		}
		#elif defined USE_INT2_PIN
		/*Int2 pin generate setting*/
		acc_data[0] = CTRL_REG6;
		acc_data[1] = DEFAULT_CTRL3_SETTING;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			ctrl_reg = CTRL_REG6;
			goto err_i2c_write;
		}
		#endif  /* Need to enable at least one of these to use interrupt */

		g_k3dh->movement_recog_flag = ON;
	}
	else if (onoff == OFF && g_k3dh->movement_recog_flag == ON) {
		printk(KERN_INFO "[K3DH] [%s] reactive alert is off.\n", __FUNCTION__);

		/* INT disable */
		#if defined USE_INT1_PIN  /*int1 pin*/
		acc_data[0] = CTRL_REG3;
		#elif defined USE_INT2_PIN
		acc_data[0] = CTRL_REG6; /*INT2 pin*/
		#endif
		acc_data[1] = PM_OFF;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			ctrl_reg = CTRL_REG6;
			goto err_i2c_write;
		}
		//if (device_may_wakeup(&g_k3dh->client->dev))
			disable_irq_wake(g_k3dh->irq);
		disable_irq_nosync(g_k3dh->irq);
		/* return the power state */
		acc_data[0] = CTRL_REG1;
		acc_data[1] = g_k3dh->ctrl_reg1_shadow;
		if(k3dh_acc_i2c_write(acc_data, 2) !=0){
			printk(KERN_ERR "[%s] Change to shadow status : 0x%x\n",__FUNCTION__, g_k3dh->ctrl_reg1_shadow);
			ctrl_reg = CTRL_REG1;
			goto err_i2c_write;
		}
		g_k3dh->movement_recog_flag = OFF;
		g_k3dh->interrupt_state = 0; /* Init interrupt state.*/
		printk(KERN_INFO "[K3DH] [%s] interrupt OFF\n",__func__);
	}
	return count;
err_i2c_write:
	pr_err("%s: i2c write ctrl_reg = 0x%d failed(err=%d)\n",
				__func__, ctrl_reg, err);
exit:
	return ((err < 0) ? err : -err);
}

static ssize_t k3dh_accel_reactive_alert_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", g_k3dh->interrupt_state);
}
#endif

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
static DEVICE_ATTR(acc_file, S_IRUGO, k3dh_fs_read, NULL);
#else
static ssize_t k3dh_accel_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t k3dh_accel_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(name, S_IRUGO, k3dh_accel_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, k3dh_accel_vendor_show, NULL);
#ifdef USES_MOVEMENT_RECOGNITION
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
	k3dh_accel_reactive_alert_show,
	k3dh_accel_reactive_alert_store);
#endif
static DEVICE_ATTR(raw_data, S_IRUGO, k3dh_fs_read, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP, k3dh_calibration_show, k3dh_calibration_store);

static struct device_attribute *accel_sensor_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_vendor,
	&dev_attr_name,
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	&dev_attr_enable,
#endif
#ifdef USES_MOVEMENT_RECOGNITION
	&dev_attr_reactive_alert,
#endif
	NULL,
};
#endif

#ifdef USES_MOVEMENT_RECOGNITION
static irqreturn_t k3dh_accel_interrupt_thread(int irq, void *k3dh_data_p)
{
#ifdef DEBUG_REACTIVE_ALERT
	u8 int1_src_reg = 0, int2_src_reg = 0;
// [HSS][TEMP]
#if 0
	struct k3dh_acc raw_data;
#endif
#endif
	unsigned char acc_data[2] = {0};

#ifndef DEBUG_REACTIVE_ALERT
	/* INT disable */
	#if defined USE_INT1_PIN
	acc_data[0] = CTRL_REG3;
	#elif defined USE_INT2_PIN
	acc_data[0] = CTRL_REG6;
	#endif
	acc_data[1] = PM_OFF;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write CTRL_REG6 failed\n",__FUNCTION__);
	}
#else
	acc_data[0] = INT1_SRC;
	if(k3dh_acc_i2c_read(acc_data, 1) < 0)
	{
		printk(KERN_ERR "[%s] i2c read int1_src failed\n",__FUNCTION__);
	}
	int1_src_reg = acc_data[0];
	if (int1_src_reg < 0)
	{
		printk(KERN_ERR "[%s] read int1_src failed\n",__FUNCTION__);
	}
	printk(KERN_INFO "[K3DH] [%s] interrupt source reg1 = 0x%x\n", __FUNCTION__, int1_src_reg);

	acc_data[0] = INT2_SRC;
	if(k3dh_acc_i2c_read(acc_data, 1) < 0)
	{
		printk(KERN_ERR "[%s] i2c read int2_src failed\n",__FUNCTION__);
	}
	int2_src_reg = acc_data[0];
	if (int1_src_reg < 0)
	{
		printk(KERN_ERR "[%s] read int2_src failed\n",__FUNCTION__);
	}
	printk(KERN_INFO "[K3DH] [%s] interrupt source reg2 = 0x%x\n", __FUNCTION__, int2_src_reg);
#endif

	g_k3dh->interrupt_state = 1;
	//wake_lock_timeout(&g_k3dh->reactive_wake_lock, msecs_to_jiffies(2000));
	printk(KERN_INFO "[K3DH] [%s] irq is handled\n", __FUNCTION__);

	return IRQ_HANDLED;
}
#endif

void k3dh_shutdown(struct i2c_client *client)
{
	int res = 0;
	struct k3dh_data *data = i2c_get_clientdata(client);

	k3dh_infomsg("is called.\n");

#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	if (atomic_read(&data->enable))
		cancel_delayed_work_sync(&data->work);
#endif
	res = i2c_smbus_write_byte_data(data->client,
					CTRL_REG1, PM_OFF);
	if (res < 0)
		pr_err("%s: pm_off failed %d\n", __func__, res);
}

#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
static void k3dh_work_func(struct work_struct *work)
{
	k3dh_read_accel_xyz(g_k3dh, &g_k3dh->acc_xyz);
	pr_debug("%s: x: %d, y: %d, z: %d\n", __func__,
		g_k3dh->acc_xyz.x, g_k3dh->acc_xyz.y, g_k3dh->acc_xyz.z);
	input_report_rel(g_k3dh->input, REL_X, g_k3dh->acc_xyz.x);
	input_report_rel(g_k3dh->input, REL_Y, g_k3dh->acc_xyz.y);
	input_report_rel(g_k3dh->input, REL_Z, g_k3dh->acc_xyz.z);
	input_sync(g_k3dh->input);
	schedule_delayed_work(&g_k3dh->work, msecs_to_jiffies(
		atomic_read(&g_k3dh->delay)));
}

/* ----------------- *
   Input device interface
 * ------------------ */
static int k3dh_input_init(struct k3dh_data *data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = "accelerometer_sensor";
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0){
		pr_err("%s: failed to register acc input device!\n", __func__);
		err = -ENODEV;
		goto done;
	}
	data->input = dev;
done:
	return 0;
}
#endif

#ifdef CONFIG_MULTI_ACC_GARDA
extern bool isAccFound;
#endif
static int k3dh_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct k3dh_data *data;
#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
	struct device *dev_t, *dev_cal;
#endif
	struct accel_platform_data *pdata;
#ifdef USES_MOVEMENT_RECOGNITION
	unsigned char acc_data[2] = {0};
#endif
	int err;
	int state = 0;

	k3dh_infomsg("is started.\n");
#ifdef CONFIG_MULTI_ACC_GARDA
	if(isAccFound)
	{
		printk(KERN_INFO "ACC] Another Accelerometer is already probed => isAccFound: %d \n", isAccFound);	
		return -1;
	}
#endif

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	state = i2c_smbus_read_byte_data(client,WHO_AM_I);

	if (state != 0x33) {
		dev_err(&client->dev,
				"Not exist device\n");
		err = -ENOMEM;
		goto exit;
	}

	data = kzalloc(sizeof(struct k3dh_data), GFP_KERNEL);
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
	g_k3dh = data;
	i2c_set_clientdata(client, data);

	init_completion(&data->data_ready);
	mutex_init(&data->read_lock);
	mutex_init(&data->write_lock);
	atomic_set(&data->opened, 0);

	/* sensor HAL expects to find /dev/accelerometer */
	data->k3dh_device.minor = MISC_DYNAMIC_MINOR;
	data->k3dh_device.name = ACC_DEV_NAME;
	data->k3dh_device.fops = &k3dh_fops;
#ifdef CONFIG_MACH_NEVISTD
	data->position = 6;
#else
	data->position = 7;
#endif

	err = misc_register(&data->k3dh_device);
	if (err) {
		pr_err("%s: misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	pdata = client->dev.platform_data;
	data->acc_pdata = pdata;
	/* accelerometer position set */
	if (!pdata)
		pr_err("Acc sensor using defualt position\n");
	else
		pr_err("Acc sensor Position %d",data->position);

#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	atomic_set(&data->enable, 0);
	atomic_set(&data->delay, 200);
	k3dh_input_init(data);
	/* Setup sysfs */
	err = sysfs_create_group(&data->input->dev.kobj,
			       &k3dh_attribute_group);
	if (err < 0)
		goto err_sysfs_create_group;

	/* Setup driver interface */
	INIT_DELAYED_WORK(&data->work, k3dh_work_func);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	atomic_set(&data->suspended,0);
	g_k3dh->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	g_k3dh->early_suspend.suspend = k3dh_early_suspend;
	g_k3dh->early_suspend.resume = k3dh_late_resume;
	register_early_suspend(&g_k3dh->early_suspend);
#endif

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
	/* creating class/device for test */
	data->acc_class = class_create(THIS_MODULE, "accelerometer");
	if (IS_ERR(data->acc_class)) {
		pr_err("%s: class create failed(accelerometer)\n", __func__);
		err = PTR_ERR(data->acc_class);
		goto err_class_create;
	}

	dev_t = device_create(data->acc_class, NULL,
				MKDEV(ACC_DEV_MAJOR, 0), "%s", "accelerometer");
	if (IS_ERR(dev_t)) {
		pr_err("%s: class create failed(accelerometer)\n", __func__);
		err = PTR_ERR(dev_t);
		goto err_acc_device_create;
	}

	err = device_create_file(dev_t, &dev_attr_acc_file);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_acc_file.attr.name);
		goto err_acc_device_create_file;
	}
	dev_set_drvdata(dev_t, data);

	/* creating device for calibration */
	dev_cal = device_create(sec_class, NULL, 0, NULL, "gsensorcal");
	if (IS_ERR(dev_cal)) {
		pr_err("%s: class create failed(gsensorcal)\n", __func__);
		err = PTR_ERR(dev_cal);
		goto err_cal_device_create;
	}

	err = device_create_file(dev_cal, &dev_attr_calibration);
	if (err < 0) {
		pr_err("%s: Failed to create device file(%s)\n",
				__func__, dev_attr_calibration.attr.name);
		goto err_cal_device_create_file;
	}
	dev_set_drvdata(dev_cal, data);
#endif

#ifdef USES_MOVEMENT_RECOGNITION
	g_k3dh->pdata = client->dev.platform_data;
	g_k3dh->movement_recog_flag = OFF;
	g_k3dh->irq = g_k3dh->pdata->gpio_acc_int;

	/*gpio request*/
	if(gpio_request(g_k3dh->pdata->gpio_acc_int, "acc_int"))
		printk(KERN_ERR "Acce sensor Request GPIO%d failed!\n", g_k3dh->pdata->gpio_acc_int);
	else
		printk(KERN_ERR "Acce sensor Request GPIO%d Succeeded!\n", g_k3dh->pdata->gpio_acc_int);
	gpio_direction_input(g_k3dh->pdata->gpio_acc_int);

	/*initial interrupt set*/
	acc_data[0] = INT1_THS;
	acc_data[1] = DEFAULT_THRESHOLD;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write int1_ths failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}

	acc_data[0] = INT1_DURATION;
	acc_data[1] = MOVEMENT_DURATION;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write int1_duration failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}

	acc_data[0] = INT1_CFG;
	acc_data[1] = DEFAULT_INTERRUPT_SETTING;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write int1_cfg failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}

	acc_data[0] = INT2_THS;
	acc_data[1] = DEFAULT_THRESHOLD;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write int2_ths failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}

	acc_data[0] = INT2_DURATION;
	acc_data[1] = MOVEMENT_DURATION;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write int1_duration failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}
	acc_data[0] = INT2_CFG;
	acc_data[1] = DEFAULT_INTERRUPT2_SETTING;
	if(k3dh_acc_i2c_write(acc_data, 2) !=0)
	{
		printk(KERN_ERR "[%s] i2c write INT2_CFG failed\n",__FUNCTION__);
		goto err_sensor_device_create_file;
	}

	/*irq request*/
	g_k3dh->irq = gpio_to_irq(g_k3dh->pdata->gpio_acc_int);
#if 0
	irq_set_irq_type(g_k3dh->irq, IRQ_TYPE_EDGE_RISING);
	if( (ret = request_irq(g_k3dh->irq, gp2a_irq_handler,IRQF_DISABLED | IRQF_NO_SUSPEND , "proximity_int", NULL )) )
	{
		error("GP2A request_irq failed IRQ_NO:%d", g_k3dh->irq);
		goto DESTROY_WORK_QUEUE;
	}
	else
		debug("GP2A request_irq success IRQ_NO:%d", g_k3dh->irq);
#endif
	/*threaded irq*/
	err = request_threaded_irq(g_k3dh->irq, NULL,k3dh_accel_interrupt_thread,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT,"k3dh_accel", g_k3dh);
	if (err < 0) {
		pr_err("%s: Failed to request_threaded_irq\n",__func__);
		goto err_sensor_device_create_file;
	}

	disable_irq(g_k3dh->irq);
	g_k3dh->interrupt_state = 0;
#endif

	err = sensors_register(data->dev, data, accel_sensor_attrs,
			"accelerometer_sensor");
	if (err < 0) {
		pr_err("%s: Failed to sensor register file\n",__func__);
		goto err_sensor_device_create_file;
	}

#ifdef CONFIG_MULTI_ACC_GARDA
	isAccFound=1;
#endif
	k3dh_infomsg("is successful.\n");

	return 0;

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
err_cal_device_create_file:
	device_destroy(sec_class, 0);
err_cal_device_create:
	device_remove_file(dev_t, &dev_attr_acc_file);
err_acc_device_create_file:
	device_destroy(data->acc_class, MKDEV(ACC_DEV_MAJOR, 0));
err_acc_device_create:
	class_destroy(data->acc_class);
err_class_create:
#else
err_sensor_device_create_file:
	device_remove_file(data->dev, &dev_attr_raw_data);
	device_remove_file(data->dev, &dev_attr_vendor);
	device_remove_file(data->dev, &dev_attr_calibration);
	device_remove_file(data->dev, &dev_attr_raw_data);
//err_acc_device_create:
#endif
#ifdef CONFIG_SENSOR_K3DH_INPUTDEV
	input_free_device(data->input);
err_sysfs_create_group:
#endif
misc_deregister(&data->k3dh_device);
err_misc_register:
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->write_lock);
err_read_reg:
	kfree(data);
exit:
	return err;
}

static int k3dh_remove(struct i2c_client *client)
{
	struct k3dh_data *data = i2c_get_clientdata(client);
	s32 err = 0;

	if (atomic_read(&data->opened) > 0) {
		err = i2c_smbus_write_byte_data(data->client,
					CTRL_REG1, PM_OFF);
		if (err < 0)
			pr_err("%s: pm_off failed %d\n", __func__, err);
	}

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)
	device_destroy(sec_class, 0);
	device_destroy(data->acc_class, MKDEV(ACC_DEV_MAJOR, 0));
	class_destroy(data->acc_class);
#else
	device_remove_file(data->dev, &dev_attr_name);
	device_remove_file(data->dev, &dev_attr_vendor);
	device_remove_file(data->dev, &dev_attr_calibration);
	device_remove_file(data->dev, &dev_attr_raw_data);
#endif
	misc_deregister(&data->k3dh_device);
	mutex_destroy(&data->read_lock);
	mutex_destroy(&data->write_lock);
	kfree(data);

	return 0;
}

static const struct i2c_device_id k3dh_id[] = {
	{ "k3dh", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, k3dh_id);

static struct i2c_driver k3dh_driver = {
	.probe = k3dh_probe,
	.shutdown = k3dh_shutdown,
	.remove = k3dh_remove,
	.id_table = k3dh_id,
	.driver = {
#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm = &k3dh_pm_ops,
#endif
		.owner = THIS_MODULE,
		.name = "k3dh",
	},
};

static int __init k3dh_init(void)
{
	return i2c_add_driver(&k3dh_driver);
}

static void __exit k3dh_exit(void)
{
	i2c_del_driver(&k3dh_driver);
}

module_init(k3dh_init);
module_exit(k3dh_exit);

MODULE_DESCRIPTION("k3dh accelerometer driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
