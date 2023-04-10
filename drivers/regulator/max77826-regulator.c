/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/regmap.h>
#include <linux/regulator/max77826-regulator.h>
#include <linux/module.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

#define M2SH	__CONST_FFS

/* Slave Address */
#define MAX77826_I2C_ADDR	(0xC0>>1)

/* Register */
#define REG_INT_SRC		0x00
#define BIT_TOPSYS_INT		BIT (0)
#define BIT_REG_INT		BIT (1)
#define BIT_BB_INT		BIT (2)

#define REG_SYS_INT		0x01
#define BIT_TJCT_140C		BIT (0)
#define BIT_TJCT_120C		BIT (1)

#define REG_REG_INT1		0x02
#define BIT_LDO1_POKn		BIT (0)
#define BIT_LDO2_POKn		BIT (1)
#define BIT_LDO3_POKn		BIT (2)
#define BIT_LDO4_POKn		BIT (3)
#define BIT_LDO5_POKn		BIT (4)
#define BIT_LDO6_POKn		BIT (5)
#define BIT_LDO7_POKn		BIT (6)
#define BIT_LDO8_POKn		BIT (7)

#define REG_REG_INT2		0x03
#define BIT_LDO9_POKn		BIT (0)
#define BIT_LDO10_POKn		BIT (1)
#define BIT_LDO11_POKn		BIT (2)
#define BIT_LDO12_POKn		BIT (3)
#define BIT_LDO13_POKn		BIT (4)
#define BIT_LDO14_POKn		BIT (5)
#define BIT_LDO15_POKn		BIT (6)
#define BIT_B_POKn		BIT (7)

#define REG_BB_INT		0x04
#define BIT_BB_OCP		BIT (0)
#define BIT_BB_OVP		BIT (1)
#define BIT_BB_POKn		BIT (2)

#define REG_INT_SRC_M		0x05
#define REG_TOPSYS_INT_M	0x06
#define REG_REG_INT1_M		0x07
#define REG_REG_INT2_M		0x08
#define REG_BB_INT_M		0x09

#define REG_TOPSYS_STAT	0x0A
#define REG_REG_STAT1		0x0B
#define REG_REG_STAT2		0x0C
#define REG_BB_STAT		0x0D

#define REG_LDO_OPMD1		0x10
#define BIT_L1_LPM		BIT (0)
#define BIT_L1_EN		BIT (1)
#define BIT_L2_LPM		BIT (2)
#define BIT_L2_EN		BIT (3)
#define BIT_L3_LPM		BIT (4)
#define BIT_L3_EN		BIT (5)
#define BIT_L4_LPM		BIT (6)
#define BIT_L4_EN		BIT (7)

#define REG_LDO_OPMD2		0x11
#define BIT_L5_LPM		BIT (0)
#define BIT_L5_EN		BIT (1)
#define BIT_L6_LPM		BIT (2)
#define BIT_L6_EN		BIT (3)
#define BIT_L7_LPM		BIT (4)
#define BIT_L7_EN		BIT (5)
#define BIT_L8_LPM		BIT (6)
#define BIT_L8_EN		BIT (7)

#define REG_LDO_OPMD3		0x12
#define BIT_L9_LPM		BIT (0)
#define BIT_L9_EN		BIT (1)
#define BIT_L10_LPM		BIT (2)
#define BIT_L10_EN		BIT (3)
#define BIT_L11_LPM		BIT (4)
#define BIT_L11_EN		BIT (5)
#define BIT_L12_LPM		BIT (6)
#define BIT_L12_EN		BIT (7)

#define REG_LDO_OPMD4		0x13
#define BIT_L13_LPM		BIT (0)
#define BIT_L13_EN		BIT (1)
#define BIT_L14_LPM		BIT (2)
#define BIT_L14_EN		BIT (3)
#define BIT_L15_LPM		BIT (4)
#define BIT_L15_EN		BIT (5)

#define REG_B_BB_OPMD		0x14
#define BIT_B_LPM		BIT (0)
#define BIT_B_EN		BIT (1)
#define BIT_BB_EN		BIT (3)

