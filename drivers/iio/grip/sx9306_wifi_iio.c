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
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>

#include "sx9306_wifi_reg.h"
#include <linux/sensor/sensors_core.h>
///IIO additions...
//
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9306_WIFI"
#define MODULE_NAME              "grip_sensor_wifi"
#define CALIBRATION_FILE_PATH    "/efs/FactoryApp/grip_wifi_cal_data"

#define SX9306_GRIP_WIFI_SENSOR_NAME					"sx9306_grip_wifi"

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

#define SX9306_WIFI_MODE_SLEEP        0
#define SX9306_WIFI_MODE_NORMAL       1

#define MAIN_SENSOR              2
#define REF_SENSOR               0
#define CSX_STATUS_REG           SX9306_WIFI_TCHCMPSTAT_TCHSTAT2_FLAG

#define LIMIT_PROXOFFSET                4000 /* 46 pF */
#define LIMIT_PROXUSEFUL                10000
#define STANDARD_CAP_MAIN               450000

#define DEFAULT_INIT_TOUCH_THRESHOLD    2000
#define DEFAULT_NORMAL_TOUCH_THRESHOLD  17

#define TOUCH_CHECK_REF_AMB      0 /* 44523 */
#define TOUCH_CHECK_SLOPE        0 /* 50 */
#define TOUCH_CHECK_MAIN_AMB     0 /* 151282 */

#define DEFENCE_CODE_FOR_DEVICE_DAMAGE

/* CS0, CS1, CS2, CS3 */
#define TOTAL_BOTTON_COUNT       1
#define ENABLE_CSX               ((1 << MAIN_SENSOR) /*| (1 << REF_SENSOR)*/)

#define IRQ_PROCESS_CONDITION   (SX9306_WIFI_IRQSTAT_TOUCH_FLAG	\
				| SX9306_WIFI_IRQSTAT_RELEASE_FLAG	\
				| SX9306_WIFI_IRQSTAT_COMPDONE_FLAG)

struct sx9306_wifi_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct wake_lock grip_wifi_wake_lock;
	struct mutex mode_mutex;

	bool calSuccessed;
	bool flagDataSkip;
	u8 touchTh;
	int initTh;
	int calData[3];
	int touchMode;

	int irq;
	int gpioNirq;
#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	int gpio_adjdet;
	int irq_adjdet;
	int grip_wifi_state;
	int enable_adjdet;
#endif
	int state[TOTAL_BOTTON_COUNT];

#if defined(CONFIG_SENSORS_SX9306_WIFI_REGULATOR_ONOFF)
	struct regulator *vdd;
	struct regulator *ldo;
#endif
	atomic_t enable;

	/* iio variables */
	struct iio_trigger *trig;
	struct mutex lock;
	spinlock_t spin_lock;
	int iio_state;
	atomic_t pseudo_irq_enable;
};

enum {
	SX9306_WIFI_SCAN_GRIP_WIFI_CH,
	SX9306_WIFI_SCAN_GRIP_WIFI_TIMESTAMP,
};

#define SX9306_WIFI_GRIP_WIFI_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define SX9306_WIFI_GRIP_WIFI_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

static const struct iio_chan_spec sx9306_wifi_channels[] = {
    {
	.type = IIO_GRIP_WIFI,
	.modified = 1,
	.channel2 = IIO_MOD_GRIP_WIFI,
	.info_mask_separate = SX9306_WIFI_GRIP_WIFI_INFO_SEPARATE_MASK,
	.info_mask_shared_by_type = SX9306_WIFI_GRIP_WIFI_INFO_SHARED_MASK,
	.scan_index = SX9306_WIFI_SCAN_GRIP_WIFI_CH,
	.scan_type = IIO_ST('s', 32, 32, 0),
    },
    IIO_CHAN_SOFT_TIMESTAMP(SX9306_WIFI_SCAN_GRIP_WIFI_TIMESTAMP),
};

static int sx9306_wifi_get_nirq_state(struct sx9306_wifi_p *data)
{
	return gpio_get_value_cansleep(data->gpioNirq);
}

static int sx9306_wifi_i2c_write(struct sx9306_wifi_p *data, u8 reg_addr, u8 buf)
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
		pr_err("[SX9306_WIFI]: %s - i2c write error %d\n", __func__, ret);

	return ret;
}

static int sx9306_wifi_i2c_read(struct sx9306_wifi_p *data, u8 reg_addr, u8 *buf)
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
		pr_err("[SX9306_WIFI]: %s - i2c read error %d\n", __func__, ret);

	return ret;
}

static u8 sx9306_wifi_read_irqstate(struct sx9306_wifi_p *data)
{
	u8 val = 0;
	u8 ret;

	if (sx9306_wifi_i2c_read(data, SX9306_WIFI_IRQSTAT_REG, &val) >= 0) {
		ret = val & 0x00FF;
		return ret;
	}

	return 0;
}

static void sx9306_wifi_initialize_register(struct sx9306_wifi_p *data)
{
	u8 val = 0;
	int idx;

	for (idx = 0; idx < (sizeof(setup_reg) >> 1); idx++) {
		sx9306_wifi_i2c_write(data, setup_reg[idx].reg, setup_reg[idx].val);
		pr_info("[SX9306_WIFI]: %s - Write Reg: 0x%x Value: 0x%x\n",
			__func__, setup_reg[idx].reg, setup_reg[idx].val);

		sx9306_wifi_i2c_read(data, setup_reg[idx].reg, &val);
		pr_info("[SX9306_WIFI]: %s - Read Reg: 0x%x Value: 0x%x\n\n",
			__func__, setup_reg[idx].reg, val);
	}
}

