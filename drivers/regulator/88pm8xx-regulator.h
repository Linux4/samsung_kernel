/*
 * 88pm800-regulator.h - Regulator definitions for Marvell PMIC
 * Copyright (C) 2014  Marvell Semiconductor Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __88PM8XX_REGULATOR_H__
#define __88PM8XX_REGULATOR_H__

#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define MAX_SLEEP_CURRENT	5000 /* uA */
/* LDO1 with DVC[0..3] */
#define PM800_LDO1_VOUT		(0x08) /* VOUT1 */
#define PM800_LDO1_VOUT_2	(0x09)
#define PM800_LDO1_VOUT_3	(0x0A)
#define PM800_LDO2_VOUT		(0x0B)
#define PM800_LDO3_VOUT		(0x0C)
#define PM800_LDO4_VOUT		(0x0D)
#define PM800_LDO5_VOUT		(0x0E)
#define PM800_LDO6_VOUT		(0x0F)
#define PM800_LDO7_VOUT		(0x10)
#define PM800_LDO8_VOUT		(0x11)
#define PM800_LDO9_VOUT		(0x12)
#define PM800_LDO10_VOUT	(0x13)
#define PM800_LDO11_VOUT	(0x14)
#define PM800_LDO12_VOUT	(0x15)
#define PM800_LDO13_VOUT	(0x16)
#define PM800_LDO14_VOUT	(0x17)
#define PM800_LDO15_VOUT	(0x18)
#define PM800_LDO16_VOUT	(0x19)
#define PM800_LDO17_VOUT	(0x1A)
#define PM800_LDO18_VOUT	(0x1B)
#define PM800_LDO19_VOUT	(0x1C)
#define PM800_VOUTSW_VOUT	(0xFF) /* fake register */

/*88ppm86x register*/
#define PM800_LDO20_VOUT	(0x1D)

/* BUCK1 with DVC[0..3] */
#define PM800_BUCK1		(0x3C)
#define PM800_BUCK1_1		(0x3D)
#define PM800_BUCK1_2		(0x3E)
#define PM800_BUCK1_3		(0x3F)
#define PM800_BUCK2		(0x40)
#define PM800_BUCK3		(0x41)
#define PM800_BUCK3		(0x41)
#define PM800_BUCK4		(0x42)
#define PM800_BUCK4_1		(0x43)
#define PM800_BUCK4_2		(0x44)
#define PM800_BUCK4_3		(0x45)
#define PM800_BUCK5		(0x46)

#define PM800_BUCK_ENA		(0x50)
#define PM800_LDO_ENA1_1	(0x51)
#define PM800_LDO_ENA1_2	(0x52)
#define PM800_LDO_ENA1_3	(0x53)

#define PM800_LDO_ENA2_1	(0x56)
#define PM800_LDO_ENA2_2	(0x57)
#define PM800_LDO_ENA2_3	(0x58)

#define PM800_BUCK1_MISC1	(0x78)
#define PM800_BUCK3_MISC1	(0x7E)
#define PM800_BUCK4_MISC1	(0x81)
#define PM800_BUCK5_MISC1	(0x84)

struct pm800_regulator_info {
	struct regulator_desc desc;
	struct pm80x_chip *chip;
	int max_ua;
	unsigned int sleep_enable_bit;
	unsigned int sleep_enable_reg;
};

struct pm800_regulators {
	struct regulator_dev *regulators[PMIC_ID_RG_MAX];
	struct pm80x_chip *chip;
	struct regmap *map;
};

/*
 * vreg - the buck regs string.
 * ereg - the string for the enable register.
 * ebit - the bit number in the enable register.
 * amax - the current
 * slp_bit - sleep control bits
 * slp_reg - sleep control register
 * Buck has 2 kinds of voltage steps. It is easy to find voltage by ranges,
 * not the constant voltage table.
 * n_volt - Number of available selectors
 */
#define PM800_BUCK(vreg, ereg, ebit, amax, slp_bit, slp_reg, volt_ranges, n_volt)		\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm800_volt_range_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM800_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages		= n_volt,			\
		.linear_ranges		= volt_ranges,			\
		.n_linear_ranges	= ARRAY_SIZE(volt_ranges),	\
		.vsel_reg		= PM800_##vreg,			\
		.vsel_mask		= 0x7f,				\
		.enable_reg		= PM800_##ereg,			\
		.enable_mask		= 1 << (ebit),			\
	},								\
	.max_ua		= (amax),					\
	.sleep_enable_bit = slp_bit,					\
	.sleep_enable_reg = PM800_##slp_reg,				\
}

/*
 * vreg - the LDO regs string
 * ereg -  the string for the enable register.
 * ebit - the bit number in the enable register.
 * amax - the current
 * slp_bit - sleep control bits
 * slp_reg - sleep control register
 * volt_table - the LDO voltage table
 * For all the LDOes, there are too many ranges. Using volt_table will be
 * simpler and faster.
 */
#define PM800_LDO(vreg, ereg, ebit, amax, slp_bit, slp_reg, ldo_volt_table)		\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm800_volt_table_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM800_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),		\
		.vsel_reg	= PM800_##vreg##_VOUT,			\
		.vsel_mask	= 0xf,					\
		.enable_reg	= PM800_##ereg,				\
		.enable_mask	= 1 << (ebit),				\
		.volt_table	= ldo_volt_table,			\
	},								\
	.max_ua		= (amax),					\
	.sleep_enable_bit = slp_bit,					\
	.sleep_enable_reg = PM800_##slp_reg,				\
}

#define PM800_REGULATOR_OF_MATCH(id)					\
	{								\
		.name = "88PM800-" #id,					\
		.driver_data = &pm800_regulator_info[PM800_ID_##id],	\
	}

#define PM822_REGULATOR_OF_MATCH(id)					\
	{								\
		.name = "88PM800-" #id,					\
		.driver_data = &pm822_regulator_info[PM800_ID_##id],	\
	}

#define PM86X_REGULATOR_OF_MATCH(id)					\
	{								\
		.name = "88PM800-" #id,					\
		.driver_data = &pm86x_regulator_info[PM800_ID_##id],	\
	}

extern const struct regulator_linear_range buck_volt_range1[];
extern const struct regulator_linear_range buck_volt_range2[];
extern const struct regulator_linear_range buck_volt_range3[];

extern const unsigned int ldo_volt_table1[];
extern const unsigned int ldo_volt_table2[];
extern const unsigned int ldo_volt_table3[];
extern const unsigned int ldo_volt_table4[];
extern const unsigned int ldo_volt_table5[];
extern const unsigned int ldo_volt_table6[];
extern const unsigned int ldo_volt_table7[];
extern const unsigned int ldo_volt_table8[];
extern const unsigned int ldo_volt_table9[];
extern const unsigned int voutsw_table[];

/* for buck */
extern struct regulator_ops pm800_volt_range_ops;
/* for ldo */
extern struct regulator_ops pm800_volt_table_ops;

extern struct pm800_regulator_info pm800_regulator_info[];
extern struct pm800_regulator_info pm822_regulator_info[];
extern struct pm800_regulator_info pm86x_regulator_info[];

extern struct of_regulator_match pm800_regulator_matches[];
extern struct of_regulator_match pm822_regulator_matches[];
extern struct of_regulator_match pm86x_regulator_matches[];
#endif
