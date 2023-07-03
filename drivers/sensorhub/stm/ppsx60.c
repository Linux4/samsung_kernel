/*
 * Copyright (C) 2015 Partron Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/file.h>
#include <linux/syscalls.h>
#include "ppsx60.h"

#define MODULE_NAME_HRM			"hrm_sensor"
#define MODULE_NAME_HRM_LED		"hrmled_sensor"
#define CHIP_NAME			"PPSX60"
#define VENDOR				"PARTRON"

static unsigned int reg_defaults[NUM_OF_PPG_REG] = {
	0x0,			/* 0x00_00 AFE_CONTROL0 : REG_READ[0]=1 */
	0x00008d,		/* 0x01_01 AFE_LED2STC : [15:0] */
	0x0000f1,		/* 0x02_02 AFE_LED2ENDC : [15:0] */
	0x000000,		/* 0x03_03 AFE_LED1LEDSTC : [15:0] */
	0x000078,		/* 0x04_04 AFE_LED1LEDENDC : [15:0] */
	0x00232f,		/* 0x05_05 AFE_ALED2STC_LED3STC : [15:0] */
	0x002393,		/* 0x06_06 AFE_ALED2ENDC_LED3ENDC : [15:0] */
	0x000014,		/* 0x07_07 AFE_LED1STC : [15:0] */
	0x000078,		/* 0x08_08 AFE_LED1ENDC : [15:0] */
	0x000079,		/* 0x09_09 AFE_LED2LEDSTC : [15:0] */
	0x0000f1,		/* 0x0a_10 AFE_LED2LEDENDC : [15:0] */
	0x0022b6,		/* 0x0b_11 AFE_ALED1STC : [15:0] */
	0x00231a,		/* 0x0c_12 AFE_ALED1ENDC : [15:0] */
	0x00239b,		/* 0x0d_13 AFE_LED2CONVST : [15:0] */
	0x002472,		/* 0x0e_14 AFE_LED2CONVEND : [15:0] */
	0x00247a,		/* 0x0f_15 AFE_ALED2CONVST_LED3CONVST : [15:0] */
	0x002551,		/* 0x10_16 AFE_ALED2CONVEND_LED3CONVEND : [15:0] */
	0x002559,		/* 0x11_17 AFE_LED1CONVST : [15:0] */
	0x002630,		/* 0x12_18 AFE_LED1CONVEND : [15:0] */
	0x002638,		/* 0x13_19 AFE_ALED1CONVST : [15:0] */
	0x00270f,		/* 0x14_20 AFE_ALED1CONVEND : [15:0] */
	0x002394,		/* 0x15_21 AFE_ADCRSTSTCT0 : [15:0] */
	0x00239a,		/* 0x16_22 AFE_ADCRSTENDCT0 : [15:0] */
	0x002473,		/* 0x17_23 AFE_ADCRSTSTCT1 : [15:0] */
	0x002479,		/* 0x18_24 AFE_ADCRSTENDCT1 : [15:0] */
	0x002552,		/* 0x19_25 AFE_ADCRSTSTCT2 : [15:0] */
	0x002558,		/* 0x1a_26 AFE_ADCRSTENDCT2 : [15:0] */
	0x002631,		/* 0x1b_27 AFE_ADCRSTSTCT3 : [15:0] */
	0x002637,		/* 0x1c_28 AFE_ADCRSTENDCT3 : [15:0] */
	0x00270F,		/* 0x1d_29 AFE_PRPCOUNT : [15:0] */
	0x000102,		/* 0x1e_30 AFE_CONTROL1 : TIMEREN[8]=1, NUMAV[3:0]=2 */
	0x0,			/* 0x1f_31 AFE_SPARE1 : */
	0x0,			/* 0x20_32 AFE_TIAGAIN : ENSEPGAIN[15]=0, TIA_CF_SEP[5:3]=5pF, TIA_GAIN_SEP[2:0]=50k */
	0x1,			/* 0x21_33 AFE_TIAAMBGAIN : TIA_CF[5:3]=5pF, TIA_GAIN[2:0]=50k */
	0x03ffc0,		/* 0x22_34 AFE_LEDCNTRL : ILED3[17:12]=0mA, ILED2[11:6]=0mA, ILED1[5:0]=0mA */
	0x124218,		/* 0x23_35 AFE_CONTROL2 : [17]=1, OSC_ENABLE[9]=1(internal), DYNAMIC[20][14][4][3], PWDN[1][0], [0x124218->0x20200]*/
	0x0,			/* 0x24_36 AFE_SPARE2 : */
	0x0,			/* 0x25_37 AFE_SPARE3 : */
	0x0,			/* 0x26_38 AFE_SPARE4 : */
	0x0,			/* 0x27_39 AFE_RESERVED1 : */
	0xc00000,		/* 0x28_40 AFE_RESERVED2 : FIX_BITS1[23:22]=3 pre-release Silicon fixed */
	0x200,		/* 0x29_41 AFE_ALARM : ENABLE_CLKOUT[9]=1(internal), CLKDIV_CLKOUT[4:1]=0 */
	0x0,			/* 0x2a_42 AFE_LED2VAL : IR_DATA[23:0], LED2 */
	0x0,			/* 0x2b_43 AFE_ALED2VAL_LED3VAL : RED_DATA[23:0], LED3 */
	0x0,			/* 0x2c_44 AFE_LED1VAL : GREEN_DATA[23:0], LED1 */
	0x0,			/* 0x2d_45 AFE_ALED1VAL : AMBIENT_DATA[23:0], NONE */
	0x0,			/* 0x2e_46 AFE_LED2ALED2VAL : Ignore register because LED3 is used[23:0] */
	0x0,			/* 0x2f_47 AFE_LED1ALED1VAL : [23:0] */
	0x0,			/* 0x30_48 AFE_DIAG : */
	0x0,			/* 0x31_49 AFE_CONTROL3 : [5], CLKDIV_EXIMODE[2:0] */
	0x0001b9,		/* 0x32_50 AFE_DPD1STC_PDNCYCLESTC : [15:0] */
	0x0021ee,		/* 0x33_51 AFE_DPD1ENDC_PDNCYCLEENDC : [15:0] */
	0,			/* 0x34_52 AFE_DPD2STC : */
	0,			/* 0x35_53 AFE_DPD2ENDC : */
	0x00231b,		/* 0x36_54 AFE_REFSTC_LED3LEDSTC : [15:0] */
	0x002393,		/* 0x37_55 AFE_REFENDC_LED3LEDENDC : [15:0] */
	0x0,			/* 0x38_56 AFE_RESERVED3 : */
	0x000005,		/* 0x39_57 AFE_CLK_DIV_REG : [2:0]=0 , ratio==1 */
	0x0,			/* 0x3a_58 AFE_DAC_SETTING_REG : [19][18:15][14][13:10][9][8:5][4][3:0] */
