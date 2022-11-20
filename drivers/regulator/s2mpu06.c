/*
 * s2mpu06.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
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
#include <linux/mfd/samsung/s2mpu06.h>
#include <linux/mfd/samsung/s2mpu06-private.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#ifdef CONFIG_SEC_PM
#include <linux/sec_sysfs.h>

#define STATUS1_ACOK	BIT(2)

static struct device *ap_pmic_dev;
#endif /* CONFIG_SEC_PM */

static unsigned int sel2volt(int id, unsigned int sel, unsigned int mask);

static struct s2mpu06_info *static_info;

#ifdef CONFIG_DEBUG_FS
static u8 i2caddr;
static u8 i2cdata;
static struct i2c_client *dbgi2c;
static struct dentry *s2mpu06_root;
static struct dentry *s2mpu06_i2caddr;
static struct dentry *s2mpu06_i2cdata;
#endif

struct s2mpu06_info {
	struct regulator_dev *rdev[S2MPU06_REGULATOR_MAX];
	unsigned int opmode[S2MPU06_REGULATOR_MAX];
	int num_regulators;
	struct i2c_client *i2c;
	struct s2mpu06_dev *iodev;
	unsigned int vsel_value[S2MPU06_REGULATOR_MAX];
	bool cache_data;
#ifdef CONFIG_SEC_DEBUG_PMIC
	struct notifier_block pm_notifier;
#endif
};

#ifdef CONFIG_SEC_DEBUG_PMIC
#define PMIC_NAME	"s2mpu06"
#define PMIC_NUM_OF_BUCKS	3
#define PMIC_NUM_OF_LDOS	24

static int buck_addrs[PMIC_NUM_OF_BUCKS] = { 0x13, 0x17, 0x19 };
static int ldo_addrs[PMIC_NUM_OF_LDOS] = { 0x1e, 0x20, 0x21, 0x22, 0x23, 0x24, 0x26, 0x27, 0x28, 0x29, /* LDO1~10 */
	0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 }; /* LDO11~24 */

