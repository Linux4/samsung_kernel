/*
 * s2mpu12-regulator.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
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
#include <linux/mfd/samsung/s2mpu12.h>
#include <linux/mfd/samsung/s2mpu12-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/sec_pm_debug.h>
#endif /* CONFIG_SEC_PM_DEBUG */

#ifdef CONFIG_SEC_PMIC_PWRKEY
extern int (*pmic_key_get_pwrkey)(void);
#endif

static struct s2mpu12_info *static_info;
static struct regulator_desc regulators[S2MPU12_REGULATOR_MAX];

int s2mpu12_buck_ocp_cnt[S2MPU12_BUCK_MAX]; /* BUCK 1~5 OCP count */
int s2mpu12_bb_ocp_cnt; /* BUCK-BOOST OCP count */
int s2mpu12_buck_oi_cnt[S2MPU12_BUCK_MAX]; /* BUCK 1~5 OI count */
int s2mpu12_temp_cnt[S2MPU12_TEMP_MAX]; /* 0 : 120 degree , 1 : 140 degree */

struct s2mpu12_info {
	struct regulator_dev *rdev[S2MPU12_REGULATOR_MAX];
	struct s2mpu12_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	unsigned int opmode[S2MPU12_REGULATOR_MAX];
	int buck_ocp_irq[S2MPU12_BUCK_MAX]; /* BUCK OCP IRQ */
	int bb_ocp_irq; /* BUCK-BOOST OCP IRQ */
	int buck_oi_irq[S2MPU12_BUCK_MAX]; /* BUCK OI IRQ */
	int temp_irq[2]; /* 0 : 120 degree, 1 : 140 degree */
	int num_regulators;
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

static unsigned int s2mpu12_of_map_mode(unsigned int val)
{
	switch (val) {
	case SEC_OPMODE_SUSPEND:	/* ON in Standby Mode */
		return 0x1;
	case SEC_OPMODE_MIF:		/* ON in PWREN_MIF mode */
		return 0x2;
	case SEC_OPMODE_ON:		/* ON in Normal Mode */
		return 0x3;
	default:
		return 0x3;
	}
}

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU12_ENABLE_SHIFT;
	s2mpu12->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);

	return s2mpu12_update_reg(s2mpu12->i2c, rdev->desc->enable_reg,
				  s2mpu12->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	u8 val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu12_update_reg(s2mpu12->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;


	ret = s2mpu12_read_reg(s2mpu12->i2c,
				rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
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
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPU12_BUCK1:
	case S2MPU12_BUCK2:
		ramp_shift = 6;
		break;
/*
	case S2MPU12_BUCK3:
		ramp_shift = 4;
		break;
*/
	case S2MPU12_BUCK4:
	case S2MPU12_BUCK5:
		ramp_shift = 2;
		break;
	default:
		return -EINVAL;
	}

	return s2mpu12_update_reg(s2mpu12->i2c, S2MPU12_PMIC_BUCK_RISE_RAMP,
				  ramp_value << ramp_shift,
				  ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu12_read_reg(s2mpu12->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);
	int ret;
	unsigned int voltage;

	voltage = ((sel & rdev->desc->vsel_mask) * S2MPU12_LDO_STEP2) +
		  S2MPU12_LDO_MIN1;
	ret = s2mpu12_update_reg(s2mpu12->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu12_update_reg(s2mpu12->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					   unsigned sel)
{
	int ret;
	struct s2mpu12_info *s2mpu12 = rdev_get_drvdata(rdev);

	ret = s2mpu12_write_reg(s2mpu12->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = s2mpu12_update_reg(s2mpu12->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;

i2c_out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
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

static struct regulator_ops s2mpu12_ldo_ops = {
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

static struct regulator_ops s2mpu12_buck_ops = {
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

#define _BUCK(macro)	S2MPU12_BUCK##macro
#define _buck_ops(num)	s2mpu12_buck_ops##num

#define _LDO(macro)	S2MPU12_LDO##macro
#define _REG(ctrl)	S2MPU12_PMIC##ctrl
#define _ldo_ops(num)	s2mpu12_ldo_ops##num
#define _TIME(macro)	S2MPU12_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU12_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU12_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU12_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu12_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU12_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU12_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU12_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu12_of_map_mode			\
}

static struct regulator_desc regulators[S2MPU12_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN3),
		 _LDO(_STEP3), _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN3),
		 _LDO(_STEP3), _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),

	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
/*
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),

	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
 */
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L28CTRL), _REG(_L28CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L29CTRL), _REG(_L29CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L30CTRL), _REG(_L30CTRL), _TIME(_LDO)),

	LDO_DESC("LDO31", _LDO(31), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L31CTRL), _REG(_L31CTRL), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L32CTRL), _REG(_L32CTRL), _TIME(_LDO)),
	LDO_DESC("LDO33", _LDO(33), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L33CTRL), _REG(_L33CTRL), _TIME(_LDO)),
	LDO_DESC("LDO34", _LDO(34), &_ldo_ops(), _LDO(_MIN5),
		 _LDO(_STEP5), _REG(_L34CTRL), _REG(_L34CTRL), _TIME(_LDO)),
	LDO_DESC("LDO35", _LDO(35), &_ldo_ops(), _LDO(_MIN4),
		 _LDO(_STEP4), _REG(_L35CTRL), _REG(_L35CTRL), _TIME(_LDO)),
	LDO_DESC("LDO36", _LDO(36), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L36CTRL), _REG(_L36CTRL), _TIME(_LDO)),

	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B1OUT1), _REG(_B1CTRL), _TIME(_BUCK1)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B2OUT1), _REG(_B2CTRL), _TIME(_BUCK2)),
