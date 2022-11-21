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
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#include "sensors_core.h"

#define I2C_M_WR                      0 /* for i2c Write */
#define I2c_M_RD                      1 /* for i2c Read */
#define READ_DATA_LENTH               6

#define VENDOR_NAME                   "STM"
#define MODEL_NAME                    "K2HH"
#define MODULE_NAME                   "accelerometer_sensor"

#define CALIBRATION_FILE_PATH         "/efs/FactoryApp/accel_calibration_data"
#define CALIBRATION_DATA_AMOUNT       20
#define MAX_ACCEL_1G                  8192

#define K2HH_DEFAULT_DELAY            200000000LL

#define CHIP_ID_RETRIES               3
#define ACCEL_LOG_TIME                15 /* 15 sec */

#define K2HH_MODE_SUSPEND             0
#define K2HH_MODE_NORMAL              1

#define SENSITIVITY_2G                61
#define SENSITIVITY_4G                122
#define SENSITIVITY_8G                244

#define K2HH_RANGE_2G                 0
#define K2HH_RANGE_4G                 1
#define K2HH_RANGE_8G                 2

#define WHOAMI_REG                    0x0F
#define AXISDATA_REG                  0x28

#define CTRL1_REG                     0x20
#define CTRL2_REG                     0x21
#define CTRL3_REG                     0x22
#define CTRL4_REG                     0x23
#define CTRL5_REG                     0x24
#define CTRL6_REG                     0x25
#define CTRL7_REG                     0x26
#define STATUS_REG                    0x27

/* CTRL1 */
#define CTRL1_HR_DISABLE              0x00
#define CTRL1_HR_ENABLE               0x80
#define CTRL1_HR_MASK                 0x80
#define CTRL1_BDU_ENABLE              0x08
#define CTRL1_BDU_MASK                0x08

/* CTRL2 */
#define CTRL2_IG1_INT1                0x08

/* CTRL3 */
#define CTRL3_IG1_INT1                0x08

/* CTRL7 */
#define CTRL7_LIR2                    0x08
#define CTRL7_LIR1                    0x04

#define ACC_PM_OFF                    0x00
#define ACC_ENABLE_ALL_AXES           0x07

#define INT_CFG1_REG                  0x30
#define INT_SRC1_REG                  0x31
#define K2HH_CHIP_ID                  0x41

#define K2HH_ACC_FS_MASK              0x30
#define K2HH_ACC_ODR_MASK             0x70
#define K2HH_ACC_BW_MASK              0xC0
#define K2HH_ACC_AXES_MASK            0x07
#define K2HH_ACC_BW_SCALE_ODR_MASK    0x08

#define SELF_TEST_2G_MAX_LSB          24576
#define SELF_TEST_2G_MIN_LSB          1146

#define K2HH_ACC_FS_2G                0x00
#define K2HH_ACC_FS_4G                0x20
#define K2HH_ACC_FS_8G                0x30

#define K2HH_ACC_BW_50                0xC0
#define K2HH_ACC_BW_100               0x80
#define K2HH_ACC_BW_200               0x40
#define K2HH_ACC_BW_400               0x00

#define INT_THSX1_REG                 0x32
#define INT_THSY1_REG                 0x33
#define INT_THSZ1_REG                 0x34

#define K2HH_ACC_BW_SCALE_ODR_ENABLE  0x08
#define K2HH_ACC_BW_SCALE_ODR_DISABLE 0x00

#define DYNAMIC_THRESHOLD             5000

enum {
	OFF = 0,
	ON = 1
};

struct k2hh_v {
	union {
		s16 v[3];
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
	};
};

struct k2hh_p {
	struct wake_lock reactive_wake_lock;
	struct i2c_client *client;
#if defined(CONFIG_SENSORS_IIO)
	struct iio_trigger *trig;
	spinlock_t spin_lock;
#else
	struct input_dev *input;
#endif
	struct delayed_work irq_work;
	struct device *factory_device;
	struct k2hh_v accdata;
	struct k2hh_v caldata;
	struct mutex mode_mutex;
	struct hrtimer accel_timer;
	struct workqueue_struct *accel_wq;
	struct work_struct work_accel;
#ifdef CONFIG_SENSORS_K2HH_HAS_REGULATOR
	struct regulator *reg_vdd;
#endif
	ktime_t poll_delay;
	atomic_t enable;

	int recog_flag;
	int irq1;
	int irq_state;
	int acc_int1;
	int time_count;
#if defined(CONFIG_SENSORS_HAS_GPIO_LDO)
	int sensor_en;
#endif

	u8 odr;
	u8 hr;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	u64 old_timestamp;
};

#define ACC_ODR10		0x10	/*   10Hz output data rate */
#define ACC_ODR50		0x20	/*   50Hz output data rate */
#define ACC_ODR100		0x30	/*  100Hz output data rate */
#define ACC_ODR200		0x40	/*  200Hz output data rate */
#define ACC_ODR400		0x50	/*  400Hz output data rate */
#define ACC_ODR800		0x60	/*  800Hz output data rate */
#define ACC_ODR_MASK		0X70

struct k2hh_acc_odr {
	unsigned int cutoff_ms;
	unsigned int mask;
};

#define OUTPUT_ALWAYS_ANTI_ALIASED	/* Anti aliasing filter */

const struct k2hh_acc_odr k2hh_acc_odr_table[] = {
	{  2, ACC_ODR800},
	{  3, ACC_ODR400},
	{  5, ACC_ODR200},
	{ 10, ACC_ODR100},
#ifndef OUTPUT_ALWAYS_ANTI_ALIASED
	{ 20, ACC_ODR50},
	{100, ACC_ODR10},
#endif
};

#if defined(CONFIG_SENSORS_IIO)
enum {
	K2HH_SCAN_ACCEL_X,
	K2HH_SCAN_ACCEL_Y,
	K2HH_SCAN_ACCEL_Z,
	K2HH_SCAN_TIMESTAMP,
};

#define K2HH_ACC_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define K2HH_ACC_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

#define K2HH_ACCELEROMETER_CHANNEL(axis)	\
{							\
	.type = IIO_ACCEL,				\
	.modified = 1,				\
	.channel2 = IIO_MOD_##axis,			\
	.info_mask_separate = K2HH_ACC_INFO_SEPARATE_MASK,   \
	.info_mask_shared_by_type = K2HH_ACC_INFO_SHARED_MASK,\
	.scan_index = K2HH_SCAN_ACCEL_##axis,		\
	.scan_type = IIO_ST('s', 32, 32, 0)		\
}

#define IIO_BUFFER_6_BYTES	14 /* data 6 bytes + timestamp 8 bytes */

static const struct iio_chan_spec k2hh_channels[] = {
		K2HH_ACCELEROMETER_CHANNEL(X),
		K2HH_ACCELEROMETER_CHANNEL(Y),
		K2HH_ACCELEROMETER_CHANNEL(Z),
		IIO_CHAN_SOFT_TIMESTAMP(K2HH_SCAN_TIMESTAMP)
	};

