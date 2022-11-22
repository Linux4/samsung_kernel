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
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include "sx9310_reg.h"

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9310"
#define MODULE_NAME              "grip_sensor"
#define CALIBRATION_FILE_PATH    "/efs/FactoryApp/grip_cal_data"

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

#define CSX_STATUS_REG           SX9310_TCHCMPSTAT_TCHSTAT1_FLAG

#define DEFAULT_THRESHOLD        0x8E

#define LIMIT_PROXOFFSET         13500 /* 99.9pF */// 2550 /* 30pF */
#define LIMIT_PROXUSEFUL_MIN     -10000
#define LIMIT_PROXUSEFUL_MAX     10000
#define STANDARD_CAP_MAIN        1000000

#define	TOUCH_CHECK_REF_AMB      0 // 44523
#define	TOUCH_CHECK_SLOPE        0 // 50
#define	TOUCH_CHECK_MAIN_AMB     0 // 151282

#define DEFENCE_CODE_FOR_DEVICE_DAMAGE
/* CS0, CS1, CS2, CS3 */
#define TOTAL_BOTTON_COUNT       1
#define ENABLE_CSX               (1 << MAIN_SENSOR)// | (1 << REF_SENSOR))

#define IRQ_PROCESS_CONDITION   (SX9310_IRQSTAT_TOUCH_FLAG	\
				| SX9310_IRQSTAT_RELEASE_FLAG	\
				| SX9310_IRQSTAT_COMPDONE_FLAG)

struct sx9310_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct wake_lock grip_wake_lock;
	struct mutex mode_mutex;

	bool calSuccessed;
	bool flagDataSkip;
	u8 touchTh;
	int initTh;
	int calData[3];
	int touchMode;

	int irq;
	int gpioNirq;
	int state;

	atomic_t enable;
};

static int sx9310_get_nirq_state(struct sx9310_p *data)
{
	return gpio_get_value_cansleep(data->gpioNirq);
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
		pr_err("[SX9310]: %s - i2c write error %d\n", __func__, ret);

	return 0;
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
		pr_err("[SX9310]: %s - i2c read error %d\n", __func__, ret);

	return ret;
}

