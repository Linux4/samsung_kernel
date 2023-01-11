/*
 * virtual regulator driver for Marvell 88PM88X
 *
 * Copyright (C) 2015 Marvell International Ltd.
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
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm880.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>

#define PM88X_VR_EN		(0x28)

#define PM886_BUCK1_SLP_VOUT	(0xa3)
#define PM886_BUCK1_SLP_EN	(0xa2)

#define PM880_BUCK1A_SLP_VOUT	(0x26)
#define PM880_BUCK1A_SLP_EN	(0x24)
#define PM880_BUCK1B_SLP_VOUT	(0x3e)
#define PM880_BUCK1B_SLP_EN	(0x3c)

#define PM880_BUCK1A_AUDIO_VOUT (0x27)
#define PM880_BUCK1A_AUDIO_EN	(0x27)
#define PM880_BUCK1B_AUDIO_EN	(0x3f)
#define PM880_BUCK1B_AUDIO_VOUT (0x3f)

#define PM88X_VR(vreg, ebit, nr)					\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm88x_virtual_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM88X##_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.enable_reg	= PM88X##_VR_EN,			\
		.enable_mask	= 1 << (ebit),				\
	},								\
	.page_nr = nr,							\
}

#define PM88X_BUCK_SLP(_pmic, vreg, ebit, nr, volt_ranges, n_volt)	\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm88x_buck_slp_ops,				\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages		= n_volt,			\
		.linear_ranges		= volt_ranges,			\
		.n_linear_ranges	= ARRAY_SIZE(volt_ranges),	\
		.vsel_reg	= _pmic##_##vreg##_VOUT,		\
		.vsel_mask	= 0x7f,					\
		.enable_reg	= _pmic##_##vreg##_EN,			\
		.enable_mask	= (0x3) << (ebit),			\
	},								\
	.page_nr = nr,							\
}

#define PM88X_BUCK_AUDIO(_pmic, vreg, ebit, nr, volt_ranges, n_volt)	\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm88x_buck_audio_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages		= n_volt,			\
		.linear_ranges		= volt_ranges,			\
		.n_linear_ranges	= ARRAY_SIZE(volt_ranges),	\
		.vsel_reg	= _pmic##_##vreg##_VOUT,		\
		.vsel_mask	= 0x7f,					\
		.enable_reg	= _pmic##_##vreg##_EN,			\
		.enable_mask	= (0x1) << (ebit),			\
	},								\
	.page_nr = nr,							\
}

#define PM880_BUCK_SLP(vreg, ebit, nr, volt_ranges, n_volt)		\
	PM88X_BUCK_SLP(PM880, vreg, ebit, nr, volt_ranges, n_volt)

#define PM886_BUCK_SLP(vreg, ebit, nr, volt_ranges, n_volt)		\
	PM88X_BUCK_SLP(PM886, vreg, ebit, nr, volt_ranges, n_volt)

#define PM880_BUCK_AUDIO(vreg, ebit, nr, volt_ranges, n_volt)		\
	PM88X_BUCK_AUDIO(PM880, vreg, ebit, nr, volt_ranges, n_volt)

#define PM88X_VR_OF_MATCH(comp, label) \
	{ \
		.compatible = comp, \
		.data = &pm88x_vr_configs[PM88X_ID##_##label], \
	}

#define PM88X_BUCK_SLP_OF_MATCH(_pmic, id, comp, label) \
	{ \
		.compatible = comp, \
		.data = &_pmic##_buck_slp_configs[id##_##label], \
	}

#define PM88X_BUCK_AUDIO_OF_MATCH(_pmic, id, comp, label) \
	{ \
		.compatible = comp, \
		.data = &_pmic##_buck_audio_configs[id##_##label], \
	}

#define PM880_BUCK_SLP_OF_MATCH(comp, label) \
	PM88X_BUCK_SLP_OF_MATCH(pm880, PM880_ID, comp, label)

#define PM886_BUCK_SLP_OF_MATCH(comp, label) \
	PM88X_BUCK_SLP_OF_MATCH(pm886, PM886_ID, comp, label)

#define PM880_BUCK_AUDIO_OF_MATCH(comp, label) \
	PM88X_BUCK_AUDIO_OF_MATCH(pm880, PM880_ID, comp, label)

static const struct regulator_linear_range buck_slp_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 0x4f, 12500),
	REGULATOR_LINEAR_RANGE(1600000, 0x50, 0x54, 50000),
};

static const struct regulator_linear_range buck_audio_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 0x54, 12500),
};

struct pm88x_vr_info {
	struct regulator_desc desc;
	unsigned int page_nr;
};

struct pm88x_buck_slp_info {
	struct regulator_desc desc;
	unsigned int page_nr;
};

struct pm88x_buck_audio_info {
	struct regulator_desc desc;
	unsigned int page_nr;
};

struct pm88x_regulators {
	struct regulator_dev *rdev;
	struct pm88x_chip *chip;
	struct regmap *map;
};

struct pm88x_vr_print {
	char name[15];
	char enable[15];
	char slp_mode[15];
	char set_slp[15];
	char volt[10];
	char audio_en[15];
	char audio[10];
};

static struct regulator_ops pm88x_virtual_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static int pm88x_buck_slp_enable(struct regulator_dev *rdev)
{
	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, 0x2);
}

static int pm88x_buck_slp_disable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev, "%s: this buck is _off_ in suspend.\n", __func__);
	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, 0x0);
}

static int pm88x_buck_slp_is_enabled(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	ret = regmap_read(rdev->regmap, rdev->desc->enable_reg, &val);
	if (ret != 0)
		return ret;

	return !!((val & rdev->desc->enable_mask) == (0x2 << 5));
}

static int pm88x_buck_slp_map_voltage(struct regulator_dev *rdev,
				      int min_uV, int max_uV)
{
	/* use higest voltage */
	if (max_uV >= 1600000)
		return 0x50 + (max_uV - 1600000) / 50000;
	else
		return (max_uV - 600000) / 12500;

	return -EINVAL;
}