static int k2hh_data_rdy_trig_poll(struct iio_dev *indio_dev);

static inline s64 k2hh_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

irqreturn_t k2hh_iio_pollfunc_store_time(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = k2hh_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}
#endif

static int k2hh_regulator_onoff(struct device *dev, bool onoff)
{
	int ret = 0;

	SENSOR_INFO("\n");

#if defined(CONFIG_SENSORS_K2HH_HAS_REGULATOR)
	vdd = devm_regulator_get(dev, "stm,reg_vdd");
	if (IS_ERR(vdd)) {
		SENSOR_ERR("could not get vdd, %ld\n", PTR_ERR(vdd));
		ret = -ENOMEM;
		goto err_vdd;
	}

	regulator_set_voltage(vdd, 2850000, 2850000);

	if (onoff) {
		ret = regulator_enable(vdd);
		if (ret)
			SENSOR_ERR("Failed to enable vdd.\n");
		msleep(30);
	} else {
		ret = regulator_disable(vdd);
		if (ret)
			SENSOR_ERR("Failed to disable vdd.\n");
	}
err_vdd:
#endif

	return ret;
}

static int k2hh_i2c_read(struct i2c_client *client, unsigned char reg_addr,
	unsigned char *buf, unsigned int len)
{
	int ret, retries = 0;
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret >= 0)
			break;
	} while (retries++ < 2);

	if (ret < 0) {
		SENSOR_ERR("i2c read error %d\n", ret);
		return ret;
	}

	return 0;
}

static int k2hh_i2c_write(struct i2c_client *client, unsigned char reg_addr,
	unsigned char buf)
{
	int ret, retries = 0;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;
	} while (retries++ < 2);

	if (ret < 0) {
		SENSOR_ERR("i2c write error %d\n", ret);
		return ret;
	}

	return 0;
}

static int k2hh_read_accel_xyz(struct k2hh_p *data, struct k2hh_v *acc)
{
	int ret = 0;
	struct k2hh_v rawdata;
	unsigned char buf[READ_DATA_LENTH];

	ret = k2hh_i2c_read(data->client, AXISDATA_REG, buf, READ_DATA_LENTH);
	if (ret < 0)
		goto exit;

	rawdata.v[0] = ((s16) ((buf[1] << 8) | buf[0]));
	rawdata.v[1] = ((s16) ((buf[3] << 8) | buf[2]));
	rawdata.v[2] = ((s16) ((buf[5] << 8) | buf[4]));

	acc->v[0] = ((data->negate_x) ? (-rawdata.v[data->axis_map_x]) :
		(rawdata.v[data->axis_map_x]));
	acc->v[1] = ((data->negate_y) ? (-rawdata.v[data->axis_map_y]) :
		(rawdata.v[data->axis_map_y]));
	acc->v[2] = ((data->negate_z) ? (-rawdata.v[data->axis_map_z]) :
		(rawdata.v[data->axis_map_z]));

exit:
	return ret;
}

static int k2hh_set_range(struct k2hh_p *data, unsigned char range)
{
	int ret = 0;
	unsigned char temp, new_range, buf, mask;

	switch (range) {
	case K2HH_RANGE_2G:
		new_range = K2HH_ACC_FS_2G;
		break;
	case K2HH_RANGE_4G:
		new_range = K2HH_ACC_FS_4G;
		break;
	case K2HH_RANGE_8G:
		new_range = K2HH_ACC_FS_8G;
		break;
	default:
		new_range = K2HH_ACC_FS_2G;
		break;
	}

	mask = K2HH_ACC_FS_MASK;
	ret = k2hh_i2c_read(data->client, CTRL4_REG, &temp, 1);

	buf = (mask & new_range) | ((~mask) & temp);
	ret += k2hh_i2c_write(data->client, CTRL4_REG, buf);
	SENSOR_INFO("0x%x\n", new_range);

	return ret;
}

static int k2hh_set_odr(struct k2hh_p *data)
{
	int ret = 0, i;
	unsigned char buf, new_odr, mask, temp;

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(k2hh_acc_odr_table) - 1; i >= 0; i--) {
		if ((k2hh_acc_odr_table[i].cutoff_ms <=
			ktime_to_ms(data->poll_delay)) || (i == 0))
			break;
	}

	if (data->recog_flag == ON)
		i = ARRAY_SIZE(k2hh_acc_odr_table) - 1;

	new_odr = k2hh_acc_odr_table[i].mask;

	mask = K2HH_ACC_ODR_MASK;
	ret = k2hh_i2c_read(data->client, CTRL1_REG, &temp, 1);
	buf = ((mask & new_odr) | ((~mask) & temp));
	ret += k2hh_i2c_write(data->client, CTRL1_REG, buf);

	data->odr = new_odr;

	SENSOR_INFO("change odr %d\n", i);

	return ret;
}

static int k2hh_set_bw(struct k2hh_p *data)
{
	int ret = 0;
	unsigned char temp, buf, mask, new_range;

#if defined(OUTPUT_ALWAYS_ANTI_ALIASED)
	new_range = K2HH_ACC_BW_SCALE_ODR_ENABLE;
	mask = K2HH_ACC_BW_SCALE_ODR_MASK;
	ret = k2hh_i2c_read(data->client, CTRL4_REG, &temp, 1);

	buf = (mask & new_range) | ((~mask) & temp);
	ret += k2hh_i2c_write(data->client, CTRL4_REG, buf);
	new_range = K2HH_ACC_BW_50;
#else
	new_range = K2HH_ACC_BW_400;
#endif
	mask = K2HH_ACC_BW_MASK;
	ret = k2hh_i2c_read(data->client, CTRL4_REG, &temp, 1);

	buf = (mask & new_range) | ((~mask) & temp);
	ret += k2hh_i2c_write(data->client, CTRL4_REG, buf);

	return ret;
}

static int k2hh_set_hr(struct k2hh_p *data, int set)
{
	int ret;
	u8 buf, bw, odr;

	SENSOR_INFO("%d\n", set);

	if (set) {
		data->hr = CTRL1_HR_ENABLE;
		odr = data->odr;
		k2hh_set_bw(data);
	} else {
		data->hr = CTRL1_HR_DISABLE;
		odr = ACC_ODR800;
		bw = K2HH_ACC_BW_400;
		k2hh_i2c_read(data->client, CTRL4_REG, &buf, 1);
		buf = (K2HH_ACC_BW_MASK & bw) | ((~K2HH_ACC_BW_MASK) & buf);
		k2hh_i2c_write(data->client, CTRL4_REG, buf);

#if defined(OUTPUT_ALWAYS_ANTI_ALIASED)
		bw = K2HH_ACC_BW_SCALE_ODR_DISABLE;
		k2hh_i2c_read(data->client, CTRL4_REG, &buf, 1);
		buf = (K2HH_ACC_BW_SCALE_ODR_MASK & bw) |
			((~K2HH_ACC_BW_SCALE_ODR_MASK) & buf);
		k2hh_i2c_write(data->client, CTRL4_REG, buf);
#endif
	}

	ret = k2hh_i2c_read(data->client, CTRL1_REG, &buf, 1);
	buf = data->hr | ((~CTRL1_HR_MASK) & buf);
	ret += k2hh_i2c_write(data->client, CTRL1_REG, buf);

	ret += k2hh_i2c_read(data->client, CTRL1_REG, &buf, 1);
	buf = ((K2HH_ACC_ODR_MASK & odr) | ((~K2HH_ACC_ODR_MASK) & buf));
	ret += k2hh_i2c_write(data->client, CTRL1_REG, buf);

	return ret;
}