static int s2m_debug_reg(struct regulator_dev *rdev, char *str)
{
	int ret;
	u8 val;
	int i;

	if (!static_info)
		return 0;

	for (i = 0; i < PMIC_NUM_OF_BUCKS; ++i) {
		if (buck_addrs[i] == rdev->desc->enable_reg) {
			ret = s2mpu06_read_reg(static_info->i2c, rdev->desc->enable_reg, &val);
			pr_info("%s %s: BUCK%d 0x%x = 0x%x (EN=%d)\n", \
					PMIC_NAME, str, i+1, rdev->desc->enable_reg, val, (val >> 6));
			return ret;
		}
	}

	for (i = 0; i < PMIC_NUM_OF_LDOS; ++i) {
		if (ldo_addrs[i] == rdev->desc->enable_reg) {
			ret = s2mpu06_read_reg(static_info->i2c, rdev->desc->enable_reg, &val);
			pr_info("%s %s: LDO%d 0x%x = 0x%x (EN=%d)\n", \
					PMIC_NAME, str, i+1, rdev->desc->enable_reg, val, (val >> 6));
			return ret;
		}
	}

	return 0;
}
#endif

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
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, id = rdev_get_id(rdev);

	switch (mode) {
	case SEC_OPMODE_SUSPEND:		/* ON in Standby Mode */
		val = 0x1 << S2MPU06_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_TCXO:			/* ON in TCXO_EN Mode */
		val = 0x2 << S2MPU06_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_ON:			/* ON in Normal Mode */
		val = 0x3 << S2MPU06_ENABLE_SHIFT;
		break;
	default:
		pr_warn("%s: regulator_suspend_mode : 0x%x not supported\n",
			rdev->desc->name, mode);
		return -EINVAL;
	}

	ret = s2mpu06_update_reg(s2mpu06->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
	if (ret)
		return ret;

	s2mpu06->opmode[id] = val;

#ifdef CONFIG_SEC_DEBUG_PMIC
	s2m_debug_reg(rdev, "set_mode");
#endif
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret;
	int id = rdev_get_id(rdev);
	u8 reg = S2MPU06_PMIC_REG_EXT_CTRL, val, mask;

	switch (id) {
	case S2MPU06_LDO4:
		val = mask = 1;
		break;
	case S2MPU06_LDO14:
		val = mask = 2;
		break;
	case S2MPU06_LDO15:
		val = mask = 4;
		break;
	case S2MPU06_LDO16:
		val = mask = 8;
		break;
	default:
		reg = rdev->desc->enable_reg;
		val = s2mpu06->opmode[id];
		mask = rdev->desc->enable_mask;
		break;
	}

	ret = s2mpu06_update_reg(s2mpu06->i2c, reg, val, mask);
#ifdef CONFIG_SEC_DEBUG_PMIC
	s2m_debug_reg(rdev, "regulator_enable");
#endif
	return ret;
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret;
	int id = rdev_get_id(rdev);
	u8 reg = S2MPU06_PMIC_REG_EXT_CTRL, val = 0, mask;

	switch (id) {
	case S2MPU06_LDO4:
		mask = 1;
		break;
	case S2MPU06_LDO14:
		mask = 2;
		break;
	case S2MPU06_LDO15:
		mask = 4;
		break;
	case S2MPU06_LDO16:
		mask = 8;
		break;
	default:
		reg = rdev->desc->enable_reg;
		val = rdev->desc->enable_is_inverted ?
			rdev->desc->enable_mask : 0;
		mask = rdev->desc->enable_mask;
		break;
	}

	ret = s2mpu06_update_reg(s2mpu06->i2c, reg, val, mask);
#ifdef CONFIG_SEC_DEBUG_PMIC
	s2m_debug_reg(rdev, "regulator_disable");
#endif
	return ret;
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret, id = rdev_get_id(rdev);
	u8 reg = S2MPU06_PMIC_REG_EXT_CTRL, val, mask;

	switch (id) {
	case S2MPU06_LDO4:
		mask = 1;
		break;
	case S2MPU06_LDO14:
		mask = 2;
		break;
	case S2MPU06_LDO15:
		mask = 4;
		break;
	case S2MPU06_LDO16:
		mask = 8;
		break;
	default:
		reg = rdev->desc->enable_reg;
		mask = rdev->desc->enable_mask;
		break;
	}

	ret = s2mpu06_read_reg(s2mpu06->i2c, reg, &val);
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
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPU06_LDO1:
		ramp_shift = 4;
		break;
	case S2MPU06_LDO6:
		ramp_shift = 2;
		break;
	case S2MPU06_BUCK1:
		ramp_shift = 0;
		break;
	default:
		return 0;
	}

	return s2mpu06_update_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_RAMP,
			  ramp_value << ramp_shift, ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret, reg_id = rdev_get_id(rdev);
	u8 val;

	if ((reg_id >= S2MPU06_BUCK1 && reg_id <= S2MPU06_BUCK3)
		&& s2mpu06->vsel_value[reg_id] && s2mpu06->cache_data)
		return s2mpu06->vsel_value[reg_id];
	else {
		ret = s2mpu06_read_reg(s2mpu06->i2c, rdev->desc->vsel_reg, &val);
		if (ret)
			return ret;
	}
	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int volt;

	volt = sel2volt(reg_id, sel, rdev->desc->vsel_mask);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_IN);

	ret = s2mpu06_update_reg(s2mpu06->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu06_update_reg(s2mpu06->i2c, rdev->desc->apply_reg,
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
	struct s2mpu06_info *s2mpu06 = rdev_get_drvdata(rdev);
	int ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int volt;

	s2mpu06->vsel_value[reg_id] = sel;

	volt = sel2volt(reg_id, sel, rdev->desc->vsel_mask);
	exynos_ss_regulator(rdev_name(rdev), rdev->desc->vsel_reg, volt, ESS_FLAG_IN);

	ret = s2mpu06_write_reg(s2mpu06->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu06_update_reg(s2mpu06->i2c, rdev->desc->apply_reg,
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

#ifdef CONFIG_SEC_DEBUG_PMIC
static int _pmic_debug_print_all_regulators(void)
{
	u8 val;
	int i;

	if (!static_info)
		return 0;

	for (i = 0; i < PMIC_NUM_OF_BUCKS; ++i) {
		s2mpu06_read_reg(static_info->i2c, buck_addrs[i], &val);
		pr_info("%s Buck%d 0x%0x = 0x%x (EN=0x%x)\n", \
			PMIC_NAME, i+1, buck_addrs[i], val, (val >> 6));
	}

	for (i = 0; i < PMIC_NUM_OF_LDOS; ++i) {
		s2mpu06_read_reg(static_info->i2c, ldo_addrs[i], &val);
		pr_info("%s LDO%02d 0x%0x = 0x%x (EN=0x%x)\n", \
			PMIC_NAME, i+1, ldo_addrs[i], val, (val >> 6));
	}

	s2mpu06_read_reg(static_info->i2c, 0x3B, &val);
	pr_info("%s DRAM_USIM 0x%x = 0x%x\n", PMIC_NAME, 0x3B, val);

	s2mpu06_read_reg(static_info->i2c, 0xFF, &val);
	pr_info("%s EXT_CTRL 0x%x = 0x%x\n", PMIC_NAME, 0xFF, val);
	
	return 0;
}

static int pmic_debug_print_all_regulators(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		_pmic_debug_print_all_regulators();
		break;
	}
	return NOTIFY_DONE;
}
#endif

static struct regulator_ops s2mpu06_ldo_ops = {
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

static struct regulator_ops s2mpu06_buck_ops = {
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

#define _BUCK(macro)	S2MPU06_BUCK##macro
#define _buck_ops(num)	s2mpu06_buck_ops##num

#define _LDO(macro)	S2MPU06_LDO##macro
#define _REG(ctrl)	S2MPU06_PMIC_REG##ctrl
#define _ldo_ops(num)	s2mpu06_ldo_ops##num
#define _TIME(macro)	S2MPU06_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU06_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU06_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU06_ENABLE_MASK,			\
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
	.n_voltages	= S2MPU06_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU06_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU06_ENABLE_MASK,			\
	.enable_time	= t					\
}

static struct regulator_desc regulators[S2MPU06_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L1CTRL1), _REG(_L1CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L6CTRL1), _REG(_L6CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B1OUT2), _REG(_B1CTRL), _TIME(_BUCK1)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B2CTRL2), _REG(_B2CTRL1), _TIME(_BUCK2)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B3CTRL2), _REG(_B3CTRL1), _TIME(_BUCK3)),
};

