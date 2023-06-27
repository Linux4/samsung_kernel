/*
 * linux/arch/arm/mach-mmp/pm-eden.c
 *
 *
 * Copyright (C) 2011 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <asm/system_misc.h>

#ifdef CONFIG_SMP
void handle_coherency_maint_req(void *p)
{
	/* TODO - Add Coherency maintenance code
	   when SoC is available in Silicon */
	return;
}
#endif

static int __init eden_pm_init(void)
{
	/* TODO - PMU Initialization when SoC is available in Silicon */
	return 0;
}

postcore_initcall(eden_pm_init);
