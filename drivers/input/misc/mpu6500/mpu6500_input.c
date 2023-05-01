/*
	$License:
	Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	$
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/wakelock.h>
#include <linux/sensors_core.h>
#include <linux/hrtimer.h>
#include <linux/mpu6500_input.h>
#ifdef CONFIG_INPUT_MPU6500_LP
#include "mpu6500_lp.h"
#endif

#include "./mpu6500_selftest.h"

#define DEBUG 0
#define MPU6500_MODE_NORMAL	0
#define MPU6500_MODE_SLEEP	2
#define MPU6500_MODE_WAKE_UP	3

#define MAX_GYRO	32767
#define MIN_GYRO	-32768

#define LOG_RESULT_LOCATION(x) {\
	printk(KERN_ERR "%s:%s:%d result=%d\n",__FILE__,__func__,__LINE__, x);\
}\

#define CHECK_RESULT(x) {\
		result = x;\
		if (unlikely(result)) \
			LOG_RESULT_LOCATION(result);\
	}

#ifndef MIN
#define	MIN(a, b)		(((a) < (b))?(a):(b))
#endif

#ifndef MAX
#define	MAX(a, b)		(((a) > (b))?(a):(b))
#endif

#define MPU6500_ACCEL_LPF_GAIN(x)				(((x)*8)/10)

#define INT_SRC_ORIENT 0x02
#define INT_SRC_DISPLAY_ORIENT  0x08
#define DMP_MASK_DIS_ORIEN       0xC0
#define DMP_DIS_ORIEN_SHIFT      6

#define MPU6500_GYRO_SPC_CFG	0x49
#define MPU6500_REG_BANK_SEL	0x76
#define MPU6500_CFG_SET_BIT	0x20

#define MPU6500_CALIB_FILENAME	"//data//mpu6500.cal"

#define MPU6500_IRQ_ENABLE_STAT		1
#define MPU6500_IRQ_DISABLE_STAT	2

struct motion_int_data {
	unsigned char pwr_mnt[2];
	unsigned char cfg;
	unsigned char accel_cfg;
	unsigned char gyro_cfg;
	unsigned char int_cfg;
	unsigned char smplrt_div;
	bool is_set;
	unsigned char accel_cfg2;
};

struct mpu6500_input_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct motion_int_data mot_data;
	struct mutex mutex;
	struct wake_lock reactive_wake_lock;
	atomic_t accel_enable;
	atomic_t accel_delay;

	atomic_t gyro_enable;
	atomic_t gyro_delay;
	atomic_t reactive_state;
	atomic_t reactive_enable;

	atomic_t motion_recg_enable;
	unsigned long motion_recg_st_time; //start-up time of motion interrupt

	unsigned char gyro_pwr_mgnt[2];
	unsigned char int_pin_cfg;

#ifdef CONFIG_INPUT_MPU6500_LP
	atomic_t lp_scr_orient_enable;
#endif
	u16 enabled_sensors;
	u16 sleep_sensors;
	int current_delay;

	int gyro_bias[3];
	int irq_flag;

	u8 mode;

#ifdef CONFIG_INPUT_MPU6500_POLLING
	struct delayed_work accel_work;
	struct delayed_work gyro_work;
#endif
	struct device *accel_sensor_device;
	struct device *gyro_sensor_device;
	struct mpu6k_input_platform_data *pdata;
	s16 acc_cal[3];
	bool factory_mode;
};

static struct mpu6500_input_data *gb_mpu_data;

struct mpu6500_input_cfg {
	int dummy;

};

static int mpu6500_input_activate_devices(struct mpu6500_input_data *data,
					  int sensors, bool enable);


static void mpu6500_proc_msleep(unsigned int msecs,
	struct hrtimer_sleeper *sleeper, int sigs)
{
	enum hrtimer_mode mode = HRTIMER_MODE_REL;
	int state = sigs ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE;

	hrtimer_init(&sleeper->timer, CLOCK_MONOTONIC, mode);
	sleeper->timer._softexpires = ktime_set(0, msecs*NSEC_PER_MSEC);
	hrtimer_init_sleeper(sleeper, current);

	do {
		set_current_state(state);
		hrtimer_start(&sleeper->timer,
				sleeper->timer._softexpires, mode);
		if (sleeper->task)
			schedule();
		hrtimer_cancel(&sleeper->timer);
		mode = HRTIMER_MODE_ABS;
	} while (sleeper->task && !(sigs && signal_pending(current)));
}

void mpu6500_msleep(unsigned int msecs)
{
	struct hrtimer_sleeper sleeper;

	mpu6500_proc_msleep(msecs, &sleeper, 0);
}

void mpu6500_msleep_interruptible(unsigned int msecs)
{
	struct hrtimer_sleeper sleeper;

	mpu6500_proc_msleep(msecs, &sleeper, 1);
}

int mpu6500_i2c_write(struct i2c_client *i2c_client,
		      unsigned int len, unsigned char *data)
{
	struct i2c_msg msgs[1];
	int res;

	if (NULL == data || NULL == i2c_client)
		return -EINVAL;

	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = (unsigned char *)data;
	msgs[0].len = len;

	res = i2c_transfer(i2c_client->adapter, msgs, 1);
	if (res < 1)
		return res;
	else
		return 0;
}

int mpu6500_i2c_read(struct i2c_client *i2c_client,
		     unsigned int len, unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (NULL == data || NULL == i2c_client)
		return -EINVAL;

	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = I2C_M_RD;
	msgs[0].buf = data;
	msgs[0].len = len;

	res = i2c_transfer(i2c_client->adapter, msgs, 1);
	if (res < 1)
		return res;
	else
		return 0;
}

int mpu6500_i2c_write_single_reg(struct i2c_client *i2c_client,
				 unsigned char reg, unsigned char value)
{

	unsigned char data[2];

	data[0] = reg;
	data[1] = value;

	return mpu6500_i2c_write(i2c_client, 2, data);
}

int mpu6500_i2c_read_reg(struct i2c_client *i2c_client,
			 unsigned char reg, unsigned int len,
			 unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (NULL == data || NULL == i2c_client)
		return -EINVAL;

	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = i2c_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = (u16)len;

	res = i2c_transfer(i2c_client->adapter, msgs, 2);
	if (res < 1)
		return res;
	else
		return 0;
}

int mpu6500_i2c_read_fifo(struct i2c_client *i2c_client,
			  unsigned short length, unsigned char *data)
{
	int result;
	unsigned short bytes_read = 0;

	while (bytes_read < length) {
		unsigned short this_len = length - bytes_read;

		result =
		    mpu6500_i2c_read_reg(i2c_client, MPUREG_FIFO_R_W, this_len,
					 &data[bytes_read]);
		if (result) {
			return result;
		}
		bytes_read += this_len;
	}

	return 0;
}

int mpu6500_i2c_memory_write(struct i2c_client *i2c_client,
			unsigned short mem_addr, unsigned int len, unsigned char const *data)
{
	unsigned char bank[2];
	unsigned char addr[2];
	unsigned char buf[513];

	struct i2c_msg msgs[3];
	int res;

	if (!data || !i2c_client)
		return -EINVAL;

	if (len >= (sizeof(buf) - 1))
		return -ENOMEM;

	bank[0] = MPUREG_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = MPUREG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;

	buf[0] = MPUREG_MEM_R_W;
	memcpy(buf + 1, data, len);

	/* write message */
	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = i2c_client->addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = i2c_client->addr;
	msgs[2].flags = 0;
	msgs[2].buf = (unsigned char *)buf;
	msgs[2].len = len + 1;

	res = i2c_transfer(i2c_client->adapter, msgs, 3);
	if (res != 3) {
		if (res >= 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}

}

