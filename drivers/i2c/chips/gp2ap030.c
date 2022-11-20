/*
 * Copyright (c) 2012 SAMSUNG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
//#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/gp2ap030.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
//#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/sensors_core.h>

#define GP2A_VENDOR	"SHARP"
#define GP2A_CHIP_ID		"GP2AP030A00F"

#define PROX_READ_NUM	10

#define SENSOR_NAME "light_sensor"

#define SENSOR_DEFAULT_DELAY		(200)	/* 200 ms */
#define SENSOR_MAX_DELAY		(2000)	/* 2000 ms */

static struct i2c_client *opt_i2c_client = NULL;
static int lightval_logcount = 250;

struct gp2a_data {
	struct i2c_client *client;
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct work_struct proximity_work;	/* for proximity sensor */
	struct mutex light_mutex;
	struct mutex data_mutex;
	struct delayed_work light_work;
	struct device *proximity_dev;
	struct device *light_dev;
	struct gp2ap030_pdata *pdata;
	struct wake_lock prx_wake_lock;

	int proximity_enabled;
	int light_enabled;
	u8 lightsensor_mode;		/* 0 = low, 1 = high */
	int prox_data;
	int irq;
	int average[PROX_READ_NUM];	/*for proximity adc average */
	int light_delay;
	char proximity_detection;
	struct device *light_sensor_device;
	struct device *proximity_sensor_device;
	/* Auto Calibration */
	int offset_value;
	int cal_result;
	uint16_t threshold_high;
	bool offset_cal_high;
};


/* initial value for sensor register */
#define COL 8
static u8 gp2a_reg[COL][2] = {
	/*  {Regster, Value} */
	/*PRST :01(4 cycle at Detection/Non-detection),
	   ALSresolution :16bit, range *128   //0x1F -> 5F by sharp */
	{0x01, 0x63},
	/*ALC : 0, INTTYPE : 1, PS mode resolution : 12bit, range*1 */
	{0x02, 0x1A},
	/*LED drive current 110mA, Detection/Non-detection judgment output */
	{0x03, 0x3C},
	{0x08, 0x08},		/*PS mode LTH(Loff):  (??mm) */
	{0x09, 0x00},		/*PS mode LTH(Loff) : */
	{0x0A, 0x0A},		/*PS mode HTH(Lon) : (??mm) */
	{0x0B, 0x00},		/* PS mode HTH(Lon) : */
	/* {0x13 , 0x08}, by sharp for internal calculation (type:0) */
	/*alternating mode (PS+ALS), TYPE=1
	   (0:externel 1:auto calculated mode) //umfa.cal */
	{0x00, 0xC0}
};

static struct gp2a_data *gp2a_opt_data;

#define THR_REG_LSB(data, reg) \
	{ \
		reg = (u8)data & 0xff; \
	}
#define THR_REG_MSB(data, reg) \
	{ \
		reg = (u8)data >> 8; \
	}

static int gp2a_i2c_read(u8 reg, unsigned char *rbuf, int len)
{
	int ret = -1;
	struct i2c_msg msg;

	if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) ){
	    return -ENODEV;
	}
	
	msg.addr = opt_i2c_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &reg;

	ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1);

	if (ret >= 0) {
		msg.flags = I2C_M_RD;
		msg.len = len;
		msg.buf = rbuf;
		ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1);
	}

	if (ret < 0)
		pr_err("i2c transfer error ret=%d\n", ret);

	return ret;
}

static int gp2a_i2c_write(u8 reg, u8 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 3;

	if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) ){
	    return -ENODEV;
	}
	
	do {
		data[0] = reg;
		data[1] = *val;

		msg->addr = opt_i2c_client->addr;
		msg->flags = 0;
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(opt_i2c_client->adapter, msg, 1);

		if (err >= 0)
			return 0;
	} while (--retry > 0);
	pr_err(" i2c transfer error(%d)\n", err);
	return err;
}


