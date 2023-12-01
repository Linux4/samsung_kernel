/*
 * s2mps25.c
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
//#include <../drivers/pinctrl/samsung/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/samsung/pmic/s2mps25.h>
#include <linux/samsung/pmic/s2mps25-regulator-9935.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/samsung/pmic/pmic_class.h>
#include <linux/reset/exynos/exynos-reset.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define MAIN_CHANNEL	0
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_VGPIO	0x00
#define I2C_BASE_COMMON	0x03
#define I2C_BASE_RTC	0x04
#define I2C_BASE_PM1	0x05
#define I2C_BASE_PM2	0x06
#define I2C_BASE_CLOSE1	0x0E
#define I2C_BASE_CLOSE2	0x0F
#define I2C_BASE_ADC	0x0A

#define BUCK_SR1M_IDX	(10)

static struct s2mps25_info *s2mps25_static_info;
static int s2mps25_buck_ocp_cnt[S2MPS25_BUCK_MAX]; /* BUCK 1~10, SR1~SR3 OCP count */
static int s2mps25_temp_cnt[S2MPS25_TEMP_MAX]; /* 0 : 120 degree , 1 : 140 degree */
static int s2mps25_buck_oi_cnt[S2MPS25_BUCK_MAX]; /* BUCK 1~10, SR1~SR3 OI count */

struct s2mps25_info {
	struct regulator_dev *rdev[S2MPS25_REGULATOR_MAX];
	unsigned int opmode[S2MPS25_REGULATOR_MAX];
	struct s2mps25_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int num_regulators;
	int buck_ocp_irq[S2MPS25_BUCK_MAX];	/* BUCK OCP IRQ */
	int buck_oi_irq[S2MPS25_BUCK_MAX];	/* BUCK OI IRQ */
	int temp_irq[S2MPS25_TEMP_MAX];	/* 0 : 120 degree, 1 : 140 degree */
	int ldo_oi_irq[S2MPS25_LDO_OI_MAX];	/* LDO OI IRQ */
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	uint8_t base_addr;
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
#endif
};

