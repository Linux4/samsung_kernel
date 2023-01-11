/*
 * Buck driver for Marvell 88PM88X
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

/* BUCK enable2 register offset relative to enable1 register */
#define PM88X_BUCK_EN2_OFF	(0x06)
/* ------------- 88pm886 buck registers --------------- */

/* buck voltage */
#define PM886_BUCK2_VOUT	(0xb3)
#define PM886_BUCK3_VOUT	(0xc1)
#define PM886_BUCK4_VOUT	(0xcf)
#define PM886_BUCK5_VOUT	(0xdd)

/* set buck sleep voltage */
#define PM886_BUCK1_SET_SLP	(0xa3)
#define PM886_BUCK2_SET_SLP	(0xb1)
#define PM886_BUCK3_SET_SLP	(0xbf)
#define PM886_BUCK4_SET_SLP	(0xcd)
#define PM886_BUCK5_SET_SLP	(0xdb)

/* control section */
#define PM886_BUCK_EN		(0x08)

/* ------------- 88pm880 buck registers --------------- */
/* buck voltage */
#define PM880_BUCK2_VOUT	(0x58)
#define PM880_BUCK3_VOUT	(0x70)
#define PM880_BUCK4_VOUT	(0x88)
#define PM880_BUCK5_VOUT	(0x98)
#define PM880_BUCK6_VOUT	(0xa8)

/* set buck sleep voltage */
#define PM880_BUCK1_SET_SLP	(0x27)
#define PM880_BUCK1A_SET_SLP	(0x27)
#define PM880_BUCK1B_SET_SLP	(0x3c)

#define PM880_BUCK2_SET_SLP	(0x56)
#define PM880_BUCK3_SET_SLP	(0x6e)
#define PM880_BUCK4_SET_SLP	(0x86)
#define PM880_BUCK5_SET_SLP	(0x96)
#define PM880_BUCK6_SET_SLP	(0xa6)
#define PM880_BUCK7_SET_SLP	(0xb6)

/* control section */
#define PM880_BUCK_EN		(0x08)

/*
 * vreg - the buck regs string.
 * ebit - the bit number in the enable register.
 * amax - the current
 * Buck has 2 kinds of voltage steps. It is easy to find voltage by ranges,
 * not the constant voltage table.
 */
#define PM88X_BUCK(_pmic, vreg, ebit, amax, volt_ranges, n_volt, slp_en_msk, slp_en_off)	\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm88x_volt_buck_ops,				\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages		= n_volt,			\
		.linear_ranges		= volt_ranges,			\
		.n_linear_ranges	= ARRAY_SIZE(volt_ranges),	\
		.vsel_reg	= _pmic##_##vreg##_VOUT,		\
		.vsel_mask	= 0x7f,					\
		.enable_reg	= _pmic##_BUCK_EN,			\
		.enable_mask	= 1 << (ebit),				\
	},								\
	.max_ua			= (amax),				\
	.sleep_vsel_reg		= _pmic##_##vreg##_SET_SLP,		\
	.sleep_vsel_mask	= 0x7f,					\
	.sleep_enable_reg	= _pmic##_##vreg##_SLP_CTRL,		\
	.sleep_enable_mask	= (slp_en_msk << slp_en_off),		\
	.sleep_enable_off	= (slp_en_off),				\
}

#define PM886_BUCK(vreg, ebit, amax, volt_ranges, n_volt)		\
	PM88X_BUCK(PM886, vreg, ebit, amax, volt_ranges, n_volt, 0x3, 4)

#define PM880_BUCK(vreg, ebit, amax, volt_ranges, n_volt)		\
	PM88X_BUCK(PM880, vreg, ebit, amax, volt_ranges, n_volt, 0x3, 4)

#define PM886_BUCK_AUDIO(vreg, ebit, amax, volt_ranges, n_volt)		\
	PM88X_BUCK(PM886, vreg, ebit, amax, volt_ranges, n_volt, 0x1, 7)

#define PM880_BUCK_AUDIO(vreg, ebit, amax, volt_ranges, n_volt)		\
	PM88X_BUCK(PM880, vreg, ebit, amax, volt_ranges, n_volt, 0x1, 7)

