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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include "sx9310_reg.h"

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9310"
#define MODULE_NAME              "grip_sensor"
#define CALIBRATION_FILE_PATH    "/efs/FactoryApp/grip_cal_data"
#define SX9310_GRIP_SENSOR_NAME	 "sx9310_grip"

#define I2C_M_WR                 0 /* for i2c Write */
#define I2c_M_RD                 1 /* for i2c Read */

#define IDLE                     0
#define ACTIVE                   1

#define CAL_RET_ERROR            -1
#define CAL_RET_NONE             0
#define CAL_RET_EXIST            1
#define CAL_RET_SUCCESS          2

#define INIT_TOUCH_MODE          0
#define NORMAL_TOUCH_MODE        1

#define SX9310_MODE_SLEEP        0
#define SX9310_MODE_NORMAL       1

#define MAIN_SENSOR              1
#define REF_SENSOR               2
#define CSX_STATUS_REG           SX9310_TCHCMPSTAT_TCHSTAT0_FLAG

#define LIMIT_PROXOFFSET         13500 /* 99.9pF */
#define LIMIT_PROXUSEFUL_MIN     -10000
#define LIMIT_PROXUSEFUL_MAX     10000
#define PROXUSEFUL_DELTA_SPEC    2000
#define STANDARD_CAP_MAIN        1000000

/* CS0, CS1, CS2, CS3 */
#define ENABLE_CSX               (1 << MAIN_SENSOR)/* | (1 << REF_SENSOR)) */

#define IRQ_PROCESS_CONDITION   (SX9310_IRQSTAT_TOUCH_FLAG	\
				| SX9310_IRQSTAT_RELEASE_FLAG	\
				| SX9310_IRQSTAT_COMPDONE_FLAG)

#define DEFENCE_CODE_FOR_DEVICE_DAMAGE
#define SX9310_CPS_CTRL0_REG_VAL	0x10

#ifdef CONFIG_SENSORS_SX9310_DEFENCE_CODE_FOR_TA_NOISE
#ifdef CONFIG_SENSORS_GTS210_COMMON
#define SX9310_NORMAL_TOUCH_CABLE_THRESHOLD	208
#else
#define SX9310_NORMAL_TOUCH_CABLE_THRESHOLD	232
#endif
#endif

struct sx9310_p {
	struct i2c_client *client;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct wake_lock grip_wake_lock;
	struct mutex mode_mutex;
	struct mutex read_mutex;
	bool cal_successed;
	bool flag_dataskip;
	u8 normal_th;
	u8 normal_th_buf;
	int init_th;
	int cal_data[3];
	int touch_mode;
	int irq;
	int gpio_nirq;
	int state;
	s32 capmain;
	s16 useful;
	s16 avg;
	u16 offset;
	u16 freq;
	atomic_t enable;

	/* iio variables */
	struct iio_trigger *trig;
	struct iio_dev *indio_dev;
	struct mutex lock;
	spinlock_t spin_lock;
	int iio_state;
	atomic_t pseudo_irq_enable;
};

struct sx9310_iio_data {
	struct sx9310_p *cdata;
};

enum {
	SX9310_SCAN_GRIP_CH,
	SX9310_SCAN_GRIP_TIMESTAMP,
};

#define SX9310_GRIP_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define SX9310_GRIP_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

static const struct iio_chan_spec sx9310_channels[] = {
	{
		.type = IIO_GRIP,
		.modified = 1,
		.channel2 = IIO_MOD_GRIP,
		.info_mask_separate = SX9310_GRIP_INFO_SEPARATE_MASK,
		.info_mask_shared_by_type = SX9310_GRIP_INFO_SHARED_MASK,
		.scan_index = SX9310_SCAN_GRIP_CH,
		.scan_type = IIO_ST('s', 32, 32, 0),
	},
	IIO_CHAN_SOFT_TIMESTAMP(SX9310_SCAN_GRIP_TIMESTAMP),
};

#ifdef CONFIG_SENSORS_SX9310_DEFENCE_CODE_FOR_TA_NOISE
#include <linux/power_supply.h>

static int check_ta_state(void)
{
	static struct power_supply *psy = NULL;
	union power_supply_propval ret = {0,};

	if (psy == NULL) {
		psy = power_supply_get_by_name("battery");
		if (psy == NULL) {
			pr_err("[SX9310]: failed to get ps battery\n");
			return -EINVAL;
		}
	}

	psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &ret);

	return ret.intval;
}
#endif

