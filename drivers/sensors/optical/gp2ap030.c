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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
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
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/sensors_core.h>
#include <linux/of_gpio.h>

#define GP2A_VENDOR	"SHARP"
#define GP2A_CHIP_ID		"GP2AP030"

#define PROX_READ_NUM	10

#define SENSOR_NAME "light_sensor"

#define SENSOR_DEFAULT_DELAY		(200)	/* 200 ms */
#define SENSOR_MAX_DELAY		(2000)	/* 2000 ms */

static int is_threshold_changed = 0;

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
	struct gp2ap020_pdata *pdata;
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
/*#if !defined(CONFIG_MACH_MS01_CHN_CTC)
	u32  vdd_en ;
#endif*/
	struct regulator *vdd_2p85;
	struct regulator *vreg_2p8;
	struct regulator *lvs1_1p8;
};


/* initial value for sensor register */
#define COL 10
static u8 gp2a_reg[COL][2] = {
	/*  {Regster, Value} */
	/*PRST :01(4 cycle at Detection/Non-detection),
	   ALSresolution :16bit, range *128   //0x1F -> 5F by sharp */
	{0x01, 0x63},
	/*ALC : 0, INTTYPE : 1, PS mode resolution : 12bit, range*1 */
	{0x02, 0x1A},
	/*LED drive current 110mA, Detection/Non-detection judgment output */
	{0x03, 0x3C},
	{0x06, 0xFF},
	{0x07, 0xFF},
#ifdef CONFIG_MACH_CRATERQ_CHN_OPEN
	{0x08, 0x05},		/*PS mode LTH(Loff):  (??mm) */
#else
	{0x08, 0x06},		/*PS mode LTH(Loff):  (??mm) */
#endif
	{0x09, 0x00},		/*PS mode LTH(Loff) : */
	{0x0A, 0x07},		/*PS mode HTH(Lon) : (??mm) */
	{0x0B, 0x00},		/* PS mode HTH(Lon) : */
	/* {0x13 , 0x08}, by sharp for internal calculation (type:0) */
	/*alternating mode (PS+ALS), TYPE=1
	   (0:externel 1:auto calculated mode) //umfa.cal */
	{0x00, 0xCE}
};

#define THR_REG_LSB(data, reg) \
	{ \
		reg = (u8)data & 0xff; \
	}
#define THR_REG_MSB(data, reg) \
	{ \
		reg = (u8)data >> 8; \
	}

static int gp2a_i2c_read(struct gp2a_data *gp2a,
	u8 reg, unsigned char *rbuf, int len)
{
	int ret = -1;
	struct i2c_msg msg;
	struct i2c_client *client = gp2a->client;

	if (unlikely((client == NULL) || (!client->adapter)))
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (likely(ret >= 0)) {
		msg.flags = I2c_M_RD;
		msg.len = len;
		msg.buf = rbuf;
		ret = i2c_transfer(client->adapter, &msg, 1);
	}

	if (unlikely(ret < 0))
		pr_err("i2c transfer error ret=%d\n", ret);

	return ret;
}