static void sx9306_wifi_initialize_chip(struct sx9306_wifi_p *data)
{
	int cnt = 0;

	while ((sx9306_wifi_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9306_wifi_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		pr_err("[SX9306_WIFI]: %s - s/w reset fail(%d)\n", __func__, cnt);

	sx9306_wifi_initialize_register(data);
}

static int sx9306_wifi_set_offset_calibration(struct sx9306_wifi_p *data)
{
	int ret = 0;

	ret = sx9306_wifi_i2c_write(data, SX9306_WIFI_IRQSTAT_REG, 0xFF);

	return ret;
}

static inline s64 sx9306_wifi_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

static int sx9306_grip_wifi_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	unsigned long flags;

	spin_lock_irqsave(&st->spin_lock, flags);
	iio_trigger_poll(st->trig, sx9306_wifi_iio_get_boottime_ns());
	spin_unlock_irqrestore(&st->spin_lock, flags);
	return 0;
}

static void send_event(struct sx9306_wifi_p *data, int cnt, u8 state)
{
	u8 buf;
	struct iio_dev *indio_dev = iio_priv_to_dev(data);

	buf = data->touchTh;

	if (state == ACTIVE) {
		data->state[cnt] = ACTIVE;
		sx9306_wifi_i2c_write(data, SX9306_WIFI_CPS_CTRL6_REG, buf);
		pr_info("[SX9306_WIFI]: %s - %d button touched\n", __func__, cnt);
	} else {
		data->touchMode = NORMAL_TOUCH_MODE;
		data->state[cnt] = IDLE;
		sx9306_wifi_i2c_write(data, SX9306_WIFI_CPS_CTRL6_REG, buf);
		pr_info("[SX9306_WIFI]: %s - %d button released\n", __func__, cnt);
	}

	if (data->flagDataSkip == true)
		return;

#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	if (data->enable_adjdet == 1
	   &&
	   gpio_get_value_cansleep(data->gpio_adjdet) == 1) {
		pr_info("[SX9306_WIFI]: %s : adj detect cable connected," \
			" skip grip_wifi sensor\n", __func__);
		return;
	}
#endif
	switch (cnt) {
	case 0:
#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
		data->grip_wifi_state = state;
#endif
		if (state == ACTIVE) {
			data->iio_state = ACTIVE;
			sx9306_grip_wifi_data_rdy_trig_poll(indio_dev);
        	} else
			data->iio_state = IDLE;
			sx9306_grip_wifi_data_rdy_trig_poll(indio_dev);
        break;
	case 1:
	case 2:
	case 3:
		pr_info("[SX9306_WIFI]: %s - There is no defined event for" \
			" button %d.\n", __func__, cnt);
		break;
	default:
		break;
	}

}

static void sx9306_wifi_display_data_reg(struct sx9306_wifi_p *data)
{
	u8 val, reg;
	int i;

	for (i = 0; i < TOTAL_BOTTON_COUNT; i++) {
		sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, i);
		pr_info("[SX9306_WIFI]: ############# %d button #############\n", i);
		for (reg = SX9306_WIFI_REGUSEMSB;
			reg <= SX9306_WIFI_REGOFFSETLSB; reg++) {
			sx9306_wifi_i2c_read(data, reg, &val);
			pr_info("[SX9306_WIFI]: %s - Register(0x%2x) data(0x%2x)\n",
				__func__, reg, val);
		}
	}
}

static s32 sx9306_wifi_get_init_threshold(struct sx9306_wifi_p *data)
{
	s32 threshold;

	/* Because the STANDARD_CAP_MAIN was 300,000 in the previous patch,
	 * the exception code is added. It will be removed later */
	if (data->calData[0] == 0)
		threshold = STANDARD_CAP_MAIN + data->initTh;
	else if (data->calData[0] > 100000)
		threshold = data->initTh + data->calData[0];
	else
		threshold = 300000 + data->initTh + data->calData[0];

	return threshold;
}
static s32 sx9306_wifi_get_capMain(struct sx9306_wifi_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	u16 offset = 0;
	s32 capMain = 0, useful = 0;

	/* Calculate out the Main Cap information */
	sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, MAIN_SENSOR);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSEMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSELSB, &lsByte);

	useful = (s32)msByte;
	useful = (useful << 8) | ((s32)lsByte);
	if (useful > 32767)
		useful -= 65536;

	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETLSB, &lsByte);

	offset = (u16)msByte;
	offset = (offset << 8) | ((u16)lsByte);

	msByte = (u8)(offset >> 6);
	lsByte = (u8)(offset - (((u16)msByte) << 6));

	capMain = 2 * (((s32)msByte * 3600) + ((s32)lsByte * 225)) +
		(((s32)useful * 50000) / (8 * 65536));

#if 0 /* disable ref_sensor */
	/* Calculate out the Reference Cap information */
	sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, REF_SENSOR);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSEMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSELSB, &lsByte);