/*	0x0,			0x3a_59 AFE_PROX_L_THRESH_REG : */
/*	0x0,			0x3a_60 AFE_PROX_H_THRESH_REG : */
/*	0x0,			0x3a_61 AFE_PROX_SETTING_REG : */
/*	0x0,			0x3a_62 AFE_COUNTER_32K_REG : */
/*	0x0			0x3a_63 AFE_PROX_AVG_REG : */
};

/* I2C function */
static int ppsX60_i2c_write(struct ppsX60_device_data *device, u8 cmd, u32 val)
{
	u8 buffer[4] = {0, };
	int err = 0;
	int retry = 3;
	struct i2c_msg msg[1];
	struct i2c_client *client = device->hrm_i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	buffer[0] = cmd;
	buffer[1] = (val>>16) & 0xff;	/*H*/
	buffer[2] = (val>>8) & 0xff;	/*M*/
	buffer[3] = (val) & 0xff;		/* L*/

	msg->addr = client->addr;
	msg->flags = I2C_WRITE_ENABLE;
	msg->len = 4;
	msg->buf = buffer;

	while (retry--) {
		mutex_lock(&device->i2clock);
		err = i2c_transfer(client->adapter, msg, 1);	/*L M H*/
		mutex_unlock(&device->i2clock);
		if (err >= 0)
			return 0;
	}
	pr_err("%s - write transfer error\n", __func__);
	return -EIO;
}

static int ppsX60_i2c_read(struct ppsX60_device_data *device, u8 cmd, u32 *val)
{
	int err = 0;
	int retry = 3;
	struct i2c_client *client = device->hrm_i2c_client;
	struct i2c_msg msg[2];
	u8 buffer[3] = {0,};

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		msg[0].addr = client->addr;
		msg[0].flags = I2C_WRITE_ENABLE;
		msg[0].len = 1;
		msg[0].buf = &cmd;

		msg[1].addr = client->addr;
		msg[1].flags = I2C_READ_ENABLE;
		msg[1].len = 3;
		msg[1].buf = buffer;

		err = i2c_transfer(client->adapter, msg, 2);

		if (err >= 0) {
			*val = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
			return 0;
		}
	}
	pr_err("%s - read transfer error\n", __func__);
	return -EIO;
}

/* Device Control */
static int ppsX60_regulator_onoff(struct ppsX60_device_data *data, int onoff)
{
	int err;

	if (!data->hrm_en) {
		data->vdd_1p8 = regulator_get(NULL, "HRM_1.8V_AP");
		if (IS_ERR(data->vdd_1p8)) {
			pr_err("%s - regulator_get fail\n", __func__);
			goto err_1p8;
		}
	}

	if(!data->led_3p3_en) {
		data->vdd_3p3 = regulator_get(NULL, "VLED_3P3");
		if (IS_ERR(data->vdd_3p3)) {
			pr_err("%s - regulator_get fail\n", __func__);
			goto err_3p3;
		}
	}

	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == HRM_LDO_ON) {
		if (!data->hrm_en) {
			err = regulator_enable(data->vdd_1p8);
			if (err < 0)
				pr_err("%s enable vdd_1p8 fail\n", __func__);
		} else {
			gpio_set_value(data->hrm_en, HRM_LDO_ON);
		}
		if(!data->led_3p3_en) {
			err = regulator_enable(data->vdd_3p3);
			if (err < 0)
				pr_err("%s enable vdd_3p3 fail\n", __func__);
			msleep(10);
		}else {
			gpio_set_value(data->led_3p3_en, HRM_LDO_ON);
		}
		gpio_set_value(data->hrm_rst, HRM_LDO_ON);
	} else {
		if (!data->hrm_en) {
			regulator_disable(data->vdd_1p8);
		} else {
			gpio_set_value(data->hrm_en, HRM_LDO_OFF);
		}
		if(!data->led_3p3_en) {
			regulator_disable(data->vdd_3p3);
		}else {
			gpio_set_value(data->led_3p3_en, HRM_LDO_ON);
		}
		msleep(10);
		gpio_set_value(data->hrm_rst, HRM_LDO_OFF);
	}
	if (!data->hrm_en) {
		regulator_put(data->vdd_1p8);
	}
	if(!data->led_3p3_en) {
		regulator_put(data->vdd_3p3);
	}
	return 0;
