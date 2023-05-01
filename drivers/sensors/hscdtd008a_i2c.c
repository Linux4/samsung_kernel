/* hscdtd008a_i2c.c
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
#include <linux/input.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "sensors_head.h"

#define I2C_RETRY_DELAY  5
#define I2C_RETRIES      5

#define I2C_HSCD_ADDR    (0x0c)    /* 000 1100    */
#define I2C_BUS_NUMBER   4

#define HSCD_DRIVER_NAME "hscd_i2c"
#define HSCD_ID         0x1511

#define HSCD_STB         0x0C
#define HSCD_INFO         0x0D
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

#define CHIP_DEV_NAME	"HSCDTD008A"
#define CHIP_DEV_VENDOR	"ALPS"

static struct i2c_driver hscd_driver;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler;
#endif

static atomic_t flgEna;
static atomic_t delay;
static struct device *magnetic_device;
static atomic_t flgSuspend;
static int probe_done;

struct hscd_i2c_data {
	struct i2c_client *this_client;
	struct input_dev *input_dev;
	struct early_suspend early_suspend;
};
struct hscd_i2c_data *hscd_data;

static int hscd_i2c_readm(char *rxData, int length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr  = hscd_data->this_client->addr,
			.flags = 0,
			.len   = 1,
			.buf   = rxData,
		},
		{
			.addr  = hscd_data->this_client->addr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxData,
		},
	};

	do {
		err = i2c_transfer(hscd_data->this_client->adapter, msgs, 2);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&hscd_data->this_client->adapter->dev, "read transfer error\n");
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
			.addr  = hscd_data->this_client->addr,
			.flags = 0,
			.len   = length,
			.buf   = txData,
		},
	};

#ifdef ALPS_DEBUG
	printk("[HSCD] i2c_writem : ");
	for (i=0; i<length;i++) printk("0X%02X, ", txData[i]);
		printk("\n");
#endif

	do {
		err = i2c_transfer(hscd_data->this_client->adapter, msg, 1);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&hscd_data->this_client->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

int hscd_self_test_A(void)
{
	u8 sx[2], cr1[1];

	if (atomic_read(&flgSuspend) == 1) return -1;
	/* Control resister1 backup  */
	cr1[0] = HSCD_CTRL1;
	if (hscd_i2c_readm(cr1, 1)) return 1;
#ifdef ALPS_DEBUG
	else printk("[HSCD] Control resister1 value, %02X\n", cr1[0]);
#endif
	mdelay(1);

	/* Move active mode (force state)  */
	sx[0] = HSCD_CTRL1;
	sx[1] = 0x8A;
	if (hscd_i2c_writem(sx, 2)) return 1;

	/* Get inital value of self-test-A register  */
	sx[0] = HSCD_STB;
	hscd_i2c_readm(sx, 1);
	mdelay(1);
	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1)) return 1;
#ifdef ALPS_DEBUG
	else printk("[HSCD] self test A register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0x55) {
		printk("error: self-test-A, initial value is %02X\n", sx[0]);
		return 2;
	}

	/* do self-test*/
	sx[0] = HSCD_CTRL3;
	sx[1] = 0x10;
	if (hscd_i2c_writem(sx, 2)) return 1;
	mdelay(3);

	/* Get 1st value of self-test-A register  */
	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1)) return 1;
#ifdef ALPS_DEBUG
	else printk("[HSCD] self test register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0xAA) {
		printk("error: self-test, 1st value is %02X\n", sx[0]);
		return 3;
	}
	mdelay(3);

	/* Get 2nd value of self-test register  */
	sx[0] = HSCD_STB;
	if (hscd_i2c_readm(sx, 1)) return 1;
#ifdef ALPS_DEBUG
	else printk("[HSCD] self test  register value, %02X\n", sx[0]);
#endif
	if (sx[0] != 0x55) {
		printk("error: self-test, 2nd value is %02X\n", sx[0]);
		return 4;
	}

	/* Resume  */
	sx[0] = HSCD_CTRL1;
	sx[1] = cr1[0];
	if (hscd_i2c_writem(sx, 2)) return 1;

	return 0;
}

int hscd_self_test_B(void)
{
	if (atomic_read(&flgSuspend) == 1) return -1;
	return 0;
}

int hscd_get_magnetic_field_data(int *xyz)
{
	int err = -1;
	int i;
	u8 sx[6];

	if (atomic_read(&flgSuspend) == 1) return err;
	sx[0] = HSCD_XOUT;
	err = hscd_i2c_readm(sx, 6);
	if (err < 0) return err;
	for (i=0; i<3; i++) {
		xyz[i] = (int) ((short)((sx[2*i+1] << 8) | (sx[2*i])));
	}

#ifdef ALPS_DEBUG
	/*** DEBUG OUTPUT - REMOVE ***/
	printk("Mag_I2C, x:%d, y:%d, z:%d\n",xyz[0], xyz[1], xyz[2]);
	/*** <end> DEBUG OUTPUT - REMOVE ***/
#endif

	return err;
}