static unsigned int sel2volt(int id, unsigned int sel, unsigned int mask)
{
	return ((sel & mask) * regulators[id].uV_step) + regulators[id].min_uV;
}

#ifdef CONFIG_OF
static int s2mpu06_pmic_dt_parse_pdata(struct s2mpu06_dev *iodev,
					struct s2mpu06_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu06_regulator_data *rdata;
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
static int s2mpu06_pmic_dt_parse_pdata(struct s2mpu06_dev *iodev,
					struct s2mpu06_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_DEBUG_FS
static ssize_t s2mpu06_i2caddr_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2caddr);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpu06_i2caddr_write(struct file *file, const char __user *user_buf,
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

static ssize_t s2mpu06_i2cdata_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = s2mpu06_read_reg(dbgi2c, i2caddr, &i2cdata);
	if (ret)
		return ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2cdata);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpu06_i2cdata_write(struct file *file, const char __user *user_buf,
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
		ret = s2mpu06_write_reg(dbgi2c, i2caddr, val);
		if (ret < 0)
			return ret;
	}

	return len;
}

static const struct file_operations s2mpu06_i2caddr_fops = {
	.open = simple_open,
	.read = s2mpu06_i2caddr_read,
	.write = s2mpu06_i2caddr_write,
	.llseek = default_llseek,
};
static const struct file_operations s2mpu06_i2cdata_fops = {
	.open = simple_open,
	.read = s2mpu06_i2cdata_read,
	.write = s2mpu06_i2cdata_write,
	.llseek = default_llseek,
};
#endif

#ifdef CONFIG_SEC_PM
static ssize_t chg_det_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret, chg_det;
	u8 val;

	ret = s2mpu06_read_reg(static_info->i2c, S2MPU06_PMIC_REG_STATUS1, &val);

	if(ret)
		chg_det = -1;
	else
		chg_det = !!(val & STATUS1_ACOK); // ACOK active high

	pr_info("%s: ap pmic chg det: %d\n", __func__, chg_det);

	return sprintf(buf, "%d\n", chg_det);
}

static DEVICE_ATTR_RO(chg_det);

static struct attribute *ap_pmic_attributes[] = {
	&dev_attr_chg_det.attr,
	NULL
};

static const struct attribute_group ap_pmic_attr_group = {
	.attrs = ap_pmic_attributes,
};
#endif /* CONFIG_SEC_PM */

