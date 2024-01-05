/*
 * Samsung SMIES driver
 *
 * Copyright (c) 2014-2015 Samsung Electronics Co., Ltd.
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#ifndef SAMSUMG_SMIES_H
#define SAMSUNG_SMIES_H

#include <plat/smies.h>
#include <plat/map-base.h>

#define SMIES_CLK_NAME		"smie"
#define SMIES_GAMMALUT_CNT	65
#define SYSREG_LCDBLK_CFG	(S3C_VA_SYS + 0x0210)
#define LCDBLK_CFG_FIMDBYPASS	(1 << 1)

enum SMIES_IF {
	SMIES_RGB_IF = 0,
	SMIES_I80_IF,
};

enum SMIES_STATE {
	SMIES_DISABLED = 0,	/* clock off, operation stopped */
	SMIES_ENABLED,		/* clock on, operation started */
	SMIES_INITIALIZED,	/* clock off, probed */
};

enum SMIES_MASK {
	SMIES_MASK_OFF = 0,
	SMIES_MASK_ON,
};

enum SMIES_GAMMA_CUV {
	SMIES_CUV_LINEAR = 0,
	SMIES_CUV_2_1,
	SMIES_CUV_2_2,
};

struct s5p_smies_device {
	spinlock_t slock;
	struct mutex mutex;
	struct clk *clock;
	struct device *dev;
	struct resource *res;
	void __iomem *reg_base;

	struct s5p_smies_platdata *pdata;

	enum SMIES_STATE state;
};

static inline
void smies_write(struct s5p_smies_device *smies, u32 reg_id, u32 value)
{
	writel(value, smies->reg_base + reg_id);
}

static inline
void smies_write_mask(struct s5p_smies_device *smies, u32 reg_id, u32 value, u32 mask)
{
	u32 old = readl(smies->reg_base + reg_id);
	value = (value & mask) | (old & ~mask);
	writel(value, smies->reg_base + reg_id);
}

static inline u32 smies_read(struct s5p_smies_device *smies, u32 reg_id)
{
	return readl(smies->reg_base + reg_id);
}

#endif /* SAMSUNG_SMIES_H */
