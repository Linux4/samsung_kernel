/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL3 monitor support
 * Author: Jang Hyunsung <hs79.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <linux/module.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-el3_mon.h>

int exynos_pd_tz_save(unsigned int addr)
{
	return exynos_smc(SMC_CMD_PREAPRE_PD_ONOFF,
				EXYNOS_GET_IN_PD_DOWN,
				addr,
				RUNTIME_PM_TZPC_GROUP);
}
EXPORT_SYMBOL(exynos_pd_tz_save);

int exynos_pd_tz_restore(unsigned int addr)
{
	return exynos_smc(SMC_CMD_PREAPRE_PD_ONOFF,
				EXYNOS_WAKEUP_PD_DOWN,
				addr,
				RUNTIME_PM_TZPC_GROUP);
}
EXPORT_SYMBOL(exynos_pd_tz_restore);

MODULE_SOFTDEP("pre: exynos-el3");
MODULE_LICENSE("GPL");
