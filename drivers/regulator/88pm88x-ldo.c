/*
 * LDO driver for Marvell 88PM88X
 *
 * Copyright (C) 2014 Marvell International Ltd.
 * Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm880.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>

/* max current in sleep */
#define MAX_SLEEP_CURRENT	5000

/* ------------- 88pm886 ldo registers --------------- */
/* control section */
#define PM886_LDO_EN1		(0x09)
#define PM886_LDO_EN2		(0x0a)

/* LDO enable2 register offset relative to enable1 register */
#define PM88X_LDO_EN2_OFF	(0x06)

/*
 * 88pm886 ldo voltage:
 * ldox_set_slp[7: 4] ldox_set [3: 0]
 */
#define PM886_LDO1_VOUT		(0x20)
#define PM886_LDO2_VOUT		(0x26)
#define PM886_LDO3_VOUT		(0x2c)
#define PM886_LDO4_VOUT		(0x32)
#define PM886_LDO5_VOUT		(0x38)
#define PM886_LDO6_VOUT		(0x3e)
#define PM886_LDO7_VOUT		(0x44)
#define PM886_LDO8_VOUT		(0x4a)
#define PM886_LDO9_VOUT		(0x50)
#define PM886_LDO10_VOUT	(0x56)
#define PM886_LDO11_VOUT	(0x5c)
#define PM886_LDO12_VOUT	(0x62)
#define PM886_LDO13_VOUT	(0x68)
#define PM886_LDO14_VOUT	(0x6e)
#define PM886_LDO15_VOUT	(0x74)
#define PM886_LDO16_VOUT	(0x7a)

/* ------------- 88pm880 ldo registers --------------- */
/* control section */
#define PM880_LDO_EN1		(0x09)
#define PM880_LDO_EN2		(0x0a)
#define PM880_LDO_EN3		(0x0b)

/*
 * 88pm880 ldo voltage:
 * ldox_set_slp[7: 4] ldox_set [3: 0]
 */
#define PM880_LDO1_VOUT		(0x20)
#define PM880_LDO2_VOUT		(0x26)
#define PM880_LDO3_VOUT		(0x2c)
#define PM880_LDO4_VOUT		(0x32)
#define PM880_LDO5_VOUT		(0x38)
#define PM880_LDO6_VOUT		(0x3e)
#define PM880_LDO7_VOUT		(0x44)
#define PM880_LDO8_VOUT		(0x4a)
#define PM880_LDO9_VOUT		(0x50)
#define PM880_LDO10_VOUT	(0x56)
#define PM880_LDO11_VOUT	(0x5c)
#define PM880_LDO12_VOUT	(0x62)
#define PM880_LDO13_VOUT	(0x68)
#define PM880_LDO14_VOUT	(0x6e)
#define PM880_LDO15_VOUT	(0x74)
#define PM880_LDO16_VOUT	(0x7a)
#define PM880_LDO17_VOUT	(0x80)
#define PM880_LDO18_VOUT	(0x86)

/*
 * set ldo sleep voltage register is the same as the active registers;
 * only the mask is not the same:
 * bit [7 : 4] --> to set sleep voltage
 * bit [3 : 0] --> to set active voltage
 * no need to give definition here
 */

/*
 * vreg - the LDO regs string
 * ebit - the bit number in the enable register.
 * ereg - the enable register
 * amax - the current
 * volt_table - the LDO voltage table
 * For all the LDOes, there are too many ranges. Using volt_table will be
 * simpler and faster.
 */
#define PM88X_LDO(_pmic, vreg, ereg, ebit, amax, ldo_volt_table)	\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm88x_volt_ldo_ops,				\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),		\
		.vsel_reg	= _pmic##_##vreg##_VOUT,			\
		.vsel_mask	= 0xf,					\
		.enable_reg	= _pmic##_LDO_##ereg,			\
		.enable_mask	= 1 << (ebit),				\
		.volt_table	= ldo_volt_table,			\
	},								\
	.max_ua			= (amax),				\
	.sleep_vsel_reg		= _pmic##_##vreg##_VOUT,			\
	.sleep_vsel_mask	= (0xf << 4),				\
	.sleep_enable_reg	= _pmic##_##vreg##_SLP_CTRL,		\
	.sleep_enable_mask	= (0x3 << 4),				\
}