int mpu6500_i2c_memory_read(struct i2c_client *i2c_client,
		unsigned short mem_addr, unsigned int len, unsigned char *data)
{
	unsigned char bank[2];
	unsigned char addr[2];
	unsigned char buf;

	struct i2c_msg msgs[4];
	int res;

	if (!data || !i2c_client)
		return -EINVAL;

	bank[0] = MPUREG_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = MPUREG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;

	buf = MPUREG_MEM_R_W;

	/* write message */
	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = i2c_client->addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = i2c_client->addr;
	msgs[2].flags = 0;
	msgs[2].buf = &buf;
	msgs[2].len = 1;

	msgs[3].addr = i2c_client->addr;
	msgs[3].flags = I2C_M_RD;
	msgs[3].buf = data;
	msgs[3].len = len;

	res = i2c_transfer(i2c_client->adapter, msgs, 4);
	if (res != 4) {
		if (res >= 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}

}

static int mpu6500_input_set_mode(struct mpu6500_input_data *data, u8 mode)
{
	int err = 0;
	data->mode = mode;

	if (mode == MPU6500_MODE_SLEEP) {
		err = mpu6500_input_activate_devices(data,
				MPU6500_SENSOR_ACCEL |
					       MPU6500_SENSOR_GYRO
#ifdef CONFIG_INPUT_MPU6500_LP
					| MPU6500_SENSOR_LP_SCR_ORIENT
#endif
					, false);
	}
	if (mode == MPU6500_MODE_NORMAL) {
		if (atomic_read(&data->accel_enable))
			err = mpu6500_input_activate_devices(data,
				MPU6500_SENSOR_ACCEL
#ifdef CONFIG_INPUT_MPU6500_LP
					       | MPU6500_SENSOR_LPSO
#endif
						,true);
		if (atomic_read(&data->gyro_enable))
			err = mpu6500_input_activate_devices(data,
				MPU6500_SENSOR_GYRO
#ifdef CONFIG_INPUT_MPU6500_LP
					       | MPU6500_SENSOR_LPSO
#endif
						, true);
	}
	return err;
}

static void mpu6500_apply_orientation(const __s8 orientation[9],
				const s16 raw[3], s16 orient_raw[3])
{
	int i = 0;
	s32 value = 0;
	for (i = 0; i < 3; i++) {
		value = orientation[i * 3]*raw[0]
			+ orientation[i * 3 + 1]*raw[1]
			+ orientation[i * 3 + 2]*raw[2];

		/*trim the edge*/
		if (value > 32767)
			value = 32767;
		else if (value < -32768)
			value = -32768;
		orient_raw[i] = (s16)value;
	}
}

static void mpu6500_input_report_accel_xyz(struct mpu6500_input_data *data)
{
	u8 regs[6];
	s16 raw[3], orien_raw[3];
	int result;

	const struct mpu6k_input_platform_data *pdata =
	    data->client->dev.platform_data;

	result =
	    mpu6500_i2c_read_reg(data->client, MPUREG_ACCEL_XOUT_H, 6, regs);

	raw[0] = ((s16) ((s16) regs[0] << 8)) | regs[1];
	raw[1] = ((s16) ((s16) regs[2] << 8)) | regs[3];
	raw[2] = ((s16) ((s16) regs[4] << 8)) | regs[5];

	mpu6500_apply_orientation(pdata->orientation, raw, orien_raw);

	input_report_rel(data->input, REL_X, orien_raw[0] - data->acc_cal[0]);
	input_report_rel(data->input, REL_Y, orien_raw[1] - data->acc_cal[1]);
	input_report_rel(data->input, REL_Z, orien_raw[2] - data->acc_cal[2]);

	input_sync(data->input);
}

static void mpu6500_input_report_gyro_xyz(struct mpu6500_input_data *data)
{
	u8 regs[6];
	s16 raw[3], orien_raw[3], raw_tmp[3];
	int result;

	const struct mpu6k_input_platform_data *pdata =
	    data->client->dev.platform_data;

	result =
	    mpu6500_i2c_read_reg(data->client, MPUREG_GYRO_XOUT_H, 6, regs);

	if (result) {
		pr_err("%s: i2c_read err= %d\n", __func__, result);
		return;
	}

	raw_tmp[0] = (((s16) ((s16) regs[0] << 8)) | regs[1]);
	raw_tmp[1] = (((s16) ((s16) regs[2] << 8)) | regs[3]);
	raw_tmp[2] = (((s16) ((s16) regs[4] << 8)) | regs[5]);

	raw[0] = raw_tmp[0] - (s16) data->gyro_bias[0];
	raw[1] = raw_tmp[1] - (s16) data->gyro_bias[1];
	raw[2] = raw_tmp[2] - (s16) data->gyro_bias[2];

	if (!(raw[0] >> 15 == raw_tmp[0] >> 15) &&\
		!((s16) data->gyro_bias[0] >> 15 == raw_tmp[0] >> 15)) {
		pr_info("%s GYRO X is overflowed!!!\n", __func__);
		raw[0] = (raw[0] >= 0 ? MIN_GYRO : MAX_GYRO);

	}
	if (!(raw[1] >> 15 == raw_tmp[1] >> 15) &&\
		!((s16) data->gyro_bias[1] >> 15 == raw_tmp[1] >> 15)) {
		pr_info("%s GYRO Y is overflowed!!!\n", __func__);
		raw[1] = (raw[1] >= 0 ? MIN_GYRO : MAX_GYRO);

	}
	if (!(raw[2] >> 15 == raw_tmp[2] >> 15) &&\
		!((s16) data->gyro_bias[2] >> 15 == raw_tmp[2] >> 15)) {
		pr_info("%s GYRO Z is overflowed!!!\n", __func__);
		raw[2] = (raw[2] >= 0 ? MIN_GYRO : MAX_GYRO);
	}

	mpu6500_apply_orientation(pdata->orientation, raw, orien_raw);

	input_report_rel(data->input, REL_RX, orien_raw[0]);
	input_report_rel(data->input, REL_RY, orien_raw[1]);
	input_report_rel(data->input, REL_RZ, orien_raw[2]);

	input_sync(data->input);
}

#ifdef CONFIG_INPUT_MPU6500_LP

#define REG_DMP_INT_STATUS      0x39
#define INV_MPU_DMP_MOTION_INT   0x20
#define INV_MPU_DMP_NO_MOTION_INT   0x10


static void mpu6500_input_report_fifo_data(struct mpu6500_input_data *data)
{
	unsigned char val[3];
	unsigned char fifo_data[50] = { 0 };
	unsigned short fifo_count = 0;
	short accel[3] = { 0 }, gyro[3] = {
	0};
	int i = 0;
	unsigned int event = 0;
	short orien_raw[3];

	const struct mpu6k_input_platform_data *pdata =
	    data->client->dev.platform_data;

	mpu6500_i2c_read_reg(data->client, MPUREG_FIFO_COUNTH, 2, val);

	fifo_count = (unsigned short)(val[0] << 8) | val[1];
	if (fifo_count > 0) {
		mpu6500_i2c_read_fifo(data->client, fifo_count, fifo_data);

		for (i = 0; i < 3; i++) {
			accel[i] = be16_to_cpup((__be16 *) (&fifo_data[i * 2]));
			gyro[i] =
			    be16_to_cpup((__be16 *) (&fifo_data[i * 2 + 6]));
		}

		mpu6500_apply_orientation(pdata->orientation, accel, orien_raw);

		input_report_rel(data->input, REL_X, orien_raw[0]);
		input_report_rel(data->input, REL_Y, orien_raw[1]);
		input_report_rel(data->input, REL_Z, orien_raw[2]);

		mpu6500_apply_orientation(pdata->orientation, gyro, orien_raw);

		input_report_rel(data->input, REL_RX, orien_raw[0]);
		input_report_rel(data->input, REL_RY, orien_raw[1]);
		input_report_rel(data->input, REL_RZ, orien_raw[2]);

		input_sync(data->input);

		event =
		    (unsigned
		     int)(be32_to_cpup((unsigned int *)&fifo_data[12]));

		if (((event >> 16) & 0xff) & INT_SRC_DISPLAY_ORIENT) {
			u8 so_data = ((DMP_MASK_DIS_ORIEN & (event & 0xff)) >>
				      DMP_DIS_ORIEN_SHIFT);
			u8 so_event = 0;
			printk(KERN_INFO "DISPLAY_ORIENTATION : %d\n", so_data);

			if (so_data == 0) /* 0 degree */
				so_event = 0;
			else if (so_data == 1) /* 90degree */
				so_event = 1;
			else if (so_data == 2) /* 180degree */
				so_event = 3;
			else if (so_data == 3) /* 270degree */
				so_event = 2;

			input_report_rel(data->input, REL_MISC, so_event + 1);

			input_sync(data->input);
		}
#if 0
		else if (((event >> 16) & 0xff) & INT_SRC_ORIENT) {
			__s8 orient_event[2] = { 0 }
			, current_event[2] = {
			0};
			u8 so_data = ((event >> 8) & 0xff);
			u8 orient = 0;

			if (so_data & MPU6500_X_UP)
				current_event[0] = 1;
			else if (so_data & MPU6500_X_DOWN)
				current_event[0] = -1;
			else if (so_data & MPU6500_Y_UP)
				current_event[1] = 1;
			else if (so_data & MPU6500_Y_DOWN)
				current_event[1] = -1;

			for (i = 0; i < 2; i++) {
				orient_event[i] =
				    pdata->orientation[i * 3] * current_event[0]
				    + pdata->orientation[i * 3 +
							 1] * current_event[1]
				    + pdata->orientation[i * 3 +
							 2] * current_event[2];
			}

			if (orient_event[0] > 0)	/* 90 degree */
				orient = 2;
			else if (orient_event[0] < 0)	/* 180 degree */
				orient = 3;
			else if (orient_event[1] > 0)	/* 0 degree */
				orient = 1;
			else if (orient_event[1] < 0)	/* 270 degree */
				orient = 4;

			printk(KERN_INFO "DMP_ORIENTATION : (%d) %d\n", so_data,
			       orient);

			input_report_rel(data->input, REL_MISC, orient);

			input_sync(data->input);
		}
#endif
	}
}
#endif

static irqreturn_t mpu6500_input_irq_thread(int irq, void *dev)
{
	struct mpu6500_input_data *data = (struct mpu6500_input_data *)dev;
	struct motion_int_data *mot_data = &data->mot_data;
	unsigned char reg;
	unsigned long timediff = 0;
	int result;
	if (!atomic_read(&data->reactive_enable)) {
#ifdef CONFIG_INPUT_MPU6500_LP
		if (IS_LP_ENABLED(data->enabled_sensors))
			mpu6500_input_report_fifo_data(data);
		else {
#ifndef CONFIG_INPUT_MPU6500_POLLING
			if (data->enabled_sensors & MPU6500_SENSOR_ACCEL)
				mpu6500_input_report_accel_xyz(data);
			if (data->enabled_sensors & MPU6500_SENSOR_GYRO)
				mpu6500_input_report_gyro_xyz(data);
#endif
		}
#else
#ifndef CONFIG_INPUT_MPU6500_POLLING
		if (data->enabled_sensors & MPU6500_SENSOR_ACCEL)
			mpu6500_input_report_accel_xyz(data);

		if (data->enabled_sensors & MPU6500_SENSOR_GYRO)
			mpu6500_input_report_gyro_xyz(data);
#endif
#endif
	} else {
		result = mpu6500_i2c_read_reg(data->client,
			MPUREG_INT_STATUS, 1, &reg);
		if (result) {
			pr_err("%s: i2c_read err= %d\n", __func__, result);
			goto done;
		}

		timediff = jiffies_to_msecs(jiffies - data->motion_recg_st_time);
		/* ignore motion interrupt happened in 100ms to skip intial erronous interrupt */
		if (timediff < 1000 && !(data->factory_mode)) {
			pr_debug("%s: timediff = %ld msec\n",
				__func__, timediff);
			goto done;
		}
		if (reg & (1 << 6) || data->factory_mode) {
			/* handle motion recognition */
			atomic_set(&data->reactive_state, true);
			gb_mpu_data->factory_mode = false;
			pr_info("%s: motion interrupt happened\n", __func__);
			/* disable motion int */
                   mpu6500_i2c_write_single_reg(data->client, MPUREG_INT_ENABLE, mot_data->int_cfg);
			wake_lock_timeout(&data->reactive_wake_lock, msecs_to_jiffies(2000));
		}
	}
done:
	return IRQ_HANDLED;
}

static int mpu6500_input_set_fsr(struct mpu6500_input_data *data, int fsr)
{
	unsigned char fsr_mask;
	int result;
	unsigned char reg;

	if (fsr <= 2000) {
		fsr_mask = 0x00;
	} else if (fsr <= 4000) {
		fsr_mask = 0x08;
	} else if (fsr <= 8000) {
		fsr_mask = 0x10;
	} else {		/* fsr = [8001, oo) */
		fsr_mask = 0x18;
	}

	result =
	    mpu6500_i2c_read_reg(data->client, MPUREG_ACCEL_CONFIG, 1, &reg);
	if (result) {
		LOG_RESULT_LOCATION(result);
		return result;
	}
	result =
	    mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_CONFIG,
					 reg | fsr_mask);
	if (result) {
		LOG_RESULT_LOCATION(result);
		return result;
	}

	return result;
}

