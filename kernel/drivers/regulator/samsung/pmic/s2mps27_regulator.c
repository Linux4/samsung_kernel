/*
 * s2mps27_regulator.c
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/samsung/pmic/s2mps27.h>
#include <linux/samsung/pmic/s2mps27-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/samsung/pmic/pmic_class.h>
#include <linux/reset/exynos/exynos-reset.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
static struct device_node *acpm_mfd_node;
#endif
#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
#include <linux/cpufreq.h>
#endif /* CONFIG_SEC_PM_DEBUG */
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
#include <linux/sec_debug.h>
#endif

#define BUCK_BOOST_IDX		(1)

static struct s2mps27_info *s2mps27_static_info;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
struct pmic_sysfs_dev {
	uint8_t base_addr;
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
};
#endif

struct pmic_irq_info {
	int buck_ocp_irq[S2MPS27_BUCK_MAX];
	int buck_oi_irq[S2MPS27_BUCK_MAX];
	int temp_irq[S2MPS27_TEMP_MAX];
	int ldo_oi_irq[S2MPS27_LDO_OI_MAX];

	int buck_ocp_cnt[S2MPS27_BUCK_MAX];
	int temp_cnt[S2MPS27_TEMP_MAX];
	int buck_oi_cnt[S2MPS27_BUCK_MAX];
};

struct s2mps27_info {
	struct regulator_dev *rdev[S2MPS27_REGULATOR_MAX];
	unsigned int opmode[S2MPS27_REGULATOR_MAX];
	struct s2mps27_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int num_regulators;
	struct pmic_irq_info *pmic_irq;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct pmic_sysfs_dev *pmic_sysfs;
#endif
};

static unsigned int s2mps27_of_map_mode(unsigned int mode) {
	switch (mode) {
	case PMIC_OPMODE_VGPIO ... PMIC_OPMODE_ON:
		return mode;
	case PMIC_OPMODE_AUTO:
		return 0x4;
	case PMIC_OPMODE_FCCM:
		return 0x8;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

/* Regulators support mode for Enable_ctrl/Auto/FCCM */
static int s2mps27_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);
	struct device *dev = s2mps27->iodev->dev;

	switch (mode) {
	case PMIC_OPMODE_VGPIO ... PMIC_OPMODE_ON:
		val = mode << S2MPS27_ENABLE_SHIFT;
		s2mps27->opmode[id] = val;
		return 0;
	case PMIC_OPMODE_AUTO:
		mode = S2MPS27_BUCK_AUTO_MODE;
		break;
	case PMIC_OPMODE_FCCM:
		mode = S2MPS27_BUCK_FCCM_MODE;
		break;
	default:
		dev_err(dev, "%s: invalid mode %d specified\n", __func__, mode);
		return -EINVAL;
	}

	if (S2MPS27_BUCK_SR1 <= id && id <= S2MPS27_BB)
		return s2mps27_update_reg(s2mps27->i2c, rdev->desc->enable_reg,
						mode, S2MPS27_BUCK_MODE_MASK);

	dev_info(dev, "%s: %s don't use Auto and FCCM mode\n", __func__, rdev->desc->name);

	return 0;
}

/* BUCKs & BUCK_SRs & BB support [Auto/FCCM] mode */
static unsigned int s2mps27_get_mode(struct regulator_dev *rdev)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	uint8_t val;
	int ret = 0;

	ret = s2mps27_read_reg(s2mps27->i2c, rdev->desc->enable_reg, &val);
	if (ret)
		return REGULATOR_MODE_INVALID;

	dev_info(s2mps27->iodev->dev, "%s: [%s] enable_reg: 0x%02hhx, val: 0x%02hhx\n",
			__func__, rdev->desc->name, rdev->desc->enable_reg, val);

	val &= S2MPS27_BUCK_MODE_MASK;

	if (val == S2MPS27_BUCK_AUTO_MODE)
		ret = PMIC_OPMODE_AUTO;
	else if (val == S2MPS27_BUCK_FCCM_MODE)
		ret = PMIC_OPMODE_FCCM;

	return ret;
}