#define REG_LDO1_CFG		0x20
#define REG_LDO2_CFG		0x21
#define REG_LDO3_CFG		0x22
#define REG_LDO4_CFG		0x23
#define REG_LDO5_CFG		0x24
#define REG_LDO6_CFG		0x25
#define REG_LDO7_CFG		0x26
#define REG_LDO8_CFG		0x27
#define REG_LDO9_CFG		0x28
#define REG_LDO10_CFG		0x29
#define REG_LDO11_CFG		0x2A
#define REG_LDO12_CFG		0x2B
#define REG_LDO13_CFG		0x2C
#define REG_LDO14_CFG		0x2D
#define REG_LDO15_CFG		0x2E
#define BIT_LX_VOUT		BITS(6, 0)
#define BIT_LX_AD		BIT (7)

#define REG_LDOX_CFG(X)	(REG_LDO1_CFG + X - 1)

#define REG_BUCK_CFG		0x30
#define BIT_B_FSRADE		BIT (0)
#define BIT_B_FPWM		BIT (2)
#define BIT_B_AD		BIT (3)
#define BIT_B_RAMP		BITS(7, 6)

#define REG_BUCK_VOUT		0x31
#define BIT_B_VOUT		BITS(7, 0)

#define REG_BB_CFG		0x32
#define BIT_BB_FPWM		BIT (1)
#define BIT_BB_HSKIP		BIT (2)
#define BIT_BB_AD		BIT (3)
#define BIT_BB_OVP_TH		BITS(5, 4)

#define REG_BB_VOUT		0x33
#define BIT_BB_VOUT		BITS(6, 0)

#define REG_BUCK_SS_FREQ	0x40
#define BIT_B_FREQ		BITS(2, 0)
#define BIT_B_SS		BIT (4)

#define REG_UVLO_FALL		0x41
#define BIT_UVLO_F		BITS(1, 0)

#define REG_DEVICE_ID		0xCF
#define BIT_CHIP_REV		BITS(2, 0)
#define BIT_VERSION		BITS(6, 3)

/* Undefined register address value */
#define REG_RESERVED 		0xFF

/* define H/W default value */
#define HW_DEFAULT_VALUE	0xFF

/* voltage range and step */
#define NMOS_MINUV		600000		/* 0.6V */
#define NMOS_MAXUV		2187500		/* 2.1875V */
#define NMOS_STEP		12500		/* 12.5mV */

#define PMOS_MINUV		800000		/* 0.8V */
#define PMOS_MAXUV		3975000		/* 3.975V */
#define PMOS_STEP		25000		/* 25mV */

#define BUCK_MINUV		500000		/* 0.5V */
#define BUCK_MAXUV		1800000		/* 1.8V */
#define BUCK_MAXVSEL		0xD0
#define BUCK_STEP		6250		/* 6.25mV */

#define BB_MINUV		2600000		/* 2.6V */
#define BB_MAXUV		4187500		/* 4.1875V */
#define BB_STEP		12500		/* 12.5mV */

/* LDO mode
 * X is max77826_reg_id.
 * LDO1(MAX77826_LDO1) is 1 and LDO15(MAX77826_LDO15) is 15
 */
#define REG_LX_OPMODE(X)	(X < MAX77826_LDO5 ? REG_LDO_OPMD1 : 	\
				X < MAX77826_LDO9 ? REG_LDO_OPMD2 :	\
				X < MAX77826_LDO13 ? REG_LDO_OPMD3 :	\
				X < MAX77826_BUCK ? REG_LDO_OPMD4 : REG_RESERVED)

#define BIT_LX_OPMODE(X)	((X%4) == 1 ? BIT(0) :	\
				(X%4) == 2 ? BIT(2) :	\
				(X%4) == 3 ? BIT(4) : BIT(6))

#define REG_LX_ENABLE(X)	(X < MAX77826_LDO5 ? REG_LDO_OPMD1 : 	\
				X < MAX77826_LDO9 ? REG_LDO_OPMD2 :	\
				X < MAX77826_LDO13 ? REG_LDO_OPMD3 :	\
				X < MAX77826_BUCK ? REG_LDO_OPMD4 : REG_RESERVED)