int lightsensor_get_adc(struct gp2a_data *data)
{
	unsigned char get_data[4] = { 0, };
	int D0_raw_data;
	int D1_raw_data;
	int D0_data;
	int D1_data;
	int lx = 0;
	u8 value;
	int light_alpha;
	int light_beta;
	static int lx_prev;
	int ret = 0;
	int d0_boundary = 91;
	int d0_custom[9] = { 0, };
	int i = 0;
	mutex_lock(&data->data_mutex);
	ret = gp2a_i2c_read(DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		return lx_prev;
	D0_raw_data = (get_data[1] << 8) | get_data[0];	/* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2];	/* IR */
	if (data->pdata->version) {  /* GP2AP 030 */
		if (data->pdata->d0_value[0]) {
			do {
				d0_custom[i] = data->pdata->d0_value[i];
			} while (i++ < 8);
			d0_boundary = d0_custom[D0_BND];
			if (100 * D1_raw_data <=
				d0_custom[D0_COND1] * D0_raw_data) {
				light_alpha = d0_custom[D0_COND1_A];
				light_beta = 0;
			} else if (100 * D1_raw_data <=
				d0_custom[D0_COND2] * D0_raw_data) {
				light_alpha = d0_custom[D0_COND2_A];
				light_beta = d0_custom[D0_COND2_B];
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = d0_custom[D0_COND3_A];
				light_beta = d0_custom[D0_COND3_B];
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} else {
			if (100 * D1_raw_data <= 40 * D0_raw_data) {
				light_alpha = 978;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 62 * D0_raw_data) {
				light_alpha = 2333;
				light_beta = 3385;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				light_alpha = 734;
				light_beta = 806;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}
	} else {   /* GP2AP 020 */
		if (data->lightsensor_mode) {	/* HIGH_MODE */
			if (100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = 56;
				light_beta = 12;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} else {		/* LOW_MODE */
			if (100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = 547;
				light_beta = 599;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}
	}

	if (data->lightsensor_mode) {	/* HIGH_MODE */
		D0_data = D0_raw_data * 16;
		D1_data = D1_raw_data * 16;
	} else {		/* LOW_MODE */
		D0_data = D0_raw_data;
		D1_data = D1_raw_data;
	}
	if (data->pdata->version) {  /* GP2AP 030 */

		if (D0_data < 3) {

			lx = 0;
		} else if (data->lightsensor_mode == 0
			&& (D0_raw_data >= 16000 || D1_raw_data >= 16000)
			&& (D0_raw_data <= 16383 && D1_raw_data <= 16383)) {
			lx = lx_prev;
		} else if (100 * D1_data > d0_boundary * D0_data) {

			lx = lx_prev;
			return lx;
		} else {
			lx = (int)((light_alpha / 10 * D0_data * 33)
				- (light_beta / 10 * D1_data * 33)) / 1000;
		}
	} else {	/* GP2AP 020 */
		if ((D0_data == 0 || D1_data == 0)\
			&& (D0_data < 300 && D1_data < 300)) {
			lx = 0;
		} else if (data->lightsensor_mode == 0
			&& (D0_raw_data >= 16000 || D1_raw_data >= 16000)
			&& (D0_raw_data <= 16383 && D1_raw_data <= 16383)) {
			lx = lx_prev;
		} else if ((100 * D1_data > d0_boundary * D0_data)
				|| (100 * D1_data < 15 * D0_data)) {
			lx = lx_prev;
			return lx;
		} else {
			lx = (int)((light_alpha / 10 * D0_data * 33)
				- (light_beta / 10 * D1_data * 33)) / 1000;
		}
	}

	lx_prev = lx;

	if (data->lightsensor_mode) {	/* HIGH MODE */
		if (D0_raw_data < 1000) {
			printk(KERN_INFO "[GP2A] %s: change to LOW_MODE detection=%d\n",
				__func__, data->proximity_detection);
			data->lightsensor_mode = 0;	/* change to LOW MODE */

			value = 0x0C;
			gp2a_i2c_write(COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x23;
			else
				value = 0x63;
			gp2a_i2c_write(COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			gp2a_i2c_write(COMMAND1, &value);
		}
	} else {		/* LOW MODE */
		if (D0_raw_data > 16000 || D1_raw_data > 16000) {
			printk(KERN_INFO "[GP2A] %s: change to HIGH_MODE detection=%d\n",
				__func__, data->proximity_detection);
			/* change to HIGH MODE */
			data->lightsensor_mode = 1;

			value = 0x0C;
			gp2a_i2c_write(COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x27;
			else
				value = 0x67;
			gp2a_i2c_write(COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			gp2a_i2c_write(COMMAND1, &value);
		}
	}

	return lx;
}

static int lightsensor_onoff(u8 onoff, struct gp2a_data *data)
{
	u8 value;

	if (onoff) {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x01;
			gp2a_i2c_write(COMMAND4, &value);
			value = 0x63;
			gp2a_i2c_write(COMMAND2, &value);
			/*OP3 : 1(operating mode) OP2 :1
				(coutinuous operating mode)
				OP1 : 01(ALS mode) TYPE=0(auto) */
			value = 0xD0;
			gp2a_i2c_write(COMMAND1, &value);
			/* other setting have defualt value. */
		}
	} else {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x00;	/*shutdown mode */
			gp2a_i2c_write((u8) (COMMAND1), &value);
		}
	}

	return 0;
}

static int proximity_onoff(u8 onoff, struct gp2a_data  *data)
{
	u8 value;
	int i;
	int err = 0;

	/* already on light sensor, so must simultaneously
		turn on light sensor and proximity sensor */
	if (onoff) {
		for (i = 0; i < COL; i++) {
			err = gp2a_i2c_write(gp2a_reg[i][0], &gp2a_reg[i][1]);
			if (err < 0)
				pr_err("%s : turnning on error i = %d, err=%d\n",
					__func__, i, err);
			data->lightsensor_mode = 0;
		}
	} else { /* light sensor turn on and proximity turn off */
		if (data->lightsensor_mode)
			value = 0x67; /*resolution :16bit, range: *8(HIGH) */
		else
			value = 0x63; /* resolution :16bit, range: *128(LOW) */
		gp2a_i2c_write(COMMAND2, &value);
		/* OP3 : 1(operating mode)
			OP2 :1(coutinuous operating mode) OP1 : 01(ALS mode) */
		value = 0xD0;
		gp2a_i2c_write(COMMAND1, &value);
	}

	return 0;
}

static int proximity_adc_read(struct gp2a_data *data)
{
	int sum[OFFSET_ARRAY_LENGTH];
	int i = OFFSET_ARRAY_LENGTH-1;
	int avg;
	int min = 0;
	int max = 0;
	int total = 0;
	int D2_data;
	unsigned char get_D2_data[2]={0,};

	mutex_lock(&data->data_mutex);
	do {
		msleep(50);
		gp2a_i2c_read(DATA2_LSB, get_D2_data,
			sizeof(get_D2_data));
		D2_data = (get_D2_data[1] << 8) | get_D2_data[0];
		sum[i] = D2_data;
		if (i == OFFSET_ARRAY_LENGTH - 1) {
			min = sum[i];
			max = sum[i];
		} else {
			if (sum[i] < min)
				min = sum[i];
			else if (sum[i] > max)
				max = sum[i];
		}
		total += sum[i];
	} while (i--);
	mutex_unlock(&data->data_mutex);

	total -= (min + max);
	avg = (int)(total / (OFFSET_ARRAY_LENGTH - 2));
	printk(KERN_INFO "[GP2A] %s: offset = %d\n", __func__, avg);

	return avg;
}

static int proximity_open_calibration(struct gp2a_data  *data)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->pdata->prox_cal_path,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&data->offset_value,
			sizeof(int), &cal_filp->f_pos);
	if (err != sizeof(int)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	printk(KERN_INFO "[GP2A] %s: (%d)\n", __func__,
		data->offset_value);

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

static int proximity_do_calibrate(struct gp2a_data  *data,
			bool do_calib, bool thresh_set)
{
	struct file *cal_filp;
	int err;
	int xtalk_avg = 0;
	int offset_change = 0;
	uint16_t thrd = 0;
	u8 reg;
	mm_segment_t old_fs;

	if (do_calib) {
		if (thresh_set) {
			/* for proximity_thresh_store */
			data->offset_value =
				data->threshold_high -
				(gp2a_reg[6][1] << 8 | gp2a_reg[5][1]);
		} else {
			/* tap offset button */
			/* get offset value */
			xtalk_avg = proximity_adc_read(data);
			offset_change =
				(gp2a_reg[6][1] << 8 | gp2a_reg[5][1])
				- DEFAULT_HI_THR;
			if (xtalk_avg < offset_change) {
				/* do not need calibration */
				data->cal_result = 0;
				err = 0;
				goto no_cal;
			}
			data->offset_value = xtalk_avg - offset_change;
		}
		/* update threshold */
		thrd = (gp2a_reg[4][1] << 8 | gp2a_reg[3][1])
			+ (data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[3][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[4][0], &reg);

		thrd = (gp2a_reg[4][1] << 8 | gp2a_reg[5][1])
			+(data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[5][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[6][0], &reg);

		/* calibration result */
		if (!thresh_set)
			data->cal_result = 1;
	} else {
		/* tap reset button */
		data->offset_value = 0;
		/* update threshold */
		gp2a_i2c_write(gp2a_reg[3][0], &gp2a_reg[3][1]);
		gp2a_i2c_write(gp2a_reg[4][0], &gp2a_reg[4][1]);
		gp2a_i2c_write(gp2a_reg[5][0], &gp2a_reg[5][1]);
		gp2a_i2c_write(gp2a_reg[6][0], &gp2a_reg[6][1]);
		/* calibration result */
		data->cal_result = 2;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->pdata->prox_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&data->offset_value, sizeof(int),
			&cal_filp->f_pos);
	if (err != sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
no_cal:
	return err;
}

static int proximity_manual_offset(struct gp2a_data  *data, u8 change_on)
{
	struct file *cal_filp;
	int err;
	int16_t thrd;
	u8 reg;
	mm_segment_t old_fs;

	data->offset_value = change_on;
	/* update threshold */
	thrd = gp2a_reg[3][1]+(data->offset_value);
	THR_REG_LSB(thrd, reg);
	gp2a_i2c_write(gp2a_reg[3][0], &reg);
	THR_REG_MSB(thrd, reg);
	gp2a_i2c_write(gp2a_reg[4][0], &reg);

	thrd = gp2a_reg[5][1]+(data->offset_value);
	THR_REG_LSB(thrd, reg);
	gp2a_i2c_write(gp2a_reg[5][0], &reg);
	THR_REG_MSB(thrd, reg);
	gp2a_i2c_write(gp2a_reg[6][0], &reg);

	/* calibration result */
	data->cal_result = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->pdata->prox_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&data->offset_value, sizeof(int),
			&cal_filp->f_pos);
	if (err != sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

static ssize_t
proximity_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->proximity_enabled);
}

static ssize_t
proximity_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	int value = 0;
	int err = 0;
	int16_t thrd;
	u8 reg;

	err = kstrtoint(buf, 10, &value);

	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	if (value != 0 && value != 1)
		goto done;

        printk(KERN_INFO "[GP2A] proximity_enable_store : value=%d, offset=%d\n", value, data->offset_value);

	if (data->proximity_enabled && !value) {	/* Prox power off */
		data->proximity_enabled = value;
		disable_irq(data->irq);

		proximity_onoff(0, data);
		if (data->pdata->led_on)
			data->pdata->led_on(0);
	}
	if (!data->proximity_enabled && value) {	/* prox power on */
		data->proximity_enabled = value;
		if (data->pdata->led_on)
			data->pdata->led_on(1);
		msleep(5);
		proximity_onoff(1, data);

		thrd = gp2a_reg[3][1]+(data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[3][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[4][0], &reg);

		thrd = gp2a_reg[5][1]+(data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[5][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(gp2a_reg[6][0], &reg);

		input_report_abs(data->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(data->proximity_input_dev);

		enable_irq(data->irq);
	}
done:
	return count;
}
static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
			proximity_enable_show, proximity_enable_store);

static int D2_data_val = 0;

static ssize_t proximity_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	struct gp2a_data *data = dev_get_drvdata(dev);
	static int count;		/*count for proximity average */

	int D2_data = 0;
	unsigned char get_D2_data[2] = { 0, };

	mutex_lock(&data->data_mutex);
	msleep(10);
	gp2a_i2c_read(DATA2_LSB, get_D2_data, sizeof(get_D2_data));
	mutex_unlock(&data->data_mutex);
	D2_data = (get_D2_data[1] << 8) | get_D2_data[0];

	data->average[count] = D2_data;
	count++;
	if (count == PROX_READ_NUM)
		count = 0;

	//D2_data = D2_data - (data->offset_value); // for ADC compensation
	
	//printk(KERN_INFO "[GP2A] %s: D2_data = %d\n", __func__, D2_data);

        if(D2_data >=0 && D2_data <1024)
            D2_data_val = D2_data;
        else
            D2_data = D2_data_val;
    
	return snprintf(buf, PAGE_SIZE, "%d\n", D2_data);
}
static struct device_attribute dev_attr_proximity_sensor_state =
	__ATTR(state, S_IRUSR | S_IRGRP, proximity_state_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_raw_data =
	__ATTR(raw_data, S_IRUSR | S_IRGRP, proximity_state_show, NULL);

static ssize_t proximity_avg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	int min = 0, max = 0, avg = 0;
	int i;
	int proximity_value = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		proximity_value = data->average[i];
		if (proximity_value > 0) {

			avg += proximity_value;

			if (!i)
				min = proximity_value;
			else if (proximity_value < min)
				min = proximity_value;

			if (proximity_value > max)
				max = proximity_value;
		}
	}
	avg /= i;

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", min, avg, max);
}

static ssize_t proximity_avg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	return proximity_enable_store(dev, attr, buf, size);
}

static struct device_attribute dev_attr_proximity_sensor_prox_avg =
	__ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
				proximity_avg_show, proximity_avg_store);

static ssize_t proximity_cal_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	int thresh_hi;
	unsigned char get_D2_data[2];

	msleep(20);
	gp2a_i2c_read(PS_HT_LSB, get_D2_data,
		sizeof(get_D2_data));
	thresh_hi = (get_D2_data[1] << 8) | get_D2_data[0];
	data->threshold_high = thresh_hi;
	return sprintf(buf, "%d,%d\n",
			data->offset_value, data->threshold_high);
}

static ssize_t proximity_cal_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1")) { /* calibrate cancelation value */
		do_calib = true;
	} else if (sysfs_streq(buf, "0")) { /* reset cancelation value */
		do_calib = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		err = -EINVAL;
		goto done;
	}
	err = proximity_do_calibrate(data, do_calib, false);
	if (err < 0) {
		pr_err("%s: proximity_store_offset() failed\n", __func__);
		goto done;
	} else
		err = size;
done:
	return err;
}

static struct device_attribute dev_attr_proximity_sensor_prox_cal =
	__ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
				proximity_cal_show, proximity_cal_store);

static ssize_t proximity_cal2_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	u8 change_on;
	int err;

	if (sysfs_streq(buf, "1")) /* change hi threshold by -2 */
		change_on = -2;
	else if (sysfs_streq(buf, "2")) /*change hi threshold by +4 */
		change_on = 4;
	else if (sysfs_streq(buf, "3")) /*change hi threshold by +8 */
		change_on = 8;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		err = -EINVAL;
		goto done;
	}
	err = proximity_manual_offset(data, change_on);
	if (err < 0) {
		pr_err("%s: proximity_store_offset() failed\n", __func__);
		goto done;
	}
done:
	return size;
}
static struct device_attribute dev_attr_proximity_sensor_prox_cal2 =
	__ATTR(prox_cal2, S_IRUGO | S_IWUSR | S_IWGRP,
				NULL, proximity_cal2_store);

static ssize_t proximity_thresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	/*struct gp2a_data *data = dev_get_drvdata(dev);*/
	int thresh_hi = 0;
	unsigned char get_D2_data[2];

	msleep(20);
	gp2a_i2c_read(PS_HT_LSB, get_D2_data,
		sizeof(get_D2_data));
	thresh_hi = (get_D2_data[1] << 8) | get_D2_data[0];
	printk(KERN_INFO "[GP2A] %s: THRESHOLD = %d\n", __func__, thresh_hi);

	return sprintf(buf, "prox_threshold = %d\n", thresh_hi);
}