/*
 * 88pm886 buck1 and 88pm880 buck1. both have dvc function
 * from 0x00 to 0x4F: step is 12.5mV, range is from 0.6V to 1.6V
 * from 0x50 to 0x3F step is 50mV, range is from 1.6V to 1.8V
 */
static const struct regulator_linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 0x4f, 12500),
	REGULATOR_LINEAR_RANGE(1600000, 0x50, 0x54, 50000),
};

/*
 * 88pm886 buck 2-5, and 88pm880 buck 2-7
 * from 0x00 to 0x4F VOUT step is 12.5mV, range is from 0.6V to 1.6V
 * from 0x50 to 0x72 step is 50mV, range is from 1.6V to  3.3V
 */
static const struct regulator_linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 0x4f, 12500),
	REGULATOR_LINEAR_RANGE(1600000, 0x50, 0x72, 50000),
};

struct pm88x_buck_info {
	struct regulator_desc desc;
	int max_ua;
	u8 sleep_enable_mask;
	u8 sleep_enable_reg;
	u8 sleep_enable_off;
	u8 sleep_vsel_reg;
	u8 sleep_vsel_mask;
};

struct pm88x_regulators {
	struct regulator_dev *rdev;
	struct pm88x_chip *chip;
	struct regmap *map;
};

struct pm88x_buck_print {
	char name[15];
	char enable[15];
	char slp_mode[15];
	char set_slp[15];
	char volt[10];
	char audio_en[15];
	char audio[10];
};

struct pm88x_buck_extra {
	const char *name;
	bool dvc;
	u8 audio_enable_reg;
	u8 audio_enable_mask;
	u8 audio_enable_off;
	u8 audio_vsel_reg;
	u8 audio_vsel_mask;
};

#define PM88X_BUCK_EXTRA(_name, _dvc, _areg, _amsk, _aoff, _vreg, _vmsk)	\
{										\
	.name			= _name,					\
	.dvc			= _dvc,						\
	.audio_enable_reg	= _areg,					\
	.audio_enable_mask	= _amsk,					\
	.audio_enable_off	= _aoff,					\
	.audio_vsel_reg		= _vreg,					\
	.audio_vsel_mask	= _vmsk						\
}

static struct pm88x_buck_extra pm886_buck_extra_info[] = {
	PM88X_BUCK_EXTRA("BUCK1", 1, 0xa4, 0x80, 0x07, 0xa4, 0x7f),
	PM88X_BUCK_EXTRA("BUCK2", 0, 0xb2, 0x80, 0x07, 0xb2, 0x7f),
	PM88X_BUCK_EXTRA("BUCK3", 0, 0xc0, 0x80, 0x07, 0xc0, 0x7f),
	PM88X_BUCK_EXTRA("BUCK4", 0, 0, 0, 0, 0, 0),
	PM88X_BUCK_EXTRA("BUCK5", 0, 0, 0, 0, 0, 0),
};

static struct pm88x_buck_extra pm880_buck_extra_info[] = {
	PM88X_BUCK_EXTRA("BUCK1A", 1, 0x27, 0x80, 0x07, 0x27, 0x7f),
	PM88X_BUCK_EXTRA("BUCK2", 0, 0x57, 0x80, 0x07, 0x57, 0x7f),
	PM88X_BUCK_EXTRA("BUCK3", 0, 0x6f, 0x80, 0x07, 0x6f, 0x7f),
	PM88X_BUCK_EXTRA("BUCK4", 0, 0, 0, 0, 0, 0),
	PM88X_BUCK_EXTRA("BUCK5", 0, 0, 0, 0, 0, 0),
	PM88X_BUCK_EXTRA("BUCK6", 0, 0, 0, 0, 0, 0),
	PM88X_BUCK_EXTRA("BUCK7", 1, 0, 0, 0, 0, 0),
};

#define BUCK_OFF		(0x0)
#define BUCK_ACTIVE_VOLT_SLP	(0x1)
#define BUCK_SLP_VOLT_SLP	(0x2)
#define BUCK_ACTIVE_VOLT_ACTIVE	(0x3)
#define BUCK_AUDIO_MODE_EN	(0x1)