static int s2mpu06_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu06_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu06_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu06_info *s2mpu06;
	int i, ret;

	/* todo : revision ID 0x00 (PMIC_ID) SEC_PMIC_REV <- Common block */

	if (iodev->dev->of_node) {
		ret = s2mpu06_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu06 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu06_info),
				GFP_KERNEL);
	if (!s2mpu06)
		return -ENOMEM;

	s2mpu06->iodev = iodev;
	s2mpu06->i2c = iodev->pmic;
	s2mpu06->cache_data = pdata->cache_data;
	static_info = s2mpu06;

	platform_set_drvdata(pdev, s2mpu06);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu06;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu06->opmode[id] = regulators[id].enable_mask;

		s2mpu06->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mpu06->rdev[i])) {
			ret = PTR_ERR(s2mpu06->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mpu06->rdev[i] = NULL;
			goto err;
		}
	}

	s2mpu06->num_regulators = pdata->num_regulators;

#ifdef CONFIG_SEC_PM
	ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	ret = sysfs_create_group(&ap_pmic_dev->kobj, &ap_pmic_attr_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create ap_pmic sysfs group\n");
#endif /* CONFIG_SEC_PM */

#ifdef CONFIG_DEBUG_FS
	dbgi2c = s2mpu06->i2c;
	s2mpu06_root = debugfs_create_dir("s2mpu06-regs", NULL);
	s2mpu06_i2caddr = debugfs_create_file("i2caddr", 0644, s2mpu06_root, NULL, &s2mpu06_i2caddr_fops);
	s2mpu06_i2cdata = debugfs_create_file("i2cdata", 0644, s2mpu06_root, NULL, &s2mpu06_i2cdata_fops);
#endif

#ifdef CONFIG_SEC_DEBUG_PMIC
	s2mpu06->pm_notifier.notifier_call = pmic_debug_print_all_regulators;
	ret = register_pm_notifier(&s2mpu06->pm_notifier);
	if (ret) {
		dev_err(&pdev->dev, "Failed to setup pm notifier\n");
		goto err;
	}
#endif

	pr_info("%s pmic prove complete\n", __func__);
	return 0;

err:
	for (i = 0; i < S2MPU06_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu06->rdev[i]);

	return ret;
}

static int s2mpu06_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu06_info *s2mpu06 = platform_get_drvdata(pdev);
	int i;

#ifdef CONFIG_SEC_DEBUG_PMIC
	unregister_pm_notifier(&s2mpu06->pm_notifier);
#endif

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(s2mpu06_i2cdata);
	debugfs_remove_recursive(s2mpu06_i2caddr);
	debugfs_remove_recursive(s2mpu06_root);
#endif

	for (i = 0; i < S2MPU06_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu06->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpu06_pmic_id[] = {
	{ "s2mpu06-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu06_pmic_id);

#define SYNC_ON		0x4
#define SYNC_OFF	0x0
static int s2mpu06_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s2mpu06_info *s2mpu06 = platform_get_drvdata(pdev);
	s2mpu06_update_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_L6DVS, SYNC_OFF, 0x4);
	return 0;
}

static int s2mpu06_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s2mpu06_info *s2mpu06 = platform_get_drvdata(pdev);
	s2mpu06_update_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_L6DVS, SYNC_ON, 0x4);
	return 0;
}

static const struct dev_pm_ops s2mpu06_pmic_pm_ops = {
	.suspend_late = s2mpu06_pmic_suspend,
	.resume_early = s2mpu06_pmic_resume,
};

static struct platform_driver s2mpu06_pmic_driver = {
	.driver = {
		.name = "s2mpu06-regulator",
		.owner = THIS_MODULE,
		.pm = &s2mpu06_pmic_pm_ops,
	},
	.probe = s2mpu06_pmic_probe,
	.remove = s2mpu06_pmic_remove,
	.id_table = s2mpu06_pmic_id,
};

static int __init s2mpu06_pmic_init(void)
{
	return platform_driver_register(&s2mpu06_pmic_driver);
}
subsys_initcall(s2mpu06_pmic_init);

static void __exit s2mpu06_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu06_pmic_driver);
}
module_exit(s2mpu06_pmic_exit);

/* Module information */
MODULE_AUTHOR("Samsung LSI");
MODULE_DESCRIPTION("SAMSUNG S2MPU06 Regulator Driver");
MODULE_LICENSE("GPL");