#define PM886_LDO(vreg, ereg, ebit, amax, ldo_volt_table)	\
	PM88X_LDO(PM886, vreg, ereg, ebit, amax, ldo_volt_table)

#define PM880_LDO(vreg, ereg, ebit, amax, ldo_volt_table)	\
	PM88X_LDO(PM880, vreg, ereg, ebit, amax, ldo_volt_table)

/* 88pm886 ldo1~3, 88pm880 ldo1-3 */
static const unsigned int ldo_volt_table1[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};
/* 88pm886 ldo4~15, 88pm880 ldo4-17 */
static const unsigned int ldo_volt_table2[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};
/* 88pm886 ldo16, 88pm880 ldo18 */
static const unsigned int ldo_volt_table3[] = {
	1700000, 1800000, 1900000, 2000000, 2100000, 2500000, 2700000, 2800000,
};

struct pm88x_ldo_info {
	struct regulator_desc desc;
	int max_ua;
	u8 sleep_enable_mask;
	u8 sleep_enable_reg;
	u8 sleep_vsel_reg;
	u8 sleep_vsel_mask;
};

struct pm88x_ldos {
	struct regulator_dev *rdev;
	struct pm88x_chip *chip;
	struct regmap *map;
};

struct pm88x_ldo_print {
	char name[15];
	char enable[15];
	char slp_mode[15];
	char set_slp[15];
	char volt[10];
	char audio_en[15];
	char audio[10];
};

struct pm88x_ldo_extra {
	const char *name;
	u8 audio_enable_reg;
	u8 audio_enable_mask;
	u8 audio_enable_off;
	u8 audio_vsel_reg;
	u8 audio_vsel_mask;
};

#define PM88X_LDO_EXTRA(_name, _areg, _amsk, _aoff, _vreg, _vmsk)	\
{									\
	.name			= _name,				\
	.audio_enable_reg	= _areg,				\
	.audio_enable_mask	= _amsk,				\
	.audio_enable_off	= _aoff,				\
	.audio_vsel_reg		= _vreg,				\
	.audio_vsel_mask	= _vmsk					\
}

static struct pm88x_ldo_extra pm88x_ldo_extra_info[] = {
	PM88X_LDO_EXTRA("LDO2", 0x28, 0x10, 0x04, 0x28, 0x0f),
};

#define LDO_OFF			(0x0 << 4)
#define LDO_ACTIVE_VOLT_SLP	(0x1 << 4)
#define LDO_SLP_VOLT_SLP	(0x2 << 4)
#define LDO_ACTIVE_VOLT_ACTIVE	(0x3 << 4)

int pm88x_ldo_set_suspend_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm88x_ldo_info *info = rdev_get_drvdata(rdev);
	u8 val;
	int ret;

	if (!info)
		return -EINVAL;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		/* regulator will be active with normal voltage */
		val = LDO_ACTIVE_VOLT_ACTIVE;
		break;
	case REGULATOR_MODE_IDLE:
		/* regulator will be in sleep with sleep voltage */
		val = LDO_SLP_VOLT_SLP;
		break;
	case REGULATOR_MODE_STANDBY:
		/* regulator will be off */
		val = LDO_OFF;
		break;
	default:
		/* regulator will be active with sleep voltage */
		val = LDO_ACTIVE_VOLT_SLP;
		break;
	}

	ret = regmap_update_bits(rdev->regmap, info->sleep_enable_reg,
				 info->sleep_enable_mask, val);
	return ret;
}

static unsigned int pm88x_ldo_get_optimum_mode(struct regulator_dev *rdev,
					   int input_uV, int output_uV,
					   int output_uA)
{
	struct pm88x_ldo_info *info = rdev_get_drvdata(rdev);
	if (!info)
		return REGULATOR_MODE_IDLE;

	if (output_uA < 0) {
		dev_err(rdev_get_dev(rdev), "current needs to be > 0.\n");
		return REGULATOR_MODE_IDLE;
	}

	return (output_uA < MAX_SLEEP_CURRENT) ?
		REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
}

static int pm88x_ldo_get_current_limit(struct regulator_dev *rdev)
{
	struct pm88x_ldo_info *info = rdev_get_drvdata(rdev);
	if (!info)
		return 0;
	return info->max_ua;
}

