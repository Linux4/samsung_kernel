/*
 * s2mpu11-regulator.c
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
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/s2mpu11.h>
#include <linux/mfd/samsung/s2mpu11-regulator.h>
#include <linux/io.h>
#include <linux/regulator/pmic_class.h>

static struct s2mpu11_info *static_info;
static struct regulator_desc regulators[S2MPU11_REGULATOR_MAX];

struct s2mpu11_info {
	struct regulator_dev *rdev[S2MPU11_REGULATOR_MAX];
	struct s2mpu11_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	unsigned int opmode[S2MPU11_REGULATOR_MAX];
	int num_regulators;
	u8 wtsr_en;
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

static unsigned int s2mpu11_of_map_mode(unsigned int val)
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
static int s2m_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU11_ENABLE_SHIFT;
	s2mpu11->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);

	return s2mpu11_update_reg(s2mpu11->i2c, rdev->desc->enable_reg,
				  s2mpu11->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	u8 val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu11_update_reg(s2mpu11->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;


	ret = s2mpu11_read_reg(s2mpu11->i2c, rdev->desc->enable_reg, &val);
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
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	int ramp_shift, ramp_addr, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPU11_BUCK1:
		ramp_shift = 6;
		break;
	case S2MPU11_BUCK2:
		ramp_shift = 4;
		break;
	case S2MPU11_BUCK3:
		ramp_shift = 2;
		break;
/*
	case S2MPU11_BUCK4:
		ramp_shift = 2;
		break;
	case S2MPU11_BUCK5:
		ramp_shift = 0;
		break;
 */
	default:
		return -EINVAL;
	}

	switch (reg_id) {
	case S2MPU11_BUCK1:
	case S2MPU11_BUCK2:
	case S2MPU11_BUCK3:
/*
	case S2MPU11_BUCK4:
	case S2MPU11_BUCK5:
 */
		ramp_addr = S2MPU11_PMIC_REG_BUCK_RISE_RAMP;
		break;
	default:
		return -EINVAL;
	}

	return s2mpu11_update_reg(s2mpu11->i2c, ramp_addr,
				  ramp_value << ramp_shift,
				  ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu11_read_reg(s2mpu11->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	int ret;
	char name[16] = {0, };

	snprintf(name, sizeof(name), "LDO%d", (reg_id - S2MPU11_LDO14) + 1);
	ret = s2mpu11_update_reg(s2mpu11->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu11_update_reg(s2mpu11->i2c, rdev->desc->apply_reg,
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
	struct s2mpu11_info *s2mpu11 = rdev_get_drvdata(rdev);

	ret = s2mpu11_write_reg(s2mpu11->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = s2mpu11_update_reg(s2mpu11->i2c, rdev->desc->apply_reg,
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

static struct regulator_ops s2mpu11_ldo_ops = {
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

static struct regulator_ops s2mpu11_buck_ops = {
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

#define _BUCK(macro)	S2MPU11_BUCK##macro
#define _buck_ops(num)	s2mpu11_buck_ops##num

#define _LDO(macro)	S2MPU11_LDO##macro
#define _REG(ctrl)	S2MPU11_PMIC_REG##ctrl
#define _ldo_ops(num)	s2mpu11_ldo_ops##num
#define _TIME(macro)	S2MPU11_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU11_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU11_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu11_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU11_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU11_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU11_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu11_of_map_mode			\
}

static struct regulator_desc regulators[S2MPU11_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	/* LDO 1~15 */
/*
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN3),
		 _LDO(_STEP3), _REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN3),
		 _LDO(_STEP3), _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN3),
		 _LDO(_STEP3), _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
 */
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	/* BUCK 1~5 */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B1OUT), _REG(_B1CTRL), _TIME(_BUCK1)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B2OUT), _REG(_B2CTRL), _TIME(_BUCK2)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B3OUT), _REG(_B3CTRL), _TIME(_BUCK3)),
/*
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B4OUT), _REG(_B4CTRL), _TIME(_BUCK4)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN1),
		  _BUCK(_STEP1), _REG(_B5OUT), _REG(_B5CTRL), _TIME(_BUCK5)),
 */
};
#ifdef CONFIG_OF
static int s2mpu11_pmic_dt_parse_pdata(struct s2mpu11_dev *iodev,
				       struct s2mpu11_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu11_regulator_data *rdata;
	unsigned int i;
	int ret;
	u32 val;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		pr_info("%s: fail to read wtsr_en\n", __func__);
	else
		pdata->wtsr_en = val;

	/* OCP_WARN */
	ret = of_property_read_u32(pmic_np, "ocp_warn_b2_en", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn_b2_en = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn_b2_cnt", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn_b2_cnt = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn_b2_dvs_mask", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn_b2_dvs_mask = val;


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
	pr_info("%s: num_regulators = %d\n", __func__, pdata->num_regulators);

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
		rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
							     &regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mpu11_pmic_dt_parse_pdata(struct s2mpu11_pmic_dev *iodev,
					struct s2mpu11_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static void s2mpu11_wtsr_enable(struct s2mpu11_info *s2mpu11,
				struct s2mpu11_platform_data *pdata)
{
	int ret;

	pr_info("%s: WTSR (%s)\n", __func__,
		pdata->wtsr_en ? "enable" : "disable");

	ret = s2mpu11_update_reg(s2mpu11->i2c, S2MPU11_PMIC_REG_CFG_PM,
				 S2MPU11_WTSREN_MASK, S2MPU11_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mpu11->wtsr_en = pdata->wtsr_en;
}

static void s2mpu11_wtsr_disable(struct s2mpu11_info *s2mpu11)
{
	int ret;

	pr_info("%s: disable WTSR\n", __func__);
	ret = s2mpu11_update_reg(s2mpu11->i2c, S2MPU11_PMIC_REG_CFG_PM,
				 0x00, S2MPU11_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}

#ifdef CONFIG_DRV_SAMSUNG_PMIC
static ssize_t s2mpu11_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpu11_info *s2mpu11 = dev_get_drvdata(dev);
	int ret;
	u8 val, reg_addr;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = kstrtou8(buf, 0, &reg_addr);
	if (ret < 0)
		pr_info("%s: fail to transform i2c address\n", __func__);

	ret = s2mpu11_read_reg(s2mpu11->i2c, reg_addr, &val);
	if (ret < 0)
		pr_info("%s: fail to read i2c address\n", __func__);

	pr_info("%s: reg(0x%02x) data(0x%02x)\n", __func__, reg_addr, val);
	s2mpu11->read_addr = reg_addr;
	s2mpu11->read_val = val;

	return size;
}

static ssize_t s2mpu11_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpu11_info *s2mpu11 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02x: 0x%02x\n", s2mpu11->read_addr,
		       s2mpu11->read_val);
}

static ssize_t s2mpu11_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mpu11_info *s2mpu11 = dev_get_drvdata(dev);
	int ret;
	u8 reg, data;

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

	ret = s2mpu11_write_reg(s2mpu11->i2c, reg, data);
	if (ret < 0)
		pr_info("%s: fail to write i2c addr/data\n", __func__);

	return size;
}

static ssize_t s2mpu11_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpu11_write\n");
}
static DEVICE_ATTR(s2mpu11_write, 0644,
		   s2mpu11_write_show, s2mpu11_write_store);
static DEVICE_ATTR(s2mpu11_read, 0644,
		   s2mpu11_read_show, s2mpu11_read_store);

int create_s2mpu11_sysfs(struct s2mpu11_info *s2mpu11)
{
	struct device *s2mpu11_pmic = s2mpu11->dev;
	int err = -ENODEV;

	pr_info("%s: slave pmic sysfs start\n", __func__);
	s2mpu11->read_addr = 0;
	s2mpu11->read_val = 0;

	s2mpu11_pmic = pmic_device_create(s2mpu11, "s2mpu11");

	err = device_create_file(s2mpu11_pmic, &dev_attr_s2mpu11_write);
	if (err) {
		pr_err("s2mpu11_sysfs: failed to create device file, %s\n",
			dev_attr_s2mpu11_write.attr.name);
	}

	err = device_create_file(s2mpu11_pmic, &dev_attr_s2mpu11_read);
	if (err) {
		pr_err("s2mpu11_sysfs: failed to create device file, %s\n",
			dev_attr_s2mpu11_read.attr.name);
	}

	return 0;
}
#endif

void s2mpu11_oi_function(struct s2mpu11_info *s2mpu11)
{
	struct i2c_client *i2c = s2mpu11->i2c;
	int i;
	u8 val;

	/* BUCK1~5 OI function enable & power down disable */
	s2mpu11_update_reg(i2c, S2MPU11_PMIC_REG_BUCK_OI_EN, 0x1F, 0x1F);
	s2mpu11_update_reg(i2c, S2MPU11_PMIC_REG_BUCK_OI_PD_EN, 0x1F, 0x1F);

	/* OI detection time window : 300us, OI comp. output count : 50 times */
	s2mpu11_write_reg(i2c, S2MPU11_PMIC_REG_BUCK_OI_CTRL1, 0xCC);
	s2mpu11_write_reg(i2c, S2MPU11_PMIC_REG_BUCK_OI_CTRL2, 0xCC);
	s2mpu11_update_reg(i2c, S2MPU11_PMIC_REG_BUCK_OI_CTRL3, 0x0C, 0x0C);

	pr_info("%s\n", __func__);
	for (i = S2MPU11_PMIC_REG_BUCK_OI_EN; i <= S2MPU11_PMIC_REG_BUCK_OI_CTRL3; i++) {
		s2mpu11_read_reg(i2c, i, &val);
		pr_info("0x%x[0x%x], ", i, val);
	}
	pr_info("\n");
}

static int s2mpu11_set_warn(struct platform_device *pdev,
			    struct s2mpu11_platform_data *pdata,
			    struct s2mpu11_info *s2mpu11)
{
	u8 val;
	int ret;

	/* OCP_WARN */
	if (pdata->ocp_warn_b2_en) {
		val = (pdata->ocp_warn_b2_en << S2MPU11_OCP_WARN_EN) |
		      (pdata->ocp_warn_b2_cnt << S2MPU11_OCP_WARN_CNT) |
		      (pdata->ocp_warn_b2_dvs_mask << S2MPU11_OCP_WARN_DVS_MASK);

		ret = s2mpu11_update_reg(s2mpu11->i2c,
					 S2MPU11_PMIC_REG_OCP_WARN_BUCK2,
					 val, S2MPU11_OCP_WARN_MASK);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to write OCP_WARN_B2 configuration\n",
				__func__);
			goto err;
		}

		pr_info("%s: OCP_WARN_B2[0x%x]\n", __func__, val);
	}

	pr_info("%s: Done\n", __func__);
	return 0;

err:
	pr_info("%s: Fail\n", __func__);
	return -1;
}

static int s2mpu11_set_on_sequence(struct s2mpu11_info *s2mpu11)
{
	int ret;
	/* set VDD_G3D on-seq. */
	ret = s2mpu11_update_reg(s2mpu11->i2c,
				 S2MPU11_PMIC_REG_ON_SEQ_SEL1, 0x00, 0xF0);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}

	/* set PWREN_MIF on-seq. */
	ret = s2mpu11_update_reg(s2mpu11->i2c,
				 S2MPU11_PMIC_REG_ON_SEQ_SEL1, 0x03, 0x0F);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}

	ret = s2mpu11_update_reg(s2mpu11->i2c,
				 S2MPU11_PMIC_REG_ON_SEQ_SEL2, 0x04, 0x0F);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}

	/* set PWREN_CP on-seq. */
	ret = s2mpu11_update_reg(s2mpu11->i2c,
				 S2MPU11_PMIC_REG_ON_SEQ_SEL3, 0x00, 0x0F);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}

	ret = s2mpu11_update_reg(s2mpu11->i2c,
				 S2MPU11_PMIC_REG_ON_SEQ_SEL2, 0x10, 0xF0);
	if (ret) {
		pr_err("%s: fail to update register\n", __func__);
		return -1;
	}

	return 0;
}