static int s2mps27_enable(struct regulator_dev *rdev)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);

	return s2mps27_update_reg(s2mps27->i2c, rdev->desc->enable_reg,
				  s2mps27->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2mps27_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps27_update_reg(s2mps27->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2mps27_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps27_read_reg(s2mps27->i2c,
				rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

#if 0
static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}

	return cnt;
}
#endif

static int s2mps27_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	uint8_t ramp_addr = 0;

	//ramp_value = get_ramp_delay(ramp_delay / 1000);
	//if (ramp_value > 4) {
	//	pr_warn("%s: ramp_delay: %d not supported\n",
	//		rdev->desc->name, ramp_delay);
	//}
	ramp_value = 0x00;	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPS27_BUCK_SR1:
		ramp_addr = S2MPS27_PM1_BUCK_SR1_DVS;
		break;
	case S2MPS27_BB:
		ramp_addr = S2MPS27_PM1_BB_DVS1;
		break;
	default:
		return -EINVAL;
	}

	return s2mps27_update_reg(s2mps27->i2c, ramp_addr,
				  ramp_value << BUCK_RAMP_UP_SHIFT,
				  BUCK_RAMP_MASK << BUCK_RAMP_UP_SHIFT);
}

static int s2mps27_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps27_read_reg(s2mps27->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2mps27_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mps27_update_reg(s2mps27->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps27_update_reg(s2mps27->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;
}

static int s2mps27_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps27_info *s2mps27 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mps27_write_reg(s2mps27->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
 		ret = s2mps27_update_reg(s2mps27->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;
}

static int s2mps27_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);
	else
		return DIV_ROUND_UP(old_volt - new_volt, ramp_delay);

	return 0;
}

static int s2mps27_read_pwron_status(void)
{
	uint8_t val;
	struct s2mps27_info *s2mps27 = s2mps27_static_info;

	s2mps27_read_reg(s2mps27->i2c, S2MPS27_PM1_STATUS1, &val);
	pr_info("%s: 0x%02hhx\n", __func__, val);

	return (val & S2MPS27_STATUS1_PWRON);
}

static int s2mps27_read_mrb_status(void)
{
	uint8_t val;
	struct s2mps27_info *s2mps27 = s2mps27_static_info;

	s2mps27_read_reg(s2mps27->i2c, S2MPS27_PM1_STATUS1, &val);
	pr_info("%s: 0x%02hhx\n", __func__, val);

	return (val & S2MPS27_STATUS1_MR1B);
}

int pmic_read_pwrkey_status(void)
{
	return s2mps27_read_pwron_status();
}
EXPORT_SYMBOL_GPL(pmic_read_pwrkey_status);

int pmic_read_vol_dn_key_status(void)
{
	return s2mps27_read_mrb_status();
}
EXPORT_SYMBOL_GPL(pmic_read_vol_dn_key_status);

static struct regulator_ops s2mps27_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mps27_is_enabled_regmap,
	.enable			= s2mps27_enable,
	.disable		= s2mps27_disable_regmap,
	.get_voltage_sel	= s2mps27_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mps27_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2mps27_set_voltage_time_sel,
	.set_mode		= s2mps27_set_mode,
};

static struct regulator_ops s2mps27_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mps27_is_enabled_regmap,
	.enable			= s2mps27_enable,
	.disable		= s2mps27_disable_regmap,
	.get_voltage_sel	= s2mps27_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mps27_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2mps27_set_voltage_time_sel,
	.set_mode		= s2mps27_set_mode,
	.get_mode		= s2mps27_get_mode,
	.set_ramp_delay		= s2mps27_set_ramp_delay,
};

static struct regulator_ops s2mps27_bb_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mps27_is_enabled_regmap,
	.enable			= s2mps27_enable,
	.disable		= s2mps27_disable_regmap,
	.get_voltage_sel	= s2mps27_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mps27_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2mps27_set_voltage_time_sel,
	.set_mode		= s2mps27_set_mode,
	.get_mode		= s2mps27_get_mode,
};

#define _BUCK(macro)		S2MPS27_BUCK##macro
#define _buck_ops(num)		s2mps27_buck_ops##num
#define _LDO(macro)		S2MPS27_LDO##macro
#define _ldo_ops(num)		s2mps27_ldo_ops##num
#define _BB(macro)		S2MPS27_BB##macro
#define _bb_ops(num)		s2mps27_bb_ops##num

#define _REG(ctrl)		S2MPS27_PM1##ctrl
#define _TIME(macro)		S2MPS27_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS27_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS27_LDO_STEP##group
#define _LDO_MASK(num)		S2MPS27_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPS27_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS27_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS27_BB_MIN##group
#define _BB_STEP(group)		S2MPS27_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS27_BUCK_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps27_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPS27_LDO_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps27_of_map_mode			\
}