static int pm88x_ldo_set_suspend_voltage(struct regulator_dev *rdev, int uv)
{
	int ret, sel;
	struct pm88x_ldo_info *info = rdev_get_drvdata(rdev);
	if (!info || !info->desc.ops)
		return -EINVAL;
	if (!info->desc.ops->set_suspend_mode)
		return 0;
	/*
	 * two steps:
	 * 1) set the suspend voltage to *_set_slp registers
	 * 2) set regulator mode via set_suspend_mode() interface to enable output
	 */
	/* the suspend voltage mapping is the same as active */
	sel = regulator_map_voltage_iterate(rdev, uv, uv);
	if (sel < 0)
		return -EINVAL;

	sel <<= ffs(info->sleep_vsel_mask) - 1;

	ret = regmap_update_bits(rdev->regmap, info->sleep_vsel_reg,
				 info->sleep_vsel_mask, sel);
	if (ret < 0)
		return -EINVAL;

	/* TODO: do we need this? */
	ret = pm88x_ldo_set_suspend_mode(rdev, REGULATOR_MODE_IDLE);
	return ret;
}

/*
 * about the get_optimum_mode()/set_suspend_mode()/set_suspend_voltage() interface:
 * - 88pm88x has two sets of registers to set and enable/disable regulators
 *   in active and suspend(sleep) status:
 *   the following focues on the sleep part:
 *   - there are two control bits: 00-disable,
 *				   01/10-use sleep voltage,
 *				   11-use active voltage,
 *- in most of the scenario, these registers are configured when the whole PMIC
 *  initialized, when the system enters into suspend(sleep) mode, the regulator
 *  works according to the setting or disabled;
 *- there is also case that the device driver needs to:
 *  - set the sleep voltage;
 *  - choose to use sleep voltage or active voltage depends on the load;
 *  so:
 *  set_suspend_voltage() is used to manipulate the registers to set sleep volt;
 *  set_suspend_mode() is used to switch between sleep voltage and active voltage
 *  get_optimum_mode() is used to get right mode
 */
static struct regulator_ops pm88x_volt_ldo_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm88x_ldo_get_current_limit,
	.get_optimum_mode = pm88x_ldo_get_optimum_mode,
	.set_suspend_mode = pm88x_ldo_set_suspend_mode,
	.set_suspend_voltage = pm88x_ldo_set_suspend_voltage,
};

/* The array is indexed by id(PM886_ID_LDO*) */
static struct pm88x_ldo_info pm886_ldo_configs[] = {
	/* 88pm886 ldo */
	PM886_LDO(LDO1, EN1, 0, 100000, ldo_volt_table1),
	PM886_LDO(LDO2, EN1, 1, 100000, ldo_volt_table1),
	PM886_LDO(LDO3, EN1, 2, 100000, ldo_volt_table1),
	PM886_LDO(LDO4, EN1, 3, 400000, ldo_volt_table2),
	PM886_LDO(LDO5, EN1, 4, 400000, ldo_volt_table2),
	PM886_LDO(LDO6, EN1, 5, 400000, ldo_volt_table2),
	PM886_LDO(LDO7, EN1, 6, 400000, ldo_volt_table2),
	PM886_LDO(LDO8, EN1, 7, 400000, ldo_volt_table2),
	PM886_LDO(LDO9, EN2, 0, 400000, ldo_volt_table2),
	PM886_LDO(LDO10, EN2, 1, 200000, ldo_volt_table2),
	PM886_LDO(LDO11, EN2, 2, 200000, ldo_volt_table2),
	PM886_LDO(LDO12, EN2, 3, 200000, ldo_volt_table2),
	PM886_LDO(LDO13, EN2, 4, 200000, ldo_volt_table2),
	PM886_LDO(LDO14, EN2, 5, 200000, ldo_volt_table2),
	PM886_LDO(LDO15, EN2, 6, 200000, ldo_volt_table2),
	PM886_LDO(LDO16, EN2, 7, 200000, ldo_volt_table3),
};