static u8 sx9310_read_irqstate(struct sx9310_p *data)
{
	u8 val = 0;

	if (sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val) >= 0)
		return (val & 0x00FF);

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

	while((sx9310_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9310_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		pr_err("[SX9310]: %s - s/w reset fail(%d)\n", __func__, cnt);

	sx9310_initialize_register(data);
}

static int sx9310_set_offset_calibration(struct sx9310_p *data)
{
	int ret = 0;

	ret = sx9310_i2c_write(data, SX9310_IRQSTAT_REG, 0xFF);

	return ret;
}

static void send_event(struct sx9310_p *data, u8 state)
{
	u8 buf = data->touchTh;

	if (state == ACTIVE) {
		data->state = ACTIVE;
#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, buf);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, buf);
#endif
		pr_info("[SX9310]: %s - button touched\n", __func__);
	} else {
		data->touchMode = NORMAL_TOUCH_MODE;
		data->state = IDLE;
#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, buf);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, buf);
#endif
		pr_info("[SX9310]: %s - button released\n", __func__);
	}

	if (data->flagDataSkip == true)
		return;

			if (state == ACTIVE)
				input_report_rel(data->input, REL_MISC, 1);
			else
				input_report_rel(data->input, REL_MISC, 2);

	input_sync(data->input);
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

	/* Because the STANDARD_CAP_MAIN was 300,000 in the previous patch,
	 * the exception code is added. It will be removed later */
	if (data->calData[0] == 0)
		threshold = STANDARD_CAP_MAIN + data->initTh;
	else
		threshold = data->initTh + data->calData[0];

	return threshold;
}

static s32 sx9310_get_capMain(struct sx9310_p *data)
{
	u8 msByte = 0;
	u8 isByte = 0;
	u8 lsByte = 0;
	u16 offset = 0;
	s32 capMain = 0, useful = 0;

	/* Calculate out the Main Cap information */
	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read(data, SX9310_REGUSEMSB, &msByte);
	sx9310_i2c_read(data, SX9310_REGUSELSB, &lsByte);

	useful = (s32)msByte;
	useful = (useful << 8) | ((s32)lsByte);
	if (useful > 32767)
		useful -= 65536;

	sx9310_i2c_read(data, SX9310_REGOFFSETMSB, &msByte);
	sx9310_i2c_read(data, SX9310_REGOFFSETLSB, &lsByte);

	offset = (u16)msByte;
	offset = (offset << 8) | ((u16)lsByte);

	msByte = (u8)(offset >> 13);
	isByte = (u8)((offset&0x1fc0)>>6);
	lsByte = (u8)(offset&0x3f);

	capMain = (((s32)msByte * 234000) + ((s32)isByte*9000) + ((s32)lsByte * 450)) +
		(((s32)useful * 50000) / (8 * 65536) );

	pr_info("[SX9310]: %s - CapsMain: %ld, useful: %ld, Offset: %u\n",
		__func__, (long int)capMain, (long int)useful, offset);

	return capMain;
}

static void sx9310_touchCheckWithRefSensor(struct sx9310_p *data)
{
	s32 capMain, threshold;

	threshold = sx9310_get_init_threshold(data);
	capMain = sx9310_get_capMain(data);

	if (capMain >= threshold)
		send_event(data, ACTIVE);
			else
		send_event(data, IDLE);
}

static int sx9310_save_caldata(struct sx9310_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("[SX9310]: %s - Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)data->calData,
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

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		if (ret != -ENOENT)
			pr_err("[SX9310]: %s - Can't open calibration file.\n",
				__func__);
		else {
			pr_info("[SX9310]: %s - There is no calibration file\n",
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
		pr_err("[SX9310]: %s - Can't read the cal data from file\n",
			__func__);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SX9310]: %s - (%d, %d, %d)\n", __func__,
		data->calData[0], data->calData[1], data->calData[2]);
}

static int sx9310_set_mode(struct sx9310_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	mutex_lock(&data->mode_mutex);
	if (mode == SX9310_MODE_SLEEP) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG, 0x50);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	} else if (mode == SX9310_MODE_NORMAL) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			0x50 | ENABLE_CSX);
		msleep(20);

		sx9310_set_offset_calibration(data);
		msleep(400);

		sx9310_touchCheckWithRefSensor(data);
		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	}

	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9310_read_irqstate(data);

	pr_info("[SX9310]: %s - change the mode : %u\n", __func__, mode);

	mutex_unlock(&data->mode_mutex);
	return ret;
}

static int sx9310_get_useful(struct sx9310_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	s16 fullByte = 0;

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read(data, SX9310_REGUSEMSB, &msByte);
	sx9310_i2c_read(data, SX9310_REGUSELSB, &lsByte);
	fullByte = (s16)((msByte << 8) | lsByte);

	pr_info("[SX9310]: %s - PROXIUSEFUL = %d\n", __func__, fullByte);

	return (int)fullByte;
}

static int sx9310_get_offset(struct sx9310_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	u16 fullByte = 0;

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read(data, SX9310_REGOFFSETMSB, &msByte);
	sx9310_i2c_read(data, SX9310_REGOFFSETLSB, &lsByte);
	fullByte = (u16)((msByte << 8) | lsByte);

	pr_info("[SX9310]: %s - PROXIOFFSET = %u\n", __func__, fullByte);

	return (int)fullByte;
}

static int sx9310_do_calibrate(struct sx9310_p *data, bool do_calib)
{
	int ret = 0;
	s32 capMain, useful_sum = 0;
	s16 offset;
	u8 msByte = 0;
	u8 isByte = 0;
	u8 lsByte = 0;
	int i = 0;

	memset(data->calData, 0, sizeof(int) * 3);
	if (do_calib == false) {
		pr_info("[SX9310]: %s - Erase!\n", __func__);
		goto cal_erase;
	}

	if (atomic_read(&data->enable) == OFF)
		sx9310_set_mode(data, SX9310_MODE_NORMAL);

	data->calData[2] = sx9310_get_offset(data);
	if ((data->calData[2] >= LIMIT_PROXOFFSET)
		|| (data->calData[2] == 0)) {
		pr_err("[SX9310]: %s - offset fail(%d)\n", __func__,
			data->calData[2]);
		goto cal_fail;
	}

	data->calData[1] = sx9310_get_useful(data);

	for (i = 0; i < 8; i++) {
		useful_sum += sx9310_get_useful(data);
		offset = sx9310_get_offset(data);
		if (offset != data->calData[2]) {
			pr_err("[SX9310]: %s - offset fail(%d)-(%d)\n",
				__func__, data->calData[2], offset);
			goto cal_fail;
		}
		mdelay(90);
	}

	data->calData[1] = useful_sum >> 3;

	if (data->calData[1] <= LIMIT_PROXUSEFUL_MIN ||
		data->calData[1] >= LIMIT_PROXUSEFUL_MAX) {
		pr_err("[SX9310]: %s - useful fail(%d)\n", __func__,
			data->calData[1]);
		goto cal_fail;
	}

	msByte = (u8)(offset >> 13);
	isByte = (u8)((offset&0x1fc0)>>6);
	lsByte = (u8)(offset&0x3f);

	capMain = (((s32)msByte * 234000) + ((s32)isByte*9000)\
		+ ((s32)lsByte * 450)) + (((s32)data->calData[1] * 50000) / (8 * 65536));

	data->calData[0] = capMain;

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
		data->calData[0], data->calData[1], data->calData[2]);
	return ret;
}

