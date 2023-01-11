/*
 * Regulators driver for Marvell 88PM800
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
 * Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#include "88pm8xx-regulator.h"

static int pm800_set_voltage(struct regulator_dev *rdev,
			     int min_uv, int max_uv, unsigned *selector)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, old_selector = -1, best_val = 0;

	if (!info)
		return -EINVAL;

	if (info->desc.id == PM800_ID_VOUTSW)
		return 0;

	ret = regulator_map_voltage_iterate(rdev, min_uv, max_uv);
	if (ret >= 0) {
		best_val = rdev->desc->ops->list_voltage(rdev, ret);
		if (min_uv <= best_val && max_uv >= best_val) {
			*selector = ret;
			if (old_selector == *selector)
				ret = 0;
			else
				ret = regulator_set_voltage_sel_regmap(rdev, ret);
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int pm800_get_voltage(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	int sel, ret;

	if (!info)
		return -EINVAL;

	if (info->desc.id == PM800_ID_VOUTSW)
		return 0;

	sel = regulator_get_voltage_sel_regmap(rdev);
	if (sel < 0)
		return sel;

	ret = rdev->desc->ops->list_voltage(rdev, sel);

	return ret;
}

int pm800_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val, sleep_bit, sleep_enable_mask, reg;

	sleep_bit = info->sleep_enable_bit;
	sleep_enable_mask = (0x3 << sleep_bit);

	switch (mode) {
	case REGULATOR_MODE_IDLE:
		val = (0x2 << sleep_bit);
		break;
	case REGULATOR_MODE_NORMAL:
		val = (0x3 << sleep_bit);
		break;
	default:
		dev_err(rdev_get_dev(rdev), "not supported mode.\n");
		return -EINVAL;
	}

	reg = info->sleep_enable_reg;

	return regmap_update_bits(rdev->regmap, reg, sleep_enable_mask, val);
}

static unsigned int pm800_get_optimum_mode(struct regulator_dev *rdev,
						 int input_uV, int output_uV,
						 int current_uA)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);

	if (!info) {
		dev_err(rdev_get_dev(rdev),
			"return REGULATOR_MODE_IDLE by default\n");
		return REGULATOR_MODE_IDLE;
	}
	if (current_uA < 0) {
		dev_err(rdev_get_dev(rdev),
			"return REGULATOR_MODE_IDLE for unexpected current\n");
		return REGULATOR_MODE_IDLE;
	}
	/*
	 * get_optimum_mode be called at enbale/disable_regulator function.
	 * If current_uA is not set it will be 0,
	 * set defult value to be REGULATOR_MODE_IDLE.
	 */
	return (MAX_SLEEP_CURRENT > current_uA) ?
		REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
}

static int pm800_get_current_limit(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);

	return info->max_ua;
}

static int pm800_set_suspend_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	struct pm80x_chip *chip = info->chip;
	unsigned int val, sleep_bit, sleep_enable_mask, reg, rid;

	if (!info)
		return -EINVAL;

	rid = rdev_get_id(rdev);
	/* only handle 88pm860 buck1 audio mode */
	if ((rid != PM800_ID_BUCK1) && (rid != PM800_ID_BUCK1A))
		return -EINVAL;

	sleep_bit = info->sleep_enable_bit;
	sleep_enable_mask = (0x1 << sleep_bit);
	reg = info->sleep_enable_reg;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	case REGULATOR_MODE_IDLE:
		val = (0x1 << sleep_bit);
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(chip->regmap, reg, sleep_enable_mask, val);
}

static int pm800_set_suspend_voltage(struct regulator_dev *rdev, int uv)
{
	int ret, sel, rid;
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);

	if (!info || !info->desc.ops)
		return -EINVAL;

	if (!info->desc.ops->set_suspend_mode)
		return 0;

	/* only handle pm860 buck1 audio mode */
	rid = rdev_get_id(rdev);
	if ((rid != PM800_ID_BUCK1) && (rid != PM800_ID_BUCK1A))
		return -EINVAL;

	/*
	 * two steps:
	 * 1) set the suspend voltage to *_set_slp register
	 * 2) set regulator mode via set_suspend_mode() interface to enable output
	 */
	sel = regulator_map_voltage_linear_range(rdev, uv, uv);
	if (sel < 0)
		return -EINVAL;

	sel <<= ffs(PM800_BUCK1_AUDIO_SET_MSK) - 1;

	ret = regmap_update_bits(rdev->regmap, PM800_BUCK1_AUDIO_SET,
				PM800_BUCK1_AUDIO_SET_MSK, sel);
	if (ret < 0)
		return -EINVAL;

	/* TODO: do we need this? */
	ret = pm800_set_suspend_mode(rdev, REGULATOR_MODE_IDLE);
	return ret;
}