#endif
#if 0 /* Calculate out the difference between the two */
	s32 capRef = 0;

	capRef = (s32)msByte;
	capRef = (capRef << 8) | ((s32)lsByte);
	if (capRef > 32767)
		capRef -= 65536;

	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETLSB, &lsByte);

	offset = (u16)msByte;
	offset = (offset << 8) | ((u16)lsByte);
	msByte = (u8)(offset >> 6);
	lsByte = (u8)(offset - (((u16)msByte) << 6));

	capRef = 2 * (((s32)msByte * 3600) + ((s32)lsByte * 225)) +
		(((s32)capRef * 50000) / (8 * 65536));

	capRef = (capRef - TOUCH_CHECK_REF_AMB) *
		TOUCH_CHECK_SLOPE + TOUCH_CHECK_MAIN_AMB;

	capMain = capMain - capRef;
#endif

	pr_info("[SX9306_WIFI]: %s - CapsMain: %ld, useful: %ld, Offset: %u\n",
		__func__, (long int)capMain, (long int)useful, offset);

	return capMain;
}

static void sx9306_wifi_touchCheckWithRefSensor(struct sx9306_wifi_p *data)
{
	s32 capMain, threshold;
	int cnt = 0;
	threshold = sx9306_wifi_get_init_threshold(data);

	capMain = sx9306_wifi_get_capMain(data);

	if (data->state[cnt] == IDLE) {
		if (capMain >= threshold)
			send_event(data, cnt, ACTIVE);
		else
			send_event(data, cnt, IDLE);
	} else {
		if (capMain < threshold)
			send_event(data, cnt, IDLE);
		else
			send_event(data, cnt, ACTIVE);
	}
}

static int sx9306_wifi_save_caldata(struct sx9306_wifi_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SX9306_WIFI]: %s - Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)data->calData,
		sizeof(int) * 3, &cal_filp->f_pos);
	if (ret != (sizeof(int) * 3)) {
		pr_err("[SX9306_WIFI]: %s - Can't write the cal data to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static void sx9306_wifi_open_caldata(struct sx9306_wifi_p *data)
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
			pr_err("[SX9306_WIFI]: %s - Can't open calibration file.\n",
				__func__);
		else {
			pr_info("[SX9306_WIFI]: %s - There is no calibration file\n",
				__func__);
			/* calibration status init */
			memset(data->calData, 0, sizeof(int) * 3);
		}
		set_fs(old_fs);
		return;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)data->calData,
		sizeof(int) * 3, &cal_filp->f_pos);
	if (ret != (sizeof(int) * 3))
		pr_err("[SX9306_WIFI]: %s - Can't read the cal data from file\n",
			__func__);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SX9306_WIFI]: %s - (%d, %d, %d)\n", __func__,
		data->calData[0], data->calData[1], data->calData[2]);
}

static int sx9306_wifi_set_mode(struct sx9306_wifi_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	mutex_lock(&data->mode_mutex);
	if (mode == SX9306_WIFI_MODE_SLEEP) {
		ret = sx9306_wifi_i2c_write(data, SX9306_WIFI_CPS_CTRL0_REG, 0x20);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	} else if (mode == SX9306_WIFI_MODE_NORMAL) {
		ret = sx9306_wifi_i2c_write(data, SX9306_WIFI_CPS_CTRL0_REG,
			0x20 | ENABLE_CSX);
		msleep(20);

		sx9306_wifi_set_offset_calibration(data);
		msleep(400);

		sx9306_wifi_touchCheckWithRefSensor(data);
		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	}

	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9306_wifi_read_irqstate(data);

	pr_info("[SX9306_WIFI]: %s - change the mode : %u\n", __func__, mode);

	mutex_unlock(&data->mode_mutex);
	return ret;
}

static int sx9306_wifi_get_useful(struct sx9306_wifi_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	s16 fullByte = 0;

	sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, MAIN_SENSOR);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSEMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSELSB, &lsByte);
	fullByte = (s16)((msByte << 8) | lsByte);

	pr_info("[SX9306_WIFI]: %s - PROXIUSEFUL = %d\n", __func__, fullByte);

	return (int)fullByte;
}

static int sx9306_wifi_get_offset(struct sx9306_wifi_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	u16 fullByte = 0;

	sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, MAIN_SENSOR);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETMSB, &msByte);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETLSB, &lsByte);
	fullByte = (u16)((msByte << 8) | lsByte);

	pr_info("[SX9306_WIFI]: %s - PROXIOFFSET = %u\n", __func__, fullByte);

	return (int)fullByte;
}

static int sx9306_wifi_do_calibrate(struct sx9306_wifi_p *data, bool do_calib)
{
	int ret = 0;
	s32 capMain;

	if (do_calib == false) {
		pr_info("[SX9306_WIFI]: %s - Erase!\n", __func__);
		goto cal_erase;
	}

	if (atomic_read(&data->enable) == OFF)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_NORMAL);

	data->calData[2] = sx9306_wifi_get_offset(data);
	if ((data->calData[2] >= LIMIT_PROXOFFSET)
		|| (data->calData[2] == 0)) {
		pr_err("[SX9306_WIFI]: %s - offset fail(%d)\n", __func__,
			data->calData[2]);
		goto cal_fail;
	}

	data->calData[1] = sx9306_wifi_get_useful(data);
	if (data->calData[1] >= LIMIT_PROXUSEFUL) {
		pr_err("[SX9306_WIFI]: %s - useful warning(%d)\n", __func__,
			data->calData[1]);
	}

	capMain = sx9306_wifi_get_capMain(data);
	data->calData[0] = capMain;

	if (atomic_read(&data->enable) == OFF)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_SLEEP);

	goto exit;