static int sx9310_get_nirq_state(struct sx9310_p *data)
{
	return gpio_get_value_cansleep(data->gpio_nirq);
}

static int sx9310_i2c_write(struct sx9310_p *data, u8 reg_addr, u8 buf)
{
	int ret;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = data->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("[SX9310]: %s - i2c write error %d\n",
			__func__, ret);

	return ret;
}

static int sx9310_i2c_read(struct sx9310_p *data, u8 reg_addr, u8 *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0)
		pr_err("[SX9310]: %s - i2c read error %d\n",
			__func__, ret);

	return ret;
}

static int sx9310_i2c_read_block(struct sx9310_p *data, u8 reg_addr,
	u8 *buf, u8 buf_size)
{
      int ret;
      struct i2c_msg msg[2];

      msg[0].addr = data->client->addr;
      msg[0].flags = I2C_M_WR;
      msg[0].len = 1;
      msg[0].buf = &reg_addr;

      msg[1].addr = data->client->addr;
      msg[1].flags = I2C_M_RD;
      msg[1].len = buf_size;
      msg[1].buf = buf;

      ret = i2c_transfer(data->client->adapter, msg, 2);
      if (ret < 0)
            pr_err("[SX9310]: %s - i2c read error %d\n", __func__, ret);

      return ret;
}

static u8 sx9310_read_irqstate(struct sx9310_p *data)
{
	u8 val = 0;
	u8 ret;

	if (sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val) >= 0) {
		ret = val & 0x00FF;
		return ret;
	}

	return 0;
}

static void sx9310_initialize_register(struct sx9310_p *data)
{
	u8 val = 0;
	int idx;

	for (idx = 0; idx < (sizeof(setup_reg) >> 1); idx++) {
		sx9310_i2c_write(data, setup_reg[idx].reg, setup_reg[idx].val);
		pr_info("[SX9310]: %s - Write Reg: 0x%x Value: 0x%x\n",
			__func__, setup_reg[idx].reg, setup_reg[idx].val);

		sx9310_i2c_read(data, setup_reg[idx].reg, &val);
		pr_info("[SX9310]: %s - Read Reg: 0x%x Value: 0x%x\n\n",
			__func__, setup_reg[idx].reg, val);
	}
}

static void sx9310_initialize_chip(struct sx9310_p *data)
{
	int cnt = 0;

	while ((sx9310_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9310_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		pr_err("[SX9310]: %s - s/w reset fail(%d)\n",
			__func__, cnt);

	sx9310_initialize_register(data);
}

static int sx9310_set_offset_calibration(struct sx9310_p *data)
{
	int ret = 0;

	ret = sx9310_i2c_write(data, SX9310_IRQSTAT_REG, 0xFF);

	return ret;
}

static inline s64 sx9310_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

static int sx9310_grip_data_rdy_trig_poll(struct sx9310_p *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->spin_lock, flags);
	iio_trigger_poll(data->trig, sx9310_iio_get_boottime_ns());
	spin_unlock_irqrestore(&data->spin_lock, flags);
	return 0;
}

static void send_event(struct sx9310_p *data, u8 state)
{
	data->normal_th = data->normal_th_buf;
#ifdef CONFIG_SENSORS_SX9310_DEFENCE_CODE_FOR_TA_NOISE
	if (check_ta_state() > 1) {
		data->normal_th = SX9310_NORMAL_TOUCH_CABLE_THRESHOLD;
		pr_info("[SX9310]: %s - TA cable connected\n", __func__);
	}
#endif

	if (state == ACTIVE) {
		data->state = ACTIVE;

#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		pr_info("[SX9310]: %s - button touched\n", __func__);

	} else {
		data->touch_mode = NORMAL_TOUCH_MODE;
		data->state = IDLE;

#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		pr_info("[SX9310]: %s - button released\n", __func__);

	}
	if (data->flag_dataskip == true)
		return;

	if (state == ACTIVE) {
		data->iio_state = ACTIVE;
		sx9310_grip_data_rdy_trig_poll(data);
	} else {
		data->iio_state = IDLE;
		sx9310_grip_data_rdy_trig_poll(data);
	}
}

static void sx9310_display_data_reg(struct sx9310_p *data)
{
	u8 val, reg;

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	for (reg = SX9310_REGUSEMSB; reg <= SX9310_REGOFFSETLSB; reg++) {
		sx9310_i2c_read(data, reg, &val);
		pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
			__func__, reg, val);
	}
}