static void k2hh_set_enable(struct k2hh_p *data, int enable)
{
	if (enable == ON) {
		hrtimer_start(&data->accel_timer, data->poll_delay,
		      HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&data->accel_timer);
		cancel_work_sync(&data->work_accel);
	}
}

static int k2hh_set_mode(struct k2hh_p *data, unsigned char mode)
{
	int ret = 0;
	unsigned char buf, mask, temp;

	mutex_lock(&data->mode_mutex);

	switch (mode) {
	case K2HH_MODE_NORMAL:
		mask = K2HH_ACC_ODR_MASK;
		ret = k2hh_i2c_read(data->client, CTRL1_REG, &temp, 1);
		buf = ((mask & data->odr) | ((~mask) & temp));
		buf = data->hr | ((~CTRL1_HR_MASK) & buf);
		ret += k2hh_i2c_write(data->client, CTRL1_REG, buf);
		break;
	case K2HH_MODE_SUSPEND:
		if (data->recog_flag == ON)
			break;

		mask = K2HH_ACC_ODR_MASK;
		ret = k2hh_i2c_read(data->client, CTRL1_REG, &temp, 1);
		buf = ((mask & ACC_PM_OFF) | ((~mask) & temp));
		ret += k2hh_i2c_write(data->client, CTRL1_REG, buf);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&data->mode_mutex);
	SENSOR_INFO("change mode %u\n", mode);

	return ret;
}

static int k2hh_open_calibration(struct k2hh_p *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		SENSOR_ERR("No Calibration\n");

		return ret;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)&data->caldata.v,
		3 * sizeof(s16), &cal_filp->f_pos);
	if (ret != 3 * sizeof(s16)) {
		SENSOR_ERR("Can't read the cal data\n");
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	SENSOR_INFO("open accel calibration %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	if ((data->caldata.x == 0) && (data->caldata.y == 0)
		&& (data->caldata.z == 0))
		return -EIO;

	return ret;
}

static int k2hh_do_calibrate(struct k2hh_p *data, int enable)
{
	int sum[3] = { 0, };
	int ret = 0, cnt;
	struct file *cal_filp = NULL;
	struct k2hh_v acc;
	mm_segment_t old_fs;

	data->caldata.x = 0;
	data->caldata.y = 0;
	data->caldata.z = 0;

	if (enable) {
		if (atomic_read(&data->enable) == ON)
			k2hh_set_enable(data, OFF);
		else
			k2hh_set_mode(data, K2HH_MODE_NORMAL);

		msleep(300);

		for (cnt = 0; cnt < CALIBRATION_DATA_AMOUNT; cnt++) {
			k2hh_read_accel_xyz(data, &acc);
			sum[0] += acc.x;
			sum[1] += acc.y;
			sum[2] += acc.z;
			msleep(20);
		}

		if (atomic_read(&data->enable) == ON)
			k2hh_set_enable(data, ON);
		else
			k2hh_set_mode(data, K2HH_MODE_SUSPEND);

		data->caldata.x = (sum[0] / CALIBRATION_DATA_AMOUNT);
		data->caldata.y = (sum[1] / CALIBRATION_DATA_AMOUNT);
		data->caldata.z = (sum[2] / CALIBRATION_DATA_AMOUNT);

		if (data->caldata.z > 0)
			data->caldata.z -= MAX_ACCEL_1G;
		else if (data->caldata.z < 0)
			data->caldata.z += MAX_ACCEL_1G;
	}

	SENSOR_INFO("do accel calibrate %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if (IS_ERR(cal_filp)) {
		SENSOR_ERR("Can't open calibration file\n");
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)&data->caldata.v,
		3 * sizeof(s16), &cal_filp->f_pos);
	if (ret != 3 * sizeof(s16)) {
		SENSOR_ERR("Can't write the caldata to file\n");
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static enum hrtimer_restart k2hh_timer_func(struct hrtimer *timer)
{
	struct k2hh_p *data = container_of(timer,
					struct k2hh_p, accel_timer);

	if (!work_pending(&data->work_accel))
		queue_work(data->accel_wq, &data->work_accel);

	hrtimer_forward_now(&data->accel_timer, data->poll_delay);

	return HRTIMER_RESTART;
}

static void k2hh_work_func(struct work_struct *work)
{
	struct k2hh_v acc;
	struct k2hh_p *data = container_of(work, struct k2hh_p, work_accel);
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev = iio_priv_to_dev(data);
	u8 buf[IIO_BUFFER_6_BYTES];
#else
	struct timespec ts = ktime_to_timespec(ktime_get_boottime());
	u64 timestamp_new = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	u64 timestamp;
	int time_hi, time_lo;
#endif
	int ret;

	ret = k2hh_read_accel_xyz(data, &acc);
	if (ret < 0)
		goto exit;

	data->accdata.x = acc.x - data->caldata.x;
	data->accdata.y = acc.y - data->caldata.y;
	data->accdata.z = acc.z - data->caldata.z;

#if defined(CONFIG_SENSORS_IIO)
	memcpy(buf, &data->accdata.x, sizeof(data->accdata.x));
	memcpy(buf + 2, &data->accdata.y, sizeof(data->accdata.y));
	memcpy(buf + 4, &data->accdata.z, sizeof(data->accdata.z));
	k2hh_data_rdy_trig_poll(indio_dev);
#else
	if (((timestamp_new - data->old_timestamp) > ktime_to_ms(data->poll_delay) * 1800000LL)
			&& (data->old_timestamp != 0)) {
		timestamp = (timestamp_new + data->old_timestamp) >>  1;
		time_hi = (int)((timestamp & TIME_HI_MASK) >> TIME_HI_SHIFT);
		time_lo = (int)(timestamp & TIME_LO_MASK);

		input_report_rel(data->input, REL_X, data->accdata.x);
		input_report_rel(data->input, REL_Y, data->accdata.y);
		input_report_rel(data->input, REL_Z, data->accdata.z);
		input_report_rel(data->input, REL_DIAL, time_hi);
		input_report_rel(data->input, REL_MISC, time_lo);
		input_sync(data->input);
	}

	time_hi = (int)((timestamp_new & TIME_HI_MASK) >> TIME_HI_SHIFT);
	time_lo = (int)(timestamp_new & TIME_LO_MASK);

	input_report_rel(data->input, REL_X, data->accdata.x);
	input_report_rel(data->input, REL_Y, data->accdata.y);
	input_report_rel(data->input, REL_Z, data->accdata.z);
	input_report_rel(data->input, REL_DIAL, time_hi);
	input_report_rel(data->input, REL_MISC, time_lo);
	input_sync(data->input);

	data->old_timestamp = timestamp_new;
#endif

exit:
	if ((ktime_to_ns(data->poll_delay) * (int64_t)data->time_count)
		>= ((int64_t)ACCEL_LOG_TIME * NSEC_PER_SEC)) {
		SENSOR_INFO("x = %d, y = %d, z = %d (ra:%d)\n",
			data->accdata.x, data->accdata.y,
			data->accdata.z, data->recog_flag);
		data->time_count = 0;
	} else
		data->time_count++;
}

static ssize_t k2hh_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(dev_get_drvdata(dev));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t k2hh_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(dev_get_drvdata(dev));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif
	u8 enable;
	int ret, pre_enable;

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		SENSOR_ERR("Invalid Argument\n");
		return ret;
	}

	pre_enable = atomic_read(&data->enable);
	SENSOR_INFO("pre_enable = %d, enable = %d\n", pre_enable, enable);

	if (enable) {
		if (pre_enable == OFF) {
			data->old_timestamp = 0LL;
			k2hh_regulator_onoff(&data->client->dev, true);

			/* Access to calibration data in filesystem
			 * only when there are no caldata */
			if (data->caldata.x == 0 && data->caldata.y == 0
					&& data->caldata.z == 0)
				k2hh_open_calibration(data);
			k2hh_set_range(data, K2HH_RANGE_4G);
			k2hh_set_bw(data);

			k2hh_set_mode(data, K2HH_MODE_NORMAL);
			atomic_set(&data->enable, ON);
			k2hh_set_enable(data, ON);
		}
	} else {
		if (pre_enable == ON) {
			atomic_set(&data->enable, OFF);
			k2hh_set_mode(data, K2HH_MODE_SUSPEND);
			k2hh_set_enable(data, OFF);

			k2hh_regulator_onoff(&data->client->dev, false);
		}
	}

	return size;
}

