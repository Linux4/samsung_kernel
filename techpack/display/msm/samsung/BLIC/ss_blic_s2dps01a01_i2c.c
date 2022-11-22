/*
 * s2dps01a01_hw_i2c.c - Platform data for s2dps01a01 backlight driver
 *
 * Copyright (C) 2020 Samsung Electronics
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

#include "ss_blic_s2dps01a01_i2c.h"

static int ss_backlight_i2c_read(struct i2c_client *client, u8 addr,  u8 *value)
{
	int retry = 3;
	u8 wr[] = {addr};

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = wr,
			.len = 1
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = value,
			.len = 1
		},
	};
	int ret;

	do {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret != 2)
			pr_err("%s : client->addr 0x%02x read_addr 0x%02x error (ret == %d)\n",
					__func__, client->addr, addr, ret);
		else
			break;
	} while (--retry);

	return ret;
}

static int ss_backlight_i2c_write(struct i2c_client *client, u8 addr,  u8 value)
{
	int retry = 3;
	u8 wr[] = {addr, value};
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = wr,
			.len = 2
		}
	};
	int ret;

	do {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret != 1) {
			pr_err("%s : addr 0x%02x value 0x%02x error (ret == %d)\n", __func__,
				 addr, value, ret);
		} else
			break;
	} while (--retry);

	return ret;
}

enum {
	BLIC_ADDR = 0,
	BLIC_VAL,
	BLIC_MAX,
};

static int ss_blic_s2dps01a01_configure(struct i2c_client *client)
{
	u8 check;

	u8 data[][BLIC_MAX] = {
		/* pwmi brightness mode */
		/* addr value */
		{0x1C, 0x13},
		{0x1D, 0xCC},
		{0x1E, 0x70},
		{0x1F, 0x0A},
		{0x21, 0x0F},
		{0x22, 0x00},
		{0x23, 0x00},
		{0x24, 0x01},
		{0x25, 0x01},
		{0x26, 0x02},

		{0x11, 0xDF},

		//GPIO EN_VP HIGH
		//GPIO EN_VN HIGH
		{0x20, 0x01},
		//GPIO PWMI
		{0x24, 0x00},
	};
	int size = ARRAY_SIZE(data);
	int ret = 0;
	int i;

	if (!client) {
		pr_err("%s: i2c is not prepared\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < size; i++) {
		ss_backlight_i2c_write(client, data[i][BLIC_ADDR], data[i][BLIC_VAL]);
		ss_backlight_i2c_read(client, data[i][BLIC_ADDR], &check);

		if (check != data[i][BLIC_VAL]) {
			dev_err(&client->dev, "fail: add: %x, val: %x, %x\n",
					data[i][BLIC_ADDR], data[i][BLIC_VAL], check);
			ret = -EINVAL;
		} else {
			dev_info(&client->dev, "pass: add: %x, val: %x\n",
					data[i][BLIC_ADDR], data[i][BLIC_VAL]);
		}
	}

	dev_info(&client->dev, "s2dps01a01 config done\n");

	return ret;
}

static int ss_blic_s2dps01a01_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	dev_info(&client->dev, "%s: +++\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c check fail\n", __func__);
		return -EIO;
	}

	ss_blic_s2dps01a01_configure(client);

	dev_info(&client->dev, "%s: ---\n", __func__);

	return 0;
}

static int ss_blic_s2dps01a01_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ss_blic_s2dps01a01_id[] = {
	{"s2dps01a01", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ss_blic_s2dps01a01_id);

static struct of_device_id ss_blic_s2dps01a01_match_table[] = {
	{ .compatible = "s2dps01a01,display_backlight",},
	{ }
};

struct i2c_driver ss_blic_s2dps01a01_driver = {
	.probe = ss_blic_s2dps01a01_probe,
	.remove = ss_blic_s2dps01a01_remove,
	.driver = {
		.name = "s2dps01a01",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ss_blic_s2dps01a01_match_table),
#endif
		   },
	.id_table = ss_blic_s2dps01a01_id,
};

int ss_blic_s2dps01a01_init(void)
{

	int ret = 0;

	ret = i2c_add_driver(&ss_blic_s2dps01a01_driver);
	if (ret)
		pr_err("%s : s2dps01a01_init registration failed. ret= %d\n", __func__, ret);

	pr_info("%s: done\n", __func__);

	return ret;
}

void ss_blic_s2dps01a01_exit(void)
{
	i2c_del_driver(&ss_blic_s2dps01a01_driver);
}

module_init(ss_blic_s2dps01a01_init);
module_exit(ss_blic_s2dps01a01_exit);

MODULE_DESCRIPTION("ss_blic_s2dps01a01_i2c driver");
MODULE_LICENSE("GPL");