#ifdef CONFIG_MPU6500_LP_MODE
static int mpu6500_input_set_lp_mode(struct mpu6500_input_data *data,
				     unsigned char lpa_freq)
{
	unsigned char b = 0;
	/* Reducing the duration setting for lp mode */
	b = 0x1;
	mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_INT_ENABLE, b);
	/* Setting the cycle bit and LPA wake up freq */
	mpu6500_i2c_read_reg(data->client, MPUREG_PWR_MGMT_1, 1, &b);
	b |= BIT_CYCLE | BIT_PD_PTAT;
	mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_1, b);
	mpu6500_i2c_read_reg(data->client, MPUREG_PWR_MGMT_2, 1, &b);
	b |= lpa_freq & BITS_LPA_WAKE_CTRL;
	mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_2, b);

	return 0;
}
#endif
static int mpu6500_input_set_fp_mode(struct mpu6500_input_data *data)
{
	unsigned char b;

	/* Resetting the cycle bit and LPA wake up freq */
	mpu6500_i2c_read_reg(data->client, MPUREG_PWR_MGMT_1, 1, &b);
	b &= ~BIT_CYCLE & ~BIT_PD_PTAT;
	mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_1, b);
	mpu6500_i2c_read_reg(data->client, MPUREG_PWR_MGMT_2, 1, &b);
	b &= ~BITS_LPA_WAKE_CTRL;
	mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_2, b);
	/* Resetting the duration setting for fp mode */
	b = (unsigned char)10 / ACCEL_MOT_DUR_LSB;
	mpu6500_i2c_write_single_reg(data->client,
					MPUREG_ACCEL_INTEL_ENABLE, b);

	return 0;
}

static int mpu6500_input_set_odr(struct mpu6500_input_data *data, int odr)
{
	int result;
	unsigned char b;

	if (!data->enabled_sensors)
		return 0;

#ifdef CONFIG_INPUT_MPU6500_LP
	if (data->enabled_sensors & MPU6500_SENSOR_LP_SCR_ORIENT)
		odr = MIN(odr, 4000);
#endif
	b = (unsigned char)(odr / 1000);

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_SMPLRT_DIV, b));

	mpu6500_i2c_read_reg(data->client, MPUREG_PWR_MGMT_1, 1, &b);
	b &= BIT_CYCLE;
	if (b == BIT_CYCLE) {
	  printk(KERN_INFO " Accel LP - > FP mode. \n ");
	  mpu6500_input_set_fp_mode(data);
	}

#ifdef CONFIG_INPUT_MPU6500_LP
	if (data->enabled_sensors & MPU6500_SENSOR_LP_SCR_ORIENT)
		mpu6500_lp_set_delay(data->client, data->current_delay);
#endif
	return result;
}

static int mpu6500_input_set_motion_interrupt(struct mpu6500_input_data *data,
					      int enable, bool factory_test)
{
	struct motion_int_data *mot_data = &data->mot_data;
	unsigned char reg;

	atomic_set(&data->reactive_state, false);

	if (enable) {
		/* 1) initialize */
		mpu6500_i2c_read_reg(data->client, MPUREG_INT_STATUS, 1, &reg);
		printk(KERN_INFO "@@Initialize motion interrupt : INT_STATUS=%x\n", reg);


		/* Power up the chip and clear the cycle bit. Full power */
		reg = 0x01;
		mpu6500_i2c_write_single_reg(data->client,
					     MPUREG_PWR_MGMT_1, reg);
		mdelay(50);

		/* 2. mpu& accel config */
		if (factory_test)
			reg = 0x0; /*260Hz LPF */
		else
			reg = 0x1; /*44Hz LPF */
		mpu6500_i2c_write_single_reg(data->client, MPUREG_CONFIG, reg);

		reg = 0x0; /* Clear Accel Config. */
		mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_CONFIG, reg);

		reg = 0x08;
		mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_CONFIG2, reg);


		/* 3. set motion thr & dur */
		if (factory_test)
			reg = 0x41;	/* Make the motion & drdy enable */
		else
			reg = 0x40;	/* Make the motion interrupt enable */
		mpu6500_i2c_write_single_reg(data->client, MPUREG_INT_ENABLE, reg);

		reg = 4;	// 3.91 Hz (low power accel odr)
		mpu6500_i2c_write_single_reg(data->client, MPUREG_LP_ACCEL_ODR, reg);

		reg = 0xC0;	/* Motion Duration =1 ms */
		mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_INTEL_CTRL, reg);

		/* Motion Threshold =1mg, based on the data sheet. */
		if (factory_test)
			reg = 0x00;
		else
			reg = 0x0C; // 0x4B;
		mpu6500_i2c_write_single_reg(data->client, MPUREG_WOM_THR, reg);

		if (!factory_test) {
			/* 5. */
			/* Steps to setup the lp mode for PWM-2 register */
			reg = mot_data->pwr_mnt[1];
			reg |= (BITS_LPA_WAKE_20HZ); /* the freq of wakeup */
			reg |= 0x07; /* put gyro in standby. */
			reg &= ~(BIT_STBY_XA | BIT_STBY_YA | BIT_STBY_ZA);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_2, reg);

			reg = 0x1;
			reg |= 0x20; /* Set the cycle bit to be 1. LP MODE */
			reg &= ~0x08; /* Clear the temp disp bit. */
			mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_1, reg & ~BIT_SLEEP);
		}
		data->motion_recg_st_time = jiffies;
	} else {
		if (mot_data->is_set) {
			mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_1,
				mot_data->pwr_mnt[0]);
			msleep(50);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_PWR_MGMT_2,
				mot_data->pwr_mnt[1]);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_CONFIG,
				mot_data->cfg);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_CONFIG,
				mot_data->accel_cfg);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_CONFIG2,
				mot_data->accel_cfg2);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_GYRO_CONFIG,
				mot_data->gyro_cfg);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_INT_ENABLE,
				mot_data->int_cfg);
			reg = 0xff; /* Motion Duration =1 ms */
			mpu6500_i2c_write_single_reg(data->client, MPUREG_ACCEL_INTEL_ENABLE, reg);
			/* Motion Threshold =1mg, based on the data sheet. */
			reg = 0xff;
			mpu6500_i2c_write_single_reg(data->client, MPUREG_WOM_THR, reg);
			mpu6500_i2c_read_reg(data->client, MPUREG_INT_STATUS, 1, &reg);
			mpu6500_i2c_write_single_reg(data->client, MPUREG_SMPLRT_DIV, mot_data->smplrt_div);
			pr_info("%s: disable interrupt\n", __func__);
		}
	}
	mot_data->is_set = enable;

	return 0;
}