static s32 sx9310_get_init_threshold(struct sx9310_p *data)
{
	s32 threshold;

	if (data->cal_data[0] == 0)
		threshold = STANDARD_CAP_MAIN + data->init_th;
	else
		threshold = data->init_th + data->cal_data[0];

	return threshold;
}

#define RAW_DATA_BLOCK_SIZE (SX9310_REGOFFSETLSB - SX9310_REGUSEMSB + 1)

static void sx9310_get_data(struct sx9310_p *data)
{
	u8 ms_byte = 0;
	u8 is_byte = 0;
	u8 ls_byte = 0;

	u8 buf[RAW_DATA_BLOCK_SIZE];
	mutex_lock(&data->read_mutex);

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read_block(data, SX9310_REGUSEMSB,
		&buf[0], RAW_DATA_BLOCK_SIZE);

	data->useful = (s16)((s32)buf[0] << 8) | ((s32)buf[1]);
	data->avg = (s16)((s32)buf[2] << 8) | ((s32)buf[3]);
	data->offset = ((u16)buf[6] << 8) | ((u16)buf[7]);

	ms_byte = (u8)(data->offset >> 13);
	is_byte = (u8)((data->offset & 0x1fc0) >> 6);
	ls_byte = (u8)(data->offset & 0x3f);

	data->capmain = (((s32)ms_byte * 234000) + ((s32)is_byte * 9000) +
		((s32)ls_byte * 450)) + (((s32)data->useful * 50000) /
		(8 * 65536));

	mutex_unlock(&data->read_mutex);

	pr_info("[SX9310]: %s - CapsMain: %d, Useful: %d, Offset: %u\n",
		__func__, data->capmain, data->useful, data->offset);
}

static void sx9310_touchCheckWithRefSensor(struct sx9310_p *data)
{
	s32 threshold;

	threshold = sx9310_get_init_threshold(data);
	sx9310_get_data(data);

	if (data->state == IDLE) {
		if (data->capmain >= threshold)
			send_event(data, ACTIVE);
		else
			send_event(data, IDLE);
	} else {
		if (data->capmain < threshold)
			send_event(data, IDLE);
		else
			send_event(data, ACTIVE);
	}
}

static int sx9310_save_caldata(struct sx9310_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SX9310]: %s - Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)data->cal_data,
		sizeof(int) * 3, &cal_filp->f_pos);
	if (ret != (sizeof(int) * 3)) {
		pr_err("[SX9310]: %s - Can't write the cal data to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static void sx9310_open_caldata(struct sx9310_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		if (ret != -ENOENT)
			pr_err("[SX9310]: %s - Can't open calibration file.\n",
				__func__);
		else {
			pr_info("[SX9310]: %s - There is no calibration file\n",
				__func__);
			/* calibration status init */
			memset(data->cal_data, 0, sizeof(int) * 3);
		}
		set_fs(old_fs);
		return;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)data->cal_data,
		sizeof(int) * 3, &cal_filp->f_pos);
	if (ret != (sizeof(int) * 3))
		pr_err("[SX9310]: %s -Can't read the cal data from file\n",
			__func__);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SX9310]: %s - (%d, %d, %d)\n", __func__,
		data->cal_data[0], data->cal_data[1], data->cal_data[2]);
}

static int sx9310_set_mode(struct sx9310_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	mutex_lock(&data->mode_mutex);
	if (mode == SX9310_MODE_SLEEP) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[2].val);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
		/* make sure no interrupts are pending since enabling irq
		 * will only work on next falling edge */
		sx9310_read_irqstate(data);
	} else if (mode == SX9310_MODE_NORMAL) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[2].val | ENABLE_CSX);
		msleep(20);

		sx9310_set_offset_calibration(data);
		msleep(400);

		sx9310_touchCheckWithRefSensor(data);
		/* make sure no interrupts are pending since enabling irq
		 * will only work on next falling edge */
		sx9310_read_irqstate(data);
		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	}

	pr_info("[SX9310]: %s - change the mode : %u\n", __func__, mode);

	mutex_unlock(&data->mode_mutex);
	return ret;
}

