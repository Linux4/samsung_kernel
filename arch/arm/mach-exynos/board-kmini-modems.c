/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <mach/pmu.h>
#include <asm/system.h>

#include <linux/platform_data/modem_v1.h>

#include "dev-modem-ss222.h"

static int __init init_modem(void)
{
	int err;
	mif_err("+++\n");

	mif_err("System Revision %d\n", system_rev);
#if 0
	if (system_rev == 0) {
		mif_err("[TEMP CODE] Modem On for TCXO at early time\n");
		exynos4_set_cp_power_onoff(CP_POWER_ON);
	}
#endif

	err = init_exynos_dev_modem_ss222(NULL, 0);
	if (err)
		goto error;

	mif_err("---\n");
	return 0;

error:
	mif_err("xxx\n");
	return err;
}
late_initcall(init_modem);