err_3p3:
	if (!data->hrm_en) {
		regulator_put(data->vdd_1p8);
	}
err_1p8:
	return -ENODEV;
}

static int ppsX60_init_device(struct ppsX60_device_data *data)
{
	int err;
	u8 i;

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL2, 0x124218);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL2\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_RESERVED2, reg_defaults[AFE_RESERVED2]);
	if (err != 0) {
		pr_err("%s - error AFE_RESERVED2\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, 0x2);	/* timer reset*/
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}

	for(i = 1; i < 0x30; i++) {
		err = ppsX60_i2c_write(data, i, reg_defaults[i]);
		if (err != 0) {
			pr_err("%s - error 0x%x reg\n", __func__, i);
			return -EIO;
		}
	}
	err = ppsX60_i2c_write(data, AFE_LED3LEDSTC, reg_defaults[AFE_LED3LEDSTC]);
	if (err != 0) {
		pr_err("%s - error AFE_LED3LEDSTC\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_LED3LEDENDC, reg_defaults[AFE_LED3LEDENDC]);
	if (err != 0) {
		pr_err("%s - error AFE_LED3LEDENDC\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_CLK_DIV_REG, reg_defaults[AFE_CLK_DIV_REG]);
	if (err != 0) {
		pr_err("%s - error AFE_LED3LEDENDC\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, 0x0);	/*remove timer reset*/
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL1, reg_defaults[AFE_CONTROL1]);	/*enable timer*/
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL1\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_PDNCYCLESTC, reg_defaults[AFE_PDNCYCLESTC]);
	if (err != 0) {
		pr_err("%s - error AFE_PDNCYCLESTC\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_PDNCYCLEENDC, reg_defaults[AFE_PDNCYCLEENDC]);
	if (err != 0) {
		pr_err("%s - error AFE_PDNCYCLEENDC\n", __func__);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}
	pr_info("%s init done, err  = %d\n", __func__, err);

	return 0;
}

static int ppsX60_enable(struct ppsX60_device_data *data)
{
	int err;
	data->sample_cnt = 0;
	data->ir_sum = 0;
	data->r_sum = 0;

	mutex_lock(&data->activelock);
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error initializing I2C_WRITE_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL2, reg_defaults[AFE_CONTROL2]);
	if (err != 0) {
		pr_err("%s - error initializing AFE_CONTROL2!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error initializing I2C_READ_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	enable_irq(data->irq);
	mutex_unlock(&data->activelock);
	return 0;
}

static int ppsX60_disable(struct ppsX60_device_data *data)
{
	u8 err;

	mutex_lock(&data->activelock);
	disable_irq(data->irq);

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error initializing I2C_WRITE_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL2, 0x124218);
	if (err != 0) {
		pr_err("%s - error initializing AFE_CONTROL2!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error initializing I2C_READ_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	mutex_unlock(&data->activelock);

	return 0;
}

static int afeConvP32(int val)
{
	int ConvertTemp = 0;

	ConvertTemp = (val<<8);
	ConvertTemp = ConvertTemp/256;

	return (int)ConvertTemp;
}

static int ppsX60_read_data(struct ppsX60_device_data *device, u32 *r_data, int *c_data)
{
	u8 err;
	int ret = 0;

	if ((err = ppsX60_i2c_read(device, AFE_LED2VAL, &r_data[0])) != 0) {
		pr_err("%s ppsX60_i2c_read err=%d, IR=0x%02x\n",
			__func__, err, r_data[0]);
		return -EIO;
	}

	if ((err = ppsX60_i2c_read(device, AFE_LED3VAL, &r_data[1])) != 0) {
		pr_err("%s ppsX60_i2c_read err=%d, RED=0x%02x\n",
			__func__, err, r_data[1]);
		return -EIO;
	}

	if ((err = ppsX60_i2c_read(device, AFE_LED1VAL, &r_data[2])) != 0) {
		pr_err("%s ppsX60_i2c_read err=%d, GREEN=0x%02x\n",
			__func__, err, r_data[2]);
		return -EIO;
	}

	if ((err = ppsX60_i2c_read(device, AFE_ALED1VAL, &r_data[3])) != 0) {
		pr_err("%s ppsX60_i2c_read err=%d, AMBIENT=0x%02x\n",
			__func__, err, r_data[3]);
		return -EIO;
	}

	c_data[0] = afeConvP32((int)r_data[0]);
	c_data[1] = afeConvP32((int)r_data[1]);
	c_data[2] = afeConvP32((int)r_data[2]);
	c_data[3] = afeConvP32((int)r_data[3]);

	return ret;
}

void ppsX60_mode_enable(struct ppsX60_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		if (atomic_read(&data->hrm_is_enable)) {
			pr_err("%s - already enabled !!!\n", __func__);
			goto exit;
		}
		atomic_set(&data->hrm_is_enable, 1);
		err = ppsX60_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s ppsX60_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = ppsX60_init_device(data);
		if (err)
			pr_err("%s ppsX60_init device fail err = %d\n",
				__func__, err);
		err = ppsX60_enable(data);
		if (err != 0)
			pr_err("ppsX60_enable err : %d\n", err);
	} else {
		if (!atomic_read(&data->hrm_is_enable)) {
			pr_err("%s - already disabled !!!\n", __func__);
			goto exit;
		}
		atomic_set(&data->hrm_is_enable, 0);
		err = ppsX60_disable(data);
		if (err != 0)
			pr_err("ppsX60_disable err : %d\n", err);
			err = ppsX60_regulator_onoff(data, HRM_LDO_OFF);
			if (err < 0)
				pr_err("%s ppsX60_regulator_off fail err = %d\n",
					__func__, err);
	}
exit:
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

/* sysfs */
static ssize_t ppsX60_hrm_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&data->hrm_is_enable));
}

static ssize_t ppsX60_hrm_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ppsX60_mode_enable(data, new_value);
	return count;
}

static ssize_t ppsX60_hrm_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t ppsX60_hrm_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - ppsX60 hrm sensor delay was fixed as 10ms\n", __func__);
	return size;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	ppsX60_hrm_enable_show, ppsX60_hrm_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	ppsX60_hrm_poll_delay_show, ppsX60_hrm_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};


/* hrm led sysfs */
static ssize_t ppsX60_hrmled_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&data->hrm_is_enable));
}