cal_fail:
	if (atomic_read(&data->enable) == OFF)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_SLEEP);
	ret = -1;
cal_erase:
	memset(data->calData, 0, sizeof(int) * 3);
exit:
	pr_info("[SX9306_WIFI]: %s - (%d, %d, %d)\n", __func__,
		data->calData[0], data->calData[1], data->calData[2]);
	return ret;
}

static void sx9306_wifi_set_enable(struct sx9306_wifi_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	if (enable) {
		if (pre_enable == OFF) {
			data->touchMode = INIT_TOUCH_MODE;
			data->calSuccessed = false;

			sx9306_wifi_open_caldata(data);
			sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_NORMAL);
			atomic_set(&data->enable, ON);
		}
	} else {
		if (pre_enable == ON) {
			sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_SLEEP);
			atomic_set(&data->enable, OFF);
		}
	}
}

static ssize_t sx9306_wifi_get_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	sx9306_wifi_i2c_read(data, SX9306_WIFI_IRQSTAT_REG, &val);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sx9306_wifi_set_offset_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	if (val)
		sx9306_wifi_set_offset_calibration(data);

	return count;
}

static ssize_t sx9306_wifi_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0, val = 0;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d,%d", &regist, &val) != 2) {
		pr_err("[SX9306_WIFI]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9306_wifi_i2c_write(data, (unsigned char)regist, (unsigned char)val);
	pr_info("[SX9306_WIFI]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9306_wifi_register_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0;
	unsigned char val = 0;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &regist) != 1) {
		pr_err("[SX9306_WIFI]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9306_wifi_i2c_read(data, (unsigned char)regist, &val);
	pr_info("[SX9306_WIFI]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9306_wifi_read_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	sx9306_wifi_display_data_reg(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9306_wifi_sw_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_SLEEP);

	ret = sx9306_wifi_i2c_write(data, SX9306_WIFI_SOFTRESET_REG, SX9306_WIFI_SOFTRESET);
	msleep(300);

	sx9306_wifi_initialize_chip(data);

	if (atomic_read(&data->enable) == ON)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_NORMAL);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t sx9306_wifi_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9306_wifi_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx9306_wifi_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->touchMode);
}

static ssize_t sx9306_wifi_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 msb, lsb;
	s16 useful;
	u16 offset;
	s32 capMain;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == OFF)
		pr_err("[SX9306_WIFI]: %s - SX9306_WIFI was not enabled\n", __func__);

	capMain = sx9306_wifi_get_capMain(data);

	sx9306_wifi_i2c_write(data, SX9306_WIFI_REGSENSORSELECT, MAIN_SENSOR);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSEMSB, &msb);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGUSELSB, &lsb);
	useful = (s16)((msb << 8) | lsb);

	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETMSB, &msb);
	sx9306_wifi_i2c_read(data, SX9306_WIFI_REGOFFSETLSB, &lsb);
	offset = (u16)((msb << 8) | lsb);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u\n", capMain, useful, offset);
}

static ssize_t sx9306_wifi_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	/* It's for init touch */
	return snprintf(buf, PAGE_SIZE, "%d\n",
			sx9306_wifi_get_init_threshold(data));
}

static ssize_t sx9306_wifi_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	/* It's for init touch */
	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9306_WIFI]: %s - init threshold %lu\n", __func__, val);
	data->initTh = (int)val;

	return count;
}

static ssize_t sx9306_wifi_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	return snprintf(buf, PAGE_SIZE, "%d\n", data->touchTh);
}

static ssize_t sx9306_wifi_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9306_WIFI]: %s - normal threshold %lu\n", __func__, val);
	data->touchTh = (u8)val;

	return count;
}

static ssize_t sx9306_wifi_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->flagDataSkip);
}

static ssize_t sx9306_wifi_onoff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (val == 0)
		data->flagDataSkip = true;
	else
		data->flagDataSkip = false;

	pr_info("[SX9306_WIFI]: %s -%u\n", __func__, val);
	return count;
}

static ssize_t sx9306_wifi_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if ((data->calSuccessed == false) && (data->calData[0] == 0))
		ret = CAL_RET_NONE;
	else if ((data->calSuccessed == false) && (data->calData[0] != 0))
		ret = CAL_RET_EXIST;
	else if ((data->calSuccessed == true) && (data->calData[0] != 0))
		ret = CAL_RET_SUCCESS;
	else
		ret = CAL_RET_ERROR;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", ret,
			data->calData[1], data->calData[2]);
}

static ssize_t sx9306_wifi_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool do_calib;
	int ret;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1"))
		do_calib = true;
	else if (sysfs_streq(buf, "0"))
		do_calib = false;
	else {
		pr_info("[SX9306_WIFI]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ret = sx9306_wifi_do_calibrate(data, do_calib);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - sx9306_wifi_do_calibrate fail(%d)\n",
			__func__, ret);
		goto exit;
	}

	ret = sx9306_wifi_save_caldata(data);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - sx9306_wifi_save_caldata fail(%d)\n",
			__func__, ret);
		memset(data->calData, 0, sizeof(int) * 3);
		goto exit;
	}

	pr_info("[SX9306_WIFI]: %s - %u success!\n", __func__, do_calib);