static ssize_t proximity_thresh_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	long thresh_value = 0;
	int err = 0;

	err = strict_strtol(buf, 10, &thresh_value);
	if (unlikely(err < 0)) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	data->threshold_high = (uint16_t)thresh_value;
	err = proximity_do_calibrate(data, true, true);
	if (err < 0) {
		pr_err("%s: thresh_store failed\n", __func__);
		goto done;
	}
	msleep(20);
done:
	return size;
}

static struct device_attribute dev_attr_proximity_sensor_prox_thresh =
	__ATTR(prox_thresh, S_IRUGO | S_IWUSR | S_IWGRP,
				proximity_thresh_show, proximity_thresh_store);

static ssize_t prox_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->cal_result);
}
static struct device_attribute dev_attr_proximity_sensor_offset_pass =
	__ATTR(prox_offset_pass, S_IRUGO | S_IWUSR | S_IWGRP,
				prox_offset_pass_show, NULL);

/* Light Sysfs interface */
static ssize_t lightsensor_file_state_show(struct device *dev,
						   struct device_attribute *attr,
						   char *buf)
{
	int adc = 0;
	struct gp2a_data *data = dev_get_drvdata(dev);

	adc = lightsensor_get_adc(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);
}
	
static ssize_t lightsensor_raw_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	unsigned char get_data[4] = { 0, };
	int D0_raw_data;
	int D1_raw_data;
	int ret = 0;

	mutex_lock(&data->data_mutex);
	ret = gp2a_i2c_read(DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		pr_err("%s i2c err: %d\n", __func__, ret) ;
	D0_raw_data = (get_data[1] << 8) | get_data[0];	/* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2];	/* IR */

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", D0_raw_data, D1_raw_data);
}
static struct device_attribute dev_attr_light_sensor_lux =
	__ATTR(lux, S_IRUSR | S_IRGRP, lightsensor_file_state_show, NULL);
