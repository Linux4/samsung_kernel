/*
 * s2mps22.c
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
#include <../drivers/pinctrl/samsung/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/s2mps22.h>
#include <linux/mfd/samsung/s2mps22-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#ifdef CONFIG_SEC_PM
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
#include <linux/sec_debug.h>
#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

static struct s2mps22_info *s2mps22_static_info;
static struct regulator_desc regulators[S2MPS22_REG_MAX];
int s2mps22_buck_ocp_cnt[S2MPS22_BUCK_CNT]; /* BUCK 1~4 OCP count */
int s2mps22_temp_cnt[S2MPS22_TEMP_NR]; /* 0 : 120 degree , 1 : 140 degree */
int s2mps22_buck_oi_cnt[S2MPS22_BUCK_OI_MAX]; /* BUCK 1~4 OI count */

struct s2mps22_info {
	bool g3d_en;
	u8 adc_en_val;
	int wtsr_en;
	int buck_ocp_irq[S2MPS22_BUCK_OCP_IRQ_NUM]; /* BUCK OCP IRQ */
	int buck_oi_irq[S2MPS22_BUCK_OI_MAX]; /* BUCK OI IRQ */
	int temp_irq[S2MPS22_TEMP_NR];	/* 0 : 120 degree, 1 : 140 degree */
	int num_regulators;
	unsigned int opmode[S2MPS22_REG_MAX];
	struct regulator_dev *rdev[S2MPS22_REG_MAX];
	struct s2mps22_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

static unsigned int s2mps22_of_map_mode(unsigned int val) {
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
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPS22_ENABLE_SHIFT;

	s2mps22->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);