exit:

	if ((data->calData[0] != 0) && (ret >= 0))
		data->calSuccessed = true;
	else
		data->calSuccessed = false;

	return count;
}

static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_get_offset_calibration_show,
		sx9306_wifi_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IWUSR | S_IWGRP,
		NULL, sx9306_wifi_register_write_store);
static DEVICE_ATTR(register_read, S_IWUSR | S_IWGRP,
		NULL, sx9306_wifi_register_read_store);
static DEVICE_ATTR(readback, S_IRUGO, sx9306_wifi_read_data_show, NULL);
static DEVICE_ATTR(reset, S_IRUGO, sx9306_wifi_sw_reset_show, NULL);

static DEVICE_ATTR(name, S_IRUGO, sx9306_wifi_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx9306_wifi_vendor_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx9306_wifi_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx9306_wifi_raw_data_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_calibration_show, sx9306_wifi_calibration_store);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_onoff_show, sx9306_wifi_onoff_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_threshold_show, sx9306_wifi_threshold_store);
static DEVICE_ATTR(normal_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_normal_threshold_show, sx9306_wifi_normal_threshold_store);

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
	NULL,
};

/*****************************************************************************/
static ssize_t sx9306_wifi_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SX9306_WIFI]: %s - new_value = %u\n", __func__, enable);
	if ((enable == 0) || (enable == 1))
		sx9306_wifi_set_enable(data, (int)enable);

	return size;
}

static ssize_t sx9306_wifi_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t sx9306_wifi_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9306_WIFI]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (enable == 1) {
		mutex_lock(&data->mode_mutex);
		input_report_rel(data->input, REL_MAX, 1);
		input_sync(data->input);
		mutex_unlock(&data->mode_mutex);
	}

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9306_wifi_enable_show, sx9306_wifi_enable_store);
static DEVICE_ATTR(flush, S_IWUSR | S_IWGRP,
		NULL, sx9306_wifi_flush_store);

static struct attribute *sx9306_wifi_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_flush.attr,
	NULL
};

static struct attribute_group sx9306_wifi_attribute_group = {
	.attrs = sx9306_wifi_attributes
};

static void sx9306_wifi_touch_process(struct sx9306_wifi_p *data)
{
	u8 status = 0;
	int cnt;
	s32 capMain, threshold;

	threshold = sx9306_wifi_get_init_threshold(data);
	capMain = sx9306_wifi_get_capMain(data);

	pr_info("[SX9306_WIFI]: %s\n", __func__);

	sx9306_wifi_i2c_read(data, SX9306_WIFI_TCHCMPSTAT_REG, &status);
	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++) {
		if (data->state[cnt] == IDLE) {
			if (status & (CSX_STATUS_REG << cnt)) {
#ifdef DEFENCE_CODE_FOR_DEVICE_DAMAGE
				if ((data->calData[0] - 5000) > capMain) {
					sx9306_wifi_set_offset_calibration(data);
					msleep(400);
					sx9306_wifi_touchCheckWithRefSensor(data);
					pr_err("[SX9306_WIFI]: %s - Defence code for"
						" device damage\n", __func__);
					return;
				}
#endif
				send_event(data, cnt, ACTIVE);
			} else {
				pr_info("[SX9306_WIFI]: %s - %d already released.\n",
					__func__, cnt);
			}
		} else { /* User released button */
			if (!(status & (CSX_STATUS_REG << cnt))) {
				if ((data->touchMode == INIT_TOUCH_MODE)
					&& (capMain >= threshold))
					pr_info("[SX9306_WIFI]: %s - IDLE SKIP\n",
						__func__);
				else
					send_event(data, cnt, IDLE);

			} else {
				pr_info("[SX9306_WIFI]: %s - %d still touched\n",
					__func__, cnt);
			}
		}
	}
}

static void sx9306_wifi_process_interrupt(struct sx9306_wifi_p *data)
{
	u8 status = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	status = sx9306_wifi_read_irqstate(data);

	if (status & IRQ_PROCESS_CONDITION)
		sx9306_wifi_touch_process(data);
}

static void sx9306_wifi_init_work_func(struct work_struct *work)
{
	struct sx9306_wifi_p *data = container_of((struct delayed_work *)work,
		struct sx9306_wifi_p, init_work);

	sx9306_wifi_initialize_chip(data);

	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9306_wifi_read_irqstate(data);
}

static void sx9306_wifi_irq_work_func(struct work_struct *work)
{
	struct sx9306_wifi_p *data = container_of((struct delayed_work *)work,
		struct sx9306_wifi_p, irq_work);

	sx9306_wifi_process_interrupt(data);
}

#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
static irqreturn_t sx9306_wifi_adjdet_interrupt_thread(int irq, void *pdata)
{
	struct iio_dev *indio_dev = pdata;
	struct sx9306_wifi_p *data = iio_priv(indio_dev);

	if (gpio_get_value_cansleep(data->gpio_adjdet) == 0)
		pr_info("[SX9306_WIFI]: %s : adj detect cable disconnect!" \
				" grip_wifi sensor enable\n", __func__);
	else {
		pr_info("[SX9306_WIFI]: %s : adj detect cable connect!" \
				" grip_wifi sensordisable\n", __func__);
		if (data->grip_wifi_state == ACTIVE) {
			pr_info("[SX9306_WIFI]: %s : Send FAR(IDLE)\n", __func__);
            iio_push_event(indio_dev,
                            IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY,
                                                 0,
                                                 IIO_EV_TYPE_THRESH,
                                                 IIO_EV_DIR_RISING),
                           iio_get_time_ns());
			data->grip_wifi_state = IDLE;
		}
	}

	return IRQ_HANDLED;
}
#endif