static int s2mpu11_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu11_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu11_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu11_info *s2mpu11;
	int i, ret;
	u8 val;

	pr_info("%s: s2mpu11 pmic driver Loading start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu11_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu11 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu11_info),
			       GFP_KERNEL);
	if (!s2mpu11)
		return -ENOMEM;

	s2mpu11->iodev = iodev;
	s2mpu11->i2c = iodev->pmic;

	mutex_init(&s2mpu11->lock);
	static_info = s2mpu11;

	platform_set_drvdata(pdev, s2mpu11);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu11;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu11->opmode[id] = regulators[id].enable_mask;

		s2mpu11->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mpu11->rdev[i])) {
			ret = PTR_ERR(s2mpu11->rdev[i]);
			dev_err(&pdev->dev,
				"regulator init failed for %d\n", i);
			s2mpu11->rdev[i] = NULL;
			goto err;
		}

	}

	s2mpu11->num_regulators = pdata->num_regulators;

	/* Enable WTSR */
	if (pdata->wtsr_en)
		s2mpu11_wtsr_enable(s2mpu11, pdata);

	s2mpu11_read_reg(s2mpu11->i2c, S2MPU11_PMIC_REG_OFFSRC, &val);
	pr_info("OFFSRC 0x%x\n", __func__, val);

	/* Warning setting */
	ret = s2mpu11_set_warn(pdev, pdata, s2mpu11);
	if (ret < 0)
		goto err;

	/* OI setting */
	s2mpu11_oi_function(s2mpu11);

	/* Regulator on-seq. setting */
	ret = s2mpu11_set_on_sequence(s2mpu11);
	if (ret < 0)
		goto err;