/* LDO Enable
 * X is LDOx number.
 * LDO1(MAX77826_LDO1) is 1 and LDO15(MAX77826_LDO15) is 15
 */
#define BIT_LX_ENABLE(X)	((X%4) == 1 ? BIT(1) :	\
				(X%4) == 2 ? BIT(3) :	\
				(X%4) == 3 ? BIT(5) : BIT(7))

extern unsigned int system_rev;

static const struct regmap_config max77826_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 8,
    .cache_type = REGCACHE_NONE,
};

struct max77826_reg {
	struct device *dev;
	struct regulator_dev **rdev;
	int num_regulators;
	struct regmap *regmap;
};

static int max77826_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max77826_reg *reg_data = rdev_get_drvdata(rdev);
	int rid = rdev_get_id(rdev);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* forced pwm mode */
		if (rid == MAX77826_BUCK)		/* BUCK */
			regmap_update_bits(reg_data->regmap,
				REG_BUCK_CFG, BIT_B_FPWM, BIT_B_FPWM);
		else if (rid == MAX77826_BB)	/* BUCK-BOOST */
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_FPWM, BIT_BB_FPWM);
		break;
	case REGULATOR_MODE_NORMAL:
		/* Normal Mode */
		if (rid < MAX77826_BUCK)		/* LDO */
			regmap_update_bits(reg_data->regmap,
			REG_LX_OPMODE(rid), BIT_LX_OPMODE(rid), 0);
		else if (rid == MAX77826_BUCK) {	/* BUCK */
			regmap_update_bits(reg_data->regmap,
				REG_B_BB_OPMD, BIT_B_LPM, 0);
			regmap_update_bits(reg_data->regmap,
				REG_BUCK_CFG, BIT_B_FPWM, 0);
		} else if (rid == MAX77826_BB) {	/* BUCK-BOOST */
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_FPWM, 0);
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_HSKIP, 0);
		}
		break;
	case REGULATOR_MODE_IDLE:
		/* Low Power Mode */
		if (rid < MAX77826_BUCK)		/* LDO */
			regmap_update_bits(reg_data->regmap,
						REG_LX_OPMODE(rid),
						BIT_LX_OPMODE(rid),
						BIT_LX_OPMODE(rid));
		else if (rid == MAX77826_BUCK) {	/* BUCK */
			regmap_update_bits(reg_data->regmap,
				REG_B_BB_OPMD, BIT_B_LPM, BIT_B_LPM);
			regmap_update_bits(reg_data->regmap,
				REG_BUCK_CFG, BIT_B_FPWM, 0);
		} else if (rid == MAX77826_BB) {	/* BUCK-BOOST */
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_FPWM, 0);
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_HSKIP, 0);
		}
		break;
	case REGULATOR_MODE_STANDBY:
		/* LDO Low Power mode */
		if (rid < MAX77826_BUCK)		/* LDO */
			regmap_update_bits(reg_data->regmap,
						REG_LX_OPMODE(rid),
						BIT_LX_OPMODE(rid),
						BIT_LX_OPMODE(rid));
		else if (rid == MAX77826_BUCK) {	/* BUCK */
			regmap_update_bits(reg_data->regmap,
				REG_B_BB_OPMD, BIT_B_LPM, BIT_B_LPM);
			regmap_update_bits(reg_data->regmap,
				REG_BUCK_CFG, BIT_B_FPWM, 0);
		} else if (rid == MAX77826_BB) {	/* BUCK-BOOST */
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_FPWM, 0);
			regmap_update_bits(reg_data->regmap,
				REG_BB_CFG, BIT_BB_HSKIP, BIT_BB_HSKIP);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int max77826_get_mode(struct regulator_dev *rdev)
{
	struct max77826_reg *reg_data = rdev_get_drvdata(rdev);
	int rid = rdev_get_id(rdev);
	unsigned int reg;
	unsigned int val;
	int ret;

	unsigned int mode = REGULATOR_MODE_NORMAL;

	switch (rid) {
	case MAX77826_LDO1 ... MAX77826_LDO15:
		reg = REG_LX_OPMODE(rid);
		ret = regmap_read(reg_data->regmap, reg, &val);
		if (ret != 0)
			return -EINVAL;
		mode = (val & BIT_LX_OPMODE(rid)) ?
			REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
		break;
	case MAX77826_BUCK:
		reg = REG_BUCK_CFG;
		ret = regmap_read(reg_data->regmap, reg, &val);
		if (ret != 0)
			return -EINVAL;
		if (val & BIT_B_FPWM)
			mode = REGULATOR_MODE_FAST;
		else {
			reg = REG_B_BB_OPMD;
			ret = regmap_read(reg_data->regmap, reg, &val);
			if (ret != 0)
				return -EINVAL;
			mode = (val & BIT_B_LPM) ?
			REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
		}
		break;
	case MAX77826_BB:
		reg = REG_BB_CFG;
		ret = regmap_read(reg_data->regmap, reg, &val);
		if (ret != 0)
			return -EINVAL;
		if (val & BIT_BB_FPWM)
			mode = REGULATOR_MODE_FAST;
		else
			mode = (val & BIT_BB_HSKIP) ?
			REGULATOR_MODE_STANDBY : REGULATOR_MODE_NORMAL;
		break;
	default:
		return -EINVAL;
	}

	return mode;
}

