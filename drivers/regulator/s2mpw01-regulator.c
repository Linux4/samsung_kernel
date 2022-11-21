/*
 * s2mpw01-regulator.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
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
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>
#include <linux/io.h>
#include <linux/debugfs.h>

static unsigned int sel2volt(int id, unsigned int sel, unsigned int mask);

static struct s2mpw01_info *static_info;

#ifdef CONFIG_DEBUG_FS
static u8 i2caddr;
static u8 i2cdata;
static struct i2c_client *dbgi2c;
static struct dentry *s2mpw01_root;
static struct dentry *s2mpw01_i2caddr;
static struct dentry *s2mpw01_i2cdata;
#endif

struct s2mpw01_info {
	struct regulator_dev *rdev[S2MPW01_REGULATOR_MAX];
	u8 opmode[S2MPW01_REGULATOR_MAX];
	int num_regulators;
	struct i2c_client *i2c;
	struct s2mpw01_dev *iodev;
	unsigned int vsel_value[S2MPW01_REGULATOR_MAX];
	bool cache_data;
};

#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
static char *rdev_name(struct regulator_dev *rdev)
{
	if (rdev->desc->name)
		return (char *)rdev->desc->name;
	else if (rdev->constraints && rdev->constraints->name)
		return (char *)rdev->constraints->name;
	else
		return "";
}
#endif

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	u8 val;
	int ret, id = rdev_get_id(rdev);

	switch (mode) {
	case SEC_OPMODE_SUSPEND:		/* ON in Standby Mode */
		val = 0x1 << S2MPW01_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_LOWPOWER:
		val = 0x2 << S2MPW01_ENABLE_SHIFT; /* ON in LowPower Mode */
		break;
	case SEC_OPMODE_ON:			/* ON in Normal Mode */
		val = 0x3 << S2MPW01_ENABLE_SHIFT;
		break;
	default:
		pr_warn("%s: regulator_suspend_mode : 0x%x not supported\n",
			rdev->desc->name, mode);
		return -EINVAL;
	}

	ret = s2mpw01_update_reg(s2mpw01->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
	if (ret)
		return ret;

	s2mpw01->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg = S2MPW01_PMIC_REG_EXT_CTRL, val, mask;

	switch (id) {
	case S2MPW01_LDO19:
		val = mask = 1;
		break;
	case S2MPW01_LDO8:
		val = mask = 2;
		break;
	case S2MPW01_LDO9:
		val = mask = 4;
		break;
	default:
		reg = rdev->desc->enable_reg;
		val = s2mpw01->opmode[id];
		mask = rdev->desc->enable_mask;
		break;
	}

	return s2mpw01_update_reg(s2mpw01->i2c, reg, val, mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg = S2MPW01_PMIC_REG_EXT_CTRL, val = 0, mask;

	switch (id) {
	case S2MPW01_LDO19:
		mask = 1;
		break;
	case S2MPW01_LDO8:
		mask = 2;
		break;
	case S2MPW01_LDO9:
		mask = 4;
		break;
	default:
		reg = rdev->desc->enable_reg;
		val = rdev->desc->enable_is_inverted ?
			rdev->desc->enable_mask : 0;
		mask = rdev->desc->enable_mask;
		break;
	}

	return s2mpw01_update_reg(s2mpw01->i2c, reg, val, mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int ret, id = rdev_get_id(rdev);
	u8 reg = S2MPW01_PMIC_REG_EXT_CTRL, val, mask;

	switch (id) {
	case S2MPW01_LDO19:
		mask = 1;
		break;
	case S2MPW01_LDO8:
		mask = 2;
		break;
	case S2MPW01_LDO9:
		mask = 4;
		break;
	default:
		reg = rdev->desc->enable_reg;
		mask = rdev->desc->enable_mask;
		break;
	}

	ret = s2mpw01_read_reg(s2mpw01->i2c, reg, &val);
	if (ret)
		return ret;

	return rdev->desc->enable_is_inverted ?
		!(val & mask) : !!(val & mask);
}

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

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	u8 ramp_mask = 0x03;
	u8 ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPW01_BUCK1:
		ramp_shift = 6;
		break;
	case S2MPW01_LDO2:
		ramp_shift = 2;
		break;
	case S2MPW01_LDO1:
		ramp_shift = 0;
		break;
	default:

		return 0;
	}

	return s2mpw01_update_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_RAMP,
			  ramp_value << ramp_shift, ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;
	ret = s2mpw01_read_reg(s2mpw01->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int volt;

	volt = sel2volt(reg_id, sel, rdev->desc->vsel_mask);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_IN);

	ret = s2mpw01_update_reg(s2mpw01->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpw01_update_reg(s2mpw01->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_OUT);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_ON);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					unsigned sel)
{
	int ret;
	struct s2mpw01_info *s2mpw01 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int volt;

	volt = sel2volt(reg_id, sel, rdev->desc->vsel_mask);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_IN);

	ret = s2mpw01_write_reg(s2mpw01->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpw01_update_reg(s2mpw01->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_OUT);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_ON);
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
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

	return 0;
}