#ifdef CONFIG_DRV_SAMSUNG_PMIC
	/* create sysfs */
	create_s2mpu11_sysfs(s2mpu11);
#endif

	pr_info("%s: s2mpu11 pmic driver Loading end\n", __func__);

	return 0;
err:
	for (i = 0; i < S2MPU11_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu11->rdev[i]);

	return ret;
}

static int s2mpu11_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu11_info *s2mpu11 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPU11_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu11->rdev[i]);

#ifdef CONFIG_DRV_SAMSUNG_PMIC
	pmic_device_destroy(s2mpu11->dev->devt);
#endif
	return 0;
}

static void s2mpu11_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mpu11_info *s2mpu11 = platform_get_drvdata(pdev);

	/* disable WTSR */
	if (s2mpu11->wtsr_en)
		s2mpu11_wtsr_disable(s2mpu11);
}

static const struct platform_device_id s2mpu11_pmic_id[] = {
	{ "s2mpu11-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu11_pmic_id);

static struct platform_driver s2mpu11_pmic_driver = {
	.driver = {
		.name = "s2mpu11-regulator",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu11_pmic_probe,
	.remove = s2mpu11_pmic_remove,
	.shutdown = s2mpu11_pmic_shutdown,
	.id_table = s2mpu11_pmic_id,
};

static int __init s2mpu11_pmic_init(void)
{
	return platform_driver_register(&s2mpu11_pmic_driver);
}
subsys_initcall(s2mpu11_pmic_init);

static void __exit s2mpu11_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu11_pmic_driver);
}
module_exit(s2mpu11_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPU11 Regulator Driver");
MODULE_LICENSE("GPL");