static ssize_t k2hh_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(dev_get_drvdata(dev));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif

	return snprintf(buf, PAGE_SIZE, "%lld\n",
			ktime_to_ns(data->poll_delay));
}

static ssize_t k2hh_delay_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(dev_get_drvdata(dev));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif
	int ret;
	int64_t delay;

	ret = kstrtoll(buf, 10, &delay);
	if (ret) {
		SENSOR_ERR("Invalid Argument\n");
		return ret;
	}

	if (delay > K2HH_DEFAULT_DELAY)
		delay = K2HH_DEFAULT_DELAY;

	data->poll_delay = ns_to_ktime(delay);
	k2hh_set_odr(data);

	if (atomic_read(&data->enable) == ON) {
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);
		k2hh_set_mode(data, K2HH_MODE_NORMAL);
	}

	SENSOR_INFO("poll_delay = %lld\n", delay);
	return size;
}

#if defined(CONFIG_SENSORS_IIO)
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_delay_show, k2hh_delay_store, 0);
static IIO_DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_enable_show, k2hh_enable_store, 0);

static struct attribute *k2hh_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_enable.dev_attr.attr,
	NULL
};
#else
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_delay_show, k2hh_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_enable_show, k2hh_enable_store);

static struct attribute *k2hh_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	NULL
};
#endif

static struct attribute_group k2hh_attribute_group = {
	.attrs = k2hh_attributes
};

static ssize_t k2hh_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t k2hh_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t k2hh_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct k2hh_p *data = dev_get_drvdata(dev);

	ret = k2hh_open_calibration(data);
	if (ret < 0)
		SENSOR_ERR("calibration open failed(%d)\n", ret);

	SENSOR_INFO("cal data %d %d %d - ret : %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z, ret);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", ret,
		data->caldata.x, data->caldata.y, data->caldata.z);
}

static ssize_t k2hh_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int64_t d_enable;
	struct k2hh_p *data = dev_get_drvdata(dev);

	ret = kstrtoll(buf, 10, &d_enable);
	if (ret < 0)
		return ret;

	ret = k2hh_do_calibrate(data, (int)d_enable);
	if (ret < 0)
		SENSOR_ERR("accel calibrate failed\n");

	return size;
}

static ssize_t k2hh_lowpassfilter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct k2hh_p *data = dev_get_drvdata(dev);

	if (data->hr == CTRL1_HR_ENABLE)
		ret = 1;
	else
		ret = 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t k2hh_lowpassfilter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int64_t d_enable;
	struct k2hh_p *data = dev_get_drvdata(dev);

	SENSOR_INFO("\n");

	ret = kstrtoll(buf, 10, &d_enable);
	if (ret < 0)
		SENSOR_ERR("kstrtoll failed\n");

	ret = k2hh_set_hr(data, d_enable);
	if (ret < 0)
		SENSOR_ERR("set_hr failed\n");

	return size;
}

static ssize_t k2hh_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct k2hh_v acc;
	struct k2hh_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == OFF) {
		k2hh_set_mode(data, K2HH_MODE_NORMAL);
		msleep(20);
		k2hh_read_accel_xyz(data, &acc);
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);

		acc.x = acc.x - data->caldata.x;
		acc.y = acc.y - data->caldata.y;
		acc.z = acc.z - data->caldata.z;
	} else
		acc = data->accdata;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		acc.x, acc.y, acc.z);
}

static ssize_t k2hh_reactive_alert_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct k2hh_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->irq_state);
}

