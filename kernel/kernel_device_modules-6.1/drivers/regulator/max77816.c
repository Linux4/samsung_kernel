// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 Mediatek Inc.
// Author: Stanley Song <stanley.song@mediatek.com>

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/gpio.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#define MAX77816_REG_DEVICE_ID	0x00
#define MAX77816_REG_STATUS		0x01
#define MAX77816_REG_CONFIG1	0x02
#define MAX77816_REG_CONFIG2	0x03
#define MAX77816_REG_VOUT		0x04
#define MAX77816_REG_VOUT_H		0x05
#define MAX77816_REG_INT_MASK	0x06
#define MAX77816_REG_INT		0x07
#define MAX77816_REG_MAX_REG	MAX77816_REG_INT

#define MAX77816_REG_VOLTAGE_BASE	2600000
#define MAX77816_REG_VOLTAGE_GAP	20000

#define MAX77816_REG_ON_VOLTAGE		0x3f // 3.86V
#define MAX77816_REG_OFF_VOLTAGE	0x2b // 3.46V

#define MAX77816_BIT_BUCK_BOOST_EN	BIT(6)

static int max77816_enable(struct regulator_dev *rdev)
{
	// 3.86V
	return regmap_write(rdev->regmap, MAX77816_REG_VOUT, MAX77816_REG_ON_VOLTAGE);
}

static int max77816_disable(struct regulator_dev *rdev)
{
	// 3.46V
	return regmap_write(rdev->regmap, MAX77816_REG_VOUT, MAX77816_REG_OFF_VOLTAGE);
}

static int max77816_is_enabled(struct regulator_dev *rdev)
{
	unsigned int reg, config2;
	int enabled;
	int ret;

	ret = regmap_read(rdev->regmap, MAX77816_REG_CONFIG2, &config2);
	if (ret < 0) {
		pr_err("max77816 CONFIG2 read fail, ret=%d\n", ret);
		return 0;
	} else if(config2 & MAX77816_BIT_BUCK_BOOST_EN) {
		ret = regmap_read(rdev->regmap, MAX77816_REG_VOUT, &reg);
		if (ret < 0) {
			pr_err("max77816 VOUT read fail, ret=%d\n", ret);
			return 0;
		}

		enabled = (reg == MAX77816_REG_ON_VOLTAGE) ? 1 : 0;
	} else {
		pr_err("max77816 not enabled, CONFIG2(0x%x)\n", config2);
		enabled = 0;
	}

	return enabled;
}

static int max77816_get_voltage(struct regulator_dev *rdev)
{
	unsigned int reg;
	int ret;

	ret = regmap_read(rdev->regmap, MAX77816_REG_VOUT, &reg);
	if (ret < 0) {
		pr_err("[max77816_get_voltage] read fail, %d\n", ret);
		return 0;
	}

	return MAX77816_REG_VOLTAGE_BASE + (reg * MAX77816_REG_VOLTAGE_GAP);
}

static int max77816_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV, unsigned *selector)
{
	return regmap_write(rdev->regmap, MAX77816_REG_VOUT, MAX77816_REG_ON_VOLTAGE);
}


static const struct regmap_config max77816_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct regulator_ops max77816_i2c_ops = {
	.enable = max77816_enable,
	.disable = max77816_disable,
	.is_enabled = max77816_is_enabled,
	.get_voltage = max77816_get_voltage,
	.set_voltage = max77816_set_voltage,
};

static const struct regulator_desc max77816_regulators = {
	.id = 0,
	.type = REGULATOR_VOLTAGE,
	.name = "max77816_vbb2",
	.of_match = of_match_ptr("max77816_vbb2"),
	.ops = &max77816_i2c_ops,
	.owner = THIS_MODULE,
};

static int max77816_i2c_probe(struct i2c_client *i2c)
{
	int ret;
	struct regulator_config config = {.dev = &i2c->dev};
	struct regmap *regmap = devm_regmap_init_i2c(i2c, &max77816_regmap);
	struct regulator_dev *rdev;

	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	rdev = devm_regulator_register(&i2c->dev, &max77816_regulators, &config);

	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(&i2c->dev, "failed to register %s: %d\n",
			max77816_regulators.name, ret);
		return ret;
	}
	return 0;
}


static const struct i2c_device_id max77816_id_table[] = {
	{ "max77816", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max77816_id_table);

static const struct of_device_id max77816_device_match_table[] = {
	{ .compatible = "maxim,max77816" },
	{},
};

MODULE_DEVICE_TABLE(of, max77816_device_match_table);

static struct i2c_driver max77816_i2c_driver = {
	.driver = {
		.name = "max77816",
		.of_match_table = max77816_device_match_table,
	},
	.probe_new = max77816_i2c_probe,
	.id_table = max77816_id_table,
};

module_i2c_driver(max77816_i2c_driver);

MODULE_DESCRIPTION("mt77816 Regulator Driver");
MODULE_AUTHOR("Stanley Song <stanley.song@mediatek.com>");
MODULE_LICENSE("GPL");