static int gp2a_i2c_write(struct gp2a_data *gp2a,
	u8 reg, u8 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 3;
	struct i2c_client *client = gp2a->client;

	if (unlikely((client == NULL) || (!client->adapter)))
		return -ENODEV;

	do {
		data[0] = reg;
		data[1] = *val;

		msg->addr = client->addr;
		msg->flags = I2C_M_WR;
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(client->adapter, msg, 1);

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
	int light_alpha = 0;
	int light_beta = 0;
	static int lx_prev;
	int ret ;
	int d0_boundary = 91;
#ifndef  CONFIG_MACH_LT02
//	int d0_custom[9] = { 0, };
//	int i = 0;
#endif
	mutex_lock(&data->data_mutex);
	ret = gp2a_i2c_read(data, DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		return lx_prev;
	D0_raw_data = (get_data[1] << 8) | get_data[0]; /* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2]; /* IR */
	if (data->pdata->version) { /* GP2AP 030 */
#ifdef  CONFIG_MACH_LT02
		d0_boundary = 91;
		if (100 * D1_raw_data <= 40 * D0_raw_data) {
			light_alpha = 861;
			light_beta = 0;
		} else if (100 * D1_raw_data <= 62 * D0_raw_data) {
			light_alpha = 2668;
			light_beta = 4027;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			if( (D0_raw_data < 3000) && (data->lightsensor_mode == 0 ) ) {
				light_alpha = 292;
				light_beta = 316;
			} else { /* Incandescent High lux */
				light_alpha = 998;
				light_beta = 1096;
			}
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
#else
/*		if (data->pdata->d0_value[0]) {
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
				light_alpha = 935;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 54 * D0_raw_data) {
				light_alpha = 3039;
				light_beta = 5176;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = 494;
				light_beta = 533;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}*/
	/* GP2A030 light sensor mode */
/*printk("\n gp2a030 light sensor code");*/
			if (100 * D1_raw_data <= 55 * D0_raw_data) {
				light_alpha = 695;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 75 * D0_raw_data) {
				light_alpha = 2386;
				light_beta = 3037;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				if((D0_raw_data < 400) && (data->lightsensor_mode == 0)) {
					light_alpha = 319;
					light_beta = 346;
			} else if ((D0_raw_data < 1950) && (data->lightsensor_mode == 0)) {
					light_alpha = 382;
					light_beta = 416;
				} else {
					 light_alpha = 437;
					 light_beta = 475;
				}
				/*light_alpha = 56;*/
				/*light_beta = 12;*/
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
#endif
	} else {   /* GP2AP 020 */
		if (data->lightsensor_mode) {	/* HIGH_MODE */
			if (100 * D1_raw_data <= 55 * D0_raw_data) {
				light_alpha = 775;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 75 * D0_raw_data) {
				light_alpha = 2681;
				light_beta = 3464;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				if((D0_raw_data < 3800) && (data->lightsensor_mode == 0)) {
						light_alpha = 385;
						light_beta = 424;
				} else {
						light_alpha = 350;
						light_beta = 357;
				}
				/*light_alpha = 56;*/
				/*light_beta = 12;*/
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} /*else { Low Mode
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
		}*/
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
			lx = (int)((light_alpha * D0_data)
                    - (light_beta * D1_data)) / 100 * 33 /100;
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
			pr_info("%s: change to LOW_MODE detection=%d\n",
				__func__, data->proximity_detection);
			data->lightsensor_mode = 0;	/* change to LOW MODE */

			value = 0x0C;
			gp2a_i2c_write(data, COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x23;
			else
				value = 0x63;
			gp2a_i2c_write(data, COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			gp2a_i2c_write(data, COMMAND1, &value);
		}
	} else {		/* LOW MODE */
		if (D0_raw_data > 16000 || D1_raw_data > 16000) {
			pr_info("%s: change to HIGH_MODE detection=%d\n",
				__func__, data->proximity_detection);
			/* change to HIGH MODE */
			data->lightsensor_mode = 1;

			value = 0x0C;
			gp2a_i2c_write(data, COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x27;
			else
				value = 0x67;
			gp2a_i2c_write(data, COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			gp2a_i2c_write(data, COMMAND1, &value);
		}
	}

	return lx;
}

static int lightsensor_onoff(u8 onoff, struct gp2a_data *data)
{
	u8 value;

	pr_debug("%s : light_sensor onoff = %d\n", __func__, onoff);

	if (onoff) {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x01;
			gp2a_i2c_write(data, COMMAND4, &value);
			value = 0x63;
			gp2a_i2c_write(data, COMMAND2, &value);
			/*OP3 : 1(operating mode) OP2 :1
				(coutinuous operating mode)
				OP1 : 01(ALS mode) TYPE=0(auto) */
			value = 0xD0;
			gp2a_i2c_write(data, COMMAND1, &value);
			/* other setting have defualt value. */
		}
	} else {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x00;	/*shutdown mode */
			gp2a_i2c_write(data, (u8) (COMMAND1), &value);
		}
	}

	return 0;
}

static int proximity_onoff(u8 onoff, struct gp2a_data  *data)
{
	u8 value;
	int i;
	int err = 0;
	unsigned char get_D2_data[2];

	pr_info("%s : proximity turn on/off = %d\n", __func__, onoff);

	/* already on light sensor, so must simultaneously
		turn on light sensor and proximity sensor */
	if (onoff) {
		for (i = 0; i < COL; i++) {
			if(is_threshold_changed) {
				if(gp2a_reg[i][0] == PS_HT_LSB) {
					gp2a_i2c_read(data, PS_HT_LSB, get_D2_data, sizeof(get_D2_data));
					mdelay(10);
					err = gp2a_i2c_write(data, PS_HT_LSB, get_D2_data);
				} else
					err = gp2a_i2c_write(data, gp2a_reg[i][0],&gp2a_reg[i][1]);
			} else
				err = gp2a_i2c_write(data, gp2a_reg[i][0],&gp2a_reg[i][1]);

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
		gp2a_i2c_write(data, COMMAND2, &value);
		/* OP3 : 1(operating mode)
			OP2 :1(coutinuous operating mode) OP1 : 01(ALS mode) */
		value = 0xD0;
		gp2a_i2c_write(data, COMMAND1, &value);
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
	unsigned char get_D2_data[2];

	mutex_lock(&data->data_mutex);
	do {
		msleep(50);
		gp2a_i2c_read(data, DATA2_LSB, get_D2_data,
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
	pr_info("%s: offset = %d\n", __func__, avg);

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

	pr_info("%s: (%d)\n", __func__,
		data->offset_value);

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	//temporary patch
#if defined(CONFIG_MACH_CRATERTD_CHN_3G)

	if(err<=-2)
		{
           data->offset_value=12;
		   err=4;
	    }
#endif

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
				(gp2a_reg[8][1] << 8 | gp2a_reg[7][1]);
		} else {
			/* tap offset button */
			/* get offset value */
			is_threshold_changed = 1;
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
		thrd = (gp2a_reg[6][1] << 8 | gp2a_reg[5][1])
			+ (data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(data, gp2a_reg[5][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(data, gp2a_reg[6][0], &reg);

		thrd = (gp2a_reg[8][1] << 8 | gp2a_reg[7][1])
			+(data->offset_value);
		THR_REG_LSB(thrd, reg);
		gp2a_i2c_write(data, gp2a_reg[7][0], &reg);
		THR_REG_MSB(thrd, reg);
		gp2a_i2c_write(data, gp2a_reg[8][0], &reg);

		/* calibration result */
		if (!thresh_set)
			data->cal_result = 1;
	} else {
		/* tap reset button */
		data->offset_value = 0;
		/* update threshold */
		gp2a_i2c_write(data, gp2a_reg[5][0], &gp2a_reg[5][1]);
		gp2a_i2c_write(data, gp2a_reg[6][0], &gp2a_reg[6][1]);
		gp2a_i2c_write(data, gp2a_reg[7][0], &gp2a_reg[7][1]);
		gp2a_i2c_write(data, gp2a_reg[8][0], &gp2a_reg[8][1]);
		/* calibration result */
		data->cal_result = 2;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->pdata->prox_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY,
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
	gp2a_i2c_write(data, gp2a_reg[3][0], &reg);
	THR_REG_MSB(thrd, reg);
	gp2a_i2c_write(data, gp2a_reg[4][0], &reg);

	thrd = gp2a_reg[5][1]+(data->offset_value);
	THR_REG_LSB(thrd, reg);
	gp2a_i2c_write(data, gp2a_reg[5][0], &reg);
	THR_REG_MSB(thrd, reg);
	gp2a_i2c_write(data, gp2a_reg[6][0], &reg);

	/* calibration result */
	data->cal_result = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->pdata->prox_cal_path,
			O_CREAT | O_TRUNC | O_WRONLY,
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
	char input;
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

	pr_info("%s, %d value = %d\n", __func__, __LINE__, value);

	if (data->proximity_enabled && !value) {	/* Prox power off */
		disable_irq(data->irq);

		proximity_onoff(0, data);
		disable_irq_wake(data->irq);
		//if (data->pdata->led_on)
		//	data->pdata->led_on(false);
	}
	if (!data->proximity_enabled && value) {	/* prox power on */
		//if (data->pdata->led_on)
		//	data->pdata->led_on(true);
		usleep_range(1000, 1100);
		proximity_onoff(1, data);

		err = proximity_open_calibration(data);
		if (err < 0 && err != -ENOENT)
			pr_err("%s: proximity_open_offset() failed\n",
				__func__);
		else {
			thrd = gp2a_reg[3][1]+(data->offset_value);
			THR_REG_LSB(thrd, reg);
			gp2a_i2c_write(data, gp2a_reg[3][0], &reg);
			THR_REG_MSB(thrd, reg);
			gp2a_i2c_write(data, gp2a_reg[4][0], &reg);

			thrd = gp2a_reg[5][1]+(data->offset_value);
			THR_REG_LSB(thrd, reg);
			gp2a_i2c_write(data, gp2a_reg[5][0], &reg);
			THR_REG_MSB(thrd, reg);
			gp2a_i2c_write(data, gp2a_reg[6][0], &reg);
		}

		enable_irq_wake(data->irq);
		msleep(160);

		input = gpio_get_value_cansleep(data->pdata->p_out);
		if (input == 0) {
			input_report_abs(data->proximity_input_dev,
					ABS_DISTANCE, 1);
			input_sync(data->proximity_input_dev);
		}

		enable_irq(data->irq);
	}
	data->proximity_enabled = value;
done:
	return count;
}
static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
			proximity_enable_show, proximity_enable_store);

static ssize_t proximity_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	struct gp2a_data *data = dev_get_drvdata(dev);
	static int count;		/*count for proximity average */

	int D2_data = 0;
	unsigned char get_D2_data[2] = { 0, };

	mutex_lock(&data->data_mutex);
	gp2a_i2c_read(data, 0x10, get_D2_data, sizeof(get_D2_data));
	mutex_unlock(&data->data_mutex);
	D2_data = (get_D2_data[1] << 8) | get_D2_data[0];

	data->average[count] = D2_data;
	count++;
	if (count == PROX_READ_NUM)
		count = 0;
	pr_debug("%s: D2_data = %d\n", __func__, D2_data);
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
	gp2a_i2c_read(data, PS_HT_LSB, get_D2_data,
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
		is_threshold_changed = 0;
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
	struct gp2a_data *data = dev_get_drvdata(dev);
	int thresh_hi = 0;
	unsigned char get_D2_data[2];

	msleep(20);
	gp2a_i2c_read(data, PS_HT_LSB, get_D2_data,
		sizeof(get_D2_data));
	thresh_hi = (get_D2_data[1] << 8) | get_D2_data[0];
	pr_info("%s: THRESHOLD = %d\n", __func__, thresh_hi);

	return sprintf(buf, "prox_threshold = %d\n", thresh_hi);
}

static ssize_t proximity_thresh_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	long thresh_value = 0;
	int err = 0;

	is_threshold_changed = 1;
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
	ret = gp2a_i2c_read(data, DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		pr_err("%s i2c err: %d\n", __func__, ret) ;
	D0_raw_data = (get_data[1] << 8) | get_data[0];	/* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2];	/* IR */

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", D0_raw_data, D1_raw_data);
}
static struct device_attribute dev_attr_light_sensor_lux =
	__ATTR(lux, S_IRUSR | S_IRGRP, lightsensor_raw_show, NULL);
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

	pr_info("%s, new_delay = %d, old_delay = %d", __func__, delay,
			data->light_delay);

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
	pr_debug("%s, %d value = %d\n", __func__, __LINE__, value);

	if (value != 0 && value != 1)
		goto done;

	mutex_lock(&data->light_mutex);

	if (data->light_enabled && !value) {
		cancel_delayed_work_sync(&data->light_work);
		lightsensor_onoff(0, data);
	}
	if (!data->light_enabled && value) {
		lightsensor_onoff(1, data);
		schedule_delayed_work(&data->light_work, msecs_to_jiffies(100));
	}

	data->light_enabled = value;


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
	wake_lock_timeout(&gp2a->prx_wake_lock, 3 * HZ);

	schedule_work(&gp2a->proximity_work);

	pr_debug("[PROXIMITY] IRQ_HANDLED.\n");
	return IRQ_HANDLED;
}


static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc = -EIO;
	struct gp2ap020_pdata *pdata = gp2a->pdata;
	int irq;

	pr_err("%s, start\n", __func__);

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
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"proximity_int", gp2a);
	if (unlikely(rc < 0)) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
				__func__, irq, pdata->p_out, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	gp2a->irq = irq;

	pr_err("%s, success\n", __func__);

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
	result = gpio_get_value_cansleep(gp2a->pdata->p_out);
	gp2a->proximity_detection = !result;

	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, result);
	pr_info("%s Proximity values = %d\n", __func__, result);
	input_sync(gp2a->proximity_input_dev);

	disable_irq(gp2a->irq);

	/*Software reset */
	value = 0x0C;
	ret = gp2a_i2c_write(gp2a, COMMAND1, &value);

	if (result == 0) {	/* detection = Falling Edge */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x23;
		else		/* High mode */
			value = 0x27;
		ret = gp2a_i2c_write(gp2a, COMMAND2, &value);
	} else {		/* none Detection */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x63;
		else		/* High mode */
			value = 0x67;
		ret = gp2a_i2c_write(gp2a, COMMAND2, &value);
	}

	enable_irq(gp2a->irq);

	value = 0xCC;
	ret = gp2a_i2c_write(gp2a, COMMAND1, &value);

	gp2a->prox_data = result;
	pr_debug("proximity = %d, lightsensor_mode=%d\n",
		result, gp2a->lightsensor_mode);
}

static int proximity_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	pr_info("%s, %d start\n", __func__, __LINE__);

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

	pr_info("%s, success\n", __func__);
done:
	return err;
}

static int light_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	pr_info("%s, %d start\n", __func__, __LINE__);

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s, error\n", __func__);
		err = -ENOMEM;
		goto done;
	}

	input_set_capability(dev, EV_REL, REL_MISC);

	dev->name = "light_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto done;
	}
	data->light_input_dev = dev;

	pr_info("%s, success\n", __func__);
done:
	return err;
}

static void gp2a_work_func_light(struct work_struct *work)
{
	struct gp2a_data *data = container_of((struct delayed_work *)work,
						struct gp2a_data, light_work);
	int adc;
	static int count;

	if (data == NULL || data->light_input_dev == NULL)
		return;

	adc = lightsensor_get_adc(data);

	/* detecting 0 after 3.6sec, set the register again. */
	if (adc == 0) {
		count++;
		if (count == 18) {
			lightsensor_onoff(1, data);
			count = 0;
			pr_info("%s: register reset\n", __func__);
		}
	} else
		count = 0;

	input_report_rel(data->light_input_dev, REL_MISC, adc + 1);
	input_sync(data->light_input_dev);

	if (data->light_enabled)
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(data->light_delay));
}

/*#if !defined(CONFIG_MACH_MS01_CHN_CTC)
static void gp2a_request_gpio(struct gp2ap020_pdata *pdata)
{
	int ret;
	ret = gpio_request(pdata->vdd_en, "vdd_en");
	if(ret)
		pr_err("[GP2A]%s: gpio request fail\n",__func__);
	gpio_tlmm_config(GPIO_CFG(pdata->vdd_en, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	if (ret) {
		pr_err("[GP2A]%s: unable to request vdd_en [%d]\n",
				__func__, pdata->vdd_en);
		return;
	}
	ret = gpio_direction_output(pdata->vdd_en, 1);
	if (ret)
		pr_err("[GP2A]%s: unable to set_direction for vdd_en [%d]\n",__func__, pdata->vdd_en);
}
#endif*/

#ifdef CONFIG_OF
/*device tree parsing */

static int gp2a_parse_dt(struct device *dev, struct gp2ap020_pdata *pdata)
{
	u32 version_flags;
	struct device_node *np = dev->of_node;
/*#if !defined(CONFIG_MACH_MS01_CHN_CTC)
	pdata->vdd_en= of_get_named_gpio(np, "gp2a,vdd_en-gpio", 0);
#endif*/
	pdata->p_out= of_get_named_gpio(np, "gp2a,p_out-gpio", 0);
	pdata->prox_cal_path = of_get_property(np, "gp2a,prox_cal_path",NULL);
	//pdata->vdd_2p85 = of_get_property(np, "gp2a,vdd_2p85",NULL);
	of_property_read_u32(np, "gp2a,version", &version_flags);
	pdata->version = (int)version_flags;
/*#if !defined(CONFIG_MACH_MS01_CHN_CTC)
	pr_err("gp2a_parse_dt complete, vdd_en:%d p_out:%d %s\n",pdata->vdd_en, pdata->p_out, pdata->prox_cal_path);
#endif*/
	return 0;
}
#else
static int gp2a_parse_dt(struct device *dev,
		struct gp2ap020_pdata *pdata)
	{
		return -ENODEV;
	}
#endif

static void sensor_power_on_vdd(struct gp2a_data *info, int onoff)
{
	int ret;
	if (info->vdd_2p85 == NULL) {
		info->vdd_2p85 =regulator_get(&info->client->dev, "8226_l19");
		if (IS_ERR(info->vdd_2p85)){
			pr_err("%s: regulator_get failed for 8226_l19\n", __func__);
			return ;
		}
		ret = regulator_set_voltage(info->vdd_2p85, 2850000, 2850000);
		if (ret)
			pr_err("%s: error vsensor_2p85 setting voltage ret=%d\n",__func__, ret);
	}
	if (info->vreg_2p8 == NULL) {
		info->vreg_2p8 =regulator_get(&info->client->dev, "8226_l15");
		if (IS_ERR(info->vreg_2p8)){
			pr_err("%s: regulator_get failed for 8226_l15\n", __func__);
			return ;
		}
		ret = regulator_set_voltage(info->vreg_2p8, 2800000, 2800000);
		if (ret)
			pr_err("%s: error vreg_2p8 setting voltage ret=%d\n",__func__, ret);
	}
#if defined(CONFIG_MACH_MS01_EUR_3G)
	if (info->lvs1_1p8 == NULL )
	{
		info->lvs1_1p8 = regulator_get(&info->client->dev, "8226_lvs1");
		if(!info->lvs1_1p8)
		{
			pr_err("%s: regulator_get for 8226_lvs1 failed\n", __func__);
			return ;
		}
	}
#endif
	if (onoff == 1) {
		ret = regulator_enable(info->vdd_2p85);
		if (ret)
			pr_err("%s: error enablinig regulator info->vdd_2p85\n", __func__);

		ret = regulator_enable(info->vreg_2p8);
		if (ret)
			pr_err("%s: error enablinig regulator info->vreg_2p8\n", __func__);
#if defined(CONFIG_MACH_MS01_EUR_3G)
		ret = regulator_enable(info->lvs1_1p8);
		if ( ret )
			 pr_err("%s: error enablinig regulator info->lvs1_1p8 \n", __func__);
#endif
	}
    else if (onoff == 0) {
		if (regulator_is_enabled(info->vdd_2p85)) {
			ret = regulator_disable(info->vdd_2p85);
			if (ret)
				pr_err("%s: error vdd_2p85 disabling regulator\n",__func__);
		}
		if (regulator_is_enabled(info->vreg_2p8)) {
			ret = regulator_disable(info->vreg_2p8);
			if (ret)
				pr_err("%s: error vreg_2p8 disabling regulator\n",__func__);
		}
#if defined(CONFIG_MACH_MS01_EUR_3G)
       ret = regulator_disable(info->lvs1_1p8);
       if (ret) {
            pr_err("%s: Failed to disable regulator lvs1_1p8.\n", __func__);
       }
#endif
	}
	msleep(30);
	return;
}

static int gp2a_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct gp2a_data *gp2a;
	struct gp2ap020_pdata *pdata ;
	u8 value = 0;
	int err = 0;

	pr_info("%s : gp2a_i2c_probe probe start!\n", __func__)

	if(client->dev.of_node) {
		pdata = devm_kzalloc (&client->dev ,
			sizeof(struct gp2ap020_pdata ), GFP_KERNEL);
		if(!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
		}
		err = gp2a_parse_dt(&client->dev, pdata);
		if(err) {
			pr_err("%s: gp2a_parse_dt error\n", __func__);
			goto done;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		err = -EINVAL;
		goto done;
	}
/*
	if (!pdata->power_on) {
		pr_err("%s: incomplete pdata!\n", __func__);
		err = -EINVAL;
		goto done;
	}
*/
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
/*#if !defined(CONFIG_MACH_MS01_CHN_CTC)
	gp2a_request_gpio(pdata);
#endif*/
	sensor_power_on_vdd(gp2a,1);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		goto done;
	}

	pr_err("GP2A poweron, functionality check pass !!!\n");
/*
	if (pdata->version) {
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
*/
	INIT_DELAYED_WORK(&gp2a->light_work, gp2a_work_func_light);
	INIT_WORK(&gp2a->proximity_work, gp2a_work_func_prox);

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
	err = gp2a_i2c_write(gp2a, (u8) (COMMAND1), &value);
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

	err = sensors_register(gp2a->light_sensor_device,
		gp2a, additional_light_attrs, "light_sensor");
	if (err) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_light;
	}

	err = sensors_register(gp2a->proximity_sensor_device,
		gp2a, additional_proximity_attrs, "proximity_sensor");
	if (err) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_proximity;
	}

#ifdef CONFIG_SENSOR_USE_SYMLINK
	err =  sensors_initialize_symlink(gp2a->proximity_input_dev);
	if (err) {
		pr_err("%s: cound not make proximity sensor symlink(%d).\n",
			__func__, err);
		goto err_sensors_initialize_symlink_proximity;
	}

	err =  sensors_initialize_symlink(gp2a->light_input_dev);
	if (err) {
		pr_err("%s: cound not make light sensor symlink(%d).\n",
			__func__, err);
		goto err_sensors_initialize_symlink_light;
	}
#endif

	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 1);

	pr_info("%s : probe success!\n", __func__);

	return 0;