static int mpu6500_input_set_irq(struct mpu6500_input_data *data, unsigned char irq)
{
	int result;

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_INT_ENABLE, irq));

	return result;
}

static int mpu6500_input_suspend_accel(struct mpu6500_input_data *data)
{
	unsigned char reg;
	int result;

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_PWR_MGMT_2, 1, &reg));

	reg |= (BIT_STBY_XA | BIT_STBY_YA | BIT_STBY_ZA);
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_2, reg));

	return result;
}

static int mpu6500_input_resume_accel(struct mpu6500_input_data *data)
{
	int result = 0;
	unsigned char reg;

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_PWR_MGMT_1, 1, &reg));

	if (reg & BIT_SLEEP) {
		CHECK_RESULT(mpu6500_i2c_write_single_reg(data->client,
							  MPUREG_PWR_MGMT_1,
							  reg & ~BIT_SLEEP));
	}

	msleep(2);

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_PWR_MGMT_2, 1, &reg));

	reg &= ~(BIT_STBY_XA | BIT_STBY_YA | BIT_STBY_ZA);
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_2, reg));

	/* settings */

	/*----- LPF configuration  : 41hz ---->*/
	reg = MPU_FILTER_41HZ;
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_ACCEL_CONFIG2, reg));
	/*<----- LPF configuration  : 44hz ---- */

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_ACCEL_CONFIG, 1, &reg));
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_ACCEL_CONFIG, reg | 0x0));
	CHECK_RESULT(mpu6500_input_set_fsr(data, 2000));

	return result;

}

static int mpu6500_input_activate_accel(struct mpu6500_input_data *data,
				       bool enable)
{
	int result;

	if (enable) {
		result = mpu6500_input_resume_accel(data);
		if (result) {
			LOG_RESULT_LOCATION(result);
			return result;
		} else {
			data->enabled_sensors |= MPU6500_SENSOR_ACCEL;
		}
	} else {
		result = mpu6500_input_suspend_accel(data);
		if (result == 0) {
			data->enabled_sensors &= ~MPU6500_SENSOR_ACCEL;
		}
	}

	return result;
}

static int mpu6500_input_suspend_gyro(struct mpu6500_input_data *data)
{
	int result = 0;

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_PWR_MGMT_1, 2, data->gyro_pwr_mgnt));

	data->gyro_pwr_mgnt[1] |= (BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG);

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_2, data->gyro_pwr_mgnt[1]));


	return result;
}

static int mpu6500_input_resume_gyro(struct mpu6500_input_data *data)
{
	int result = 0;
	unsigned regs[2] = { 0, };

	CHECK_RESULT(mpu6500_i2c_read_reg
		     (data->client, MPUREG_PWR_MGMT_1, 2, data->gyro_pwr_mgnt));

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_1,
		      data->gyro_pwr_mgnt[0] & ~BIT_SLEEP));

	data->gyro_pwr_mgnt[1] &= ~(BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG);

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_2, data->gyro_pwr_mgnt[1]));

	regs[0] = MPU_FS_500DPS << 3;
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_GYRO_CONFIG, regs[0]));

	regs[0] = MPU_FILTER_41HZ | 0x18;
	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_CONFIG, regs[0]));

	return result;
}

static int mpu6500_input_activate_gyro(struct mpu6500_input_data *data,
				       bool enable)
{
	int result;
	if (enable) {
		result = mpu6500_input_resume_gyro(data);
		if (result) {
			LOG_RESULT_LOCATION(result);
			return result;
		} else {
			data->enabled_sensors |= MPU6500_SENSOR_GYRO;
		}
	} else {
		result = mpu6500_input_suspend_gyro(data);
		if (result == 0) {
			data->enabled_sensors &= ~MPU6500_SENSOR_GYRO;
		}
	}

	return result;
}

static int mpu6500_set_delay(struct mpu6500_input_data *data)
{
	int result = 0;
	int delay = 200000;
	unsigned char irq = 0;

	if (data->enabled_sensors & MPU6500_SENSOR_ACCEL) {
		delay = MIN(delay, atomic_read(&data->accel_delay));
		irq = BIT_RAW_RDY_EN;
	}

	if (data->enabled_sensors & MPU6500_SENSOR_GYRO) {
		delay = MIN(delay, atomic_read(&data->gyro_delay));
		irq |= BIT_RAW_RDY_EN;
	}

#ifdef CONFIG_INPUT_MPU6500_LP
	if (data->enabled_sensors & MPU6500_SENSOR_LP_SCR_ORIENT)
		irq = BIT_DMP_INT_EN;
#endif

	data->current_delay = delay;

	if (data->enabled_sensors & MPU6500_SENSOR_ACCEL ||
		data->enabled_sensors & MPU6500_SENSOR_GYRO) {
		CHECK_RESULT(mpu6500_input_set_odr(data,
			data->current_delay));
#ifndef CONFIG_INPUT_MPU6500_POLLING
		if (!atomic_read(&data->reactive_enable))
			CHECK_RESULT(mpu6500_input_set_irq(data,
				BIT_RAW_RDY_EN));
#endif
	}
	return result;
}

#ifdef CONFIG_INPUT_MPU6500_LP
static int
mpu6500_input_activate_lp_scr_orient(struct mpu6500_input_data *data,
				     bool enable)
{
	int result = 0;
	if (enable) {
		/* enable screen orientation */

		if (!(data->enabled_sensors & MPU6500_SENSOR_ACCEL))
			result = mpu6500_input_resume_accel(data);

		if (!result) {

			if (IS_LP_ENABLED(sensor)
			    || IS_LP_ENABLED(data->enabled_sensors))
				result =
				    mpu6500_lp_activate_lpfunc(data->client,
							       enable,
							       data->current_delay);
		}

		if (result) {
			LOG_RESULT_LOCATION(result);
			return result;
		} else
			data->enabled_sensors |= MPU6500_SENSOR_LP_SCR_ORIENT;
	} else {
		/* disable screen orientation */

		if (!(data->enabled_sensors & MPU6500_SENSOR_ACCEL))
			result = mpu6500_input_suspend_accel(data);

		if (!result)
			result =
			    mpu6500_lp_activate_screen_orientation(data->client,
						enable,
						data->current_delay);

		if (result == 0)
			data->enabled_sensors &= ~MPU6500_SENSOR_LP_SCR_ORIENT;
	}

	return result;
}
#endif



static int mpu6500_input_activate_devices(struct mpu6500_input_data *data,
					  int sensors, bool enable)
{
	int result = 0;

	/* disable reactive alert when any sensors turn on */
	if (atomic_read(&data->motion_recg_enable)) {
		if (enable && (sensors != 0))
			mpu6500_input_set_motion_interrupt(data, false, false);
	}

	if (sensors & MPU6500_SENSOR_ACCEL) {
		CHECK_RESULT(mpu6500_input_activate_accel(data, enable));
	}
	if (sensors & MPU6500_SENSOR_GYRO) {
		CHECK_RESULT(mpu6500_input_activate_gyro(data, enable));
	}


#ifdef CONFIG_INPUT_MPU6500_LP
	if ((sensors & MPU6500_SENSOR_LP_SCR_ORIENT) ||
	    (data->enabled_sensors & MPU6500_SENSOR_LP_SCR_ORIENT)) {
		bool en = true;
		if (sensors & MPU6500_SENSOR_LP_SCR_ORIENT)
			en = enable;
		CHECK_RESULT(mpu6500_input_activate_lp_scr_orient(data, en));
	}

	if (data->enabled_sensors &
	    (MPU6500_SENSOR_ACCEL | MPU6500_SENSOR_GYRO))
		mpu6500_lp_set_interrupt_on_gesture_event(data->client, false);
	else if (data->enabled_sensors & MPU6500_SENSOR_LP_SCR_ORIENT)
			mpu6500_lp_set_interrupt_on_gesture_event(data->client,
								  true);
#endif

	if (data->enabled_sensors) {
		CHECK_RESULT(mpu6500_set_delay(data));
	} else {
		unsigned char reg;
		/*enable reactive alert when all sensors go off*/
		if (atomic_read(&data->motion_recg_enable))
			mpu6500_input_set_motion_interrupt(data, true,
				gb_mpu_data->factory_mode);
		else {
			CHECK_RESULT(mpu6500_input_set_irq(data, 0x0));

			CHECK_RESULT(mpu6500_i2c_read_reg(data->client,
				MPUREG_PWR_MGMT_1, 1, &reg));

			if (!(reg & BIT_SLEEP))
				CHECK_RESULT(mpu6500_i2c_write_single_reg(
					data->client, MPUREG_PWR_MGMT_1,
					 reg | BIT_SLEEP));
		}
	}

	return result;
}
static void mpu6500_set_65XX_gyro_config(struct i2c_client *i2c_client)
{

	unsigned char d, cfg;

	mpu6500_i2c_read_reg(i2c_client, MPU6500_REG_BANK_SEL, 1, &cfg);

	mpu6500_i2c_write_single_reg(i2c_client, MPU6500_REG_BANK_SEL,
			cfg | MPU6500_CFG_SET_BIT);

	mpu6500_i2c_read_reg(i2c_client, MPU6500_GYRO_SPC_CFG, 1, &d);

	d |= 1;
	mpu6500_i2c_write_single_reg(i2c_client, MPU6500_GYRO_SPC_CFG, d);

	mpu6500_i2c_write_single_reg(i2c_client, MPU6500_REG_BANK_SEL, cfg);

}