static struct regulator_ops pm88x_buck_slp_ops = {
	.enable = pm88x_buck_slp_enable,
	.disable = pm88x_buck_slp_disable,
	.is_enabled = pm88x_buck_slp_is_enabled,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = pm88x_buck_slp_map_voltage,
};

static int pm88x_buck_audio_enable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev, "%s is empty.\n", __func__);
	return 0;
}

static int pm88x_buck_audio_disable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev, "%s is empty.\n", __func__);
	return 0;
}

static int pm88x_buck_audio_is_enabled(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev, "%s is empty.\n", __func__);
	return 0;
}

static struct regulator_ops pm88x_buck_audio_ops = {
	.enable = pm88x_buck_audio_enable,
	.disable = pm88x_buck_audio_disable,
	.is_enabled = pm88x_buck_audio_is_enabled,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
};

static struct pm88x_vr_info pm88x_vr_configs[] = {
	PM88X_VR(VOTG, 7, 3),
};

static struct pm88x_buck_slp_info pm880_buck_slp_configs[] = {
	PM880_BUCK_SLP(BUCK1A_SLP, 4, 4, buck_slp_volt_range1, 0x55),
	PM880_BUCK_SLP(BUCK1B_SLP, 4, 4, buck_slp_volt_range1, 0x55),
};

static struct pm88x_buck_slp_info pm886_buck_slp_configs[] = {
	PM886_BUCK_SLP(BUCK1_SLP, 4, 1, buck_slp_volt_range1, 0x55),
};

static struct pm88x_buck_audio_info pm880_buck_audio_configs[] = {
	PM880_BUCK_AUDIO(BUCK1A_AUDIO, 7, 4, buck_audio_volt_range1, 0x50),
	PM880_BUCK_AUDIO(BUCK1B_AUDIO, 7, 4, buck_audio_volt_range1, 0x50),
};

static const struct of_device_id pm88x_vrs_of_match[] = {
	PM88X_VR_OF_MATCH("marvell,88pm88x-votg", VOTG),

	PM886_BUCK_SLP_OF_MATCH("marvell,88pm886-buck1-slp", BUCK1_SLP),

	PM880_BUCK_SLP_OF_MATCH("marvell,88pm880-buck1a-slp", BUCK1A_SLP),
	PM880_BUCK_SLP_OF_MATCH("marvell,88pm880-buck1b-slp", BUCK1B_SLP),

	PM880_BUCK_AUDIO_OF_MATCH("marvell,88pm880-buck1a-audio", BUCK1A_AUDIO),
	PM880_BUCK_AUDIO_OF_MATCH("marvell,88pm880-buck1b-audio", BUCK1B_AUDIO),
};

