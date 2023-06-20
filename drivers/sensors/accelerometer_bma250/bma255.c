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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>

#include <linux/sensors_core.h>
#include <linux/regulator/consumer.h>
#include "bma255_reg.h"

extern int poweroff_charging;

#define VENDOR_NAME                     "BOSCH"
#define MODEL_NAME                      "BMA255"
#define MODULE_NAME                     "accelerometer_sensor"

#define CALIBRATION_FILE_PATH           "/efs/calibration_data"
#define CALIBRATION_DATA_AMOUNT         20
#define MAX_ACCEL_1G			1024
/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH                                                   9806550
#define ABSMAX_2G                                           (GRAVITY_EARTH * 2)
#define ABSMIN_2G                                          (-GRAVITY_EARTH * 2)

#define BMA255_DEFAULT_DELAY            200
#define BMA255_CHIP_ID                  0xFA

#define BMA255_TOP_UPPER_RIGHT          0
#define BMA255_TOP_LOWER_RIGHT          1
#define BMA255_TOP_LOWER_LEFT           2
#define BMA255_TOP_UPPER_LEFT           3
#define BMA255_BOTTOM_UPPER_RIGHT       4
#define BMA255_BOTTOM_LOWER_RIGHT       5
#define BMA255_BOTTOM_LOWER_LEFT        6
#define BMA255_BOTTOM_UPPER_LEFT        7

#define ACCEL_LOG_TIME                  15 /* 15 sec */

#define SLOPE_X_INT                     0
#define SLOPE_Y_INT                     1
#define SLOPE_Z_INT                     2

#define SLOPE_DURATION_VALUE            1
#define SLOPE_THRESHOLD_VALUE           16

enum {
	OFF = 0,
	ON = 1
};

struct bma255_v {
	union {
		s16 v[3];
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
	};
};

struct bma255_p {
	struct wake_lock reactive_wake_lock;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
	struct device *factory_device;
	struct bma255_v accdata;
	struct bma255_v caldata;

	atomic_t delay;
	atomic_t enable;
	bool i2c_pull_up;
	const u8 *str_l6;
	const u8 *str_lvs1;
	u32 used_chip_flags;
	int used_chip;
	u32 chip_pos;
	int recog_flag;
	int irq1;
	int irq_state;
	int acc_int1;
	int acc_int2;
	int time_count;

	struct regulator *l19;

	struct regulator *lvs1_1p8;
};
/*static struct bma255_p *bma_acc_data;
static struct bma255_p *bma_acc_get_data(void)
{
	return bma_acc_data;
}
static void bma_acc_set_data(struct bma255_p *data)
{
	bma_acc_data = data;
}*/

static int bma255_open_calibration(struct bma255_p *);

static int bma255_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;

	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0) {
		pr_err("[SENSOR]: %s - i2c bus read error %d\n",
			__func__, dummy);
		return -EIO;
	}
	return 0;
}

static int bma255_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *buf)
{
	s32 dummy;

	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0) {
		pr_err("[SENSOR]: %s - i2c bus read error %d\n",
			__func__, dummy);
		return -EIO;
	}
	*buf = dummy & 0x000000ff;

	return 0;
}

static int bma255_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *buf)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *buf);
	if (dummy < 0) {
		pr_err("[SENSOR]: %s - i2c bus read error %d\n",
			__func__, dummy);
		return -EIO;
	}
	return 0;
}

