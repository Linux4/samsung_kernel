/* hscdtd007a_i2c.c
 *
 * GeoMagneticField device driver for I2C (HSCDTD007/HSCDTD008)
 *
 * Copyright (C) 2012 ALPS ELECTRIC CO., LTD. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/sensors_core.h>
#include <linux/err.h>


#define I2C_RETRY_DELAY  5
#define I2C_RETRIES      5

#define I2C_HSCD_ADDR    (0x0c)    /* 000 1100    */
#define I2C_BUS_NUMBER   8

#define HSCD_DRIVER_NAME "hscd_i2c"
#define CHIP_DEV_NAME "HSCDTD008A"
#define CHIP_DEV_VENDOR "ALPS"

#undef ALPS_DEBUG
#define alps_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)
#define alps_errmsg(str, args...) pr_err("%s: " str, __func__, ##args)
#define alps_info(str, args...) pr_info("%s: " str, __func__, ##args)


#define HSCD_STB         0x0C
#define HSCD_XOUT        0x10
#define HSCD_YOUT        0x12
#define HSCD_ZOUT        0x14
#define HSCD_XOUT_H      0x11
#define HSCD_XOUT_L      0x10
#define HSCD_YOUT_H      0x13
#define HSCD_YOUT_L      0x12
#define HSCD_ZOUT_H      0x15
#define HSCD_ZOUT_L      0x14

#define HSCD_STATUS      0x18
#define HSCD_CTRL1       0x1b
#define HSCD_CTRL2       0x1c
#define HSCD_CTRL3       0x1d
#define HSCD_CTRL4       0x1e

/* hscd chip id */
#define DEVICE_ID	0x49
/* hscd magnetic registers */
#define WHO_AM_I	0x0F

#define RETRY_COUNT	5

static struct i2c_driver hscd_driver;
static struct i2c_client *this_client;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler;
#endif

struct hscd_power_data {
	struct regulator *regulator_vdd;
	struct regulator *regulator_vio;
};

static struct hscd_power_data hscd_power;

static atomic_t flgEna;
static atomic_t delay;
static atomic_t flgSuspend;
static int reset_cnt;


static int hscd_i2c_readm(char *rxData, int length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
	{
		.addr  = this_client->addr,
		.flags = 0,
		.len   = 1,
		.buf   = rxData,
	},
	{
		.addr  = this_client->addr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxData,
	},
	};

	do {
		err = i2c_transfer(this_client->adapter, msgs, 2);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&this_client->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int hscd_i2c_writem(char *txData, int length)
{
	int err;
	int tries = 0;
#ifdef ALPS_DEBUG
	int i;
#endif

	struct i2c_msg msg[] = {
	{
		.addr  = this_client->addr,
		.flags = 0,
		.len   = length,
		.buf   = txData,
	},
	};

#ifdef ALPS_DEBUG
	pr_debug("%s : ", __func__);
	for (i = 0; i < length; i++)	pr_debug("0X%02X, ", txData[i]);
	pr_debug("\n");
#endif

	do {
		err = i2c_transfer(this_client->adapter, msg, 1);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&this_client->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int hscd_power_on(void)
{
	int err = 0;

	alps_info("is called\n");

	if (hscd_power.regulator_vdd) {
		err = regulator_enable(hscd_power.regulator_vdd);
		if (err) {
			alps_errmsg("Couldn't enable VDD %d\n", err);
			return err;
		}
	}

	if (hscd_power.regulator_vio) {
		err = regulator_enable(hscd_power.regulator_vio);
		if (err) {
			alps_errmsg("Couldn't enable VIO %d\n", err);
			return err;
		}
	}

	msleep(60);
	return err;
}

/*
static int hscd_power_off(void)
{
	int err = 0;

	alps_info("is called\n");

	if (hscd_power.regulator_vdd) {
		err = regulator_disable(hscd_power.regulator_vdd);
		if (err) {
			alps_errmsg("Couldn't disable VDD %d\n", err);
			return err;
		}
	}
	if (hscd_power.regulator_vio) {
		err = regulator_disable(hscd_power.regulator_vio);
		if (err) {
			alps_errmsg("Couldn't disable VIO %d\n", err);
			return err;
		}
	}

	return err;
}
*/

int hscd_self_test_A(void)
{
	u8 sx[2], cr1[1];

	if (atomic_read(&flgSuspend) == 1)
		return -1;

	alps_info("is called\n");

	/* Control resister1 backup */
	cr1[0] = HSCD_CTRL1;
	if (hscd_i2c_readm(cr1, 1))
		return 1;
#ifdef ALPS_DEBUG
	else
		alps_dbgmsg("Control resister1 value, %02X\n", cr1[0]);
#endif
	mdelay(1);

	/* Move active mode (force state) */
	sx[0] = HSCD_CTRL1;
	sx[1] = 0x8A;
	if (hscd_i2c_writem(sx, 2))
		return 1;

	/* Get inital value of self-test-A register */
	sx[0] = HSCD_STB;
	hscd_i2c_readm(sx, 1);

	mdelay(1);

	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1))
		return 1;
#ifdef ALPS_DEBUG
	else
		alps_dbgmsg("Self test A register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0x55) {
		alps_errmsg("Err! self-test-A, initial value is %02X\n", sx[0]);
		return 2;
	}

	/* do self-test*/
	sx[0] = HSCD_CTRL3;
	sx[1] = 0x10;
	if (hscd_i2c_writem(sx, 2))
		return 1;
	mdelay(3);

	/* Get 1st value of self-test-A register */
	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1))
		return 1;