static int __devinit mpu6500_input_initialize(struct mpu6500_input_data *data, const struct mpu6500_input_cfg
					      *cfg)
{
	int result;

	data->int_pin_cfg = BIT_INT_ANYRD_2CLEAR;

	data->current_delay = -1;
	data->enabled_sensors = 0;

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_PWR_MGMT_1, BIT_H_RESET));
	msleep(100);

	CHECK_RESULT(mpu6500_i2c_write_single_reg
		     (data->client, MPUREG_INT_PIN_CFG,
		      data->int_pin_cfg | BIT_BYPASS_EN));

#ifdef CONFIG_INPUT_MPU6500_LP
	CHECK_RESULT(mpu6500_lp_init_dmp(data->client, pdata->orientation));
#endif //CONFIG_INPUT_MPU6500_LP

	mpu6500_set_65XX_gyro_config(data->client);

	return mpu6500_input_set_mode(data, MPU6500_MODE_SLEEP);
}

static int accel_open_calibration(void)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs = {0};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(gb_mpu_data->pdata->acc_cal_path,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&gb_mpu_data->acc_cal,
			3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d,%d,%d)\n", __func__,
		gb_mpu_data->acc_cal[0], gb_mpu_data->acc_cal[1],
			gb_mpu_data->acc_cal[2]);

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

static ssize_t mpu6500_input_accel_enable_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->accel_enable));

}

static ssize_t mpu6500_input_accel_enable_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);
#ifdef CONFIG_INPUT_MPU6500_POLLING
	struct motion_int_data *mot_data = &data->mot_data;
#endif
	int value = simple_strtoul(buf, NULL, 10);

	if (value == 1)
		accel_open_calibration();

	pr_info("%s : enable = %d\n", __func__, value);

	mutex_lock(&data->mutex);

#ifdef CONFIG_INPUT_MPU6500_POLLING
	if (value && !atomic_read(&data->accel_enable)) {
		if (mot_data->is_set)
			mpu6500_input_set_motion_interrupt(
				data, false, data->factory_mode);

		mpu6500_input_activate_devices(data,
			MPU6500_SENSOR_ACCEL, true);

		schedule_delayed_work(&data->accel_work, msecs_to_jiffies(5));
	}
	if (!value && atomic_read(&data->accel_enable)) {
		cancel_delayed_work_sync(&data->accel_work);
		mpu6500_input_activate_devices(data,
			MPU6500_SENSOR_ACCEL, false);
		if (atomic_read(&data->reactive_enable))
			mpu6500_input_set_motion_interrupt(
				data, true, data->factory_mode);
	}
#else
	if (value && !atomic_read(&data->accel_enable)) {

		mpu6500_input_activate_devices(data, MPU6500_SENSOR_ACCEL, true);
	}
	if (!value && atomic_read(&data->accel_enable)) {
		mpu6500_input_activate_devices(data, MPU6500_SENSOR_ACCEL, false);
	}
#endif
	atomic_set(&data->accel_enable, value);
	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t mpu6500_input_accel_delay_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->accel_delay));

}

static ssize_t mpu6500_input_accel_delay_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	unsigned long value = (simple_strtoul(buf, NULL, 10))/1000;

	pr_info("%s : delay = %ld\n", __func__, value);

	mutex_lock(&data->mutex);

	atomic_set(&data->accel_delay, value);

	mpu6500_set_delay(data);

	mutex_unlock(&data->mutex);

	return count;
}

static int gyro_open_calibration(void)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs = {0};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(gb_mpu_data->pdata->gyro_cal_path,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&gb_mpu_data->gyro_bias, 3 * sizeof(int),
			&cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d,%d,%d)\n", __func__,
		gb_mpu_data->gyro_bias[0],
		gb_mpu_data->gyro_bias[1],
		gb_mpu_data->gyro_bias[2]);

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

static int gyro_do_calibrate(void)
{
	struct file *cal_filp;
	int err;
	mm_segment_t old_fs = {0};

	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
		gb_mpu_data->gyro_bias[0],
		gb_mpu_data->gyro_bias[1],
		gb_mpu_data->gyro_bias[2]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(gb_mpu_data->pdata->gyro_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&gb_mpu_data->gyro_bias, 3 * sizeof(int),
			&cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}


static ssize_t mpu6500_input_gyro_enable_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->gyro_enable));

}

static ssize_t mpu6500_input_gyro_enable_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);
#ifdef CONFIG_INPUT_MPU6500_POLLING
	struct motion_int_data *mot_data = &data->mot_data;
#endif
	int value = simple_strtoul(buf, NULL, 10);

	if (value == 1)
		gyro_open_calibration();

	mutex_lock(&data->mutex);

#ifdef CONFIG_INPUT_MPU6500_POLLING
	if (value && !atomic_read(&data->gyro_enable)) {
		if (mot_data->is_set)
			mpu6500_input_set_motion_interrupt(
				data, false, data->factory_mode);
		mpu6500_input_activate_devices(data,
			MPU6500_SENSOR_GYRO, true);
		schedule_delayed_work(&data->gyro_work,
			msecs_to_jiffies(5));
	}
	if (!value && atomic_read(&data->gyro_enable)) {
		cancel_delayed_work_sync(&data->gyro_work);
		mpu6500_input_activate_devices(data,
			MPU6500_SENSOR_GYRO, false);
		if (atomic_read(&data->reactive_enable))
			mpu6500_input_set_motion_interrupt(
				data, true, data->factory_mode);
	}
#endif

	atomic_set(&data->gyro_enable, value);
	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t mpu6500_input_gyro_delay_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->gyro_delay));

}

static ssize_t mpu6500_input_gyro_delay_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	unsigned long value = (simple_strtoul(buf, NULL, 10))/1000;

	pr_info("%s : delay = %ld\n", __func__, value);

	mutex_lock(&data->mutex);

	atomic_set(&data->gyro_delay, value);

	mpu6500_set_delay(data);

	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t mpu6500_input_motion_recg_enable_show(struct device *dev,
						     struct device_attribute
						     *attr, char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->motion_recg_enable));

}

static ssize_t mpu6500_input_motion_recg_enable_store(struct device *dev,
						      struct device_attribute
						      *attr, const char *buf,
						      size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	int value = simple_strtoul(buf, NULL, 10);

	mutex_lock(&data->mutex);

	atomic_set(&data->motion_recg_enable, value);

	mpu6500_input_set_motion_interrupt(data, value, false);

	mutex_unlock(&data->mutex);

	return count;
}

static int gyro_do_calibrate(void);

static ssize_t mpu6500_input_gyro_self_test_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	int scaled_gyro_bias[3] = { 0 };	//absolute gyro bias scaled by 1000 times. (gyro_bias x 1000)
	int scaled_gyro_rms[3] = { 0 };	//absolute gyro rms scaled by 1000 times. (gyro_bias x 1000)
	int packet_count[3] = { 0 };
	int result;

	mutex_lock(&data->mutex);

	result = mpu6500_selftest_run(data->client,
				      packet_count,
				      scaled_gyro_bias,
				      scaled_gyro_rms, data->gyro_bias);
	if (!result) {
		//store calibration to file
		gyro_do_calibrate();
	} else {
		data->gyro_bias[0] = 0;
		data->gyro_bias[1] = 0;
		data->gyro_bias[2] = 0;
	}

	mutex_unlock(&data->mutex);

	return sprintf(buf, "%d "
		       "%d %d %d "
		       "%d.%03d %d.%03d %d.%03d "
		       "%d.%03d %d.%03d %d.%03d ",
		       result,
		       packet_count[0], packet_count[1], packet_count[2],
		       (int)abs(scaled_gyro_bias[0] / 1000),
		       (int)abs(scaled_gyro_bias[0]) % 1000,
		       (int)abs(scaled_gyro_bias[1] / 1000),
		       (int)abs(scaled_gyro_bias[1]) % 1000,
		       (int)abs(scaled_gyro_bias[2] / 1000),
		       (int)abs(scaled_gyro_bias[2]) % 1000,
		       scaled_gyro_rms[0] / 1000,
		       (int)abs(scaled_gyro_rms[0]) % 1000,
		       scaled_gyro_rms[1] / 1000,
		       (int)abs(scaled_gyro_rms[1]) % 1000,
		       scaled_gyro_rms[2] / 1000,
		       (int)abs(scaled_gyro_rms[2]) % 1000);
}

#ifdef CONFIG_INPUT_MPU6500_LP
static ssize_t
mpu6500_input_lp_scr_orient_enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	long value;

	strict_strtol(buf,  10, &value);

	mutex_lock(&data->mutex);

	mpu6500_input_activate_devices(data, MPU6500_SENSOR_LP_SCR_ORIENT,
				       (value == 1) ? true : false);

	atomic_set(&data->lp_scr_orient_enable, (int)value);

	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t
mpu6500_input_lp_scr_orent_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct mpu6500_input_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", atomic_read(&data->lp_scr_orient_enable));
}

static DEVICE_ATTR(lpso_enable, 0664,
		   mpu6500_input_lp_scr_orent_show,
		   mpu6500_input_lp_scr_orient_enable_store);
#endif