static int sx9310_do_calibrate(struct sx9310_p *data, bool do_calib)
{
	int ret = 0, useful_min = 32768, useful_max = -32767;
	s32 useful_sum = 0;
	u8 ms_byte = 0;
	u8 is_byte = 0;
	u8 ls_byte = 0;
	int i = 0;

	memset(data->cal_data, 0, sizeof(int) * 3);
	if (do_calib == false) {
		pr_info("[SX9310]: %s - Erase!\n", __func__);
		goto cal_erase;
	}

	if (atomic_read(&data->enable) == OFF)
		sx9310_set_mode(data, SX9310_MODE_NORMAL);

	sx9310_get_data(data);
	data->cal_data[2] = data->offset;
	if ((data->cal_data[2] >= LIMIT_PROXOFFSET)
		|| (data->cal_data[2] == 0)) {
		pr_err("[SX9310]: %s - offset fail(%d)\n", __func__,
			data->cal_data[2]);
		goto cal_fail;
	}

	for (i = 0; i < 8; i++) {
		mdelay(90);
		sx9310_get_data(data);
		useful_sum += data->useful;
		if (data->useful > useful_max)
			useful_max = data->useful;
		if (data->useful < useful_min)
			useful_min = data->useful;

		pr_info("[SX9310]: %s - useful(%d)-offset(%u)\n",
				__func__, data->useful, data->offset);
		if (data->offset != data->cal_data[2]) {
			data->cal_data[1] = data->useful;
			pr_err("[SX9310]: %s - offset fail(%d)-(%d)\n",
				__func__, data->cal_data[2], data->offset);
			goto cal_fail;
		}
	}

	data->cal_data[1] = useful_sum >> 3;
	if ((useful_max - useful_min) > PROXUSEFUL_DELTA_SPEC) {
		pr_err("[SX9310]: %s - useful delta fail(min : %d, max : %d)\n",
			__func__, useful_min, useful_max);
		goto cal_fail;
	}

	if (data->cal_data[1] <= LIMIT_PROXUSEFUL_MIN ||
		data->cal_data[1] >= LIMIT_PROXUSEFUL_MAX) {
		pr_err("[SX9310]: %s - useful spec fail(%d)\n", __func__,
			data->cal_data[1]);
		goto cal_fail;
	}

	ms_byte = (u8)(data->cal_data[2] >> 13);
	is_byte = (u8)((data->cal_data[2] & 0x1fc0) >> 6);
	ls_byte = (u8)(data->cal_data[2] & 0x3f);

	data->cal_data[0] = (((s32)ms_byte * 234000) + ((s32)is_byte*9000)
		+ ((s32)ls_byte * 450)) + (((s32)data->cal_data[1]
		* 50000) / (8 * 65536));

	if (atomic_read(&data->enable) == OFF)
		sx9310_set_mode(data, SX9310_MODE_SLEEP);

	goto exit;

cal_fail:
	if (atomic_read(&data->enable) == OFF)
		sx9310_set_mode(data, SX9310_MODE_SLEEP);
	ret = -1;
cal_erase:
exit:
	pr_info("[SX9310]: %s - (%d, %d, %d)\n", __func__,
		data->cal_data[0], data->cal_data[1], data->cal_data[2]);
	return ret;
}

static void sx9310_set_enable(struct sx9310_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	if (enable) {
		if (pre_enable == OFF) {
			data->touch_mode = INIT_TOUCH_MODE;
			data->cal_successed = false;

			sx9310_open_caldata(data);
			sx9310_set_mode(data, SX9310_MODE_NORMAL);
			atomic_set(&data->enable, ON);
		}
	} else {
		if (pre_enable == ON) {
			sx9310_set_mode(data, SX9310_MODE_SLEEP);
			atomic_set(&data->enable, OFF);
		}
	}
}

static ssize_t sx9310_get_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sx9310_set_offset_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	if (val)
		sx9310_set_offset_calibration(data);

	return count;
}

static ssize_t sx9310_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0, val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%4d,%4d", &regist, &val) != 2) {
		pr_err("[SX9310]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9310_i2c_write(data, (unsigned char)regist, (unsigned char)val);
	pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9310_register_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0;
	unsigned char val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%4d", &regist) != 1) {
		pr_err("[SX9310]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9310_i2c_read(data, (unsigned char)regist, &val);
	pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9310_read_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_display_data_reg(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9310_sw_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_mode(data, SX9310_MODE_SLEEP);

	ret = sx9310_i2c_write(data, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	msleep(300);

	sx9310_initialize_chip(data);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_mode(data, SX9310_MODE_NORMAL);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t sx9310_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return count;
	}

	data->freq = (u16)val;
	val = ((val << 3) | 0x05) & 0xff;
	sx9310_i2c_write(data, SX9310_CPS_CTRL4_REG, (u8)val);

	pr_info("[SX9310]: %s - Freq : 0x%x\n", __func__, data->freq);

	return count;
}

static ssize_t sx9310_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	pr_info("[SX9310]: %s - Freq : 0x%x\n", __func__, data->freq);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->freq);
}