int pm88x_buck_set_suspend_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct pm88x_buck_info *info = rdev_get_drvdata(rdev);
	u8 val;
	int ret, rid;

	if (!info)
		return -EINVAL;

	rid = rdev_get_id(rdev);
	/* we enabled buck1 audio mode enable/disable for 88pm886 and 88pm880 */
	if (rid == PM880_ID_BUCK1A) {
		switch (mode) {
		case REGULATOR_MODE_NORMAL:
			val = 0;
			break;
		case REGULATOR_MODE_IDLE:
			val = BUCK_AUDIO_MODE_EN;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (mode) {
		case REGULATOR_MODE_NORMAL:
			/* regulator will be active with normal voltage */
			val = BUCK_ACTIVE_VOLT_ACTIVE;
			break;
		case REGULATOR_MODE_IDLE:
			/* regulator will be in sleep with sleep voltage */
			val = BUCK_SLP_VOLT_SLP;
			break;
		case REGULATOR_MODE_STANDBY:
			/* regulator will be off */
			val = BUCK_OFF;
			break;
		default:
			/* regulator will be active with sleep voltage */
			val = BUCK_ACTIVE_VOLT_SLP;
			break;
		}
	}

	ret = regmap_update_bits(rdev->regmap, info->sleep_enable_reg,
				 info->sleep_enable_mask, (val << info->sleep_enable_off));
	return ret;
}

static unsigned int pm88x_buck_get_optimum_mode(struct regulator_dev *rdev,
					   int input_uV, int output_uV,
					   int output_uA)
{
	struct pm88x_buck_info *info = rdev_get_drvdata(rdev);
	if (!info)
		return REGULATOR_MODE_IDLE;

	if (output_uA < 0) {
		dev_err(rdev_get_dev(rdev), "current needs to be > 0.\n");
		return REGULATOR_MODE_IDLE;
	}

	return (output_uA < MAX_SLEEP_CURRENT) ?
		REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
}

static int pm88x_buck_get_current_limit(struct regulator_dev *rdev)
{
	struct pm88x_buck_info *info = rdev_get_drvdata(rdev);
	if (!info)
		return 0;
	return info->max_ua;
}

static int pm88x_buck_set_suspend_voltage(struct regulator_dev *rdev, int uv)
{
	int ret, sel;
	struct pm88x_buck_info *info = rdev_get_drvdata(rdev);
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
	sel = regulator_map_voltage_linear_range(rdev, uv, uv);
	if (sel < 0)
		return -EINVAL;

	sel <<= ffs(info->sleep_vsel_mask) - 1;

	ret = regmap_update_bits(rdev->regmap, info->sleep_vsel_reg,
				 info->sleep_vsel_mask, sel);
	if (ret < 0)
		return -EINVAL;

	/* TODO: do we need this? */
	ret = pm88x_buck_set_suspend_mode(rdev, REGULATOR_MODE_IDLE);
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
static struct regulator_ops pm88x_volt_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_current_limit = pm88x_buck_get_current_limit,
	.get_optimum_mode = pm88x_buck_get_optimum_mode,
	.set_suspend_mode = pm88x_buck_set_suspend_mode,
	.set_suspend_voltage = pm88x_buck_set_suspend_voltage,
};

/* The array is indexed by id(PM886_ID_BUCK*) */
static struct pm88x_buck_info pm886_buck_configs[] = {
	PM886_BUCK(BUCK1, 0, 3000000, buck_volt_range1, 0x55),
	PM886_BUCK(BUCK2, 1, 1200000, buck_volt_range2, 0x73),
	PM886_BUCK(BUCK3, 2, 1200000, buck_volt_range2, 0x73),
	PM886_BUCK(BUCK4, 3, 1200000, buck_volt_range2, 0x73),
	PM886_BUCK(BUCK5, 4, 1200000, buck_volt_range2, 0x73),
};

/* The array is indexed by id(PM880_ID_BUCK*) */
static struct pm88x_buck_info pm880_buck_configs[] = {
	PM880_BUCK_AUDIO(BUCK1A, 0, 3000000, buck_volt_range1, 0x55),
	PM880_BUCK(BUCK2,  2, 1200000, buck_volt_range2, 0x73),
	PM880_BUCK(BUCK3,  3, 1200000, buck_volt_range2, 0x73),
	PM880_BUCK(BUCK4,  4, 1200000, buck_volt_range2, 0x73),
	PM880_BUCK(BUCK5,  5, 1200000, buck_volt_range2, 0x73),
	PM880_BUCK(BUCK6,  6, 1200000, buck_volt_range2, 0x73),
	PM880_BUCK(BUCK7,  7, 1200000, buck_volt_range2, 0x73),
};