static struct device_attribute dev_attr_light_sensor_raw_data =
	__ATTR(raw_data, S_IRUSR | S_IRGRP, lightsensor_raw_show, NULL);

static ssize_t
light_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->light_delay);
}

static ssize_t
light_delay_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	int delay;
	int err = 0;

	err = kstrtoint(buf, 10, &delay);

	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	if (delay < 0)
		goto done;

	delay = delay / 1000000;	/* ns to msec */

        printk(KERN_INFO "[GP2A] poll_delay_store : new_delay=%d\n", delay);    

	if (SENSOR_MAX_DELAY < delay)
		delay = SENSOR_MAX_DELAY;

	data->light_delay = delay;

	mutex_lock(&data->light_mutex);

	if (data->light_enabled) {
		cancel_delayed_work_sync(&data->light_work);
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(delay));
	}

	mutex_unlock(&data->light_mutex);
done:
	return count;
}
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP, light_delay_show,
	light_delay_store);

static ssize_t
light_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->light_enabled);
}

static ssize_t
light_enable_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	int value;
	int err = 0;

	err = kstrtoint(buf, 10, &value);

	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
        printk(KERN_INFO "[GP2A] light_enable_store : value=%d\n", value);

	if (value != 0 && value != 1)
		goto done;

	mutex_lock(&data->light_mutex);

	if (data->light_enabled && !value) {
		data->light_enabled = value;
		cancel_delayed_work_sync(&data->light_work);
		lightsensor_onoff(0, data);
        
	}
	if (!data->light_enabled && value) {
		data->light_enabled = value;
		lightsensor_onoff(1, data);
            
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(20));
	}

	mutex_unlock(&data->light_mutex);