static ssize_t ppsX60_hrmled_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1")) {
		new_value = 1;
		atomic_set(&data->isEnable_led, 1);
	} else if (sysfs_streq(buf, "0")) {
		new_value = 0;
		atomic_set(&data->isEnable_led, 0);
	} else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ppsX60_mode_enable(data, new_value);
	return count;
}

static ssize_t ppsX60_hrmled_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t ppsX60_hrmled_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - ppsX60 hrm sensor delay was fixed as 10ms\n", __func__);
	return size;
}

static struct device_attribute dev_attr_hrmled_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	ppsX60_hrmled_enable_show, ppsX60_hrmled_enable_store);

static struct device_attribute dev_attr_hrmled_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	ppsX60_hrmled_poll_delay_show, ppsX60_hrmled_poll_delay_store);

static struct attribute *hrmled_sysfs_attrs[] = {
	&dev_attr_hrmled_enable.attr,
	&dev_attr_hrmled_poll_delay.attr,
	NULL
};

static struct attribute_group hrmled_attribute_group = {
	.attrs = hrmled_sysfs_attrs,
};


static int ppsX60_set_led_current(struct ppsX60_device_data *data)
{
	int err;

	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_LEDCNTRL,  data->led_current);
	if (err != 0) {
		pr_err("%s - error AFE_LEDCNTRL\n", __func__);
		return -EIO;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}
	return 0;
}

/* test sysfs */
static int ppsX60_set_look_mode_ir(struct ppsX60_device_data *device)
{
	pr_info("%s - look mode ir = %u\n", __func__, device->look_mode_ir);
	return 0;
}

static int ppsX60_set_look_mode_red(struct ppsX60_device_data *device)
{
	pr_info("%s - look mode red = %u\n", __func__, device->look_mode_red);
	return 0;
}

static ssize_t ppsX60_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t ppsX60_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}


static ssize_t ppsX60_led_current_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	err = sscanf(buf, "%d", &data->led_current);
	if (err < 0){
		pr_info("ppsX60_%s - faile error = %x\n", __func__, err);
		return err;
	}
	pr_info("ppsX60_%s - AFE_LEDCNTRL = %x\n", __func__, data->led_current);
	err = ppsX60_set_led_current(data);
	if (err < 0)
		return err;
	return size;
}

static ssize_t ppsX60_led_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	pr_info("ppsX60_%s - led_current = %x\n", __func__, data->led_current);

	return snprintf(buf, PAGE_SIZE, "%d\n",  data->led_current);
}


static ssize_t look_mode_ir_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_ir);
	if (err < 0)
		return err;

	err = ppsX60_set_look_mode_ir(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_ir_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	pr_info("ppsX60_%s - look_mode_ir = %x\n", __func__, data->look_mode_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_ir);
}

static ssize_t look_mode_red_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_red);
	if (err < 0)
		return err;

	err = ppsX60_set_look_mode_red(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_red_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	pr_info("ppsX60_%s - look_mode_red = %x\n", __func__, data->look_mode_red);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_red);
}

static ssize_t int_pin_check(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int err = -1;
	int pin_state = -1;

	/* DEVICE Power-up */
	err = ppsX60_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("ppsX60_%s - regulator on fail\n", __func__);
		goto exit;
	}

	usleep_range(1000, 1100);
	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (pin_state) {
		pr_err("ppsX60_%s - INT pin state is high before INT clear\n", __func__);
		err = -1;
		ppsX60_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}

	pr_info("ppsX60_%s - Before INT clear %d\n", __func__, pin_state);

	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (!pin_state) {
		pr_err("ppsX60_%s - INT pin state is low after INT clear\n", __func__);
		err = -1;
		ppsX60_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}
	pr_info("ppsX60_%s - After INT clear %d\n", __func__, pin_state);

	err = ppsX60_regulator_onoff(data, HRM_LDO_OFF);
	if (err < 0)
		pr_err("ppsX60_%s - regulator off fail\n", __func__);

	pr_info("ppsX60_%s - success\n", __func__);
exit:
	return snprintf(buf, PAGE_SIZE, "%d\n", err);
}

static ssize_t ppsX60_read_reg_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	pr_info("ppsX60_%s - val=0x%06x\n", __func__, data->sysfslastreadval);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->sysfslastreadval);
}