#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPS27_BB_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps27_of_map_mode			\
}


static struct regulator_desc regulators[S2MPS27_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// BUCK_SR1
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), &_buck_ops(), 1, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL), _TIME(_BUCK)),
	// BUCK BOOST
	BB_DESC("BUCKB", _BB(), &_bb_ops(), 1, _REG(_BB_OUT1), _REG(_BB_CTRL), _TIME(_BB)),
	// LDO 1 ~ 18
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1_CTRL), _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2_CTRL), _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 1, _REG(_LDO3_CTRL), _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4_CTRL), _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5_CTRL), _REG(_LDO5_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 2, _REG(_LDO6_CTRL), _REG(_LDO6_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_LDO7_CTRL), _REG(_LDO7_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 3, _REG(_LDO8_CTRL), _REG(_LDO8_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 3, _REG(_LDO9_CTRL), _REG(_LDO9_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 3, _REG(_LDO10_CTRL), _REG(_LDO10_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 3, _REG(_LDO11_CTRL), _REG(_LDO11_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 1, _REG(_LDO12_CTRL), _REG(_LDO12_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 1, _REG(_LDO13_CTRL), _REG(_LDO13_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 1, _REG(_LDO14_CTRL), _REG(_LDO14_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 1, _REG(_LDO15_CTRL), _REG(_LDO15_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 1, _REG(_LDO16_CTRL), _REG(_LDO16_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 1, _REG(_LDO17_CTRL), _REG(_LDO17_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 1, _REG(_LDO18_CTRL), _REG(_LDO18_CTRL), _TIME(_LDO)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mps27_pmic_dt_parse_pdata(struct s2mps27_dev *iodev,
				       struct s2mps27_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps27_regulator_data *rdata;
	uint32_t i, val;
	int ret;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif

	/* get 1 gpio values */
	if (of_gpio_count(pmic_np) < 1) {
		dev_err(iodev->dev, "could not find pmic gpios\n");
		return -EINVAL;
	}
	pdata->smpl_warn = of_get_gpio(pmic_np, 0);

	ret = of_property_read_u32(pmic_np, "smpl_warn_en", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_en = !!val;

	ret = of_property_read_u32(pmic_np, "smpl_warn_vth", &val);
	if (!ret && !(val & ~0x7))
		pdata->smpl_warn_vth = val;
	else
		pdata->smpl_warn_vth = -1;

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) * pdata->num_regulators, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	pdata->regulators = rdata;
	pdata->num_rdata = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
				 "[PMIC] %s: don't know how to configure regulator %s\n",
				 __func__, reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np, &regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
		pdata->num_rdata++;
	}

	return 0;
}
#else
static int s2mps27_pmic_dt_parse_pdata(struct s2mps27_pmic_dev *iodev,
					struct s2mps27_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static irqreturn_t s2mps27_buck_ocp_irq(int irq, void *data)
{
	struct s2mps27_info *s2mps27 = data;
	struct pmic_irq_info *pmic_irq = s2mps27->pmic_irq;
	uint32_t i;

	mutex_lock(&s2mps27->lock);

	for (i = 0; i < S2MPS27_BUCK_MAX; i++) {
		if (pmic_irq->buck_ocp_irq[i] == irq) {
			pmic_irq->buck_ocp_cnt[i]++;
			if (i < BUCK_BOOST_IDX)
				pr_info("%s: BUCK_SR[%d] OCP IRQ : %d, %d\n",
					__func__, i + 1, pmic_irq->buck_ocp_cnt[i], irq);
			else
				pr_info("%s: BUCK_BOOST[%d] OCP IRQ : %d, %d\n",
					__func__, i + 1 - BUCK_BOOST_IDX, pmic_irq->buck_ocp_cnt[i], irq);
			break;
		}
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_sub_ocp_count(OCP_OI_COUNTER_TYPE_OCP);
#endif

	mutex_unlock(&s2mps27->lock);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	pr_info("BUCK OCP: BIG: %u kHz, MIDH: %u kHz, MIDL: %u kHz, LITTLE: %u kHz\n",
			cpufreq_get(9), cpufreq_get(7), cpufreq_get(4), cpufreq_get(0));
#endif /* CONFIG_SEC_PM_DEBUG */

	return IRQ_HANDLED;
}

static irqreturn_t s2mps27_buck_oi_irq(int irq, void *data)
{
	struct s2mps27_info *s2mps27 = data;
	struct pmic_irq_info *pmic_irq = s2mps27->pmic_irq;
	uint32_t i;

	mutex_lock(&s2mps27->lock);

	for (i = 0; i < S2MPS27_BUCK_MAX; i++) {
		if (pmic_irq->buck_oi_irq[i] == irq) {
			pmic_irq->buck_oi_cnt[i]++;
			if (i < BUCK_BOOST_IDX)
				pr_info("%s: BUCK_SR[%d] OI IRQ : %d, %d\n",
					__func__, i + 1, pmic_irq->buck_oi_cnt[i], irq);
			else
				pr_info("%s: BUCK_BOOST[%d] OI IRQ : %d, %d\n",
					__func__, i + 1 - BUCK_BOOST_IDX, pmic_irq->buck_oi_cnt[i], irq);
			break;
		}
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_sub_ocp_count(OCP_OI_COUNTER_TYPE_OI);
#endif

	mutex_unlock(&s2mps27->lock);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	pr_info("BUCK OI: BIG: %u kHz, MIDH: %u kHz, MIDL: %u kHz, LITTLE: %u kHz\n",
			cpufreq_get(9), cpufreq_get(7), cpufreq_get(4), cpufreq_get(0));
#endif /* CONFIG_SEC_PM_DEBUG */

	return IRQ_HANDLED;
}

static irqreturn_t s2mps27_temp_irq(int irq, void *data)
{
	struct s2mps27_info *s2mps27 = data;
	struct pmic_irq_info *pmic_irq = s2mps27->pmic_irq;

	mutex_lock(&s2mps27->lock);

	if (pmic_irq->temp_irq[0] == irq) {
		pmic_irq->temp_cnt[0]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, pmic_irq->temp_cnt[0], irq);
	} else if (pmic_irq->temp_irq[1] == irq) {
		pmic_irq->temp_cnt[1]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, pmic_irq->temp_cnt[1], irq);
	}

	mutex_unlock(&s2mps27->lock);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_EXYNOS_AFM) || IS_ENABLED(CONFIG_NPU_AFM)
int main_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	return (i2c) ? s2mps27_read_reg(i2c, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(main_pmic_read_reg);

int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	return (i2c) ? s2mps27_update_reg(i2c, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(main_pmic_update_reg);

int main_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mps27_static_info)
		return -ENODEV;

	*i2c = s2mps27_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(main_pmic_get_i2c);
#endif

#if IS_ENABLED(CONFIG_SEC_PM)
static ssize_t th120C_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int cnt = pmic_irq->temp_cnt[0];

	pr_info("%s: PMIC thermal 120C count: %d\n", __func__, cnt);
	pmic_irq->temp_cnt[0] = 0;

	return sprintf(buf, "%d\n", cnt);
}

static ssize_t th120C_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	pmic_irq->temp_cnt[0] = val;

	return count;
}
static ssize_t th140C_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int cnt = pmic_irq->temp_cnt[1];

	pr_info("%s: PMIC thermal 140C count: %d\n", __func__, cnt);
	pmic_irq->temp_cnt[0] = 0;

	return sprintf(buf, "%d\n", cnt);
}

static ssize_t th140C_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	pmic_irq->temp_cnt[1] = val;

	return count;
}

static ssize_t buck_ocp_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int i, buf_offset = 0;

	for (i = 0; i < S2MPS27_BUCK_MAX; i++)
		if (pmic_irq->buck_ocp_irq[i])
			buf_offset += sprintf(buf + buf_offset, "B%d : %d\n",
					i+1, pmic_irq->buck_ocp_cnt[i]);

	return buf_offset;
}

static ssize_t buck_oi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pmic_irq_info *pmic_irq = s2mps27_static_info->pmic_irq;
	int i, buf_offset = 0;

	for (i = 0; i < S2MPS27_BUCK_MAX; i++)
		if (pmic_irq->buck_oi_irq[i])
			buf_offset += sprintf(buf + buf_offset, "B%d : %d\n",
					i+1, pmic_irq->buck_oi_cnt[i]);

	return buf_offset;
}

static ssize_t pmic_id_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int pmic_id = s2mps27_static_info->iodev->pmic_rev;

	return sprintf(buf, "0x%02X\n", pmic_id);
}

static ssize_t chg_det_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret, chg_det;
	u8 val;

	ret = s2mps27_read_reg(s2mps27_static_info->i2c, S2MPS27_PM1_STATUS1, &val);
	if (ret)
		chg_det = -1;
	else
		chg_det = !(val & (1 << 2));

	pr_info("%s: ap pmic chg det: %d\n", __func__, chg_det);

	return sprintf(buf, "%d\n", chg_det);
}

static ssize_t manual_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	bool enabled;
	u8 val;

	ret = s2mps27_read_reg(s2mps27_static_info->i2c, S2MPS27_PM1_CTRL1, &val);
	if (ret)
		return ret;

	enabled = !!(val & (1 << 4));

	pr_info("%s: %s[0x%02X]\n", __func__, enabled ? "enabled" :  "disabled",
			val);

	return sprintf(buf, "%s\n", enabled ? "enabled" :  "disabled");
}

static ssize_t manual_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	bool enable;
	u8 val;

	ret = strtobool(buf, &enable);
	if (ret)
		return ret;

	ret = s2mps27_read_reg(s2mps27_static_info->i2c, S2MPS27_PM1_CTRL1, &val);
	if (ret)
		return ret;

	val &= ~(1 << 4);
	val |= enable << 4;

	ret = s2mps27_write_reg(s2mps27_static_info->i2c, S2MPS27_PM1_CTRL1, val);
	if (ret)
		return ret;

	pr_info("%s: %d [0x%02X]\n", __func__, enable, val);

	return count;
}

static DEVICE_ATTR_RW(th120C_count);
static DEVICE_ATTR_RW(th140C_count);
static DEVICE_ATTR_RO(buck_ocp_count);
static DEVICE_ATTR_RO(buck_oi_count);
static DEVICE_ATTR_RO(pmic_id);
static DEVICE_ATTR_RO(chg_det);
static DEVICE_ATTR_RW(manual_reset);

static struct attribute *ap_pmic_attributes[] = {
	&dev_attr_th120C_count.attr,
	&dev_attr_th140C_count.attr,
	&dev_attr_buck_ocp_count.attr,
	&dev_attr_buck_oi_count.attr,
	&dev_attr_pmic_id.attr,
	&dev_attr_chg_det.attr,
	&dev_attr_manual_reset.attr,
	NULL
};

static const struct attribute_group ap_pmic_attr_group = {
	.attrs = ap_pmic_attributes,
};
#endif /* CONFIG_SEC_PM */

struct s2mps27_oi_data {
	uint8_t reg;
	uint8_t val;
};

#define DECLARE_OI(_reg, _val)	{ .reg = (_reg), .val = (_val) }
static const struct s2mps27_oi_data s2mps27_oi[] = {
	/* BUCK_SR1, BUCK_BOOST OCL,OI function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS27_PM1_BUCK_SR1_OCP, 0xCC),
	DECLARE_OI(S2MPS27_PM1_BB_OCP, 0xCC),
};

static int s2mps27_oi_function(struct s2mps27_dev *iodev)
{
	struct i2c_client *i2c = iodev->pm1;
	uint32_t i;
	uint8_t val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mps27_oi); i++) {
		ret = s2mps27_write_reg(i2c, s2mps27_oi[i].reg, s2mps27_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mps27_oi); i++) {
		ret = s2mps27_read_reg(i2c, s2mps27_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mps27_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mps27_set_interrupt(struct platform_device *pdev,
				 struct s2mps27_info *s2mps27, int irq_base)
{
	int i, ret;
	static char buck_ocp_name[S2MPS27_BUCK_MAX][32] = {0, };
	static char buck_oi_name[S2MPS27_BUCK_MAX][32] = {0, };
	static char temp_name[S2MPS27_TEMP_MAX][32] = {0, };
	struct device *dev = s2mps27->iodev->dev;
	struct pmic_irq_info *pmic_irq = NULL;

	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	s2mps27->pmic_irq = devm_kzalloc(dev, sizeof(struct pmic_irq_info), GFP_KERNEL);
	pmic_irq = s2mps27->pmic_irq;

	/* BUCK_SR1, BUCK_BOOST OCP interrupt */
	for (i = 0; i < S2MPS27_BUCK_MAX; i++) {
		pmic_irq->buck_ocp_irq[i] = irq_base + S2MPS27_PMIC_IRQ_OCP_SR1_INT4 + i;

		/* Dynamic allocation for device name */
		if (i < BUCK_BOOST_IDX)
			snprintf(buck_ocp_name[i], sizeof(buck_ocp_name[i]) - 1, "BUCK_SR_OCP_IRQ%d@%s",
				 i + 1, dev_name(dev));
		else
			snprintf(buck_ocp_name[i], sizeof(buck_ocp_name[i]) - 1, "BUCK_BOOST_OCP_IRQ%d@%s",
				 i + 1 - BUCK_BOOST_IDX, dev_name(dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						pmic_irq->buck_ocp_irq[i], NULL,
						s2mps27_buck_ocp_irq, 0,
						buck_ocp_name[i], s2mps27);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, pmic_irq->buck_ocp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK_SR1, BUCK_BOOST OI interrupt */
	for (i = 0; i < S2MPS27_BUCK_MAX; i++) {
		pmic_irq->buck_oi_irq[i] = irq_base + S2MPS27_PMIC_IRQ_OI_SR1_INT4 + i;

		/* Dynamic allocation for device name */
		if (i < BUCK_BOOST_IDX)
			snprintf(buck_oi_name[i], sizeof(buck_oi_name[i]) - 1, "BUCK_SR_OI_IRQ%d@%s",
				 i + 1, dev_name(dev));
		else
			snprintf(buck_oi_name[i], sizeof(buck_oi_name[i]) - 1, "BUCK_BOOST_OI_IRQ%d@%s",
				 i + 1 - BUCK_BOOST_IDX, dev_name(dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						pmic_irq->buck_oi_irq[i], NULL,
						s2mps27_buck_oi_irq, 0,
						buck_oi_name[i], s2mps27);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request BUCK[%d] OI IRQ: %d: %d\n",
				i + 1, pmic_irq->buck_oi_irq[i], ret);
			goto err;
		}
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPS27_TEMP_MAX; i++) {
		pmic_irq->temp_irq[i] = irq_base + S2MPS27_PMIC_IRQ_INT120C_INT3 + i;

		/* Dynamic allocation for device name */
		snprintf(temp_name[i], sizeof(temp_name[i]) - 1, "TEMP_IRQ%d@%s",
			 i + 1, dev_name(dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						pmic_irq->temp_irq[i], NULL,
						s2mps27_temp_irq, 0,
						temp_name[i], s2mps27);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request over temperature[%d] IRQ: %d: %d\n",
				i, pmic_irq->temp_irq[i], ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static int check_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case VGPIO_ADDR:
	case COM_ADDR:
	case RTC_ADDR:
	case PM1_ADDR:
	case PM2_ADDR:
	case PM3_ADDR:
	case ADC_ADDR:
	case EXT_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

static ssize_t s2mps27_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps27_info *s2mps27 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mps27->pmic_sysfs;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
	int ret;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx", &base_addr, &reg_addr);
	if (ret != 2) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	ret = check_base_address(base_addr);
	if(ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&s2mps27->iodev->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_ID, base_addr, reg_addr, &val);
	mutex_unlock(&s2mps27->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(s2mps27->iodev->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n",
				       	__func__, base_addr, reg_addr, val);

	return size;
}

static ssize_t s2mps27_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps27_info *s2mps27 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mps27->pmic_sysfs;

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			pmic_sysfs->base_addr, pmic_sysfs->read_addr, pmic_sysfs->read_val);
}

static ssize_t s2mps27_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps27_info *s2mps27 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mps27->pmic_sysfs;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
	int ret;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx 0x%02hhx", &base_addr, &reg_addr, &val);
	if (ret != 3) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	ret = check_base_address(base_addr);
	if (ret < 0)
		return ret;


#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&s2mps27->iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MAIN_ID, base_addr, reg_addr, val);
	mutex_unlock(&s2mps27->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(s2mps27->iodev->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n",
			       		__func__, base_addr, reg_addr, val);

	return size;
}

static ssize_t s2mps27_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct s2mps27_info *s2mps27 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mps27->pmic_sysfs;

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			pmic_sysfs->base_addr, pmic_sysfs->read_addr, pmic_sysfs->read_val);

}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(write, S_IRUGO | S_IWUSR, s2mps27_write_show, s2mps27_write_store),
	PMIC_ATTR(read, S_IRUGO | S_IWUSR, s2mps27_read_show, s2mps27_read_store),
};

static int s2mps27_create_sysfs(struct s2mps27_info *s2mps27)
{
	struct device *dev = s2mps27->iodev->dev;
	struct device *sysfs_dev = NULL;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps27->pmic_sysfs = devm_kzalloc(dev, sizeof(struct pmic_sysfs_dev), GFP_KERNEL);
	s2mps27->pmic_sysfs->dev = pmic_device_create(s2mps27, device_name);
	sysfs_dev = s2mps27->pmic_sysfs->dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(sysfs_dev, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);

	return -1;
}
#endif

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
static u8 pmic_onsrc[2];
static u8 pmic_offsrc[2];

static ssize_t pwr_on_off_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ONSRC:0x%02X,0x%02X OFFSRC:0x%02X,0x%02X\n",
			pmic_onsrc[0], pmic_onsrc[1],
			pmic_offsrc[0], pmic_offsrc[1]);
}