/* The array is indexed by id(PM880_ID_LDO*) */
static struct pm88x_ldo_info pm880_ldo_configs[] = {
	/* 88pm880 ldo */
	PM880_LDO(LDO1, EN1, 0, 100000, ldo_volt_table1),
	PM880_LDO(LDO2, EN1, 1, 100000, ldo_volt_table1),
	PM880_LDO(LDO3, EN1, 2, 100000, ldo_volt_table1),
	PM880_LDO(LDO4, EN1, 3, 400000, ldo_volt_table2),
	PM880_LDO(LDO5, EN1, 4, 400000, ldo_volt_table2),
	PM880_LDO(LDO6, EN1, 5, 400000, ldo_volt_table2),
	PM880_LDO(LDO7, EN1, 6, 400000, ldo_volt_table2),
	PM880_LDO(LDO8, EN1, 7, 400000, ldo_volt_table2),
	PM880_LDO(LDO9, EN2, 0, 400000, ldo_volt_table2),
	PM880_LDO(LDO10, EN2, 1, 200000, ldo_volt_table2),
	PM880_LDO(LDO11, EN2, 2, 200000, ldo_volt_table2),
	PM880_LDO(LDO12, EN2, 3, 200000, ldo_volt_table2),
	PM880_LDO(LDO13, EN2, 4, 200000, ldo_volt_table2),
	PM880_LDO(LDO14, EN2, 5, 200000, ldo_volt_table2),
	PM880_LDO(LDO15, EN2, 6, 200000, ldo_volt_table2),
	PM880_LDO(LDO16, EN2, 7, 200000, ldo_volt_table2),
	PM880_LDO(LDO17, EN3, 0, 200000, ldo_volt_table2),
	PM880_LDO(LDO18, EN3, 1, 200000, ldo_volt_table3),
};

#define PM88X_LDO_OF_MATCH(_pmic, id, comp, label) \
	{ \
		.compatible = comp, \
		.data = &_pmic##_ldo_configs[id##_##label], \
	}

#define PM886_LDO_OF_MATCH(comp, label) \
	PM88X_LDO_OF_MATCH(pm886, PM886_ID, comp, label)

#define PM880_LDO_OF_MATCH(comp, label) \
	PM88X_LDO_OF_MATCH(pm880, PM880_ID, comp, label)

static const struct of_device_id pm88x_ldos_of_match[] = {
	/* 88pm886 */
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo1", LDO1),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo2", LDO2),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo3", LDO3),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo4", LDO4),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo5", LDO5),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo6", LDO6),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo7", LDO7),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo8", LDO8),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo9", LDO9),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo10", LDO10),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo11", LDO11),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo12", LDO12),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo13", LDO13),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo14", LDO14),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo15", LDO15),
	PM886_LDO_OF_MATCH("marvell,88pm886-ldo16", LDO16),
	/* 88pm880 */
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo1", LDO1),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo2", LDO2),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo3", LDO3),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo4", LDO4),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo5", LDO5),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo6", LDO6),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo7", LDO7),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo8", LDO8),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo9", LDO9),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo10", LDO10),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo11", LDO11),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo12", LDO12),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo13", LDO13),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo14", LDO14),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo15", LDO15),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo16", LDO16),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo17", LDO17),
	PM880_LDO_OF_MATCH("marvell,88pm880-ldo18", LDO18),
};

/*
 * The function convert the ldo voltage register value
 * to a real voltage value (in uV) according to the voltage table.
 */
static int pm88x_get_vldo_vol(unsigned int val, struct pm88x_ldo_info *info)
{
	int volt = -EINVAL;

	/* get the voltage via the register value */
	volt = info->desc.volt_table[val];
	return volt;
}

/* The function check if the regulator register is configured to enable/disable */
static int pm88x_check_en(struct pm88x_chip *chip, unsigned int reg, unsigned int mask,
			unsigned int reg2)
{
	struct regmap *map = chip->ldo_regmap;
	int ret, value;
	unsigned int enable1, enable2;

	ret = regmap_read(map, reg, &enable1);
	if (ret < 0)
		return ret;

	ret = regmap_read(map, reg2, &enable2);
	if (ret < 0)
		return ret;

	value = (enable1 | enable2) & mask;

	return value;
}

/* The function check the regulator sleep mode as configured in his register */
static int pm88x_check_slp_mode(struct regmap *map, unsigned int reg, int mask)
{
	int ret;
	unsigned int slp_mode;

	ret = regmap_read(map, reg, &slp_mode);
	if (ret < 0)
		return ret;

	slp_mode = (slp_mode & mask) >> (ffs(mask) - 1);

	return slp_mode;
}