static void sx9310_set_enable(struct sx9310_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	if (enable) {
		if (pre_enable == OFF) {
			data->touchMode = INIT_TOUCH_MODE;
			data->calSuccessed = false;

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

	if (strict_strtoul(buf, 10, &val)) {
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

	if (sscanf(buf, "%d,%d", &regist, &val) != 2) {
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

	if (sscanf(buf, "%d", &regist) != 1) {
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

	return snprintf(buf, PAGE_SIZE, "%d\n", data->touchMode);
}

static ssize_t sx9310_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 msb, lsb;
	s16 proxdiff;
	s16 useful;
	u16 offset;
	s32 capMain;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == OFF)
		pr_err("[SX9310]: %s - SX9310 was not enabled\n", __func__);

	capMain = sx9310_get_capMain(data);

	useful = sx9310_get_useful(data);

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read(data, SX9310_REGDIFFMSB, &msb);
	sx9310_i2c_read(data, SX9310_REGDIFFLSB, &lsb);

	proxdiff = (s16)((msb << 8) | lsb);

	if(proxdiff > 4095)
		proxdiff -= 8192;

	sx9310_i2c_read(data, SX9310_REGOFFSETMSB, &msb);
	sx9310_i2c_read(data, SX9310_REGOFFSETLSB, &lsb);
	offset = (u16)((msb << 8) | lsb);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u,%d\n", capMain, useful, offset, (int)proxdiff);
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

	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9310]: %s - touch threshold %lu\n", __func__, val);
	data->touchTh = data->initTh = (u8)val;

	return count;
}

static ssize_t sx9310_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[32] = {2, 4, 6, 8, 12, 16, 20, 24, 28, 32,\
				40, 48, 56, 64, 72, 80, 88, 96, 112, 128,\
				144, 160, 192, 224, 256, 320, 384, 512, 640, 768,\
				1024, 1536};

	thresh_temp = data->touchTh >> 3 & 0x1f;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = setup_reg[11].val>>4 & 0x3;

	switch (hysteresis) {
		case 0x01:/* 6% */
			hysteresis = thresh_temp >> 4;
			break;
		case 0x02:/* 12% */
			hysteresis = thresh_temp >> 3;
			break;
		case 0x03:/* 25% */
			hysteresis = thresh_temp >> 2;
			break;
		default:
			/* None */
			break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis, thresh_temp - hysteresis);
}

static ssize_t sx9310_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9310]: %s - normal threshold %lu\n", __func__, val);
	data->touchTh = (u8)val;

	return count;
}

static ssize_t sx9310_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->flagDataSkip);
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
		data->flagDataSkip = true;
	else
		data->flagDataSkip = false;

	pr_info("[SX9310]: %s -%u\n", __func__, val);
	return count;
}

static ssize_t sx9310_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

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
		pr_info("[SX9310]: %s - invalid value %d\n", __func__, *buf);
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
		memset(data->calData, 0, sizeof(int) * 3);
		goto exit;
	}

	pr_info("[SX9310]: %s - %u success!\n", __func__, do_calib);

exit:

	if ((data->calData[0] != 0) && (ret >= 0))
		data->calSuccessed = true;
	else
		data->calSuccessed = false;

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
static ssize_t sx9310_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SX9310]: %s - new_value = %u\n", __func__, enable);
	if ((enable == 0) || (enable == 1))
		sx9310_set_enable(data, (int)enable);

	return size;
}