static ssize_t sx9310_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9310_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx9310_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->touch_mode);
}

static ssize_t sx9310_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 proxdiff;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == OFF)
		pr_err("[SX9310]: %s - SX9310 was not enabled\n",
			__func__);

	sx9310_get_data(data);
	proxdiff = (data->useful - data->avg) >> 4;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u,%d\n",
			data->capmain, data->useful, data->offset, proxdiff);
}

static ssize_t sx9310_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			sx9310_get_init_threshold(data));
}

static ssize_t sx9310_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9310]: %s - init threshold %lu\n", __func__, val);
	data->init_th = (int)val;

	return count;
}

static ssize_t sx9310_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[32] = {2, 4, 6, 8, 12, 16, 20, 24, 28, 32,
				40, 48, 56, 64, 72, 80, 88, 96, 112, 128,
				144, 160, 192, 224, 256, 320, 384, 512, 640,
				768, 1024, 1536};

	thresh_temp = (data->normal_th >> 3) & 0x1f;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[12].val >> 4) & 0x3;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
}

static ssize_t sx9310_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9310]: %s - normal threshold %lu\n", __func__, val);
	data->normal_th_buf = data->normal_th = (u8)(val << 3);

	return count;
}

static ssize_t sx9310_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->flag_dataskip);
}

static ssize_t sx9310_onoff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (val == 0)
		data->flag_dataskip = true;
	else
		data->flag_dataskip = false;

	pr_info("[SX9310]: %s -%u\n", __func__, val);
	return count;
}

static ssize_t sx9310_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if ((data->cal_successed == false) && (data->cal_data[0] == 0))
		ret = CAL_RET_NONE;
	else if ((data->cal_successed == false) && (data->cal_data[0] != 0))
		ret = CAL_RET_EXIST;
	else if ((data->cal_successed == true) && (data->cal_data[0] != 0))
		ret = CAL_RET_SUCCESS;
	else
		ret = CAL_RET_ERROR;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", ret,
			data->cal_data[1], data->cal_data[2]);
}

static ssize_t sx9310_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool do_calib;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1"))
		do_calib = true;
	else if (sysfs_streq(buf, "0"))
		do_calib = false;
	else {
		pr_info("[SX9310]: %s - invalid value %d\n",
			__func__, *buf);
		return -EINVAL;
	}

	ret = sx9310_do_calibrate(data, do_calib);
	if (ret < 0) {
		pr_err("[SX9310]: %s - sx9310_do_calibrate fail(%d)\n",
			__func__, ret);
		goto exit;
	}

	ret = sx9310_save_caldata(data);
	if (ret < 0) {
		pr_err("[SX9310]: %s - sx9310_save_caldata fail(%d)\n",
			__func__, ret);
		memset(data->cal_data, 0, sizeof(int) * 3);
		goto exit;
	}

	pr_info("[SX9310]: %s - %u success!\n", __func__, do_calib);

exit:

	if ((data->cal_data[0] != 0) && (ret >= 0))
		data->cal_successed = true;
	else
		data->cal_successed = false;

	return count;
}

static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_get_offset_calibration_show,
		sx9310_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IWUSR | S_IWGRP,
		NULL, sx9310_register_write_store);
static DEVICE_ATTR(register_read, S_IWUSR | S_IWGRP,
		NULL, sx9310_register_read_store);
static DEVICE_ATTR(readback, S_IRUGO, sx9310_read_data_show, NULL);
static DEVICE_ATTR(reset, S_IRUGO, sx9310_sw_reset_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, sx9310_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx9310_vendor_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx9310_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx9310_raw_data_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_calibration_show, sx9310_calibration_store);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_onoff_show, sx9310_onoff_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_threshold_show, sx9310_threshold_store);
static DEVICE_ATTR(normal_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_normal_threshold_show, sx9310_normal_threshold_store);
static DEVICE_ATTR(freq, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_freq_show, sx9310_freq_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_readback,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_raw_data,
	&dev_attr_threshold,
	&dev_attr_normal_threshold,
	&dev_attr_onoff,
	&dev_attr_calibration,
	&dev_attr_freq,
	NULL,
};