#define PM88X_BUCK_OF_MATCH(_pmic, id, comp, label) \
	{ \
		.compatible = comp, \
		.data = &_pmic##_buck_configs[id##_##label], \
	}
#define PM886_BUCK_OF_MATCH(comp, label) \
	PM88X_BUCK_OF_MATCH(pm886, PM886_ID, comp, label)

#define PM880_BUCK_OF_MATCH(comp, label) \
	PM88X_BUCK_OF_MATCH(pm880, PM880_ID, comp, label)

static const struct of_device_id pm88x_bucks_of_match[] = {
	PM886_BUCK_OF_MATCH("marvell,88pm886-buck1", BUCK1),
	PM886_BUCK_OF_MATCH("marvell,88pm886-buck2", BUCK2),
	PM886_BUCK_OF_MATCH("marvell,88pm886-buck3", BUCK3),
	PM886_BUCK_OF_MATCH("marvell,88pm886-buck4", BUCK4),
	PM886_BUCK_OF_MATCH("marvell,88pm886-buck5", BUCK5),

	PM880_BUCK_OF_MATCH("marvell,88pm880-buck1a", BUCK1A),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck2", BUCK2),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck3", BUCK3),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck4", BUCK4),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck5", BUCK5),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck6", BUCK6),
	PM880_BUCK_OF_MATCH("marvell,88pm880-buck7", BUCK7),
};

/*
 * The function convert the buck voltage register value
 * to a real voltage value (in uV) according to the voltage table.
 */
static int pm88x_get_vbuck_vol(unsigned int val, struct pm88x_buck_info *info)
{
	const struct regulator_linear_range *range;
	int i, volt = -EINVAL;

	/* get the voltage via the register value */
	for (i = 0; i < info->desc.n_linear_ranges; i++) {
		range = &info->desc.linear_ranges[i];
		if (!range)
			return -EINVAL;

		if (val >= range->min_sel && val <= range->max_sel) {
			volt = (val - range->min_sel) * range->uV_step + range->min_uV;
			break;
		}
	}
	return volt;
}