/*
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B3OUT), _REG(_B3CTRL), _TIME(_BUCK3)),
 */
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN2),
		  _BUCK(_STEP2), _REG(_B4OUT1), _REG(_B4CTRL), _TIME(_BUCK4)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN3),
		  _BUCK(_STEP3), _REG(_B5OUT1), _REG(_B5CTRL), _TIME(_BUCK5)),
};
#ifdef CONFIG_OF
static int s2mpu12_pmic_dt_parse_pdata(struct s2mpu12_dev *iodev,
					struct s2mpu12_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu12_regulator_data *rdata;
	size_t i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	pdata->loop_bw_en = false;
	if (of_get_property(pmic_np, "pmic_loop_bw", NULL))
		pdata->loop_bw_en = true;

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
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(iodev->dev,
							     reg_np,
							     &regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mpu12_pmic_dt_parse_pdata(struct s2mpu12_pmic_dev *iodev,
					struct s2mpu12_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_DRV_SAMSUNG_PMIC
static ssize_t s2mpu12_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpu12_info *s2mpu12 = dev_get_drvdata(dev);
	int ret;
	u8 val, reg_addr;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = kstrtou8(buf, 0, &reg_addr);
	if (ret < 0)
		pr_info("%s: fail to transform i2c address\n", __func__);

	ret = s2mpu12_read_reg(s2mpu12->i2c, reg_addr, &val);
	if (ret < 0)
		pr_info("%s: fail to read i2c address\n", __func__);

	pr_info("%s: reg(0x%02x) data(0x%02x)\n", __func__, reg_addr, val);
	s2mpu12->read_addr = reg_addr;
	s2mpu12->read_val = val;

	return size;
}

static ssize_t s2mpu12_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpu12_info *s2mpu12 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02x: 0x%02x\n", s2mpu12->read_addr,
		       s2mpu12->read_val);
}