void hscd_activate(int flgatm, int flg, int dtime)
{
	u8 buf[2];

	if (flg != 0) flg = 1;

	if (flg) {
		buf[0] = HSCD_CTRL4;                       // 15 bit signed value
		buf[1] = 0x90;
		hscd_i2c_writem(buf, 2);
	}
	mdelay(1);

	if (dtime <=  20) buf[1] = (3<<3);        // 100Hz- 10msec
	else if (dtime <=  70) buf[1] = (2<<3);        //  20Hz- 50msec
	else                   buf[1] = (1<<3);        //  10Hz-100msec
	buf[0]  = HSCD_CTRL1;
	buf[1] |= (flg<<7);
	hscd_i2c_writem(buf, 2);
	mdelay(3);

	if (flgatm) {
		atomic_set(&flgEna, flg);
		atomic_set(&delay, dtime);
	}
}

/*
static void hscd_register_init(void)
{
	int v[3];
	u8  buf[2];

#ifdef ALPS_DEBUG
	printk("[HSCD] register_init\n");
#endif

	buf[0] = HSCD_CTRL3;
	buf[1] = 0x80;
	hscd_i2c_writem(buf, 2);
	mdelay(5);

	atomic_set(&delay, 100);
	hscd_activate(0, 1, atomic_read(&delay));
	hscd_get_magnetic_field_data(v);
	printk("[HSCD] x:%d y:%d z:%d\n", v[0], v[1], v[2]);
	hscd_activate(0, 0, atomic_read(&delay));
}
*/

int hscd_open(void)
{
	return probe_done;
}
EXPORT_SYMBOL(hscd_open);

static ssize_t selttest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result1 = hscd_self_test_A();
	int result2 = hscd_self_test_B();
	if (result1 == 0)
		result1 = 1;
	else
		result1 = 0;
	if (result2 == 0)
		result2 = 1;
	else
		result2 = 0;
#ifdef ALPS_DEBUG
	pr_info("Selftest Result is %d, %d\n", result1, result2);
#endif
	return snprintf(buf, PAGE_SIZE, "%d, %d\n", result1, result2);
}
static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result = hscd_self_test_B();
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
	int d[3];
	if( atomic_read(&flgEna) == 0)
	{
		hscd_activate(0, 1, 10);
		mdelay(15);
	}
	hscd_get_magnetic_field_data(d);
	if( atomic_read(&flgEna) == 0)
	{
		hscd_activate(0, 1, atomic_read(&delay));
	}
#ifdef ALPS_DEBUG
	pr_info("[HSCD] x:%d y:%d z:%d\n", d[0], d[1], d[2]);
#endif
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", d[0], d[1], d[2]);
}
static ssize_t name_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}
static ssize_t vendor_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}

static DEVICE_ATTR(selftest, 0664, selttest_show, NULL);
static DEVICE_ATTR(status, 0664, status_show, NULL);
static DEVICE_ATTR(dac, 0644, dac_show, NULL);
static DEVICE_ATTR(adc, 0644, adc_show, NULL);
static DEVICE_ATTR(name, 0440, name_read, NULL);
static DEVICE_ATTR(vendor, 0440, vendor_read, NULL);
static DEVICE_ATTR(raw_data, 0644, adc_show, NULL);

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
static ssize_t mag_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result = hscd_self_test_A();
	if (result == 0)
		result = 1;
	else
		result = 0;
	return snprintf(buf, PAGE_SIZE, "%d\n", result);
}
static struct device_attribute dev_attr_enable =
__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		mag_enable_show, NULL);
static struct attribute *mag_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	NULL
};
static struct attribute_group mag_attribute_group = {
	.attrs = mag_sysfs_attrs,
};

static int hscd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int d[3];
	int err = 0;
	u8	sx[2];
	int	ID;
	u8  buf[2];
	/*struct input_dev *input_dev;*/
	struct hscd_i2c_data *hscd;

	pr_info("[HSCD] probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		return -ENOMEM;
	}

	hscd = kzalloc(sizeof(struct hscd_i2c_data), GFP_KERNEL);
	if (!hscd) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, hscd->this_client);

	hscd->this_client = client;
	hscd_data = hscd;

	dev_info(&client->adapter->dev, "detected HSCDTD007/008 geomagnetic field sensor\n");

	/*software reset*/
	buf[0] = HSCD_CTRL3;
	buf[1] = 0x80;
	hscd_i2c_writem(buf, 2);
	mdelay(5);

	hscd_activate(0, 1, atomic_read(&delay));
	err = hscd_get_magnetic_field_data(d);
	if (err) {
		pr_info("[HSCD] Read Data error\n");
		goto err_data_read;
	}
#ifdef ALPS_DEBUG
	pr_info("[HSCD] x:%d y:%d z:%d\n", d[0], d[1], d[2]);
