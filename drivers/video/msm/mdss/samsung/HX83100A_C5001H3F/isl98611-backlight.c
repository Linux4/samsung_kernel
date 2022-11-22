/*
 * isl98611-backlight.c - Platform data for isl98611 backlight driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <linux/device.h>
#include <linux/of_gpio.h>

#include "../ss_dsi_panel_common.h"


struct isl98611_backlight_platform_data {
	int gpio_en;
	u32 gpio_en_flags;

	int gpio_enn;
	u32 gpio_enn_flags;

	int gpio_enp;
	u32 gpio_enp_flags;
};

struct isl98611_backlight_info {
	struct i2c_client			*client;
	struct isl98611_backlight_platform_data	*pdata;
};

static struct isl98611_backlight_info *pinfo;

static int backlight_i2c_write(struct i2c_client *client,
		u8 reg,  u8 val, unsigned int len)
{
	int err = 0;
	int retry = 3;
	u8 temp_val = val;

	while (retry--) {
		err = i2c_smbus_write_i2c_block_data(client,
				reg, len, &temp_val);
		if (err >= 0)
			return err;

		LCD_ERR("i2c transfer error. %d\n", err);
	}
	return err;
}

static void backlight_request_gpio(struct isl98611_backlight_platform_data *pdata)
{
	int ret;

	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "gpio_en");
		if (ret) {
			LCD_ERR("unable to request gpio_en [%d]\n", pdata->gpio_en);
			return;
		}
	}

	if (gpio_is_valid(pdata->gpio_enn)) {
		ret = gpio_request(pdata->gpio_enn, "gpio_enn");
		if (ret) {
			LCD_ERR("unable to request gpio_enn [%d]\n", pdata->gpio_enn);
			return;
		}
	}

	if (gpio_is_valid(pdata->gpio_enp)) {
		ret = gpio_request(pdata->gpio_enp, "gpio_enp");
		if (ret) {
			LCD_ERR("unable to request gpio_enp [%d]\n", pdata->gpio_enp);
			return;
		}
	}
}

static int isl98611_backlight_parse_dt(struct device *dev,
			struct isl98611_backlight_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	/* enable, en, enp */
	pdata->gpio_en = of_get_named_gpio_flags(np, "isl98611_en_gpio",
				0, &pdata->gpio_en_flags);
	pdata->gpio_enp = of_get_named_gpio_flags(np, "isl98611_enp_gpio",
				0, &pdata->gpio_enp_flags);
	pdata->gpio_enn = of_get_named_gpio_flags(np, "isl98611_enn_gpio",
				0, &pdata->gpio_enn_flags);

	LCD_ERR("gpio_en : %d , gpio_enp : %d gpio_enn : %d\n", 
		pdata->gpio_en, pdata->gpio_enp, pdata->gpio_enn);
	return 0;
}

int isl98611_backlight_control(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	struct isl98611_backlight_info *info = pinfo;
	int test_flag = 0;

	if (!info) {
		LCD_ERR("error pinfo\n");
		return 0;
	}

	if (enable)
	{
		usleep_range(1000, 1000);
		gpio_set_value(info->pdata->gpio_en, 1);
		gpio_set_value(info->pdata->gpio_enp, 1);
		gpio_set_value(info->pdata->gpio_enn, 1);

		if (test_flag) {
			usleep_range(5000, 5000);
			backlight_i2c_write(info->client, 0x10, 0x10, 1);
			backlight_i2c_write(info->client, 0x11, 0x00, 1);
		}
	} else {
		gpio_set_value(info->pdata->gpio_en, 0);
		gpio_set_value(info->pdata->gpio_enp, 0);
		gpio_set_value(info->pdata->gpio_enn, 0);
	}
	

	LCD_INFO(" enable : %d \n", enable);

	return 0;
}

static int isl98611_backlight_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct isl98611_backlight_platform_data *pdata;
	struct isl98611_backlight_info *info;

	int error = 0;

	LCD_INFO("\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct isl98611_backlight_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			LCD_INFO("Failed to allocate memory\n");
			return -ENOMEM;
		}

		error = isl98611_backlight_parse_dt(&client->dev, pdata);
		if (error)
			return error;
	} else
		pdata = client->dev.platform_data;

	backlight_request_gpio(pdata);

	pinfo = info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		LCD_INFO("Failed to allocate memory\n");
		return -ENOMEM;
	}

	info->client = client;
	info->pdata = pdata;

	i2c_set_clientdata(client, info);

	return error;
}

static int isl98611_backlight_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id isl98611_backlight_id[] = {
	{"isl98611_backlight", 0},
};
MODULE_DEVICE_TABLE(i2c, isl98611_backlight_id);

static struct of_device_id isl98611_backlight_match_table[] = {
	{ .compatible = "isl98611,backlight-control",},
};

MODULE_DEVICE_TABLE(of, isl98611_backlight_id);

struct i2c_driver isl98611_backlight_driver = {
	.probe = isl98611_backlight_probe,
	.remove = isl98611_backlight_remove,
	.driver = {
		.name = "isl98611_backlight",
		.owner = THIS_MODULE,
		.of_match_table = isl98611_backlight_match_table,
		   },
	.id_table = isl98611_backlight_id,
};

static int __init isl98611_backlight_init(void)
{

	int ret = 0;

	ret = i2c_add_driver(&isl98611_backlight_driver);
	if (ret) {
		printk(KERN_ERR "isl98611_backlight_init registration failed. ret= %d\n",
			ret);
	}

	return ret;
}

static void __exit isl98611_backlight_exit(void)
{
	i2c_del_driver(&isl98611_backlight_driver);
}

module_init(isl98611_backlight_init);
module_exit(isl98611_backlight_exit);

MODULE_DESCRIPTION("isl98611 backlight driver");
MODULE_LICENSE("GPL");