done:
	return count;
}
static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
			light_enable_show, light_enable_store);

static ssize_t gp2a_vendor_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", GP2A_VENDOR);
}
static struct device_attribute dev_attr_light_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, gp2a_vendor_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, gp2a_vendor_show, NULL);

static ssize_t gp2a_name_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", GP2A_CHIP_ID);
}

static struct device_attribute dev_attr_light_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, gp2a_name_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, gp2a_name_show, NULL);

static struct attribute *proximity_attributes[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute *lightsensor_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_light_enable.attr,
	NULL
};


static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_attributes
};

static struct attribute_group lightsensor_attribute_group = {
	.attrs = lightsensor_attributes
};

static struct device_attribute *additional_light_attrs[] = {
	&dev_attr_light_sensor_lux,
	&dev_attr_light_sensor_raw_data,
	&dev_attr_light_sensor_vendor,
	&dev_attr_light_sensor_name,
	NULL,
};

static struct device_attribute *additional_proximity_attrs[] = {
	&dev_attr_proximity_sensor_state,
	&dev_attr_proximity_sensor_raw_data,
	&dev_attr_proximity_sensor_prox_avg,
	&dev_attr_proximity_sensor_prox_cal,
	&dev_attr_proximity_sensor_prox_cal2,
	&dev_attr_proximity_sensor_prox_thresh,
	&dev_attr_proximity_sensor_offset_pass,
	&dev_attr_proximity_sensor_vendor,
	&dev_attr_proximity_sensor_name,
	NULL,
};


