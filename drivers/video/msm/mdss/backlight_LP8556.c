/*
 *	LP8556 Backlight IC driver.
 *
 *	Author: Raghu Ballappa Bankapur <rb.bankapur@samsung.com>
 *	Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/lcd.h>
#include <linux/leds.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "backlight_LP8556.h"




//static struct bl_data *p_bl_data;

struct bl_data {
	struct i2c_client *client;
};

static struct bl_data *p_bl_data;

unsigned long last_bl_Algorithm = 0xffff;

static struct i2c_client *g_client;

int bl_I2cRead8(u8 reg, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	u8 regaddr = reg;
	u8 data;

	if (!p_bl_data) {
		pr_err("%s p_cmc624_data is NULL\n", __func__);
		return -ENODEV;
	}
	g_client = p_bl_data->client;

	if ((g_client == NULL)) {
		pr_err("%s g_client is NULL\n", __func__);
		return -ENODEV;
	}

	if (!g_client->adapter) {
		pr_err("%s g_client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

	if (regaddr == 0x0001) {
		*val = last_bl_Algorithm;
		return 0;
	}

	msg[0].addr = g_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &regaddr;
	msg[1].addr = g_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &data;
	err = i2c_transfer(g_client->adapter, &msg[0], 2);

	if (err >= 0) {
		*val = data & 0xFF;
		return 0;
	}
	/* add by inter.park */
	pr_err("%s %d i2c transfer error: %d\n", __func__, __LINE__, err);

	return err;
}


bool bl_I2cWrite8(unsigned char Addr, unsigned long Data)
{
	int err = -1000;
	struct i2c_msg msg[1];
	unsigned char data[3];

	if (!p_bl_data) {
		pr_info("p_bl_data is NULL\n");
		return -ENODEV;
	}

	g_client = p_bl_data->client;

	if ((g_client == NULL)) {
		pr_info("cmc624_I2cWrite16 g_client is NULL\n");
		return -ENODEV;
	}

	if (!g_client->adapter) {
		pr_info("cmc624_I2cWrite16 g_client->adapter is NULL\n");
		return -ENODEV;
	}

	data[0] = Addr;
	data[1] = (Data) & 0xFF;
	msg->addr = g_client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	err = i2c_transfer(g_client->adapter, msg, 1);

	if (err >= 0) {
		pr_info("%s %d i2c transfer OK\n", __func__, __LINE__);
		return 0;
	}

	pr_info("%s i2c transfer error:%d(a:%d)\n", __func__, err, Addr);
	return err;
}

static int lp8556_input_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct bl_data *data;

	pr_info("%s +\n", __func__);
	pr_info("==============================\n");
	pr_info("bl attach START!!!\n");
	pr_info("==============================\n");

	data = kzalloc(sizeof(struct bl_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	
	dev_err(&client->dev, "memory allocation successful\n");

	data->client = client;
	i2c_set_clientdata(client, data);

	dev_info(&client->dev, "bl i2c probe success!!!\n");

	p_bl_data = data;

	pr_info("==============================\n");
	pr_info("bl SYSFS INIT!!!\n");
	pr_info("==============================\n");
	pr_info("%s -\n", __func__);

	return 0;
}

static int __devexit lp8556_input_remove(struct i2c_client *client)
{
	struct bl_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	kfree(data);

	dev_info(&client->dev, "bl i2c remove success!!!\n");

	return 0;
}


static const struct i2c_device_id lp8556_input_id[] = {
	{"backlight_lp8556", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lp8556_input_id);

#ifdef CONFIG_OF
static struct of_device_id lp8556_match_table[] = {
	{ .compatible = "backlight_lp8556",},
	{},
};
#else
#define mpu6500_match_table NULL
#endif
static struct i2c_driver backlight_lp8556_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "BACKLIGHT_LP8556_DRIVER",
		   .of_match_table = lp8556_match_table,
		   },
	.class = I2C_CLASS_HWMON,
	.id_table = lp8556_input_id,
	.probe = lp8556_input_probe,
	.remove = __devexit_p(lp8556_input_remove),
};

int samsung_bl_init(void)
{
	int ret;

	pr_err("<bl_i2c_driver Add START>\n");
	ret = i2c_add_driver(&backlight_lp8556_driver);
	pr_err("bl_init Return value  (%d)\n", ret);
	pr_err("<bl_i2c_driver Add END>\n");

	return 0;

}

MODULE_DESCRIPTION("LP8556 driver");
MODULE_LICENSE("GPL");

