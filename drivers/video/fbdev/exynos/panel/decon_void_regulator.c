// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * fixed.c
 *
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * Copyright (c) 2009 Nokia Corporation
 * Roger Quadros <ext-roger.quadros@nokia.com>
 *
 * This is useful for systems with mixed controllable and
 * non-controllable regulators, as well as for allowing testing on
 * systems with no controllable regulators.
 */

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>


struct fixed_voltage_data {
	struct regulator_desc desc;
	struct regulator_dev *dev;

	unsigned int enable_counter;
};

static const char *rdev_get_name(struct regulator_dev *rdev)
{
	if (rdev->constraints && rdev->constraints->name)
		return rdev->constraints->name;
	else if (rdev->desc->name)
		return rdev->desc->name;
	else
		return "";
}

static int regulator_void_enable(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);

	dev_info(&rdev->dev, "%s: %s\n", __func__, rdev_get_name(rdev));

	priv->enable_counter = 1;

	return 0;
}

static int regulator_void_disable(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);

	dev_info(&rdev->dev, "%s: %s\n", __func__, rdev_get_name(rdev));

	priv->enable_counter = 0;

	return 0;
}

static int regulator_void_is_enabled(struct regulator_dev *rdev)
{
	struct fixed_voltage_data *priv = rdev_get_drvdata(rdev);

	dev_info(&rdev->dev, "%s: %s(%d)\n", __func__, rdev_get_name(rdev), priv->enable_counter);

	return priv->enable_counter > 0;
}


/**
 * of_get_fixed_voltage_config - extract fixed_voltage_config structure info
 * @dev: device requesting for fixed_voltage_config
 * @desc: regulator description
 *
 * Populates fixed_voltage_config structure by extracting data from device
 * tree node, returns a pointer to the populated structure of NULL if memory
 * alloc fails.
 */
static struct fixed_voltage_config *
of_get_fixed_voltage_config(struct device *dev,
			    const struct regulator_desc *desc)
{
	struct fixed_voltage_config *config;
	struct device_node *np = dev->of_node;
	struct regulator_init_data *init_data;

	config = devm_kzalloc(dev, sizeof(struct fixed_voltage_config),
								 GFP_KERNEL);
	if (!config)
		return ERR_PTR(-ENOMEM);

	config->init_data = of_get_regulator_init_data(dev, dev->of_node, desc);
	if (!config->init_data)
		return ERR_PTR(-EINVAL);

	init_data = config->init_data;
	init_data->constraints.apply_uV = 0;

	config->supply_name = init_data->constraints.name;
	if (init_data->constraints.min_uV == init_data->constraints.max_uV) {
		config->microvolts = init_data->constraints.min_uV;
	} else {
		dev_err(dev,
			 "Fixed regulator specified with variable voltages\n");
		return ERR_PTR(-EINVAL);
	}

	if (init_data->constraints.boot_on)
		config->enabled_at_boot = true;

	of_property_read_u32(np, "startup-delay-us", &config->startup_delay);

	if (of_find_property(np, "vin-supply", NULL))
		config->input_supply = "vin";

	return config;
}

static const struct regulator_ops fixed_voltage_ops = {
	.enable = regulator_void_enable,
	.disable = regulator_void_disable,
	.is_enabled = regulator_void_is_enabled,
};

int regulator_void_register(struct platform_device *pdev, const struct regulator_ops *ops)
{
	struct fixed_voltage_config *config;
	struct fixed_voltage_data *drvdata;
	struct regulator_config cfg = { };
	enum gpiod_flags gflags;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct fixed_voltage_data),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		config = of_get_fixed_voltage_config(&pdev->dev,
						     &drvdata->desc);
		if (IS_ERR(config))
			return PTR_ERR(config);
	} else {
		config = dev_get_platdata(&pdev->dev);
	}

	if (!config)
		return -ENOMEM;

	drvdata->desc.name = devm_kstrdup(&pdev->dev,
					  config->supply_name,
					  GFP_KERNEL);
	if (drvdata->desc.name == NULL) {
		dev_err(&pdev->dev, "Failed to allocate supply name\n");
		return -ENOMEM;
	}
	drvdata->desc.type = REGULATOR_VOLTAGE;
	drvdata->desc.owner = THIS_MODULE;
	drvdata->desc.ops = ops ? ops : &fixed_voltage_ops;

	drvdata->desc.enable_time = config->startup_delay;

	if (config->input_supply) {
		drvdata->desc.supply_name = devm_kstrdup(&pdev->dev,
					    config->input_supply,
					    GFP_KERNEL);
		if (!drvdata->desc.supply_name) {
			dev_err(&pdev->dev,
				"Failed to allocate input supply\n");
			return -ENOMEM;
		}
	}

	if (config->microvolts)
		drvdata->desc.n_voltages = 1;

	drvdata->desc.fixed_uV = config->microvolts;

	/*
	 * The signal will be inverted by the GPIO core if flagged so in the
	 * descriptor.
	 */
	if (config->enabled_at_boot)
		gflags = GPIOD_OUT_HIGH;
	else
		gflags = GPIOD_OUT_LOW;

	cfg.dev = &pdev->dev;
	cfg.init_data = config->init_data;
	cfg.driver_data = drvdata;
	cfg.of_node = pdev->dev.of_node;

	drvdata->dev = devm_regulator_register(&pdev->dev, &drvdata->desc,
					       &cfg);
	if (IS_ERR(drvdata->dev)) {
		ret = PTR_ERR(drvdata->dev);
		dev_err(&pdev->dev, "Failed to register regulator: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, drvdata);

	dev_info(&pdev->dev, "%s supplying %duV\n", drvdata->desc.name,
		drvdata->desc.fixed_uV);

	return 0;
}
EXPORT_SYMBOL(regulator_void_register);

int regulator_void_probe(struct platform_device *pdev)
{
	return regulator_void_register(pdev, NULL);
}

#if defined(CONFIG_OF)
static const struct of_device_id fixed_of_match[] = {
	{
		.compatible = "regulator-void",
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, fixed_of_match);
#endif

static struct platform_driver regulator_void_driver = {
	.probe		= regulator_void_probe,
	.driver		= {
		.name		= "regulator-void",
		.of_match_table = of_match_ptr(fixed_of_match),
	},
};

int __init regulator_void_init(void)
{
	return platform_driver_register(&regulator_void_driver);
}
//subsys_initcall(regulator_void_init);

void __exit regulator_void_exit(void)
{
	platform_driver_unregister(&regulator_void_driver);
}
//module_exit(regulator_void_exit);