static struct regmap *nr_to_regmap(struct pm88x_chip *chip, unsigned int nr)
{
	switch (nr) {
	case 0:
		return chip->base_regmap;
	case 1:
		return chip->ldo_regmap;
	case 2:
		return chip->gpadc_regmap;
	case 3:
		return chip->battery_regmap;
	case 4:
		return chip->buck_regmap;
	case 7:
		return chip->test_regmap;
	default:
		pr_err("unsupported pages.\n");
		return NULL;
	}
}

static int of_get_legacy_init_data(struct device *dev,
				   struct regulator_init_data **init_data)
{
	struct device_node *node = dev->of_node;
	struct regulator_consumer_supply *consumer_supplies;
	int len, num, ret, i;

	len =  of_property_count_strings(node, "marvell,consumer-supplies");
	if (len <= 0)
		return 0;

	/* format: marvell,consumer-supplies = "supply-name", "dev-name" */
	if (len % 2 != 0) {
		dev_err(dev,
			"format should be: marvell,consumer-supplies = supply-name, dev-name\n");

		return -EINVAL;
	}

	num = len / 2;
	consumer_supplies = devm_kzalloc(dev,
					  sizeof(struct regulator_consumer_supply) * num,
					  GFP_KERNEL);
	if (!consumer_supplies) {
		dev_err(dev, "alloc memory fails.\n");
		return -ENOMEM;
	}

	for (i = 0; i < num; i++) {
		ret = of_property_read_string_index(node,
						    "marvell,consumer-supplies",
						    i * 2,
						    &consumer_supplies[i].supply);
		if (ret) {
			dev_err(dev, "read property fails.\n");
			devm_kfree(dev, consumer_supplies);
			return ret;
		}

		ret = of_property_read_string_index(node,
						    "marvell,consumer-supplies",
						    i * 2 + 1,
						    &consumer_supplies[i].dev_name);
		if (ret) {
			dev_err(dev, "read property fails.\n");
			devm_kfree(dev, consumer_supplies);
			return ret;
		}

		if (!strcmp((consumer_supplies[i].dev_name), "nameless"))
			consumer_supplies[i].dev_name = NULL;

	}

	(*init_data)->consumer_supplies = consumer_supplies;
	(*init_data)->num_consumer_supplies = num;

	return 0;
}

/*
 * The function convert the vr voltage register value
 * to a real voltage value (in uV) according to the voltage table.
 */
static int pm88x_get_vvr_vol(unsigned int val, unsigned int n_linear_ranges,
			     const struct regulator_linear_range *ranges)
{
	const struct regulator_linear_range *range;
	int i, volt = -EINVAL;

	/* get the voltage via the register value */
	for (i = 0; i < n_linear_ranges; i++) {
		range = &ranges[i];
		if (!range)
			return -EINVAL;

		if (val >= range->min_sel && val <= range->max_sel) {
			volt = (val - range->min_sel) * range->uV_step + range->min_uV;
			break;
		}
	}
	return volt;
}

/* The function check if the regulator register is configured to enable/disable */
static int pm88x_check_en(struct regmap *map, unsigned int reg, unsigned int mask)
{
	int ret, value;
	unsigned int enable1;

	ret = regmap_read(map, reg, &enable1);
	if (ret < 0)
		return ret;

	value = enable1 & mask;

	return value;
}

/* The function return the value in the regulator voltage register */
static unsigned int pm88x_check_vol(struct regmap *map, unsigned int reg, unsigned int mask)
{
	int ret;
	unsigned int vol_val;

	ret = regmap_bulk_read(map, reg, &vol_val, 1);
	if (ret < 0)
		return ret;

	/* mask and shift the relevant value from the register */
	vol_val = (vol_val & mask) >> (ffs(mask) - 1);

	return vol_val;
}