#endif
	hscd_activate(0, 0, atomic_read(&delay));

	/* Sensor Info check */
	sx[0] = HSCD_INFO;
	err = hscd_i2c_readm(sx, 2);
	if (err < 0) {
		pr_info("[HSCD] read error\n");
		err = -ENOMEM;
		goto err_sensor_info_check;
	} else {
		ID = (int)((sx[1]<<8) | sx[0]);
		pr_info("[HSCD] chip ID = 0x%X\n", ID);
		if (ID != HSCD_ID) {
			pr_info("[HSCD] chip ID is error.\n");
			err = -EIO;
			goto err_sensor_info_check;
		}
	}

	hscd->input_dev = input_allocate_device();
	if (!hscd->input_dev) {
		pr_info("%s: could not allocate input device\n", __func__);
		err = -ENOMEM;
		goto err_input_allocate_device;
	}
	err = input_register_device(hscd->input_dev);
	if (err < 0) {
		pr_info("%s: could not register input device\n", __func__);
		goto err_input_register_device;
	}
	hscd->input_dev->name = "magnetic_sensor";
	err = sysfs_create_group(&hscd->input_dev->dev.kobj,
		&mag_attribute_group);
	if (err) {
		pr_info("Creating hscd attribute group failed");
		goto error_device;
	}

	/* symlink */
#ifdef CONFIG_SENSOR_USE_SYMLINK
	err =  sensors_initialize_symlink(hscd->input_dev);
	if (err) {
		pr_err("%s: cound not make hscd sensor symlink(%d).\n",
			__func__, err);
		goto err_hscd_sensor_initialize_symlink;
	}
#endif

	err = sensors_register(magnetic_device, NULL,
		magnetic_attrs, "magnetic_sensor");
	if (err < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto exit_hscd_sensors_register;
	}

	probe_done = PROBE_SUCCESS;
	return 0;
exit_hscd_sensors_register:
#ifdef CONFIG_SENSOR_USE_SYMLINK
	sensors_delete_symlink(hscd->input_dev);
err_hscd_sensor_initialize_symlink:
#endif
	sysfs_remove_group(&hscd->input_dev->dev.kobj,
		&mag_attribute_group);
error_device:
	input_unregister_device(hscd->input_dev);
err_input_register_device:
	input_free_device(hscd->input_dev);
err_input_allocate_device:
err_sensor_info_check:
err_data_read:
	kfree(hscd);
exit_alloc_data_failed:
	return err;
}

static int __devexit hscd_remove(struct i2c_client *client)
{
	printk("[HSCD] remove\n");
	hscd_activate(0, 0, atomic_read(&delay));
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hscd_early_suspend_handler);
#endif
	kfree(hscd_data->this_client);
	return 0;
}

static int hscd_suspend(struct i2c_client *client, pm_message_t mesg)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] suspend\n");
#endif
	atomic_set(&flgSuspend, 1);
	hscd_activate(0, 0, atomic_read(&delay));
	return 0;
}

static int hscd_resume(struct i2c_client *client)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] resume\n");
#endif
	atomic_set(&flgSuspend, 0);
	hscd_activate(0, atomic_read(&flgEna), atomic_read(&delay));
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hscd_early_suspend(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] early_suspend\n");
#endif
	hscd_suspend(hscd_data->this_client, PMSG_SUSPEND);
}

static void hscd_early_resume(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] early_resume\n");
#endif
	hscd_resume(hscd_data->this_client);
}
#endif

static const struct i2c_device_id ALPS_id[] = {
	{ HSCD_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver hscd_driver = {
	.probe = hscd_probe,
	.remove = hscd_remove,
	.id_table = ALPS_id,
	.driver = {
	.name = HSCD_DRIVER_NAME,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = hscd_suspend,
	.resume = hscd_resume,
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler = {
	.suspend = hscd_early_suspend,
	.resume = hscd_early_resume,
};
#endif

static int __init hscd_init(void)
{
	int rc;
	probe_done = PROBE_FAIL;

#ifdef ALPS_DEBUG
	printk("[HSCD] init\n");
#endif
	atomic_set(&flgEna, 0);
	atomic_set(&delay, 200);
	atomic_set(&flgSuspend, 0);

	rc = i2c_add_driver(&hscd_driver);
	if (rc != 0) {
		printk("can't add i2c driver\n");
		rc = -ENOTSUPP;
		return rc;
	}

	return rc;
}

static void __exit hscd_exit(void)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] exit\n");
#endif
	i2c_del_driver(&hscd_driver);
}

module_init(hscd_init);
module_exit(hscd_exit);

EXPORT_SYMBOL(hscd_self_test_A);
EXPORT_SYMBOL(hscd_self_test_B);
EXPORT_SYMBOL(hscd_get_magnetic_field_data);
EXPORT_SYMBOL(hscd_activate);

MODULE_DESCRIPTION("Alps HSCDTD Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