static ssize_t s2mpu12_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mpu12_info *s2mpu12 = dev_get_drvdata(dev);
	int ret;
	unsigned int reg, data;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "%x %x", &reg, &data);
	if (ret != 2) {
		pr_info("%s: input error\n", __func__);
		return size;
	}

	pr_info("%s: reg(0x%02x) data(0x%02x)\n", __func__, reg, data);

	ret = s2mpu12_write_reg(s2mpu12->i2c, reg, data);
	if (ret < 0)
		pr_info("%s: fail to write i2c addr/data\n", __func__);

	return size;
}

static ssize_t s2mpu12_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpu12_write\n");
}

static DEVICE_ATTR(s2mpu12_write,
		   0644, s2mpu12_write_show, s2mpu12_write_store);
static DEVICE_ATTR(s2mpu12_read,
		   0644, s2mpu12_read_show, s2mpu12_read_store);

int create_s2mpu12_sysfs(struct s2mpu12_info *s2mpu12)
{
	struct device *s2mpu12_pmic = s2mpu12->dev;
	int err = -ENODEV;

	pr_info("%s: master pmic sysfs start\n", __func__);
	s2mpu12->read_addr = 0;
	s2mpu12->read_val = 0;

	s2mpu12_pmic = pmic_device_create(s2mpu12, "s2mpu12");

	err = device_create_file(s2mpu12_pmic, &dev_attr_s2mpu12_write);
	if (err)
		pr_err("s2mpu12_sysfs: failed to create device file, %s\n",
			dev_attr_s2mpu12_write.attr.name);

	err = device_create_file(s2mpu12_pmic, &dev_attr_s2mpu12_read);
	if (err)
		pr_err("s2mpu12_sysfs: failed to create device file, %s\n",
			dev_attr_s2mpu12_read.attr.name);

	return 0;
}
#endif
int s2mpu12_read_pwron_status(void)
{
	u8 val;
	struct s2mpu12_info *s2mpu12 = static_info;

	s2mpu12_read_reg(s2mpu12->i2c, S2MPU12_PMIC_STATUS1, &val);
	pr_info("%s : 0x%x\n", __func__, val);

	return (val & 0x1);
}