static int pm88x_update_print(struct pm88x_chip *chip, struct regmap *map,
			      struct regulator_desc *desc, struct pm88x_vr_print *print_temp,
			      int index, int num)
{
	int ret, volt;

	sprintf(print_temp->name, "%s", desc->name);

	/* check enable/disable */
	ret = pm88x_check_en(map, desc->enable_reg, desc->enable_mask);
	if (ret < 0)
		return ret;
	else if (ret)
		strcpy(print_temp->enable, "enable");
	else
		strcpy(print_temp->enable, "disable");

	/* no sleep mode */
	strcpy(print_temp->slp_mode, "   -");

	/* no sleep voltage */
	sprintf(print_temp->set_slp, "   -");

	/* print active voltage(s) */
	ret = pm88x_check_vol(map, desc->vsel_reg, desc->vsel_mask);
	if (ret < 0)
		return ret;

	if (desc->n_linear_ranges) {
		volt = pm88x_get_vvr_vol(ret, desc->n_linear_ranges, desc->linear_ranges);
		if (volt < 0)
			return volt;
		else
			sprintf(print_temp->volt, "%4d", volt/1000);
	} else {
		sprintf(print_temp->volt, "   -");
	}
	/* no audio mode*/
	strcpy(print_temp->audio_en, "   -   ");
	sprintf(print_temp->audio, "  -");

	return 0;
}

int pm88x_display_vr(struct pm88x_chip *chip, char *buf)
{
	struct pm88x_vr_print *print_temp;
	struct pm88x_buck_slp_info *slp_info = NULL;
	struct pm88x_buck_audio_info *audio_info = NULL;
	struct pm88x_vr_info *vr_info = NULL;
	struct regmap *map;
	int slp_num, audio_num, vr_num, i, len = 0;
	ssize_t ret;

	switch (chip->type) {
	case PM886:
		slp_info = pm886_buck_slp_configs;
		slp_num = ARRAY_SIZE(pm886_buck_slp_configs);
		vr_info = pm88x_vr_configs;
		vr_num = ARRAY_SIZE(pm88x_vr_configs);
		break;
	case PM880:
		slp_info = pm880_buck_slp_configs;
		slp_num = ARRAY_SIZE(pm880_buck_slp_configs);
		audio_info = pm880_buck_audio_configs;
		audio_num = ARRAY_SIZE(pm880_buck_audio_configs);
		vr_info = pm88x_vr_configs;
		vr_num = ARRAY_SIZE(pm88x_vr_configs);
		break;
	default:
		pr_err("%s: Cannot find chip type.\n", __func__);
		return -ENODEV;
	}

	print_temp = kmalloc(sizeof(struct pm88x_vr_print), GFP_KERNEL);
	if (!print_temp) {
		pr_err("%s: Cannot allocate print template.\n", __func__);
		return -ENOMEM;
	}

	len += sprintf(buf + len, "\nVirtual Regulator");
	len += sprintf(buf + len, "\n------------------------------------");
	len += sprintf(buf + len, "--------------------------------------\n");
	len += sprintf(buf + len, "|    name    | status  |  slp_mode  |slp_volt");
	len += sprintf(buf + len, "|  volt  | audio_en| audio  |\n");
	len += sprintf(buf + len, "-------------------------------------");
	len += sprintf(buf + len, "-------------------------------------\n");

	if (slp_info) {
		map = nr_to_regmap(chip, slp_info->page_nr);
		for (i = 0; i < slp_num; i++) {
			ret = pm88x_update_print(chip, map, &slp_info[i].desc,
						 print_temp, i, slp_num);
			if (ret < 0) {
				pr_err("Print of regulator %s failed\n", print_temp->name);
				goto out_print;
			}
			len += sprintf(buf + len, "|%-12s|", print_temp->name);
			len += sprintf(buf + len, " %-7s |", print_temp->enable);
			len += sprintf(buf + len, "  %-10s|", print_temp->slp_mode);
			len += sprintf(buf + len, "  %-5s |", print_temp->set_slp);
			len += sprintf(buf + len, "  %-5s |", print_temp->volt);
			len += sprintf(buf + len, " %-7s |", print_temp->audio_en);
			len += sprintf(buf + len, "  %-5s |\n", print_temp->audio);
		}
	}

	if (audio_info) {
		map = nr_to_regmap(chip, audio_info->page_nr);
		for (i = 0; i < audio_num; i++) {
			ret = pm88x_update_print(chip, map, &audio_info[i].desc,
						 print_temp, i, audio_num);
			if (ret < 0) {
				pr_err("Print of regulator %s failed\n", print_temp->name);
				goto out_print;
			}
			len += sprintf(buf + len, "|%-12s|", print_temp->name);
			len += sprintf(buf + len, " %-7s |", print_temp->enable);
			len += sprintf(buf + len, "  %-10s|", print_temp->slp_mode);
			len += sprintf(buf + len, "  %-5s |", print_temp->set_slp);
			len += sprintf(buf + len, "  %-5s |", print_temp->volt);
			len += sprintf(buf + len, " %-7s |", print_temp->audio_en);
			len += sprintf(buf + len, "  %-5s |\n", print_temp->audio);
		}
	}

	if (vr_info) {
		map = nr_to_regmap(chip, vr_info->page_nr);
		for (i = 0; i < vr_num; i++) {
			ret = pm88x_update_print(chip, map, &vr_info[i].desc,
						 print_temp, i, vr_num);
			if (ret < 0) {
				pr_err("Print of regulator %s failed\n", print_temp->name);
				goto out_print;
			}
			len += sprintf(buf + len, "|%-12s|", print_temp->name);
			len += sprintf(buf + len, " %-7s |", print_temp->enable);
			len += sprintf(buf + len, "  %-10s|", print_temp->slp_mode);
			len += sprintf(buf + len, "  %-5s |", print_temp->set_slp);
			len += sprintf(buf + len, "  %-5s |", print_temp->volt);
			len += sprintf(buf + len, " %-7s |", print_temp->audio_en);
			len += sprintf(buf + len, "  %-5s |\n", print_temp->audio);
		}
	}

	len += sprintf(buf + len, "-------------------------------------");
	len += sprintf(buf + len, "-------------------------------------\n");

	ret = len;
out_print:
	kfree(print_temp);
	return ret;
}