#ifdef CONFIG_SENSOR_USE_SYMLINK
err_sensors_initialize_symlink_light:
	sensors_delete_symlink(gp2a->proximity_input_dev);
err_sensors_initialize_symlink_proximity:
#endif
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
	//if (pdata->power_on)
	//	pdata->power_on(false);
	kfree(gp2a);
done:
	return err;
}

#if 0
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
		gp2a->proximity_input_dev = NULL;
	}

	cancel_delayed_work_sync(&gp2a->light_work);
	flush_delayed_work(&gp2a->light_work);

	mutex_destroy(&gp2a->light_mutex);

	if (gp2a->light_input_dev != NULL) {
		sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
				&lightsensor_attribute_group);
		input_unregister_device(gp2a->light_input_dev);
		gp2a->light_input_dev = NULL;
	}

	mutex_destroy(&gp2a->data_mutex);
	kfree(gp2a);

	return 0;
}
#endif

static void gp2a_i2c_shutdown(struct i2c_client *client)
{
#if 0// temp for power off alarm not working issue. waiting vendor double check
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	if (unlikely(gp2a == NULL)) {
		pr_err("%s, gp2a_data is NULL!!!!!\n", __func__);
		return;
	}

	if (gp2a->proximity_input_dev != NULL) {
		sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			&proximity_attribute_group);
		input_unregister_device(gp2a->proximity_input_dev);
		gp2a->proximity_input_dev = NULL;
	}

	cancel_delayed_work_sync(&gp2a->light_work);
	flush_delayed_work(&gp2a->light_work);

	mutex_destroy(&gp2a->light_mutex);

	if (gp2a->light_input_dev != NULL) {
		sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
				&lightsensor_attribute_group);
		input_unregister_device(gp2a->light_input_dev);
		gp2a->light_input_dev = NULL;
	}

	mutex_destroy(&gp2a->data_mutex);
	kfree(gp2a);
 #endif
}