static DEVICE_ATTR(acc_enable, 0664,
		   mpu6500_input_accel_enable_show,
		   mpu6500_input_accel_enable_store);
static DEVICE_ATTR(acc_delay, 0664,
		   mpu6500_input_accel_delay_show,
		   mpu6500_input_accel_delay_store);
static DEVICE_ATTR(gyro_enable, 0664,
		   mpu6500_input_gyro_enable_show,
		   mpu6500_input_gyro_enable_store);
static DEVICE_ATTR(gyro_delay, 0664,
		   mpu6500_input_gyro_delay_show,
		   mpu6500_input_gyro_delay_store);
static DEVICE_ATTR(self_test, 0444, mpu6500_input_gyro_self_test_show, NULL);
static DEVICE_ATTR(mot_recg_enable, 0664,
		   mpu6500_input_motion_recg_enable_show,
		   mpu6500_input_motion_recg_enable_store);



static struct attribute *mpu6500_attributes[] = {
	&dev_attr_acc_enable.attr,
	&dev_attr_acc_delay.attr,
	&dev_attr_gyro_enable.attr,
	&dev_attr_gyro_delay.attr,
	&dev_attr_self_test.attr,
	&dev_attr_mot_recg_enable.attr,
#ifdef CONFIG_INPUT_MPU6500_LP
	&dev_attr_lpso_enable.attr,
#endif
	NULL
};

static struct attribute_group mpu6500_attribute_group = {
	.attrs = mpu6500_attributes
};

/* samsung factory test */

static int read_accel_raw_xyz(s16 *x, s16 *y, s16 *z)
{
	u8 regs[6];
	s16 raw[3], orien_raw[3];
	int i = 2;
	int result;

	result =
		mpu6500_i2c_read_reg(gb_mpu_data->client,
			MPUREG_ACCEL_XOUT_H, 6, regs);

	raw[0] = ((s16) ((s16) regs[0] << 8)) | regs[1];
	raw[1] = ((s16) ((s16) regs[2] << 8)) | regs[3];
	raw[2] = ((s16) ((s16) regs[4] << 8)) | regs[5];

	do {
		orien_raw[i] = gb_mpu_data->pdata->orientation[i*3]
				* raw[0]
			+ gb_mpu_data->pdata->orientation[i*3+1]
				* raw[1]
			+ gb_mpu_data->pdata->orientation[i*3+2]
				* raw[2];
	} while (i-- != 0);

/*	pr_info("%s : x = %d, y = %d, z = %d\n", __func__,
		orien_raw[0], orien_raw[1], orien_raw[2]); */

	*x = orien_raw[0];
	*y = orien_raw[1];
	*z = orien_raw[2];

	return 0;
}

static int read_accel_raw_xyz_cal(s16 *x, s16 *y, s16 *z)
{
	s16 raw_x;
	s16 raw_y;
	s16 raw_z;
	if (!(gb_mpu_data->enabled_sensors & MPU6500_SENSOR_ACCEL)) {
		mpu6500_input_resume_accel(gb_mpu_data);
		usleep_range(10000, 11000);
	}

	read_accel_raw_xyz(&raw_x, &raw_y, &raw_z);
	*x = raw_x - gb_mpu_data->acc_cal[0];
	*y = raw_y - gb_mpu_data->acc_cal[1];
	*z = raw_z - gb_mpu_data->acc_cal[2];

	if (!(gb_mpu_data->enabled_sensors & MPU6500_SENSOR_ACCEL)) {
		mpu6500_input_suspend_accel(gb_mpu_data);
		usleep_range(10000, 11000);
	}
	return 0;
}

static int accel_do_calibrate(int enable)
{
	struct file *cal_filp;
	int sum[3] = { 0, };
	int err;
	int i;
	s16 x;
	s16 y;
	s16 z;
	mm_segment_t old_fs = {0};

	if (!(gb_mpu_data->enabled_sensors & MPU6500_SENSOR_ACCEL)) {
		mpu6500_input_resume_accel(gb_mpu_data);
		usleep_range(10000, 11000);
	}

	for (i = 0; i < ACC_CAL_TIME; i++) {
		err = read_accel_raw_xyz(&x, &y, &z);
		if (err < 0) {
			pr_err("%s: accel_read_accel_raw_xyz() "
				"failed in the %dth loop\n", __func__, i);
			goto done;
		}
		usleep_range(10000, 11000);
		sum[0] += x/ACC_CAL_DIV;
		sum[1] += y/ACC_CAL_DIV;
		sum[2] += z/ACC_CAL_DIV;
	}

	if (!(gb_mpu_data->enabled_sensors & MPU6500_SENSOR_ACCEL))
		mpu6500_input_suspend_accel(gb_mpu_data);

	if (enable) {
		gb_mpu_data->acc_cal[0] =
			(sum[0] / ACC_CAL_TIME) * ACC_CAL_DIV;
		gb_mpu_data->acc_cal[1] =
			(sum[1] / ACC_CAL_TIME) * ACC_CAL_DIV;
		if(sum[2] > 0)
			gb_mpu_data->acc_cal[2] =
			((sum[2] / ACC_CAL_TIME) - ACC_IDEAL) * ACC_CAL_DIV;
		else if(sum[2] < 0)
			gb_mpu_data->acc_cal[2] =
			((sum[2] / ACC_CAL_TIME) + ACC_IDEAL) * ACC_CAL_DIV;

	} else {
		gb_mpu_data->acc_cal[0] = 0;
		gb_mpu_data->acc_cal[1] = 0;
		gb_mpu_data->acc_cal[2] = 0;
	}

	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
		gb_mpu_data->acc_cal[0], gb_mpu_data->acc_cal[1],
		gb_mpu_data->acc_cal[2]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(gb_mpu_data->pdata->acc_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&gb_mpu_data->acc_cal, 3 * sizeof(s16),
			&cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}




static ssize_t mpu6500_input_reactive_enable_show(struct device *dev,
					struct device_attribute
						*attr, char *buf)
{
	pr_info("%s: state =%d\n", __func__,
		atomic_read(&gb_mpu_data->reactive_state));
	return sprintf(buf, "%d\n",
		atomic_read(&gb_mpu_data->reactive_state));
}

static ssize_t mpu6500_input_reactive_enable_store(struct device *dev,
					struct device_attribute
						*attr, const char *buf,
							size_t count)
{
	bool onoff = false;
	bool factory_test = false;
	unsigned long value = 0;
	struct motion_int_data *mot_data = &gb_mpu_data->mot_data;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	if (value == 1) {
		onoff = true;
	} else if (value == 0) {
		onoff = false;
	} else if (value == 2) {
		onoff = true;
		factory_test = true;
		gb_mpu_data->factory_mode = true;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

#ifdef CONFIG_INPUT_MPU6500_POLLING
	if (!value) {
		if(gb_mpu_data->irq_flag == MPU6500_IRQ_ENABLE_STAT) {
			disable_irq_wake(gb_mpu_data->client->irq);
			disable_irq(gb_mpu_data->client->irq);

			pr_info("[SENSOR - %s] disable irq\n", __func__);

			gb_mpu_data->irq_flag = MPU6500_IRQ_DISABLE_STAT;
		} else
			pr_err("[SENSOR - %s] irq disable reject\n", __func__);
	} else {
		if(gb_mpu_data->irq_flag == MPU6500_IRQ_DISABLE_STAT) {
			enable_irq(gb_mpu_data->client->irq);
			enable_irq_wake(gb_mpu_data->client->irq);

			pr_info("[SENSOR - %s] enable irq\n", __func__);

			gb_mpu_data->irq_flag =	MPU6500_IRQ_ENABLE_STAT;
		} else
			pr_err("[SENSOR - %s] irq enable reject\n", __func__);
	}
#endif

	mutex_lock(&gb_mpu_data->mutex);
	if (onoff) {
		pr_info("enable from %s\n", __func__);
		atomic_set(&gb_mpu_data->reactive_enable, true);
		if (!mot_data->is_set) {
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_PWR_MGMT_1, 2,
					mot_data->pwr_mnt);
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_CONFIG, 1, &mot_data->cfg);
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_ACCEL_CONFIG, 1,
					&mot_data->accel_cfg);
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_GYRO_CONFIG, 1,
					&mot_data->gyro_cfg);
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_INT_ENABLE, 1,
					&mot_data->int_cfg);
			mpu6500_i2c_read_reg(gb_mpu_data->client,
				MPUREG_SMPLRT_DIV, 1,
					&mot_data->smplrt_div);
		}
		mpu6500_input_set_motion_interrupt(gb_mpu_data,
			true, gb_mpu_data->factory_mode);
	} else {
		pr_info("%s disable\n", __func__);
		mpu6500_input_set_motion_interrupt(gb_mpu_data,
			false, gb_mpu_data->factory_mode);
		atomic_set(&gb_mpu_data->reactive_enable, false);
		if (gb_mpu_data->factory_mode)
			gb_mpu_data->factory_mode = false;
	}	mutex_unlock(&gb_mpu_data->mutex);

	pr_info("%s: onoff = %d, state =%d\n", __func__,
		atomic_read(&gb_mpu_data->reactive_enable),
		atomic_read(&gb_mpu_data->reactive_state));
	return count;
}