static irqreturn_t gp2a_irq_handler(int irq, void *dev_id)
{
	struct gp2a_data *gp2a = dev_id;

  	printk(KERN_INFO "[GP2A] gp2a_irq_handler called\n");
    
	wake_lock_timeout(&gp2a->prx_wake_lock, 3 * HZ);

	schedule_work(&gp2a->proximity_work);

	return IRQ_HANDLED;
}


static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc = -EIO;
	struct gp2ap030_pdata *pdata = gp2a->pdata;
	int irq;

	gpio_free(pdata->p_out);
	rc = gpio_request(pdata->p_out, "gpio_proximity_out");
	if (unlikely(rc < 0)) {
		pr_err("%s: gpio %d request failed (%d)\n",
				__func__, pdata->p_out, rc);
		goto done;
	}

	rc = gpio_direction_input(pdata->p_out);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
				__func__, pdata->p_out, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->p_out);
	rc = request_threaded_irq(irq, NULL,
			gp2a_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
			"proximity_int", gp2a);
	if (unlikely(rc < 0)) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
				__func__, irq, pdata->p_out, rc);
		goto err_request_irq;
	}

	printk(KERN_INFO "[GP2A] request_irq success IRQ_NO:%d, GPIO:%d", irq, pdata->p_out);

	/* start with interrupts disabled */
	disable_irq(irq);
	gp2a->irq = irq;

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->p_out);
done:
	return rc;
}



static void gp2a_work_func_prox(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of((struct work_struct *)work,
					struct gp2a_data, proximity_work);

	unsigned char value;
	char result;
	int ret;

	/* 0 : proximity, 1 : away */
        result = gpio_get_value(gp2a->pdata->p_out);
	gp2a->proximity_detection = !result;

	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, result);
	input_sync(gp2a->proximity_input_dev);

        printk(KERN_INFO "[GP2A] proximity value = %d \n",result);
	
	disable_irq(gp2a->irq);

	/*Software reset */
	value = 0x0C;
	ret = gp2a_i2c_write(COMMAND1, &value);

	if (result == 0) {	/* detection = Falling Edge */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x23;
		else		/* High mode */
			value = 0x27;
		ret = gp2a_i2c_write(COMMAND2, &value);
	} else {		/* none Detection */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x63;
		else		/* High mode */
			value = 0x67;
		ret = gp2a_i2c_write(COMMAND2, &value);
	}

	enable_irq(gp2a->irq);

	value = 0xCC;
	ret = gp2a_i2c_write(COMMAND1, &value);

	gp2a->prox_data = result;
}

static int proximity_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s, error\n", __func__);
		err = -ENOMEM;
		goto done;
	}

	input_set_capability(dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(dev, ABS_DISTANCE, 0, 1, 0, 0);

	dev->name = "proximity_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto done;
	}
	data->proximity_input_dev = dev;

	printk(KERN_INFO "[GP2A] %s, success\n", __func__);
done:
	return err;
}

static int light_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s, error\n", __func__);
		err = -ENOMEM;
		goto done;
	}

	input_set_capability(dev, EV_ABS, ABS_MISC);
       input_set_abs_params(dev, ABS_MISC, 0, 1, 0, 0);

	dev->name = "light_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto done;
	}
	data->light_input_dev = dev;

	printk(KERN_INFO "[GP2A] %s, success\n", __func__);
done:
	return err;
}

static void gp2a_work_func_light(struct work_struct *work)
{
	struct gp2a_data *data = container_of((struct delayed_work *)work,
						struct gp2a_data, light_work);
	int adc;

	adc = lightsensor_get_adc(data);

	input_report_abs(data->light_input_dev, ABS_MISC, adc);
	input_sync(data->light_input_dev);

	if (lightval_logcount++ > 250) {
		printk(KERN_INFO "[GP2A] light value = %d \n", adc);
		lightval_logcount = 0;
	}
   
	if (data->light_enabled)
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(data->light_delay));
}