static ssize_t sx9310_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t sx9310_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
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
		sx9310_enable_show, sx9310_enable_store);
static DEVICE_ATTR(flush, S_IWUSR | S_IWGRP,
		NULL, sx9310_flush_store);

static struct attribute *sx9310_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_flush.attr,
	NULL
};

static struct attribute_group sx9310_attribute_group = {
	.attrs = sx9310_attributes
};

static void sx9310_touch_process(struct sx9310_p *data)
{
	u8 status = 0;
	int cnt;
	s32 capMain, threshold;
	//s32 capMain, initThd = INIT_THRESHOLD + data->calData[0];
	threshold = sx9310_get_init_threshold(data);
	capMain = sx9310_get_capMain(data);

	pr_info("[SX9310]: %s\n", __func__);
	sx9310_i2c_read(data, SX9310_STAT0_REG, &status);
	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++) {
		if (data->state == IDLE) {
			if (status & (CSX_STATUS_REG << cnt)) {
#ifdef DEFENCE_CODE_FOR_DEVICE_DAMAGE
				if ((data->calData[0] - 5000) > capMain) {
					sx9310_set_offset_calibration(data);
					msleep(400);
					sx9310_touchCheckWithRefSensor(data);
					pr_err("[SX9310]: %s - Defence code for"
						" device damage\n", __func__);
					return;
				}
#endif
				send_event(data, ACTIVE);
			} else{
				pr_info("[SX9310]: %s - %d already released.\n",
					__func__, cnt);
			}
		} else { /* User released button */
			if (!(status & (CSX_STATUS_REG << cnt))) {
				if ((data->touchMode == INIT_TOUCH_MODE)
					&& (capMain >= threshold))
					pr_info("[SX9310]: %s - IDLE SKIP\n",
						__func__);
				else
					send_event(data , IDLE);

			} else {
				pr_info("[SX9310]: %s - %d still touched\n",
					__func__, cnt);
			}
		}
	}
}

static void sx9310_process_interrupt(struct sx9310_p *data)
{
	u8 status = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	status = sx9310_read_irqstate(data);

	if (status & IRQ_PROCESS_CONDITION)
		sx9310_touch_process(data);
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

static int sx9310_input_init(struct sx9310_p *data)
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
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx9310_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&data->input->dev.kobj,
			data->input->name);
		input_unregister_device(dev);
		return ret;
	}

	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}