struct regulator_ops pm800_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage = pm800_set_voltage,
	.get_voltage = pm800_get_voltage,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm800_get_current_limit,
	.get_optimum_mode = pm800_get_optimum_mode,
	.set_mode = pm800_set_mode,
	.set_suspend_mode = pm800_set_suspend_mode,
	.set_suspend_voltage = pm800_set_suspend_voltage,
};

struct regulator_ops pm800_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm800_get_current_limit,
	.get_optimum_mode = pm800_get_optimum_mode,
	.set_mode = pm800_set_mode,
};

static int pm800_regulator_dt_init(struct platform_device *pdev,
	struct of_regulator_match **regulator_matches, int *range)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np = pdev->dev.of_node;

	switch (chip->type) {
		case CHIP_PM800:
			*regulator_matches = pm800_regulator_matches;
			*range = PM800_ID_RG_MAX;
			break;
		case CHIP_PM822:
			*regulator_matches = pm822_regulator_matches;
			*range = PM822_ID_RG_MAX;
			break;
		case CHIP_PM86X:
			*regulator_matches = pm86x_regulator_matches;
			*range = PM86X_ID_RG_MAX;
			break;
		default:
			return -ENODEV;
	}

	return of_regulator_match(&pdev->dev, np, *regulator_matches, *range);
}

static int pm800_regulator_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct pm800_regulators *pm800_data;
	struct pm800_regulator_info *info;
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	int i, ret, range = 0;
	struct of_regulator_match *regulator_matches;

	if (!pdata || pdata->num_regulators == 0) {
		if (IS_ENABLED(CONFIG_OF)) {
			ret = pm800_regulator_dt_init(pdev, &regulator_matches,
						      &range);
			if (ret < 0)
				return ret;
		} else {
			return -ENODEV;
		}
	} else if (pdata->num_regulators) {
		unsigned int count = 0;

		/* Check whether num_regulator is valid. */
		for (i = 0; i < ARRAY_SIZE(pdata->regulators); i++) {
			if (pdata->regulators[i])
				count++;
		}
		if (count != pdata->num_regulators)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	pm800_data = devm_kzalloc(&pdev->dev, sizeof(*pm800_data),
					GFP_KERNEL);
	if (!pm800_data) {
		dev_err(&pdev->dev, "Failed to allocate pm800_regualtors");
		return -ENOMEM;
	}

	pm800_data->map = chip->subchip->regmap_power;
	pm800_data->chip = chip;

	platform_set_drvdata(pdev, pm800_data);

	for (i = 0; i < range; i++) {
		if (!pdata || pdata->num_regulators == 0)
			init_data = regulator_matches->init_data;
		else
			init_data = pdata->regulators[i];
		if (!init_data) {
			regulator_matches++;
			continue;
		}
		info = regulator_matches->driver_data;
		info->chip = chip;
		config.dev = &pdev->dev;
		config.init_data = init_data;
		config.driver_data = info;
		config.regmap = pm800_data->map;
		config.of_node = regulator_matches->of_node;

		pm800_data->regulators[i] =
				regulator_register(&info->desc, &config);
		if (IS_ERR(pm800_data->regulators[i])) {
			ret = PTR_ERR(pm800_data->regulators[i]);
			dev_err(&pdev->dev, "Failed to register %s\n",
				info->desc.name);

			while (--i >= 0)
				regulator_unregister(pm800_data->regulators[i]);

			return ret;
		}

		pm800_data->regulators[i]->constraints->valid_ops_mask |=
				(REGULATOR_CHANGE_DRMS | REGULATOR_CHANGE_MODE);
		pm800_data->regulators[i]->constraints->valid_modes_mask |=
				(REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE);
		pm800_data->regulators[i]->constraints->input_uV = 1000;

		regulator_matches++;
	}

	return 0;
}

static int pm800_regulator_remove(struct platform_device *pdev)
{
	struct pm800_regulators *pm800_data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < PM800_ID_RG_MAX; i++)
		regulator_unregister(pm800_data->regulators[i]);

	return 0;
}

static struct platform_driver pm800_regulator_driver = {
	.driver		= {
		.name	= "88pm80x-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= pm800_regulator_probe,
	.remove		= pm800_regulator_remove,
};

static int __init pm800_regulator_init(void)
{
	return platform_driver_register(&pm800_regulator_driver);
}

subsys_initcall(pm800_regulator_init);

static void __exit pm800_regulator_exit(void)
{
	platform_driver_unregister(&pm800_regulator_driver);
}

module_exit(pm800_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph(Yossi) Hanin <yhanin@marvell.com>");
MODULE_DESCRIPTION("Regulator Driver for Marvell 88PM800 PMIC");
MODULE_ALIAS("platform:88pm800-regulator");