static ssize_t k2hh_reactive_alert_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char threshx, threshy, threshz;
	int enable = OFF, factory_mode = OFF;
	struct k2hh_v acc;
	struct k2hh_p *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "0")) {
		enable = OFF;
		factory_mode = OFF;
		SENSOR_INFO("disable\n");
	} else if (sysfs_streq(buf, "1")) {
		enable = ON;
		factory_mode = OFF;
		SENSOR_INFO("enable\n");
	} else if (sysfs_streq(buf, "2")) {
		enable = ON;
		factory_mode = ON;
		SENSOR_INFO("factory mode\n");
	} else {
		SENSOR_ERR("invalid value %d\n", *buf);
		return -EINVAL;
	}

	if ((enable == ON) && (data->recog_flag == OFF)) {
		data->irq_state = 0;
		data->recog_flag = ON;

		if (factory_mode == ON) {
			k2hh_i2c_write(data->client, INT_THSX1_REG, 0x00);
			k2hh_i2c_write(data->client, INT_THSY1_REG, 0x00);
			k2hh_i2c_write(data->client, INT_THSZ1_REG, 0x00);
			k2hh_i2c_write(data->client, INT_CFG1_REG, 0x3f);
		} else {
			k2hh_set_odr(data);
			if (atomic_read(&data->enable) == OFF) {
				k2hh_set_mode(data, K2HH_MODE_NORMAL);
				msleep(20);
				k2hh_read_accel_xyz(data, &acc);
				k2hh_set_mode(data, K2HH_MODE_SUSPEND);
			} else {
				acc.x = data->accdata.x;
				acc.y = data->accdata.y;
				acc.z = data->accdata.z;
			}

			threshx = (abs(acc.v[data->axis_map_x])
					+ DYNAMIC_THRESHOLD) >> 8;
			threshy = (abs(acc.v[data->axis_map_y])
					+ DYNAMIC_THRESHOLD) >> 8;
			threshz = (abs(acc.v[data->axis_map_z])
					+ DYNAMIC_THRESHOLD) >> 8;

			k2hh_i2c_write(data->client, INT_THSX1_REG, threshx);
			k2hh_i2c_write(data->client, INT_THSY1_REG, threshy);
			k2hh_i2c_write(data->client, INT_THSZ1_REG, threshz);
			k2hh_i2c_write(data->client, INT_CFG1_REG, 0x0a);
		}

		k2hh_i2c_write(data->client, CTRL7_REG, CTRL7_LIR1);
		k2hh_i2c_write(data->client, CTRL3_REG, CTRL3_IG1_INT1);

		enable_irq(data->irq1);
		enable_irq_wake(data->irq1);

		SENSOR_INFO("reactive alert is on!\n");
	} else if ((enable == OFF) && (data->recog_flag == ON)) {
		k2hh_i2c_write(data->client, CTRL3_REG, 0x00);

		disable_irq_wake(data->irq1);
		disable_irq_nosync(data->irq1);
		data->recog_flag = OFF;
		SENSOR_INFO("reactive alert is off! irq = %d\n",
			data->irq_state);
	}

	return size;
}

static ssize_t k2hh_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct k2hh_p *data = dev_get_drvdata(dev);
	struct k2hh_v acc;
	unsigned char temp, backup[4];
	int result = 1, i, retry;
	ssize_t ret;
	s32 NO_ST[3] = {0, 0, 0};
	s32 ST[3] = {0, 0, 0};

	k2hh_i2c_read(data->client, CTRL1_REG, &backup[0], 1);
	k2hh_i2c_read(data->client, CTRL4_REG, &backup[1], 1);
	k2hh_i2c_read(data->client, CTRL5_REG, &backup[2], 1);
	k2hh_i2c_read(data->client, CTRL6_REG, &backup[3], 1);

	if (atomic_read(&data->enable) == OFF)
		k2hh_set_mode(data, K2HH_MODE_NORMAL);
	else
		k2hh_set_enable(data, OFF);

	k2hh_i2c_write(data->client, CTRL1_REG, 0x3f);
	k2hh_i2c_write(data->client, CTRL4_REG, 0x04);
	k2hh_i2c_write(data->client, CTRL5_REG, 0x00);
	k2hh_i2c_write(data->client, CTRL6_REG, 0x00);

	msleep(80);

	k2hh_read_accel_xyz(data, &acc);

	for (i = 0; i < 5; i++) {
		retry = 5;
		do {
			ret = k2hh_i2c_read(data->client,
					STATUS_REG, &temp, 1);
			if (ret < 0) {
				SENSOR_ERR("i2c error");
				goto exit_status_err;
			}

			if (temp & 0x08)
				break;

			msleep(20);
		} while (retry-- >= 0);

		if (retry < 0) {
			SENSOR_ERR("failed to update data\n");
			goto exit_status_err;
		}

		k2hh_read_accel_xyz(data, &acc);

		NO_ST[0] += acc.x;
		NO_ST[1] += acc.y;
		NO_ST[2] += acc.z;
	}

	NO_ST[0] /= 5;
	NO_ST[1] /= 5;
	NO_ST[2] /= 5;

	k2hh_i2c_write(data->client, CTRL5_REG, 0x04);

	msleep(80);

	k2hh_read_accel_xyz(data, &acc);

	for (i = 0; i < 5; i++) {
		retry = 5;
		do {
			ret = k2hh_i2c_read(data->client,
					STATUS_REG, &temp, 1);
			if (ret < 0) {
				SENSOR_ERR("i2c error");
				goto exit_status_err;
			}

			if (temp & 0x08)
				break;

			msleep(20);
		} while (retry-- >= 0);

		if (retry < 0) {
			SENSOR_ERR("failed to update data\n");
			goto exit_status_err;
		}

		k2hh_read_accel_xyz(data, &acc);
		ST[0] += acc.x;
		ST[1] += acc.y;
		ST[2] += acc.z;
	}

	ST[0] /= 5;
	ST[1] /= 5;
	ST[2] /= 5;

	for (i = 0; i < 3; i++) {
		ST[i] -= NO_ST[i];
		ST[i] = abs(ST[i]);

		if ((SELF_TEST_2G_MIN_LSB > ST[i])
			|| (ST[i] > SELF_TEST_2G_MAX_LSB)) {
			SENSOR_ERR("%d Out of range!! (%d)\n", i, ST[i]);
			result = 0;
		}
	}

	if (result)
		ret = snprintf(buf, 0xff, "1,%d,%d,%d\n", ST[0], ST[1], ST[2]);
	else
		ret = snprintf(buf, 0xff, "0,%d,%d,%d\n", ST[0], ST[1], ST[2]);

	goto exit;

exit_status_err:
	ret = snprintf(buf, 0xff, "-1,0,0,0\n");
exit:
	k2hh_i2c_write(data->client, CTRL1_REG, 0x00);
	k2hh_i2c_write(data->client, CTRL5_REG, 0x00);

	k2hh_i2c_write(data->client, CTRL1_REG, backup[0]);
	k2hh_i2c_write(data->client, CTRL4_REG, backup[1]);
	k2hh_i2c_write(data->client, CTRL5_REG, backup[2]);
	k2hh_i2c_write(data->client, CTRL6_REG, backup[3]);

	if (atomic_read(&data->enable) == OFF) {
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);
	} else {
		k2hh_set_mode(data, K2HH_MODE_NORMAL);
		k2hh_set_enable(data, ON);
	}

	return ret;
}

static DEVICE_ATTR(selftest, S_IRUGO, k2hh_selftest_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, k2hh_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, k2hh_vendor_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_calibration_show, k2hh_calibration_store);
static DEVICE_ATTR(lowpassfilter, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_lowpassfilter_show, k2hh_lowpassfilter_store);
static DEVICE_ATTR(raw_data, S_IRUGO, k2hh_raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
	k2hh_reactive_alert_show, k2hh_reactive_alert_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_lowpassfilter,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	&dev_attr_selftest,
	NULL,
};