static irqreturn_t s2mpu12_buck_ocp_irq(int irq, void *dev_id)
{
	struct s2mpu12_info *s2mpu12 = dev_id;
	int i;

	mutex_lock(&s2mpu12->lock);

	for (i = 0; i < S2MPU12_BUCK_MAX; i++) {
		if (s2mpu12->buck_ocp_irq[i] == irq) {
			s2mpu12_buck_ocp_cnt[i]++;
			pr_info("%s: BUCK[%d] OCP IRQ: %d, %d\n",
				__func__, i + 1, s2mpu12_buck_ocp_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mpu12->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu12_bb_ocp_irq(int irq, void *dev_id)
{
	struct s2mpu12_info *s2mpu12 = dev_id;

	mutex_lock(&s2mpu12->lock);

	s2mpu12_bb_ocp_cnt++;
	pr_info("%s: BUCKBOOST OCP IRQ: %d, %d\n",
		__func__, s2mpu12_bb_ocp_cnt, irq);

	mutex_unlock(&s2mpu12->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu12_buck_oi_irq(int irq, void *dev_id)
{
	struct s2mpu12_info *s2mpu12 = dev_id;
	int i;

	mutex_lock(&s2mpu12->lock);

	for (i = 0; i < S2MPU12_BUCK_MAX; i++) {
		if (s2mpu12->buck_oi_irq[i] == irq) {
			s2mpu12_buck_oi_cnt[i]++;
			pr_info("%s: S2MPU12 BUCK[%d] OI IRQ: %d, %d\n",
				__func__, i + 1, s2mpu12_buck_oi_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mpu12->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu12_temp_irq(int irq, void *dev_id)
{
	struct s2mpu12_info *s2mpu12 = dev_id;

	mutex_lock(&s2mpu12->lock);

	if (s2mpu12->temp_irq[0] == irq) {
		s2mpu12_temp_cnt[0]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, s2mpu12_temp_cnt[0], irq);
	} else if (s2mpu12->temp_irq[1] == irq) {
		s2mpu12_temp_cnt[1]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, s2mpu12_temp_cnt[1], irq);
	}

	mutex_unlock(&s2mpu12->lock);

	return IRQ_HANDLED;
}

static int s2mpu12_oi_function(struct s2mpu12_info *s2mpu12)
{
	struct i2c_client *i2c = s2mpu12->i2c;
	ssize_t i = 0;
	u8 val = 0;
	int ret = 0;

	/* BUCK1~9 & BUCK-BOOST OI function enable */
	ret = s2mpu12_update_reg(i2c, S2MPU12_PMIC_BUCK_OI_EN, 0x1F, 0x1F);
	if (ret)
		goto err;

	/* BUCK1~9 & BUCK-BOOST OI power down disable */
	ret = s2mpu12_update_reg(i2c, S2MPU12_PMIC_BUCK_OI_PD_EN, 0x00, 0x1F);
	if (ret)
		goto err;

	/* OI detection time window : 300us, OI comp. output count : 50 times */
	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_BUCK_OI_CTRL1, 0xCC);
	if (ret)
		goto err;
	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_BUCK_OI_CTRL2, 0xCC);
	if (ret)
		goto err;
	ret = s2mpu12_update_reg(i2c, S2MPU12_PMIC_BUCK_OI_CTRL3, 0x0C, 0x0C);
	if (ret)
		goto err;

	for (i = S2MPU12_PMIC_BUCK_OI_EN; i <= S2MPU12_PMIC_BUCK_OI_PD_EN; i++) {
		ret = s2mpu12_read_reg(i2c, i, &val);
		if (ret)
			goto err;
		pr_info("0x%x[0x%x], ", i, val);
	}
	pr_info("\n");
	pr_info("%s: Done\n", __func__);

	return 0;
err:
	return -1;
}

static int s2mpu12_set_interrupt(struct platform_device *pdev,
				 struct s2mpu12_info *s2mpu12, int irq_base)
{
	ssize_t i = 0;
	int ret = 0;

	/* BUCK1~5 OCP interrupt */
	for (i = 0; i < S2MPU12_BUCK_MAX; i++) {
		s2mpu12->buck_ocp_irq[i] = irq_base +
					   S2MPU12_PMIC_IRQ_OCPB1_INT3 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu12->buck_ocp_irq[i], NULL,
						s2mpu12_buck_ocp_irq, 0,
						"BUCK_OCP_IRQ", s2mpu12);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mpu12->buck_ocp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK-BOOST OCP interrupt */
	s2mpu12->bb_ocp_irq = irq_base + S2MPU12_PMIC_IRQ_OCPBST_INT3;
	ret = devm_request_threaded_irq(&pdev->dev,
					s2mpu12->bb_ocp_irq, NULL,
					s2mpu12_bb_ocp_irq, 0,
					"BB_OCP_IRQ", s2mpu12);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to request BUCKBOOST OCP IRQ: %d: %d\n",
			s2mpu12->bb_ocp_irq, ret);
		goto err;
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPU12_TEMP_MAX; i++) {
		s2mpu12->temp_irq[i] = irq_base +
				       S2MPU12_PMIC_IRQ_INT120C_INT6 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu12->temp_irq[i], NULL,
						s2mpu12_temp_irq, 0,
						"TEMP_IRQ", s2mpu12);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request over temperature[%d] "
				"IRQ: %d: %d\n",
				i, s2mpu12->temp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK1~5 OI interrupt */
	for (i = 0; i < S2MPU12_BUCK_MAX; i++) {
		s2mpu12->buck_oi_irq[i] = irq_base +
					  S2MPU12_PMIC_IRQ_OI_B1_INT5 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu12->buck_oi_irq[i], NULL,
						s2mpu12_buck_oi_irq, 0,
						"BUCK_OI_IRQ", s2mpu12);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OI IRQ: %d: %d\n",
				i + 1, s2mpu12->buck_oi_irq[i], ret);
			goto err;
		}
	}

	pr_info("%s: Done\n", __func__);

	return 0;
err:
	return -1;
}
#if 0
static int s2mpu12_set_stable_time(struct s2mpu12_info *s2mpu12)
{
	int ret;
	/* set VDD_CPUCL1 stable time */
	ret = s2mpu12_update_reg(s2mpu12->i2c,
				 S2MPU12_PMIC_ON_SEQ_SEL1, 0x00, 0xF0);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}
	return 0;
}
#endif
static int s2mpu12_set_on_sequence(struct s2mpu12_info *s2mpu12)
{
	struct i2c_client *i2c = s2mpu12->i2c;
	int ret = 0;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL23, 0x04);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL24, 0x01);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL1, 0x00);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL2, 0x10);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL5, 0x32);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL6, 0x51);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL7, 0x14);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL8, 0x01);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL9, 0x13);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL10, 0x32);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL12, 0x58);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL13, 0x76);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL14, 0x94);
	if (ret)
		goto err;

	ret = s2mpu12_write_reg(i2c, S2MPU12_PMIC_ON_SEQ_SEL21, 0x10);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

