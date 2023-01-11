/*
 * 88pm8xx-config.h - chip and board specific configuration for Marvell PMIC
 * Copyright (C) 2014  Marvell Semiconductor Ltd.
 * Yi Zhang <yizhang@marvell.com>
 */

#ifndef __88PM8XX_CONFIG_H
#define __88PM8XX_CONFIG_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/of_device.h>

extern int pm800_init_config(struct pm80x_chip *chip, struct device_node *np);
extern void parse_powerup_down_log(struct pm80x_chip *chip);

#endif