	return s2mps22_update_reg(s2mps22->i2c, rdev->desc->enable_reg,
				  s2mps22->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps22_update_reg(s2mps22->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps22_read_reg(s2mps22->i2c,
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
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPS22_BUCK1:
		ramp_shift = 0;
		break;
	case S2MPS22_BUCK2:
		ramp_shift = 2;
		break;
	case S2MPS22_BUCK3:
		ramp_shift = 4;
		break;
	case S2MPS22_BUCK4:
		ramp_shift = 6;
		break;
	default:
		return -EINVAL;
	}

	return s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_BUCK_RAMP1,
				  ramp_value << ramp_shift,
				  ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps22_read_reg(s2mps22->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev), ret;
	char name[16];

	/* voltage information logging to snapshot feature */
	snprintf(name, sizeof(name), "LDO%d", (reg_id - S2MPS22_LDO1) + 1);
	ret = s2mps22_update_reg(s2mps22->i2c, rdev->desc->vsel_reg,
				 sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return ret;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps22_update_reg(s2mps22->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					   unsigned sel)
{
	struct s2mps22_info *s2mps22 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mps22_write_reg(s2mps22->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps22_update_reg(s2mps22->i2c, rdev->desc->apply_reg,
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

static struct regulator_ops s2mps22_ldo_ops = {
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

static struct regulator_ops s2mps22_buck_ops = {
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

#define _BUCK(macro)		S2MPS22_BUCK##macro
#define _buck_ops(num)		s2mps22_buck_ops##num
#define _LDO(macro)		S2MPS22_LDO##macro
#define _ldo_ops(num)		s2mps22_ldo_ops##num

#define _REG(ctrl)		S2MPS22_REG##ctrl
#define _TIME(macro)		S2MPS22_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS22_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS22_LDO_STEP##group
#define _BUCK_MIN(group)	S2MPS22_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS22_BUCK_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS22_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS22_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS22_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps22_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPS22_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS22_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS22_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps22_of_map_mode			\
}

static struct regulator_desc regulators[S2MPS22_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	/* LDO 1~7 */
	LDO_DESC("LDO1S", _LDO(1), &_ldo_ops(), 1,
		 _REG(_LDO1S_CTRL), _REG(_LDO1S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2S", _LDO(2), &_ldo_ops(), 1,
		 _REG(_LDO2S_CTRL), _REG(_LDO2S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3S", _LDO(3), &_ldo_ops(), 1,
		 _REG(_LDO3S_CTRL), _REG(_LDO3S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4S", _LDO(4), &_ldo_ops(), 2,
		 _REG(_LDO4S_CTRL), _REG(_LDO4S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5S", _LDO(5), &_ldo_ops(), 3,
		 _REG(_LDO5S_CTRL), _REG(_LDO5S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6S", _LDO(6), &_ldo_ops(), 2,
		 _REG(_LDO6S_CTRL), _REG(_LDO6S_CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO7S", _LDO(7), &_ldo_ops(), 2,
		 _REG(_LDO7S_CTRL), _REG(_LDO7S_CTRL), _TIME(_LDO)),*/
	/* BUCK 1~4 */
	BUCK_DESC("BUCK1S", _BUCK(1), &_buck_ops(), 1,
		 _REG(_BUCK1S_OUT), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2S", _BUCK(2), &_buck_ops(), 1,
		 _REG(_BUCK2S_OUT), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3S", _BUCK(3), &_buck_ops(), 1,
		 _REG(_BUCK3S_OUT), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4S", _BUCK(4), &_buck_ops(), 1,
		 _REG(_BUCK4S_OUT), _REG(_BUCK4S_CTRL), _TIME(_BUCK))
};

#ifdef CONFIG_OF
static int s2mps22_pmic_dt_parse_pdata(struct s2mps22_dev *iodev,
				       struct s2mps22_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps22_regulator_data *rdata;
	size_t i;
	int ret;
	u32 val;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
	/* adc_mode */
	pdata->adc_mode = 0;
	ret = of_property_read_u32(pmic_np, "adc_mode", &val);
	pdata->adc_mode = val;

	/* sync_mode */
	pdata->adc_sync_mode = 0;
	ret = of_property_read_u32(pmic_np, "adc_sync_mode", &val);
	pdata->adc_sync_mode = val;

	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		pr_info("%s: fail to read wtsr_en\n", __func__);
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
		rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
							     &regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mps22_pmic_dt_parse_pdata(struct s2mps22_pmic_dev *iodev,
				       struct s2mps22_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_EXYNOS_OCP
void get_s2mps22_i2c(struct i2c_client **i2c)
{
	*i2c = s2mps22_static_info->i2c;
}
#endif

static irqreturn_t s2mps22_buck_ocp_irq(int irq, void *data)
{
	struct s2mps22_info *s2mps22 = data;
	int i = 0;

	mutex_lock(&s2mps22->lock);

	for (i = 0; i < S2MPS22_BUCK_OCP_IRQ_NUM; i++) {
		if (s2mps22_static_info->buck_ocp_irq[i] == irq) {
			s2mps22_buck_ocp_cnt[i]++;
			pr_info("%s : BUCK[%d] OCP IRQ : %d, %d\n",
				__func__, i + 1, s2mps22_buck_ocp_cnt[i], irq);
			break;
		}
	}

#if defined(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_slave_ocp();
#endif

	mutex_unlock(&s2mps22->lock);
	return IRQ_HANDLED;
}

static irqreturn_t s2mps22_temp_irq(int irq, void *data)
{
	struct s2mps22_info *s2mps22 = data;

	mutex_lock(&s2mps22->lock);

	if (s2mps22_static_info->temp_irq[S2MPS22_TEMP_120] == irq) {
		s2mps22_temp_cnt[S2MPS22_TEMP_120]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, s2mps22_temp_cnt[S2MPS22_TEMP_120], irq);
	} else if (s2mps22_static_info->temp_irq[S2MPS22_TEMP_140] == irq) {
		s2mps22_temp_cnt[S2MPS22_TEMP_140]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, s2mps22_temp_cnt[S2MPS22_TEMP_140], irq);
	}

	mutex_unlock(&s2mps22->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mps22_buck_oi_irq(int irq, void *data)
{
	struct s2mps22_info *s2mps22 = data;
	int i = 0;

	mutex_lock(&s2mps22->lock);

	for (i = 1; i < S2MPS22_BUCK_OI_MAX; i++) {
		if (s2mps22->buck_oi_irq[i] == irq) {
			s2mps22_buck_oi_cnt[i]++;
			pr_info("%s: S2MPS22 BUCK[%d] OI IRQ: %d, irq = %d\n",
				__func__, i + 1, s2mps22_buck_oi_cnt[i], irq);
			break;
		}
	}

#if defined(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_slave_ocp();
#endif

	mutex_unlock(&s2mps22->lock);

	return IRQ_HANDLED;
}

void s2mps22_oi_function(struct s2mps22_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic;
	unsigned int i = 0;
	u8 val;

	/* BUCK 1~4 OI enable */
	s2mps22_update_reg(i2c, S2MPS22_REG_BUCK_OI_EN, 0x0E,
			   S2MPS22_BUCK_OI_EN_MASK);

	/* BUCK 1~4 OI power down disable */
	s2mps22_update_reg(i2c, S2MPS22_REG_BUCK_OI_PD_CTRL1, 0x00,
			   S2MPS22_BUCK_OI_PD_CTRL1_MASK);

	/* BUCK 1~4 OI detection time window control 300us */
	s2mps22_write_reg(i2c, S2MPS22_REG_BUCK_OI_CTRL1, 0xFF);

	/* BUCK 1~4 OI detection count 50 */
	s2mps22_write_reg(i2c, S2MPS22_REG_BUCK_OI_CTRL3, 0x00);

	pr_info("%s\n", __func__);
	for (i = S2MPS22_REG_BUCK_OI_EN; i <= S2MPS22_REG_BUCK_OI_CTRL3; i++) {
		s2mps22_read_reg(i2c, i, &val);
		pr_info("0x%x[0x%02hhx], ", i, val);
	}
	pr_info("\n");
}

static void s2mps22_set_oi_interrupt(struct platform_device *pdev,
				     struct s2mps22_info *s2mps22, int irq_base)
{
	int ret;
	int i = 0;

	for (i = 1; i < S2MPS22_BUCK_OI_MAX; i++) {
		s2mps22->buck_oi_irq[i] = irq_base + S2MPS22_IRQ_OI_B1_INT4 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps22->buck_oi_irq[i], NULL,
						s2mps22_buck_oi_irq, 0,
						"BUCK_OI_IRQ", s2mps22);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OI IRQ: %d: %d\n",
				i + 1, s2mps22->buck_oi_irq[i], ret);
		}
	}
}

static void s2mps22_wtsr_enable(struct s2mps22_info *s2mps22,
				struct s2mps22_platform_data *pdata)
{
	int ret;

	pr_info("%s: WTSR (%s)\n", __func__,
		pdata->wtsr_en ? "enable" : "disable");

	ret = s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_CTRL1,
				 S2MPS22_WTSREN_MASK, S2MPS22_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mps22->wtsr_en = pdata->wtsr_en;
}

static void s2mps22_wtsr_disable(struct s2mps22_info *s2mps22)
{
	int ret;

	pr_info("%s: disable WTSR\n", __func__);
	ret = s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_CTRL1,
				 0x00, S2MPS22_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}
#ifdef CONFIG_DRV_SAMSUNG_PMIC
static ssize_t s2mps22_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps22_info *s2mps22 = dev_get_drvdata(dev);
	int ret;
	u8 val, reg_addr;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = kstrtou8(buf, 0, &reg_addr);
	if (ret < 0)
		pr_info("%s: fail to transform i2c address\n", __func__);

	ret = s2mps22_read_reg(s2mps22->i2c, reg_addr, &val);
	if (ret < 0)
		pr_info("%s: fail to read i2c address\n", __func__);

	pr_info("%s: reg(0x%02hhx) data(0x%02hhx)\n", __func__, reg_addr, val);
	s2mps22->read_addr = reg_addr;
	s2mps22->read_val = val;

	return size;
}

static ssize_t s2mps22_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps22_info *s2mps22 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx: 0x%02hhx\n", s2mps22->read_addr,
		       s2mps22->read_val);
}

static ssize_t s2mps22_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps22_info *s2mps22 = dev_get_drvdata(dev);
	int ret;
	u8 reg = 0, data = 0;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx 0x%02hhx", &reg, &data);
	if (ret != 2) {
		pr_info("%s: input error\n", __func__);
		return size;
	}

	pr_info("%s: reg(0x%02hhx) data(0x%02hhx)\n", __func__, reg, data);

	ret = s2mps22_write_reg(s2mps22->i2c, reg, data);
	if (ret < 0)
		pr_info("%s: fail to write i2c addr/data\n", __func__);

	return size;
}

static ssize_t s2mps22_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mps22_write\n");
}
static DEVICE_ATTR(s2mps22_write, 0644, s2mps22_write_show, s2mps22_write_store);
static DEVICE_ATTR(s2mps22_read, 0644, s2mps22_read_show, s2mps22_read_store);

int create_s2mps22_sysfs(struct s2mps22_info *s2mps22)
{
	struct device *s2mps22_pmic = s2mps22->dev;
	int err = -ENODEV;

	pr_info("%s: slave pmic sysfs start\n", __func__);
	s2mps22->read_addr = 0;
	s2mps22->read_val = 0;

	s2mps22_pmic = pmic_device_create(s2mps22, "s2mps22");

	err = device_create_file(s2mps22_pmic, &dev_attr_s2mps22_write);
	if (err) {
		pr_err("s2mps22_sysfs: failed to create device file, %s\n",
			dev_attr_s2mps22_write.attr.name);
		goto err_sysfs;
	}

	err = device_create_file(s2mps22_pmic, &dev_attr_s2mps22_read);
	if (err) {
		pr_err("s2mps22_sysfs: failed to create device file, %s\n",
			dev_attr_s2mps22_read.attr.name);
		goto err_sysfs;
	}

	return 0;

err_sysfs:
	return err;
}
#endif
static int s2mps22_pmic_probe(struct platform_device *pdev)
{
	struct s2mps22_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps22_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps22_info *s2mps22;
	int irq_base, ret;
	int i = 0;

	if (iodev->dev->of_node) {
		ret = s2mps22_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps22 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps22_info),
			       GFP_KERNEL);
	if (!s2mps22)
		return -ENOMEM;

	irq_base = pdata->irq_base;
	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	s2mps22->iodev = iodev;
	s2mps22->i2c = iodev->pmic;

	mutex_init(&s2mps22->lock);

	s2mps22->g3d_en = pdata->g3d_en;
	s2mps22_static_info = s2mps22;

	platform_set_drvdata(pdev, s2mps22);

#ifdef CONFIG_SEC_PM
	/* Clear OFFSRC register */
	ret = s2mps22_write_reg(s2mps22->i2c, S2MPS22_REG_OFFSRC, 0);
	if (ret)
		dev_err(&pdev->dev, "failed to write OFFSRC\n");
#endif /* CONFIG_SEC_PM */

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps22;
		config.of_node = pdata->regulators[i].reg_node;
		s2mps22->opmode[id] = regulators[id].enable_mask;
		s2mps22->rdev[i] = devm_regulator_register(&pdev->dev,
							   &regulators[id], &config);
		if (IS_ERR(s2mps22->rdev[i])) {
			ret = PTR_ERR(s2mps22->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mps22->rdev[i] = NULL;
			goto err_s2mps22_data;
		}
	}

	s2mps22->num_regulators = pdata->num_regulators;
	/* OCP_IRQ */
	for (i = 0; i < S2MPS22_BUCK_OCP_IRQ_NUM; i++) {
		s2mps22->buck_ocp_irq[i] = irq_base +
					   S2MPS22_IRQ_OCP_B1_INT3 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps22->buck_ocp_irq[i], NULL,
						s2mps22_buck_ocp_irq, 0,
						"BUCK_OCP_IRQ", s2mps22);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mps22->buck_ocp_irq[i], ret);
		}
	}
	/* Temperature_IRQ */
	for (i = 0; i < S2MPS22_TEMP_NR; i++) {
		s2mps22->temp_irq[i] = irq_base + S2MPS22_IRQ_INT120C_INT1 + i;

		ret = devm_request_threaded_irq(&pdev->dev, s2mps22->temp_irq[i],
						NULL, s2mps22_temp_irq, 0,
						"TEMP_IRQ", s2mps22);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request over temperature[%d] IRQ: %d: %d\n",
				i, s2mps22->temp_irq[i], ret);
		}
	}

	s2mps22_oi_function(iodev);

	/* OI interrupt setting */
	s2mps22_set_oi_interrupt(pdev, s2mps22, irq_base);

	/* enable WTSR */
	if (pdata->wtsr_en)
		s2mps22_wtsr_enable(s2mps22, pdata);

#ifdef CONFIG_SEC_PM
	iodev->ap_sub_pmic_dev = sec_device_create(NULL, "ap_sub_pmic");
#endif /* CONFIG_SEC_PM */

	iodev->adc_mode = pdata->adc_mode;
	iodev->adc_sync_mode = pdata->adc_sync_mode;
	if (iodev->adc_mode > 0)
		s2mps22_powermeter_init(iodev);
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	/* create sysfs */
	create_s2mps22_sysfs(s2mps22);
	if (ret < 0)
		return ret;
#endif
	return 0;

err_s2mps22_data:
	mutex_destroy(&s2mps22->lock);
err_pdata:
	return ret;
}

static int s2mps22_pmic_remove(struct platform_device *pdev)
{
	struct s2mps22_info *s2mps22 = platform_get_drvdata(pdev);

	s2mps22_powermeter_deinit(s2mps22->iodev);
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	pmic_device_destroy(s2mps22->dev->devt);
#endif
#ifdef CONFIG_SEC_PM
	if (!IS_ERR_OR_NULL(s2mps22->iodev->ap_sub_pmic_dev))
		sec_device_destroy(s2mps22->iodev->ap_sub_pmic_dev->devt);
#endif /* CONFIG_SEC_PM */
	return 0;
}

static void s2mps22_pmic_WA(struct s2mps22_info *s2mps22)
{
	u8 reg1 = 0, reg2 = 0;

	s2mps22_read_reg(s2mps22->i2c, S2MPS22_REG_CTRL1, &reg1);

	s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_CTRL1, 0x80, 0x80);

	s2mps22_read_reg(s2mps22->i2c, S2MPS22_REG_CTRL1, &reg2);
	pr_info("%s: 0x%02hhx -> 0x%02hhx\n", __func__, reg1, reg2);
}

static void s2mps22_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mps22_info *s2mps22 = platform_get_drvdata(pdev);

