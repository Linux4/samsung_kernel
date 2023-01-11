/*
 * 88pm8xx-debugfs.h - dump registes for Marvell PMIC
 * Copyright (C) 2014  Marvell Semiconductor Ltd.
 * Yi Zhang <yizhang@marvell.com>
 */

#ifndef __88PM8XX_DEBUGFS_H
#define __88PM8XX_DEBUGFS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#define PM800_REG_NUM		(0xf9)

#define PM80X_BASE_REG_NUM		0xf0
#define PM80X_POWER_REG_NUM		0x9b
#define PM80X_GPADC_REG_NUM		0xb6

extern ssize_t pm800_dump_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos);
extern ssize_t pm800_dump_write(struct file *file, const char __user *buff,
				size_t len, loff_t *ppos);
extern int pm800_dump_debugfs_init(struct pm80x_chip *chip);
extern void pm800_dump_debugfs_remove(struct pm80x_chip *chip);

#endif