static unsigned int s2mps25_of_map_mode(unsigned int val) {
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
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPS25_ENABLE_SHIFT;
	s2mps25->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);

	return s2mps25_update_reg(s2mps25->i2c, rdev->desc->enable_reg,
				  s2mps25->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps25_update_reg(s2mps25->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps25_read_reg(s2mps25->i2c,
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
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
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
	case S2MPS25_BUCK1:
		ramp_addr = S2MPS25_REG_BUCK1M_DVS;
		break;
	case S2MPS25_BUCK2:
		ramp_addr = S2MPS25_REG_BUCK2M_DVS;
		break;
	case S2MPS25_BUCK3:
		ramp_addr = S2MPS25_REG_BUCK3M_DVS;
		break;
	case S2MPS25_BUCK4:
		ramp_addr = S2MPS25_REG_BUCK4M_DVS;
		break;
	case S2MPS25_BUCK5:
		ramp_addr = S2MPS25_REG_BUCK5M_DVS;
		break;
	case S2MPS25_BUCK6:
		ramp_addr = S2MPS25_REG_BUCK6M_DVS;
		break;
	case S2MPS25_BUCK7:
		ramp_addr = S2MPS25_REG_BUCK7M_DVS;
		break;
	case S2MPS25_BUCK8:
		ramp_addr = S2MPS25_REG_BUCK8M_DVS;
		break;
	case S2MPS25_BUCK9:
		ramp_addr = S2MPS25_REG_BUCK9M_DVS;
		break;
	case S2MPS25_BUCK10:
		ramp_addr = S2MPS25_REG_BUCK10M_DVS;
		break;
	case S2MPS25_BUCK_SR1:
		ramp_addr = S2MPS25_REG_BUCK_SR1M_DVS;
		break;
	case S2MPS25_BUCK_SR2:
		ramp_addr = S2MPS25_REG_BUCK_SR2M_DVS;
		break;
	case S2MPS25_BUCK_SR3:
		ramp_addr = S2MPS25_REG_BUCK_SR3M_DVS;
		break;
	default:
		return -EINVAL;
	}

	return s2mps25_update_reg(s2mps25->i2c, ramp_addr,
				  ramp_value << BUCK_RAMP_UP_SHIFT,
				  BUCK_RAMP_MASK << BUCK_RAMP_UP_SHIFT);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps25_read_reg(s2mps25->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mps25_update_reg(s2mps25->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps25_update_reg(s2mps25->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps25_info *s2mps25 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mps25_write_reg(s2mps25->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps25_update_reg(s2mps25->i2c, rdev->desc->apply_reg,
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

static int s2mps25_read_pwron_status(void)
{
	uint8_t val;
	struct s2mps25_info *s2mps25 = s2mps25_static_info;

	s2mps25_read_reg(s2mps25->i2c, S2MPS25_REG_STATUS1, &val);
	pr_info("%s: 0x%02hhx\n", __func__, val);

	return (val & S2MPS25_STATUS1_PWRON);
}

static int s2mps25_read_mrb_status(void)
{
	uint8_t val;
	struct s2mps25_info *s2mps25 = s2mps25_static_info;

	s2mps25_read_reg(s2mps25->i2c, S2MPS25_REG_STATUS1, &val);
	pr_info("%s: 0x%02hhx\n", __func__, val);

	return (val & S2MPS25_STATUS1_MRB);
}

int pmic_read_pwrkey_status(void)
{
	return s2mps25_read_pwron_status();
}
EXPORT_SYMBOL_GPL(pmic_read_pwrkey_status);

int pmic_read_vol_dn_key_status(void)
{
	return s2mps25_read_mrb_status();
}
EXPORT_SYMBOL_GPL(pmic_read_vol_dn_key_status);

static struct regulator_ops s2mps25_ldo_ops = {
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

static struct regulator_ops s2mps25_buck_ops = {
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
#if 0
static struct regulator_ops s2mps25_bb_ops = {
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
#endif

#define _BUCK(macro)		S2MPS25_BUCK##macro
#define _buck_ops(num)		s2mps25_buck_ops##num
#define _LDO(macro)		S2MPS25_LDO##macro
#define _ldo_ops(num)		s2mps25_ldo_ops##num
#define _BB(macro)		S2MPS25_BB##macro
#define _bb_ops(num)		s2mps25_bb_ops##num

#define _REG(ctrl)		S2MPS25_REG##ctrl
#define _TIME(macro)		S2MPS25_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS25_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS25_LDO_STEP##group
#define _LDO_MASK(num)		S2MPS25_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPS25_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS25_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS25_BB_MIN##group
#define _BB_STEP(group)		S2MPS25_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS25_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS25_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS25_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps25_of_map_mode			\
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
	.enable_mask	= S2MPS25_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps25_of_map_mode			\
}
#if 0
#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPS25_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS25_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS25_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps25_of_map_mode			\
}
#endif

static struct regulator_desc regulators[S2MPS25_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// LDO 1M ~ 30M
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1M_OUT), 1, _REG(_LDO1M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2M_OUT), 1, _REG(_LDO2M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2, _REG(_LDO3M_CTRL), 2, _REG(_LDO3M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4M_OUT), 1, _REG(_LDO4M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5M_OUT), 1, _REG(_LDO5M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 1, _REG(_LDO6M_OUT), 1, _REG(_LDO6M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_LDO7M_OUT), 1, _REG(_LDO7M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 1, _REG(_LDO8M_OUT), 1, _REG(_LDO8M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1, _REG(_LDO9M_OUT), 1, _REG(_LDO9M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 1, _REG(_LDO10M_OUT), 1, _REG(_LDO10M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 1, _REG(_LDO11M_OUT), 1, _REG(_LDO11M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 2, _REG(_LDO12M_CTRL), 2, _REG(_LDO12M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 2, _REG(_LDO13M_CTRL), 2, _REG(_LDO13M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 1, _REG(_LDO14M_OUT), 1, _REG(_LDO14M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 2, _REG(_LDO15M_CTRL), 2, _REG(_LDO15M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 2, _REG(_LDO16M_CTRL), 2, _REG(_LDO16M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 3, _REG(_LDO17M_CTRL), 2, _REG(_LDO17M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 4, _REG(_LDO18M_CTRL), 2, _REG(_LDO18M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 1, _REG(_LDO19M_OUT), 1, _REG(_LDO19M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 2, _REG(_LDO20M_CTRL), 2, _REG(_LDO20M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 2, _REG(_LDO21M_CTRL), 2, _REG(_LDO21M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 2, _REG(_LDO22M_CTRL), 2, _REG(_LDO22M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 2, _REG(_LDO23M_CTRL), 2, _REG(_LDO23M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), 2, _REG(_LDO24M_CTRL), 2, _REG(_LDO24M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), 2, _REG(_LDO25M_CTRL), 2, _REG(_LDO25M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), 2, _REG(_LDO26M_CTRL), 2, _REG(_LDO26M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), 2, _REG(_LDO27M_CTRL), 2, _REG(_LDO27M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), 2, _REG(_LDO28M_CTRL), 2, _REG(_LDO28M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), 2, _REG(_LDO29M_CTRL), 2, _REG(_LDO29M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), 2, _REG(_LDO30M_CTRL), 2, _REG(_LDO30M_CTRL), _TIME(_LDO)),

	// BUCK 1M ~ 10M, SR1M ~ SR3M
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1M_OUT1), _REG(_BUCK1M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_BUCK2M_OUT1), _REG(_BUCK2M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_BUCK3M_OUT2), _REG(_BUCK3M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_BUCK4M_OUT2), _REG(_BUCK4M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_BUCK5M_OUT3), _REG(_BUCK5M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_BUCK6M_OUT3), _REG(_BUCK6M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_BUCK7M_OUT3), _REG(_BUCK7M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1, _REG(_BUCK8M_OUT1), _REG(_BUCK8M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 1, _REG(_BUCK9M_OUT1), _REG(_BUCK9M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), 1, _REG(_BUCK10M_OUT1), _REG(_BUCK10M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), &_buck_ops(), 2, _REG(_BUCK_SR1M_OUT1), _REG(_BUCK_SR1M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR2", _BUCK(_SR2), &_buck_ops(), 3, _REG(_BUCK_SR2M_OUT1), _REG(_BUCK_SR2M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR3", _BUCK(_SR3), &_buck_ops(), 2, _REG(_BUCK_SR3M_OUT1), _REG(_BUCK_SR3M_CTRL), _TIME(_BUCK_SR)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mps25_pmic_dt_parse_pdata(struct s2mps25_dev *iodev,
				       struct s2mps25_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps25_regulator_data *rdata;
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

	/* adc_mode */
	pdata->adc_mode = 0;
	ret = of_property_read_u32(pmic_np, "adc_mode", &val);
	if (ret)
		return -EINVAL;
	pdata->adc_mode = val;

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
static int s2mps25_pmic_dt_parse_pdata(struct s2mps25_pmic_dev *iodev,
					struct s2mps25_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static irqreturn_t s2mps25_buck_ocp_irq(int irq, void *data)
{
	struct s2mps25_info *s2mps25 = data;
	uint32_t i;

	mutex_lock(&s2mps25->lock);

	for (i = 0; i < S2MPS25_BUCK_MAX; i++) {
		if (s2mps25_static_info->buck_ocp_irq[i] == irq) {
			s2mps25_buck_ocp_cnt[i]++;
			if (i < BUCK_SR1M_IDX)
				pr_info("%s: BUCK[%d] OCP IRQ : %d, %d\n",
					__func__, i + 1, s2mps25_buck_ocp_cnt[i], irq);
			else
				pr_info("%s: BUCK_SR[%d] OCP IRQ : %d, %d\n",
					__func__, i + 1 - BUCK_SR1M_IDX, s2mps25_buck_ocp_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mps25->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mps25_buck_oi_irq(int irq, void *data)
{
	struct s2mps25_info *s2mps25 = data;
	uint32_t i;

	mutex_lock(&s2mps25->lock);

	for (i = 0; i < S2MPS25_BUCK_MAX; i++) {
		if (s2mps25_static_info->buck_oi_irq[i] == irq) {
			s2mps25_buck_oi_cnt[i]++;
			if (i < BUCK_SR1M_IDX)
				pr_info("%s: BUCK[%d] OI IRQ : %d, %d\n",
					__func__, i + 1, s2mps25_buck_oi_cnt[i], irq);
			else
				pr_info("%s: BUCK_SR[%d] OI IRQ : %d, %d\n",
					__func__, i + 1 - BUCK_SR1M_IDX, s2mps25_buck_oi_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mps25->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mps25_temp_irq(int irq, void *data)
{
	struct s2mps25_info *s2mps25 = data;

	mutex_lock(&s2mps25->lock);

	if (s2mps25_static_info->temp_irq[0] == irq) {
		s2mps25_temp_cnt[0]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, s2mps25_temp_cnt[0], irq);
	} else if (s2mps25_static_info->temp_irq[1] == irq) {
		s2mps25_temp_cnt[1]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, s2mps25_temp_cnt[1], irq);
	}

	mutex_unlock(&s2mps25->lock);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
int main_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	if (!i2c)
		return -ENODEV;

	return s2mps25_read_reg(i2c, reg, val);
}
EXPORT_SYMBOL_GPL(main_pmic_read_reg);

int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	if (!i2c)
		return -ENODEV;

	return s2mps25_update_reg(i2c, reg, val, mask);
}
EXPORT_SYMBOL_GPL(main_pmic_update_reg);

int main_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mps25_static_info)
		return -ENODEV;

	*i2c = s2mps25_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(main_pmic_get_i2c);
#endif

struct s2mps25_oi_data {
	uint8_t reg;
	uint8_t val;
};

#define DECLARE_OI(_reg, _val)	{ .reg = (_reg), .val = (_val) }
static const struct s2mps25_oi_data s2mps25_oi[] = {
	/* BUCK 1~10, SR1M~SR3M OI function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS25_REG_BUCK1M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK2M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK3M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK4M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK5M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK6M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK7M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK8M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK9M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK10M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK_SR1M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK_SR2M_OCP, 0xC4),
	DECLARE_OI(S2MPS25_REG_BUCK_SR3M_OCP, 0xC4),
};

static int s2mps25_oi_function(struct s2mps25_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic1;
	uint32_t i;
	uint8_t val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mps25_oi); i++) {
		ret = s2mps25_write_reg(i2c, s2mps25_oi[i].reg, s2mps25_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mps25_oi); i++) {
		ret = s2mps25_read_reg(i2c, s2mps25_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mps25_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mps25_set_interrupt(struct platform_device *pdev,
				 struct s2mps25_info *s2mps25, int irq_base)
{
	int i, ret;
	static char buck_ocp_name[S2MPS25_BUCK_MAX][32] = {0, };
	static char buck_oi_name[S2MPS25_BUCK_MAX][32] = {0, };
	static char temp_name[S2MPS25_TEMP_MAX][32] = {0, };

	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	/* BUCK 1~10,SR1M~SR3M OCP interrupt */
	for (i = 0; i < S2MPS25_BUCK_MAX; i++) {
		s2mps25->buck_ocp_irq[i] = irq_base +
					   S2MPS25_PMIC_IRQ_OCP_B1M_INT4 + i;

		/* Dynamic allocation for device name */
		if (i < BUCK_SR1M_IDX)
			snprintf(buck_ocp_name[i], sizeof(buck_ocp_name[i]) - 1, "BUCK_OCP_IRQ%d@%s",
				 i + 1, dev_name(s2mps25->iodev->dev));
		else
			snprintf(buck_ocp_name[i], sizeof(buck_ocp_name[i]) - 1, "BUCK_SR_OCP_IRQ%d@%s",
				 i + 1 - BUCK_SR1M_IDX, dev_name(s2mps25->iodev->dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps25->buck_ocp_irq[i], NULL,
						s2mps25_buck_ocp_irq, 0,
						buck_ocp_name[i], s2mps25);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mps25->buck_ocp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK 1~10,SR1M~SR3M OI interrupt */
	for (i = 0; i < S2MPS25_BUCK_MAX; i++) {
		s2mps25->buck_oi_irq[i] = irq_base +
					   S2MPS25_PMIC_IRQ_OI_B1M_INT6 + i;

		/* Dynamic allocation for device name */
		if (i < BUCK_SR1M_IDX)
			snprintf(buck_oi_name[i], sizeof(buck_oi_name[i]) - 1, "BUCK_OI_IRQ%d@%s",
				 i + 1, dev_name(s2mps25->iodev->dev));
		else
			snprintf(buck_oi_name[i], sizeof(buck_oi_name[i]) - 1, "BUCK_SR_OI_IRQ%d@%s",
				 i + 1 - BUCK_SR1M_IDX, dev_name(s2mps25->iodev->dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps25->buck_oi_irq[i], NULL,
						s2mps25_buck_oi_irq, 0,
						buck_oi_name[i], s2mps25);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request BUCK[%d] OI IRQ: %d: %d\n",
				i + 1, s2mps25->buck_oi_irq[i], ret);
			goto err;
		}
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPS25_TEMP_MAX; i++) {
		s2mps25->temp_irq[i] = irq_base + S2MPS25_PMIC_IRQ_120C_INT3 + i;

		/* Dynamic allocation for device name */
		snprintf(temp_name[i], sizeof(temp_name[i]) - 1, "TEMP_IRQ%d@%s",
			 i + 1, dev_name(s2mps25->iodev->dev));

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps25->temp_irq[i], NULL,
						s2mps25_temp_irq, 0,
						temp_name[i], s2mps25);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request over temperature[%d] IRQ: %d: %d\n",
				i, s2mps25->temp_irq[i], ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mps25_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps25_info *s2mps25 = dev_get_drvdata(dev);
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

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&s2mps25->iodev->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&s2mps25->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	dev_info(s2mps25->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__, base_addr, reg_addr, val);
	s2mps25->base_addr = base_addr;
	s2mps25->read_addr = reg_addr;
	s2mps25->read_val = val;

	return size;
}

static ssize_t s2mps25_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps25_info *s2mps25 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mps25->base_addr, s2mps25->read_addr, s2mps25->read_val);
}

static ssize_t s2mps25_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps25_info *s2mps25 = dev_get_drvdata(dev);
	uint8_t base_addr = 0, reg_addr = 0, data = 0;
	int ret;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx 0x%02hhx", &base_addr, &reg_addr, &data);
	if (ret != 3) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	switch (base_addr) {
	case I2C_BASE_VGPIO:
	case I2C_BASE_COMMON:
	case I2C_BASE_RTC:
	case I2C_BASE_PM1:
	case I2C_BASE_PM2:
	case I2C_BASE_ADC:
	case I2C_BASE_CLOSE1:
	case I2C_BASE_CLOSE2:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}
	dev_info(s2mps25->dev, "%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__, base_addr, reg_addr, data);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&s2mps25->iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MAIN_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&s2mps25->iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mps25_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mps25_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(write, S_IRUGO | S_IWUSR, s2mps25_write_show, s2mps25_write_store),
	PMIC_ATTR(read, S_IRUGO | S_IWUSR, s2mps25_read_show, s2mps25_read_store),
};

static int s2mps25_create_sysfs(struct s2mps25_info *s2mps25)
{
	struct device *s2mps25_pmic = s2mps25->dev;
	struct device *dev = s2mps25->iodev->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mps25->base_addr = 0;
	s2mps25->read_addr = 0;
	s2mps25->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps25_pmic = pmic_device_create(s2mps25, device_name);
	s2mps25->dev = s2mps25_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(s2mps25_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps25_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps25_pmic->devt);

	return -1;
}
#endif

static int s2mps25_set_powermeter(struct s2mps25_dev *iodev,
				  struct s2mps25_platform_data *pdata)
{
	if (pdata->adc_mode > 0) {
		iodev->adc_mode = pdata->adc_mode;
		s2mps25_powermeter_init(iodev);

		return 0;
	}

	return -1;
}

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static bool is_enabled_sr2m_dvs(struct s2mps25_info *s2mps25)
{
	struct device_node *pmic_np;

	pmic_np = s2mps25->iodev->dev->of_node;
	if (of_property_read_bool(pmic_np, "disable_buck_sr2m_dvs"))
		return false;
	else
		return true;
}
#endif

static int s2mps25_set_regulator_vol(struct s2mps25_info *s2mps25)
{
	int ret = 0;

	/* Set SICD DVS voltage BUCK3M(VDD_INT), BUCK7M(VDD_CPUCL0_LIT) */
	ret |= s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_BUCK3M_OUT1, 0x20);	//0.500V
	ret |= s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_BUCK7M_OUT1, 0x1C);  //0.475V

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	/* Set SR2M DVS voltage */
	ret |= exynos_acpm_write_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM2, S2MPS25_REG_M_SEL_SRDVS_EN, 0x10);
	if (is_enabled_sr2m_dvs(s2mps25))
		ret |= s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_BUCK_SR2M_OUT2, 0xB8);  //1.850V
	else
		ret |= s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_BUCK_SR2M_OUT2, 0xC8);  //1.950V
#endif
	if (ret) {
		pr_err("%s %d: failed to write register\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int __maybe_unused s2mps25_power_off_seq_wa(void)
{
	int ret = 0;

	return ret;
}

static uint8_t off_time_delay_val; /*  WA: Off time delay init */

static int s2mps25_pmic_probe(struct platform_device *pdev)
{
	struct s2mps25_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps25_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps25_info *s2mps25;
	int ret;
	uint32_t i;

	pr_info("[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mps25_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps25 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps25_info), GFP_KERNEL);
	if (!s2mps25)
		return -ENOMEM;

	s2mps25->iodev = iodev;
	s2mps25->i2c = iodev->pmic1;

	mutex_init(&s2mps25->lock);

	s2mps25_static_info = s2mps25;

	platform_set_drvdata(pdev, s2mps25);

	config.dev = &pdev->dev;
	config.driver_data = s2mps25;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		s2mps25->opmode[id] = regulators[id].enable_mask;
		s2mps25->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators[id], &config);
		if (IS_ERR(s2mps25->rdev[i])) {
			ret = PTR_ERR(s2mps25->rdev[i]);
			dev_err(&pdev->dev, "[PMIC] regulator init failed for %s(%d)\n",
				regulators[i].name, i);
			goto err_s2mps25_data;
		}
	}

	s2mps25->num_regulators = pdata->num_rdata;

	ret = s2mps25_set_interrupt(pdev, s2mps25, pdata->irq_base);
	if (ret < 0) {
		pr_err("%s: s2mps25_set_interrupt fail(%d)\n", __func__, ret);
		goto err_s2mps25_data;
	}
	ret = s2mps25_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mps25_oi_function fail\n", __func__);

	ret = s2mps25_set_powermeter(iodev, pdata);
	if (ret < 0)
		pr_err("%s: Powermeter disable\n", __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mps25_create_sysfs(s2mps25);
	if (ret < 0) {
		pr_err("%s: s2mps25_create_sysfs fail\n", __func__);
		goto err_s2mps25_data;
	}
#endif
	ret = s2mps25_set_regulator_vol(s2mps25);
	if (ret < 0) {
		pr_err("%s: s2mps25_set_regulator_vol fail\n", __func__);
		goto err_s2mps25_data;
	}

	/* WA: Off time delay init */
	ret = s2mps25_read_reg(s2mps25->iodev->pmic2, 0x7A, &off_time_delay_val);
	if (ret)
		pr_err("%s: Failed to get off time delay\n", __func__);

	exynos_reboot_register_pmic_ops(NULL, NULL, s2mps25_power_off_seq_wa, s2mps25_read_pwron_status);

	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_s2mps25_data:
	mutex_destroy(&s2mps25->lock);
err_pdata:
	return ret;
}

static int s2mps25_pmic_remove(struct platform_device *pdev)
{
	struct s2mps25_info *s2mps25 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps25_pmic = s2mps25->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(s2mps25_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps25_pmic->devt);
#endif
	if (s2mps25->iodev->adc_mode > 0)
		s2mps25_powermeter_deinit(s2mps25->iodev);
	mutex_destroy(&s2mps25->lock);
	return 0;
}

static void s2mps25_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mps25_info *s2mps25 = platform_get_drvdata(pdev);

	/* Power-meter off */
	if (s2mps25->iodev->adc_mode > 0)
		if (s2mps25_adc_set_enable(s2mps25->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);

	pr_info("%s: Power-meter off\n", __func__);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mps25_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps25_info *s2mps25 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps25->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter off */
	if (s2mps25->iodev->adc_mode > 0)
		if (s2mps25_adc_set_enable(s2mps25->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);

	/* WA: Off time reduction */
	ret = s2mps25_write_reg(s2mps25->iodev->pmic2, 0x7A, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time\n", __func__);

	return 0;
}

static int s2mps25_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps25_info *s2mps25 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps25->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter on */
	if (s2mps25->iodev->adc_mode > 0)
		if (s2mps25_adc_set_enable(s2mps25->iodev->adc_meter, 1) < 0)
			pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);

	/* WA: Restore off time reduction */
	ret = s2mps25_write_reg(s2mps25->iodev->pmic2, 0x7A, off_time_delay_val);
	if (ret)
		pr_err("%s: Failed to restore off time\n", __func__);

	return 0;
}
#else
#define s2mps25_pmic_suspend	NULL
#define s2mps25_pmic_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(s2mps25_pmic_pm, s2mps25_pmic_suspend, s2mps25_pmic_resume);

static const struct platform_device_id s2mps25_pmic_id[] = {
	{ "s2mps25-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps25_pmic_id);

static struct platform_driver s2mps25_pmic_driver = {
	.driver = {
		.name = "s2mps25-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mps25_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mps25_pmic_probe,
	.remove = s2mps25_pmic_remove,
	.shutdown = s2mps25_pmic_shutdown,
	.id_table = s2mps25_pmic_id,
};

module_platform_driver(s2mps25_pmic_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPS25 Regulator Driver");
MODULE_LICENSE("GPL");