#ifdef ALPS_DEBUG
	else
		alps_dbgmsg("Self test register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0xAA) {
		alps_errmsg("Err! self-test, 1st value is %02X\n", sx[0]);
		return 3;
	}
	mdelay(3);

	/* Get 2nd value of self-test register */
	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1))
		return 1;
#ifdef ALPS_DEBUG
	else
		alps_dbgmsg("Self test register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0x55) {
		alps_errmsg("Err! self-test, 2nd value is %02X\n", sx[0]);
		return 4;
	}

	/* Resume */
	sx[0] = HSCD_CTRL1;
	sx[1] = cr1[0];
	if (hscd_i2c_writem(sx, 2))
		return 1;

	return 0;
}

int hscd_self_test_B(void)
{
    if (atomic_read(&flgSuspend) == 1)
		return -1;
	alps_info("is called\n");
    return 0;
}

void hscd_activate(int flgatm, int flg, int dtime)
{
	u8 buf[2];

	if (this_client == NULL)
		return;
	else if ((atomic_read(&delay) == dtime)
				&& (atomic_read(&flgEna) == flg)
				&& (flgatm == 1))
		return;

	alps_info("is called\n");

	if (flg != 0) {
		buf[0] = HSCD_CTRL3;	/* Soft Reset */
		buf[1] = 0x80;
		hscd_i2c_writem(buf, 2);
		mdelay(5);

		buf[0] = HSCD_CTRL4;	/* 15 bit signed value */
		buf[1] = 0x90;
		hscd_i2c_writem(buf, 2);
		mdelay(1);

		if (dtime <=  20)
			buf[1] = (3 << 3);	/* 100Hz- 10msec */
		else if (dtime <=  70)
			buf[1] = (2 << 3);	/* 20Hz- 50msec */
		else
			buf[1] = (1 << 3);	/* 10Hz-100msec */

		buf[0]  = HSCD_CTRL1;
		buf[1] |= (1 << 7);
		hscd_i2c_writem(buf, 2);
		mdelay(1);
	} else {
		buf[0] = HSCD_CTRL1;	/* Sleep mode */
		buf[1] = 0;
		hscd_i2c_writem(buf, 2);
		mdelay(1);
	}

	if (flgatm) {
		atomic_set(&flgEna, flg);
		atomic_set(&delay, dtime);
	}
}

int hscd_get_magnetic_field_data(int *xyz)
{
	int err = -1;
	int i;
	u8 sx[6] = {0, };

	if (atomic_read(&flgSuspend) == 1)
		return err;

	sx[0] = HSCD_XOUT;

	err = hscd_i2c_readm(sx, 6);
	if (err < 0) {
		alps_errmsg("Fail to read data from i2c\n");
		return err;
	}

	for (i=0; i<3; i++) {
		xyz[i] = (int) ((short)((sx[2*i + 1] << 8) | (sx[2*i])));
	}

	if ((xyz[0] == 0) && (xyz[1] == 0) && (xyz[2] == 0)) {
		reset_cnt++;
		if (reset_cnt == RETRY_COUNT) {
			alps_info("Raw data seems wrong. Reset...........\n");
			hscd_activate(0, atomic_read(&flgEna), atomic_read(&delay));
			reset_cnt = 0;
		}
	} else {
		reset_cnt = 0;
	}

#ifdef ALPS_DEBUG
	/*** DEBUG OUTPUT - REMOVE ***/
	alps_dbgmsg("x = %d, y = %d, z = %d\n", xyz[0], xyz[1], xyz[2]);
	/*** <end> DEBUG OUTPUT - REMOVE ***/
#endif

	return err;
}