u32 pmic_rev_get(void)
{
	return SEC_PMIC_REV(static_info->iodev);
}

static struct regulator_ops s2mpw01_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mpw01_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};

#define _BUCK(macro)	S2MPW01_BUCK##macro
#define _buck_ops(num)	s2mpw01_buck_ops##num

#define _LDO(macro)	S2MPW01_LDO##macro
#define _REG(ctrl)	S2MPW01_PMIC_REG##ctrl
#define _ldo_ops(num)	s2mpw01_ldo_ops##num
#define _TIME(macro)	S2MPW01_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPW01_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPW01_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPW01_ENABLE_MASK,			\
	.enable_time	= t					\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPW01_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPW01_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPW01_ENABLE_MASK,			\
	.enable_time	= t					\
}

static struct regulator_desc regulators[S2MPW01_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L1CTRL2), _REG(_L1CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L2CTRL2), _REG(_L2CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1),
			_REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1),
			_REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1),
			_REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1),
			_REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B1OUT2), _REG(_B1CTRL), _TIME(_BUCK1)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B3CTRL2), _REG(_B3CTRL1), _TIME(_BUCK3)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B4CTRL2), _REG(_B4CTRL1), _TIME(_BUCK4)),
};

static unsigned int sel2volt(int id, unsigned int sel, unsigned int mask)
{
	return ((sel & mask) * regulators[id].uV_step) + regulators[id].min_uV;
}

#ifdef CONFIG_OF
static struct of_device_id s2mpw01_of_match[] = {
	{
		.compatible = "s2mpw01-regulator",
	},
	{},
};

static int s2mpw01_pmic_dt_parse_pdata(struct s2mpw01_dev *iodev,
					struct s2mpw01_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpw01_regulator_data *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

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

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name,
					regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mpw01_pmic_dt_parse_pdata(struct s2mpw01_dev *iodev,
					struct s2mpw01_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_DEBUG_FS
static ssize_t s2mpw01_i2caddr_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2caddr);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpw01_i2caddr_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t len;
	u8 val;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (!kstrtou8(buf, 0, &val))
		i2caddr = val;

	return len;
}

static ssize_t s2mpw01_i2cdata_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = s2mpw01_read_reg(dbgi2c, i2caddr, &i2cdata);
	if (ret)
		return ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2cdata);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpw01_i2cdata_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t len, ret;
	u8 val;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (!kstrtou8(buf, 0, &val)) {
		ret = s2mpw01_write_reg(dbgi2c, i2caddr, val);
		if (ret < 0)
			return ret;
	}

	return len;
}

static const struct file_operations s2mpw01_i2caddr_fops = {
	.open = simple_open,
	.read = s2mpw01_i2caddr_read,
	.write = s2mpw01_i2caddr_write,
	.llseek = default_llseek,
};
static const struct file_operations s2mpw01_i2cdata_fops = {
	.open = simple_open,
	.read = s2mpw01_i2cdata_read,
	.write = s2mpw01_i2cdata_write,
	.llseek = default_llseek,
};
#endif

