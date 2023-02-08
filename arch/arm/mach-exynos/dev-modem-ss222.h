/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Header for EXYNOS machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_EXYNOS_DEV_MODEM_SS222_H__
#define __ARCH_ARM_MACH_EXYNOS_DEV_MODEM_SS222_H__

#define MBREG_PDA_ACTIVE	6
#define MBREG_PHONE_ACTIVE	9
#define MBREG_HW_REVISION	33
#define MBREG_PMIC_REVISION	35
#define MBREG_PACKAGE_ID	37

#define REG_PA_PACKAGE_ID	0x10000004

int init_exynos_dev_modem_ss222(struct modem_io_t *devices, int num_of_devices);

#endif