/*echo 0x + reg > read_reg*/
static ssize_t ppsX60_read_reg_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	int err = -1;
	unsigned int cmd = 0;
	u32 val = 0;

	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		return size;
	}
	err = sscanf(buf, "%x", &cmd);
	if (err == 0) {
		pr_err("ppsX60_%s - sscanf fail\n", __func__);
		return size;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return size;
	}

	if ((err = ppsX60_i2c_read(data, cmd, &val)) != 0) {
		pr_err("%s ppsX60_i2c_read err=%d, val=0x%06x\n",
			__func__, err, val);
		return size;
	}
	data->sysfslastreadval = val;
	pr_info("ppsX60_%s - success, cmd=0x%02x val=0x%06x\n", __func__, cmd, data->sysfslastreadval);

	return size;
}
/*echo 0x00,0x000000 > read_reg*/
static ssize_t ppsX60_write_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	int err = -1;
	unsigned int cmd = 0;
	unsigned int val = 0;

	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		return size;
	}
	err = sscanf(buf, "%x, %x", &cmd, &val);
	if (err == 0) {
		pr_err("ppsX60_%s - sscanf fail %s\n", __func__,buf);
		return size;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return size;
	}

	err = ppsX60_i2c_write(data, cmd, val);
	if (err != 0) {
		pr_err("%s - error write\n", __func__);
		return size;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return size;
	}
	pr_info("ppsX60_%s - success, cmd=0x%02x, val=0x%06x\n", __func__, cmd, val);
	return size;
}

static ssize_t ppsX60_read_allreg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	char file_data[NUM_OF_PPG_REG*10];
	int fd;
	int i = 0, j = 0;
	mm_segment_t old_fs = get_fs();
	int err = -1;

	pr_info("ppsX60_%s\n", __func__);
	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		goto error_old_fs;
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("ppsX60_%s - error AFE_CONTROL0\n", __func__);
		goto error_old_fs;
	}

	for (i = 1; i < NUM_OF_PPG_REG; i++) {
		err = ppsX60_i2c_read(data, i, &reg_defaults[i]);
		if (err != 0) {
			pr_err("ppsX60_%s - error 0x%x reg\n", __func__, i);
			goto error_old_fs;
		}
		pr_info("ppsX60_%x 0x%x\n", i, reg_defaults[i]);
	}

	for (i = 1; i < NUM_OF_PPG_REG; i++) {
		j += sprintf(file_data + j, "%x ", i);
		j += sprintf(file_data + j, "%x\n", reg_defaults[i]);
	}
	sprintf(file_data + j, "%s\n", "Do not remove this line : For check end of file.");
	set_fs(KERNEL_DS);

	fd = sys_open(PPSX60_REGFILE_PATH, O_WRONLY|O_CREAT, S_IRUGO | S_IWUSR | S_IWGRP);
	if (fd >= 0) {
		sys_write(fd, file_data, strlen(file_data));
	}
	sys_close(fd);
error_old_fs:
	set_fs(old_fs);
	return snprintf(buf, PAGE_SIZE, "%s\n", __func__);
}

static ssize_t ppsX60_write_allreg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int fd;
	int err = -1;
	int line = 0, i = 0, j = 0, k = 0;
	int reg_flag = 1;
	char reg_add[2];
	char reg_data[6];
	char file_data_int[NUM_OF_PPG_REG*10];
	unsigned int reg[NUM_OF_PPG_REG];
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	pr_info("ppsX60_%s\n", __func__);

	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		goto error_old_fs;
	}

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("ppsX60_%s - error AFE_CONTROL0\n", __func__);
		goto error_old_fs;
	}
/*read file*/
	fd = sys_open(PPSX60_REGFILE_PATH, O_RDONLY, 0);
	if (fd >= 0) {
		while (sys_read(fd, &file_data_int[i], 1) == 1) {
			i++;
		}
		sys_close(fd);
	} else {
		goto error;
	}
/*parse data*/
	i = 0;
	for (line = 1; line < NUM_OF_PPG_REG; i++) {
		if (file_data_int[i] == 32) {
			reg_flag = 0;
			sscanf(reg_add, "%02x", &reg[line]);
			j = 0;
			reg_add[0] = 0;
			reg_add[1] = 0;
			continue;
		} else if (file_data_int[i] == 10) {
			sscanf(reg_data, "%06x", &reg_defaults[line]);
			pr_info("ppsX60_%x 0x%x\n", reg[line], reg_defaults[line]);
			for (k = 0; k < 6; k++)
				reg_data[k] = 0;
			reg_flag = 1;
			k = 0;
			line++;
			continue;
		}
		if (reg_flag) {
			reg_add[j] = file_data_int[i];
			j++;
		} else {
			reg_data[k] = file_data_int[i];
			k++;
		}
	}
/*write reg*/
	for (i = 1; i < NUM_OF_PPG_REG; i++) {
		err = ppsX60_i2c_write(data, i, reg_defaults[i]);
		if (err != 0) {
			pr_err("ppsX60_%s - error 0x%x reg\n", __func__, i);
			goto error;
		}
	}
error:
	sys_close(fd);
error_old_fs:
	set_fs(old_fs);
	return size;
}


static ssize_t ppsX60_hrm_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	u8 handle = 0;

	if (sysfs_streq(buf, "16")) /* ID_SAM_HRM */
		handle = 16;
	else if (sysfs_streq(buf, "17")) /* ID_AOSP_HRM */
		handle = 17;
	else if (sysfs_streq(buf, "18")) /* ID_HRM_RAW */
		handle = 18;
	else {
		pr_info("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	input_report_rel(data->hrm_input_dev, REL_MISC, handle);
	return size;
}

static ssize_t ppsX60_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	buf_len = strlen(buf) + 1;
	if (buf_len > PPS_LIB_VER)
		buf_len = PPS_LIB_VER;

	if (data->lib_ver != NULL)
		kfree(data->lib_ver);

	data->lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}
	strncpy(data->lib_ver, buf, buf_len);
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	return size;
}

