/*
 * s2mpu16.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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
#include <linux/samsung/pmic/s2mpu16.h>
#include <linux/samsung/pmic/s2mpu16-regulator-8835.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/samsung/pmic/pmic_class.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
static struct device_node *acpm_mfd_node;
#endif

static struct s2mpu16_info *s2mpu16_static_info;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
struct pmic_sysfs_dev {
	uint8_t base_addr;
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
};
#endif

struct s2mpu16_info {
	struct regulator_dev *rdev[S2MPU16_REGULATOR_MAX];
	unsigned int opmode[S2MPU16_REGULATOR_MAX];
	struct s2mpu16_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int num_regulators;
	int wtsr_en;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct pmic_sysfs_dev *pmic_sysfs;
#endif
};

static unsigned int s2mpu16_of_map_mode(unsigned int val) {
	switch (val) {
	case SEC_OPMODE_SUSPEND:	/* ON in Standby Mode */
		return 0x1;
	case SEC_OPMODE_MIF:		/* ON in PWREN_MIF mode */
		return 0x2;
	case SEC_OPMODE_ON:		/* ON in Normal Mode */
		return 0x3;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
			unsigned int mode)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU16_ENABLE_SHIFT;
	s2mpu16->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);

	return s2mpu16_update_reg(s2mpu16->i2c, rdev->desc->enable_reg,
				  s2mpu16->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu16_update_reg(s2mpu16->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mpu16_read_reg(s2mpu16->i2c,
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
static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	uint8_t ramp_addr = 0;

	//ramp_value = get_ramp_delay(ramp_delay / 1000);
	//if (ramp_value > 4) {
	//	pr_warn("%s: ramp_delay: %d not supported\n",
	//		rdev->desc->name, ramp_delay);
	//}
	ramp_value = 0x00; 	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPU16_BUCK1:
		ramp_addr = S2MPU16_PM1_BUCK1S_DVS;
		break;
	//case S2MPU16_BUCK2:
	//	ramp_addr = S2MPU16_PM1_BUCK2S_DVS;
	//	break;
	case S2MPU16_BUCK3:
		ramp_addr = S2MPU16_PM1_BUCK3S_DVS;
		break;
	case S2MPU16_BUCK4:
		ramp_addr = S2MPU16_PM1_BUCK4S_DVS;
		break;
	case S2MPU16_BUCK5:
		ramp_addr = S2MPU16_PM1_BUCK5S_DVS;
		break;
	//case S2MPU16_BUCK6:
	//	ramp_addr = S2MPU16_PM1_BUCK6S_DVS;
	//	break;
	//case S2MPU16_BUCK7:
	//	ramp_addr = S2MPU16_PM1_BUCK7S_DVS;
	//	break;
	//case S2MPU16_BUCK8:
	//	ramp_addr = S2MPU16_PM1_BUCK8S_DVS;
	//	break;
	case S2MPU16_BUCK9:
		ramp_addr = S2MPU16_PM1_BUCK9S_DVS;
		break;
	case S2MPU16_BUCK10:
		ramp_addr = S2MPU16_PM1_BUCK10S_DVS;
		break;
	case S2MPU16_BUCK_SR1:
		ramp_addr = S2MPU16_PM1_SR1S_DVS1;
		break;
	case S2MPU16_BUCK_SR2:
		return 0;
	default:
		return -EINVAL;
	}

	return s2mpu16_update_reg(s2mpu16->i2c, ramp_addr,
				  ramp_value << BUCK_RAMP_UP_SHIFT,
				  BUCK_RAMP_MASK << BUCK_RAMP_UP_SHIFT);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mpu16_read_reg(s2mpu16->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mpu16_update_reg(s2mpu16->i2c, rdev->desc->vsel_reg,
				 sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpu16_update_reg(s2mpu16->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu16_info *s2mpu16 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mpu16_write_reg(s2mpu16->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpu16_update_reg(s2mpu16->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

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
	else
		return DIV_ROUND_UP(old_volt - new_volt, ramp_delay);

	return 0;
}

static struct regulator_ops s2mpu16_ldo_ops = {
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

static struct regulator_ops s2mpu16_buck_ops = {
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

#define _BUCK(macro)		S2MPU16_BUCK##macro
#define _buck_ops(num)		s2mpu16_buck_ops##num
#define _LDO(macro)		S2MPU16_LDO##macro
#define _ldo_ops(num)		s2mpu16_ldo_ops##num

#define _REG(ctrl)		S2MPU16_PM1##ctrl
#define _TIME(macro)		S2MPU16_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPU16_LDO_MIN##group
#define _LDO_STEP(group)	S2MPU16_LDO_STEP##group
#define _LDO_MASK(num)		S2MPU16_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPU16_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPU16_BUCK_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPU16_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU16_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU16_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu16_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, v_m, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= _LDO_MASK(v_m) + 1,			\
	.vsel_reg	= v,					\
	.vsel_mask	= _LDO_MASK(v_m),			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU16_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu16_of_map_mode			\
}

static struct regulator_desc regulators[S2MPU16_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* BUCK 1S to 10S, SR1(BB), SR2 */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1S_OUT2), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_BUCK2S_OUT1), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_BUCK3S_OUT1), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_BUCK4S_OUT2), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_BUCK5S_OUT1), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_BUCK6S_OUT1), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_BUCK7S_OUT1), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1, _REG(_BUCK8S_OUT1), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 1, _REG(_BUCK9S_OUT2), _REG(_BUCK9S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), 1, _REG(_BUCK10S_OUT2), _REG(_BUCK10S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), &_buck_ops(), 2, _REG(_SR1S_OUT2), _REG(_SR1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR2", _BUCK(_SR2), &_buck_ops(), 3, _REG(_SR2S_OUT1), _REG(_SR2S_CTRL), _TIME(_BUCK)),

	/* LDO 1S to 15S */
	//LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1S_OUT1), 1, _REG(_LDO1S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2S_OUT), 1, _REG(_LDO2S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2, _REG(_LDO3S_CTRL), 2, _REG(_LDO3S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4S_OUT1), 1, _REG(_LDO4S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5S_OUT1), 1, _REG(_LDO5S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 1, _REG(_LDO6S_OUT), 1, _REG(_LDO6S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_LDO7S_OUT), 1, _REG(_LDO7S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 2, _REG(_LDO8S_CTRL), 2, _REG(_LDO8S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 2, _REG(_LDO9S_CTRL), 2, _REG(_LDO9S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 2, _REG(_LDO10S_CTRL), 2, _REG(_LDO10S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 3, _REG(_LDO11S_CTRL), 2, _REG(_LDO11S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 3, _REG(_LDO12S_CTRL), 2, _REG(_LDO12S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 3, _REG(_LDO13S_CTRL), 2, _REG(_LDO13S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 1, _REG(_LDO14S_OUT), 1, _REG(_LDO14S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 1, _REG(_LDO15S_OUT1), 1, _REG(_LDO15S_CTRL), _TIME(_LDO)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mpu16_pmic_dt_parse_pdata(struct s2mpu16_dev *iodev,
				       struct s2mpu16_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu16_regulator_data *rdata;
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
	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		return -EINVAL;
	pdata->wtsr_en = val;

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
static int s2mpu16_pmic_dt_parse_pdata(struct s2mpu16_pmic_dev *iodev,
				       struct s2mpu16_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
int sub_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	return (i2c) ? s2mpu16_read_reg(i2c, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_read_reg);

int sub_pmic_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
	return (i2c) ? s2mpu16_update_reg(i2c, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_update_reg);

int sub_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mpu16_static_info)
		return -ENODEV;

	*i2c = s2mpu16_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(sub_pmic_get_i2c);
#endif

struct s2mpu16_oi_data {
	uint8_t reg;
	uint8_t val;
};

#define DECLARE_OI(_reg, _val) { .reg = (_reg), .val = (_val) }
static const struct s2mpu16_oi_data s2mpu16_oi[] = {
	/* BUCK 1S to 10S, SR1(BB), SR2, OI function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPU16_PM1_BUCK1S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK2S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK3S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK4S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK5S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK6S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK7S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK8S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK9S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_BUCK10S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_SR1S_OCP, 0xC4),
	DECLARE_OI(S2MPU16_PM1_SR2S_OCP, 0xC4),
};

static int s2mpu16_oi_function(struct s2mpu16_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic1;
	uint32_t i;
	uint8_t val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mpu16_oi); i++) {
		ret = s2mpu16_write_reg(i2c, s2mpu16_oi[i].reg, s2mpu16_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mpu16_oi); i++) {
		ret = s2mpu16_read_reg(i2c, s2mpu16_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mpu16_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -EINVAL;
}

static void s2mpu16_wtsr_enable(struct s2mpu16_info *s2mpu16)
{
	int ret;

	pr_info("[PMIC] %s: enable WTSR\n", __func__);

	ret = s2mpu16_update_reg(s2mpu16->i2c, S2MPU16_PM1_CFG_PM,
				 S2MPU16_WTSREN_MASK, S2MPU16_WTSREN_MASK);

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mpu16->wtsr_en = 1;
}

static void s2mpu16_wtsr_disable(struct s2mpu16_info *s2mpu16)
{
	int ret;

	pr_info("[PMIC] %s: disable WTSR\n", __func__);

	ret = s2mpu16_update_reg(s2mpu16->i2c, S2MPU16_PM1_CFG_PM,
				 0x00, S2MPU16_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static int check_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case VGPIO_ADDR:
	case COMMON_ADDR:
	case PMIC1_ADDR:
	case PMIC2_ADDR:
	case CLOSE1_ADDR:
	case CLOSE2_ADDR:
	case GPIO_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

static ssize_t s2mpu16_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpu16_info *s2mpu16 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mpu16->pmic_sysfs;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
	int ret;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -EINVAL;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx", &base_addr, &reg_addr);
	if (ret != 2) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	ret = check_base_address(base_addr);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&s2mpu16->iodev->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, SUB_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&s2mpu16->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif
	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(s2mpu16->iodev->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n",
					__func__, base_addr, reg_addr, val);

	return size;
}

static ssize_t s2mpu16_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpu16_info *s2mpu16 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mpu16->pmic_sysfs;

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			pmic_sysfs->base_addr, pmic_sysfs->read_addr, pmic_sysfs->read_val);
}

static ssize_t s2mpu16_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mpu16_info *s2mpu16 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mpu16->pmic_sysfs;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
	int ret;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -EINVAL;
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
	mutex_lock(&s2mpu16->iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, base_addr, reg_addr, val);
	mutex_unlock(&s2mpu16->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif
	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(s2mpu16->iodev->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n",
					__func__, base_addr, reg_addr, val);

	return size;
}

static ssize_t s2mpu16_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct s2mpu16_info *s2mpu16 = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = s2mpu16->pmic_sysfs;

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			pmic_sysfs->base_addr, pmic_sysfs->read_addr, pmic_sysfs->read_val);
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(write, S_IRUGO | S_IWUSR, s2mpu16_write_show, s2mpu16_write_store),
	PMIC_ATTR(read, S_IRUGO | S_IWUSR, s2mpu16_read_show, s2mpu16_read_store),
};

static int s2mpu16_create_sysfs(struct s2mpu16_info *s2mpu16)
{
	struct device *dev = s2mpu16->iodev->dev;
	struct device *sysfs_dev = NULL;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpu16->pmic_sysfs = devm_kzalloc(dev, sizeof(struct pmic_sysfs_dev), GFP_KERNEL);
	s2mpu16->pmic_sysfs->dev = pmic_device_create(s2mpu16, device_name);
	sysfs_dev = s2mpu16->pmic_sysfs->dev;

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

	return -EINVAL;
}
#endif

static int s2mpu16_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu16_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu16_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu16_info *s2mpu16;
	int ret;
	uint32_t i;

	dev_info(&pdev->dev, "[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu16_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu16 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu16_info), GFP_KERNEL);
	if (!s2mpu16)
		return -ENOMEM;

	s2mpu16->iodev = iodev;
	s2mpu16->i2c = iodev->pmic1;

	mutex_init(&s2mpu16->lock);

	s2mpu16_static_info = s2mpu16;

	platform_set_drvdata(pdev, s2mpu16);

	config.dev = &pdev->dev;
	config.driver_data = s2mpu16;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu16->opmode[id] = regulators[id].enable_mask;
		s2mpu16->rdev[i] = devm_regulator_register(&pdev->dev, &regulators[id], &config);
		if (IS_ERR(s2mpu16->rdev[i])) {
			ret = PTR_ERR(s2mpu16->rdev[i]);
			dev_err(&pdev->dev, "[PMIC] %s: regulator init failed for %s(%d)\n",
						__func__, regulators[i].name, i);
			goto err_s2mpu16_data;
		}
	}

	s2mpu16->num_regulators = pdata->num_rdata;

	if (pdata->wtsr_en)
		s2mpu16_wtsr_enable(s2mpu16);

	ret = s2mpu16_oi_function(iodev);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: s2mpu16_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mpu16_create_sysfs(s2mpu16);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: s2mpu16_create_sysfs fail\n", __func__);
		goto err_s2mpu16_data;
	}
#endif
	dev_info(&pdev->dev, "[PMIC] %s: end\n", __func__);

	return 0;

err_s2mpu16_data:
	mutex_destroy(&s2mpu16->lock);
err_pdata:
	return ret;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static void s2mpu16_remove_sysfs_entries(struct device *sysfs_dev)
{
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);
}
#endif
static int s2mpu16_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu16_info *s2mpu16 = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2mpu16_remove_sysfs_entries(s2mpu16->pmic_sysfs->dev);
#endif
	mutex_destroy(&s2mpu16->lock);

	return 0;
}

static void s2mpu16_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mpu16_info *s2mpu16 = platform_get_drvdata(pdev);

	/* disable WTSR */
	if (s2mpu16->wtsr_en)
		s2mpu16_wtsr_disable(s2mpu16);
}

static const struct platform_device_id s2mpu16_pmic_id[] = {
	{ "s2mpu16-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu16_pmic_id);

static struct platform_driver s2mpu16_pmic_driver = {
	.driver = {
		.name = "s2mpu16-regulator",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe = s2mpu16_pmic_probe,
	.remove = s2mpu16_pmic_remove,
	.shutdown = s2mpu16_pmic_shutdown,
	.id_table = s2mpu16_pmic_id,
};

module_platform_driver(s2mpu16_pmic_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPU16 Regulator Driver");
MODULE_LICENSE("GPL");