/* The function return the value in the regulator voltage register */
static unsigned int pm88x_check_vol(struct regmap *map, unsigned int reg, unsigned int mask)
{
	int ret;
	unsigned int vol_val;

	ret = regmap_bulk_read(map, reg, &vol_val, 1);
	if (ret < 0)
		return ret;

	/* mask and shift the relevant value from the register */
	vol_val = (vol_val & mask) >> (ffs(mask) - 1);

	return vol_val;
}

static int pm88x_update_print(struct pm88x_chip *chip, struct pm88x_ldo_info *info,
			      struct pm88x_ldo_extra *extra, struct pm88x_ldo_print *print_temp,
			      int index, int ldo_num, int extra_info_num)
{
	int i, ret, volt, flag = 0;
	struct regmap *map = chip->ldo_regmap;
	char *slp_mode_str[] = {"off", "active_slp", "sleep", "active"};
	int slp_mode_num = sizeof(slp_mode_str)/sizeof(slp_mode_str[0]);

	sprintf(print_temp->name, "%s", info[index].desc.name);

	/* check enable/disable */
	ret = pm88x_check_en(chip, info[index].desc.enable_reg, info[index].desc.enable_mask,
			     info[index].desc.enable_reg + PM88X_LDO_EN2_OFF);
	if (ret < 0)
		return ret;
	else if (ret)
		strcpy(print_temp->enable, "enable");
	else
		strcpy(print_temp->enable, "disable");

	/* check sleep mode */
	ret = pm88x_check_slp_mode(map, info[index].sleep_enable_reg,
				   info[index].sleep_enable_mask);
	if (ret < 0)
		return ret;
	if (ret < slp_mode_num)
		strcpy(print_temp->slp_mode, slp_mode_str[ret]);
	else
		strcpy(print_temp->slp_mode, "unknown");

	/* print sleep voltage */
	ret = pm88x_check_vol(map, info[index].sleep_vsel_reg, info[index].sleep_vsel_mask);
	if (ret < 0)
		return ret;

	volt = pm88x_get_vldo_vol(ret, &info[index]);
	if (volt < 0)
		return volt;
	else
		sprintf(print_temp->set_slp, "%4d", volt/1000);

	/* print active voltage(s) */
	ret = pm88x_check_vol(map, info[index].desc.vsel_reg,
			      info[index].desc.vsel_mask);
	if (ret < 0)
		return ret;

	volt = pm88x_get_vldo_vol(ret, &info[index]);
	if (volt < 0)
		return volt;
	else
		sprintf(print_temp->volt, "%4d", volt/1000);

	for (i = 0; i < extra_info_num; i++) {
		/* print audio voltage */
		if (!strcmp(print_temp->name, extra[i].name) && extra[i].audio_enable_reg) {
			ret = pm88x_check_en(chip, extra[i].audio_enable_reg,
					     extra[i].audio_enable_mask,
					     extra[i].audio_enable_reg);
			if (ret < 0)
				return ret;
			else if (ret)
				strcpy(print_temp->audio_en, "enable");
			else
				strcpy(print_temp->audio_en, "disable");
			ret = pm88x_check_vol(map, extra[i].audio_vsel_reg,
					      extra[i].audio_vsel_mask);
			if (ret < 0)
				return ret;

			volt = pm88x_get_vldo_vol(ret, &info[index]);
			if (volt < 0)
				return volt;
			else
				sprintf(print_temp->audio, "%4d", volt/1000);
			flag = 1;
		}
	}
	if (!flag) {
		strcpy(print_temp->audio_en, "   -   ");
		sprintf(print_temp->audio, "  -");
		flag = 0;
	}
	return 0;
}