static int gp2a_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	u8 value;

	mutex_lock(&gp2a->light_mutex);
	if (gp2a->light_enabled)
		cancel_delayed_work_sync(&gp2a->light_work);

	mutex_unlock(&gp2a->light_mutex);
	if (gp2a->proximity_enabled) {
			/*enable_irq_wake(gp2a->irq); already wake irq enabled */
	} else {
		value = 0x00;	/*shutdown mode */
		gp2a_i2c_write(gp2a, (u8) (COMMAND1), &value);

		gpio_free(gp2a->pdata->p_out);
		sensor_power_on_vdd(gp2a,0);
		//if (gp2a->pdata->power_on)
		//	gp2a->pdata->power_on(0);
	}

	return 0;
}

static int gp2a_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);

	int ret = 0;

	if (gp2a->proximity_enabled) {
			/*enable_irq_wake(gp2a->irq); already wake irq enabled */
	} else {
		ret = gpio_request(gp2a->pdata->p_out, "gpio_proximity_out");
		if (ret)
			pr_err("%s gpio request %d err\n", __func__, gp2a->irq);
			sensor_power_on_vdd(gp2a,1);
		//if (gp2a->pdata->power_on)
		//	gp2a->pdata->power_on(1);
	}

	mutex_lock(&gp2a->light_mutex);

	if (gp2a->light_enabled)
		schedule_delayed_work(&gp2a->light_work, msecs_to_jiffies(100));

	mutex_unlock(&gp2a->light_mutex);

	return 0;
}

static const struct i2c_device_id gp2a_device_id[] = {
	{"gp2a030", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, gp2a_device_id);

static const struct dev_pm_ops gp2a_dev_pm_ops = {
	.suspend = gp2a_i2c_suspend,
	.resume = gp2a_i2c_resume,
};
#ifdef CONFIG_OF
static struct of_device_id gp2a_match_table[] = {
	{ .compatible = "gp2a,gp2a030",},
	{},
};
#else
#define gp2a_match_table NULL
#endif

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
			.name = "gp2a030",
			.owner = THIS_MODULE,
			.pm = &gp2a_dev_pm_ops,
			.of_match_table = gp2a_match_table,
	},
	.probe = gp2a_i2c_probe,
	.shutdown = gp2a_i2c_shutdown,
	.id_table = gp2a_device_id,
};

module_i2c_driver(gp2a_i2c_driver);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP030A00F");
MODULE_LICENSE("GPL");