static void sx9310_touch_process(struct sx9310_p *data, u8 flag)
{
	u8 status = 0;
	s32 threshold;

	threshold = sx9310_get_init_threshold(data);
	sx9310_get_data(data);
	sx9310_i2c_read(data, SX9310_STAT0_REG, &status);

	if (flag & SX9310_IRQSTAT_COMPDONE_FLAG) {
		if (data->state == IDLE) {
			if (status & (CSX_STATUS_REG << MAIN_SENSOR))
				send_event(data, ACTIVE);
			else if (data->capmain > threshold)
				send_event(data, ACTIVE);
			else
				pr_info("[SX9310]: %s-already released.\n",
					__func__);
		} else { /* User released button */
			if (status & (CSX_STATUS_REG << MAIN_SENSOR))
				pr_info("[SX9310]: %s - still touched.\n",
					__func__);
			else if (data->capmain <= threshold + 300)
				send_event(data, IDLE);
			else
				pr_info("[SX9310]: %s - still touched.\n",
					__func__);
		}

		return;
	}

	if (data->state == IDLE) {
		if (status & (CSX_STATUS_REG << MAIN_SENSOR)) {
#ifdef DEFENCE_CODE_FOR_DEVICE_DAMAGE
			if ((data->cal_data[0] - 5000) > data->capmain) {
				sx9310_set_offset_calibration(data);
				msleep(400);
				sx9310_touchCheckWithRefSensor(data);
				pr_err("[SX9310]: %s - Defence code\n",
					__func__);
				return;
			}
#endif
			send_event(data, ACTIVE);
		} else {
			pr_info("[SX9310]: %s - already released.\n",
				__func__);
		}
	} else { /* User released button */
		if (!(status & (CSX_STATUS_REG << MAIN_SENSOR))) {
			if ((data->touch_mode == INIT_TOUCH_MODE)
				&& (data->capmain >= threshold))
				pr_info("[SX9310]: %s - IDLE SKIP\n",
					__func__);
			else
				send_event(data , IDLE);
		} else {
			pr_info("[SX9310]: %s - still touched\n",
				__func__);
		}
	}
}

static void sx9310_process_interrupt(struct sx9310_p *data)
{
	u8 flag = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	flag = sx9310_read_irqstate(data);

	if (flag & IRQ_PROCESS_CONDITION)
		sx9310_touch_process(data, flag);
}

static void sx9310_init_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, init_work);

	sx9310_initialize_chip(data);

	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9310_read_irqstate(data);
}

static void sx9310_irq_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, irq_work);

	if (sx9310_get_nirq_state(data) == 0)
		sx9310_process_interrupt(data);
	else
		pr_err("[SX9310]: %s - nirq read high %d\n",
			__func__, sx9310_get_nirq_state(data));
}

static irqreturn_t sx9310_interrupt_thread(int irq, void *pdata)
{
	struct sx9310_p *data = pdata;

	if (sx9310_get_nirq_state(data) == 1) {
		pr_err("[SX9310]: %s - nirq read high\n", __func__);
	} else {
		wake_lock_timeout(&data->grip_wake_lock, 3 * HZ);
		schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));
	}

	return IRQ_HANDLED;
}

