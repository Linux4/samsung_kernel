// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6681-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6681_REGULATOR_MODE_NORMAL	0
#define MT6681_REGULATOR_MODE_LP	2

#define DEFAULT_DELAY_MS		10

/*
 * MT6681 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_reg: Calibrates output voltage register.
 * @modeset_mask: MASK of Calibrates output voltage register.
 */
struct mt6681_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 vocal_reg;
	u32 vocal_mask;
};

#define MT6681_LDO(_name, _volt_table, _enable_reg, en_bit,	\
		   _vsel_reg, _vsel_mask, _vocal_reg,		\
		   _vocal_mask, _lp_mode_reg, lp_bit)		\
[MT6681_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6681_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6681_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6681_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6681_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

static const unsigned int ldo_volt_table[] = {
	1800000,
};

static unsigned int mt6681_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6681_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_info(&rdev->dev, "Failed to get mt6681 mode: %d\n", ret);
		return ret;
	}

	if (val & info->modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &val);
	if (ret) {
		dev_info(&rdev->dev,
			 "Failed to get mt6681 lp mode: %d\n", ret);
		return ret;
	}

	if (val & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6681_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6681_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6681_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 0);
		udelay(100);
		break;
	case REGULATOR_MODE_IDLE:
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 info->lp_mode_mask);
		break;
	default:
		return -EINVAL;
	}

	if (ret) {
		dev_info(&rdev->dev,
			 "Failed to set mt6681 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static inline unsigned int mt6681_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6681_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6681_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const struct regulator_ops mt6681_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6681_regulator_set_mode,
	.get_mode = mt6681_regulator_get_mode,
};

static int mt6681_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6681_ID_XXX) */
static struct mt6681_regulator_info mt6681_regulators[] = {
	MT6681_LDO(VAUD18, ldo_volt_table,
		   MT6681_PMIC_RG_LDO_VAUD18_EN0_ADDR, MT6681_PMIC_RG_LDO_VAUD18_EN0_SHIFT,
		   MT6681_PMIC_RG_VAUD18_VOSEL_ADDR, MT6681_PMIC_RG_VAUD18_VOSEL_MASK,
		   MT6681_PMIC_RG_VAUD18_VOCAL_ADDR, MT6681_PMIC_RG_VAUD18_VOCAL_MASK,
		   MT6681_PMIC_RG_LDO_VAUD18_LP_ADDR, MT6681_PMIC_RG_LDO_VAUD18_LP_SHIFT),
};

static void mt6681_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6681_regulator_info *info
		= container_of(dwork, struct mt6681_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6681_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6681_regulator_info *info = rdev_get_drvdata(rdev);

	disable_irq_nosync(info->irq);
	if (!regulator_is_enabled_regmap(rdev))
		goto delayed_enable;
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
delayed_enable:
	schedule_delayed_work(&info->oc_work,
			      msecs_to_jiffies(info->oc_irq_enable_delay_ms));
	return IRQ_HANDLED;
}

static int mt6681_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret;
	struct mt6681_regulator_info *info = config->driver_data;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
	}
	return 0;
}

static int mt6681_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6681_regulator_info *info;
	int i, ret;

	dev_info(&pdev->dev, "%s(), dev name %s\n", __func__,
		 dev_name(&pdev->dev));

	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	for (i = 0; i < MT6681_MAX_REGULATOR; i++) {
		info = &mt6681_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		config.driver_data = info;

		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_info(&pdev->dev, "failed to register %s, ret=%d\n",
				 info->desc.name, ret);
			continue;
		}

		if (info->irq <= 0)
			continue;
		INIT_DELAYED_WORK(&info->oc_work, mt6681_oc_irq_enable_work);
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6681_oc_irq,
						IRQF_TRIGGER_HIGH,
						info->desc.name,
						rdev);
		if (ret) {
			dev_info(&pdev->dev, "Failed to request IRQ:%s, ret=%d",
				 info->desc.name, ret);
			continue;
		}
	}

	return 0;
}

static void mt6681_regulator_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;

	dev_info(&pdev->dev, "%s(), dev name %s\n", __func__,
		 dev_name(&pdev->dev));

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_notice(&pdev->dev, "invalid regmap.\n");
		return;
	}
}

static const struct of_device_id mt6681_regulator_of_match[] = {
	{
		.compatible = "mediatek,mt6681-regulator",
	},
	{}
};
MODULE_DEVICE_TABLE(of, mt6681_regulator_of_match);

static const struct platform_device_id mt6681_regulator_ids[] = {
	{ "mt6681-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6681_regulator_ids);

static struct platform_driver mt6681_regulator_driver = {
	.driver = {
		.name = "mt6681-regulator",
	},
	.probe = mt6681_regulator_probe,
	.shutdown = mt6681_regulator_shutdown,
	.id_table = mt6681_regulator_ids,
};
module_platform_driver(mt6681_regulator_driver);

MODULE_AUTHOR("Yiwen Chiou <yiwen.chiou@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6681 PMIC");
MODULE_LICENSE("GPL");
