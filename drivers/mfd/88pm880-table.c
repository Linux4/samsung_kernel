/*
 * Marvell 88PM880 specific setting
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *  Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm880.h>
#include <linux/mfd/88pm88x-reg.h>

#include "88pm88x.h"

#define PM880_BUCK_NAME		"88pm880-buck"
#define PM880_LDO_NAME		"88pm880-ldo"

const struct regmap_config pm880_base_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm880_base_i2c_regmap);

const struct regmap_config pm880_power_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm880_power_i2c_regmap);

const struct regmap_config pm880_gpadc_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm880_gpadc_i2c_regmap);

const struct regmap_config pm880_battery_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm880_battery_i2c_regmap);

const struct regmap_config pm880_test_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm880_test_i2c_regmap);

static const struct resource buck_resources[] = {
	{
	.name = PM880_BUCK_NAME,
	},
};

static const struct resource ldo_resources[] = {
	{
	.name = PM880_LDO_NAME,
	},
};

const struct mfd_cell pm880_cell_devs[] = {
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck1a", 0),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck2", 1),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck3", 2),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck4", 3),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck5", 4),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck6", 5),
	CELL_DEV(PM880_BUCK_NAME, buck_resources, "marvell,88pm880-buck7", 6),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo1", 7),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo2", 8),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo3", 9),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo4", 10),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo5", 11),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo6", 12),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo7", 13),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo8", 14),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo9", 15),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo10", 16),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo11", 17),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo12", 18),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo13", 19),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo14", 20),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo15", 21),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo16", 22),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo17", 23),
	CELL_DEV(PM880_LDO_NAME, ldo_resources, "marvell,88pm880-ldo18", 24),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm880-buck1a-slp", 25),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm880-buck1a-audio", 26),
};
EXPORT_SYMBOL_GPL(pm880_cell_devs);

struct pmic_cell_info pm880_cell_info = {
	.cells   = pm880_cell_devs,
	.cell_nr = ARRAY_SIZE(pm880_cell_devs),
};
EXPORT_SYMBOL_GPL(pm880_cell_info);

static const struct reg_default pm880_base_patch[] = {
	{PM88X_WDOG, 0x1},	 /* disable watchdog */
	{PM88X_AON_CTRL2, 0x2a},  /* output 32kHZ from XO */
	{PM88X_BK_OSC_CTRL1, 0x0f}, /* OSC_FREERUN = 1, to lock FLL */
	{PM88X_LOWPOWER2, 0x20}, /* XO_LJ = 1, enable low jitter for 32kHZ */
	/* enable LPM for internal reference group in sleep */
	{PM88X_LOWPOWER4, 0xc0},
	{PM88X_BK_OSC_CTRL3, 0xc0}, /* set the duty cycle of charger DC/DC to max */
};

static const struct reg_default pm880_power_patch[] = {
};

static const struct reg_default pm880_gpadc_patch[] = {
	{PM88X_GPADC_CONFIG6, 0x03}, /* enable non-stop mode */
};

static const struct reg_default pm880_battery_patch[] = {
};

static const struct reg_default pm880_test_patch[] = {
};

/* 88pm880 chip itself related */
int pm880_apply_patch(struct pm88x_chip *chip)
{
	int ret, size;

	if (!chip || !chip->base_regmap || !chip->power_regmap ||
	    !chip->gpadc_regmap || !chip->battery_regmap ||
	    !chip->test_regmap)
		return -EINVAL;

	size = ARRAY_SIZE(pm880_base_patch);
	if (size == 0)
		goto power;
	ret = regmap_register_patch(chip->base_regmap, pm880_base_patch, size);
	if (ret < 0)
		return ret;

power:
	size = ARRAY_SIZE(pm880_power_patch);
	if (size == 0)
		goto gpadc;
	ret = regmap_register_patch(chip->power_regmap, pm880_power_patch, size);
	if (ret < 0)
		return ret;

gpadc:
	size = ARRAY_SIZE(pm880_gpadc_patch);
	if (size == 0)
		goto battery;
	ret = regmap_register_patch(chip->gpadc_regmap, pm880_gpadc_patch, size);
	if (ret < 0)
		return ret;
battery:
	size = ARRAY_SIZE(pm880_battery_patch);
	if (size == 0)
		goto test;
	ret = regmap_register_patch(chip->battery_regmap, pm880_battery_patch, size);
	if (ret < 0)
		return ret;

test:
	size = ARRAY_SIZE(pm880_test_patch);
	if (size == 0)
		goto out;
	ret = regmap_register_patch(chip->test_regmap, pm880_test_patch, size);
	if (ret < 0)
		return ret;
out:
	return 0;
}
EXPORT_SYMBOL_GPL(pm880_apply_patch);
