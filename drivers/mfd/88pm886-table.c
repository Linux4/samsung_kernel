/*
 * Marvell 88PM886 specific setting
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
#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm886-reg.h>
#include <linux/mfd/88pm88x-reg.h>

#include "88pm88x.h"

#define PM886_BUCK_NAME		"88pm886-buck"
#define PM886_LDO_NAME		"88pm886-ldo"

static bool pm886_base_writeable_reg(struct device *dev, unsigned int reg)
{
	bool ret = false;

	switch (reg) {
	case 0x00:
	case 0x01:
	case 0x05 ... 0x08:
	case 0x0a ... 0x0d:
	case 0x14:
	case 0x15:
	case 0x17 ... 0x19:
	case 0x1d:
	case 0x1f ... 0x23:
	case 0x25:
	case 0x30 ... 0x33:
	case 0x36:
	case 0x38 ... 0x3b:
	case 0x40 ... 0x48:
	case 0x50 ... 0x5c:
	case 0x61 ... 0x6d:
	case 0x6f:
	case 0xce ... 0xef:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

static bool pm886_power_writeable_reg(struct device *dev, unsigned int reg)
{
	bool ret = false;

	switch (reg) {
	case 0x00 ... 0x03:
	case 0x06:
	case 0x08 ... 0x0a:
	case 0x0e ... 0x10:
	case 0x16:
	case 0x19:
	case 0x1a:
	case 0x20:
	case 0x21:
	case 0x23:
	case 0x26:
	case 0x27 ... 0x29:
	case 0x2c ... 0x2f:
	case 0x32:
	case 0x33:
	case 0x35:
	case 0x38:
	case 0x39:
	case 0x3b:
	case 0x3e:
	case 0x3f:
	case 0x41:
	case 0x44:
	case 0x45:
	case 0x47:
	case 0x4a:
	case 0x4b:
	case 0x4d:
	case 0x50:
	case 0x51:
	case 0x53:
	case 0x56:
	case 0x57:
	case 0x59:
	case 0x5c:
	case 0x5d:
	case 0x5f:
	case 0x62:
	case 0x63:
	case 0x65:
	case 0x68:
	case 0x69:
	case 0x6b:
	case 0x6e:
	case 0x6f:
	case 0x71:
	case 0x74:
	case 0x75:
	case 0x77:
	case 0x7a:
	case 0x7b:
	case 0x7d:
	case 0x9a ... 0xa8:
	case 0xac ... 0xb3:
	case 0xba ... 0xc1:
	case 0xc8 ... 0xcd:
	case 0xcf:
	case 0xd6 ... 0xdb:
	case 0xdd:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

static bool pm886_gpadc_writeable_reg(struct device *dev, unsigned int reg)
{
	bool ret = false;

	switch (reg) {
	case 0x00 ... 0x03:
	case 0x05 ... 0x08:
	case 0x0a ... 0x0e:
	case 0x13:
	case 0x14:
	case 0x18:
	case 0x1a:
	case 0x1b:
	case 0x20 ... 0x26:
	case 0x28:
	case 0x2a:
	case 0x2b:
	case 0x30 ... 0x34:
	case 0x38:
	case 0x3d:
	case 0x40 ... 0x43:
	case 0x46 ... 0x5d:
	case 0x80:
	case 0x81:
	case 0x84 ... 0x8b:
	case 0x90:
	case 0x91:
	case 0x94 ... 0x9b:
	case 0xa0:
	case 0xa1:
	case 0xa4 ... 0xad:
	case 0xb0 ... 0xb3:
	case 0xc0 ... 0xc7:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

static bool pm886_battery_writeable_reg(struct device *dev, unsigned int reg)
{
	bool ret = false;

	switch (reg) {
	case 0x00 ... 0x15:
	case 0x28 ... 0x31:
	case 0x34 ... 0x36:
	case 0x3e ... 0x40:
	case 0x42 ... 0x45:
	case 0x47:
	case 0x4a ... 0x51:
	case 0x53:
	case 0x54:
	case 0x58:
	case 0x5b:
	case 0x60 ... 0x63:
	case 0x65:
	case 0x6b ... 0x71:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

static bool pm886_test_writeable_reg(struct device *dev, unsigned int reg)
{
	return true;
}

const struct regmap_config pm886_base_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm886_base_i2c_regmap);

const struct regmap_config pm886_power_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm886_power_i2c_regmap);

const struct regmap_config pm886_gpadc_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm886_gpadc_i2c_regmap);

const struct regmap_config pm886_battery_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm886_battery_i2c_regmap);

const struct regmap_config pm886_test_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xfe,
};
EXPORT_SYMBOL_GPL(pm886_test_i2c_regmap);

static const struct resource buck_resources[] = {
	{
	.name = PM886_BUCK_NAME,
	},
};

static const struct resource ldo_resources[] = {
	{
	.name = PM886_LDO_NAME,
	},
};

const struct mfd_cell pm886_cell_devs[] = {
	CELL_DEV(PM886_BUCK_NAME, buck_resources, "marvell,88pm886-buck1", 0),
	CELL_DEV(PM886_BUCK_NAME, buck_resources, "marvell,88pm886-buck2", 1),
	CELL_DEV(PM886_BUCK_NAME, buck_resources, "marvell,88pm886-buck3", 2),
	CELL_DEV(PM886_BUCK_NAME, buck_resources, "marvell,88pm886-buck4", 3),
	CELL_DEV(PM886_BUCK_NAME, buck_resources, "marvell,88pm886-buck5", 4),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo1", 5),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo2", 6),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo3", 7),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo4", 8),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo5", 9),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo6", 10),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo7", 11),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo8", 12),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo9", 13),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo10", 14),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo11", 15),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo12", 16),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo13", 17),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo14", 18),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo15", 19),
	CELL_DEV(PM886_LDO_NAME, ldo_resources, "marvell,88pm886-ldo16", 20),
	CELL_DEV(PM88X_VIRTUAL_REGULATOR_NAME, vr_resources, "marvell,88pm886-buck1-slp", 21),
};
EXPORT_SYMBOL_GPL(pm886_cell_devs);

struct pmic_cell_info pm886_cell_info = {
	.cells   = pm886_cell_devs,
	.cell_nr = ARRAY_SIZE(pm886_cell_devs),
};
EXPORT_SYMBOL_GPL(pm886_cell_info);

static const struct reg_default pm886_base_patch[] = {
	{PM88X_WDOG, 0x1},	 /* disable watchdog */
	{PM88X_GPIO_CTRL1, 0x40}, /* gpio1: dvc    , gpio0: input   */
	{PM88X_GPIO_CTRL2, 0x00}, /*               , gpio2: input   */
	{PM88X_GPIO_CTRL3, 0x44}, /* dvc2          , dvc1           */
	{PM88X_GPIO_CTRL4, 0x00}, /* gpio5v_1:input, gpio5v_2: input*/
	{PM88X_AON_CTRL2, 0x2a},  /* output 32kHZ from XO */
	{PM88X_BK_OSC_CTRL1, 0x0f}, /* OSC_FREERUN = 1, to lock FLL */
	{PM88X_LOWPOWER2, 0x20}, /* XO_LJ = 1, enable low jitter for 32kHZ */
	{PM88X_LOWPOWER4, 0xc8}, /* OV_VSYS and UV_VSYS1 comparators on VSYS disabled, VSYS_OVER_TH : 5.6V */