static int bma255_set_mode(struct i2c_client *client, unsigned char mode)
{
	int ret = 0;
	unsigned char buf1, buf2, buf3;
	mdelay(1);

	ret = bma255_smbus_read_byte(client, BMA255_MODE_CTRL_REG, &buf1);
	mdelay(1);

	ret += bma255_smbus_read_byte(client, BMA255_LOW_NOISE_CTRL_REG, &buf2);
	mdelay(1);

	ret += bma255_smbus_read_byte(client, BMA255_BW_SEL_REG, &buf3);
	mdelay(1);

	switch (mode) {
	case BMA255_MODE_NORMAL:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 0);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 0);
		buf3  = BMA255_SET_BITSLICE(buf3, BMA255_BANDWIDTH, BMA255_BW_31_25HZ);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_BW_SEL_REG, &buf3);
		break;
	case BMA255_MODE_LOWPOWER1:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 2);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 0);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		break;
	case BMA255_MODE_SUSPEND:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 4);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 0);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		break;
	case BMA255_MODE_DEEP_SUSPEND:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 1);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 1);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		break;
	case BMA255_MODE_LOWPOWER2:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 2);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 1);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		break;
	case BMA255_MODE_STANDBY:
		buf1  = BMA255_SET_BITSLICE(buf1, BMA255_MODE_CTRL, 4);
		buf2  = BMA255_SET_BITSLICE(buf2, BMA255_LOW_POWER_MODE, 1);
		ret += bma255_smbus_write_byte(client,
				BMA255_LOW_NOISE_CTRL_REG, &buf2);
		mdelay(3);
		ret += bma255_smbus_write_byte(client,
				BMA255_MODE_CTRL_REG, &buf1);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bma255_set_range(struct i2c_client *client, unsigned char range)
{
	int ret = 0 ;
	unsigned char buf;

	ret = bma255_smbus_read_byte(client, BMA255_RANGE_SEL_REG, &buf);
	switch (range) {
	case BMA255_RANGE_2G:
		buf = BMA255_SET_BITSLICE(buf, BMA255_RANGE_SEL, 3);
		break;
	case BMA255_RANGE_4G:
		buf = BMA255_SET_BITSLICE(buf, BMA255_RANGE_SEL, 5);
		break;
	case BMA255_RANGE_8G:
		buf = BMA255_SET_BITSLICE(buf, BMA255_RANGE_SEL, 8);
		break;
	case BMA255_RANGE_16G:
		buf = BMA255_SET_BITSLICE(buf, BMA255_RANGE_SEL, 12);
		break;
	default:
		buf = BMA255_SET_BITSLICE(buf, BMA255_RANGE_SEL, 3);
		break;
	}

	ret += bma255_smbus_write_byte(client, BMA255_RANGE_SEL_REG, &buf);

	return ret;
}

static int bma255_set_bandwidth(struct i2c_client *client,
		unsigned char bandwidth)
{
	int ret = 0;
	unsigned char buf;

	if (bandwidth <= 7 || bandwidth >= 16)
		bandwidth = BMA255_BW_1000HZ;

	ret = bma255_smbus_read_byte(client, BMA255_BANDWIDTH__REG, &buf);
	buf = BMA255_SET_BITSLICE(buf, BMA255_BANDWIDTH, bandwidth);
	ret += bma255_smbus_write_byte(client, BMA255_BANDWIDTH__REG, &buf);

	return ret;
}

static int bma255_read_accel_xyz(struct bma255_p *data,	struct bma255_v *acc)
{
	int ret = 0;
	unsigned char buf[6];

	ret = bma255_smbus_read_byte_block(data->client,
			BMA255_ACC_X12_LSB__REG, buf, 6);

	acc->x = BMA255_GET_BITSLICE(buf[0], BMA255_ACC_X12_LSB) |
			(BMA255_GET_BITSLICE(buf[1], BMA255_ACC_X_MSB) <<
				(BMA255_ACC_X12_LSB__LEN));
	acc->x = acc->x << (sizeof(short) * 8 - (BMA255_ACC_X12_LSB__LEN +
				BMA255_ACC_X_MSB__LEN));
	acc->x = acc->x >> (sizeof(short) * 8 - (BMA255_ACC_X12_LSB__LEN +
				BMA255_ACC_X_MSB__LEN));

	acc->y = BMA255_GET_BITSLICE(buf[2], BMA255_ACC_Y12_LSB) |
			(BMA255_GET_BITSLICE(buf[3],
			BMA255_ACC_Y_MSB) << (BMA255_ACC_Y12_LSB__LEN));
	acc->y = acc->y << (sizeof(short) * 8 - (BMA255_ACC_Y12_LSB__LEN +
			BMA255_ACC_Y_MSB__LEN));
	acc->y = acc->y >> (sizeof(short) * 8 - (BMA255_ACC_Y12_LSB__LEN +
			BMA255_ACC_Y_MSB__LEN));

	acc->z = BMA255_GET_BITSLICE(buf[4], BMA255_ACC_Z12_LSB) |
			(BMA255_GET_BITSLICE(buf[5],
			BMA255_ACC_Z_MSB) << (BMA255_ACC_Z12_LSB__LEN));
	acc->z = acc->z << (sizeof(short) * 8 - (BMA255_ACC_Z12_LSB__LEN +
			BMA255_ACC_Z_MSB__LEN));
	acc->z = acc->z >> (sizeof(short) * 8 - (BMA255_ACC_Z12_LSB__LEN +
			BMA255_ACC_Z_MSB__LEN));

	remap_sensor_data(acc->v, data->chip_pos);

	return ret;
}