/*
void hscd_register_init(void)
{
	u8  buf[2];

	alps_info("is called\n");

	buf[0] = HSCD_CTRL3;
	buf[1] = 0x80;
	hscd_i2c_writem(buf, 2);

	mdelay(5);

	atomic_set(&delay, 100);
	hscd_activate(0, 1, atomic_read(&delay));
	hscd_get_magnetic_field_data(v);

	alps_dbgmsg("x = %d, y = %d, z = %d\n", v[0], v[1], v[2]);

	hscd_activate(0, 0, atomic_read(&delay));
}
*/

static ssize_t selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result1, result2;

	if (!atomic_read(&flgEna))
		hscd_power_on();

	result1 = hscd_self_test_A();
	result2 = hscd_self_test_B();

	/*
	if (!atomic_read(&flgEna))
		hscd_power_off();
	*/

	if (result1 == 0)
		result1 = 1;
	else
		result1 = 0;

	if (result2 == 0)
		result2 = 1;
	else
		result2 = 0;

	alps_info("result, A = %d, B = %d\n", result1, result2);

	return snprintf(buf, PAGE_SIZE, "%d, %d\n", result1, result2);
}

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result;

	if (!atomic_read(&flgEna))
		hscd_power_on();

	result = hscd_self_test_B();

	/*
	if (!atomic_read(&flgEna))
		hscd_power_off();
	*/

	if (result == 0)
		result = 1;
	else
		result = 0;

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", result, 0);
}

static ssize_t dac_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", 0, 0, 0);
}

static ssize_t adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int data[3];

	if (!atomic_read(&flgEna))
		hscd_activate(0, 1, 100);

	msleep(20);

	hscd_get_magnetic_field_data(data);
	alps_dbgmsg("x = %d, y = %d, z = %d\n", data[0], data[1], data[2]);

	if (!atomic_read(&flgEna))
		hscd_activate(0, 0, 100);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
					data[0], data[1], data[2]);
}

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}

static ssize_t vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}


static ssize_t mag_raw_data_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int xyz[3] = {0, };

	alps_dbgmsg("is called\n");

	if (!atomic_read(&flgEna))
		hscd_power_on();

	hscd_get_magnetic_field_data(xyz);

	/*
	if (!atomic_read(&flgEna))
		hscd_power_off();
	*/

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
					xyz[0], xyz[1], xyz[2]);
}

static DEVICE_ATTR(selftest, S_IRUGO | S_IWUSR | S_IWGRP,
	selftest_show, NULL);
static DEVICE_ATTR(status, S_IRUGO | S_IWUSR | S_IWGRP,
	status_show, NULL);
static DEVICE_ATTR(dac, S_IRUGO | S_IWUSR | S_IWGRP,
	dac_show, NULL);
static DEVICE_ATTR(adc, S_IRUGO | S_IWUSR | S_IWGRP,
	adc_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO | S_IWUSR | S_IWGRP,
	mag_raw_data_read, NULL);
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR | S_IWGRP, name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO | S_IWUSR | S_IWGRP, vendor_show, NULL);

static struct device_attribute *magnetic_attrs[] = {
	&dev_attr_selftest,
	&dev_attr_status,
	&dev_attr_dac,
	&dev_attr_adc,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_raw_data,
	NULL,
};