static struct regulator_ops max77826_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_mode		= max77826_set_mode,
	.get_mode		= max77826_get_mode,
};

#define REGULATOR_DESC_NLDO(num) {				\
	.name		= "max77826-ldo"#num,			\
	.id		= MAX77826_LDO##num,			\
	.ops		= &max77826_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.n_voltages	= BIT_LX_VOUT + 1,			\
	.min_uV 	= NMOS_MINUV,				\
	.uV_step 	= NMOS_STEP,				\
	.vsel_reg	= REG_LDOX_CFG(num),			\
	.vsel_mask	= BIT_LX_VOUT,				\
	.enable_reg	= REG_LX_ENABLE(num),			\
	.enable_mask	= BIT_LX_ENABLE(num),			\
}

#define REGULATOR_DESC_PLDO(num) {				\
	.name		= "max77826-ldo"#num,			\
	.id		= MAX77826_LDO##num,			\
	.ops		= &max77826_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.n_voltages	= BIT_LX_VOUT + 1,			\
	.min_uV 	= PMOS_MINUV,				\
	.uV_step 	= PMOS_STEP,				\
	.vsel_reg	= REG_LDOX_CFG(num),			\
	.vsel_mask	= BIT_LX_VOUT,				\
	.enable_reg	= REG_LX_ENABLE(num),			\
	.enable_mask	= BIT_LX_ENABLE(num),			\
}

#define REGULATOR_DESC_BUCK() {					\
	.name		= "max77826-buck",			\
	.id		= MAX77826_BUCK,			\
	.ops		= &max77826_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.n_voltages	= BUCK_MAXVSEL + 1,			\
	.min_uV 	= BUCK_MINUV,				\
	.uV_step 	= BUCK_STEP,				\
	.vsel_reg	= REG_BUCK_VOUT,			\
	.vsel_mask	= BIT_B_VOUT,				\
	.enable_reg	= REG_B_BB_OPMD,			\
	.enable_mask	= BIT_B_EN,				\
}

#define REGULATOR_DESC_BB() {					\
	.name		= "max77826-bb",			\
	.id		= MAX77826_BB,				\
	.ops		= &max77826_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.n_voltages	= BIT_BB_VOUT + 1,			\
	.min_uV 	= BB_MINUV,				\
	.uV_step 	= BB_STEP,				\
	.vsel_reg	= REG_BB_VOUT,				\
	.vsel_mask	= BIT_BB_VOUT,				\
	.enable_reg	= REG_B_BB_OPMD,			\
	.enable_mask	= BIT_BB_EN,				\
}