static void bma255_work_func(struct work_struct *work)
{
	struct bma255_v acc;
	struct bma255_p *data = container_of((struct delayed_work *)work,
			struct bma255_p, work);
	unsigned long delay = msecs_to_jiffies(atomic_read(&data->delay));

	bma255_read_accel_xyz(data, &acc);
	data->accdata.x = acc.x - data->caldata.x;
	data->accdata.y = acc.y - data->caldata.y;
	data->accdata.z = acc.z - data->caldata.z;
	// input_report_rel(data->input, REL_X, data->accdata.x);
	// input_report_rel(data->input, REL_Y, data->accdata.y);
	// input_report_rel(data->input, REL_Z, data->accdata.z);
	input_report_abs(data->input, ABS_X, data->accdata.x);
	input_report_abs(data->input, ABS_Y, data->accdata.y);
	input_report_abs(data->input, ABS_Z, data->accdata.z);
	input_sync(data->input);

	if ((atomic_read(&data->delay) * data->time_count)
		>= (ACCEL_LOG_TIME * MSEC_PER_SEC)) {
		/*pr_info("[SENSOR]: %s - x = %d, y = %d, z = %d\n", __func__,
			data->accdata.x, data->accdata.y, data->accdata.z);*/
		data->time_count = 0;
	} else
		data->time_count++;

	schedule_delayed_work(&data->work, delay);
}

static void bma255_set_enable(struct bma255_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	pr_info("\n bma255 sysfs set_enable sensor enable %d", enable);
    if (enable) {
		if (pre_enable == OFF) {
			bma255_open_calibration(data);
			bma255_set_mode(data->client, BMA255_MODE_NORMAL);
			schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
			atomic_set(&data->enable, ON);
			//pr_info("bma255 enable =%d",data->enable);
		}
	} else {
		if (pre_enable == ON) {
			if (data->recog_flag == ON)
				bma255_set_mode(data->client,
						BMA255_MODE_LOWPOWER1);
			else
				bma255_set_mode(data->client,
						BMA255_MODE_SUSPEND);
			cancel_delayed_work_sync(&data->work);
			atomic_set(&data->enable, OFF);
		}
	}
}

static ssize_t bma255_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma255_p *data = input_get_drvdata(input);
	//struct bma255_p *data = bma_acc_get_data();
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t bma255_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct input_dev *input = to_input_dev(dev);
	struct bma255_p *data = input_get_drvdata(input);
	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);
	if ((enable == ON) || (enable == OFF))
		bma255_set_enable(data, (int)enable);

	return size;
}

static ssize_t bma255_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma255_p *data = input_get_drvdata(input);
//	struct bma255_p *data = bma_acc_get_data();
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->delay));
}

static ssize_t bma255_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int64_t delay;
	struct input_dev *input = to_input_dev(dev);
	struct bma255_p *data = input_get_drvdata(input);
//	struct bma255_p *data = bma_acc_get_data();
	ret = kstrtoll(buf, 10, &delay);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	atomic_set(&data->delay, (unsigned int)delay);
	pr_info("[SENSOR]: %s - poll_delay = %lld\n", __func__, delay);

	return size;
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		bma255_delay_show, bma255_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		bma255_enable_show, bma255_enable_store);

static struct attribute *bma255_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	NULL
};


static struct attribute_group bma255_attribute_group = {
	.attrs = bma255_attributes
};


static ssize_t bma255_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t bma255_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static int bma255_open_calibration(struct bma255_p *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		pr_err("[SENSOR]: %s - cal_filp open failed(%d)\n",
			__func__, ret);

		return ret;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)&data->caldata,
		3 * sizeof(s16), &cal_filp->f_pos);
	if (ret != 3 * sizeof(s16))
		ret = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SENSOR]: open accel calibration %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	if ((data->caldata.x == 0) && (data->caldata.y == 0)
		&& (data->caldata.z == 0))
		return -EIO;

	return ret;
}