static int s2mpw01_pmic_probe(struct platform_device *pdev)
{
	struct s2mpw01_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpw01_info *s2mpw01;
	int i, ret;

	if (iodev->dev->of_node) {
		ret = s2mpw01_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpw01 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpw01_info),
				GFP_KERNEL);
	if (!s2mpw01)
		return -ENOMEM;

	s2mpw01->iodev = iodev;
	s2mpw01->i2c = iodev->pmic;

	static_info = s2mpw01;

	platform_set_drvdata(pdev, s2mpw01);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpw01;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpw01->opmode[id] = regulators[id].enable_mask;

		s2mpw01->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mpw01->rdev[i])) {
			ret = PTR_ERR(s2mpw01->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mpw01->rdev[i] = NULL;
			goto err;
		}
	}

	s2mpw01->num_regulators = pdata->num_regulators;

#ifdef CONFIG_DEBUG_FS
	dbgi2c = s2mpw01->i2c;
	s2mpw01_root = debugfs_create_dir("s2mpw01-regs", NULL);
	s2mpw01_i2caddr = debugfs_create_file("i2caddr", 0644, s2mpw01_root, NULL, &s2mpw01_i2caddr_fops);
	s2mpw01_i2cdata = debugfs_create_file("i2cdata", 0644, s2mpw01_root, NULL, &s2mpw01_i2cdata_fops);
#endif

	return 0;

err:
	for (i = 0; i < S2MPW01_REGULATOR_MAX; i++)
		regulator_unregister(s2mpw01->rdev[i]);

	return ret;
}

static int s2mpw01_pmic_remove(struct platform_device *pdev)
{
	struct s2mpw01_info *s2mpw01 = platform_get_drvdata(pdev);
	int i;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(s2mpw01_i2cdata);
	debugfs_remove_recursive(s2mpw01_i2caddr);
	debugfs_remove_recursive(s2mpw01_root);
#endif

	for (i = 0; i < S2MPW01_REGULATOR_MAX; i++)
		regulator_unregister(s2mpw01->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpw01_pmic_id[] = {
	{ "s2mpw01-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpw01_pmic_id);

#define	SYNC_ON		0x4
#define	SYNC_OFF	0x0
static int s2mpw01_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s2mpw01_info *s2mpw01 = platform_get_drvdata(pdev);
	s2mpw01_update_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_L1DVS, SYNC_OFF, 0x4);
	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_B3CTRL2, 0x34); /* 1.25V */
	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_B4CTRL2, 0x64); /* 1.85V */
	return 0;
}

static int s2mpw01_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s2mpw01_info *s2mpw01 = platform_get_drvdata(pdev);
	s2mpw01_update_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_L1DVS, SYNC_ON, 0x4);
	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_B3CTRL2, 0x3C); /* 1.35V */
	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_B4CTRL2, 0x70); /* 2V */
	return 0;
}

static const struct dev_pm_ops s2mpw01_pmic_pm_ops = {
	.suspend_late = s2mpw01_pmic_suspend,
	.resume_early = s2mpw01_pmic_resume,
};

static struct platform_driver s2mpw01_pmic_driver = {
	.driver = {
		.name = "s2mpw01-regulator",
		.owner = THIS_MODULE,
		.pm = &s2mpw01_pmic_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(s2mpw01_of_match),
#endif
	},
	.probe = s2mpw01_pmic_probe,
	.remove = s2mpw01_pmic_remove,
	.id_table = s2mpw01_pmic_id,
};

static int __init s2mpw01_pmic_init(void)
{
	return platform_driver_register(&s2mpw01_pmic_driver);
}
subsys_initcall(s2mpw01_pmic_init);

static void __exit s2mpw01_pmic_exit(void)
{
	platform_driver_unregister(&s2mpw01_pmic_driver);
}
module_exit(s2mpw01_pmic_exit);

/* Module information */
MODULE_AUTHOR("Samsung LSI");
MODULE_DESCRIPTION("SAMSUNG S2MPW01 Regulator Driver");
MODULE_LICENSE("GPL");