static void k2hh_irq_work_func(struct work_struct *work)
{
	struct k2hh_p *data = container_of((struct delayed_work *)work,
		struct k2hh_p, irq_work);
	unsigned char buf;

	k2hh_i2c_write(data->client, INT_CFG1_REG, 0x00);
	k2hh_i2c_read(data->client, INT_SRC1_REG, &buf, 1);
}

static irqreturn_t k2hh_irq_thread(int irq, void *k2hh_data_p)
{
	struct k2hh_p *data = k2hh_data_p;

	data->irq_state = 1;
	wake_lock_timeout(&data->reactive_wake_lock,
		msecs_to_jiffies(2000));
	schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));
	SENSOR_INFO("###### reactive irq ######\n");

	return IRQ_HANDLED;
}

static int k2hh_setup_pin(struct k2hh_p *data)
{
	int ret;

	ret = gpio_request(data->acc_int1, "ACC_INT1");
	if (ret < 0) {
		SENSOR_ERR("gpio %d request failed (%d)\n",
			data->acc_int1, ret);
		goto exit;
	}

	ret = gpio_direction_input(data->acc_int1);
	if (ret < 0) {
		SENSOR_ERR("failed to set gpio %d as input (%d)\n",
			data->acc_int1, ret);
		goto exit_acc_int1;
	}

	wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND,
		"reactive_wake_lock");

	data->irq1 = gpio_to_irq(data->acc_int1);
	ret = request_threaded_irq(data->irq1, NULL, k2hh_irq_thread,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
		"k2hh_accel", data);
	if (ret < 0) {
		SENSOR_ERR("can't allocate irq.\n");
		goto exit_reactive_irq;
	}

	disable_irq(data->irq1);
	goto exit;

exit_reactive_irq:
	wake_lock_destroy(&data->reactive_wake_lock);
exit_acc_int1:
	gpio_free(data->acc_int1);
exit:
	return ret;
}

#if defined(CONFIG_SENSORS_IIO)
static int k2hh_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct k2hh_p *data = iio_priv(indio_dev);
	unsigned long flags;
	spin_lock_irqsave(&data->spin_lock, flags);
	iio_trigger_poll(data->trig, k2hh_iio_get_boottime_ns());
	spin_unlock_irqrestore(&data->spin_lock, flags);
	return 0;
}

static irqreturn_t k2hh_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct k2hh_p *data = iio_priv(indio_dev);
	char *tmp;
	int d_index = 0;
	s64 tmp_buf[2] = {0, };
	s64 timestamp = k2hh_iio_get_boottime_ns();

	/*copy valid data to buffer*/
	tmp = (unsigned char *)tmp_buf;
	memcpy(tmp, &data->accdata, sizeof(struct k2hh_v));
	d_index += sizeof(struct k2hh_v);

	if (((timestamp - data->old_timestamp) > (ktime_to_ms(data->poll_delay) * 1800000LL))
			&& (data->old_timestamp != 0)) {
		if (indio_dev->buffer->scan_timestamp)
			tmp_buf[1] = (timestamp +  data->old_timestamp) >> 1;

		iio_push_to_buffers(indio_dev, tmp);
	}

	if (indio_dev->buffer->scan_timestamp)
		tmp_buf[1] = timestamp;

	iio_push_to_buffers(indio_dev, tmp);
	data->old_timestamp = timestamp;
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int k2hh_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct k2hh_p *data = iio_priv(indio_dev);
	int pre_enable;

	pre_enable = atomic_read(&data->enable);
	if (pre_enable == OFF) {
		k2hh_regulator_onoff(&data->client->dev, true);
		/* Access to calibration data in filesystem
		 * only when there are no caldata */
		if (data->caldata.x == 0 && data->caldata.y == 0
				&& data->caldata.z == 0)
			k2hh_open_calibration(data);

		k2hh_set_range(data, K2HH_RANGE_4G);
		k2hh_set_bw(data);

		k2hh_set_mode(data, K2HH_MODE_NORMAL);
		atomic_set(&data->enable, ON);
		k2hh_set_enable(data, ON);
		data->old_timestamp = 0;
	}
	return 0;
}

static int k2hh_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct k2hh_p *data = iio_priv(indio_dev);
	int pre_enable;

	pre_enable = atomic_read(&data->enable);
	if (pre_enable == ON) {
		atomic_set(&data->enable, OFF);
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);
		k2hh_set_enable(data, OFF);

		k2hh_regulator_onoff(&data->client->dev, false);
	}

	return 0;
}

static int k2hh_data_rdy_trigger_set_state(struct iio_trigger *trig,
	bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	if (state)
		k2hh_pseudo_irq_enable(indio_dev);
	else
		k2hh_pseudo_irq_disable(indio_dev);
	return 0;
}

static const struct iio_trigger_ops k2hh_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &k2hh_data_rdy_trigger_set_state,
};

static int k2hh_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct k2hh_p *data = iio_priv(indio_dev);

	indio_dev->pollfunc = iio_alloc_pollfunc(&k2hh_iio_pollfunc_store_time,
		&k2hh_trigger_handler, IRQF_ONESHOT, indio_dev,
		"%s_consumer%d", indio_dev->name, indio_dev->id);
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	data->trig = iio_trigger_alloc("%s-dev%d", indio_dev->name,
		indio_dev->id);
	if (!data->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	data->trig->dev.parent = indio_dev->dev.parent;
	data->trig->ops = &k2hh_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);
	ret = iio_trigger_register(data->trig);
	if (ret)
		goto error_free_trig;
	indio_dev->trig = data->trig;
	return 0;

error_free_trig:
	iio_trigger_free(data->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void k2hh_remove_trigger(struct iio_dev *indio_dev)
{
	struct k2hh_p *data = iio_priv(indio_dev);
	iio_trigger_unregister(data->trig);
	iio_trigger_free(data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_buffer_setup_ops k2hh_buffer_setup_ops = {
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static void k2hh_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
};

static int k2hh_probe_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}
	buffer->scan_timestamp = true;
	buffer->bytes_per_datum = 16;
	indio_dev->buffer = buffer;
	indio_dev->setup_ops = &k2hh_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	ret = iio_buffer_register(indio_dev, indio_dev->channels,
		indio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(indio_dev, indio_dev->buffer, K2HH_SCAN_ACCEL_X);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, K2HH_SCAN_ACCEL_Y);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, K2HH_SCAN_ACCEL_Z);
	return 0;

error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int k2hh_iio_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan,
	int *val, int *val2, long mask)
{
	struct k2hh_p *data = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_ACCEL) {
		SENSOR_ERR("invalied type\n");
		return ret;
	}

	switch (chan->channel2) {
	case IIO_MOD_X:
		*val = data->accdata.x;
		break;
	case IIO_MOD_Y:
		*val = data->accdata.y;
		break;
	case IIO_MOD_Z:
		*val = data->accdata.z;
		break;
	default:
		SENSOR_ERR("invalied channel\n");
		return ret;
	}

	return IIO_VAL_INT;
}