static int bma255_do_calibrate(struct bma255_p *data, int enable)
{
	int sum[3] = { 0, };
	int ret = 0, cnt;
	struct file *cal_filp = NULL;
	struct bma255_v acc;
	mm_segment_t old_fs;

	if (enable) {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		if (atomic_read(&data->enable) == ON)
			cancel_delayed_work_sync(&data->work);
		else
			bma255_set_mode(data->client, BMA255_MODE_NORMAL);

		msleep(300);

		for (cnt = 0; cnt < CALIBRATION_DATA_AMOUNT; cnt++) {
			bma255_read_accel_xyz(data, &acc);
			sum[0] += acc.x;
			sum[1] += acc.y;
			sum[2] += acc.z;
			mdelay(10);
		}

		if (atomic_read(&data->enable) == ON)
			schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
		else
			bma255_set_mode(data->client, BMA255_MODE_SUSPEND);

		data->caldata.x = (sum[0] / CALIBRATION_DATA_AMOUNT);
		data->caldata.y = (sum[1] / CALIBRATION_DATA_AMOUNT);
		data->caldata.z = (sum[2] / CALIBRATION_DATA_AMOUNT);

		if (data->caldata.z > 0)
			data->caldata.z -= MAX_ACCEL_1G;
		else if (data->caldata.z < 0)
			data->caldata.z += MAX_ACCEL_1G;
	} else {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;
	}

	pr_info("[SENSOR]: %s - do accel calibrate %d, %d, %d\n", __func__,
		data->caldata.x, data->caldata.y, data->caldata.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("[SENSOR]: %s - Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)&data->caldata,
		3 * sizeof(int), &cal_filp->f_pos);
	if (ret != 3 * sizeof(int)) {
		pr_err("[SENSOR]: %s - Can't write the caldata to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static ssize_t bma255_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
	ret = bma255_open_calibration(data);
	if (ret < 0)
		pr_err("[SENSOR]: %s - calibration open failed(%d)\n",
			__func__, ret);

	pr_info("[SENSOR]: %s - cal data %d %d %d - ret : %d\n", __func__,
		data->caldata.x, data->caldata.y, data->caldata.z, ret);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", ret, data->caldata.x,
			data->caldata.y, data->caldata.z);
}

static ssize_t bma255_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int64_t dEnable;
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0)
		return ret;

	ret = bma255_do_calibrate(data, (int)dEnable);
	if (ret < 0)
		pr_err("[SENSOR]: %s - accel calibrate failed\n", __func__);

	return size;
}

static ssize_t bma255_raw_data_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bma255_v acc;
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
	if (atomic_read(&data->enable) == OFF) {
		bma255_set_mode(data->client, BMA255_MODE_NORMAL);
		msleep(20);
		bma255_read_accel_xyz(data, &acc);
		bma255_set_mode(data->client, BMA255_MODE_SUSPEND);
		acc.x = acc.x - data->caldata.x;
		acc.y = acc.y - data->caldata.y;
		acc.z = acc.z - data->caldata.z;
	} else {
		acc = data->accdata;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			acc.x, acc.y, acc.z);
}

static void bma255_set_int_enable(struct i2c_client *client,
		unsigned char InterruptType , unsigned char value)
{
	unsigned char reg;

	bma255_smbus_read_byte(client, BMA255_INT_ENABLE1_REG, &reg);

	switch (InterruptType) {
	case SLOPE_X_INT:
		/* Slope X Interrupt */
		reg = BMA255_SET_BITSLICE(reg, BMA255_EN_SLOPE_X_INT, value);
		break;
	case SLOPE_Y_INT:
		/* Slope Y Interrupt */
		reg = BMA255_SET_BITSLICE(reg, BMA255_EN_SLOPE_Y_INT, value);
		break;
	case SLOPE_Z_INT:
		/* Slope Z Interrupt */
		reg = BMA255_SET_BITSLICE(reg, BMA255_EN_SLOPE_Z_INT, value);
		break;
	default:
		break;
	}

	bma255_smbus_write_byte(client, BMA255_INT_ENABLE1_REG, &reg);
}

