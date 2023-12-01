/*
 * s2dps01.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_S2DPS01_H
#define __LINUX_MFD_S2DPS01_H
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "s2dps01"

struct s2dps01_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex i2c_lock;
};

#define S2DPS01_REGS_VPOS	(0x1E)
#define S2DPS01_REGS_VNEG	(0x1F)
#define S2DPS01_DEF_VPOS_DATA	(0x70)
#define S2DPS01_DEF_VNEG_DATA	(0x0A)

/* S2DPS01 shared i2c API function */

#endif /*  __LINUX_MFD_S2DPS01_H */