static DEVICE_ATTR_RO(pwr_on_off_src);

static struct attribute *sec_pm_debug_attrs[] = {
	&dev_attr_pwr_on_off_src.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_debug);

int main_pmic_init_debug_sysfs(struct device *sec_pm_dev)
{
	int ret;

	ret = sysfs_create_groups(&sec_pm_dev->kobj, sec_pm_debug_groups);
	if (ret)
		pr_err("%s: failed to create sysfs groups(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(main_pmic_init_debug_sysfs);
#endif /* CONFIG_SEC_PM_DEBUG */

static int __maybe_unused s2mps27_power_off_seq_wa(void)
{
	int ret = 0;

	return ret;
}

static int jig_acok_update(struct s2mps27_info *s2mps27)
{
	uint8_t val[2];
	int ret = 0;

	if (s2mps27->iodev->inst_acok_en) {
		ret = s2mps27_update_reg(s2mps27->i2c, S2MPS27_PM1_CFG_PM,
				S2MPS27_INST_ACOK_EN, S2MPS27_INST_ACOK_EN);
		if (ret) {
			pr_err("%s: INST_ACOK_EN set fail\n", __func__);
			return -EINVAL;
		}
	}

	if (s2mps27->iodev->jig_reboot_en) {
		ret = s2mps27_update_reg(s2mps27->i2c, S2MPS27_PM1_TIME_CTRL,
				S2MPS27_JIG_REBOOT_EN, S2MPS27_JIG_REBOOT_EN);
		if (ret) {
			pr_err("%s: JIG_REBOOT_EN set fail\n", __func__);
			return -EINVAL;
		}
	}

	ret = s2mps27_read_reg(s2mps27->i2c, S2MPS27_PM1_CFG_PM, &val[0]);
	if (ret) {
		pr_err("%s: CFG_PM read faile\n", __func__);
		return -EINVAL;
	}

	ret = s2mps27_read_reg(s2mps27->i2c, S2MPS27_PM1_TIME_CTRL, &val[1]);
	if (ret) {
		pr_err("%s: TIME_CTRL read faile\n", __func__);
		return -EINVAL;
	}

	pr_info("[PMIC] %s: end CFG_PM: 0x%02hhx, TIME_CTRL: 0x%02hhx\n",
			__func__, val[0], val[1]);
	return ret;
}

static int s2mps27_set_smpl_warn_vth(struct s2mps27_info *s2mps27,
					int smpl_warn_vth)
{
	if (smpl_warn_vth < 0)
		return 0;

	return s2mps27_update_reg(s2mps27->i2c, S2MPS27_PM1_CTRL2,
			smpl_warn_vth << SMPLWARN_LEVEL_SHIFT, SMPLWARN_LEVEL_MASK);
}

static int s2mps27_power_off(void)
{
	struct s2mps27_info *s2mps27 = s2mps27_static_info;
	int ret = 0;

	if (!s2mps27 || !s2mps27->iodev)
		return -ENODEV;

	ret = jig_acok_update(s2mps27);

	return ret;
}

static int s2mps27_pmic_probe(struct platform_device *pdev)
{
	struct s2mps27_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps27_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps27_info *s2mps27;
	int ret;
	uint32_t i;
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	u8 offsrc_val[2] = {0,};
#endif /* CONFIG_SEC_PM_DEBUG */

	pr_info("[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mps27_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps27 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps27_info), GFP_KERNEL);
	if (!s2mps27)
		return -ENOMEM;

	s2mps27->iodev = iodev;
	s2mps27->i2c = iodev->pm1;

	mutex_init(&s2mps27->lock);

	s2mps27_static_info = s2mps27;

	platform_set_drvdata(pdev, s2mps27);

	config.dev = &pdev->dev;
	config.driver_data = s2mps27;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;

		if (iodev->pmic_rev == 0 && id == S2MPS27_LDO6) {
			pr_info("%s: Skip %s register on rev.0x%02hhx\n",
				__func__, regulators[i].name, iodev->pmic_rev);
			continue;
		}
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		s2mps27->opmode[id] = regulators[id].enable_mask;
		s2mps27->rdev[i] = devm_regulator_register(&pdev->dev, &regulators[id], &config);
		if (IS_ERR(s2mps27->rdev[i])) {
			ret = PTR_ERR(s2mps27->rdev[i]);
			dev_err(&pdev->dev, "[PMIC] %s: regulator init failed for %s(%d)\n",
						__func__, regulators[i].name, i);
			goto err_s2mps27_data;
		}
	}

	s2mps27->num_regulators = pdata->num_rdata;

	ret = s2mps27_set_smpl_warn_vth(s2mps27, pdata->smpl_warn_vth);
	if (ret < 0) {
		pr_err("%s: s2mps27_set_smpl_warn_vth fail(%d)\n", __func__, ret);
		goto err_s2mps27_data;
	}

	ret = s2mps27_set_interrupt(pdev, s2mps27, pdata->irq_base);
	if (ret < 0) {
		pr_err("%s: s2mps27_set_interrupt fail(%d)\n", __func__, ret);
		goto err_s2mps27_data;
	}
	ret = s2mps27_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mps27_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PM)
	/* ap_pmic_dev should be initialized before power meter initialization */
	iodev->ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	ret = sysfs_create_group(&iodev->ap_pmic_dev->kobj, &ap_pmic_attr_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create ap_pmic sysfs group\n");
#endif /* CONFIG_SEC_PM */

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mps27_create_sysfs(s2mps27);
	if (ret < 0) {
		pr_err("%s: s2mps27_create_sysfs fail\n", __func__);
		goto err_s2mps27_data;
	}
#endif
	exynos_reboot_register_pmic_ops(s2mps27_power_off, NULL,
			s2mps27_power_off_seq_wa, s2mps27_read_pwron_status);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	ret = s2mps27_bulk_read(s2mps27->i2c, S2MPS27_PM1_PWRONSRC1, 2,
			pmic_onsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read PWRONSRC\n");

	ret = s2mps27_bulk_read(s2mps27->i2c, S2MPS27_PM1_OFFSRC1_CUR, 2,
			pmic_offsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read OFFSRC\n");

	/* Clear OFFSRC1, OFFSRC2 register */
	ret = s2mps27_bulk_write(s2mps27->i2c, S2MPS27_PM1_OFFSRC1_CUR, 2,
			offsrc_val);
	if (ret)
		dev_err(&pdev->dev, "failed to write OFFSRC\n");
#endif /* CONFIG_SEC_PM_DEBUG */

	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_s2mps27_data:
	mutex_destroy(&s2mps27->lock);
err_pdata:
	return ret;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static void s2mps27_remove_sysfs_entries(struct device *sysfs_dev)
{
	uint32_t i = 0;

	for (i=0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);
}
#endif

static int s2mps27_pmic_remove(struct platform_device *pdev)
{
	struct s2mps27_info *s2mps27 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2mps27_remove_sysfs_entries(s2mps27->pmic_sysfs->dev);
#endif
	mutex_destroy(&s2mps27->lock);

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps27->iodev->ap_pmic_dev))
		sec_device_destroy(s2mps27->iodev->ap_pmic_dev->devt);
#endif /* CONFIG_SEC_PM */
	return 0;
}

static const struct platform_device_id s2mps27_pmic_id[] = {
	{ "s2mps27-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps27_pmic_id);

static struct platform_driver s2mps27_pmic_driver = {
	.driver = {
		.name = "s2mps27-regulator",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe = s2mps27_pmic_probe,
	.remove = s2mps27_pmic_remove,
	.id_table = s2mps27_pmic_id,
};

module_platform_driver(s2mps27_pmic_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPS27 Regulator Driver");
MODULE_LICENSE("GPL");