static void bma255_slope_enable(struct i2c_client *client,
		int enable, int factory_mode)
{
	unsigned char reg;

	if (enable == ON) {
		bma255_smbus_read_byte(client,
				BMA255_EN_INT1_PAD_SLOPE__REG, &reg);
		reg = BMA255_SET_BITSLICE(reg, BMA255_EN_INT1_PAD_SLOPE, ON);
		bma255_smbus_write_byte(client,
				BMA255_EN_INT1_PAD_SLOPE__REG, &reg);

		bma255_smbus_read_byte(client, BMA255_INT_MODE_SEL__REG, &reg);
		reg = BMA255_SET_BITSLICE(reg, BMA255_INT_MODE_SEL, 0x01);
		bma255_smbus_write_byte(client, BMA255_INT_MODE_SEL__REG, &reg);

		bma255_smbus_read_byte(client, BMA255_SLOPE_DUR__REG, &reg);
		reg = BMA255_SET_BITSLICE(reg, BMA255_SLOPE_DUR,
				SLOPE_DURATION_VALUE);
		bma255_smbus_write_byte(client, BMA255_SLOPE_DUR__REG, &reg);

		if (factory_mode == OFF) {
			reg = SLOPE_THRESHOLD_VALUE;
			bma255_smbus_write_byte(client,
					BMA255_SLOPE_THRES__REG, &reg);

			bma255_set_int_enable(client, SLOPE_X_INT, ON);
			bma255_set_int_enable(client, SLOPE_Y_INT, ON);
			bma255_set_int_enable(client, SLOPE_Z_INT, ON);
		} else {
			reg = 0x00;
			bma255_smbus_write_byte(client,
					BMA255_SLOPE_THRES__REG, &reg);

			bma255_set_int_enable(client, SLOPE_Z_INT, ON);
		}

		bma255_set_bandwidth(client, BMA255_BW_7_81HZ);
	} else if (enable == OFF) {
		bma255_smbus_read_byte(client,
				BMA255_EN_INT1_PAD_SLOPE__REG, &reg);
		reg = BMA255_SET_BITSLICE(reg, BMA255_EN_INT1_PAD_SLOPE, OFF);
		bma255_smbus_write_byte(client,
				BMA255_EN_INT1_PAD_SLOPE__REG, &reg);

		bma255_set_int_enable(client, SLOPE_X_INT, OFF);
		bma255_set_int_enable(client, SLOPE_Y_INT, OFF);
		bma255_set_int_enable(client, SLOPE_Z_INT, OFF);

		bma255_set_bandwidth(client, BMA255_BW_125HZ);
	}
}

static ssize_t bma255_reactive_alert_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable = OFF, factory_mode = OFF;
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
	if (sysfs_streq(buf, "0")) {
		enable = OFF;
		factory_mode = OFF;
		pr_info("[SENSOR]: %s - disable\n", __func__);
	} else if (sysfs_streq(buf, "1")) {
		enable = ON;
		factory_mode = OFF;
		pr_info("[SENSOR]: %s - enable\n", __func__);
	} else if (sysfs_streq(buf, "2")) {
		enable = ON;
		factory_mode = ON;
		pr_info("[SENSOR]: %s - factory mode\n", __func__);
	} else {
		pr_err("[SENSOR]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if ((enable == ON) && (data->recog_flag == OFF)) {
		pr_info("[SENSOR]: %s - reactive alert is on!\n", __func__);
		data->irq_state = 0;

		bma255_slope_enable(data->client, ON, factory_mode);
		enable_irq(data->irq1);
		enable_irq_wake(data->irq1);

		if (atomic_read(&data->enable) == OFF)
			bma255_set_mode(data->client, BMA255_MODE_LOWPOWER1);

		data->recog_flag = ON;
	} else if ((enable == OFF) && (data->recog_flag == ON)) {
		pr_info("[SENSOR]: %s - reactive alert is off! irq = %d\n",
			__func__, data->irq_state);

		bma255_slope_enable(data->client, OFF, factory_mode);

		disable_irq_wake(data->irq1);
		disable_irq_nosync(data->irq1);

		if (atomic_read(&data->enable) == OFF)
			bma255_set_mode(data->client, BMA255_MODE_SUSPEND);

		data->recog_flag = OFF;
	}

	return size;
}

static ssize_t bma255_reactive_alert_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
	return snprintf(buf, PAGE_SIZE, "%d\n", data->irq_state);
}

static DEVICE_ATTR(name, S_IRUGO, bma255_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, bma255_vendor_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	bma255_calibration_show, bma255_calibration_store);
static DEVICE_ATTR(raw_data, S_IRUGO, bma255_raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
	bma255_reactive_alert_show, bma255_reactive_alert_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	NULL,
};