static int gp2a_opt_open(struct inode *ip, struct file *fp)
{
	return nonseekable_open(ip, fp);	
}

static int gp2a_opt_release(struct inode *ip, struct file *fp)
{	
	return 0;
}

static long gp2a_opt_ioctl(struct file *file, unsigned int cmd,  unsigned long arg)
{	
	int ret = 0;
	short data = 0;
	u8 thrd = 0;
	u8 reg;

	switch (cmd)
	{
		case PROX_IOC_SET_CALIBRATION:
		{
			printk(KERN_INFO "[GP2A] PROX_IOC_SET_CALIBRATION\n");                
			if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
				return -EFAULT;

			ret = proximity_open_calibration(gp2a_opt_data);
			if (ret < 0 && ret != -ENOENT)
			{
				printk(KERN_INFO "[GP2A] proximity_open_offset() failed\n");
			}else {
				thrd = gp2a_reg[3][1]+(gp2a_opt_data->offset_value);
				THR_REG_LSB(thrd, reg);
				gp2a_i2c_write(gp2a_reg[3][0], &reg);
				THR_REG_MSB(thrd, reg);
				gp2a_i2c_write(gp2a_reg[4][0], &reg);

				thrd = gp2a_reg[5][1]+(gp2a_opt_data->offset_value);
				THR_REG_LSB(thrd, reg);
				gp2a_i2c_write(gp2a_reg[5][0], &reg);
				THR_REG_MSB(thrd, reg);
				gp2a_i2c_write(gp2a_reg[6][0], &reg);
			}
			break;
		}
		case PROX_IOC_GET_CALIBRATION:
		{
			printk(KERN_INFO "[GP2A] PROX_IOC_GET_CALIBRATION\n");      
			data = gp2a_opt_data->offset_value;
			if (copy_to_user((void __user *)arg, &data, sizeof(data)))
				return -EFAULT;
			break;
		}
		default:
			printk(KERN_ERR "Unknown IOCTL command");
			ret = -ENOTTY;
			break;
	}
	return ret;
}


static struct file_operations gp2a_opt_fops = {
	.owner  	= THIS_MODULE,
	.open   	= gp2a_opt_open,
	.release 	= gp2a_opt_release,    
	.unlocked_ioctl 	= gp2a_opt_ioctl,
};

static struct miscdevice gp2a_opt_misc_device = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "proximity",
    .fops   = &gp2a_opt_fops,
};

static int gp2a_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct gp2a_data *gp2a;
	struct gp2ap030_pdata *pdata = client->dev.platform_data;
	u8 value = 0;
	int err = 0;

	printk(KERN_INFO "[GP2A] %s : probe start!\n", __func__);

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		err = -EINVAL;
		goto done;
	}
    
	if (pdata->power_on)
		pdata->power_on(1);    

	if (pdata->led_on)
		pdata->led_on(1);    
	
	/* allocate driver_data */
	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("kzalloc error\n");
		err = -ENOMEM;
		goto done;
	}

	gp2a->pdata = pdata;
	gp2a->client = client;

	gp2a->proximity_enabled = 0;

	gp2a->light_enabled = 0;
	gp2a->light_delay = SENSOR_DEFAULT_DELAY;

	i2c_set_clientdata(client, gp2a);

	opt_i2c_client = client;

	if (pdata->version) { /* GP2AP030 */
		gp2a_reg[1][1] = 0x1A;

		if (pdata->thresh[0])
			gp2a_reg[3][1] = pdata->thresh[0];
		else
			gp2a_reg[3][1] = 0x08;
		if (pdata->thresh[1])
			gp2a_reg[5][1] = pdata->thresh[1];
		else
			gp2a_reg[5][1] = 0x0A;		
	}

	INIT_DELAYED_WORK(&gp2a->light_work, gp2a_work_func_light);
	INIT_WORK(&gp2a->proximity_work, gp2a_work_func_prox);

	/*misc device registration*/
        err = misc_register(&gp2a_opt_misc_device);
	if( err < 0 )
		goto error_setup_reg_misc;

	err = proximity_input_init(gp2a);
	if (err < 0)
		goto error_setup_reg_prox;

	err = light_input_init(gp2a);
	if (err < 0)
		goto error_setup_reg_light;

	err = sysfs_create_group(&gp2a->proximity_input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (err < 0)
		goto err_sysfs_create_group_proximity;

	err = sysfs_create_group(&gp2a->light_input_dev->dev.kobj,
				&lightsensor_attribute_group);
	if (err)
		goto err_sysfs_create_group_light;

	mutex_init(&gp2a->light_mutex);
	mutex_init(&gp2a->data_mutex);

	/* wake lock init */
	wake_lock_init(&gp2a->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");

	/* GP2A Regs INIT SETTINGS  and Check I2C communication */
	/* shutdown mode op[3]=0 */
	value = 0x00;
	err = gp2a_i2c_write((u8) (COMMAND1), &value);
	if (err < 0) {
		pr_err("%s failed : threre is no such device.\n", __func__);
		goto err_no_device;
	}

	/* Setup irq */
	err = gp2a_setup_irq(gp2a);
	if (err) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	err = sensors_register(&(gp2a->light_sensor_device),
		gp2a, additional_light_attrs, "light_sensor");
	if (err) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_light;
	}
	err = sensors_register(&(gp2a->proximity_sensor_device),
		gp2a, additional_proximity_attrs, "proximity_sensor");
	if (err) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_proximity;
	}

	gp2a_opt_data = gp2a;

	if (pdata->led_on) {       
            pdata->led_on(0);   
            printk(KERN_INFO "[GP2A] gpio_get_value of GPIO(%d) is %d\n",pdata ->power_gpio, 
                gpio_get_value(pdata ->power_gpio));
	}

	/* set initial proximity value as 1 */
	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(gp2a->proximity_input_dev);
   
	printk(KERN_INFO "[GP2A] %s: probe success!\n", __func__);

	return 0;

