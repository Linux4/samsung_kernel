/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Base S5P FIMG2D resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/export.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fimg2d.h>
#include <mach/map.h>

#define S5P_PA_FIMG2D_OFFSET	0x02000000
#define S5P_PA_FIMG2D_3X	(S5P_PA_FIMG2D+S5P_PA_FIMG2D_OFFSET)

static void __iomem *sysreg_g2d;

static struct resource s5p_fimg2d_resource[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_FIMG2D, SZ_4K),
	[1] = DEFINE_RES_IRQ(IRQ_2D),
};

struct platform_device s5p_device_fimg2d = {
	.name		= "s5p-fimg2d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_fimg2d_resource),
	.resource	= s5p_fimg2d_resource
};

static struct fimg2d_platdata default_fimg2d_data __initdata = {
	.parent_clkname	= "mout_g2d0",
	.clkname	= "sclk_fimg2d",
	.gate_clkname	= "fimg2d",
	.clkrate	= 200 * MHZ,
};

void __init s5p_fimg2d_set_platdata(struct fimg2d_platdata *pd)
{
	struct fimg2d_platdata *npd;

	if (soc_is_exynos4210()) {
		s5p_fimg2d_resource[0].start = S5P_PA_FIMG2D_3X;
		s5p_fimg2d_resource[0].end = S5P_PA_FIMG2D_3X + SZ_4K - 1;
	}

	if (!pd)
		pd = &default_fimg2d_data;

	npd = s3c_set_platdata(pd, sizeof(struct fimg2d_platdata),
			&s5p_device_fimg2d);
	if (!npd)
		pr_err("%s: fail s3c_set_platdata()\n", __func__);
}

int g2d_cci_snoop_init(int ip_ver)
{
	if (ip_ver == IP_VER_G2D_5R)
		sysreg_g2d = ioremap(EXYNOS5260_PA_SYSREG_G2D, 0x1000);
	else if (ip_ver == IP_VER_G2D_5H)
		sysreg_g2d = ioremap(EXYNOS5430_PA_SYSREG_G2D, 0x1000);

	return 0;
}

void g2d_cci_snoop_remove(int ip_ver)
{
	if ((ip_ver == IP_VER_G2D_5R) ||
	    (ip_ver == IP_VER_G2D_5H))
		iounmap(sysreg_g2d);
}

int g2d_cci_snoop_control(int ip_ver,
		enum g2d_shared_val val, enum g2d_shared_sel sel)
{
	void __iomem *sysreg_con;
	void __iomem *sysreg_sel;
	unsigned int cfg;

	if ((val >= SHAREABLE_VAL_END) || (sel >= SHAREABLE_SEL_END)) {
		pr_err("g2d val or sel are out of range. val:%d, sel:%d\n"
				, val, sel);
		return -EINVAL;
	}
	pr_debug("[%s:%d] val:%d, sel:%d\n", __func__, __LINE__, val, sel);

	if (ip_ver == IP_VER_G2D_5H) {
		sysreg_con = sysreg_g2d + EXYNOS5430_G2D_USER_CON;

		cfg = readl(sysreg_con);
		cfg &= ~EXYNOS5430_G2DX_SHARED_VAL_MASK;
		cfg |= val << EXYNOS5430_G2DX_SHARED_VAL_SHIFT;

		cfg &= ~EXYNOS5430_G2DX_SHARED_SEL_MASK;
		cfg |= sel << EXYNOS5430_G2DX_SHARED_SEL_SHIFT;

		pr_debug("[%s:%d] sysreg_con :0x%p cfg:0x%x\n"
			,  __func__, __LINE__, sysreg_con, cfg);

		writel(cfg, sysreg_con);
	} else if (ip_ver == IP_VER_G2D_5R) {
		sysreg_con = sysreg_g2d + EXYNOS5260_G2D_USER_CON;
		sysreg_sel = sysreg_g2d + EXYNOS5260_G2D_AXUSER_SEL;

		if (sel == SHARED_G2D_SEL) {
			/* disable cci path */
			cfg = (EXYNOS5260_G2D_ARUSER_SEL |
				EXYNOS5260_G2D_AWUSER_SEL);
			writel(cfg, sysreg_con);
		}

		if (val == NON_SHAREABLE_PATH) {
			cfg = EXYNOS5260_G2D_SEL;
			writel(cfg, sysreg_sel);
		}
	}

	return 0;
}
