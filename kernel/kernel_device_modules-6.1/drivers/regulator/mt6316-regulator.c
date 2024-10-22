// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6316-regulator.h>
#include <linux/regulator/of_regulator.h>

#define SET_OFFSET	0x1
#define CLR_OFFSET	0x2

#define MT6316_REG_WIDTH	8

#define MT6316_BUCK_MODE_AUTO		0
#define MT6316_BUCK_MODE_FORCE_PWM	1
#define MT6316_BUCK_MODE_NORMAL		0
#define MT6316_BUCK_MODE_LP		2

#define BUCK_PHASE_3			3
#define BUCK_PHASE_4			4

struct mt6316_regulator_info {
	struct regulator_desc desc;
	u32 da_reg;
	u32 qi;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 lp_mode_shift;
};

struct mt6316_init_data {
	u32 id;
	u32 size;
};

struct mt6316_chip {
	struct device *dev;
	struct regmap *regmap;
	u32 slave_id;
};

static unsigned int s6_buck_phase;
static unsigned int s7_buck_phase;

#define MT_BUCK(match, _name, volt_ranges, _bid, _vsel)	\
[MT6316_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.ops = &mt6316_volt_range_ops,		\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6316_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 0x1ff,			\
		.linear_ranges = volt_ranges,		\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),\
		.vsel_reg = _vsel,			\
		.vsel_mask = 0xff,			\
		.enable_reg = MT6316_BUCK_TOP_CON0,	\
		.enable_mask = BIT(_bid - 1),		\
		.of_map_mode = mt6316_map_mode,		\
	},						\
	.da_reg = MT6316_VBUCK##_bid##_DBG8,		\
	.qi = BIT(0),					\
	.lp_mode_reg = MT6316_BUCK_TOP_CON1,		\
	.lp_mode_mask = BIT(_bid - 1),			\
	.lp_mode_shift = _bid - 1,			\
	.modeset_reg = MT6316_BUCK_TOP_4PHASE_TOP_ANA_CON0,\
	.modeset_mask = BIT(_bid - 1),			\
}

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 0x1fe, 2500),
};

static int mt6316_regulator_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6316_regulator_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static unsigned int mt6316_map_mode(u32 mode)
{
	switch (mode) {
	case MT6316_BUCK_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	case MT6316_BUCK_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	case MT6316_BUCK_MODE_LP:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static int mt6316_regulator_set_voltage_sel(struct regulator_dev *rdev, unsigned int selector)
{
	unsigned short reg_val = 0;
	int ret = 0;

	reg_val = ((selector & 0x1) << 8) | (selector >> 1);
	ret = regmap_bulk_write(rdev->regmap, rdev->desc->vsel_reg, (u8 *) &reg_val, 2);

	return ret;
}

static int mt6316_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret = 0;
	unsigned int reg_val = 0;

	ret = regmap_bulk_read(rdev->regmap, rdev->desc->vsel_reg, (u8 *) &reg_val, 2);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get mt6316 regulator voltage: %d\n", ret);
		return ret;
	}
	ret = ((reg_val >> 8) & 0x1) + ((reg_val & rdev->desc->vsel_mask) << 1);

	return ret;
}

static unsigned int mt6316_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6316_regulator_info *info;
	int ret = 0, regval = 0;
	u32 modeset_mask;

	info = container_of(rdev->desc, struct mt6316_regulator_info, desc);
	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get mt6316 buck mode: %d\n", ret);
		return ret;
	}

	modeset_mask = info->modeset_mask;

	if ((regval & modeset_mask) == modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get mt6316 buck lp mode: %d\n", ret);
		return ret;
	}

	if (regval & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6316_regulator_set_mode(struct regulator_dev *rdev, u32 mode)
{
	struct mt6316_regulator_info *info;
	int ret = 0, val, curr_mode;
	u32 modeset_mask;

	info = container_of(rdev->desc, struct mt6316_regulator_info, desc);
	modeset_mask = info->modeset_mask;

	curr_mode = mt6316_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = regmap_update_bits(rdev->regmap, info->modeset_reg,
					 modeset_mask, modeset_mask);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = regmap_update_bits(rdev->regmap, info->modeset_reg,
						 modeset_mask, 0);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			ret = regmap_update_bits(rdev->regmap, info->lp_mode_reg,
						 info->lp_mode_mask, 0);
			usleep_range(100, 110);
		}
		break;
	case REGULATOR_MODE_IDLE:
		val = MT6316_BUCK_MODE_LP >> 1;
		val <<= info->lp_mode_shift;
		ret = regmap_update_bits(rdev->regmap, info->lp_mode_reg, info->lp_mode_mask, val);
		break;
	default:
		ret = -EINVAL;
		goto err_mode;
	}