static int s2mpu12_lower_loop_BW(struct s2mpu12_info *s2mpu12, bool loop_bw_en)
{
	int ret = 0;

	if (!loop_bw_en)
		return 0;

	/* lower loop bw */
	ret = s2mpu12_write_reg(s2mpu12->iodev->close, 0x0E, 0x42);
	if (ret < 0)
		return ret;

	ret = s2mpu12_write_reg(s2mpu12->iodev->close, 0x08, 0x42);
	if (ret < 0)
		return ret;

	return 0;
}

static int s2mpu12_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu12_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu12_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu12_info *s2mpu12;
	int i, ret, irq_base;

	pr_info("%s s2mpu12 pmic driver Loading start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu12_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu12 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu12_info),
				GFP_KERNEL);
	if (!s2mpu12)
		return -ENOMEM;

	irq_base = pdata->irq_base;
	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	s2mpu12->iodev = iodev;
	s2mpu12->i2c = iodev->pmic;

	mutex_init(&s2mpu12->lock);
	static_info = s2mpu12;

	platform_set_drvdata(pdev, s2mpu12);
	
#ifdef CONFIG_SEC_PM_DEBUG
	ret = s2mpu12_read_reg(s2mpu12->i2c, S2MPU12_PMIC_PWRONSRC,
			&pmic_onsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read PWRONSRC\n");

	ret = s2mpu12_read_reg(s2mpu12->i2c, S2MPU12_PMIC_OFFSRC,
			&pmic_offsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read OFFSRC\n");

	/* Clear OFFSRC register */
	ret = s2mpu12_write_reg(s2mpu12->i2c, S2MPU12_PMIC_OFFSRC, 0);
	if (ret)
		dev_err(&pdev->dev, "failed to write OFFSRC\n");
#endif /* CONFIG_SEC_PM_DEBUG */

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu12;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu12->opmode[id] =
			regulators[id].enable_mask;

		s2mpu12->rdev[i] = regulator_register(
				&regulators[id], &config);
		if (IS_ERR(s2mpu12->rdev[i])) {
			ret = PTR_ERR(s2mpu12->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mpu12->rdev[i] = NULL;
			goto err;
		}

	}

	s2mpu12->num_regulators = pdata->num_regulators;

	/* Interrupt setting */
	ret = s2mpu12_set_interrupt(pdev, s2mpu12, irq_base);
	if (ret < 0) {
		pr_err("%s: failed to set interrupt\n", __func__);
		goto err;
	}

	/* OI setting */
	ret = s2mpu12_oi_function(s2mpu12);
	if (ret < 0) {
		pr_err("%s: failed to set OI function\n", __func__);
		goto err;
	}
#if 0
	/* Regulator stable time setting */
	ret = s2mpu12_set_stable_time(s2mpu12);
	if (ret < 0)
		goto err;
#endif

	/* Regulator on-seq. setting */
	if (s2mpu12_set_on_sequence(s2mpu12) < 0) {
		pr_err("%s: s2mpu12_set_on_sequence fail\n", __func__);
		goto err;
	}

#ifdef CONFIG_DRV_SAMSUNG_PMIC
	create_s2mpu12_sysfs(s2mpu12);
#endif

	s2mpu12_update_reg(s2mpu12->i2c, S2MPU12_PMIC_SEL_VGPIO0,
			(V_PWREN_MIF << B2_VGPIO_SEL) | (V_PWREN_CPUCL0 << B1_VGPIO_SEL),
			(VGPIO_SEL_MASK << B2_VGPIO_SEL) | (VGPIO_SEL_MASK << B1_VGPIO_SEL));
	s2mpu12_update_reg(s2mpu12->i2c, S2MPU12_PMIC_SEL_VGPIO2,
			(V_PWREN_MIF << L6_VGPIO_SEL) | (V_PWREN_MIF << L4_VGPIO_SEL) | (V_PWREN_MIF << L3_VGPIO_SEL),
			(VGPIO_SEL_MASK << L6_VGPIO_SEL) | (VGPIO_SEL_MASK << L4_VGPIO_SEL) | (VGPIO_SEL_MASK << L3_VGPIO_SEL));
	s2mpu12_update_reg(s2mpu12->i2c, S2MPU12_PMIC_SEL_VGPIO3,
			(V_PWREN_MIF << L9_VGPIO_SEL)| (V_PWREN_CPUCL0 << L8_VGPIO_SEL) | (V_PWREN_MIF << L7_VGPIO_SEL),
			(VGPIO_SEL_MASK << L9_VGPIO_SEL)| (VGPIO_SEL_MASK << L8_VGPIO_SEL) | (VGPIO_SEL_MASK << L7_VGPIO_SEL));
#ifdef CONFIG_SOC_EXYNOS3830
	/* SICD_DVS voltage settings - BUCK1/2_OUT2 */
	s2mpu12_write_reg(s2mpu12->i2c, S2MPU12_PMIC_B1OUT2, 0x20);
	s2mpu12_write_reg(s2mpu12->i2c, S2MPU12_PMIC_B2OUT2, 0x20);

#endif

#ifdef CONFIG_SEC_PMIC_PWRKEY
	pmic_key_get_pwrkey = s2mpu12_read_pwron_status;
#endif

	ret = s2mpu12_lower_loop_BW(s2mpu12, pdata->loop_bw_en);
	if (ret < 0)
		goto err;

	pr_info("%s s2mpu12 pmic driver Loading end\n", __func__);
	return 0;
err:
	for (i = 0; i < S2MPU12_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu12->rdev[i]);

	return ret;
}

static int s2mpu12_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu12_info *s2mpu12 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPU12_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu12->rdev[i]);

#ifdef CONFIG_DRV_SAMSUNG_PMIC
	pmic_device_destroy(s2mpu12->dev->devt);
#endif
	return 0;
}

static const struct platform_device_id s2mpu12_pmic_id[] = {
	{ "s2mpu12-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu12_pmic_id);

static struct platform_driver s2mpu12_pmic_driver = {
	.driver = {
		.name = "s2mpu12-regulator",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu12_pmic_probe,
	.remove = s2mpu12_pmic_remove,
	.id_table = s2mpu12_pmic_id,
};

static int __init s2mpu12_pmic_init(void)
{
	return platform_driver_register(&s2mpu12_pmic_driver);
}
subsys_initcall(s2mpu12_pmic_init);

static void __exit s2mpu12_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu12_pmic_driver);
}
module_exit(s2mpu12_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPU12 Regulator Driver");
MODULE_LICENSE("GPL");