static struct device_attribute dev_attr_reactive_alert =
	__ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
		mpu6500_input_reactive_enable_show,
			mpu6500_input_reactive_enable_store);

static ssize_t mpu6500_power_on(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;

	dev_dbg(dev, "this_client = %d\n", (int)gb_mpu_data->client);
	count = sprintf(buf, "%d\n",
		(gb_mpu_data->client != NULL ? 1 : 0));

	return count;
}
static struct device_attribute dev_attr_power_on =
	__ATTR(power_on, S_IRUSR | S_IRGRP, mpu6500_power_on,
		NULL);

static ssize_t mpu6500_get_temp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	short temperature = 0;
	unsigned char reg[2];
	int result;

	CHECK_RESULT(mpu6500_i2c_read_reg
		(gb_mpu_data->client, MPUREG_TEMP_OUT_H, 2, reg));

	temperature = (short) (((reg[0]) << 8) | reg[1]);
	temperature = (((temperature + 521) / 340) + 35);

	pr_info("read temperature = %d\n", temperature);

	count = sprintf(buf, "%d\n", temperature);

	return count;
}
static struct device_attribute dev_attr_temperature =
	__ATTR(temperature, S_IRUSR | S_IRGRP, mpu6500_get_temp,
		NULL);

static ssize_t mpu6500_input_gyro_selftest_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct mpu6500_input_data *data = gb_mpu_data;
	/* absolute gyro bias scaled by 1000 times. (gyro_bias x 1000) */
	int scaled_gyro_bias[3] = {0};
	/* absolute gyro rms scaled by 1000 times. (gyro_bias x 1000) */
	int scaled_gyro_rms[3] = {0};
	int packet_count[3] = {0};
	int ratio[3] = {0};
	int hw_result;
	int result;


	mutex_lock(&data->mutex);

	result = mpu6500_selftest_run(data->client,
				      packet_count,
				      scaled_gyro_bias,
				      scaled_gyro_rms, data->gyro_bias);

	hw_result = mpu6500_gyro_hw_self_check(data->client, ratio);

	pr_info("%s, result = %d, hw_result = %d\n", __func__,
			result, hw_result);

	if (!result) {
		/* store calibration to file */
		gyro_do_calibrate();
	} else {
		data->gyro_bias[0] = 0;
		data->gyro_bias[1] = 0;
		data->gyro_bias[2] = 0;
	}

	mutex_unlock(&data->mutex);

	return sprintf(buf, "%d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%01d,%d.%01d,%d.%01d,"
			"%d.%03d,%d.%03d,%d.%03d\n",
			result | hw_result,
			(int)abs(scaled_gyro_bias[0] / 1000),
			(int)abs(scaled_gyro_bias[0]) % 1000,
			(int)abs(scaled_gyro_bias[1] / 1000),
			(int)abs(scaled_gyro_bias[1]) % 1000,
			(int)abs(scaled_gyro_bias[2] / 1000),
			(int)abs(scaled_gyro_bias[2]) % 1000,
			scaled_gyro_rms[0] / 1000,
			(int)abs(scaled_gyro_rms[0]) % 1000,
			scaled_gyro_rms[1] / 1000,
			(int)abs(scaled_gyro_rms[1]) % 1000,
			scaled_gyro_rms[2] / 1000,
			(int)abs(scaled_gyro_rms[2]) % 1000,
			(int)abs(ratio[0]/10),
			(int)abs(ratio[0])%10,
			(int)abs(ratio[1]/10),
			(int)abs(ratio[1])%10,
			(int)abs(ratio[2]/10),
			(int)abs(ratio[2])%10,
			(int)abs(packet_count[0] / 100),
			(int)abs(packet_count[0]) % 100,
			(int)abs(packet_count[1] / 100),
			(int)abs(packet_count[1]) % 100,
			(int)abs(packet_count[2] / 100),
			(int)abs(packet_count[2]) % 100);
}

static struct device_attribute dev_attr_selftest =
	__ATTR(selftest, S_IRUSR | S_IRGRP,
		mpu6500_input_gyro_selftest_show,
		NULL);

static ssize_t acc_data_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 x;
	s16 y;
	s16 z;
	read_accel_raw_xyz_cal(&x, &y, &z);
/*     pr_info("%s : x = %d, y = %d, z = %d\n", __func__, x, y, z); */
	return sprintf(buf, "%d, %d, %d\n", x, y, z);
}
static struct device_attribute dev_attr_raw_data =
	__ATTR(raw_data, S_IRUSR | S_IRGRP, acc_data_read,
		NULL);

static ssize_t accel_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int count = 0;
	s16 x;
	s16 y;
	s16 z;
	int err;

	x = gb_mpu_data->acc_cal[0];
	y = gb_mpu_data->acc_cal[1];
	z = gb_mpu_data->acc_cal[2];
	pr_info(" accel_calibration_show %d %d %d\n", x, y, z);

	if (!x && !y && !z)
		err = -1;
	else
		err = 1;

	count = sprintf(buf, "%d %d %d %d\n", err, x, y, z);

	return count;
}
static ssize_t accel_calibration_store(struct device *dev,
				struct device_attribute *attr,
					const char *buf, size_t size)
{
	int err;
	int count;
	unsigned long enable;
	s16 x;
	s16 y;
	s16 z;
	char tmp[64];

	if (strict_strtoul(buf, 10, &enable))
		return -EINVAL;
	err = accel_do_calibrate(enable);
	if (err < 0)
		pr_err("%s: accel_do_calibrate() failed\n", __func__);
	x = gb_mpu_data->acc_cal[0];
	y = gb_mpu_data->acc_cal[1];
	z = gb_mpu_data->acc_cal[2];

	pr_info("accel_calibration_store %d %d %d\n", x, y, z);
	if (err > 0)
		err = 0;
	count = sprintf(tmp, "%d\n", err);

	return count;
}
static struct device_attribute dev_attr_calibration =
	__ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		accel_calibration_show, accel_calibration_store);

static ssize_t accel_vendor_show(struct device *dev,
				struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", "INVENSENSE");
}
static struct device_attribute dev_attr_accel_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, accel_vendor_show, NULL);
static struct device_attribute dev_attr_gyro_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, accel_vendor_show, NULL);

static ssize_t accel_name_show(struct device *dev,
				struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", "MPU6500");
}
static struct device_attribute dev_attr_accel_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, accel_name_show, NULL);
static struct device_attribute dev_attr_gyro_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, accel_name_show, NULL);

static struct device_attribute *gyro_sensor_attrs[] = {
	&dev_attr_power_on,
	&dev_attr_temperature,
	&dev_attr_gyro_sensor_vendor,
	&dev_attr_gyro_sensor_name,
	&dev_attr_selftest,
	NULL,
};

static struct device_attribute *accel_sensor_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_reactive_alert,
	&dev_attr_accel_sensor_vendor,
	&dev_attr_accel_sensor_name,
	NULL,
};

static int __devinit mpu6500_input_register_input_device(struct
							 mpu6500_input_data
							 *data)
{
	struct input_dev *idev;
	int error;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	pr_info("%s is called\n", __func__);

	idev->name = MPU6K_INPUT_DRIVER;
	idev->id.bustype = BUS_I2C;

	idev->evbit[0] = BIT_MASK(EV_REL);
	idev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y) | BIT_MASK(REL_Z) |
	    BIT_MASK(REL_RX) | BIT_MASK(REL_RY) | BIT_MASK(REL_RZ) | BIT_MASK(REL_MISC);
	input_set_capability(idev, EV_ABS, REL_X);
	input_set_capability(idev, EV_ABS, REL_Y);
	input_set_capability(idev, EV_ABS, REL_Z);

	input_set_capability(idev, EV_ABS, REL_RX);
	input_set_capability(idev, EV_ABS, REL_RY);
	input_set_capability(idev, EV_ABS, REL_RZ);

	input_set_capability(idev, EV_ABS, REL_MISC);


	input_set_drvdata(idev, data);

	error = input_register_device(idev);
	if (error) {
		input_free_device(idev);
		return error;
	}

	error = sysfs_create_group(&idev->dev.kobj, &mpu6500_attribute_group);
	if (error) {
		input_free_device(idev);
		return error;
	}
#ifdef CONFIG_SENSOR_USE_SYMLINK
	error =  sensors_initialize_symlink(idev);
	if (error < 0) {
		input_free_device(idev);
		return error;
	}
#endif
	mutex_init(&data->mutex);

	atomic_set(&data->accel_enable, 0);
	atomic_set(&data->accel_delay, 10);
	atomic_set(&data->motion_recg_enable, 0);
	atomic_set(&data->reactive_state, 0);
#ifdef CONFIG_INPUT_MPU6500_LP
	atomic_set(&data->lp_scr_orient_enable, 0);
#endif
	data->input = idev;
	return 0;
}

static int mpu6500_misc_open(struct inode *inode, struct file *file)
{
	int ret = 0;
#if DEBUG
	pr_info("%s\n", __func__);
#endif

	return ret;
}

static int mpu6500_misc_release(struct inode *inode, struct file *file)
{
#if DEBUG
	pr_info("%s\n", __func__);
#endif

	return 0;
}

/*
static int
mpu6500_misc_ioctl(struct file *file,
	   unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	short flag;
#if DEBUG
	pr_info("%s\n", __func__);
#endif

	return 0;
}
*/

