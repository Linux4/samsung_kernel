/*
 * TI LM3632 MFD Driver
 *
 * Copyright 2015 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
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
#include <linux/mfd/lm3632.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define LM3632_DEV_LCD_BIAS(_id)		\
{						\
	.name = "lm3632-regulator",		\
	.id   = _id,				\
	.of_compatible = "ti,lm3632-regulator",	\
}

#define LM3632_DEV_BL				\
{						\
	.name = "lm3632-backlight",		\
	.of_compatible = "ti,lm3632-backlight",	\
}

#define LM3632_DEV_FLASH				\
{						\
	.name = "lm3632-flash",		\
	.of_compatible = "ti,lm3632-flash",	\
}

static struct mfd_cell lm3632_devs[] = {
	/* 3 Regulators */
	LM3632_DEV_LCD_BIAS(1),
	LM3632_DEV_LCD_BIAS(2),
	LM3632_DEV_LCD_BIAS(3),

	/* Backlight */
	LM3632_DEV_BL,
	/* Torch Flash */
	LM3632_DEV_FLASH,
};

int lm3632_read_byte(struct lm3632 *lm3632, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(lm3632->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(lm3632_read_byte);

int lm3632_write_byte(struct lm3632 *lm3632, u8 reg, u8 data)
{
	return regmap_write(lm3632->regmap, reg, data);
}
EXPORT_SYMBOL_GPL(lm3632_write_byte);

int lm3632_update_bits(struct lm3632 *lm3632, u8 reg, u8 mask, u8 data)
{
	return regmap_update_bits(lm3632->regmap, reg, mask, data);
}
EXPORT_SYMBOL_GPL(lm3632_update_bits);

static int lm3632_init_device(struct lm3632 *lm3632)
{
	return devm_gpio_request_one(lm3632->dev, lm3632->pdata->en_gpio,
				     GPIOF_OUT_INIT_HIGH, "lm3632_hwen");
}

static void lm3632_deinit_device(struct lm3632 *lm3632)
{
	gpio_set_value(lm3632->pdata->en_gpio, 0);
}

static int lm3632_parse_dt(struct device *dev, struct lm3632 *lm3632)
{
	//struct device_node *node = dev->of_node;
	struct lm3632_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	//pdata->en_gpio = of_get_named_gpio(node, "ti,en-gpio", 0);
	pdata->en_gpio=0;
	if (pdata->en_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	lm3632->pdata = pdata;

	return 0;
}

static struct regmap_config lm3632_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LM3632_MAX_REGISTERS,
};

static int lm3632_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lm3632 *lm3632;
	struct device *dev = &cl->dev;
	struct lm3632_platform_data *pdata = dev_get_platdata(dev);
	int ret;
	printk("zyun lm3632_probe\n");

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_I2C)) {
		printk("zyun i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	lm3632 = devm_kzalloc(dev, sizeof(*lm3632), GFP_KERNEL);
	if (!lm3632)
		return -ENOMEM;

	lm3632->pdata = pdata;
	if (!pdata) {

		if (IS_ENABLED(CONFIG_OF))
			ret = lm3632_parse_dt(dev, lm3632);
	}

	lm3632->regmap = devm_regmap_init_i2c(cl, &lm3632_regmap_config);
	if (IS_ERR(lm3632->regmap))
		return PTR_ERR(lm3632->regmap);


	lm3632->dev = &cl->dev;
	i2c_set_clientdata(cl, lm3632);


	ret = lm3632_init_device(lm3632);
	if (ret)
		return ret;
	//printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	printk("zyun original lm3632 adress=0x%x\n",(int)lm3632);
	return mfd_add_devices(dev, -1, lm3632_devs, ARRAY_SIZE(lm3632_devs),
			       NULL, 0, NULL);
}

static int lm3632_remove(struct i2c_client *cl)
{
	struct lm3632 *lm3632 = i2c_get_clientdata(cl);

	lm3632_deinit_device(lm3632);
	mfd_remove_devices(lm3632->dev);

	return 0;
}

static const struct i2c_device_id lm3632_ids[] = {
	{ "lm3632", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3632_ids);

#ifdef CONFIG_OF
static const struct of_device_id lm3632_of_match[] = {
	{ .compatible = "ti,lm3632", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3632_of_match);
#endif

static struct i2c_driver lm3632_driver = {
	.probe = lm3632_probe,
	.remove = lm3632_remove,
	.driver = {
		.name = "lm3632",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3632_of_match),
	},
	.id_table = lm3632_ids,
};
module_i2c_driver(lm3632_driver);

MODULE_DESCRIPTION("TI LM3632 MFD Core");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
