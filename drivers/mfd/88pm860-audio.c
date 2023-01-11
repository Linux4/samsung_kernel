/*
 * I2C driver for Marvell 88PM860-audio.c
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * Zhao Ye <zhaoy@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/88pm80x.h>
#include <linux/mfd/core.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#define	PM860_AUDIO_REG_NUM		0xc5
static int reg_pm860 = 0xffff;
struct regmap *codec_regmap;

struct regmap *get_88pm860_codec_regmap(void)
{
	return codec_regmap;
}
EXPORT_SYMBOL(get_88pm860_codec_regmap);

struct regmap_config pm860_base_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct resource pm860_codec_resources[] = {
};

static struct mfd_cell pm860_devs[] = {
	{
		.of_compatible = "marvell,88pm860-codec",
		.name = "88pm860-codec",
		.num_resources = ARRAY_SIZE(pm860_codec_resources),
		.resources = pm860_codec_resources,
	},
};

static ssize_t pm860_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int reg_val = 0;
	int len = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm80x_chip *chip = i2c_get_clientdata(client);

	int i;

	if (reg_pm860 == 0xffff) {
		pr_info("pm860: register dump:\n");
		for (i = 0; i < PM860_AUDIO_REG_NUM; i++) {
			regmap_read(chip->regmap, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
	} else {
		regmap_read(chip->regmap, reg_pm860, &reg_val);
		len = sprintf(buf, "reg_pm860=0x%x, val=0x%x\n",
			      reg_pm860, reg_val);
	}
	return len;
}

static ssize_t pm860_reg_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u8 reg_val;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm80x_chip *chip = i2c_get_clientdata(client);
	int i = 0;
	int ret;

	char messages[20];
	memset(messages, '\0', 20);
	strncpy(messages, buf, count);

	if ('+' == messages[0]) {
		/* enable to get all the reg value */
		reg_pm860 = 0xffff;
		pr_info("read all reg enabled!\n");
	} else {
		if (messages[1] != 'x') {
			pr_err("Right format: 0x[addr]\n");
			return -EINVAL;
		}

		if (strlen(messages) > 5) {
			while (messages[i] != ' ')
				i++;
			messages[i] = '\0';
			if (kstrtouint(messages, 16, &reg_pm860) < 0)
				return -EINVAL;
			i++;
			if (kstrtou8(messages + i, 16, &reg_val) < 0)
				return -EINVAL;
			ret = regmap_write(chip->regmap, reg_pm860,
					   reg_val & 0xff);
			if (ret < 0) {
				pr_err("write reg error!\n");
				return -EINVAL;
			}
		} else {
			if (kstrtouint(messages, 16, &reg_pm860) < 0)
				return -EINVAL;
		}
	}

	return count;
}

static DEVICE_ATTR(pm860_reg, 0644, pm860_reg_show, pm860_reg_set);

/*
 * Instantiate the generic non-control parts of the device.
 */
static int pm860_dev_init(struct pm80x_chip *pm860, int irq)
{
	int ret;
	unsigned int val;

	dev_set_drvdata(pm860->dev, pm860);

	ret = regmap_read(pm860->regmap, 0x0, &val);
	if (ret < 0) {
		dev_err(pm860->dev, "Failed to read ID register\n");
		goto err;
	}

	ret = mfd_add_devices(pm860->dev, -1,
			      pm860_devs, ARRAY_SIZE(pm860_devs),
			      NULL,
			      0,
			      NULL);
	if (ret != 0) {
		dev_err(pm860->dev, "Failed to add children: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;
}

static void pm860_dev_exit(struct pm80x_chip *pm860)
{
	mfd_remove_devices(pm860->dev);
}

static const struct of_device_id pm860_of_match[] = {
	{ .compatible = "marvell,pm860", },
	{ }
};
MODULE_DEVICE_TABLE(of, pm860_of_match);

static int pm860_parse_dt(struct device_node *np, struct pm80x_chip *chip)
{
	if (!chip)
		return -EINVAL;

	if (of_property_read_u32(np,
			"marvell,pmic-type", &chip->pmic_type)) {
		dev_info(chip->dev, "do not get pmic type\n");
		/* set default pmic is 88pm860 */
		chip->pmic_type = PMIC_PM860;
	}

	return 0;
}

static int pm860_audio_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct pm80x_chip *pm860;
	int ret;

	ret = pm80x_init(client);
	if (ret) {
		dev_err(&client->dev, "pm860_init fail!\n");
		return ret;
	}


	pm860 = i2c_get_clientdata(client);
	pm860->dev = &client->dev;

	if (IS_ERR(pm860->regmap)) {
		ret = PTR_ERR(pm860->regmap);
		dev_err(pm860->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	pm860_parse_dt(client->dev.of_node, pm860);

	/* add pm860_reg sysfs entries */
	ret = device_create_file(pm860->dev, &dev_attr_pm860_reg);
	if (ret < 0) {
		dev_err(pm860->dev,
			"%s: failed to add pm860_reg sysfs files: %d\n",
			__func__, ret);
		return ret;
	}

	/* add this for headset short detection */
	codec_regmap = pm860->regmap;
	return pm860_dev_init(pm860, client->irq);
}

static int pm860_audio_remove(struct i2c_client *i2c)
{
	struct pm80x_chip *pm860 = i2c_get_clientdata(i2c);

	device_remove_file(pm860->dev, &dev_attr_pm860_reg);
	regmap_del_irq_chip(pm860->irq, pm860->irq_data);
	pm860_dev_exit(pm860);

	return 0;
}

static const struct i2c_device_id pm860_i2c_id[] = {
	{ "88pm860", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pm860_i2c_id);

static const struct of_device_id pm860_dt_ids[] = {
	{ .compatible = "marvell,88pm860", },
	{},
};
MODULE_DEVICE_TABLE(of, pm860_dt_ids);


static struct i2c_driver pm860_i2c_driver = {
	.driver = {
		.name = "88pm860",
		.owner = THIS_MODULE,
		.of_match_table	= of_match_ptr(pm860_dt_ids),
	},
	.probe = pm860_audio_probe,
	.remove = pm860_audio_remove,
	.id_table = pm860_i2c_id,
};

static int __init pm860_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&pm860_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register pm860 I2C driver: %d\n", ret);

	return ret;
}
module_init(pm860_i2c_init);

static void __exit pm860_i2c_exit(void)
{
	i2c_del_driver(&pm860_i2c_driver);
}
module_exit(pm860_i2c_exit);

MODULE_DESCRIPTION("Core support for the PM860 audio CODEC");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhao Ye <zhaoy@marvell.com>");