	s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_ADC_CTRL3, 0, ADC_EN_MASK);
	pr_info("%s: ADC OFF\n", __func__);
	/* disable WTSR */
	if (s2mps22->wtsr_en)
		s2mps22_wtsr_disable(s2mps22);

	s2mps22_pmic_WA(s2mps22);

	if (system_state == SYSTEM_POWER_OFF) {
		u8 reg;

		s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_BUCK2S_CTRL, 0xC0, 0xC0);
		udelay(96);
		s2mps22_read_reg(s2mps22->i2c, S2MPS22_REG_BUCK2S_CTRL, &reg);
		pr_info("@@@@@%s: B2S_CTRL: 0x%x\n", __func__, reg);
	}
}

#if defined(CONFIG_PM)
static int s2mps22_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps22_info *s2mps22 = platform_get_drvdata(pdev);

	pr_info("%s adc_mode : %d\n", __func__, s2mps22->iodev->adc_mode);

	if (s2mps22->iodev->adc_mode > 0) {
		s2mps22_read_reg(s2mps22->i2c,
				 S2MPS22_REG_ADC_CTRL3, &s2mps22->adc_en_val);
		if (s2mps22->adc_en_val & 0x80)
			s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_ADC_CTRL3,
					   0, ADC_EN_MASK);
	}

	return 0;
}