static int sx9310_setup_pin(struct sx9310_p *data)
{
	int ret;

	ret = gpio_request(data->gpio_nirq, "SX9310_nIRQ");
	if (ret < 0) {
		pr_err("[SX9310]: %s - gpio %d request failed (%d)\n",
			__func__, data->gpio_nirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_nirq);
	if (ret < 0) {
		pr_err("[SX9310]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->gpio_nirq, ret);
		gpio_free(data->gpio_nirq);
		return ret;
	}

	return 0;
}

static void sx9310_initialize_variable(struct sx9310_p *data)
{
	data->state = IDLE;
	data->touch_mode = INIT_TOUCH_MODE;
	data->flag_dataskip = false;
	data->cal_successed = false;
	data->freq = setup_reg[6].val >> 3;
	memset(data->cal_data, 0, sizeof(int) * 3);
	atomic_set(&data->enable, OFF);
	data->init_th = (int)CONFIG_SENSORS_SX9310_INIT_TOUCH_THRESHOLD;
	pr_info("[SX9310]: %s - Init Touch Threshold : %d\n",
		__func__, data->init_th);
	data->normal_th = (u8)CONFIG_SENSORS_SX9310_NORMAL_TOUCH_THRESHOLD;
	data->normal_th_buf = data->normal_th;
	pr_info("[SX9310]: %s - Normal Touch Threshold : %u\n",
		__func__, data->normal_th);
}

irqreturn_t sx9310_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;

	pf->timestamp = sx9310_iio_get_boottime_ns();

	return IRQ_WAKE_THREAD;
}

static irqreturn_t sx9310_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	int len = 0;
	int32_t *pdata;

	pdata = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (pdata == NULL)
		goto done;
	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		/* TODO : data update */
		if (data->iio_state)
			*pdata = 0;
		else
			*pdata = 1;
	}
	len = 4;
	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)pdata + ALIGN(len, sizeof(s64))) = pf->timestamp;
	iio_push_to_buffers(indio_dev, (u8 *)pdata);
	kfree(pdata);
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int sx9310_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	if (!atomic_cmpxchg(&data->pseudo_irq_enable, 0, 1)) {
		mutex_lock(&data->lock);
		sx9310_set_enable(data, 1);
		mutex_unlock(&data->lock);
	}
	return 0;
}

static int sx9310_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	if (atomic_cmpxchg(&data->pseudo_irq_enable, 1, 0)) {
		mutex_lock(&data->lock);
		sx9310_set_enable(data, 0);
		mutex_unlock(&data->lock);
	}
	return 0;
}

static int sx9310_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		sx9310_pseudo_irq_enable(indio_dev);
	else
		sx9310_pseudo_irq_disable(indio_dev);
	return 0;
}

static int sx9310_data_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);

	sx9310_set_pseudo_irq(indio_dev, state);

	return 0;
}

static const struct iio_trigger_ops sx9310_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &sx9310_data_rdy_trigger_set_state,
};

