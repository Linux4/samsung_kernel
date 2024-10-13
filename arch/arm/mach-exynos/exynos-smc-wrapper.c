/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * EXYNOS SMC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <mach/smc.h>

int exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
	int ret;

	if ((cmd & EXYNOS_SMC_CMD_MASK) == SMC_OEM_BASE)
		ret = _exynos_smc_oem_fc(cmd, arg1, arg2, arg3);
	else
		ret = _exynos_smc(cmd, arg1, arg2, arg3);

	return ret;
}