static const struct file_operations mpu6500_misc_fops = {
	.owner = THIS_MODULE,
	.open = mpu6500_misc_open,
	.release = mpu6500_misc_release,
/*	.unlocked_ioctl = mpu6500_misc_ioctl,*/
};

static struct miscdevice mpu6500_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mpu6500",
	.fops = &mpu6500_misc_fops,
};


#ifdef CONFIG_INPUT_MPU6500_POLLING
static void mpu6500_work_func_acc(struct work_struct *work)
{
	struct mpu6500_input_data *data =
		container_of((struct delayed_work *)work,
			struct mpu6500_input_data, accel_work);
	mpu6500_input_report_accel_xyz(data);

	pr_debug("%s: enable=%d, delay=%d\n", __func__,
		atomic_read(&data->accel_enable),
			atomic_read(&data->accel_delay));
	if (atomic_read(&data->accel_delay) < 60) {
		usleep_range(atomic_read(&data->accel_delay) * 1000,
			atomic_read(&data->accel_delay) * 1100);
		schedule_delayed_work(&data->accel_work, 0);
	} else {
		schedule_delayed_work(&data->accel_work,
			msecs_to_jiffies(
			atomic_read(&data->accel_delay)));
	}
}

static void mpu6500_work_func_gyro(struct work_struct *work)
{
	struct mpu6500_input_data *data =
		container_of((struct delayed_work *)work,
			struct mpu6500_input_data, gyro_work);

	mpu6500_input_report_gyro_xyz(data);

	pr_debug("%s: enable=%d, delay=%d\n", __func__,
		atomic_read(&data->gyro_enable),
			atomic_read(&data->gyro_delay));

	if (atomic_read(&data->gyro_delay) < 60) {
		usleep_range(atomic_read(&data->gyro_delay) * 1000,
			atomic_read(&data->gyro_delay) * 1100);
		schedule_delayed_work(&data->gyro_work, 0);
	} else {
		schedule_delayed_work(&data->gyro_work,
			msecs_to_jiffies(
			atomic_read(&data->gyro_delay)));
	}
}
#endif

static int __devinit mpu6500_input_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	struct mpu6k_input_platform_data *pdata =
	    client->dev.platform_data;
	const struct mpu6500_input_cfg *cfg;
	struct mpu6500_input_data *data;
	int error;
	unsigned char whoami = 0;

	printk(KERN_INFO "mpu6500_input_probe()\n");
	client->addr = 0x68;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}


	data = kzalloc(sizeof(struct mpu6500_input_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	data->pdata = pdata;
	gb_mpu_data= data;
	if (pdata->power_on)
		pdata->power_on(true);

	error = mpu6500_i2c_read_reg(client, MPUREG_WHOAMI, 1, &whoami);
	if (error < 0) {
		pr_err("%s failed : threre is no such device.\n", __func__);
		goto err_free_mem;
	}
	if (whoami != 0x70) {
		pr_err("%s: mpu6500 probe failed\n", __func__);
		error = -EIO;
		goto err_free_mem;
	}

	error = mpu6500_input_initialize(data, cfg);
	if (error)
		goto err_free_mem;

#ifdef CONFIG_INPUT_MPU6500_POLLING
	INIT_DELAYED_WORK(&data->accel_work, mpu6500_work_func_acc);
	INIT_DELAYED_WORK(&data->gyro_work, mpu6500_work_func_gyro);
#endif

	wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND,
		"reactive_wake_lock");

	if (client->irq > 0) {
		error = mpu6500_input_register_input_device(data);
		if (error)
			goto err_free_mem;

		error = request_threaded_irq(client->irq,
					     NULL, mpu6500_input_irq_thread,
					     IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					     MPU6500_INPUT_DRIVER, data);
		if (error) {
			dev_err(&client->dev,
				"irq request failed %d, error %d\n",
				client->irq, error);
			goto err_request_irq_failed;
		}
#ifdef CONFIG_INPUT_MPU6500_POLLING
		disable_irq(client->irq);
		gb_mpu_data->irq_flag = MPU6500_IRQ_DISABLE_STAT;
#endif
	}

	error = misc_register(&mpu6500_misc_device);
	if (error)
		goto err_misc_register_failed;

	i2c_set_clientdata(client, data);
	pm_runtime_enable(&client->dev);

	error = sensors_register(data->accel_sensor_device,
		NULL, accel_sensor_attrs,
			"accelerometer_sensor");
	if (error) {
		pr_err("%s: cound not register accelerometer sensor device(%d).\n",
			__func__, error);
		goto acc_sensor_register_failed;
	}

	error = sensors_register(data->gyro_sensor_device,
		NULL, gyro_sensor_attrs,
			"gyro_sensor");
	if (error) {
		pr_err("%s: cound not register gyro sensor device(%d).\n",
			__func__, error);
		goto gyro_sensor_register_failed;
	}

	return 0;

gyro_sensor_register_failed:
	sensors_unregister(data->accel_sensor_device);
acc_sensor_register_failed:
	misc_deregister(&mpu6500_misc_device);
err_misc_register_failed:
	free_irq(client->irq, client);
err_request_irq_failed:
	input_unregister_device(data->input);
err_free_mem:
	kfree(data);
	return error;
}

static int __devexit mpu6500_input_remove(struct i2c_client *client)
{
	struct mpu6500_input_data *data = i2c_get_clientdata(client);
	if (data == NULL)
		return 0;

	pm_runtime_disable(&client->dev);

	if (client->irq > 0) {
		free_irq(client->irq, data);
		input_unregister_device(data->input);
	}

	kfree(data);

	return 0;
}

static int mpu6500_input_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6500_input_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_INPUT_MPU6500_POLLING
		if (atomic_read(&data->accel_enable))
			cancel_delayed_work_sync(&data->accel_work);
		if (atomic_read(&data->gyro_enable))
			cancel_delayed_work_sync(&data->gyro_work);
#endif
	if (!atomic_read(&data->reactive_enable)) {
#ifndef CONFIG_INPUT_MPU6500_POLLING
		disable_irq_wake(client->irq);
		disable_irq(client->irq);
#endif
		if (atomic_read(&data->accel_enable) ||
			atomic_read(&data->gyro_enable))
			mpu6500_input_set_mode(data, MPU6500_MODE_SLEEP);
	} else {
		if(gb_mpu_data->irq_flag == MPU6500_IRQ_ENABLE_STAT) {
			pr_info("[SENSOR - %s] irq disable\n", __func__);
			disable_irq(client->irq);
			gb_mpu_data->irq_flag = MPU6500_IRQ_DISABLE_STAT;
		} else {
			pr_info("[SENSOR - %s] disable irq reject\n", __func__);
		}
	}
	return 0;
}

static int mpu6500_input_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6500_input_data *data = i2c_get_clientdata(client);

	if (client == NULL)
		return 0;
	data = i2c_get_clientdata(client);
	if (data == NULL)
		return 0;

	if (!atomic_read(&data->reactive_enable)) {
#ifndef CONFIG_INPUT_MPU6500_POLLING
		enable_irq(client->irq);
		enable_irq_wake(client->irq);
#endif
		if (atomic_read(&data->accel_enable) ||
			atomic_read(&data->gyro_enable))
			mpu6500_input_set_mode(data, MPU6500_MODE_NORMAL);
	} else {
		if(gb_mpu_data->irq_flag == MPU6500_IRQ_DISABLE_STAT) {
			pr_info("[SENSOR - %s] irq enable\n", __func__);
			enable_irq(client->irq);
			gb_mpu_data->irq_flag = MPU6500_IRQ_ENABLE_STAT;
		} else
			pr_info("[SENSOR - %s] enable irq reject\n", __func__);
	}
#ifdef CONFIG_INPUT_MPU6500_POLLING
		if (atomic_read(&data->accel_enable))
			schedule_delayed_work(&data->accel_work, 0);
		if (atomic_read(&data->gyro_enable))
			schedule_delayed_work(&data->gyro_work, 0);
#endif

	return 0;
}

static const struct i2c_device_id mpu6500_input_id[] = {
	{"mpu6500_input", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mpu6500_input_id);

static const struct dev_pm_ops mpu6500_dev_pm_ops = {
	.suspend = mpu6500_input_suspend,
	.resume = mpu6500_input_resume,
};

static struct i2c_driver mpu6500_input_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MPU6500_INPUT_DRIVER,
		   .pm = &mpu6500_dev_pm_ops
		   },
	.class = I2C_CLASS_HWMON,
	.id_table = mpu6500_input_id,
	.probe = mpu6500_input_probe,
	.remove = __devexit_p(mpu6500_input_remove),
};

static int __init mpu6500_init(void)
{
	int result = i2c_add_driver(&mpu6500_input_driver);

	printk(KERN_INFO "mpu6500_init()\n");

	return result;
}

static void __exit mpu6500_exit(void)
{
	printk(KERN_INFO "mpu6500_exit()\n");

	i2c_del_driver(&mpu6500_input_driver);
}

MODULE_AUTHOR("Tae-Soo Kim <tskim@invensense.com>");
MODULE_DESCRIPTION("MPU6500 driver");
MODULE_LICENSE("GPL");

module_init(mpu6500_init);
module_exit(mpu6500_exit);

