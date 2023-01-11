/*
 * Marvell 88PM880 Interface
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *  Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 88pm880 specific configuration: at present it's regulators and dvc part
 */

#ifndef __LINUX_MFD_88PM880_H
#define __LINUX_MFD_88PM880_H

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/reboot.h>
#include "88pm880-reg.h"

/* for chip stepping */
enum {
	PM880_A0 = 0xb0,
	PM880_A1 = 0xb1,
};

enum {
	PM880_ID_BUCK1A = 0,
	PM880_ID_BUCK2,
	PM880_ID_BUCK3,
	PM880_ID_BUCK4,
	PM880_ID_BUCK5,
	PM880_ID_BUCK6,
	PM880_ID_BUCK7,

	PM880_ID_BUCK_MAX = 7,
};

enum {
	PM880_ID_LDO1 = 0,
	PM880_ID_LDO2,
	PM880_ID_LDO3,
	PM880_ID_LDO4,
	PM880_ID_LDO5,
	PM880_ID_LDO6,
	PM880_ID_LDO7,
	PM880_ID_LDO8,
	PM880_ID_LDO9,
	PM880_ID_LDO10,
	PM880_ID_LDO11,
	PM880_ID_LDO12,
	PM880_ID_LDO13,
	PM880_ID_LDO14 = 13,
	PM880_ID_LDO15,
	PM880_ID_LDO16 = 15,

	PM880_ID_LDO17 = 16,
	PM880_ID_LDO18 = 17,

	PM880_ID_LDO_MAX = 18,
};

enum {
	PM880_ID_BUCK1A_SLP = 0,
	PM880_ID_BUCK1B_SLP,
};

enum {
	PM880_ID_BUCK1A_AUDIO = 0,
	PM880_ID_BUCK1B_AUDIO,
};

#endif /* __LINUX_MFD_88PM880_H */
