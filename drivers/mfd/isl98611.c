/*
 * ISL98611 ISL98611 MFD Driver
 *
 * Copyright 2015 Samsung Electronics
 *
 * Author: ravikant.s2@samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/isl98611.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define ISL98611_DEV_BL				\
{						\
	.name = "isl98611-backlight",		\
	.of_compatible = "intersil,isl98611-backlight",	\
}


static struct mfd_cell isl98611_devs[] = {
	/* Backlight */
	ISL98611_DEV_BL,
};

struct isl98611 *isl98611_global;

static int isl98611_i2c_write(struct i2c_client *client,
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

		dev_info(&client->dev, "%s: i2c transfer error. %d\n", __func__, err);
	}
	return err;
}

static int isl98611_i2c_read(struct i2c_client *client,
		u8 reg,  u8* val, unsigned int len)
{
	int err = 0;
	int retry = 3;

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client,
				reg, len, val);
		if (err >= 0)
			return err;
		dev_info(&client->dev, "%s:i2c transfer error.\n", __func__);
	}
	return err;
}

int isl98611_read_byte(struct isl98611 *isl98611, u8 reg, u8 *read)
{
	int ret;
	u8 val = 0;

	mutex_lock(&isl98611->isl98611_lock);
	ret = isl98611_i2c_read(isl98611->client, reg, &val, 1);
	mutex_unlock(&isl98611->isl98611_lock);
	if (ret < 0)
		return ret;

	*read = val;
	return 0;
}
EXPORT_SYMBOL_GPL(isl98611_read_byte);

int isl98611_write_byte(struct isl98611 *isl98611, u8 reg, u8 data)
{
      int ret;
      mutex_lock(&isl98611->isl98611_lock);
      ret = isl98611_i2c_write(isl98611->client, reg, data, 1);
      mutex_unlock(&isl98611->isl98611_lock);
      return ret;
}
EXPORT_SYMBOL_GPL(isl98611_write_byte);

int isl98611_update_bits(struct isl98611 *isl98611, u8 reg, u8 mask, u8 data)
{
	int ret;
       u8 tmp, orig=0;

       mutex_lock(&isl98611->isl98611_lock);
       ret = isl98611_i2c_read(isl98611->client, reg, &orig, 1);
       if (ret < 0){
		mutex_unlock(&isl98611->isl98611_lock);
		return ret;
	}

      tmp = orig & ~mask;
      tmp |= data & mask;

      if (tmp != orig) {
              ret = isl98611_i2c_write(isl98611->client, reg, tmp, 1);
        }

      mutex_unlock(&isl98611->isl98611_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(isl98611_update_bits);

static int isl98611_init_device(struct isl98611 *isl98611)
{
   if(gpio_is_valid(isl98611->pdata->en_gpio)){
	return devm_gpio_request_one(isl98611->dev, isl98611->pdata->en_gpio,
				     GPIOF_OUT_INIT_HIGH, "isl98611_hwen");
	}
   return 0;
}

static void isl98611_deinit_device(struct isl98611 *isl98611)
{
   if(gpio_is_valid(isl98611->pdata->en_gpio)){
	gpio_set_value(isl98611->pdata->en_gpio, 0);
      }
   return;
}

static int isl98611_parse_dt(struct device *dev, struct isl98611 *isl98611)
{
	struct device_node *node = dev->of_node;
	struct isl98611_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->en_gpio = of_get_named_gpio(node, "ti,en-gpio", 0);
	isl98611->pdata = pdata;

	return 0;
}

static int isl98611_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct isl98611 *isl98611;
	struct device *dev = &cl->dev;
	struct isl98611_platform_data *pdata = dev_get_platdata(dev);
	int ret;

	isl98611 = devm_kzalloc(dev, sizeof(struct isl98611), GFP_KERNEL);
	if (!isl98611)
		return -ENOMEM;

      mutex_init(&isl98611->isl98611_lock);

	isl98611->pdata = pdata;
	isl98611->client = cl;
	if (!pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = isl98611_parse_dt(dev, isl98611);
		else
			ret = -ENODEV;

		if (ret)
			return ret;
	}

	isl98611->dev = &cl->dev;
	i2c_set_clientdata(cl, isl98611);

	ret = isl98611_init_device(isl98611);
	if (ret)
		return ret;

	pr_info("%s... end\n", __func__);

	return mfd_add_devices(dev, -1, isl98611_devs, ARRAY_SIZE(isl98611_devs),
			       NULL, 0, NULL);
}

static int isl98611_remove(struct i2c_client *cl)
{
	struct isl98611 *isl98611 = i2c_get_clientdata(cl);

	isl98611_deinit_device(isl98611);
	mfd_remove_devices(isl98611->dev);
       mutex_destroy(&isl98611->isl98611_lock);
	return 0;
}

static const struct i2c_device_id isl98611_ids[] = {
	{ "isl98611", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl98611_ids);

#ifdef CONFIG_OF
static const struct of_device_id isl98611_of_match[] = {
	{ .compatible = "intersil,isl98611", },
	{ }
};
MODULE_DEVICE_TABLE(of, isl98611_of_match);
#endif

static struct i2c_driver isl98611_driver = {
	.probe = isl98611_probe,
	.remove = isl98611_remove,
	.driver = {
		.name = "isl98611",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(isl98611_of_match),
	},
	.id_table = isl98611_ids,
};
module_i2c_driver(isl98611_driver);

MODULE_DESCRIPTION("INTERSIL ISL98611 MFD Core");
MODULE_AUTHOR("Ravikant Sharma");
MODULE_LICENSE("GPL");