static int k2hh_read_event_config(struct iio_dev *indio_dev,
	u64 event_code)
{
	struct k2hh_p *data = iio_priv(indio_dev);

	return atomic_read(&data->enable);
}

static const struct iio_info k2hh_info = {
	.driver_module = THIS_MODULE,
	.attrs = &k2hh_attribute_group,
	.read_raw = &k2hh_iio_read_raw,
	.read_event_config = &k2hh_read_event_config,
};

#else	/* !defined(CONFIG_SENSORS_IIO) */

static int k2hh_input_init(struct k2hh_p *data)
{
	int ret = 0;
	struct input_dev *dev;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
	input_set_capability(dev, EV_REL, REL_DIAL);
	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0)
		goto err_register_input_dev;

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0)
		goto err_create_sensor_symlink;

	/* sysfs node creation */
	ret = sysfs_create_group(&dev->dev.kobj, &k2hh_attribute_group);
	if (ret < 0)
		goto err_create_sysfs_group;

	data->input = dev;

	return 0;

err_create_sysfs_group:
	sensors_remove_symlink(&dev->dev.kobj, dev->name);
err_create_sensor_symlink:
	input_unregister_device(dev);
err_register_input_dev:
	input_free_device(dev);
	return ret;
}
#endif

static int k2hh_parse_dt(struct k2hh_p *data, struct device *dev)
{
	struct device_node *d_node = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	u32 temp;

	if (d_node == NULL)
		return -ENODEV;

#if defined(CONFIG_SENSORS_HAS_GPIO_LDO)
	data->sensor_en = of_get_named_gpio_flags(d_node, "stm,en_gpio", 0,
		&flags);
	if (data->sensor_en < 0) {
		SENSOR_ERR("Failed to get en_gpio\n");
		data->sensor_en = -1;
	}
#endif

	data->acc_int1 = of_get_named_gpio_flags(d_node, "stm,irq_gpio", 0,
		&flags);
	if (data->acc_int1 < 0) {
		SENSOR_ERR("get acc_int1 error\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(d_node, "stm,axis_map_x", &temp);
	if ((data->axis_map_x > 2) || (ret < 0)) {
		SENSOR_ERR("invalid x axis_map value %u\n",
			data->axis_map_x);
		data->axis_map_x = 0;
	} else
		data->axis_map_x = (u8)temp;

	ret = of_property_read_u32(d_node, "stm,axis_map_y", &temp);
	if ((data->axis_map_y > 2) || (ret < 0)) {
		SENSOR_ERR("invalid y axis_map value %u\n",
			data->axis_map_y);
		data->axis_map_y = 1;
	} else
		data->axis_map_y = (u8)temp;

	ret = of_property_read_u32(d_node, "stm,axis_map_z", &temp);
	if ((data->axis_map_z > 2) || (ret < 0)) {
		SENSOR_ERR("invalid z axis_map value %u\n",
			data->axis_map_z);
		data->axis_map_z = 2;
	} else
		data->axis_map_z = (u8)temp;

	ret = of_property_read_u32(d_node, "stm,negate_x", &temp);
	if ((data->negate_x > 1) || (ret < 0)) {
		SENSOR_ERR("invalid x axis_map value %u\n",
			data->negate_x);
		data->negate_x = 0;
	} else
		data->negate_x = (u8)temp;

	ret = of_property_read_u32(d_node, "stm,negate_y", &temp);
	if ((data->negate_y > 1) || (ret < 0)) {
		SENSOR_ERR("invalid y axis_map value %u\n",
			data->negate_y);
		data->negate_y = 0;
	} else
		data->negate_y = (u8)temp;

	ret = of_property_read_u32(d_node, "stm,negate_z", &temp);
	if ((data->negate_z > 1) || (ret < 0)) {
		SENSOR_ERR("invalid z axis_map value %u\n",
			data->negate_z);
		data->negate_z = 0;
	} else
		data->negate_z = (u8)temp;

	return 0;
}


static int k2hh_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	u8 temp;
	int ret = -ENODEV, i;
	struct k2hh_p *data;
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev;
#endif

	SENSOR_INFO("Probe Start!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c_check_functionality error\n");
		goto exit;
	}

#if defined(CONFIG_SENSORS_IIO)
	/* Configure IIO device */
	indio_dev = iio_device_alloc(sizeof(*data));
	if (!indio_dev) {
		SENSOR_ERR("iio_device_alloc failed\n");
		ret = -ENOMEM;
		goto exit_mem_alloc;
	}

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
#else
	data = kzalloc(sizeof(struct k2hh_p), GFP_KERNEL);
	if (data == NULL) {
		SENSOR_ERR("kzalloc error\n");
		ret = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
#endif

	data->client = client;

	ret = k2hh_parse_dt(data, &client->dev);
	if (ret < 0) {
		SENSOR_ERR("of_node error\n");
		ret = -ENODEV;
		goto exit_of_node;
	}

	ret = k2hh_regulator_onoff(&client->dev, ON);
	if (ret < 0) {
		SENSOR_ERR("No regulator\n");
		goto exit_no_regulator;
	}

#if defined(CONFIG_SENSORS_HAS_GPIO_LDO)
	ret = gpio_request(data->sensor_en, "sensor_en");
	if (ret < 0) {
		SENSOR_ERR("gpio %d request failed (%d). may have no gpio\n",
			data->sensor_en, ret);
	} else {
		ret = gpio_direction_output(data->sensor_en, ON);
		if (ret)
			SENSOR_ERR("unable to set_direction for [%d]\n",
				data->sensor_en);

		gpio_set_value_cansleep(data->sensor_en, ON);
		msleep(20);
	}
#endif

	ret = k2hh_setup_pin(data);
	if (ret < 0) {
		SENSOR_ERR("could not setup pin\n");
		goto exit_setup_pin;
	}

	mutex_init(&data->mode_mutex);

	/* read chip id */
	k2hh_set_mode(data, K2HH_MODE_NORMAL);
	for (i = 0; i < CHIP_ID_RETRIES; i++) {
		ret = k2hh_i2c_read(client, WHOAMI_REG, &temp, 1);
		if (temp != K2HH_CHIP_ID) {
			SENSOR_INFO("chip id failed 0x%x : %d\n",
				temp, ret);
		} else {
			SENSOR_INFO("chip id success 0x%x\n",
				temp);
			break;
		}
		msleep(20);
	}

	if (i >= CHIP_ID_RETRIES) {
		ret = -ENODEV;
		goto exit_read_chipid;
	}

#if defined(CONFIG_SENSORS_IIO)
	/* IIO device init */
	indio_dev->name = "accelerometer_sensor";
	indio_dev->channels = k2hh_channels;
	indio_dev->num_channels = ARRAY_SIZE(k2hh_channels);
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &k2hh_info;

	ret = k2hh_probe_buffer(indio_dev);
	if (ret) {
		SENSOR_ERR("probe ring buffer failed\n");
		goto exit_iio_buffer_failed;
	}

	ret = k2hh_probe_trigger(indio_dev);
	if (ret) {
		SENSOR_ERR("probe trigger failed\n");
		goto exit_iio_trigger_failed;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		SENSOR_ERR("iio_device_register failed(%d)\n", ret);
		goto exit_iio_register_failed;
	}
#else

	/* input device init */
	ret = k2hh_input_init(data);
	if (ret < 0)
		goto exit_input_init;
#endif

	ret = sensors_register(data->factory_device, data, sensor_attrs,
		MODULE_NAME);
	if (ret) {
		SENSOR_ERR("failed to sensors_register (%d)\n", ret);
		goto exit_sensor_register_failed;
	}

	/* accel_timer settings. we poll for light values using a timer. */
	hrtimer_init(&data->accel_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->poll_delay = ns_to_ktime(K2HH_DEFAULT_DELAY);
	data->accel_timer.function = k2hh_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	data->accel_wq = create_singlethread_workqueue("accel_wq");
	if (!data->accel_wq) {
		ret = -ENOMEM;
		SENSOR_ERR("could not create workqueue\n");
		goto exit_create_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&data->work_accel, k2hh_work_func);
	INIT_DELAYED_WORK(&data->irq_work, k2hh_irq_work_func);

	atomic_set(&data->enable, OFF);
	data->time_count = 0;
	data->irq_state = 0;
	data->recog_flag = OFF;
	data->hr = CTRL1_HR_ENABLE;

	k2hh_set_range(data, K2HH_RANGE_4G);
	k2hh_set_mode(data, K2HH_MODE_SUSPEND);

	/*power off the regulators, for next enable power will be on*/
	ret = k2hh_regulator_onoff(&client->dev, false);

	SENSOR_INFO("Probe done!\n");

	return 0;

exit_create_workqueue:
	sensors_unregister(data->factory_device, sensor_attrs);
exit_sensor_register_failed:
#if defined(CONFIG_SENSORS_IIO)
	iio_device_unregister(indio_dev);
exit_iio_register_failed:
	k2hh_remove_trigger(indio_dev);
exit_iio_trigger_failed:
	k2hh_remove_buffer(indio_dev);
exit_iio_buffer_failed:
	i2c_set_clientdata(client, NULL);
#else
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
	sysfs_remove_group(&data->input->dev.kobj, &k2hh_attribute_group);
	input_unregister_device(data->input);
exit_input_init:
#endif
	mutex_destroy(&data->mode_mutex);
	free_irq(data->irq1, data);
	cancel_delayed_work_sync(&data->irq_work);
	wake_lock_destroy(&data->reactive_wake_lock);
	gpio_free(data->acc_int1);
exit_setup_pin:
exit_of_node:
#if !defined(CONFIG_SENSORS_IIO)
	kfree(data);
#endif
#if defined(CONFIG_SENSORS_IIO)
exit_mem_alloc:
#endif
exit_read_chipid:
#if defined(CONFIG_SENSORS_HAS_GPIO_LDO)
	if (data->sensor_en) {
		gpio_set_value_cansleep(data->sensor_en, OFF);
		gpio_free(data->sensor_en);
	}
#endif
	k2hh_regulator_onoff(&client->dev, OFF);
exit_no_regulator:
exit:
	SENSOR_ERR("Probe fail!\n");
	return ret;
}

static int k2hh_remove(struct i2c_client *client)
{
#if defined(CONFIG_SENSORS_IIO)
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct k2hh_p *data = iio_priv(indio_dev);
#else
	struct k2hh_p *data = (struct k2hh_p *)i2c_get_clientdata(client);
#endif

	SENSOR_INFO("\n");

	if (atomic_read(&data->enable) == ON) {
		k2hh_set_enable(data, OFF);
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);
		atomic_set(&data->enable, OFF);
	}

	sensors_unregister(data->factory_device, sensor_attrs);
#if defined(CONFIG_SENSORS_IIO)
	if (indio_dev) {
		k2hh_pseudo_irq_disable(indio_dev);
		iio_device_unregister(indio_dev);
		k2hh_remove_trigger(indio_dev);
		k2hh_remove_buffer(indio_dev);
		iio_device_free(indio_dev);
	}
#else
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
	sysfs_remove_group(&data->input->dev.kobj, &k2hh_attribute_group);
	input_unregister_device(data->input);
#endif

	mutex_destroy(&data->mode_mutex);
	free_irq(data->irq1, data);
	cancel_delayed_work_sync(&data->irq_work);
	wake_lock_destroy(&data->reactive_wake_lock);
	gpio_free(data->acc_int1);

#if !defined(CONFIG_SENSORS_IIO)
	kfree(data);
#endif

	return 0;
}

static int k2hh_suspend(struct device *dev)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(i2c_get_clientdata(
						to_i2c_client(dev)));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif

	SENSOR_INFO("\n");

	if (atomic_read(&data->enable) == ON) {
		k2hh_set_mode(data, K2HH_MODE_SUSPEND);
		k2hh_set_enable(data, OFF);
	}

	return 0;
}