/* The function check if the regulator register is configured to enable/disable */
static int pm88x_check_en(struct pm88x_chip *chip, unsigned int reg, unsigned int mask,
			unsigned int reg2)
{
	struct regmap *map = chip->buck_regmap;
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
static int pm88x_check_slp_mode(struct regmap *map, unsigned int reg, int off)
{
	int ret;
	unsigned int slp_mode;

	ret = regmap_read(map, reg, &slp_mode);
	if (ret < 0)
		return ret;

	slp_mode = (slp_mode >> off) & 0x3;

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

static int pm88x_update_print(struct pm88x_chip *chip, struct pm88x_buck_info *info,
			      struct pm88x_buck_extra *extra, struct pm88x_buck_print *print_temp,
			      int index, int buck_num)
{
	int ret, volt;
	struct regmap *map = chip->buck_regmap;
	char *slp_mode_str[] = {"off", "active_slp", "sleep", "active"};
	int slp_mode_num = ARRAY_SIZE(slp_mode_str);

	sprintf(print_temp->name, "%s", info[index].desc.name);

	/* check enable/disable */
	ret = pm88x_check_en(chip, info[index].desc.enable_reg, info[index].desc.enable_mask,
			     info[index].desc.enable_reg + PM88X_BUCK_EN2_OFF);
	if (ret < 0)
		return ret;
	else if (ret)
		strcpy(print_temp->enable, "enable");
	else
		strcpy(print_temp->enable, "disable");

	if (!strcmp(print_temp->name, "BUCK1A")) {
		sprintf(print_temp->slp_mode, " VR");
		sprintf(print_temp->set_slp, " VR");
	} else {
		/* check sleep mode */
		ret = pm88x_check_slp_mode(map, info[index].sleep_enable_reg,
					   info[index].sleep_enable_off);
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

		volt = pm88x_get_vbuck_vol(ret, &info[index]);
		if (volt < 0)
			return volt;
		else
			sprintf(print_temp->set_slp, "%4d", volt/1000);
	}

	/* print active voltage(s) */
	if (extra[index].dvc) {
		sprintf(print_temp->volt, " DVC ");
	} else {
		ret = pm88x_check_vol(map, info[index].desc.vsel_reg,
				      info[index].desc.vsel_mask);
		if (ret < 0)
			return ret;

		volt = pm88x_get_vbuck_vol(ret, &info[index]);
		if (volt < 0)
			return volt;
		else
			sprintf(print_temp->volt, "%4d", volt/1000);
	}

	/* print audio voltage */
	if (extra[index].audio_enable_reg) {
		ret = pm88x_check_en(chip, extra[index].audio_enable_reg,
				     extra[index].audio_enable_mask,
				     extra[index].audio_enable_reg);
		if (ret < 0)
			return ret;
		else if (ret)
			strcpy(print_temp->audio_en, "enable");
		else
			strcpy(print_temp->audio_en, "disable");

		ret = pm88x_check_vol(map, extra[index].audio_vsel_reg,
				      extra[index].audio_vsel_mask);
		if (ret < 0)
			return ret;

		volt = pm88x_get_vbuck_vol(ret, &info[index]);
		if (volt < 0)
			return volt;
		else
			sprintf(print_temp->audio, "%4d", volt/1000);
	} else {
		strcpy(print_temp->audio_en, "   -   ");
		sprintf(print_temp->audio, "  -");
	}

	return 0;
}

int pm88x_display_buck(struct pm88x_chip *chip, char *buf)
{
	struct pm88x_buck_print *print_temp;
	struct pm88x_buck_info *info;
	struct pm88x_buck_extra *extra;
	int buck_num, i, len = 0;
	ssize_t ret;

	switch (chip->type) {
	case PM886:
		info = pm886_buck_configs;
		extra = pm886_buck_extra_info;
		buck_num = ARRAY_SIZE(pm886_buck_configs);
		break;
	case PM880:
		info = pm880_buck_configs;
		extra = pm880_buck_extra_info;
		buck_num = ARRAY_SIZE(pm880_buck_configs);
		break;
	default:
		pr_err("%s: Cannot find chip type.\n", __func__);
		return -ENODEV;
	}

	print_temp = kmalloc(sizeof(struct pm88x_buck_print), GFP_KERNEL);
	if (!print_temp) {
		pr_err("%s: Cannot allocate print template.\n", __func__);
		return -ENOMEM;
	}

	len += sprintf(buf + len, "\nBUCK");
	len += sprintf(buf + len, "\n-----------------------------------");
	len += sprintf(buf + len, "-------------------------------------\n");
	len += sprintf(buf + len, "|   name   | status  |  slp_mode  |slp_volt");
	len += sprintf(buf + len, "|  volt  | audio_en| audio  |\n");
	len += sprintf(buf + len, "------------------------------------");
	len += sprintf(buf + len, "------------------------------------\n");

	for (i = 0; i < buck_num; i++) {
		ret = pm88x_update_print(chip, info, extra, print_temp, i, buck_num);
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

static int pm88x_buck_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_regulators *data;
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	struct regulation_constraints *c;
	const struct of_device_id *match;
	const struct pm88x_buck_info *const_info;
	struct pm88x_buck_info *info;
	int ret;

	match = of_match_device(pm88x_bucks_of_match, &pdev->dev);
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

	data = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_regulators),
			    GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to allocate pm88x_regualtors");
		return -ENOMEM;
	}
	data->map = chip->buck_regmap;
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

static int pm88x_buck_remove(struct platform_device *pdev)
{
	struct pm88x_regulators *data = platform_get_drvdata(pdev);
	devm_kfree(&pdev->dev, data);
	return 0;
}

static struct platform_driver pm88x_buck_driver = {
	.driver		= {
		.name	= "88pm88x-buck",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_bucks_of_match),
	},
	.probe		= pm88x_buck_probe,
	.remove		= pm88x_buck_remove,
};

static int pm88x_buck_init(void)
{
	return platform_driver_register(&pm88x_buck_driver);
}
subsys_initcall(pm88x_buck_init);

static void pm88x_buck_exit(void)
{
	platform_driver_unregister(&pm88x_buck_driver);
}
module_exit(pm88x_buck_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yi Zhang<yizhang@marvell.com>");
MODULE_DESCRIPTION("Buck for Marvell 88PM88X PMIC");
MODULE_ALIAS("platform:88pm88x-buck");