static irqreturn_t sx9306_wifi_interrupt_thread(int irq, void *pdata)
{
	struct sx9306_wifi_p *data = pdata;

	if (sx9306_wifi_get_nirq_state(data) == 1) {
		pr_err("[SX9306_WIFI]: %s - nirq read high\n", __func__);
	} else {
		wake_lock_timeout(&data->grip_wifi_wake_lock, 3 * HZ);
		schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));
	}

	return IRQ_HANDLED;
}

static int sx9306_wifi_setup_pin(struct sx9306_wifi_p *data)
{
	int ret;

	ret = gpio_request(data->gpioNirq, "SX9306_WIFI_nIRQ");
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - gpio %d request failed (%d)\n",
			__func__, data->gpioNirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpioNirq);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->gpioNirq, ret);
		gpio_free(data->gpioNirq);
		return ret;
	}
#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	if (data->enable_adjdet == 1) {
		ret = gpio_request(data->gpio_adjdet, "SX9306_WIFI_ADJDETIRQ");
		if (ret < 0) {
			pr_err("[SX9306_WIFI]: %s - gpio %d request failed (%d)\n",
				__func__, data->gpio_adjdet, ret);
			return ret;
		}

		ret = gpio_direction_input(data->gpio_adjdet);

		if (ret < 0) {
			pr_err("[SX9306_WIFI]: %s - failed to set" \
				" gpio %d as input (%d)\n",
				__func__, data->gpio_adjdet, ret);
			gpio_free(data->gpio_adjdet);
			return ret;
		}
	}
#endif
	return 0;
}

static void sx9306_wifi_initialize_variable(struct sx9306_wifi_p *data)
{
	int cnt;

	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++)
		data->state[cnt] = IDLE;

	data->touchMode = INIT_TOUCH_MODE;
	data->flagDataSkip = false;
	data->calSuccessed = false;
	memset(data->calData, 0, sizeof(int) * 3);
	atomic_set(&data->enable, OFF);

	data->initTh = (int)CONFIG_SENSORS_SX9306_WIFI_INIT_TOUCH_THRESHOLD;
	pr_info("[SX9306_WIFI]: %s - Init Touch Threshold : %d\n",
		__func__, data->initTh);

	data->touchTh = (u8)CONFIG_SENSORS_SX9306_WIFI_NORMAL_TOUCH_THRESHOLD;
	pr_info("[SX9306_WIFI]: %s - Normal Touch Threshold : %u\n",
		__func__, data->touchTh);
}

static int sx9306_wifi_parse_dt(struct sx9306_wifi_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->gpioNirq = of_get_named_gpio_flags(dNode,
		"sx9306_wifi-i2c,nirq-gpio", 0, &flags);
	if (data->gpioNirq < 0) {
		pr_err("[SX9306_WIFI]: %s - get gpioNirq error\n", __func__);
		return -ENODEV;
	}

#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	data->gpio_adjdet = of_get_named_gpio_flags(dNode,
		"sx9306_wifi-i2c,adjdet-gpio", 0, &flags);
	if (data->gpio_adjdet < 0) {
		pr_err("[SX9306_WIFI]: %s - get gpio_adjdet error\n", __func__);
		data->enable_adjdet = 0;
	} else {
		pr_info("[SX9306_WIFI]: %s - get gpio_adjdet success\n", __func__);
		data->enable_adjdet = 1;
	}
#endif
	return 0;
}

static int sx9306_wifi_input_init(struct sx9306_wifi_p *data)
{
	int ret = 0;
	struct input_dev *dev = NULL;

	/* Create the input device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_capability(dev, EV_REL, REL_MAX);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx9306_wifi_attribute_group);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}


#if defined(CONFIG_SENSORS_SX9306_WIFI_REGULATOR_ONOFF)
int sx9306_wifi_regulator_on(struct sx9306_wifi_p *data, bool onoff)
{
	int ret = -1;
	if (!data->vdd) {
		data->vdd = devm_regulator_get(&data->client->dev, "vdd");
		if (!data->vdd) {
			pr_err("%s: regulator pointer null vdd, rc=%d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_set_voltage(data->vdd, 2850000, 2850000);
		if (ret) {
			pr_err("%s: set voltage failed on vdd, rc=%d\n",
				__func__, ret);
			return ret;
		}
	}


	if (!data->ldo) {
		data->ldo = devm_regulator_get(&data->client->dev, "vdd_ldo");
		if (!data->ldo) {
			pr_err("%s: devm_regulator_get for vdd_ldo failed\n",
				__func__);
			return 0;
		}
	}

	if (onoff) {
		ret = regulator_enable(data->vdd);
		if (ret) {
			pr_err("%s: Failed to enable regulator vdd.\n",
				__func__);
			return ret;
		}

		ret = regulator_enable(data->ldo);
		if (ret) {
			pr_err("%s: Failed to enable regulator ldo.\n",
				__func__);
			return ret;
		}
	} else {
		ret = regulator_disable(data->vdd);
		if (ret) {
			pr_err("%s: Failed to disable regulator vdd.\n",
				__func__);
			return ret;
		}

		ret = regulator_disable(data->ldo);
		if (ret) {
			pr_err("%s: Failed to disable regulator ldo.\n",
				__func__);
			return ret;
		}

	}
	/*Delay added for wakeup of chip, before i2c-transactions */
	msleep(30);
	return 0;
}
#endif