static irqreturn_t bma255_irq_thread(int irq, void *bma255_data_p)
{
	struct bma255_p *data = bma255_data_p;

	pr_info("###################### [SENSOR]: %s reactive irq\n", __func__);

	bma255_set_int_enable(data->client, SLOPE_X_INT, OFF);
	bma255_set_int_enable(data->client, SLOPE_Y_INT, OFF);
	bma255_set_int_enable(data->client, SLOPE_Z_INT, OFF);

	data->irq_state = 1;
	wake_lock_timeout(&data->reactive_wake_lock, msecs_to_jiffies(2000));

	return IRQ_HANDLED;
}

static int bma255_setup_pin(struct bma255_p *data)
{
	int ret;

	ret = gpio_request(data->acc_int1, "ACC_INT1");
	if (ret < 0) {
		pr_err("[SENSOR] %s - gpio %d request failed (%d)\n",
			__func__, data->acc_int1, ret);
		goto exit;
	}

	ret = gpio_direction_input(data->acc_int1);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->acc_int1, ret);
		goto exit_acc_int1;
	}

	// ret = gpio_request(data->acc_int2, "ACC_INT2");
	// if (ret < 0) {
		// pr_err("[SENSOR]: %s - gpio %d request failed (%d)\n",
			// __func__, data->acc_int2, ret);
		// goto exit_acc_int1;
	// }

	// ret = gpio_direction_input(data->acc_int2);
	// if (ret < 0) {
		// pr_err("[SENSOR]: %s - failed to set gpio %d as input (%d)\n",
			// __func__, data->acc_int2, ret);
		// goto exit_acc_int2;
	// }

	wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND,
		       "reactive_wake_lock");

	data->irq1 = gpio_to_irq(data->acc_int1);
	ret = request_threaded_irq(data->irq1, NULL, bma255_irq_thread,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT, "bma255_accel", data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - can't allocate irq.\n", __func__);
		goto exit_reactive_irq;
	}

	disable_irq(data->irq1);
	goto exit;

exit_reactive_irq:
	wake_lock_destroy(&data->reactive_wake_lock);
// exit_acc_int2:
	// gpio_free(data->acc_int2);
exit_acc_int1:
	gpio_free(data->acc_int1);
exit:
	return ret;
}

static int bma255_input_init(struct bma255_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	//dev->name = MODULE_NAME;
	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_ABS, ABS_RUDDER);
	input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);

	// input_set_capability(dev, EV_ABS, ABS_X);
	// input_set_capability(dev, EV_ABS, ABS_Y);
	// input_set_capability(dev, EV_ABS, ABS_Z);
	// input_set_capability(dev, EV_REL, REL_X);
	// input_set_capability(dev, EV_REL, REL_Y);
	// input_set_capability(dev, EV_REL, REL_Z);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}
	data->input = dev;

	ret = sysfs_create_group(&data->input->dev.kobj, &bma255_attribute_group);
	if (ret < 0) {
        input_unregister_device(dev);
		return ret;
	}

	ret = sensors_initialize_symlink(data->input);
	if (ret) {
		pr_err("%s: cound not make bma250 accelerometer sensor symlink(%d).\n",
			__func__, ret);
		//goto err_sensors_initialize_symlink;
	}
	/* sysfs node creation */
	return 0;
	/*err_sensors_initialize_symlink:
		sensors_delete_symlink(dev);*/
}

static int bma255_parse_dt(struct bma255_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;


	/*scl,sda and irq */
	data->i2c_pull_up = of_property_read_bool(dNode, "bma255-i2c,i2c-pull-up");

	data->str_l6 = of_get_property(dNode,"bma255-i2c,reg_vdd", NULL);
	data->str_lvs1 = of_get_property(dNode,"bma255-i2c,reg_vio", NULL);
	data->used_chip = of_property_read_u32(dNode,"bma255-i2c,used_chip", &data->used_chip_flags);


	printk(KERN_INFO "%s pull-up:%d  \n", __func__, data->i2c_pull_up);

	data->acc_int1 = of_get_named_gpio(dNode,
		"bma255-i2c,acc_int1-gpio", 0);
	if (data->acc_int1 < 0) {
		pr_err("[SENSOR]: %s - get acc_int1 error\n", __func__);
		return -ENODEV;
	}

	data->acc_int2 = of_get_named_gpio_flags(dNode,
		"bma255-i2c,acc_int2-gpio", 0, &flags);
	if (data->acc_int2 < 0) {
		pr_err("[SENSOR]: %s - acc_int2 error\n", __func__);
		//return -ENODEV;
	}

	if (of_property_read_u32(dNode,
			"bma255-i2c,chip_pos", &data->chip_pos) < 0)
		data->chip_pos = BMA255_TOP_LOWER_RIGHT;

#if defined(CONFIG_MACH_MS01_LTE_KOR)
	data->chip_pos = BMA255_BOTTOM_UPPER_LEFT;
#endif

	return 0;
}