static int hscd_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *magnetic_device = NULL;

	alps_info("is called\n");

	this_client = client;
	reset_cnt = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*
	client_hscd = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!client_hscd) {
		dev_err(&client->adapter->dev,
			"failed to allocate memory for module data\n");
		return -ENOMEM;
	}
	*/

	hscd_power.regulator_vdd = hscd_power.regulator_vio = NULL;
	hscd_power.regulator_vdd = regulator_get(&client->dev, "vdd_alps");
	if (IS_ERR(hscd_power.regulator_vdd)) {
		ret = PTR_ERR(hscd_power.regulator_vdd);
		hscd_power.regulator_vdd = NULL;
		alps_errmsg("Failed to get hscd_i2c_vdd %d\n", ret);
		goto err_setup_regulator;
	}

	hscd_power.regulator_vio = regulator_get(&client->dev, "vio_mag");
	if (IS_ERR(hscd_power.regulator_vio)) {
		ret = PTR_ERR(hscd_power.regulator_vio);
		hscd_power.regulator_vio = NULL;
		alps_errmsg("Failed to get hscd_i2c_vio %d\n", ret);
		goto err_setup_regulator;
	}

	hscd_power_on();

	/* read chip id */
	ret = i2c_smbus_read_byte_data(this_client, WHO_AM_I);
	alps_info("Device ID = 0x%x, Reading ID = 0x%x\n", DEVICE_ID, ret);

	if (ret == DEVICE_ID) /* Normal Operation */
		ret = 0;
	else {
		if (ret < 0)
			alps_errmsg("i2c for reading chip id failed\n");
		else {
			alps_errmsg("Device identification failed\n");
			ret = -ENODEV;
		}
		goto err_setup_regulator;
	}

	/*
	alps_info("Call hscd power off\n", __func__);
	hscd_power_off();
	*/

	sensors_register(magnetic_device, NULL, magnetic_attrs,
						"magnetic_sensor");

	atomic_set(&flgEna, 0);
	atomic_set(&delay, 100);

	alps_info("is Successful\n");

	return 0;

err_setup_regulator:
	if (hscd_power.regulator_vdd) {
		regulator_disable(hscd_power.regulator_vdd);
		regulator_put(hscd_power.regulator_vdd);
	}
	if (hscd_power.regulator_vio) {
		regulator_disable(hscd_power.regulator_vio);
		regulator_put(hscd_power.regulator_vio);
	}
exit:
	this_client = NULL;
	alps_errmsg("failed. (errno = %d)\n", ret);

	return ret;
}

static int __devexit hscd_remove(struct i2c_client *client)
{
	alps_info("is called\n");

	hscd_activate(0, 0, atomic_read(&delay));
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hscd_early_suspend_handler);
#endif

	if (hscd_power.regulator_vdd) {
		regulator_disable(hscd_power.regulator_vdd);
		regulator_put(hscd_power.regulator_vdd);
	}
	if (hscd_power.regulator_vio) {
		regulator_disable(hscd_power.regulator_vio);
		regulator_put(hscd_power.regulator_vio);
	}
    this_client = NULL;

	return 0;
}

static int hscd_suspend(struct i2c_client *client, pm_message_t mesg)
{
	alps_info("is called\n");

	atomic_set(&flgSuspend, 1);
	if (atomic_read(&flgEna))
		hscd_activate(0, 0, atomic_read(&delay));
	return 0;
}

static int hscd_resume(struct i2c_client *client)
{
	alps_info("is called\n");

	atomic_set(&flgSuspend, 0);
	if (atomic_read(&flgEna))
		hscd_activate(0, atomic_read(&flgEna), atomic_read(&delay));

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hscd_early_suspend(struct early_suspend *handler)
{
	alps_info("is called\n");

	hscd_suspend(this_client, PMSG_SUSPEND);
}

static void hscd_early_resume(struct early_suspend *handler)
{
	alps_info("is called\n");

	hscd_resume(this_client);
}
#endif

static const struct i2c_device_id ALPS_id[] = {
	{ HSCD_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver hscd_driver = {
	.probe    = hscd_probe,
	.remove   = hscd_remove,
	.id_table = ALPS_id,
	.driver   = {
		.name = HSCD_DRIVER_NAME,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend  = hscd_suspend,
	.resume   = hscd_resume,
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler = {
	.suspend = hscd_early_suspend,
	.resume  = hscd_early_resume,
};
#endif

static int __init hscd_init(void)
{
	alps_info("is called\n");

	return i2c_add_driver(&hscd_driver);
}

static void __exit hscd_exit(void)
{
	alps_info("is called\n");

	i2c_del_driver(&hscd_driver);
}

module_init(hscd_init);
module_exit(hscd_exit);

MODULE_DESCRIPTION("Alps HSCDTD Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