static int k2hh_resume(struct device *dev)
{
#if defined(CONFIG_SENSORS_IIO)
	struct k2hh_p *data = iio_priv(i2c_get_clientdata(
						to_i2c_client(dev)));
#else
	struct k2hh_p *data = dev_get_drvdata(dev);
#endif

	SENSOR_INFO("\n");

	if (atomic_read(&data->enable) == ON) {
		k2hh_set_mode(data, K2HH_MODE_NORMAL);
		k2hh_set_enable(data, ON);
	}

	return 0;
}

static struct of_device_id k2hh_match_table[] = {
	{ .compatible = "stm,k2hh",},
	{},
};

static const struct i2c_device_id k2hh_id[] = {
	{ "k2hh_match_table", 0 },
	{ }
};

static const struct dev_pm_ops k2hh_pm_ops = {
	.suspend = k2hh_suspend,
	.resume = k2hh_resume
};

static struct i2c_driver k2hh_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = k2hh_match_table,
		.pm = &k2hh_pm_ops
	},
	.probe		= k2hh_probe,
	.remove		= k2hh_remove,
	.id_table	= k2hh_id,
};

static int __init k2hh_init(void)
{
	return i2c_add_driver(&k2hh_driver);
}

static void __exit k2hh_exit(void)
{
	i2c_del_driver(&k2hh_driver);
}

module_init(k2hh_init);
module_exit(k2hh_exit);

MODULE_DESCRIPTION("k2hh accelerometer sensor driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