const static struct regulator_desc max77826_reg_desc[] =
{
	REGULATOR_DESC_NLDO(1),
	REGULATOR_DESC_NLDO(2),
	REGULATOR_DESC_NLDO(3),
	REGULATOR_DESC_PLDO(4),
	REGULATOR_DESC_PLDO(5),
	REGULATOR_DESC_PLDO(6),
	REGULATOR_DESC_PLDO(7),
	REGULATOR_DESC_PLDO(8),
	REGULATOR_DESC_PLDO(9),
	REGULATOR_DESC_PLDO(10),
	REGULATOR_DESC_PLDO(11),
	REGULATOR_DESC_PLDO(12),
	REGULATOR_DESC_PLDO(13),
	REGULATOR_DESC_PLDO(14),
	REGULATOR_DESC_PLDO(15),
	REGULATOR_DESC_BUCK(),
	REGULATOR_DESC_BB(),
};

static int max77826_reg_set_active_discharge(struct max77826_reg *max77826_reg,
		struct max77826_regulator_platform_data *pdata, int reg_id)
{
	int rc, reg, val;

	switch (reg_id) {
	case MAX77826_LDO1 ... MAX77826_LDO15:
		val = pdata->regulators[reg_id-1].active_discharge_enable
						? BIT_LX_AD : 0;
		reg = REG_LDOX_CFG(reg_id);
		rc = regmap_update_bits(max77826_reg->regmap, reg,
						BIT_LX_AD, val);
		break;
	case MAX77826_BUCK:
		val = pdata->regulators[reg_id-1].active_discharge_enable
						? BIT_B_AD : 0;
		reg = REG_BUCK_CFG;
		rc = regmap_update_bits(max77826_reg->regmap, reg,
						BIT_B_AD, val);
		break;
	case MAX77826_BB:
		val = pdata->regulators[reg_id-1].active_discharge_enable
						? BIT_BB_AD : 0;
		reg = REG_BB_CFG;
		rc = regmap_update_bits(max77826_reg->regmap, reg,
						BIT_BB_AD, val);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int max77826_reg_hw_init(struct max77826_reg *max77826_reg,
				struct max77826_regulator_platform_data *pdata)
{
	int rc, reg, val;

	/* buck configuration */
	reg = REG_BUCK_CFG;
	val = pdata->buck_ramp_rate<<M2SH(BIT_B_RAMP) |
		pdata->buck_fpwm<<M2SH(BIT_B_FPWM) |
		pdata->buck_fsrade<<M2SH(BIT_B_FSRADE);
	rc = regmap_update_bits(max77826_reg->regmap, reg,
			BIT_B_RAMP | BIT_B_FPWM | BIT_B_FSRADE, val);
	if (rc != 0)
		return rc;

	reg = REG_BUCK_SS_FREQ;
	val = pdata->buck_ss<<M2SH(BIT_B_SS) |
		pdata->buck_freq<<M2SH(BIT_B_FREQ);
	rc = regmap_write(max77826_reg->regmap, reg, val);
	if (rc != 0)
		return rc;

	/* buck boost configuration */
	reg = REG_BB_CFG;
	val = pdata->bb_ovp_th<<M2SH(BIT_BB_OVP_TH) |
		pdata->bb_hskip<<M2SH(BIT_BB_HSKIP) |
		pdata->bb_fpwm<<M2SH(BIT_BB_FPWM);
	rc = regmap_update_bits(max77826_reg->regmap, reg,
			BIT_BB_OVP_TH | BIT_BB_HSKIP | BIT_BB_FPWM, val);
	if (rc != 0)
		return rc;

	/* uvlo falling threshold */
	reg = REG_UVLO_FALL;
	val = pdata->uvlo_fall_threshold<<M2SH(BIT_UVLO_F);
	rc = regmap_write(max77826_reg->regmap, reg, val);

	return rc;
}


static struct max77826_regulator_platform_data
			*max77826_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->of_node; 
	struct device_node *regulators_np, *reg_np;
	struct max77826_regulator_platform_data *pdata;
	struct max77826_regulator_data *rdata;
	int i;
	int ret;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (unlikely(regulators_np == NULL))
	{
		dev_err(dev, "regulators node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = of_get_child_count(regulators_np);

	rdata = devm_kzalloc(dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(dev, "could not allocate memory for regulator data\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(max77826_reg_desc); i++)
			if (!of_node_cmp(reg_np->name, max77826_reg_desc[i].name))
				break;

		if (i == ARRAY_SIZE(max77826_reg_desc)) {
			dev_warn(dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		rdata->initdata = of_get_regulator_init_data(dev, reg_np);
		rdata->of_node = reg_np;
		ret = of_property_read_u32(reg_np, "active-discharge-enable",
					&rdata->active_discharge_enable);
		if (ret != 0)
			rdata->active_discharge_enable = 1;
		rdata++;
	}
	of_node_put(regulators_np);

	/* 12.5mV/us */
	ret = of_property_read_u32(nproot, "buck-ramp-rate",
					&pdata->buck_ramp_rate);
	if (ret != 0)
		pdata->buck_ramp_rate = 0;

	/* turn off FPWM */
	ret = of_property_read_u32(nproot, "buck-fpwm",
					&pdata->buck_fpwm);
	if (ret != 0)
		pdata->buck_fpwm = 0;

	/* enable Active Discharge */
	ret = of_property_read_u32(nproot, "buck-fsrade", &pdata->buck_fsrade);
	if (ret != 0)
		pdata->buck_fsrade = 1;

	/* 14mV/us */
	ret = of_property_read_u32(nproot, "buck-ss", &pdata->buck_ss);
	if (ret != 0)
		pdata->buck_ss = 0;

	/* 2.0MHz */
	ret = of_property_read_u32(nproot, "buck-freq", &pdata->buck_freq);
	if (ret != 0)
		pdata->buck_freq = 4;

	/* 120% ov Vout */
	ret = of_property_read_u32(nproot, "bb-ovp-th", &pdata->bb_ovp_th);
	if (ret != 0)
		pdata->bb_ovp_th = 3;

	/* Disable HSKIP */
	ret = of_property_read_u32(nproot, "bb-hskip", &pdata->bb_hskip);
	if (ret != 0)
		pdata->bb_hskip = 0;

	/* SKIP MODE */
	ret = of_property_read_u32(nproot, "bb-fpwm", &pdata->bb_fpwm);
	if (ret != 0)
		pdata->bb_fpwm = 0;

	ret = of_property_read_u32(nproot, "uvlo-fall-threshold",
						&pdata->uvlo_fall_threshold);
	if (ret != 0)
		pdata->uvlo_fall_threshold = 1;	/* 2.05V */

	return pdata;
}

static void max77826_destroy (struct max77826_reg *me)
{
	struct device *dev = me->dev;

	if (likely(me->regmap))
		regmap_exit(me->regmap);

	devm_kfree(dev, me);
}

static int max77826_regulator_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77826_regulator_platform_data *pdata;
	struct max77826_reg *max77826_reg;

	struct regulator_config config;
	int rc, i;
	unsigned int val;

	pr_info("<%s> probe start\n", client->name);

	max77826_reg = devm_kzalloc(dev, sizeof(struct max77826_reg), GFP_KERNEL);
	if (unlikely(!max77826_reg))
		return -ENOMEM;

	i2c_set_clientdata(client, max77826_reg);

	pdata = max77826_reg_parse_dt(dev);
	if (IS_ERR(pdata)) {
		rc = PTR_ERR(pdata);
		goto abort;
	}

	max77826_reg->regmap = devm_regmap_init_i2c(client,
						&max77826_regmap_config);
	if (unlikely(IS_ERR(max77826_reg->regmap))) {
		rc = PTR_ERR(max77826_reg->regmap);
		max77826_reg->regmap = NULL;
		pr_err("<%s> failed to initialize i2c regmap pmic [%d]\n",
							client->name, rc);
		goto abort;
	}

	rc = regmap_read(max77826_reg->regmap, REG_DEVICE_ID, &val);
	if (rc < 0 && !(val & BIT_CHIP_REV))
	{
		pr_err("<%s> Invalid DEVICE_ID=0x%02x [%d]. skip probe...\n",client->name, val, rc);
		return -ENODEV;
	}
	pr_info("<%s> DEVICE_ID=0x%02x [%d]\n",client->name, val, rc);

	max77826_reg->num_regulators = pdata->num_regulators;
	if (pdata->num_regulators > MAX77826_REGULATORS + 1) {
		pr_err("<%s> There're too much regulators(%d) for max77826.\n",
					__func__, pdata->num_regulators);
		rc = -EPERM;
		goto abort;
	}

	max77826_reg->rdev = devm_kzalloc(dev,
			sizeof(struct regulator_dev *) * pdata->num_regulators,
			GFP_KERNEL);
	if (!max77826_reg->rdev) {
		pr_err("<%s> Can't allocate memory for regulator dev!\n",
								__func__);
		rc = -ENOMEM;
		goto abort;
	}

	max77826_reg->dev = &client->dev;
	config.dev = &client->dev;
	config.regmap = max77826_reg->regmap;
	config.driver_data = max77826_reg;

	for (i = 0; i < pdata->num_regulators; i++) {
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].of_node;

		max77826_reg->rdev[i] = regulator_register(&max77826_reg_desc[i],
								&config);

		if (IS_ERR(max77826_reg->rdev[i])) {
			rc = PTR_ERR(max77826_reg->rdev[i]);
			pr_err("<%s> regulator init failed for %d\n",
							client->name, i);
			max77826_reg->rdev[i] = NULL;
			goto abort;
		}

		rc = max77826_reg_set_active_discharge(max77826_reg, pdata,
						max77826_reg_desc[i].id);
		if (IS_ERR_VALUE(rc))
			goto abort;
	}

	/* initialize platform data */
	rc = max77826_reg_hw_init(max77826_reg, pdata);
	if (IS_ERR_VALUE(rc))
		goto abort;

	pr_info("<%s> probe end\n", client->name);
	return 0;

abort:
	i2c_set_clientdata(client, NULL);
	max77826_destroy(max77826_reg);
	pr_info("<%s> probe failed!\n", client->name);
	return rc;
}


static int max77826_regulator_remove(struct i2c_client *client)
{
	struct max77826_reg *max77826_reg = i2c_get_clientdata(client);
	int i;
	for (i = 0; i < max77826_reg->num_regulators; i++)
		regulator_unregister(max77826_reg->rdev[i]);

	i2c_set_clientdata(client, NULL);
	max77826_destroy(max77826_reg);

	return 0;
}

static struct of_device_id max77826_of_id[] = {
    { .compatible = "maxim,max77826"      },
    { },
};
MODULE_DEVICE_TABLE(of, max77826_of_id);

static const struct i2c_device_id max77826_i2c_id[] = {
    { "max77826", 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, max77826_i2c_id);

static struct i2c_driver max77826_i2c_driver = {
    .driver.name            = "max77826",
    .driver.owner           = THIS_MODULE,
    .driver.of_match_table  = max77826_of_id,
    .id_table               = max77826_i2c_id,
    .probe                  = max77826_regulator_probe,
    .remove                 = max77826_regulator_remove,
};

static __init int max77826_init (void)
{
	int rc = -ENODEV;

	rc = i2c_add_driver(&max77826_i2c_driver);
	if (rc != 0)
		pr_err("Failed to register I2C driver: %d\n", rc);

	return rc;
}
subsys_initcall(max77826_init);

static __exit void max77826_exit (void)
{
	i2c_del_driver(&max77826_i2c_driver);
}
module_exit(max77826_exit);

MODULE_ALIAS("platform:max77826-regulator");
MODULE_DESCRIPTION("Regulator driver for MAX77826");
MODULE_AUTHOR("Tai Eup<clark.kim@maximintegrated.com>");
MODULE_AUTHOR("Kyunghan,Baek<wildtaz.baek@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2");