static int pm88x_virtual_regulator_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_regulators *data;
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	struct regulation_constraints *c;
	const struct of_device_id *match;
	const struct pm88x_vr_info *const_info;
	struct pm88x_vr_info *info;
	int ret;

	match = of_match_device(pm88x_vrs_of_match, &pdev->dev);
	if (match) {
		const_info = match->data;
		init_data = of_get_regulator_init_data(&pdev->dev,
						       pdev->dev.of_node);
		ret = of_get_legacy_init_data(&pdev->dev, &init_data);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&pdev->dev, "parse dts fails!\n");
		return -EINVAL;
	}

	info = kmemdup(const_info, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	data = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_regulators),
			    GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to allocate pm88x_regualtors");
		return -ENOMEM;
	}
	data->map = nr_to_regmap(chip, info->page_nr);
	data->chip = chip;

	/* add regulator config */
	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.driver_data = info;
	config.regmap = data->map;
	config.of_node = pdev->dev.of_node;

	data->rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
	if (IS_ERR(data->rdev)) {
		dev_err(&pdev->dev, "cannot register %s\n", info->desc.name);
		ret = PTR_ERR(data->rdev);
		return ret;
	}

	c = data->rdev->constraints;
	if (info->desc.ops->enable)
		c->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	if (info->desc.ops->set_voltage_sel)
		c->valid_ops_mask |= REGULATOR_CHANGE_VOLTAGE;

	platform_set_drvdata(pdev, data);

	return 0;
}

static int pm88x_virtual_regulator_remove(struct platform_device *pdev)
{
	struct pm88x_regulators *data = platform_get_drvdata(pdev);
	devm_kfree(&pdev->dev, data);
	return 0;
}

static struct platform_driver pm88x_virtual_regulator_driver = {
	.driver		= {
		.name	= "88pm88x-virtual-regulator",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_vrs_of_match),
	},
	.probe		= pm88x_virtual_regulator_probe,
	.remove		= pm88x_virtual_regulator_remove,
};

static int pm88x_virtual_regulator_init(void)
{
	return platform_driver_register(&pm88x_virtual_regulator_driver);
}
subsys_initcall(pm88x_virtual_regulator_init);

static void pm88x_virtual_regulator_exit(void)
{
	platform_driver_unregister(&pm88x_virtual_regulator_driver);
}
module_exit(pm88x_virtual_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_DESCRIPTION("for virtual supply in Marvell 88PM88X PMIC");
MODULE_ALIAS("platform:88pm88x-vr");