err_sysfs_create_factory_proximity:
err_sysfs_create_factory_light:
	free_irq(gp2a->irq, gp2a);
	gpio_free(gp2a->pdata->p_out);
err_setup_irq:
err_no_device:
	wake_lock_destroy(&gp2a->prx_wake_lock);
	mutex_destroy(&gp2a->light_mutex);
	mutex_destroy(&gp2a->data_mutex);
	sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
			&lightsensor_attribute_group);
err_sysfs_create_group_light:
	sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			&proximity_attribute_group);
err_sysfs_create_group_proximity:
	input_unregister_device(gp2a->light_input_dev);
error_setup_reg_light:
	input_unregister_device(gp2a->proximity_input_dev);
error_setup_reg_prox:
	misc_deregister(&gp2a_opt_misc_device);    
error_setup_reg_misc:
	kfree(gp2a);
done:
	return err;
}

static int gp2a_i2c_remove(struct i2c_client *client)
{
	struct gp2a_data *gp2a = i2c_get_clientdata(client);

	if (gp2a == NULL) {
		pr_err("%s, gp2a_data is NULL!!!!!\n", __func__);
		return 0;
	}

	if (gp2a->proximity_input_dev != NULL) {
		sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
				&proximity_attribute_group);
		input_unregister_device(gp2a->proximity_input_dev);

		if (gp2a->proximity_input_dev != NULL)
			kfree(gp2a->proximity_input_dev);
	}

	wake_lock_destroy(&gp2a->prx_wake_lock);
	cancel_delayed_work(&gp2a->light_work);
	flush_scheduled_work();
	mutex_destroy(&gp2a->light_mutex);

	if (gp2a->light_input_dev != NULL) {
		sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
				&lightsensor_attribute_group);
		input_unregister_device(gp2a->light_input_dev);

		if (gp2a->light_input_dev != NULL)
			kfree(gp2a->light_input_dev);
	}

	mutex_destroy(&gp2a->data_mutex);
	kfree(gp2a);

	opt_i2c_client = NULL;

	return 0;
}

static void gp2a_i2c_shutdown(struct i2c_client *client)
{
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	if (unlikely(gp2a == NULL)) {
		pr_err("%s, gp2a_data is NULL!!!!!\n", __func__);
		return;
	}

	cancel_delayed_work(&gp2a->light_work);
	//flush_scheduled_work();
	misc_deregister(&gp2a_opt_misc_device);
}

static int gp2a_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);

	mutex_lock(&gp2a->light_mutex);
	if (gp2a->light_enabled)
		cancel_delayed_work_sync(&gp2a->light_work);

	mutex_unlock(&gp2a->light_mutex);

	return 0;
}

static int gp2a_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);

	mutex_lock(&gp2a->light_mutex);

	if (gp2a->light_enabled)
		schedule_delayed_work(&gp2a->light_work, 0);

	mutex_unlock(&gp2a->light_mutex);

	return 0;
}

static const struct i2c_device_id gp2a_device_id[] = {
	{"gp2a030", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, opt_device_id);

static const struct dev_pm_ops gp2a_dev_pm_ops = {
	.suspend = gp2a_i2c_suspend,
	.resume = gp2a_i2c_resume,
};

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
			.name = "gp2a030",
			.owner = THIS_MODULE,
			.pm = &gp2a_dev_pm_ops,
	},
	.probe = gp2a_i2c_probe,
	.remove = gp2a_i2c_remove,
	.shutdown = gp2a_i2c_shutdown,
	.id_table = gp2a_device_id,
};

static int gp2a_i2c_init(void)
{
	printk(KERN_INFO "[GP2A] %s called",__func__); 

	if (i2c_add_driver(&gp2a_i2c_driver)) {
		pr_err("i2c_add_driver failed\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit gp2a_i2c_exit(void)
{
	i2c_del_driver(&gp2a_i2c_driver);
}

module_init(gp2a_i2c_init);
module_exit(gp2a_i2c_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP030A00F");
MODULE_LICENSE("GPL");