#if defined(CONFIG_MACH_CS03_SGLTE)
static void bma255_power_enable(int en)
{
	  int rc;
	  static struct regulator* ldo19;
	  printk(KERN_ERR "%s %s\n", __func__, (en) ? "on" : "off");
	if(!ldo19){
		ldo19 = regulator_get(NULL,"8226_l19");
		rc = regulator_set_voltage(ldo19,2850000,2850000);
		pr_info("[TMP] %s, %d\n", __func__, __LINE__);

		if (rc){
			printk(KERN_ERR "%s: bma254 set_level failed (%d)\n",__func__, rc);
		}
	}

	if(en){
		rc = regulator_enable(ldo19);
		if (rc){
			printk(KERN_ERR "%s: bma254 enable failed (%d)\n",__func__, rc);
		}
	}
	else{
		rc = regulator_disable(ldo19);
		if (rc){
			printk(KERN_ERR "%s: bma254 disable failed (%d)\n",__func__, rc);
		}
	}
return;
}
#else
int bma255_power_on(struct bma255_p *data, bool onoff)
{
	int ret = -1;
	if (!data->l19) {
		data->l19 = regulator_get(&data->client->dev, "8226_l19");
		if (!data->l19) {
			pr_err("%s: regulator pointer null l19, rc=%d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_set_voltage(data->l19, 2850000, 2850000);
		if (ret) {
			pr_err("%s: set voltage failed on l19, rc=%d\n",
				__func__, ret);
			return ret;
		}
	}
#if defined(CONFIG_MACH_MS01_LTE) || defined(CONFIG_MACH_MS01_LTE_KOR)
	if (!data->lvs1_1p8) {
		data->lvs1_1p8 = regulator_get(&data->client->dev, "8226_lvs1");
		if(!data->lvs1_1p8){
			pr_err("%s: regulator_get for 8226_lvs1 failed\n", __func__);
			return 0;
		}
	}
#endif
	if(onoff){
		ret = regulator_enable(data->l19);
		if (ret) {
			pr_err("%s: Failed to enable regulator l19.\n",
				__func__);
			return ret;
		}
#if defined(CONFIG_MACH_MS01_LTE) || defined(CONFIG_MACH_MS01_LTE_KOR)
		ret = regulator_enable(data->lvs1_1p8);
		if (ret) {
			pr_err("%s: Failed to enable regulator lvs1_1p8.\n",
				__func__);
			return ret;
		}
#endif
	}
	else {
		ret = regulator_disable(data->l19);
		if (ret) {
			pr_err("%s: Failed to disable regulatorl19.\n",
				__func__);
			return ret;
		}
#if defined(CONFIG_MACH_MS01_LTE_KOR)|| defined(CONFIG_MACH_MS01_LTE)
		ret = regulator_disable(data->lvs1_1p8);
		if (ret) {
			pr_err("%s: Failed to disable regulator lvs1_1p8.\n",
				__func__);
			return ret;
		}
#endif
	}
	return 0;
}
#endif
static int bma255_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct bma255_p *data = NULL;

	pr_info("##########################################################\n");
	pr_info("[SENSOR]: %s - Probe Start!\n", __func__);
	
#if defined(CONFIG_MACH_CS03_SGLTE)
	bma255_power_enable(1);
	mdelay(10);
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	data = kzalloc(sizeof(struct bma255_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SENSOR]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	ret = bma255_parse_dt(data, &client->dev);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - of_node error\n", __func__);
		ret = -ENODEV;
		goto exit_of_node;
	}

	/* read chip id */
	ret = i2c_smbus_read_word_data(client, BMA255_CHIP_ID_REG);
	if ((ret & 0x00ff) != BMA255_CHIP_ID) {
		pr_err("[SENSOR]: %s - chip id failed %d\n", __func__, ret);
		ret = -ENODEV;
		goto exit_read_chipid;
	}
	pr_err("[SENSOR]: %s - chip id is:  %d\n", __func__, ret);

	i2c_set_clientdata(client, data);
	data->client = client;
#if !defined(CONFIG_MACH_CS03_SGLTE)
	bma255_power_on(data, 1);
#endif
	ret = bma255_setup_pin(data);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	/* input device init */
	ret = bma255_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	ret=sensors_register(data->factory_device, data, sensor_attrs, MODULE_NAME);
	if (ret < 0)
		goto exit_input_init;
	/* workqueue init */
	INIT_DELAYED_WORK(&data->work, bma255_work_func);
	atomic_set(&data->delay, BMA255_DEFAULT_DELAY);
	atomic_set(&data->enable, OFF);
	data->time_count = 0;
	data->irq_state = 0;
	data->recog_flag = OFF;

	bma255_set_bandwidth(data->client, BMA255_BW_125HZ);
	bma255_set_range(data->client, BMA255_RANGE_2G);
	bma255_set_mode(data->client, BMA255_MODE_SUSPEND);

	pr_info("[SENSOR]: %s - Probe done!(chip pos : %d)\n",
		__func__, data->chip_pos);

	return 0;
exit_input_init:
	free_irq(data->irq1, data);
	wake_lock_destroy(&data->reactive_wake_lock);
	gpio_free(data->acc_int2);
	gpio_free(data->acc_int1);
exit_read_chipid:
exit_setup_pin:
exit_of_node:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SENSOR]: %s - Probe fail!\n", __func__);
	return ret;
}

