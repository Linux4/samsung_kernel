/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "lcd_bias_i2c.h"

struct i2c_client *lcd_bias_i2c_client;
static int lcd_bias_slave_addr = (0x3e);

static int lcd_bias_i2c_read(u8 reg, u8 *data, u16 length)
{
	int err;
	struct i2c_client *i2c = lcd_bias_i2c_client;
	struct i2c_msg msgs[2];

	msgs[0].flags = 0;
	msgs[0].buf = &reg;
	msgs[0].addr = i2c->addr;
	msgs[0].len = 1;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].addr = i2c->addr;
	msgs[1].len = length;

	err = i2c_transfer(i2c->adapter, msgs, 2);
	if (err < 0) {
		SM_ERR("lcd_bias i2c read error:%d\n", err);
		SM_ERR("msg:%d-%d:%d, %d-%d:%d\n",
			msgs[0].addr, msgs[0].len, msgs[0].buf[0],
			msgs[1].addr, msgs[1].len, msgs[1].buf[1]);
		return err;
	}

	return length;
}

static int lcd_bias_i2c_write(u8 reg, u8 *data, u16 length)
{
	int err;
	u8 buf[length + 1];
	struct i2c_client *i2c = lcd_bias_i2c_client;
	struct i2c_msg msg;

	buf[0] = reg;
	memcpy(&buf[1], data, length * sizeof(u8));
	msg.flags = 0;
	msg.buf = buf;
	msg.addr = i2c->addr;
	msg.len = length + 1;

	err = i2c_transfer(i2c->adapter, &msg, 1);
	if (err < 0) {
		SM_ERR("lcd_bias i2c write error:%d\n", err);
		SM_ERR("msg:%d-%d:%d\n", msg.addr, msg.len, msg.buf[0]);
		return err;
	}

	return length;
}


int lcd_bias_set_avdd_avee(void)
{
	int err;
	u8 val[4] = {0x13, 0x13, 0x00, 0x00};

	/* set avdd */
	lcd_bias_i2c_read(0x00, &val[2], 1);
	SM_INFO("i2c read reg-0x00 :0x%x\n", val[2]);
	if (0x13 != val[2]) {
		err = lcd_bias_i2c_write(0x00, &val[0], 1);
		if (err < 0) {
			SM_ERR("i2c write reg-0x01 error:%d\n", err);
			return err;
		} else {
			err = lcd_bias_i2c_read(0x00, &val[2], 1);
			SM_INFO("i2c read reg-0x00 :0x%x\n", val[2]);
		}
	}

	/* set avee*/
	lcd_bias_i2c_read(0x01, &val[3], 1);
	SM_INFO("i2c read reg-0x01 :0x%x\n", val[3]);
	if (0x13 != val[3]) {
		err = lcd_bias_i2c_write(0x01, &val[1], 1);
		if (err < 0) {
			SM_ERR("i2c write reg-0x01 error:%d\n", err);
			return err;
		} else {
			err = lcd_bias_i2c_read(0x01, &val[3], 1);
			SM_INFO("i2c read reg-0x01 :0x%x\n", val[3]);
		}
	}
	return 0;
}



static int lcd_bias_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SM_ERR("I2C check functionality fail!");
		return -ENODEV;
	}
	SM_INFO("lcd_bias_slave_addr:0x%X,client->addr:%X\n",lcd_bias_slave_addr, client->addr);
	lcd_bias_i2c_client = client;

	ret = lcd_bias_set_avdd_avee();
	if (ret < 0) {
		SM_ERR("I2C device probe failed\n");
		return ret;
	}

	SM_INFO("I2C device probe OK\n");

	return 0;
}

static int lcd_bias_i2c_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
	lcd_bias_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id lcd_bias_i2c_ids[] = {
	{ "lcd_bias,i2c", 0 },
	{ }
};

static const struct of_device_id lcd_bias_i2c_matchs[] = {
	{ .compatible = "lcd_bias,i2c", },
	{ }
};

static struct i2c_driver lcd_bias_i2c_driver = {
	.probe		= lcd_bias_i2c_probe,
	.remove		= lcd_bias_i2c_remove,
	.id_table	= lcd_bias_i2c_ids,
	.driver	= {
		.name	= "lcd_bias_i2c_drv",
		.owner	= THIS_MODULE,
		.bus = &i2c_bus_type,
		.of_match_table = lcd_bias_i2c_matchs,
	},
};

static int __init lcd_bias_i2c_init(void)
{
	return i2c_add_driver(&lcd_bias_i2c_driver);
}

static void lcd_bias_i2c_exit(void)
{
	SM_INFO("lcd_bias i2c exit\n");
	i2c_del_driver(&lcd_bias_i2c_driver);
}

module_init(lcd_bias_i2c_init);
module_exit(lcd_bias_i2c_exit);

MODULE_AUTHOR("zhangdy@longcheer.com");
MODULE_DESCRIPTION("Longcheer LCD bias I2C Drivers");
MODULE_LICENSE("GPL");
