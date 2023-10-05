/*
 * s2mpb06.h - Header for Samsung s2mpb06 driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __S2MPB06_MFD_H__
#define __S2MPB06_MFD_H__
#include <linux/i2c.h>

#define MFD_DEV_NAME "s2mpb06"

#define S2MPB06_REG_INVALID (0xff)

enum s2mpb06_irq_source {
	OCP_INT = 0,
	TSD_INT,
	S2MPB06_IRQ_GROUP_NR,
};

enum s2mpb06_irq {
	/* LDO OCP */
	S2MPB06_LDO1_OCP_INT,
	S2MPB06_LDO2_OCP_INT,
	S2MPB06_LDO3_OCP_INT,
	S2MPB06_LDO4_OCP_INT,
	S2MPB06_LDO5_OCP_INT,
	S2MPB06_LDO6_OCP_INT,
	S2MPB06_LDO7_OCP_INT,

	/* TSD/TW  */
	S2MPB06_TW_FALL_INT,
	S2MPB06_TSD_INT,
	S2MPB06_TW_RISE_INT,

	S2MPB06_IRQ_NR,
};

struct s2mpb06_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct s2mpb06_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

	int num_regulators;
	int num_rdata;
	struct s2mpb06_regulator_data *regulators;
};

struct s2mpb06_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex i2c_lock;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[S2MPB06_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPB06_IRQ_GROUP_NR];

	struct s2mpb06_platform_data *pdata;
};

extern int s2mpb06_irq_init(struct s2mpb06_dev *s2mpb06);
extern void s2mpb06_irq_exit(struct s2mpb06_dev *s2mpb06);
extern int s2mpb06_irq_resume(struct s2mpb06_dev *s2mpb06);

/* S2MPB06 shared i2c API function */
extern int s2mpb06_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest);
extern int s2mpb06_bulk_read(struct i2c_client *i2c, uint8_t reg, int count,
				uint8_t *buf);
extern int s2mpb06_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value);
extern int s2mpb06_bulk_write(struct i2c_client *i2c, uint8_t reg, int count,
				uint8_t *buf);
extern int s2mpb06_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val,
				uint8_t mask);

#endif		/* __S2MPB06_MFD_H__ */