#if defined(CONFIG_MACH_J1ACELTE)
	{PM88X_BK_OSC_CTRL3, 0xc8}, /* set the duty cycle of charger DC/DC to max */
#else
	{PM88X_BK_OSC_CTRL3, 0xc0}, /* set the duty cycle of charger DC/DC to max */
#endif
};

static const struct reg_default pm886_power_patch[] = {
};

static const struct reg_default pm886_gpadc_patch[] = {
	{PM88X_GPADC_CONFIG6, 0x03}, /* enable non-stop mode */
};

static const struct reg_default pm886_battery_patch[] = {
};

static const struct reg_default pm886_test_patch[] = {
};

/* 88pm886 chip itself related */
int pm886_apply_patch(struct pm88x_chip *chip)
{
	int ret, size;

	if (!chip || !chip->base_regmap || !chip->power_regmap ||
	    !chip->gpadc_regmap || !chip->battery_regmap ||
	    !chip->test_regmap)
		return -EINVAL;

	size = ARRAY_SIZE(pm886_base_patch);
	if (size == 0)
		goto power;
	ret = regmap_register_patch(chip->base_regmap, pm886_base_patch, size);
	if (ret < 0)
		return ret;

power:
	size = ARRAY_SIZE(pm886_power_patch);
	if (size == 0)
		goto gpadc;
	ret = regmap_register_patch(chip->power_regmap, pm886_power_patch, size);
	if (ret < 0)
		return ret;

gpadc:
	size = ARRAY_SIZE(pm886_gpadc_patch);
	if (size == 0)
		goto battery;
	ret = regmap_register_patch(chip->gpadc_regmap, pm886_gpadc_patch, size);
	if (ret < 0)
		return ret;
battery:
	size = ARRAY_SIZE(pm886_battery_patch);
	if (size == 0)
		goto test;
	ret = regmap_register_patch(chip->battery_regmap, pm886_battery_patch, size);
	if (ret < 0)
		return ret;

test:
	size = ARRAY_SIZE(pm886_test_patch);
	if (size == 0)
		goto out;
	ret = regmap_register_patch(chip->test_regmap, pm886_test_patch, size);
	if (ret < 0)
		return ret;
out:
	return 0;
}
EXPORT_SYMBOL_GPL(pm886_apply_patch);
