/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT6681_H__
#define __MT6681_H__

#include <linux/regmap.h>

#define MT6681_PMIC_SLAVEID	(0x6B)

struct mt6681_pmic_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct regmap *regmap;
	unsigned int chip_rev;
	struct mutex io_lock;
};

#endif /* __MT6681_H__ */
