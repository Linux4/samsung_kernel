/*
 * max77816_i2c.c - Platform data for max77816 buck booster hw i2c driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "ss_boost_max77816_i2c.h"

__visible_for_testing struct ss_boost_max77816_info boost_pinfo;

/* return -1 if no max77816 ic, not probed */
int ss_boost_i2c_read_ex(u8 addr,  u8 *value)
{
	int retry = 3;
	u8 wr[] = {addr};
	int ret = -1;

	/* Check if it probed or not, to prevent KP */
	if (!boost_pinfo.client) {
		pr_err("[SDE] %s : No maxim ic, not probed.\n", __func__);
	} else {
		struct i2c_msg msg[] = {
			{
				.addr = boost_pinfo.client->addr,
				.flags = 0,
				.buf = wr,
				.len = 1
			}, {
				.addr = boost_pinfo.client->addr,
				.flags = I2C_M_RD,
				.buf = value,
				.len = 1
			},
		};

		do {
			ret = ss_wrapper_i2c_transfer(boost_pinfo.client->adapter, msg, 2);
			if (ret != 2)
				pr_err("[SDE] %s : client->addr 0x%02x read_addr 0x%02x error (ret == %d)\n",
					__func__, boost_pinfo.client->addr, addr, ret);
			else
				break;
		} while (--retry);
	}

	return ret;
}

/* returns -1 if no max77816 ic, not probed
 * returns written size, not zero.
 */
int ss_boost_i2c_write_ex(u8 addr,  u8 value)
{
	int retry = 3;
	u8 wr[] = {addr, value};
	int ret = -1;

	/* Check if it probed or not, to prevent KP */
	if (!boost_pinfo.client) {
		pr_err("[SDE] %s : No maxim ic, not probed.\n", __func__);
	} else {
		struct i2c_msg msg[] = {
			{
				.addr = boost_pinfo.client->addr,
				.flags = 0,
				.buf = wr,
				.len = 2
			}
		};

		do {
			ret = ss_wrapper_i2c_transfer(boost_pinfo.client->adapter, msg, 1);
			if (ret != 1) {
				pr_err("[SDE] %s : addr 0x%02x value 0x%02x error (ret == %d)\n", __func__,
					 addr, value, ret);
			} else
				break;
		} while (--retry);
	}

	return ret;
}

static int ss_boost_max77816_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	pr_info("[SDE] %s \n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c check fail\n", __func__);
		return -EIO;
	}

	boost_pinfo.client = client;

	i2c_set_clientdata(client, &boost_pinfo);

	return 0;
}

static int ss_boost_max77816_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ss_boost_max77816_id[] = {
	{"max77816", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ss_boost_max77816_id);

static const struct of_device_id ss_boost_max77816_match_table[] = {
	{ .compatible = "max77816,display_boost",},
	{ }
};

struct i2c_driver ss_boost_max77816_driver = {
	.probe = ss_boost_max77816_probe,
	.remove = ss_boost_max77816_remove,
	.driver = {
		.name = "max77816",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ss_boost_max77816_match_table),
#endif
		   },
	.id_table = ss_boost_max77816_id,
};

int ss_boost_max77816_init(void)
{

	int ret = 0;

	ret = i2c_add_driver(&ss_boost_max77816_driver);
	if (ret)
		pr_err("[SDE] %s registration failed. ret= %d\n", __func__, ret);

	return ret;
}

void ss_boost_max77816_exit(void)
{
	i2c_del_driver(&ss_boost_max77816_driver);
}

//module_init(ss_boost_max77816_init);
//module_exit(ss_boost_max77816_exit);

//MODULE_DESCRIPTION("ss_boost_max77816_i2c driver");
//MODULE_LICENSE("GPL");
