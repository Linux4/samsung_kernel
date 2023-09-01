/*
 * lm36274-backlight.c - Platform data for lm36274 backlight driver
 *
 * Copyright (C) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "lm36274-backlight.h"

static struct lm36274_backlight_info *pinfo;

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
		usleep_range(1000,1000);
	}
	return err;
}

static int backlight_i2c_read(struct i2c_client *client,
		u8 reg,  u8 *val, unsigned int len)
{
	int err = 0;
	int retry = 3;

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client,
				reg, len, val);
		if (err >= 0)
			return err;

		LCD_ERR("i2c transfer error. %d\n", err);
		usleep_range(1000,1000);
	}
	return err;
}

static void debug_blic(struct blic_message *message)
{
	int init_loop;
	struct lm36274_backlight_info *info = pinfo;
	char address, value;

	for (init_loop = 0; init_loop < message->size; init_loop++) {
		address = message->address[init_loop];

		backlight_i2c_read(info->client, address, &value, 1);

		LCD_ERR("BLIC address : 0x%x  value : 0x%x\n", address, value);
	}
}

static void backlight_request_gpio(struct lm36274_backlight_platform_data *pdata)
{
	int ret;

	if (gpio_is_valid(pdata->gpio_hwen)) {
		ret = gpio_request(pdata->gpio_hwen, "gpio_hwen");
		if (ret) {
			LCD_ERR("unable to request gpio_hwen [%d]\n", pdata->gpio_hwen);
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

static int lm36274_backlight_data_parse(struct device *dev,
			struct blic_message *input, char *keystring)
{
	struct device_node *np = dev->of_node;
	const char *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_ERR("%s:%d, Unable to read table %s ", __func__, __LINE__, keystring);
		return -EINVAL;
	}

	if ((len % 2) != 0) {
		LCD_ERR("%s:%d, Incorrect table entries for %s",
					__func__, __LINE__, keystring);
		return -EINVAL;
	}

	input->size = len / 2;

	input->address = kzalloc(input->size, GFP_KERNEL);
	if (!input->address)
		return -ENOMEM;

	input->data = kzalloc(input->size, GFP_KERNEL);
	if (!input->data)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < input->size; i++) {
		input->address[i] = data[data_offset++];
		input->data[i] = data[data_offset++];

		LCD_ERR("BLIC address : 0x%x  value : 0x%x\n", input->address[i], input->data[i]);
	}

	return 0;
error:
	input->size = 0;
	kfree(input->address);

	return -ENOMEM;
}

static int lm36274_backlight_parse_dt(struct device *dev,
			struct lm36274_backlight_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	/* enable, en, enp */
	pdata->gpio_hwen = of_get_named_gpio_flags(np, "lm36274_hwen_gpio",
				0, &pdata->gpio_hwen_flags);
	pdata->gpio_enp = of_get_named_gpio_flags(np, "lm36274_enp_gpio",
				0, &pdata->gpio_enp_flags);
	pdata->gpio_enn = of_get_named_gpio_flags(np, "lm36274_enn_gpio",
				0, &pdata->gpio_enn_flags);

	LCD_ERR("gpio_hwen : %d , gpio_enp : %d gpio_enn : %d\n",
		pdata->gpio_hwen, pdata->gpio_enp, pdata->gpio_enn);

	return 0;
}

int lm36274_pwm_en(int enable)
{
	//do nothing, this ic has not pwn en pin
	return 0;
}

/* active lm36274, and turn on LCD bias output +-5.5V */
void lm36274_bl_ic_en(int enable)
{
	struct lm36274_backlight_info *info = pinfo;
	char address, data;
	int init_loop;
	int debug = 1;

	if (!info) {
		LCD_ERR("error pinfo\n");
		return;
	}

	LCD_INFO("enable : %d \n", enable);

	if (enable) {
		// set HWEN high to active the IC, <50us, then I2C registers reset done.
		gpio_set_value(info->pdata->gpio_hwen, 1);
		usleep_range(70, 200);
		// initialize the registers
		for (init_loop = 0; init_loop < info->pdata->init_data.size; init_loop++) {
			address = info->pdata->init_data.address[init_loop];
			data = info->pdata->init_data.data[init_loop];
			backlight_i2c_write(info->client, address, data, 1);
		}
		usleep_range(1000, 1300);
		if (debug)
			debug_blic(&info->pdata->init_data);

		/* LCM bias output +-5.5V */
		gpio_set_value(info->pdata->gpio_enp, 1);
		usleep_range(1500, 1500);
		gpio_set_value(info->pdata->gpio_enn, 1);
		usleep_range(15000, 15000);
	} else {
		gpio_set_value(info->pdata->gpio_enn, 0);
		usleep_range(1500, 1500);
		gpio_set_value(info->pdata->gpio_enp, 0);
		usleep_range(15000, 15000);
		gpio_set_value(info->pdata->gpio_hwen, 0);
		usleep_range(1000, 3000);
	}

	return;
}

static int lm36274_backlight_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct lm36274_backlight_platform_data *pdata;
	struct lm36274_backlight_info *info;

	int error = 0;

	LCD_INFO("\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct lm36274_backlight_platform_data),
			GFP_KERNEL);
	if (!pdata) {
		LCD_INFO("Failed to allocate memory\n");
		return -ENOMEM;
	}

	pinfo = info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		LCD_INFO("Failed to allocate memory\n");
		return -ENOMEM;
	}

	info->client = client;
	info->pdata = pdata;

	if (client->dev.of_node) {
		lm36274_backlight_parse_dt(&client->dev, pdata);
		backlight_request_gpio(pdata);

		lm36274_backlight_data_parse(&client->dev, &info->pdata->init_data, "blic_init_data");
	}

	i2c_set_clientdata(client, info);

	return error;
}

static int lm36274_backlight_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id lm36274_backlight_id[] = {
	{"lm36274_backlight", 0},
};
MODULE_DEVICE_TABLE(i2c, lm36274_backlight_id);

static struct of_device_id lm36274_backlight_match_table[] = {
	{ .compatible = "lm36274,backlight-control",},
};

MODULE_DEVICE_TABLE(of, lm36274_backlight_id);

struct i2c_driver lm36274_backlight_driver = {
	.probe = lm36274_backlight_probe,
	.remove = lm36274_backlight_remove,
	.driver = {
		.name = "lm36274_backlight",
		.owner = THIS_MODULE,
		.of_match_table = lm36274_backlight_match_table,
		   },
	.id_table = lm36274_backlight_id,
};

static int __init lm36274_backlight_init(void)
{

	int ret = 0;

	ret = i2c_add_driver(&lm36274_backlight_driver);
	if (ret) {
		printk(KERN_ERR "lm36274_backlight_init registration failed. ret= %d\n",
			ret);
	}

	return ret;
}

static void __exit lm36274_backlight_exit(void)
{
	i2c_del_driver(&lm36274_backlight_driver);
}

module_init(lm36274_backlight_init);
module_exit(lm36274_backlight_exit);

MODULE_DESCRIPTION("lm36274 backlight driver");
MODULE_LICENSE("GPL");