static int sx9310_setup_pin(struct sx9310_p *data)
{
	int ret;

	ret = gpio_request(data->gpioNirq, "SX9310_nIRQ");
	if (ret < 0) {
		pr_err("[SX9310]: %s - gpio %d request failed (%d)\n",
			__func__, data->gpioNirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpioNirq);
	if (ret < 0) {
		pr_err("[SX9310]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->gpioNirq, ret);
		gpio_free(data->gpioNirq);
		return ret;
	}

	data->irq = gpio_to_irq(data->gpioNirq);

	/* initailize interrupt reporting */
	ret = request_threaded_irq(data->irq, NULL, sx9310_interrupt_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT
			, "sx9310_irq", data);
	if (ret < 0) {
		pr_err("[SX9310]: %s - failed to set request_threaded_irq %d"
			" as returning (%d)\n", __func__, data->irq, ret);
		free_irq(data->irq, data);
		gpio_free(data->gpioNirq);
		return ret;
	}

	disable_irq(data->irq);
	return 0;
}
static void sx9310_initialize_variable(struct sx9310_p *data)
{
	int cnt;
	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++)
		data->state= IDLE;

	data->touchTh = DEFAULT_THRESHOLD;
	data->initTh = DEFAULT_THRESHOLD;
	data->touchMode = INIT_TOUCH_MODE;
	data->flagDataSkip = false;
	data->calSuccessed = false;
	memset(data->calData, 0, sizeof(int) * 3);
	atomic_set(&data->enable, OFF);
	data->initTh = (int)2000;
	pr_info("[SX9310]: %s - Init Touch Threshold : %d\n",
		__func__, data->initTh);
	data->touchTh = (u8)248;
	pr_info("[SX9310]: %s - Normal Touch Threshold : %u\n",
		__func__, data->touchTh);
}

static int sx9310_parse_dt(struct sx9310_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->gpioNirq = of_get_named_gpio_flags(dNode,
		"sx9310-i2c,nirq-gpio", 0, &flags);
	if (data->gpioNirq < 0) {
		pr_err("[SX9310]: %s - get gpioNirq error\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int sx9310_regulator_on(struct sx9310_p *data, bool onoff)
{
	struct regulator *reg_vdd;
	struct regulator *reg_vio;
	int ret = 0;

	pr_info("[SX9310]: - %s start!\n", __func__);

	reg_vdd = devm_regulator_get(&data->client->dev, "sx9310-i2c,vdd");
	pr_info("[SX9310]: - %s get the vdd!\n", __func__);
	if (IS_ERR(reg_vdd)) {
		pr_err("[SX9310]: could not get vdd, %ld\n", PTR_ERR(reg_vdd));
		ret = -ENODEV;
		goto err_vdd;
	} else if (!regulator_get_voltage(reg_vdd)) {
		ret = regulator_set_voltage(reg_vdd, 2850000, 2850000);
	}
	reg_vio = devm_regulator_get(&data->client->dev, "sx9310-i2c,svdd");
	if (IS_ERR(reg_vio)) {
		pr_err("[SX9310]: could not get vio, %ld\n", PTR_ERR(reg_vio));
		ret = -ENODEV;
		goto err_vio;
	} else if (!regulator_get_voltage(reg_vio)) {
		ret = regulator_set_voltage(reg_vio, 1800000, 1800000);
	}

	if (onoff) {
		ret = regulator_enable(reg_vdd);
		if (ret)
			pr_err("[SX9310]: - %s Failed to enable vdd.\n",
				__func__);
		ret = regulator_enable(reg_vio);
		if (ret)
			pr_err("[SX9310]: - %s Failed to enable vio.\n",
				__func__);
	} else {
		ret = regulator_disable(reg_vdd);
		if (ret)
			pr_err("[SX9310]: %s - Failed to disable vdd.\n",
				__func__);
		ret = regulator_disable(reg_vio);
		if (ret)
			pr_err("[SX9310]: %s -  Failed to disable vio.\n",
				__func__);
	}

	msleep(30);

	devm_regulator_put(reg_vio);
err_vio:
	devm_regulator_put(reg_vdd);
err_vdd:
	pr_info("[SX9310]: - %s end!\n", __func__);
	return ret;
}

static int sx9310_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9310_p *data = NULL;

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

	ret = sx9310_parse_dt(data, &client->dev);
	if (ret < 0) {
		pr_err("[SX9310]: %s - of_node error\n", __func__);
		ret = -ENODEV;
		goto exit_of_node;
	}

	wake_lock_init(&data->grip_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wake_lock");

	ret = sx9310_setup_pin(data);
	if (ret) {
		pr_err("[SX9310]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;
	sx9310_regulator_on(data, 1);

	/* read chip id */
	ret = sx9310_i2c_write(data, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	if (ret < 0) {
		pr_err("[SX9310]: %s - chip reset failed %d\n", __func__, ret);
		goto exit_chip_reset;
	}

	ret = sx9310_input_init(data);
	if (ret < 0)
		goto exit_input_init;



	ret = sensors_register(data->factory_device,
		data, sensor_attrs, MODULE_NAME);

	if (ret) {
		pr_err("[SENSOR] %s - cound not register grip_sensor(%d).\n",
			__func__, ret);
		goto grip_sensor_register_failed;
	}
	
	sx9310_initialize_variable(data);

	INIT_DELAYED_WORK(&data->init_work, sx9310_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9310_irq_work_func);
	
	mutex_init(&data->mode_mutex);

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	pr_info("[SX9310]: %s - Probe done!\n", __func__);

	return 0;
grip_sensor_register_failed:
	input_unregister_device(data->input);
exit_input_init:
exit_chip_reset:
	free_irq(data->irq, data);
	gpio_free(data->gpioNirq);
exit_setup_pin:
exit_of_node:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SX9310]: %s - Probe fail!\n", __func__);
	return ret;
}

static int sx9310_remove(struct i2c_client *client)
{
	struct sx9310_p *data = (struct sx9310_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_mode(data, SX9310_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	free_irq(data->irq, data);
	gpio_free(data->gpioNirq);

	wake_lock_destroy(&data->grip_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
	sysfs_remove_group(&data->input->dev.kobj, &sx9310_attribute_group);
	input_unregister_device(data->input);
	mutex_destroy(&data->mode_mutex);

	kfree(data);

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
