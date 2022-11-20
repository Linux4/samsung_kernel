/* linux/arch/arm/mach-exynos/rcg-exynos.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * EXYNOS5 - RCG(Root Clock Gating) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <plat/cpu.h>
#include <mach/rcg.h>

static struct rcg_domain *exynos_rcg_domain;
static unsigned int exynos_rcg_domain_count;

#if defined(CONFIG_SOC_EXYNOS5260)
static struct rcg_domain exynos5260_rcg_domain[] = {
	[RCG_PERI] = {
		.reg_base		= (void __iomem *)0x10223FDC,
	},
	[RCG_MIF] = {
		.reg_base		= (void __iomem *)0x10D03FDC,
		.use_xiu_top		= true,
		.use_xiu_us		= true,
		.value_xiu_top		= 0x1,
		.value_xiu_us		= 0x1,
	},
	[RCG_MFC] = {
		.reg_base		= (void __iomem *)0x110A3FDC,
		.use_xiu_top		= true,
		.value_xiu_top		= 0x1,
	},
	[RCG_KFC] = {
		.reg_base		= (void __iomem *)0x10713FDC,
	},
	[RCG_ISP] = {
		.reg_base		= (void __iomem *)0x133E3FDC,
		.use_xiu_async		= true,
		.value_xiu_async	= 0x3,
	},
	[RCG_GSCL] = {
		.reg_base		= (void __iomem *)0x13F23FDC,
		.use_xiu_top		= true,
		.use_xiu_async		= true,
		.value_xiu_top		= 0x7,
		.value_xiu_async	= 0x1,
	},
	[RCG_G3D] = {
		.reg_base		= (void __iomem *)0x11853FDC,
	},
	[RCG_G2D] = {
		.reg_base		= (void __iomem *)0x10A23FDC,
		.use_xiu_top		= true,
		.use_xiu_us		= true,
		.value_xiu_top		= 0x3,
		.value_xiu_us		= 0x1,
	},
	[RCG_FSYS] = {
		.reg_base		= (void __iomem *)0x122F3FDC,
		.use_xiu_top		= true,
		.use_xiu_us		= true,
		.value_xiu_top		= 0x1,
		.value_xiu_us		= 0x3,
	},
	[RCG_EGL] = {
		.reg_base		= (void __iomem *)0x10613FDC,
	},
	[RCG_DISP] = {
		.reg_base		= (void __iomem *)0x14543FDC,
		.use_xiu_top		= true,
		.use_xiu_us		= true,
		.use_xiu_async		= true,
		.value_xiu_top		= 0x7,
		.value_xiu_us		= 0x3,
		.value_xiu_async	= 0x3,
	},
	[RCG_AUD] = {
		.reg_base		= (void __iomem *)0x128F3FDC,
		.use_xiu_top		= true,
		.value_xiu_top		= 0x1
	},
};
#endif

int exynos_rcg_enable(enum RCG_DOMAIN domain)
{
	struct rcg_domain *domain_info;

	if (!exynos_rcg_domain ||
		domain < 0 || RCG_AUD < domain) {
		pr_err("RCG : Failed enabling rcg, index %d\n", domain);
		return -EINVAL;
	}

	domain_info = &exynos_rcg_domain[domain];

	if (domain_info->use_xiu_top)
		__raw_writel(domain_info->value_xiu_top, domain_info->reg_base);

	if (domain_info->use_xiu_us)
		__raw_writel(domain_info->value_xiu_us, domain_info->reg_base + 0x04);

	if (domain_info->use_xiu_async)
		__raw_writel(domain_info->value_xiu_async, domain_info->reg_base + 0x08);

	return 0;
}

static int __init exynos_rcg_init(void)
{
	struct rcg_domain *domain;
	int i;

	if (soc_is_exynos5260()) {
		exynos_rcg_domain = exynos5260_rcg_domain;
		exynos_rcg_domain_count = ARRAY_SIZE(exynos5260_rcg_domain);
	} else {
		pr_err("RCG : Did not support SoC!\n");
		return -ENODEV;
	}

	for (i = 0; i < exynos_rcg_domain_count; ++i) {
		domain = &exynos_rcg_domain[i];
		domain->reg_base = ioremap((unsigned long)domain->reg_base, SZ_16);
		if (!domain->reg_base) {
			pr_err("RCG : Failed allocation of domain, index %d\n", i);
			return -EINVAL;
		}

		exynos_rcg_enable(i);
	}

	printk("RCG : Initialization Successed\n");

	return 0;
}
core_initcall(exynos_rcg_init);