err_mode:
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to set mt6316 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6316_get_status(struct regulator_dev *rdev)
{
	int ret = 0;
	u32 regval = 0;
	struct mt6316_regulator_info *info;

	info = container_of(rdev->desc, struct mt6316_regulator_info, desc);
	ret = regmap_read(rdev->regmap, info->da_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static void mt6316_buck_phase_init(struct mt6316_chip *chip)
{
	int ret = 0;
	u32 val = 0;

	ret = regmap_read(chip->regmap, MT6316_BUCK_TOP_4PHASE_TOP_ELR_0, &val);
	if (ret != 0) {
		dev_err(chip->dev, "Failed to get mt6316 buck phase: %d\n", ret);
	} else {
		dev_info(chip->dev, "S%d RG_4PH_CONFIG:%d\n", chip->slave_id, val);

		if (chip->slave_id == MT6316_SLAVE_ID_6)
			s6_buck_phase = val;
		else if (chip->slave_id == MT6316_SLAVE_ID_7)
			s7_buck_phase = val;
	}
}

static const struct regulator_ops mt6316_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = mt6316_regulator_set_voltage_sel,
	.get_voltage_sel = mt6316_regulator_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6316_regulator_enable,
	.disable = mt6316_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6316_get_status,
	.set_mode = mt6316_regulator_set_mode,
	.get_mode = mt6316_regulator_get_mode,
};

static struct mt6316_regulator_info mt6316_regulators[] = {
	MT_BUCK("vbuck1", VBUCK1, mt_volt_range1, 1, MT6316_BUCK_TOP_ELR0),
	MT_BUCK("vbuck2", VBUCK2, mt_volt_range1, 2, MT6316_BUCK_TOP_ELR2),
	MT_BUCK("vbuck3", VBUCK3, mt_volt_range1, 3, MT6316_BUCK_TOP_ELR4),
	MT_BUCK("vbuck4", VBUCK4, mt_volt_range1, 4, MT6316_BUCK_TOP_ELR6),
};

static struct mt6316_init_data mt6316_3_init_data = {
	.id = MT6316_SLAVE_ID_3,
	.size = MT6316_ID_3_MAX,
};

static struct mt6316_init_data mt6316_6_init_data = {
	.id = MT6316_SLAVE_ID_6,
	.size = MT6316_ID_6_MAX,
};

static struct mt6316_init_data mt6316_7_init_data = {
	.id = MT6316_SLAVE_ID_7,
	.size = MT6316_ID_7_MAX,
};

static struct mt6316_init_data mt6316_8_init_data = {
	.id = MT6316_SLAVE_ID_8,
	.size = MT6316_ID_8_MAX,
};

static struct mt6316_init_data mt6316_15_init_data = {
	.id = MT6316_SLAVE_ID_15,
	.size = MT6316_ID_15_MAX,
};

static const struct of_device_id mt6316_of_match[] = {
	{
		.compatible = "mediatek,mt6316-3-regulator",
		.data = &mt6316_3_init_data,
	}, {
		.compatible = "mediatek,mt6316-6-regulator",
		.data = &mt6316_6_init_data,
	}, {
		.compatible = "mediatek,mt6316-7-regulator",
		.data = &mt6316_7_init_data,
	}, {
		.compatible = "mediatek,mt6316-8-regulator",
		.data = &mt6316_8_init_data,
	}, {
		.compatible = "mediatek,mt6316-15-regulator",
		.data = &mt6316_15_init_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mt6316_of_match);

static int mt6316_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct mt6316_init_data *pdata;
	struct mt6316_chip *chip;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct device_node *node = pdev->dev.of_node;
	u32 val = 0;
	int i;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	chip = devm_kzalloc(dev, sizeof(struct mt6316_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	of_id = of_match_device(mt6316_of_match, dev);
	if (!of_id || !of_id->data)
		return -ENODEV;

	pdata = (struct mt6316_init_data *)of_id->data;
	chip->slave_id = pdata->id;
	if (!of_property_read_u32(node, "buck-size", &val))
		pdata->size = val;
	chip->dev = dev;
	chip->regmap = regmap;
	dev_set_drvdata(dev, chip);

	dev->fwnode = &(dev->of_node->fwnode);
	if (dev->fwnode && !dev->fwnode->dev)
		dev->fwnode->dev = dev;

	config.dev = dev;
	config.driver_data = pdata;
	config.regmap = regmap;

	mt6316_buck_phase_init(chip);
	for (i = 0; i < pdata->size; i++) {
		if (pdata->id == MT6316_SLAVE_ID_6 &&
		    s6_buck_phase == BUCK_PHASE_4 &&
		    (mt6316_regulators + i)->desc.id == MT6316_ID_VBUCK3) {
			dev_info(dev, "skip registering %s.\n", (mt6316_regulators + i)->desc.name);
			continue;
		}

		rdev = devm_regulator_register(dev, &(mt6316_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(dev, "failed to register %s\n", (mt6316_regulators + i)->desc.name);
			continue;
		}
	}

	return 0;
}

static struct platform_driver mt6316_regulator_driver = {
	.driver		= {
		.name	= "mt6316-regulator",
		.of_match_table = mt6316_of_match,
	},
	.probe = mt6316_regulator_probe,
};

module_platform_driver(mt6316_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6316 PMIC");
MODULE_LICENSE("GPL");