irqreturn_t sx9306_wifi_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = sx9306_wifi_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}


static irqreturn_t sx9306_wifi_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	int len = 0;
	int32_t *data;

	data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		/* TODO : data update */
		if (st->iio_state) {
			*data = 0;
		}
		else {
			*data = 1;
		}
	}
	len = 4;

	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)data + ALIGN(len, sizeof(s64))) = pf->timestamp;
	iio_push_to_buffers(indio_dev, (u8 *)data);
	kfree(data);
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int sx9306_wifi_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	if (!atomic_cmpxchg(&st->pseudo_irq_enable, 0, 1)) {
		mutex_lock(&st->lock);
		sx9306_wifi_set_enable(st, 1);
		mutex_unlock(&st->lock);
	}
	return 0;
}

static int sx9306_wifi_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	if (atomic_cmpxchg(&st->pseudo_irq_enable, 1, 0)) {
		mutex_lock(&st->lock);
		sx9306_wifi_set_enable(st, 0);
		mutex_unlock(&st->lock);
	}
	return 0;
}

static int sx9306_wifi_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		sx9306_wifi_pseudo_irq_enable(indio_dev);
	else
		sx9306_wifi_pseudo_irq_disable(indio_dev);
	return 0;
}

static int sx9306_wifi_data_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	sx9306_wifi_set_pseudo_irq(indio_dev, state);
	return 0;
}

static const struct iio_trigger_ops sx9306_wifi_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &sx9306_wifi_data_rdy_trigger_set_state,
};