int pm88x_display_ldo(struct pm88x_chip *chip, char *buf)
{
	struct pm88x_ldo_print *print_temp;
	struct pm88x_ldo_info *info;
	struct pm88x_ldo_extra *extra;
	int ldo_num, extra_info_num, i, len = 0;
	ssize_t ret;

	switch (chip->type) {
	case PM886:
		info = pm886_ldo_configs;
		ldo_num = sizeof(pm886_ldo_configs) / sizeof(pm886_ldo_configs[0]);
		extra = pm88x_ldo_extra_info;
		extra_info_num = sizeof(pm88x_ldo_extra_info) / sizeof(pm88x_ldo_extra_info[0]);
		break;
	case PM880:
		info = pm880_ldo_configs;
		ldo_num = sizeof(pm880_ldo_configs) / sizeof(pm880_ldo_configs[0]);
		extra = pm88x_ldo_extra_info;
		extra_info_num = sizeof(pm88x_ldo_extra_info) / sizeof(pm88x_ldo_extra_info[0]);
		break;
	default:
		pr_err("%s: Cannot find chip type.\n", __func__);
		return -ENODEV;
	}

	print_temp = kmalloc(sizeof(struct pm88x_ldo_print), GFP_KERNEL);
	if (!print_temp) {
		pr_err("%s: Cannot allocate print template.\n", __func__);
		return -ENOMEM;
	}

	len += sprintf(buf + len, "\nLDO");
	len += sprintf(buf + len, "\n-----------------------------------");
	len += sprintf(buf + len, "-------------------------------------\n");
	len += sprintf(buf + len, "|   name   | status  |  slp_mode  |slp_volt");
	len += sprintf(buf + len, "|  volt  | audio_en| audio  |\n");
	len += sprintf(buf + len, "------------------------------------");
	len += sprintf(buf + len, "------------------------------------\n");

	for (i = 0; i < ldo_num; i++) {
		ret = pm88x_update_print(chip, info, extra, print_temp, i, ldo_num, extra_info_num);
		if (ret < 0) {
			pr_err("Print of regulator %s failed\n", print_temp->name);
			goto out_print;
		}
		len += sprintf(buf + len, "| %-8s |", print_temp->name);
		len += sprintf(buf + len, " %-7s |", print_temp->enable);
		len += sprintf(buf + len, "  %-10s|", print_temp->slp_mode);
		len += sprintf(buf + len, "  %-5s |", print_temp->set_slp);
		len += sprintf(buf + len, "  %-5s |", print_temp->volt);
		len += sprintf(buf + len, " %-7s |", print_temp->audio_en);
		len += sprintf(buf + len, "  %-5s |\n", print_temp->audio);
	}

	len += sprintf(buf + len, "------------------------------------");
	len += sprintf(buf + len, "------------------------------------\n");

	ret = len;
out_print:
	kfree(print_temp);
	return ret;
}

static int pm88x_ldo_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_ldos *data;
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	struct regulation_constraints *c;
	const struct of_device_id *match;
	const struct pm88x_ldo_info *const_info;
	struct pm88x_ldo_info *info;
	int ret;

	match = of_match_device(pm88x_ldos_of_match, &pdev->dev);
	if (match) {
		const_info = match->data;
		init_data = of_get_regulator_init_data(&pdev->dev,
						       pdev->dev.of_node);
	} else {
		dev_err(&pdev->dev, "parse dts fails!\n");
		return -EINVAL;
	}

	info = kmemdup(const_info, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	data = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_ldos),
			    GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to allocate pm88x_regualtors");
		return -ENOMEM;
	}
	data->map = chip->ldo_regmap;
	data->chip = chip;

	/* add regulator config */
	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.driver_data = info;
	config.regmap = data->map;
	config.of_node = pdev->dev.of_node;

	data->rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
	if (IS_ERR(data->rdev)) {
		dev_err(&pdev->dev, "cannot register %s\n", info->desc.name);
		ret = PTR_ERR(data->rdev);
		return ret;
	}

	c = data->rdev->constraints;
	c->valid_ops_mask |= REGULATOR_CHANGE_DRMS | REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_VOLTAGE;
	c->valid_modes_mask |= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_IDLE;
	c->input_uV = 1000;

	platform_set_drvdata(pdev, data);

	return 0;
}

static int pm88x_ldo_remove(struct platform_device *pdev)
{
	struct pm88x_ldos *data = platform_get_drvdata(pdev);
	devm_kfree(&pdev->dev, data);
	return 0;
}

static struct platform_driver pm88x_ldo_driver = {
	.driver		= {
		.name	= "88pm88x-ldo",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_ldos_of_match),
	},
	.probe		= pm88x_ldo_probe,
	.remove		= pm88x_ldo_remove,
};

static int pm88x_ldo_init(void)
{
	return platform_driver_register(&pm88x_ldo_driver);
}
subsys_initcall(pm88x_ldo_init);

static void pm88x_ldo_exit(void)
{
	platform_driver_unregister(&pm88x_ldo_driver);
}
module_exit(pm88x_ldo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yi Zhang<yizhang@marvell.com>");
MODULE_DESCRIPTION("LDO Driver for Marvell 88PM88X PMIC");
MODULE_ALIAS("platform:88pm88x-ldo");
