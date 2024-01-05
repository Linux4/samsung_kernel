/* linux/arch/arm/plat-samsung/dev-smies.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com
 *
 * Base Samsung SoC SMIES resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <asm-generic/sizes.h>

#include <mach/map.h>
#include <mach/irqs.h>

#include <plat/devs.h>
#include <plat/cpu.h>

#include <plat/smies.h>

static struct resource s5p_smies_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS3_PA_SMIES, SZ_1K),
};

struct platform_device s5p_device_smies = {
	.name			= "s5p-smies",
	.id			= 0,
	.num_resources		= ARRAY_SIZE( s5p_smies_resource),
	.resource		=  s5p_smies_resource,
	.dev			= {
		.platform_data	= NULL,
	},
};

void __init s5p_smies_set_platdata(struct s5p_smies_platdata  *pd)
{
	s3c_set_platdata(pd, sizeof(struct s5p_smies_platdata ),
			&s5p_device_smies);
}