static ssize_t ppsX60_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	if (data->lib_ver == NULL) {
		pr_info("%s - data->lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", data->lib_ver);
}

static ssize_t ppsX60_hrm_alc_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int err, new_value;

	if (!atomic_read(&data->hrm_is_enable)) {
		pr_err("ppsX60_%s - need to power on.\n", __func__);
		return -EIO;
	}

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("%s - en:%d\n", __func__, new_value);

	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_WRITE_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}

	if (new_value) {/*alc on*/
		err = ppsX60_i2c_write(data, AFE_LEDCNTRL, reg_defaults[AFE_LEDCNTRL]);
		if (err != 0) {
			pr_err("%s - error AFE_LEDCNTRL\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_TIAGAIN, reg_defaults[AFE_TIAGAIN]);
		if (err != 0) {
			pr_err("%s - error AFE_TIAGAIN\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_DAC_SETTING_REG, reg_defaults[AFE_DAC_SETTING_REG]);
		if (err != 0) {
			pr_err("%s - error AFE_DAC_SETTING_REG\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_TIAAMBGAIN, reg_defaults[AFE_TIAAMBGAIN]);
		if (err != 0) {
			pr_err("%s - error AFE_TIAAMBGAIN\n", __func__);
			return -EIO;
		}
	} else { /*alc off*/
		err = ppsX60_i2c_write(data, AFE_LEDCNTRL, 0x0);
		if (err != 0) {
			pr_err("%s - error AFE_LEDCNTRL\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_TIAGAIN, 0x800d);
		if (err != 0) {
			pr_err("%s - error AFE_LEDCNTRL\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_DAC_SETTING_REG, 0x0);
		if (err != 0) {
			pr_err("%s - error AFE_LEDCNTRL\n", __func__);
			return -EIO;
		}
		err = ppsX60_i2c_write(data, AFE_TIAAMBGAIN, 0xd);
		if (err != 0) {
			pr_err("%s - error AFE_LEDCNTRL\n", __func__);
			return -EIO;
		}
	}
	err = ppsX60_i2c_write(data, AFE_CONTROL0, I2C_READ_ENABLE);
	if (err != 0) {
		pr_err("%s - error AFE_CONTROL0\n", __func__);
		return -EIO;
	}
	return size;
}

static ssize_t ppsX60_hrm_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	int iErr = 0;

	iErr = kstrtoint(buf, 10, &data->hrm_threshold);
	if (iErr < 0) {
		pr_err("%s - kstrtoint failed.(%d)\n", __func__, iErr);
		return iErr;
	}

	pr_info("%s - threshold = %d\n", __func__, data->hrm_threshold);
	return size;
}

static ssize_t ppsX60_hrm_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);

	if (data->hrm_threshold) {
		pr_info("%s - threshold = %d\n", __func__, data->hrm_threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", data->hrm_threshold);
	} else {
		pr_info("%s - threshold = 0\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
}

static DEVICE_ATTR(name, S_IRUGO, ppsX60_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, ppsX60_vendor_show, NULL);
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR | S_IWGRP,
	ppsX60_led_current_show, ppsX60_led_current_store);
static DEVICE_ATTR(look_mode_ir, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_ir_show, look_mode_ir_store);
static DEVICE_ATTR(look_mode_red, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_red_show, look_mode_red_store);
static DEVICE_ATTR(int_pin_check, S_IRUGO, int_pin_check, NULL);
static DEVICE_ATTR(hrm_flush, S_IWUSR | S_IWGRP,
	NULL, ppsX60_hrm_flush_store);
static DEVICE_ATTR(lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	ppsX60_lib_ver_show, ppsX60_lib_ver_store);
static DEVICE_ATTR(read_reg, S_IRUGO | S_IWUSR | S_IWGRP,
	ppsX60_read_reg_get, ppsX60_read_reg_set);
static DEVICE_ATTR(write_reg, S_IWUSR | S_IWGRP, NULL, ppsX60_write_reg_store);
static DEVICE_ATTR(ctrl_allreg_file, S_IRUGO | S_IWUSR | S_IWGRP,
	ppsX60_read_allreg_show, ppsX60_write_allreg_store);
static DEVICE_ATTR(alc_enable, S_IWUSR | S_IWGRP,
	NULL, ppsX60_hrm_alc_enable_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
	ppsX60_hrm_threshold_show, ppsX60_hrm_threshold_store);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_led_current,
	&dev_attr_look_mode_ir,
	&dev_attr_look_mode_red,
	&dev_attr_int_pin_check,
	&dev_attr_read_reg,
	&dev_attr_write_reg,
	&dev_attr_ctrl_allreg_file,
	&dev_attr_hrm_flush,
	&dev_attr_lib_ver,
	&dev_attr_alc_enable,
	&dev_attr_threshold,
	NULL,
};

static ssize_t ppsX60_hrmled_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t ppsX60_hrmled_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t ppsX60_hrmled_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	u8 handle = 0;

	if (sysfs_streq(buf, "20")) /* HRM LED IR */
		handle = 20;
	else if (sysfs_streq(buf, "21")) /* HRM LED RED */
		handle = 21;
	else if (sysfs_streq(buf, "22")) /* HRM LED GREEN */
		handle = 22;
	else if (sysfs_streq(buf, "23")) /* HRM LED VIOLET */
		handle = 23;
	else {
		pr_info("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	input_report_rel(data->hrm_input_dev, REL_MISC, handle);
	return size;
}

static struct device_attribute dev_attr_hrmled_name =
		__ATTR(name, S_IRUGO, ppsX60_hrmled_name_show, NULL);
static struct device_attribute dev_attr_hrmled_vendor =
		__ATTR(vendor, S_IRUGO, ppsX60_hrmled_vendor_show, NULL);
static DEVICE_ATTR(hrmled_flush, S_IWUSR | S_IWGRP,
	NULL, ppsX60_hrmled_flush_store);

static struct device_attribute *hrmled_sensor_attrs[] = {
	&dev_attr_hrmled_name,
	&dev_attr_hrmled_vendor,
	&dev_attr_hrmled_flush,
	NULL,
};

irqreturn_t ppsX60_irq_handler(int irq, void *device)
{
	int err;
	struct ppsX60_device_data *data = device;
	u32 raw_data[4] = {0x00, };
	int complement_data[4] = {0x00, };

	err = ppsX60_read_data(data, raw_data, complement_data);
	if (err < 0)
		pr_err("ppsX60_read_data err : %d\n", err);

	if (err == 0) {
		if (atomic_read(&data->isEnable_led)) {
			input_report_rel(data->hrmled_input_dev, REL_X, complement_data[0] + 1); /* IR */
			input_report_rel(data->hrmled_input_dev, REL_Y, complement_data[1] + 1); /* RED */
			input_sync(data->hrmled_input_dev);
		} else {
			input_report_rel(data->hrm_input_dev, REL_X, complement_data[0] + 1); /* IR */
			input_report_rel(data->hrm_input_dev, REL_Y, complement_data[1] + 1); /* RED */
			input_report_rel(data->hrm_input_dev, REL_Z, complement_data[3] + 1); /* AMBIENT */
			input_sync(data->hrm_input_dev);
		}
	}

	return IRQ_HANDLED;
}


static int ppsX60_parse_dt(struct ppsX60_device_data *data,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(dNode,
		"ppsx60,hrm_int-gpio", 0, &flags);
	if (data->hrm_int < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}
	data->hrm_en = of_get_named_gpio_flags(dNode,
		"ppsx60,vdd_1p8_en", 0, &flags);
	if (data->hrm_en < 0) {
		pr_err("%s - get hrm_en has no en gpio\n", __func__);
		return -ENODEV;
	}
	data->led_3p3_en = of_get_named_gpio_flags(dNode,
		"ppsx60,led_3p3_en", 0, &flags);
	if (data->led_3p3_en < 0) {
		pr_err("%s - get led_3p3_en has no en gpio %d\n", __func__, data->led_3p3_en);
		data->led_3p3_en =0;
	}
	data->hrm_rst = of_get_named_gpio_flags(dNode,
		"ppsx60,hrm_rst-gpio", 0, &flags);
	if (data->hrm_rst < 0) {
		pr_err("%s - get hrm_rst error\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int ppsX60_gpio_setup(struct ppsX60_device_data *data)
{
	int errorno = -EIO;

	errorno = gpio_request(data->hrm_int, "hrm_int");
	if (errorno) {
		pr_err("%s - failed to request hrm_int\n", __func__);
		return errorno;
	}

	errorno = gpio_direction_input(data->hrm_int);
	if (errorno) {
		pr_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_gpio_direction_input;
	}

	data->irq = gpio_to_irq(data->hrm_int);

	errorno = request_threaded_irq(data->irq, NULL,
		ppsX60_irq_handler, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
		"hrm_sensor_irq", data);

	if (errorno < 0) {
		pr_err("%s - request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, data->irq, data->hrm_int, errorno);
		errorno = -ENODEV;
		goto err_request_irq;
	}

	disable_irq(data->irq);
	goto done;
err_request_irq:
err_gpio_direction_input:
	gpio_free(data->hrm_int);
done:
	return errorno;
}

int ppsX60_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = -ENODEV;

	struct ppsX60_device_data *data;

	/* check to make sure that the adapter supports I2C */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s - I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	/* allocate some memory for the device */
	data = kzalloc(sizeof(struct ppsX60_device_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}

	data->hrm_i2c_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);

	data->dev = &client->dev;
	pr_info("%s - start \n", __func__);

	err = ppsX60_parse_dt(data, &client->dev);
	if (err < 0) {
		pr_err("[SENSOR] %s - of_node error\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}

	err = gpio_request(data->hrm_en, "hrm_en");
	if (err) {
		pr_err("[SENSOR] %s failed to get gpio en\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}
	if(!data->led_3p3_en) {
		err = gpio_request(data->led_3p3_en, "led_3p3_en");
		if (err) {
			pr_err("[SENSOR] %s failed to get gpio led_3p3_en\n", __func__);
		}
	}

	gpio_direction_output(data->hrm_en, HRM_LDO_OFF);
	if(!data->led_3p3_en) {
		gpio_direction_output(data->led_3p3_en, HRM_LDO_OFF);
	}
	err = gpio_request(data->hrm_rst, "hrm_rst");
	if (err) {
		pr_err("[SENSOR] %s failed to get gpio rst\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}

	gpio_direction_output(data->hrm_rst, HRM_LDO_OFF);

	err = ppsX60_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("%s ppsX60_regulator_onoff fail(%d, %d)\n", __func__,
			err, HRM_LDO_ON);
		goto err_of_node;
	}
	usleep_range(1000, 1100);

	data->hrm_i2c_client->addr = PPSX60_SLAVE_ADDR;

	/* allocate input device */
	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_input_allocate_device;
	}

	input_set_drvdata(data->hrm_input_dev, data);
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_capability(data->hrm_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Z);

	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		input_free_device(data->hrm_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_input_register_device;
	}

	err = sensors_create_symlink(data->hrm_input_dev);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		pr_err("%s - could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group;
	}

	err = ppsX60_gpio_setup(data);
	if (err) {
		pr_err("%s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(data->dev, data, hrm_sensor_attrs,
			MODULE_NAME_HRM);
	if (err) {
		pr_err("%s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	data->hrmled_input_dev = input_allocate_device();
	if (!data->hrmled_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_hrmled_input_allocate_device;
	}

	input_set_drvdata(data->hrmled_input_dev, data);
	data->hrmled_input_dev->name = MODULE_NAME_HRM_LED;
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_MISC);

	err = input_register_device(data->hrmled_input_dev);
	if (err < 0) {
		input_free_device(data->hrmled_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_hrmled_input_register_device;
	}

	err = sensors_create_symlink(data->hrmled_input_dev);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_hrmled_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrmled_input_dev->dev.kobj,
				 &hrmled_attribute_group);
	if (err) {
		pr_err("%s - could not create sysfs group\n",
			__func__);
		goto err_hrmled_sysfs_create_group;
	}

	/* set sysfs for hrm led sensor */
	err = sensors_register(data->dev, data, hrmled_sensor_attrs,
			MODULE_NAME_HRM_LED);
	if (err) {
		pr_err("%s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrmled_sensor_register_failed;
	}

	err = ppsX60_init_device(data);
	if (err) {
		pr_err("%s ppsX60_init device fail err = %d", __func__, err);
		goto ppsX60_init_device_failed;
	}

	err = dev_set_drvdata(data->dev, data);
	if (err) {
		pr_err("%s dev_set_drvdata fail err = %d", __func__, err);
		goto dev_set_drvdata_failed;
	}

	err = ppsX60_regulator_onoff(data, HRM_LDO_OFF);
	if (err < 0) {
		pr_err("%s ppsX60_regulator_onoff fail(%d, %d)\n", __func__,
			err, HRM_LDO_OFF);
		goto dev_set_drvdata_failed;
	}

	pr_info("%s success\n", __func__);
	goto done;

dev_set_drvdata_failed:
ppsX60_init_device_failed:
	sensors_unregister(data->dev, hrm_sensor_attrs);

hrmled_sensor_register_failed:
err_hrmled_sysfs_create_group:
	sensors_remove_symlink(data->hrmled_input_dev);
err_hrmled_sensors_create_symlink:
	input_unregister_device(data->hrmled_input_dev);
err_hrmled_input_register_device:
err_hrmled_input_allocate_device:
	ppsX60_regulator_onoff(data, HRM_LDO_OFF);

hrm_sensor_register_failed:
	gpio_free(data->hrm_int);
err_setup_gpio:
err_sysfs_create_group:
	sensors_remove_symlink(data->hrm_input_dev);
err_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_input_register_device:
err_input_allocate_device:
	ppsX60_regulator_onoff(data, HRM_LDO_OFF);
err_of_node:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	kfree(data);
	pr_err("%s failed\n", __func__);
done:
	return err;
}

/*
 *  Remove function for this I2C driver.
 */
int ppsX60_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	free_irq(client->irq, NULL);
	return 0;
}

static void ppsX60_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
}

static int ppsX60_pm_suspend(struct device *dev)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	if (atomic_read(&data->hrm_is_enable)) {
		ppsX60_mode_enable(data, HRM_LDO_OFF);
		atomic_set(&data->is_suspend, 1);
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int ppsX60_pm_resume(struct device *dev)
{
	struct ppsX60_device_data *data = dev_get_drvdata(dev);
	if (atomic_read(&data->is_suspend)) {
		ppsX60_mode_enable(data, HRM_LDO_ON);
		atomic_set(&data->is_suspend, 0);
	}
	pr_info("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops ppsX60_pm_ops = {
	.suspend = ppsX60_pm_suspend,
	.resume = ppsX60_pm_resume
};

static struct of_device_id ppsX60_match_table[] = {
	{ .compatible = "ppsx60",},
	{},
};

static const struct i2c_device_id ppsX60_device_id[] = {
	{ "ppsx60_match_table", 0 },
	{ }
};
/* descriptor of the ppsX60 I2C driver */
static struct i2c_driver ppsX60_i2c_driver = {
	.driver = {
	    .name = CHIP_NAME,
	    .owner = THIS_MODULE,
	    .pm = &ppsX60_pm_ops,
	    .of_match_table = ppsX60_match_table,
	},
	.probe = ppsX60_probe,
	.shutdown = ppsX60_shutdown,
	.remove = ppsX60_remove,
	.id_table = ppsX60_device_id,
};

/* initialization and exit functions */
static int __init ppsX60_init(void)
{
	return i2c_add_driver(&ppsX60_i2c_driver);
}

static void __exit ppsX60_exit(void)
{
	i2c_del_driver(&ppsX60_i2c_driver);
}

module_init(ppsX60_init);
module_exit(ppsX60_exit);

MODULE_DESCRIPTION("ppsX60 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