static int sx9310_parse_dt(struct sx9310_p *data, struct device *dev)
{
	struct device_node *dnode = dev->of_node;
	enum of_gpio_flags flags;

	if (dnode == NULL)
		return -ENODEV;

	data->gpio_nirq = of_get_named_gpio_flags(dnode,
		"sx9310-i2c,nirq-gpio", 0, &flags);
	if (data->gpio_nirq < 0) {
		pr_err("[SX9310]: %s - get gpio_nirq error\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int sx9310_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	indio_dev->pollfunc = iio_alloc_pollfunc(
			&sx9310_iio_pollfunc_store_boottime,
			&sx9310_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	data->trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!data->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}

	data->trig->dev.parent = &data->client->dev;
	data->trig->ops = &sx9310_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);
	ret = iio_trigger_register(data->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(data->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void sx9310_remove_trigger(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	iio_trigger_unregister(data->trig);
	iio_trigger_free(data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_buffer_setup_ops sx9310_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int sx9310_probe_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;
	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	buffer->scan_timestamp = true;
	indio_dev->buffer = buffer;
	indio_dev->setup_ops = &sx9310_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(indio_dev, indio_dev->buffer, SX9310_SCAN_GRIP_CH);
	return 0;
error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static void sx9310_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static int sx9310_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;
	int ret = -EINVAL;

	if (chan->type != IIO_GRIP)
		return -EINVAL;
	mutex_lock(&data->lock);
	switch (mask) {
	case 0:
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}
	mutex_unlock(&data->lock);
	return ret;
}

static const struct iio_info sx9310_info = {
	.read_raw = &sx9310_read_raw,
	.driver_module = THIS_MODULE,
};

static int sx9310_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9310_p *data = NULL;
	struct sx9310_iio_data *sx9310_iio;

	pr_info("[SX9310]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SX9310]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	/* create memory for main struct */
	data = kzalloc(sizeof(struct sx9310_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SX9310]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	/* iio device register */
	data->indio_dev = iio_device_alloc(sizeof(*sx9310_iio));
	if (data->indio_dev == NULL) {
		pr_err("[SX9310]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kfree;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;

	sx9310_iio = iio_priv(data->indio_dev);
	sx9310_iio->cdata = data;

	data->indio_dev->name = SX9310_GRIP_SENSOR_NAME;
	data->indio_dev->dev.parent = &client->dev;
	data->indio_dev->info = &sx9310_info;
	data->indio_dev->channels = sx9310_channels;
	data->indio_dev->num_channels = ARRAY_SIZE(sx9310_channels);
	data->indio_dev->modes = INDIO_DIRECT_MODE;

	spin_lock_init(&data->spin_lock);
	wake_lock_init(&data->grip_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wake_lock");
	mutex_init(&data->mode_mutex);
	mutex_init(&data->read_mutex);
	mutex_init(&data->lock);
	ret = sx9310_parse_dt(data, &client->dev);
	if (ret < 0) {
		pr_err("[SX9310]: %s - of_node error\n", __func__);
		ret = -ENODEV;
		goto exit_of_node;
	}

	ret = sx9310_setup_pin(data);
	if (ret) {
		pr_err("[SX9310]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	/* read chip id */
	ret = sx9310_i2c_write(data, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	if (ret < 0) {
		pr_err("[SX9310]: %s - chip reset failed %d\n",
			__func__, ret);
		goto exit_chip_reset;
	}

	sx9310_initialize_variable(data);
	INIT_DELAYED_WORK(&data->init_work, sx9310_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9310_irq_work_func);

	/* initailize interrupt reporting */
	data->irq = gpio_to_irq(data->gpio_nirq);
	ret = request_threaded_irq(data->irq, NULL, sx9310_interrupt_thread,
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			"sx9310_irq", data);
	if (ret < 0) {
		pr_err("[SX9310]: %s - fail to set request irq %d(%d)"
			, __func__, data->irq, ret);
		goto exit_request_threaded_irq;
	}
	disable_irq(data->irq);

	ret = sx9310_probe_buffer(data->indio_dev);
	if (ret)
		goto exit_iio_buffer_probe_failed;

	ret = sx9310_probe_trigger(data->indio_dev);
	if (ret)
		goto exit_iio_trigger_probe_failed;

	ret = iio_device_register(data->indio_dev);
	if (ret < 0)
		goto exit_iio_register_failed;

	ret = sensors_register(data->factory_device,
		data, sensor_attrs, MODULE_NAME);
	if (ret) {
		pr_err("[SX9310] %s - can not register grip_sensor(%d).\n",
			__func__, ret);
		goto grip_sensor_register_failed;
	}

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));

	pr_info("[SX9310]: %s - Probe done!\n", __func__);

	return 0;

grip_sensor_register_failed:
exit_iio_register_failed:
	sx9310_remove_trigger(data->indio_dev);
exit_iio_trigger_probe_failed:
	sx9310_remove_buffer(data->indio_dev);
exit_iio_buffer_probe_failed:
	sensors_unregister(data->factory_device, sensor_attrs);
exit_request_threaded_irq:
exit_chip_reset:
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);
exit_setup_pin:
exit_of_node:
	wake_lock_destroy(&data->grip_wake_lock);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);
	mutex_destroy(&data->lock);
exit_kfree:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SX9310]: %s - Probe fail!\n", __func__);
	return ret;
}

static int sx9310_remove(struct i2c_client *client)
{
	struct sx9310_p *data = i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_mode(data, SX9310_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);

	wake_lock_destroy(&data->grip_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);

	iio_device_unregister(data->indio_dev);

	return 0;
}

static int sx9310_suspend(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		pr_info("[SX9310]: %s\n", __func__);

	return 0;
}

static int sx9310_resume(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		pr_info("[SX9310]: %s\n", __func__);

	return 0;
}

static struct of_device_id sx9310_match_table[] = {
	{ .compatible = "sx9310-i2c",},
	{},
};

static const struct i2c_device_id sx9310_id[] = {
	{ "sx9310_match_table", 0 },
	{ }
};

static const struct dev_pm_ops sx9310_pm_ops = {
	.suspend = sx9310_suspend,
	.resume = sx9310_resume,
};

static struct i2c_driver sx9310_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sx9310_match_table,
		.pm = &sx9310_pm_ops
	},
	.probe		= sx9310_probe,
	.remove		= sx9310_remove,
	.id_table	= sx9310_id,
};

static int __init sx9310_init(void)
{
	return i2c_add_driver(&sx9310_driver);
}

static void __exit sx9310_exit(void)
{
	i2c_del_driver(&sx9310_driver);
}

module_init(sx9310_init);
module_exit(sx9310_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9310 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