static int s2mps22_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps22_info *s2mps22 = platform_get_drvdata(pdev);

	pr_info("%s adc_mode : %d\n", __func__, s2mps22->iodev->adc_mode);

	if (s2mps22->iodev->adc_mode > 0)
		s2mps22_update_reg(s2mps22->i2c, S2MPS22_REG_ADC_CTRL3,
				s2mps22->adc_en_val & 0x80, ADC_EN_MASK);
	return 0;
}
#else
#define s2mps22_pmic_suspend	NULL
#define s2mps22_pmic_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mps22_pmic_pm = {
	.suspend = s2mps22_pmic_suspend,
	.resume = s2mps22_pmic_resume,
};

static const struct platform_device_id s2mps22_pmic_id[] = {
	{ "s2mps22-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps22_pmic_id);

static struct platform_driver s2mps22_pmic_driver = {
	.driver = {
		.name = "s2mps22-regulator",
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm = &s2mps22_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mps22_pmic_probe,
	.remove = s2mps22_pmic_remove,
	.shutdown = s2mps22_pmic_shutdown,
	.id_table = s2mps22_pmic_id,
};

static int __init s2mps22_pmic_init(void)
{
	return platform_driver_register(&s2mps22_pmic_driver);
}
subsys_initcall(s2mps22_pmic_init);

static void __exit s2mps22_pmic_exit(void)
{
	platform_driver_unregister(&s2mps22_pmic_driver);
}
module_exit(s2mps22_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS22 Regulator Driver");
MODULE_LICENSE("GPL");