static int __devexit bma255_remove(struct i2c_client *client)
{
	struct bma255_p *data = (struct bma255_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		bma255_set_enable(data, OFF);

	cancel_delayed_work_sync(&data->work);
	sensors_unregister(data->factory_device);
	//sensors_delete_symlink(data->input->dev);

	sysfs_remove_group(&data->input->dev.kobj, &bma255_attribute_group);
	input_unregister_device(data->input);

	free_irq(data->irq1, data);
	wake_lock_destroy(&data->reactive_wake_lock);

	gpio_free(data->acc_int2);
	gpio_free(data->acc_int1);

	kfree(data);

	return 0;
}

static int bma255_suspend(struct device *dev)
{
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();

	if (atomic_read(&data->enable) == ON) {
		if (data->recog_flag == ON)
			bma255_set_mode(data->client, BMA255_MODE_LOWPOWER1);
		else
			bma255_set_mode(data->client, BMA255_MODE_SUSPEND);

		cancel_delayed_work_sync(&data->work);
	}
#if !defined(CONFIG_MACH_CS03_SGLTE)
		bma255_power_on(data, 0);
#endif
	return 0;
}

static int bma255_resume(struct device *dev)
{
	struct bma255_p *data = dev_get_drvdata(dev);
	//struct bma255_p *data = bma_acc_get_data();
#if !defined(CONFIG_MACH_CS03_SGLTE)
	bma255_power_on(data, 1);
#endif
	if (atomic_read(&data->enable) == ON) {
		bma255_set_mode(data->client, BMA255_MODE_NORMAL);
		schedule_delayed_work(&data->work,
			msecs_to_jiffies(atomic_read(&data->delay)));
	}

	return 0;
}

static struct of_device_id bma255_match_table[] = {
	{ .compatible = "bma255-i2c",},
	{},
};

static const struct i2c_device_id bma255_id[] = {
	{ "bma255_match_table", 0 },
	{ }
};

static const struct dev_pm_ops bma255_pm_ops = {
	.suspend = bma255_suspend,
	.resume = bma255_resume
};

static struct i2c_driver bma255_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = bma255_match_table,
		.pm = &bma255_pm_ops
	},
	.probe		= bma255_probe,
	.remove		= __devexit_p(bma255_remove),
	.id_table	= bma255_id,
};

static int __init BMA255_init(void)
{
	if (poweroff_charging) {
		printk("%s : LPM Charging Mode!!\n", __func__);
		return 0;
	}
	else
	return i2c_add_driver(&bma255_driver);
}

static void __exit BMA255_exit(void)
{
	i2c_del_driver(&bma255_driver);
}

module_init(BMA255_init);
module_exit(BMA255_exit);

MODULE_DESCRIPTION("bma255 accelerometer sensor driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