static int sx9306_wifi_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	indio_dev->pollfunc = iio_alloc_pollfunc(&sx9306_wifi_iio_pollfunc_store_boottime,
			&sx9306_wifi_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	st->trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!st->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	st->trig->dev.parent = &st->client->dev;
	st->trig->ops = &sx9306_wifi_trigger_ops;
	iio_trigger_set_drvdata(st->trig, indio_dev);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error_free_trig;
	return 0;

error_free_trig:
	iio_trigger_free(st->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void sx9306_wifi_remove_trigger(struct iio_dev *indio_dev)
{
	struct sx9306_wifi_p *st = iio_priv(indio_dev);
	iio_trigger_unregister(st->trig);
	iio_trigger_free(st->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_buffer_setup_ops sx9306_wifi_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int sx9306_wifi_probe_buffer(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &sx9306_wifi_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(indio_dev, indio_dev->buffer, SX9306_WIFI_SCAN_GRIP_WIFI_CH);

	return 0;

error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static void sx9306_wifi_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
};

static int sx9306_wifi_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val,
		int *val2,
		long mask) {
	struct sx9306_wifi_p  *st = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_GRIP_WIFI)
		return -EINVAL;

	mutex_lock(&st->lock);

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

	mutex_unlock(&st->lock);

	return ret;
}

static const struct iio_info sx9306_wifi_info = {
	.read_raw = &sx9306_wifi_read_raw,
	.driver_module = THIS_MODULE,
};

static int sx9306_wifi_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct iio_dev *indio_dev;
	struct sx9306_wifi_p *data = NULL;
	struct sx9306_wifi_p *data_input = NULL;

	pr_info("[SX9306_WIFI]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SX9306_WIFI]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	/* create memory for main struct */
	data_input = kzalloc(sizeof(struct sx9306_wifi_p), GFP_KERNEL);
	if (data_input == NULL) {
		pr_err("[SX9306_WIFI]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	/* iio device register */
	/* create memory for main struct */
	indio_dev = iio_device_alloc(sizeof(*data));
	if (indio_dev == NULL) {
		pr_err("[SX9306_WIFI]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		return -ENOMEM;
	}
	i2c_set_clientdata(client, indio_dev);

	indio_dev->name = SX9306_GRIP_WIFI_SENSOR_NAME;
	indio_dev->dev.parent = &client->dev;
	indio_dev->info = &sx9306_wifi_info;
	indio_dev->channels = sx9306_wifi_channels;
	indio_dev->num_channels = ARRAY_SIZE(sx9306_wifi_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;

	data = iio_priv(indio_dev);
	data->client = client;
	data->factory_device = &client->dev;

	spin_lock_init(&data->spin_lock);
	wake_lock_init(&data->grip_wifi_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wifi_wake_lock");
	mutex_init(&data->mode_mutex);
	mutex_init(&data->lock);

	ret = sx9306_wifi_parse_dt(data, &client->dev);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - of_node error\n", __func__);
		ret = -ENODEV;
		goto exit_of_node;
	}

	ret = sx9306_wifi_setup_pin(data);
	if (ret) {
		pr_err("[SX9306_WIFI]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

#if defined(CONFIG_SENSORS_SX9306_WIFI_REGULATOR_ONOFF)
	sx9306_wifi_regulator_on(data, 1);
#endif

	/* read chip id */
	ret = sx9306_wifi_i2c_write(data, SX9306_WIFI_SOFTRESET_REG, SX9306_WIFI_SOFTRESET);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - chip reset failed %d\n", __func__, ret);
		goto exit_chip_reset;
	}

	sx9306_wifi_initialize_variable(data);

	INIT_DELAYED_WORK(&data->init_work, sx9306_wifi_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9306_wifi_irq_work_func);

	/* initailize interrupt reporting */
	data->irq = gpio_to_irq(data->gpioNirq);
	ret = request_threaded_irq(data->irq, NULL, sx9306_wifi_interrupt_thread,
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT , "sx9306_wifi_irq", data);
	if (ret < 0) {
		pr_err("[SX9306_WIFI]: %s - failed to set request_threaded_irq %d" \
			" as returning (%d)\n", __func__, data->irq, ret);
		goto exit_request_threaded_irq;
	}

#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	if (data->enable_adjdet == 1) {
		data->irq_adjdet = gpio_to_irq(data->gpio_adjdet);
		/* initailize interrupt reporting */
		ret = request_threaded_irq(data->irq_adjdet, NULL,
				sx9306_wifi_adjdet_interrupt_thread,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"sx9306_wifi_adjdet_irq", indio_dev);
		if (ret < 0) {
			pr_err("[SX9306_WIFI]: %s - failed to set" \
				" request_threaded_adjdet_irq %d" \
				" as returning (%d)\n",
				__func__, data->irq_adjdet, ret);
			goto exit_request_threaded_adjdet_irq;
		}
		data->grip_wifi_state = IDLE;
	}
#endif
	disable_irq(data->irq);

	ret = sx9306_wifi_probe_buffer(indio_dev);
	if (ret)
		goto exit_iio_buffer_probe_failed;
	ret = sx9306_wifi_probe_trigger(indio_dev);
	if (ret)
		goto exit_iio_trigger_probe_failed;
	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto exit_iio_register_failed;

	ret = sensors_register(data->factory_device,
		data, sensor_attrs, MODULE_NAME);
	if (ret) {
		pr_err("[SENSOR] %s - cound not register grip_wifi_sensor(%d).\n",
			__func__, ret);
		goto grip_wifi_sensor_register_failed;
	}

	ret = sx9306_wifi_input_init(data_input);
	if (ret < 0)
		goto exit_input_init;

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	pr_info("[SX9306_WIFI]: %s - Probe done!\n", __func__);

	return 0;

exit_input_init:
grip_wifi_sensor_register_failed:
exit_iio_register_failed:
	sx9306_wifi_remove_trigger(indio_dev);
exit_iio_trigger_probe_failed:
	sx9306_wifi_remove_buffer(indio_dev);
exit_iio_buffer_probe_failed:
	sensors_unregister(data->factory_device, sensor_attrs);
#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
exit_request_threaded_adjdet_irq:
	free_irq(data->irq, indio_dev);
#endif
exit_request_threaded_irq:
exit_chip_reset:
	gpio_free(data->gpioNirq);
exit_setup_pin:
exit_of_node:
	wake_lock_destroy(&data->grip_wifi_wake_lock);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->lock);
	kfree(data_input);
exit_kzalloc:
exit:
	pr_err("[SX9306_WIFI]: %s - Probe fail!\n", __func__);
	return ret;
}

static int sx9306_wifi_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct sx9306_wifi_p *data = iio_priv(indio_dev);


	if (atomic_read(&data->enable) == ON)
		sx9306_wifi_set_mode(data, SX9306_WIFI_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	free_irq(data->irq, data);
#ifdef CONFIG_SENSORS_GRIP_WIFI_ADJDET
	if (data->enable_adjdet == 1)
		free_irq(data->irq_adjdet, data);
#endif
	gpio_free(data->gpioNirq);

	wake_lock_destroy(&data->grip_wifi_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
//	sensors_remove_symlink(indio_dev);
//	sysfs_remove_group(&indio_dev->dev.kobj, &sx9306_wifi_attribute_group);
	mutex_destroy(&data->mode_mutex);

	iio_device_unregister(indio_dev);

	return 0;
}

static int sx9306_wifi_suspend(struct device *dev)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		pr_info("[SX9306_WIFI]: %s\n", __func__);

	return 0;
}

static int sx9306_wifi_resume(struct device *dev)
{
	struct sx9306_wifi_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == ON)
		pr_info("[SX9306_WIFI]: %s\n", __func__);

	return 0;
}

static struct of_device_id sx9306_wifi_match_table[] = {
	{ .compatible = "sx9306_wifi-i2c",},
	{},
};

static const struct i2c_device_id sx9306_wifi_id[] = {
	{ "sx9306w_match_table", 0 },
	{ }
};

static const struct dev_pm_ops sx9306_wifi_pm_ops = {
	.suspend = sx9306_wifi_suspend,
	.resume = sx9306_wifi_resume,
};

static struct i2c_driver sx9306_wifi_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sx9306_wifi_match_table,
		.pm = &sx9306_wifi_pm_ops
	},
	.probe		= sx9306_wifi_probe,
	.remove		= sx9306_wifi_remove,
	.id_table	= sx9306_wifi_id,
};

static int __init sx9306_wifi_init(void)
{
	return i2c_add_driver(&sx9306_wifi_driver);
}

static void __exit sx9306_wifi_exit(void)
{
	i2c_del_driver(&sx9306_wifi_driver);
}

module_init(sx9306_wifi_init);
module_exit(sx9306_wifi_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9306_WIFI Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
