/* linux/arch/arm/mach-exynos/clock-exynos5260.c
 *
 * Copyright (c) 2010-2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5260 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/pm.h>

#include <mach/map.h>
#include <mach/sysmmu.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/exynos-fimc-is.h>
#include <mach/regs-pmu.h>

#define clk_fin_egl_dpll clk_ext_xtal_mux
#define clk_fin_rpll clk_ext_xtal_mux

/* KPLL clock output */
struct clk clk_fout_kpll = {
	.name		= "fout_kpll",
	.id		= -1,
};

/* Possible clock sources for KPLL Mux */
static struct clk *clk_src_kpll_list[] = {
	[0] = &clk_fin_kpll,
	[1] = &clk_fout_kpll,
};

struct clksrc_sources clk_src_kpll = {
	.sources	= clk_src_kpll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_kpll_list),
};
/* EGL_DPLL clock output */
struct clk clk_fout_egl_dpll = {
	.name		= "fout_egl_dpll",
	.id		= -1,
};

/* Possible clock sources for EGL_DPLL Mux */
static struct clk *clk_src_egl_dpll_list[] = {
	[0] = &clk_fin_egl_dpll,
	[1] = &clk_fout_egl_dpll,
};

struct clksrc_sources clk_src_egl_dpll = {
	.sources	= clk_src_egl_dpll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_egl_dpll_list),
};

#define EXYNOS5260_PLL2650_MDIV_SHIFT 		(9)
#define EXYNOS5260_PLL2650_PDIV_SHIFT 		(3)
#define EXYNOS5260_PLL2650_SDIV_SHIFT 		(0)
#define EXYNOS5260_PLL2650_KDIV_SHIFT 		(0)
#define EXYNOS5260_PLL2650_MDIV_MASK 		(0x1ff)
#define EXYNOS5260_PLL2650_PDIV_MASK 		(0x3f)
#define EXYNOS5260_PLL2650_SDIV_MASK 		(0x7)
#define EXYNOS5260_PLL2650_K_MASK 		(0xffff)
#define EXYNOS5260_PLL2650_PLL_ENABLE_SHIFT 	(23)
#define EXYNOS5260_PLL2650_PLL_LOCKTIME_SHIFT 	(21)
#define EXYNOS5260_PLL2650_PLL_FOUTMASK_SHIFT 	(31)

#define EXYNOS5260_PLL2550_MDIV_MASK		(0x3FF)
#define EXYNOS5260_PLL2550_PDIV_MASK		(0x3F)
#define EXYNOS5260_PLL2550_SDIV_MASK		(0x7)
#define EXYNOS5260_PLL2550_MDIV_SHIFT		(9)
#define EXYNOS5260_PLL2550_PDIV_SHIFT		(3)
#define EXYNOS5260_PLL2550_SDIV_SHIFT		(0)
#define EXYNOS5260_PLL2550_PLL_ENABLE_SHIFT	(23)
#define EXYNOS5260_PLL2550_PLLCON0_LOCKED_SHIFT	(21)

static unsigned long xtal_rate;

struct pll_2550 {
	struct pll_div_data *pms_div;
	int pms_cnt;
	void __iomem *pll_con0;
	void __iomem *pll_lock;
	unsigned int force_locktime;
};

struct pll_2650 {
	struct vpll_div_data *pms_div;
	int pms_cnt;
	void __iomem *pll_con0;
	void __iomem *pll_con2;
	void __iomem *pll_lock;
};

#define PLL_2550(plldat, pllname, pmsdiv) \
	static struct pll_2550 plldat = { \
		.pms_div = pmsdiv, \
		.pms_cnt = ARRAY_SIZE(pmsdiv), \
		.pll_con0 = EXYNOS5260_ ## pllname ## _CON0, \
		.pll_lock = EXYNOS5260_ ## pllname ## _LOCK, \
		.force_locktime = 0 }

#define PLL_2650(plldat, pllname, pmsdiv) \
	static struct pll_2650 plldat = { \
		.pms_div = pmsdiv, \
		.pms_cnt = ARRAY_SIZE(pmsdiv), \
		.pll_con0 = EXYNOS5260_ ## pllname ## _CON0, \
		.pll_con2 = EXYNOS5260_ ## pllname ## _CON2, \
		.pll_lock = EXYNOS5260_ ## pllname ## _LOCK, }

#ifdef CONFIG_PM_SLEEP
static struct sleep_save exynos5260_clock_save[] = {
	SAVE_ITEM(EXYNOS5260_CLKDIV_MIF),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_HPM),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_GSCL_ISP0),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_G2D_MFC),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_PERI0),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_PERI1),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_PERI2),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_ISP10),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_ISP11),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_BUS),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_DISP),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_FSYS0),
	SAVE_ITEM(EXYNOS5260_CLKDIV_TOP_FSYS1),
	SAVE_ITEM(EXYNOS5260_CLKDIV_PERI),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_MIF),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_PLL0),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_GSCL),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_MFC),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_PERI1),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_G2D),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_ISP10),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_ISP11),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_BUS),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_DISP0),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_DISP1),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_TOP_FSYS),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_FSYS1),
	SAVE_ITEM(EXYNOS5260_CLKSRC_SEL_PERI1),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_TOP_FSYS),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_PERI1),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_TOP_G2D),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_TOP_GSCL),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_TOP_ISP10),
	SAVE_ITEM(EXYNOS5260_CLKSRC_ENABLE_TOP_PLL0),
	SAVE_ITEM(EXYNOS5260_CLKGATE_SCLK_FSYS),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_TOP),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_PERI2),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_PERI_SECURE_TOP_RTC),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_PERI1),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_PERI0),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_FSYS_SECURE_RTIC),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_FSYS_SECURE_SMMU_RTIC),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_G2D),
	SAVE_ITEM(EXYNOS5260_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS_PMU_DEBUG),
};

static struct sleep_save exynos5260_cpll_save[] = {
	SAVE_ITEM(EXYNOS5260_CPLL_LOCK),
	SAVE_ITEM(EXYNOS5260_CPLL_CON0),
};

static struct sleep_save exynos5260_mpll_save[] = {
	SAVE_ITEM(EXYNOS5260_MPLL_LOCK),
	SAVE_ITEM(EXYNOS5260_MPLL_CON0),
};

static struct sleep_save exynos5260_bpll_save[] = {
	SAVE_ITEM(EXYNOS5260_BPLL_LOCK),
	SAVE_ITEM(EXYNOS5260_BPLL_CON0),
};

static struct sleep_save exynos5260_dpll_save[] = {
	SAVE_ITEM(EXYNOS5260_DPLL_LOCK),
	SAVE_ITEM(EXYNOS5260_DPLL_CON0),
};

static struct sleep_save exynos5260_epll_save[] = {
	SAVE_ITEM(EXYNOS5260_EPLL_LOCK),
	SAVE_ITEM(EXYNOS5260_EPLL_CON0),
	SAVE_ITEM(EXYNOS5260_EPLL_CON1),
	SAVE_ITEM(EXYNOS5260_EPLL_CON2),
};
#endif



/*clock gating ctrl */
static int exynos5_clk_clkout_ctrl(struct clk *clk, int enable)
{
        return s5p_gatectrl(EXYNOS_PMU_DEBUG, clk, !enable);
}

static int exynos5_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKSRC_ENABLE_TOP_FSYS, clk, enable);
}

static int exynos5_clk_sclk_fsys(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_SCLK_FSYS, clk, enable);
}

static int exynos5_clk_sclk_disp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_SCLK_DISP0, clk, enable);
}

static int exynos5_clk_sclk_disp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_SCLK_DISP1, clk, enable);
}

static int exynos5_clk_mout_aud(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKSRC_ENABLE_AUD, clk, enable);
}

static int exynos5_clk_mout_peri1(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKSRC_ENABLE_PERI1, clk, enable);
}

static int exynos5_clk_ip_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_TOP, clk, enable);
}

static int exynos5_clk_ip_disp_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_DISP, clk, enable);
}

static int exynos5_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_MFC, clk, enable);
}

static int exynos5_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos5_clk_ip_fsys_secure_rtic(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_FSYS_SECURE_RTIC, clk, enable);
}

static int exynos5_clk_ip_fsys_secure_smmu_rtic(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_FSYS_SECURE_SMMU_RTIC, clk, enable);
}

static int exynos5_clk_ip_g2d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D, clk, enable);
}

static int exynos5_clk_ip_sss_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SSS_G2D, clk, enable);
}

static int exynos5_clk_ip_slimsss_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SLIM_SSS_G2D, clk, enable);
}

static int exynos5_clk_ip_g2d_secure_smmu_slim_sss_g2d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SLIM_SSS_G2D, clk, enable);
}

static int exynos5_clk_ip_g2d_secure_smmu_sss_g2d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_SSS_G2D, clk, enable);
}

static int exynos5_clk_ip_g2d_secure_smmu_g2d_g2d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G2D_SECURE_SMMU_G2D_G2D, clk, enable);
}

static int exynos5_clk_ip_g3d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_G3D, clk, enable);
}

static int exynos5_clk_ip_gscl_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL, clk, enable);
}

static int exynos5_clk_ip_gscl_fimc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL_FIMC, clk, enable);
}

static int exynos5_clk_ip_gscl_secure_smmu_gscl0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL0, clk, enable);
}

static int exynos5_clk_ip_gscl_secure_smmu_gscl1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_GSCL1, clk, enable);
}

static int exynos5_clk_ip_gscl_secure_smmu_mscl0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL0, clk, enable);
}

static int exynos5_clk_ip_gscl_secure_smmu_mscl1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_GSCL_SECURE_SMMU_MSCL1, clk, enable);
}

static int exynos5_clk_ip_isp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_ISP1, clk, enable);
}

static int exynos5_clk_ip_mfc_secure_smmu2_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_MFC_SECURE_SMMU2_MFC, clk, enable);
}

static int exynos5_clk_ip_peri0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_PERI0, clk, enable);
}

static int exynos5_clk_ip_peri1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_PERI1, clk, enable);
}

static int exynos5_clk_ip_peri2_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_PERI2, clk, enable);
}

static int exynos5_clk_ip_peri_secure_top_rtc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_PERI_SECURE_TOP_RTC, clk, enable);
}

/* External clock source */
/* CMU_MIF */
static struct clksrc_clk exynos5_clk_sclk_mem_pll = {
	.clk	= {
		.name		= "sclk_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5_clk_sclk_bus_pll = {
	.clk	= {
		.name		= "sclk_mpll",
	},
	.sources = &clk_src_mpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 4, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 8, .size = 3},
};

static struct clksrc_clk exynos5_clk_sclk_media_pll = {
	.clk	= {
		.name		= "sclk_cpll",
	},
	.sources = &clk_src_cpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_mif_drex_list[] = {
	[0] = &exynos5_clk_sclk_mem_pll.clk,
	[1] = &exynos5_clk_sclk_bus_pll.clk,
};

static struct clksrc_sources exynos5_clkset_mif_drex = {
	.sources	= exynos5_clk_mif_drex_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_mif_drex_list),
};

static struct clksrc_clk exynos5_clk_mout_mif_drex = {
	.clk	= {
		.name		= "mout_mif_drex",
	},
	.sources = &exynos5_clkset_mif_drex,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_mif_drex2x = {
	.clk	= {
		.name		= "mout_mif_drex2x",
	},
	.sources = &exynos5_clkset_mif_drex,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 20, .size = 3 },
};

static struct clk *exynos5_clk_clkm_phy_list[] = {
	[0] = &exynos5_clk_mout_mif_drex.clk,
	[1] = &exynos5_clk_sclk_media_pll.clk,
};

static struct clksrc_sources exynos5_clkset_clkm_phy = {
	.sources	= exynos5_clk_clkm_phy_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_clkm_phy_list),
};

static struct clksrc_clk exynos5_clk_sclk_clkm_phy = {
	.clk	= {
		.name		= "sclk_clkm_phy",
	},
	.sources = &exynos5_clkset_clkm_phy,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 16, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 12, .size = 3 },
};

static struct clk *exynos5_clk_clk2x_phy_list[] = {
	[0] = &exynos5_clk_mout_mif_drex2x.clk,
	[1] = &exynos5_clk_sclk_media_pll.clk,
};

static struct clksrc_sources exynos5_clkset_clk2x_phy = {
	.sources	= exynos5_clk_clk2x_phy_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_clk2x_phy_list),
};

static struct clksrc_clk exynos5_clk_aclk_clk2x_phy = {
	.clk	= {
		.name		= "aclk_clk2x_phy",
	},
	.sources = &exynos5_clkset_clk2x_phy,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MIF, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MIF, .shift = 24, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_aclk_mif_466 = {
	.clk	= {
		.name = "aclk_mif_466",
		.parent = &exynos5_clk_aclk_clk2x_phy.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 20, .size = 3},
};

static struct clksrc_clk exynos5_clk_aclk_bus_200 = {
	.clk	= {
		.name = "aclk_bus_200",
		.parent = &exynos5_clk_sclk_bus_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 24, .size = 3},
};

static struct clksrc_clk exynos5_clk_aclk_bus_100 = {
	.clk	= {
		.name = "aclk_bus_100",
		.parent = &exynos5_clk_sclk_bus_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MIF, .shift = 28, .size = 4},
};

/* CMU_TOP(Rhea) */
static struct clksrc_clk exynos5_clk_sclk_aud_pll = {
	.clk	= {
		.name		= "sclk_epll",
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos5_clk_sclk_disp_pll = {
	.clk	= {
		.name		= "sclk_dpll",
	},
	.sources = &clk_src_dpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 12, .size = 3 },
};

static struct clk *exynos5_clkset_sclk_memtop_pll_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_mem_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_memtop_pll = {
	.sources	= exynos5_clkset_sclk_memtop_pll_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_memtop_pll_list),
};

static struct clksrc_clk exynos5_clk_sclk_memtop_pll = {
	.clk	= {
		.name		= "sclk_memtop_pll",
	},
	.sources = &exynos5_clkset_sclk_memtop_pll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clkset_sclk_bustop_pll_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bus_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_bustop_pll = {
	.sources	= exynos5_clkset_sclk_bustop_pll_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_bustop_pll_list),
};

static struct clksrc_clk exynos5_clk_sclk_bustop_pll = {
	.clk	= {
		.name		= "sclk_bustop_pll",
	},
	.sources = &exynos5_clkset_sclk_bustop_pll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5_clk_sclk_hpm_targetclk = {
	.clk	= {
		.name		= "sclk_hpm_targetclk",
		.parent		= &exynos5_clk_sclk_bustop_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_HPM, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clkset_sclk_audtop_pll_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_aud_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_audtop_pll = {
	.sources	= exynos5_clkset_sclk_audtop_pll_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_audtop_pll_list),
};

static struct clksrc_clk exynos5_clk_sclk_audtop_pll = {
	.clk	= {
		.name		= "sclk_audtop_pll",
	},
	.sources = &exynos5_clkset_sclk_audtop_pll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 24, .size = 3 },
};

static struct clk *exynos5_clkset_sclk_mediatop_pll_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_media_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_mediatop_pll = {
	.sources	= exynos5_clkset_sclk_mediatop_pll_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_mediatop_pll_list),
};

static struct clksrc_clk exynos5_clk_sclk_mediatop_pll = {
	.clk	= {
		.name		= "sclk_mediatop_pll",
	},
	.sources = &exynos5_clkset_sclk_mediatop_pll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PLL0, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PLL0, .shift = 0, .size = 3 },
};

/* CMU_TOP(GSCL) */
static struct clk *exynos5_clk_gscaler_bustop_333_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_gscaler_bustop_333 = {
	.sources = exynos5_clk_gscaler_bustop_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_gscaler_bustop_333_list)
};

static struct clksrc_clk exynos5_clk_mout_gscaler_bustop_333 = {
	.clk = {
		.name = "mout_gscaler_bustop_333",
	},
	.sources = &exynos5_clkset_gscaler_bustop_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 8, .size = 3 },
};

static struct clk *exynos5_clk_aclk_gscl_333_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_gscaler_bustop_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscl_333 = {
	.sources = exynos5_clk_aclk_gscl_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_gscl_333_list)
};

static struct clksrc_clk exynos5_clk_aclk_gscl_333 = {
	.clk = {
		.name = "aclk_gscl_333",
	},
	.sources = &exynos5_clkset_aclk_gscl_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 12, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_m2m_media_400_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_m2m_media_400 = {
	.sources = exynos5_clk_m2m_media_400_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_m2m_media_400_list)
};

static struct clksrc_clk exynos5_clk_mout_m2m_media_400 = {
	.clk = {
		.name = "mout_m2m_media_400",
	},
	.sources = &exynos5_clkset_m2m_media_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_aclk_gscl_400_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_mout_m2m_media_400.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscl_400 = {
	.sources = exynos5_clk_aclk_gscl_400_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_gscl_400_list)
};

static struct clksrc_clk exynos5_clk_aclk_gscl_400 = {
	.clk = {
		.name = "aclk_gscl_400",
	},
	.sources = &exynos5_clkset_aclk_gscl_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 4, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_gscaler_bustop_fimc_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscaler_bustop_fimc = {
	.sources = exynos5_clk_aclk_gscaler_bustop_fimc_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_gscaler_bustop_fimc_list)
};

static struct clksrc_clk exynos5_clk_mout_gscaler_bustop_fimc = {
	.clk = {
		.name = "mout_gscaler_bustop_fimc",
	},
	.sources = &exynos5_clkset_aclk_gscaler_bustop_fimc,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 16, .size = 3 },
};

static struct clk *exynos5_clk_aclk_gscl_fimc_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_gscaler_bustop_fimc.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscl_fimc = {
	.sources = exynos5_clk_aclk_gscl_fimc_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_gscl_fimc_list)
};

static struct clksrc_clk exynos5_clk_aclk_gscl_fimc = {
	.clk = {
		.name = "aclk_gscl_fimc",
	},
	.sources = &exynos5_clkset_aclk_gscl_fimc,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_GSCL, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_GSCL, .shift = 20, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 8, .size = 3 },
};

/* CMU_TOP(MFC) */
static struct clk *exynos5_clk_mfc_bustop_333_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_mfc_bustop_333 = {
	.sources = exynos5_clk_mfc_bustop_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_mfc_bustop_333_list)
};

static struct clksrc_clk exynos5_clk_mout_mfc_bustop_333 = {
	.clk = {
		.name = "mout_mfc_bustop_333",
	},
	.sources = &exynos5_clkset_mfc_bustop_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_MFC, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_MFC, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_mfc_333_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_mfc_bustop_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_mfc_333 = {
	.sources = exynos5_clk_aclk_mfc_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_mfc_333_list)
};

static struct clksrc_clk exynos5_clk_aclk_mfc_333 = {
	.clk = {
		.name = "aclk_mfc_333",
	},
	.sources = &exynos5_clkset_aclk_mfc_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_MFC, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_MFC, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_G2D_MFC, .shift = 4, .size = 3 },
};

/* CMU_TOP(PERI) */
static struct clksrc_clk exynos5_clk_aclk_66_peri = {
	.clk    = {
		.name           = "aclk_peri_66",
		.parent         = &exynos5_clk_sclk_bustop_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI2, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_peri_aud = {
	.clk    = {
		.name           = "sclk_peri_aud",
		.parent         = &exynos5_clk_sclk_audtop_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI2, .shift = 24, .size = 3 },
};

static struct clk *exynos5_clk_sclk_peri_spi_spi_ext_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_peri_spi0_spi_ext_clk = {
	.sources = exynos5_clk_sclk_peri_spi_spi_ext_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_peri_spi_spi_ext_clk_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_peri_spi0_a_ratio = {
	.clk = {
		.name = "dout_sclk_spi0",
	},
	.sources = &exynos5_clkset_sclk_peri_spi0_spi_ext_clk,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI0, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_peri_spi0_spi_ext_clk = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos5_clk_dout_sclk_peri_spi0_a_ratio.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI0, .shift = 8, .size = 8 },
};

static struct clksrc_sources exynos5_clkset_sclk_peri_spi1_spi_ext_clk = {
	.sources = exynos5_clk_sclk_peri_spi_spi_ext_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_peri_spi_spi_ext_clk_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_peri_spi1_a_ratio = {
	.clk = {
		.name = "dout_sclk_spi1",
	},
	.sources = &exynos5_clkset_sclk_peri_spi1_spi_ext_clk,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 4, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI0, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_peri_spi1_spi_ext_clk = {
	.clk	= {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos5_clk_dout_sclk_peri_spi1_a_ratio.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI0, .shift = 20, .size = 8 },
};

static struct clksrc_sources exynos5_clkset_sclk_peri_spi2_spi_ext_clk = {
	.sources = exynos5_clk_sclk_peri_spi_spi_ext_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_peri_spi_spi_ext_clk_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_peri_spi2_a_ratio = {
	.clk = {
		.name = "dout_sclk_spi2",
	},
	.sources = &exynos5_clkset_sclk_peri_spi2_spi_ext_clk,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_peri_spi2_spi_ext_clk = {
	.clk = {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.2",
		.parent 	= &exynos5_clk_dout_sclk_peri_spi2_a_ratio.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI1, .shift = 4, .size = 8 },
};

static struct clk *exynos5_clk_sclk_peri_uart_ext_uclk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_peri_uart_ext_uclk = {
	.sources = exynos5_clk_sclk_peri_uart_ext_uclk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_peri_uart_ext_uclk_list),
};

/* CMU_TOP(G2D) */
static struct clk *exynos5_clk_g2d_bustop_333_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_g2d_bustop_333 = {
	.sources = exynos5_clk_g2d_bustop_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_g2d_bustop_333_list),
};

static struct clksrc_clk exynos5_clk_mout_g2d_bustop_333 = {
	.clk = {
		.name = "mout_g2d_bustop_333",
	},
	.sources = &exynos5_clkset_g2d_bustop_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_G2D, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_G2D, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_g2d_333_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_g2d_bustop_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_g2d_333 = {
	.sources = exynos5_clk_aclk_g2d_333_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_g2d_333_list),
};

static struct clksrc_clk exynos5_clk_aclk_g2d_333 = {
	.clk = {
		.name = "aclk_g2d_333",
	},
	.sources = &exynos5_clkset_aclk_g2d_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_G2D, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_G2D, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_G2D_MFC, .shift = 0, .size = 3 },
};

/* CMU_TOP(ISP) */
static struct clk *exynos5_clk_isp1_media_266_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_sclk_memtop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_isp1_media_266 = {
	.sources = exynos5_clk_isp1_media_266_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_isp1_media_266_list),
};

static struct clksrc_clk exynos5_clk_mout_isp1_media_266 = {
	.clk = {
		.name = "mout_isp1_media_266",
	},
	.sources = &exynos5_clkset_isp1_media_266,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP10, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP10, .shift = 16, .size = 3 },
};

static struct clk *exynos5_clk_aclk_isp1_266_lit[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_mout_isp1_media_266.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_isp1_266 = {
	.sources = exynos5_clk_aclk_isp1_266_lit,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_isp1_266_lit),
};

static struct clksrc_clk exynos5_clk_aclk_isp_266 = {
	.clk = {
		.name = "aclk_isp_266",
	},
	.sources = &exynos5_clkset_aclk_isp1_266,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP10, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP10, .shift = 20, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP10, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_isp1_media_400_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_sclk_disp_pll.clk,
};

static struct clksrc_sources exynos5_clkset_isp1_media_400 = {
	.sources = exynos5_clk_isp1_media_400_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_isp1_media_400_list),
};

static struct clksrc_clk exynos5_clk_mout_isp1_media_400 = {
	.clk = {
		.name = "mout_isp1_media_400",
	},
	.sources = &exynos5_clkset_isp1_media_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP10, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP10, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_isp1_400_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_mout_isp1_media_400.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_isp1_400 = {
	.sources = exynos5_clk_aclk_isp1_400_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_isp1_400_list)
};

static struct clksrc_clk exynos5_clk_aclk_isp_400 = {
	.clk = {
		.name = "aclk_isp_400",
	},
	.sources = &exynos5_clkset_aclk_isp1_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP10, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP10, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP10, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_sclk_isp1_spi_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp1_spi0 = {
	.sources = exynos5_clk_sclk_isp1_spi_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_isp1_spi_list)
};

static struct clksrc_clk exynos5_clk_dout_sclk_isp1_spi0_a = {
	.clk = {
		.name = "dout_sclk_isp1_spi0_a",
	},
	.sources = &exynos5_clkset_sclk_isp1_spi0,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 4, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP10, .shift = 12, .size = 4 },
};

static struct clksrc_sources exynos5_clkset_sclk_isp1_spi1 = {
	.sources = exynos5_clk_sclk_isp1_spi_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_isp1_spi_list)
};

static struct clksrc_clk exynos5_clk_dout_sclk_isp1_spi1_a = {
	.clk = {
		.name = "dout_sclk_isp1_spi1_a",
	},
	.sources = &exynos5_clkset_sclk_isp1_spi1,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_isp_spi0_spi_ext_clk = {
	.clk = {
		.name = "sclk_isp_spi0_spi_ext_clk",
		.parent = &exynos5_clk_dout_sclk_isp1_spi0_a.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP10, .shift = 16, .size = 8 },
};

static struct clksrc_clk exynos5_clk_sclk_isp_spi1_spi_ext_clk = {
	.clk = {
		.name = "sclk_isp_spi1_spi_ext_clk",
		.parent = &exynos5_clk_dout_sclk_isp1_spi1_a.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 4, .size = 8 },
};

static struct clksrc_clk exynos5_clk_sclk_peri_spi3_spi_ext_clk = {
	.clk = {
		.name		= "spi_busclk0",
		.devname	= "s3c64xx-spi.3",
		.parent 	= &exynos5_clk_sclk_isp_spi0_spi_ext_clk.clk
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP10, .shift = 16, .size = 8 },
};

static struct clk *exynos5_clk_sclk_isp1_uart_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp1_uart = {
	.sources = exynos5_clk_sclk_isp1_uart_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_isp1_uart_list)
};

static struct clksrc_clk exynos5_clk_sclk_isp_uart_ext_uclk = {
	.clk = {
		.name = "sclk_isp_uart_ext_uclk",
	},
	.sources = &exynos5_clkset_sclk_isp1_uart,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 12, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 12, .size = 4 },
};

static struct clk *exynos5_clk_sclk_isp1_sensor_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp1_sensor = {
	.sources = exynos5_clk_sclk_isp1_sensor_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_isp1_sensor_list)
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor0_a = {
	.clk = {
		.name = "dout_isp1_sensor0_a",
	},
	.sources = &exynos5_clkset_sclk_isp1_sensor,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 16, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor0_b = {
	.clk = {
		.name = "dout_isp1_sensor0_b",
		.parent = &exynos5_clk_dout_isp1_sensor0_a.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor1_a = {
	.clk = {
		.name = "dout_isp1_sensor1_a",
	},
	.sources = &exynos5_clkset_sclk_isp1_sensor,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 20, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor1_b = {
	.clk = {
		.name = "dout_isp1_sensor1_b",
		.parent = &exynos5_clk_dout_isp1_sensor1_a.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor2_a = {
	.clk = {
		.name = "dout_isp1_sensor2_a",
	},
	.sources = &exynos5_clkset_sclk_isp1_sensor,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_ISP11, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_ISP11, .shift = 24, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_GSCL_ISP0, .shift = 24, .size = 4 },
};

static struct clksrc_clk exynos5_clk_dout_isp1_sensor2_b = {
	.clk = {
		.name = "dout_isp1_sensor2_b",
		.parent = &exynos5_clk_dout_isp1_sensor2_a.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_ISP11, .shift = 24, .size = 4 },
};

/* CMU_TOP(BUS1~4) */
static struct clk *exynos5_clk_bus_bustop_list[] = {
	[0] = &exynos5_clk_sclk_bustop_pll.clk,
	[1] = &exynos5_clk_sclk_memtop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_bus1_bustop_400 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus1d_400 = {
	.clk = {
		.name = "aclk_bus1d_400",
	},
	.sources = &exynos5_clkset_bus1_bustop_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 0, .size = 3 },
};

static struct clksrc_sources exynos5_clkset_bus1_bustop_100 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus1p_100 = {
	.clk = {
		.name = "aclk_bus1p_100",
	},
	.sources = &exynos5_clkset_bus1_bustop_100,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 4, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 4, .size = 4 },
};

static struct clksrc_sources exynos5_clkset_bus2_bustop_400 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus2d_400 = {
	.clk = {
		.name = "aclk_bus2d_400",
	},
	.sources = &exynos5_clkset_bus2_bustop_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 12, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 8, .size = 3 },
};

static struct clksrc_sources exynos5_clkset_bus2_bustop_100 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus2p_100 = {
	.clk = {
		.name = "aclk_bus2p_100",
	},
	.sources = &exynos5_clkset_bus2_bustop_100,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 12, .size = 4 },
};

static struct clksrc_sources exynos5_clkset_bus3_bustop_400 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus3d_400 = {
	.clk = {
		.name = "aclk_bus3d_400",
	},
	.sources = &exynos5_clkset_bus3_bustop_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 16, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 16, .size = 3 },
};

static struct clksrc_sources exynos5_clkset_bus3_bustop_100 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus3p_100 = {
	.clk = {
		.name = "aclk_bus3p_100",
	},
	.sources = &exynos5_clkset_bus3_bustop_100,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 20, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 20, .size = 4 },
};

static struct clksrc_sources exynos5_clkset_bus4_bustop_400 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus4d_400 = {
	.clk = {
		.name = "aclk_bus4d_400",
	},
	.sources = &exynos5_clkset_bus4_bustop_400,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 24, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 24, .size = 3 },
};

static struct clksrc_sources exynos5_clkset_bus4_bustop_100 = {
	.sources = exynos5_clk_bus_bustop_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_bus_bustop_list)
};

static struct clksrc_clk exynos5_clk_aclk_bus4p_100 = {
	.clk = {
		.name = "aclk_bus4p_100",
	},
	.sources = &exynos5_clkset_bus4_bustop_100,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_BUS, .shift = 28, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_BUS, .shift = 28, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_BUS, .shift = 28, .size = 4 },
};

/* CMU_TOP(DISP) */
static struct clk *exynos5_clkset_mout_disp_disp_list[] = {
	[0] = &exynos5_clk_sclk_disp_pll.clk,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_mout_disp_disp = {
	.sources	= exynos5_clkset_mout_disp_disp_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_disp_disp_list),
};

static struct clksrc_clk exynos5_clk_mout_disp_disp_333 = {
	.clk	= {
		.name		= "mout_disp_disp_333",
	},
	.sources = &exynos5_clkset_mout_disp_disp,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP0, .shift = 0, .size = 1 },
};

static struct clk *exynos5_clkset_aclk_disp_333_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_disp_disp_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_disp_333 = {
	.sources	= exynos5_clkset_aclk_disp_333_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_disp_333_list),
};

static struct clksrc_clk exynos5_clk_aclk_disp_333 = {
	.clk	= {
		.name		= "aclk_disp_333",
	},
	.sources = &exynos5_clkset_aclk_disp_333,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP0, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_DISP0, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_DISP, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_disp_disp_222 = {
	.clk	= {
		.name		= "mout_disp_disp_222",
	},
	.sources = &exynos5_clkset_mout_disp_disp,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP0, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_DISP0, .shift = 12, .size = 3 },
};

static struct clk *exynos5_clkset_aclk_disp_222_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_mout_disp_disp_222.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_disp_222 = {
	.sources	= exynos5_clkset_aclk_disp_222_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_disp_222_list),
};

static struct clksrc_clk exynos5_clk_aclk_disp_222 = {
	.clk	= {
		.name		= "aclk_disp_222",
	},
	.sources = &exynos5_clkset_aclk_disp_222,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP0, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_DISP0, .shift = 20, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_DISP, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clkset_mout_disp_media_pixel_list[] = {
	[0] = &exynos5_clk_sclk_mediatop_pll.clk,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_mout_disp_media_pixel = {
	.sources	= exynos5_clkset_mout_disp_media_pixel_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_disp_media_pixel_list),
};

static struct clksrc_clk exynos5_clk_mout_disp_media_pixel = {
	.clk	= {
		.name		= "mout_disp_media_pixel",
	},
	.sources = &exynos5_clkset_mout_disp_media_pixel,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP1, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_DISP1, .shift = 8, .size = 3 },
};

static struct clk *exynos5_clkset_sclk_disp_pixel_list[] = {
	[0] = &exynos5_clk_sclk_disp_pll.clk,
	[1] = &exynos5_clk_mout_disp_media_pixel.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_disp_pixel = {
	.sources	= exynos5_clkset_sclk_disp_pixel_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_sclk_disp_pixel_list),
};

static struct clksrc_clk exynos5_clk_sclk_disp_pixel = {
	.clk	= {
		.name		= "sclk_disp_pixel",
	},
	.sources = &exynos5_clkset_sclk_disp_pixel,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_DISP1, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_DISP1, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_DISP, .shift = 8, .size = 3 },
};

/* CMU_TOP(FSYS) */
static struct clksrc_clk exynos5_clk_aclk_fsys_200 = {
	.clk	= {
		.name		= "aclk_fsys_200",
		.parent		= &exynos5_clk_sclk_bustop_pll.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS0, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clkset_mout_sclk_fsys_mmc_sdclkin_a_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_fsys_mmc_sdclkin_a = {
	.sources	= exynos5_clkset_mout_sclk_fsys_mmc_sdclkin_a_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_sclk_fsys_mmc_sdclkin_a_list),
};

static struct clksrc_clk exynos5_clk_mout_sclk_fsys_mmc0_sdclkin_a = {
	.clk	= {
		.name		= "mout_sclk_fsys_mmc0_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc_sdclkin_a,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 20, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 20, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_sclk_fsys_mmc1_sdclkin_a = {
	.clk	= {
		.name		= "mout_sclk_fsys_mmc1_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc_sdclkin_a,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 12, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_sclk_fsys_mmc2_sdclkin_a = {
	.clk	= {
		.name		= "mout_sclk_fsys_mmc2_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc_sdclkin_a,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clkset_mout_sclk_fsys_mmc0_sdclkin_b_list[] = {
	[0] = &exynos5_clk_mout_sclk_fsys_mmc0_sdclkin_a.clk,
	[1] = &exynos5_clk_sclk_mediatop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_fsys_mmc0_sdclkin_b = {
	.sources	= exynos5_clkset_mout_sclk_fsys_mmc0_sdclkin_b_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_sclk_fsys_mmc0_sdclkin_b_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_fsys_mmc0_sdclkin_a = {
	.clk	= {
		.name		= "dout_sclk_fsys_mmc0_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc0_sdclkin_b,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 24, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS0, .shift = 12, .size = 4 },
};

static struct clk *exynos5_clkset_mout_sclk_fsys_mmc1_sdclkin_b_list[] = {
	[0] = &exynos5_clk_mout_sclk_fsys_mmc1_sdclkin_a.clk,
	[1] = &exynos5_clk_sclk_mediatop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_fsys_mmc1_sdclkin_b = {
	.sources	= exynos5_clkset_mout_sclk_fsys_mmc1_sdclkin_b_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_sclk_fsys_mmc1_sdclkin_b_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_fsys_mmc1_sdclkin_a = {
	.clk	= {
		.name		= "dout_sclk_fsys_mmc1_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc1_sdclkin_b,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 16, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 16, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS1, .shift = 0, .size = 4 },
};

static struct clk *exynos5_clkset_mout_sclk_fsys_mmc2_sdclkin_b_list[] = {
	[0] = &exynos5_clk_mout_sclk_fsys_mmc2_sdclkin_a.clk,
	[1] = &exynos5_clk_sclk_mediatop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_fsys_mmc2_sdclkin_b = {
	.sources	= exynos5_clkset_mout_sclk_fsys_mmc2_sdclkin_b_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_sclk_fsys_mmc2_sdclkin_b_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_fsys_mmc2_sdclkin_a = {
	.clk	= {
		.name		= "dout_sclk_fsys_mmc2_sdclkin_a",
	},
	.sources = &exynos5_clkset_sclk_fsys_mmc2_sdclkin_b,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 8, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS1, .shift = 12, .size = 4 },
};

static struct clk *exynos5_clkset_mout_sclk_fsys_usb_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_bustop_pll.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_fsys_usb = {
	.sources	= exynos5_clkset_mout_sclk_fsys_usb_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_mout_sclk_fsys_usb_list),
};

static struct clksrc_clk exynos5_clk_dout_sclk_fsys_usbdrd30_suspend_clk = {
	.clk	= {
		.name		= "dout_sclk_fsys_usbdrd30_suspend_clk",
	},
	.sources = &exynos5_clkset_sclk_fsys_usb,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_FSYS, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_FSYS, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS0, .shift = 4, .size = 4 },
};

/* CMU_FSYS */
static struct clk exynos5_clk_phyclk_usbhost20_phy_freeclk = {
	.name	= "phyclk_usbhost20_phy_freeclk",
	.id	= -1,
	.rate	= 60000000,
};

static struct clk exynos5_clk_phyclk_usbdrd30_udrd30_phyclock = {
	.name	= "phyclk_usbdrd30_udrd30_phyclock",
	.id	= -1,
	.rate	= 60000000,
};

static struct clk exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk = {
	.name	= "phyclk_usbdrd30_udrd30_pipe_pclk",
	.id	= -1,
	.rate	= 125000000,
};

static struct clk exynos5_clk_phyclk_usbhost20_phy_phyclock = {
	.name	= "phyclk_usbhost20_phy_phyclock",
	.id	= -1,
	.rate	= 60000000,
};

static struct clk exynos5_clk_phyclk_usbhost20_phy_clk48mohci = {
	.name	= "phyclk_usbhost20_phy_clk48mohci",
	.id	= -1,
	.rate	= 48000000,
};

static struct clk *exynos5_clk_phyclk_usbhost20_phy_freeclk_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_usbhost20_phy_freeclk,
};

static struct clksrc_sources exynos5_clkset_phyclk_usbhost20_phy_freeclk_user = {
	.sources = exynos5_clk_phyclk_usbhost20_phy_freeclk_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_usbhost20_phy_freeclk_user_list),
};

static struct clk *exynos5_clk_phyclk_usbdrd30_udrd30_phyclock_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_usbdrd30_udrd30_phyclock,
};

static struct clksrc_sources exynos5_clkset_phyclk_usbdrd30_udrd30_phyclock_user = {
	.sources = exynos5_clk_phyclk_usbdrd30_udrd30_phyclock_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_usbdrd30_udrd30_phyclock_user_list),
};

static struct clk *exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk,
};

static struct clksrc_sources exynos5_clkset_phyclk_usbdrd30_udrd30_pipe_pclk_user = {
	.sources = exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk_user_list),
};

static struct clk *exynos5_clk_phyclk_usbhost20_phy_phyclock_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_usbhost20_phy_phyclock,
};

static struct clksrc_sources exynos5_clkset_phyclk_usbhost20_phy_phyclock_user = {
	.sources = exynos5_clk_phyclk_usbhost20_phy_phyclock_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_usbhost20_phy_phyclock_user_list),
};

static struct clk *exynos5_clk_phyclk_usbhost20_phy_clk48mohci_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_usbhost20_phy_clk48mohci,
};

static struct clksrc_sources exynos5_clkset_phyclk_usbhost20_phy_clk48mohci_user = {
	.sources = exynos5_clk_phyclk_usbhost20_phy_clk48mohci_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_usbhost20_phy_clk48mohci_user_list),
};

/* CMU_DISP */
static struct clk exynos5_clk_phyclk_dptx_pyh_clk_div2 = {
	.name	= "phyclk_dptx_phy_clk_div2",
	.id	= -1,
	.rate	= 135000000,
};

static struct clk exynos5_clk_phyclk_dptx_pyh_ch0_txd_clk = {
	.name	= "phyclk_dptx_phy_ch0_txd_clk",
	.id	= -1,
	.rate	= 270000000,
};

static struct clk exynos5_clk_phyclk_dptx_pyh_ch1_txd_clk = {
	.name	= "phyclk_dptx_phy_ch1_txd_clk",
	.id	= -1,
	.rate	= 270000000,
};

static struct clk exynos5_clk_phyclk_dptx_pyh_ch2_txd_clk = {
	.name	= "phyclk_dptx_phy_ch2_txd_clk",
	.id	= -1,
	.rate	= 270000000,
};

static struct clk exynos5_clk_phyclk_dptx_pyh_ch3_txd_clk = {
	.name	= "phyclk_dptx_phy_ch3_txd_clk",
	.id	= -1,
	.rate	= 270000000,
};

static struct clk exynos5_clk_phyclk_dptx_pyh_o_ref_clk_24m = {
	.name	= "phyclk_dptx_phy_o_ref_clk_24m",
	.id	= -1,
	.rate	= 24000000,
};

static struct clk exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs = {
	.name	= "phyclk_mipi_dphy_4l_m_txbyteclkhs",
	.id	= -1,
	.rate	= 187500000,
};

static struct clk exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0 = {
	.name	= "phyclk_mipi_dphy_4l_m_rxclkesc0",
	.id	= -1,
	.rate	= 20000000,
};

static struct clk exynos5_clk_phyclk_hdmi_phy_tmds_clko = {
	.name	= "phyclk_hdmi_phy_tmds_clko",
	.id	= -1,
	.rate	= 250000000,
};

static struct clk exynos5_clk_sclk_dsim1_txclkesc0 = {
	.name	= "sclk_dsim1_txclkesc0",
	.id	= -1,
	.rate	= 37500000,
};

static struct clk exynos5_clk_sclk_dsim1_txclkesc1 = {
	.name	= "sclk_dsim1_txclkesc1",
	.id	= -1,
	.rate	= 37500000,
};

static struct clk exynos5_clk_sclk_dsim1_txclkesc2 = {
	.name	= "sclk_dsim1_txclkesc2",
	.id	= -1,
	.rate	= 37500000,
};

static struct clk exynos5_clk_sclk_dsim1_txclkesc3 = {
	.name	= "sclk_dsim1_txclkesc3",
	.id	= -1,
	.rate	= 37500000,
};

static struct clk exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi = {
	.name	= "phyclk_hdmi_link_o_tmds_clkhi",
	.id	= -1,
	.rate	= 125000000,
};

static struct clk exynos5_clk_ioclk_spdif_extclk = {
	.name	= "ioclk_spdif_extclk",
	.id	= -1,
	.rate	= 49152000,
};

static struct clk exynos5_clk_phyclk_hdmi_phy_ref_cko = {
	.name	= "phyclk_hdmi_phy_ref_cko",
	.id	= -1,
	.rate	= 24000000,
};

static struct clk exynos5_clk_sclk_dsim1_txclkescclk = {
	.name	= "sclk_dsim1_txclkescclk",
	.id	= -1,
	.rate	= 37500000,
};

static struct clk exynos5_clk_phyclk_hdmi_phy_pixel_clko = {
	.name	= "phyclk_hdmi_phy_pixel_clko",
	.id	= -1,
	.rate	= 166000000,
};

static struct clk *exynos5_clk_phyclk_dptx_link_i_clk_div2_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_clk_div2,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_link_i_clk_div2 = {
	.sources = exynos5_clk_phyclk_dptx_link_i_clk_div2_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_link_i_clk_div2_list),
};

static struct clk *exynos5_clk_phyclk_dptx_phy_ch0_txd_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_ch0_txd_clk,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_phy_ch0_txd_clk = {
	.sources = exynos5_clk_phyclk_dptx_phy_ch0_txd_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_phy_ch0_txd_clk_list),
};

static struct clk *exynos5_clk_phyclk_dptx_phy_ch1_txd_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_ch1_txd_clk,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_phy_ch1_txd_clk = {
	.sources = exynos5_clk_phyclk_dptx_phy_ch1_txd_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_phy_ch1_txd_clk_list),
};

static struct clk *exynos5_clk_phyclk_dptx_phy_ch2_txd_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_ch2_txd_clk,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_phy_ch2_txd_clk = {
	.sources = exynos5_clk_phyclk_dptx_phy_ch2_txd_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_phy_ch2_txd_clk_list),
};

static struct clk *exynos5_clk_phyclk_dptx_phy_ch3_txd_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_ch3_txd_clk,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_phy_ch3_txd_clk = {
	.sources = exynos5_clk_phyclk_dptx_phy_ch3_txd_clk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_phy_ch3_txd_clk_list),
};

static struct clk *exynos5_clk_phyclk_dptx_phy_o_ref_clk_24m_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_dptx_pyh_o_ref_clk_24m,
};

static struct clksrc_sources exynos5_clkset_phyclk_dptx_phy_o_ref_clk_24m = {
	.sources = exynos5_clk_phyclk_dptx_phy_o_ref_clk_24m_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_dptx_phy_o_ref_clk_24m_list),
};

static struct clk *exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs,
};

static struct clksrc_sources exynos5_clkset_phyclk_mipi_dphy_4l_m_txbyteclkhs = {
	.sources = exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs_list),
};

static struct clk *exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0,
};

static struct clksrc_sources exynos5_clkset_phyclk_mipi_dphy_4l_m_rxclkesc0 = {
	.sources = exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0_list),
};

static struct clk *exynos5_clk_sclk_dsim1_txclkesc0_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_dsim1_txclkesc0,
};

static struct clksrc_sources exynos5_clkset_sclk_dsim1_txclkesc0 = {
	.sources = exynos5_clk_sclk_dsim1_txclkesc0_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_dsim1_txclkesc0_list),
};

static struct clk *exynos5_clk_sclk_dsim1_txclkesc1_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_dsim1_txclkesc1,
};

static struct clksrc_sources exynos5_clkset_sclk_dsim1_txclkesc1 = {
	.sources = exynos5_clk_sclk_dsim1_txclkesc1_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_dsim1_txclkesc1_list),
};

static struct clk *exynos5_clk_sclk_dsim1_txclkesc2_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_dsim1_txclkesc2,
};

static struct clksrc_sources exynos5_clkset_sclk_dsim1_txclkesc2 = {
	.sources = exynos5_clk_sclk_dsim1_txclkesc2_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_dsim1_txclkesc2_list),
};

static struct clk *exynos5_clk_sclk_dsim1_txclkesc3_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_dsim1_txclkesc3,
};

static struct clksrc_sources exynos5_clkset_sclk_dsim1_txclkesc3 = {
	.sources = exynos5_clk_sclk_dsim1_txclkesc3_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_dsim1_txclkesc3_list),
};

static struct clk *exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi,
};

static struct clksrc_sources exynos5_clkset_phyclk_hdmi_link_o_tmds_clkhi = {
	.sources = exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi_list),
};

static struct clk *exynos5_clk_mout_sclk_hdmi_spdif_list[] = {
	[0] = &exynos5_clk_ioclk_spdif_extclk,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_peri_aud.clk,
	[3] = &exynos5_clk_phyclk_hdmi_phy_ref_cko,
};

static struct clksrc_sources exynos5_clkset_mout_sclk_hdmi_spdif = {
	.sources = exynos5_clk_mout_sclk_hdmi_spdif_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_mout_sclk_hdmi_spdif_list),
};

static struct clk *exynos5_clk_sclk_dsim1_txclkescclk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_dsim1_txclkescclk,
};

static struct clksrc_sources exynos5_clkset_sclk_dsim1_txclkescclk = {
	.sources = exynos5_clk_sclk_dsim1_txclkescclk_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_dsim1_txclkescclk_list),
};

static struct clk *exynos5_clk_phyclk_hdmi_phy_pixel_clko_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_hdmi_phy_pixel_clko,
};

static struct clksrc_sources exynos5_clkset_phyclk_hdmi_phy_pixel_clko = {
	.sources = exynos5_clk_phyclk_hdmi_phy_pixel_clko_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_hdmi_phy_pixel_clko_list),
};

static struct clk *exynos5_clk_aclk_disp_333_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_disp_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_disp_333_user = {
	.sources = exynos5_clk_aclk_disp_333_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_disp_333_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_disp_333_user = {
	.clk = {
		.name = "aclk_disp_333_nogate",
	},
	.sources = &exynos5_clkset_aclk_disp_333_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_DISP0, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_aclk_disp_222_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_disp_222.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_disp_222_user = {
	.sources = exynos5_clk_aclk_disp_222_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_aclk_disp_222_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_disp_222_user = {
	.clk = {
		.name = "aclk_disp_222_nogate",
	},
	.sources = &exynos5_clkset_aclk_disp_222_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_DISP0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_disp_111 = {
	.clk = {
		.name = "pclk_disp_111_nogate",
		.parent = &exynos5_clk_aclk_disp_222_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_DISP, .shift = 8, .size = 4 },
};

static struct clk *exynos5_clk_phyclk_hdmi_phy_tmds_clko_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_hdmi_phy_tmds_clko,
};
static struct clksrc_sources exynos5_clkset_phyclk_hdmi_phy_tmds_clko = {
	.sources = exynos5_clk_phyclk_hdmi_phy_tmds_clko_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_hdmi_phy_tmds_clko_list),
};

static struct clksrc_clk exynos5_clk_phyclk_hdmi_link_i_tmds_clk = {
	.clk = {
		.name = "phyclk_hdmi_link_i_tmds_clk_nogate",
	},
	.sources = &exynos5_clkset_phyclk_hdmi_phy_tmds_clko,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 28, .size = 1 },
};

static struct clk *exynos5_clk_sclk_disp_pixel_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_disp_pixel.clk
};

static struct clksrc_sources exynos5_clkset_sclk_disp_pixel_user = {
	.sources = exynos5_clk_sclk_disp_pixel_user_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_disp_pixel_user_list),
};

static struct clksrc_clk exynos5_clk_mout_sclk_disp_pixel_user = {
	.clk = {
		.name = "sclk_disp_pixel_user",
	},
	.sources = &exynos5_clkset_sclk_disp_pixel_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_DISP0, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_phyclk_hdmi_phy_ref_cko_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_phyclk_hdmi_phy_ref_cko,
};

static struct clksrc_sources exynos5_clkset_phyclk_hdmi_phy_ref_cko = {
	.sources = exynos5_clk_phyclk_hdmi_phy_ref_cko_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_phyclk_hdmi_phy_ref_cko_list),
};

static struct clksrc_clk exynos5_clk_phyclk_hdmi_phy_ref_cko_24m = {
	.clk = {
		.name = "phyclk_hdmi_phy_ref_cko_24m",
		.enable = exynos5_clk_sclk_disp0_ctrl,
		.ctrlbit = (1 << 11),
	},
	.sources = &exynos5_clkset_phyclk_hdmi_phy_ref_cko,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 24, .size = 1 },
};

static struct clk *exynos5_clk_sclk_hdmi_pixel_list[] = {
	[0] = &exynos5_clk_mout_sclk_disp_pixel_user.clk,
	[1] = &exynos5_clk_aclk_disp_222_user.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_hdmi_pixel = {
	.sources = exynos5_clk_sclk_hdmi_pixel_list,
	.nr_sources = ARRAY_SIZE(exynos5_clk_sclk_hdmi_pixel_list),
};

/* CMU_G3D */
static struct clk *exynos5_clkset_aclk_g3d_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources exynos5_clkset_aclk_g3d = {
	.sources	= exynos5_clkset_aclk_g3d_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_aclk_g3d_list),
};

static struct clksrc_clk exynos5_clk_aclk_g3d = {
	.clk	= {
		.name		= "aclk_g3d",
	},
	.sources = &exynos5_clkset_aclk_g3d,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_G3D, .shift = 0, .size = 1},
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_G3D, .shift = 0, .size = 3 },
	.reg_div = { .reg = EXYNOS5260_CLKDIV_G3D, .shift = 4, .size = 3},
};

static struct clksrc_clk exynos5_clk_pclk_g3d_150 = {
	.clk	= {
		.name		= "pclk_g3d_150_nogate",
		.parent		= &exynos5_clk_aclk_g3d.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_G3D, .shift = 0, .size = 3},
};

/* CMU_G2D */
static struct clk *exynos5_clk_aclk_g2d_333_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_g2d_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_g2d_333_user = {
	.sources	= exynos5_clk_aclk_g2d_333_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_g2d_333_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_g2d_333_user = {
	.clk	= {
		.name		= "aclk_g2d_333_nogate",
	},
	.sources = &exynos5_clkset_aclk_g2d_333_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_G2D, .shift = 0, .size = 1},
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_G2D, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_g2d_83 = {
	.clk	= {
		.name		= "pclk_g2d_83_nogate",
		.parent		= &exynos5_clk_aclk_g2d_333_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_G2D, .shift = 0, .size = 3},
};

/* CMU_EGL */
static struct clksrc_clk exynos5_clk_mout_egl_pll = {
	.clk    = {
		.name	= "mout_egl_pll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_EGL, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_EGL, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5_clk_mout_egl_dpll = {
	.clk	= {
		.name		= "mout_egl_dpll",
	},
	.sources = &clk_src_egl_dpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_EGL, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_EGL, .shift = 0, .size = 3 },
};
static struct clk *exynos5_clk_mout_egl_a_list[] = {
	[0] = &exynos5_clk_mout_egl_pll.clk,
	[1] = &exynos5_clk_mout_egl_dpll.clk,
};

static struct clksrc_sources exynos5_clkset_mout_egl_a = {
	.sources	= exynos5_clk_mout_egl_a_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_mout_egl_a_list),
};

static struct clksrc_clk exynos5_clk_mout_egl_a = {
	.clk	= {
		.name           = "mout_egl_a",
	},
	.sources = &exynos5_clkset_mout_egl_a,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_EGL, .shift = 12, .size = 1},
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_EGL, .shift = 12, .size = 3 },
};

static struct clk *exynos5_clk_mout_egl_b_list[] = {
	[0] = &exynos5_clk_mout_egl_a.clk,
	[1] = &exynos5_clk_sclk_bus_pll.clk,
	};

static struct clksrc_sources exynos5_clkset_mout_egl_b = {
	.sources	= exynos5_clk_mout_egl_b_list,
	.nr_sources     = ARRAY_SIZE(exynos5_clk_mout_egl_b_list),
};

static struct clksrc_clk exynos5_clk_mout_egl_b = {
	.clk    = {
		.name		= "mout_egl_b",
	},
	.sources = &exynos5_clkset_mout_egl_b,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_EGL, .shift = 16, .size = 1},
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_EGL, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_egl1 = {
	.clk    = {
		.name		= "dout_egl1",
		.parent		= &exynos5_clk_mout_egl_b.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_egl2 = {
	.clk	= {
		.name		= "dout_egl2",
		.parent		= &exynos5_clk_dout_egl1.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5_clk_sclk_egl_pll = {
	.clk	= {
		.name		= "sclk_egl_pll",
		.parent		= &exynos5_clk_mout_egl_b.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 24, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_aclk_egl = {
	.clk	= {
		.name		= "dout_aclk_egl",
		.parent		= &exynos5_clk_dout_egl2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_atclk_egl = {
	.clk	= {
		.name		= "dout_atclk_egl",
		.parent		= &exynos5_clk_dout_egl2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_pclk_egl = {
	.clk	= {
		.name		= "dout_pclk_egl",
		.parent		= &exynos5_clk_dout_atclk_egl.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_pclk_dbg_egl = {
	.clk	= {
		.name		= "dout_pclk_dbg_egl",
		.parent		= &exynos5_clk_dout_atclk_egl.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_EGL, .shift = 20, .size = 3 },
};

/* CMU_KFC */
static struct clksrc_clk exynos5_clk_mout_kfc_pll = {
	.clk    = {
		.name		= "mout_kfc_pll",
	},
	.sources = &clk_src_kpll,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_KFC0, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_KFC0, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clkset_kfc_list[] = {
	[0] = &exynos5_clk_mout_kfc_pll.clk,
	[1] = &exynos5_clk_sclk_media_pll.clk,
};

static struct clksrc_sources exynos5_clkset_kfc = {
	.sources	= exynos5_clkset_kfc_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clkset_kfc_list),
};

static struct clksrc_clk exynos5_clk_mout_kfc = {
	.clk	= {
		.name		= "mout_kfc",
	},
	.sources = &exynos5_clkset_kfc,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_KFC2, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_KFC2, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_kfc1 = {
	.clk	= {
		.name		= "dout_kfc1",
		.parent		= &exynos5_clk_mout_kfc.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 0, .size = 3},
};

static struct clksrc_clk exynos5_clk_dout_kfc2 = {
	.clk	= {
		.name		= "dout_kfc2",
		.parent		= &exynos5_clk_dout_kfc1.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_atclk_kfc = {
	.clk	= {
		.name		= "dout_atclk_kfc",
		.parent		= &exynos5_clk_dout_kfc2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_pclk_dbg_kfc = {
	.clk	= {
		.name		= "dout_pclk_dbg_kfc",
		.parent		= &exynos5_clk_dout_kfc2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_aclk_kfc = {
	.clk	= {
		.name		= "dout_aclk_kfc",
		.parent		= &exynos5_clk_dout_kfc2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_pclk_kfc = {
	.clk	= {
		.name		= "dout_pclk_kfc",
		.parent		= &exynos5_clk_dout_kfc2.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 20, .size = 3 },
};

static struct clksrc_clk exynos5_clk_sclk_kfc_pll = {
	.clk	= {
		.name		= "sclk_kfc_pll",
		.parent		= &exynos5_clk_mout_kfc.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_KFC, .shift = 24, .size = 3 },
};

/* CMU_MFC */
static struct clk *exynos5_clk_aclk_mfc_333_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_mfc_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_mfc_333_user = {
	.sources	= exynos5_clk_aclk_mfc_333_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_mfc_333_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_mfc_333_user = {
	.clk    = {
		.name		= "aclk_mfc_333_user",
	},
	.sources = &exynos5_clkset_aclk_mfc_333_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_MFC, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_MFC, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_mfc_83 = {
	.clk    = {
		.name		= "pclk_mfc_83",
		.parent		= &exynos5_clk_aclk_mfc_333_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_MFC, .shift = 0, .size = 3 },
};

/* CMU_GSCL */
static struct clk *exynos5_clk_aclk_gscaler_333_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_gscl_333.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscaler_333_user = {
	.sources	= exynos5_clk_aclk_gscaler_333_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_gscaler_333_user_list),
};

static struct clk *exynos5_clk_aclk_m2m_400_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_gscl_400.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_m2m_400_user = {
	.sources	= exynos5_clk_aclk_m2m_400_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_m2m_400_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_m2m_400 = {
	.clk    = {
		.name		= "aclk_m2m_400",
	},
	.sources = &exynos5_clkset_aclk_m2m_400_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_GSCL, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_GSCL, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_gscl_gimc_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_gscl_fimc.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_gscl_gimc_user = {
	.sources	= exynos5_clk_aclk_gscl_gimc_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_gscl_gimc_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_gscl_fimc_user = {
	.clk    = {
		.name		= "aclk_gscl_fimc_user",
	},
	.sources = &exynos5_clkset_aclk_gscl_gimc_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_GSCL, .shift = 8, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_GSCL, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos5_clk_dout_aclk_csis_200 = {
	.clk    = {
		.name		= "dout_aclk_csis_200",
		.parent		= &exynos5_clk_aclk_m2m_400.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_GSCL, .shift = 4, .size = 3 },
};

static struct clk *exynos5_clk_aclk_csis_list[] = {
	[0] = &exynos5_clk_dout_aclk_csis_200.clk,
	[1] = &exynos5_clk_aclk_gscl_fimc_user.clk,
};

static struct clksrc_sources exynos5_clkset_aclk_csis = {
	.sources	= exynos5_clk_aclk_csis_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aclk_csis_list),
};

static struct clksrc_clk exynos5_clk_sclk_csis = {
	.clk    = {
		.name		= "sclk_csis",
	},
	.sources = &exynos5_clkset_aclk_csis,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_GSCL, .shift = 24, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_GSCL, .shift = 24, .size = 3 },
};

/* For CLKOUT */
static struct clk *exynos5_clkset_clk_clkout_list[] = {
        /* Others are for debugging */
        [16] = &clk_xxti,
};

static struct clksrc_sources exynos5_clkset_clk_clkout = {
        .sources        = exynos5_clkset_clk_clkout_list,
        .nr_sources     = ARRAY_SIZE(exynos5_clkset_clk_clkout_list),
};

static struct clksrc_clk exynos5_clk_clkout = {
        .clk    = {
                .name           = "clkout",
                .enable         = exynos5_clk_clkout_ctrl,
                .ctrlbit        = (1 << 0),
        },
        .sources = &exynos5_clkset_clk_clkout,
        .reg_src = { .reg = EXYNOS_PMU_DEBUG, .shift = 8, .size = 5 },
};

/* CMU_AUD */
static struct clk exynos5_clk_ioclk_audiocdlck0 = {
	.name	= "ioclk_audiocdlck0",
	.id	= -1,
};

static struct clk exynos5_clk_ioclk_pcm_extclk = {
	.name	= "ioclk_pcm_extclk",
	.id	= -1,
};

static struct clk *exynos5_clk_aud_pll_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_aud_pll.clk,
};

static struct clksrc_sources exynos5_clkset_aud_pll_user = {
	.sources	= exynos5_clk_aud_pll_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_aud_pll_user_list),
};

static struct clksrc_clk exynos5_clk_mout_aud_pll_user = {
	.clk    = {
		.name		= "mout_aud_pll_user",
		.enable		= exynos5_clk_mout_aud,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos5_clkset_aud_pll_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_AUD, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_sclk_aud_i2s_list[] = {
	[0] = &exynos5_clk_mout_aud_pll_user.clk,
	[1] = &exynos5_clk_ioclk_audiocdlck0,
};

static struct clksrc_sources exynos5_clkset_sclk_aud_i2s = {
	.sources	= exynos5_clk_sclk_aud_i2s_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_aud_i2s_list),
};

static struct clk *exynos5_clk_sclk_aud_pcm_list[] = {
	[0] = &exynos5_clk_mout_aud_pll_user.clk,
	[1] = &exynos5_clk_ioclk_pcm_extclk,
};

static struct clksrc_sources exynos5_clkset_sclk_aud_pcm = {
	.sources	= exynos5_clk_sclk_aud_pcm_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_aud_pcm_list),
};

/* CMU_ISP */
static struct clk *exynos5_clk_sclk_isp_spi1_spi_ext_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_isp_spi1_spi_ext_clk.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp_spi1_spi_ext_clk = {
	.sources	= exynos5_clk_sclk_isp_spi1_spi_ext_clk_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_isp_spi1_spi_ext_clk_list),
};

static struct clk *exynos5_clk_sclk_isp_spi0_spi_ext_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_isp_spi0_spi_ext_clk.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp_spi0_spi_ext_clk = {
	.sources	= exynos5_clk_sclk_isp_spi0_spi_ext_clk_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_isp_spi0_spi_ext_clk_list),
};

static struct clk *exynos5_clk_sclk_isp_uart_ext_uclk_clk_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_sclk_isp_uart_ext_uclk.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_isp_uart_ext_uclk_clk = {
	.sources	= exynos5_clk_sclk_isp_uart_ext_uclk_clk_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_isp_uart_ext_uclk_clk_list),
};

static struct clk *exynos5_clk_isp_266_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_isp_266.clk,
};

static struct clksrc_sources exynos5_clkset_isp_266_user = {
	.sources	= exynos5_clk_isp_266_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_isp_266_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_isp_266_user =  {
	.clk    = {
		.name		= "aclk_isp_266_user",
	},
	.sources = &exynos5_clkset_isp_266_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_ISP0, .shift = 0, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_ISP0, .shift = 0, .size = 3 },
};

static struct clk *exynos5_clk_isp_400_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos5_clk_aclk_isp_400.clk,
};

static struct clksrc_sources exynos5_clkset_isp_400_user = {
	.sources	= exynos5_clk_isp_400_user_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_isp_400_user_list),
};

static struct clksrc_clk exynos5_clk_aclk_ca5_clkin =  {
	.clk    = {
		.name		= "aclk_ca5_clkin",
	},
	.sources = &exynos5_clkset_isp_400_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_ISP0, .shift = 4, .size = 1 },
	.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_ISP0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_isp_133 =  {
	.clk    = {
		.name		= "pclk_isp_133",
		.parent		= &exynos5_clk_aclk_isp_266_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_ISP, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_isp_66 =  {
	.clk    = {
		.name		= "pclk_isp_66",
		.parent		= &exynos5_clk_aclk_isp_266_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_ISP, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos5_clk_sclk_mpwm_isp =  {
	.clk    = {
		.name		= "sclk_mpwm_isp",
		.parent		= &exynos5_clk_pclk_isp_66.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_ISP, .shift = 20, .size = 2 },
};

static struct clksrc_clk exynos5_clk_aclk_ca5_atclkin =  {
	.clk    = {
		.name		= "aclk_ca5_atclkin",
		.parent		= &exynos5_clk_aclk_ca5_clkin.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_ISP, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos5_clk_pclk_ca5_pclkdbg =  {
	.clk    = {
		.name		= "pclk_ca5_pclkdbg",
		.parent		= &exynos5_clk_aclk_ca5_clkin.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_ISP, .shift = 16, .size = 4 },
};

/* CMU_PERI */
static struct clk exynos5_clk_ioclk_i2s_cdclk = {
	.name	= "ioclk_i2s_cdclk",
	.id	= -1,
};

static struct clk *exynos5_clk_sclk_pcm1_pcm_sclk_in_list[] = {
	[0] = &exynos5_clk_ioclk_pcm_extclk,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_peri_aud.clk,
	[3] = &exynos5_clk_phyclk_hdmi_phy_ref_cko_24m.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_pcm1_pcm_sclk_in = {
	.sources	= exynos5_clk_sclk_pcm1_pcm_sclk_in_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_pcm1_pcm_sclk_in_list),
};

static struct clk *exynos5_clk_sclk_sclk_i2scodclk_list[] = {
	[0] = &exynos5_clk_ioclk_i2s_cdclk,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_peri_aud.clk,
	[3] = &exynos5_clk_phyclk_hdmi_phy_ref_cko_24m.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_sclk_i2scodclk = {
	.sources	= exynos5_clk_sclk_sclk_i2scodclk_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_sclk_i2scodclk_list),
};

static struct clk *exynos5_clk_sclk_spdif_i_mclk_int_list[] = {
	[0] = &exynos5_clk_ioclk_spdif_extclk,
	[1] = &clk_ext_xtal_mux,
	[2] = &exynos5_clk_sclk_peri_aud.clk,
	[3] = &exynos5_clk_phyclk_hdmi_phy_ref_cko_24m.clk,
};

static struct clksrc_sources exynos5_clkset_sclk_spdif_i_mclk_int = {
	.sources	= exynos5_clk_sclk_spdif_i_mclk_int_list,
	.nr_sources	= ARRAY_SIZE(exynos5_clk_sclk_spdif_i_mclk_int_list),
};

static struct clksrc_clk exynos5_clksrcs[] = {
	{
		.clk	= {
			.name		= "clk_uart_baud0",
			.devname	= "exynos4210-uart.0",
			.enable		= exynos5_clk_ip_peri2_ctrl,
			.ctrlbit	= (1 << 19),
		},
		.sources = &exynos5_clkset_sclk_peri_uart_ext_uclk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 20, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 20, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI1, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "clk_uart_baud0",
			.devname	= "exynos4210-uart.1",
			.enable		= exynos5_clk_ip_peri2_ctrl,
			.ctrlbit	= (1 << 20),
		},
		.sources = &exynos5_clkset_sclk_peri_uart_ext_uclk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 12, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 12, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI1, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "clk_uart_baud0",
			.devname	= "exynos4210-uart.2",
			.enable		= exynos5_clk_ip_peri2_ctrl,
			.ctrlbit	= (1 << 21),
		},
		.sources = &exynos5_clkset_sclk_peri_uart_ext_uclk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_TOP_PERI1, .shift = 16, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_TOP_PERI1, .shift = 16, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI1, .shift = 20, .size = 4 },
	}, {
		.clk    = {
			.name		= "sclk_dwmci", /*sclk_fsys_mmc0_sdclkin*/
			.devname	= "dw_mmc.0",
			.parent		= &exynos5_clk_dout_sclk_fsys_mmc0_sdclkin_a.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 24),
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS0, .shift = 16, .size = 8 },
	}, {
		.clk    = {
			.name		= "sclk_dwmci", /*sclk_fsys_mmc1_sdclkin*/
			.devname	= "dw_mmc.1",
			.parent		= &exynos5_clk_dout_sclk_fsys_mmc1_sdclkin_a.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS1, .shift = 4, .size = 8 },
	}, {
		.clk    = {
			.name		= "sclk_dwmci", /*sclk_fsys_mmc2_sdclkin*/
			.devname	= "dw_mmc.2",
			.parent		= &exynos5_clk_dout_sclk_fsys_mmc2_sdclkin_a.clk,
			.enable		= exynos5_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_FSYS1, .shift = 16, .size = 8 },
	}, {
		.clk	= {
			.name		= "aclk_peri_aud",
			.parent		= &exynos5_clk_sclk_audtop_pll.clk,
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_TOP_PERI2, .shift = 24, .size = 3 },
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_clk_div2",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 6),
		},
		.sources = &exynos5_clkset_phyclk_dptx_link_i_clk_div2,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 0, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_ch0_txd_clk",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 1),
		},
		.sources = &exynos5_clkset_phyclk_dptx_phy_ch0_txd_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 16, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_ch1_txd_clk",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 2),
		},
		.sources = &exynos5_clkset_phyclk_dptx_phy_ch1_txd_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 20, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_ch2_txd_clk",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 3),
		},
		.sources = &exynos5_clkset_phyclk_dptx_phy_ch2_txd_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 24, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_ch3_txd_clk",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos5_clkset_phyclk_dptx_phy_ch3_txd_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP0, .shift = 28, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_dptx_link_i_clk_24m",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 5),
		},
		.sources = &exynos5_clkset_phyclk_dptx_phy_o_ref_clk_24m,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 4, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_dsim1_bitclkdiv8",
		},
		.sources = &exynos5_clkset_phyclk_mipi_dphy_4l_m_txbyteclkhs,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 8, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_dsim1_rxclkesc0",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos5_clkset_phyclk_mipi_dphy_4l_m_rxclkesc0,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 0, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_mipi_dphy_4l_m_txclkesc0",
			.enable		= exynos5_clk_sclk_disp1_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos5_clkset_sclk_dsim1_txclkesc0,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 12, .size = 1 },
	}, {
		.clk	= {

			.name		= "sclk_mipi_dphy_4l_m_txclkesc1",
			.enable		= exynos5_clk_sclk_disp1_ctrl,
			.ctrlbit	= (1 << 1),
		},
		.sources = &exynos5_clkset_sclk_dsim1_txclkesc1,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 16, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_mipi_dphy_4l_m_txclkesc2",
			.enable		= exynos5_clk_sclk_disp1_ctrl,
			.ctrlbit	= (1 << 2),
		},
		.sources = &exynos5_clkset_sclk_dsim1_txclkesc2,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 20, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_mipi_dphy_4l_m_txclkesc3",
			.enable		= exynos5_clk_sclk_disp1_ctrl,
			.ctrlbit	= (1 << 3),
		},
		.sources = &exynos5_clkset_sclk_dsim1_txclkesc3,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 24, .size = 1 },
	}, {
		.clk	= {
			.name		= "phyclk_hdmi_phy_tmds_clkhi",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos5_clkset_phyclk_hdmi_link_o_tmds_clkhi,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 16, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_hdmi_link_i_spdif_clk",
			.enable		= exynos5_clk_sclk_disp0_ctrl,
			.ctrlbit	= (1 << 27),
		},
		.sources = &exynos5_clkset_mout_sclk_hdmi_spdif,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP4, .shift = 4, .size = 2 },
	}, {
		.clk	= {
			.name		= "sclk_mipi_dphy_4l_m_txclkescclk",
			.enable		= exynos5_clk_sclk_disp1_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos5_clkset_sclk_dsim1_txclkescclk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 28, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_hdmi_link_i_pixel_clk_nogate",
		},
		.sources = &exynos5_clkset_phyclk_hdmi_phy_pixel_clko,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP1, .shift = 20, .size = 1 },
	}, {
		.clk	= {
			.name		= "sclk_fimd1_128_extclkpl",
			.parent		= &exynos5_clk_mout_sclk_disp_pixel_user.clk
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_DISP, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_hdmi_phy_pixel_clk",
		},
		.sources = &exynos5_clkset_sclk_hdmi_pixel,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_DISP2, .shift = 4, .size = 1 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_DISP, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "dout_pclk_g3d",
			.parent		= &exynos5_clk_aclk_g3d.clk,
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_G3D, .shift = 0, .size = 3},
	}, {
		.clk	= {
			.name		= "phyclk_usbhost20_phy_freeclk_user",
			.enable		= exynos5_clk_sclk_fsys,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos5_clkset_phyclk_usbhost20_phy_freeclk_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_FSYS1, .shift = 12, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_usbdrd30_udrd30_phyclock_user",
			.enable		= exynos5_clk_sclk_fsys,
			.ctrlbit	= (1 << 7),
		},
		.sources = &exynos5_clkset_phyclk_usbdrd30_udrd30_phyclock_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_FSYS1, .shift = 0, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_usbdrd30_udrd30_pipe_pclk_user",
			.enable		= exynos5_clk_sclk_fsys,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos5_clkset_phyclk_usbdrd30_udrd30_pipe_pclk_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_FSYS1, .shift = 4, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_usbhost20_phy_phyclock_user",
			.enable		= exynos5_clk_sclk_fsys,
			.ctrlbit	= (1 << 1),
		},
		.sources = &exynos5_clkset_phyclk_usbhost20_phy_phyclock_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_FSYS1, .shift = 16, .size = 1},
	}, {
		.clk	= {
			.name		= "phyclk_usbhost20_phy_clk48mohci_user",
			.enable		= exynos5_clk_sclk_fsys,
			.ctrlbit	= (1 << 6),
		},
		.sources = &exynos5_clkset_phyclk_usbhost20_phy_clk48mohci_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_FSYS1, .shift = 8, .size = 1},
	}, {
		.clk	= {
			.name		= "aclk_gscaler_333",
		},
		.sources = &exynos5_clkset_aclk_gscaler_333_user,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_GSCL, .shift = 0, .size = 1},
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_GSCL, .shift = 0, .size = 3 },
	}, {
		.clk	= {
			.name		= "pclk_m2m_100",
			.parent         = &exynos5_clk_aclk_m2m_400.clk,
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_GSCL, .shift = 0, .size = 3},
	}, {
		.clk	= {
			.name		= "sclk_aud_i2s",
		},
		.sources = &exynos5_clkset_sclk_aud_i2s,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 4, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_AUD, .shift = 4, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_aud_pcm",
		},
		.sources = &exynos5_clkset_sclk_aud_pcm,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 8, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_AUD, .shift = 8, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_aud_uart",
			.parent         = &exynos5_clk_mout_aud_pll_user.clk,
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "aclk_aud_131",
			.parent         = &exynos5_clk_mout_aud_pll_user.clk,
		},
		.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_isp_spi1_spi_ext_clk",
		},
		.sources = &exynos5_clkset_sclk_isp_spi1_spi_ext_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_ISP1, .shift = 12, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_ISP1, .shift = 12, .size = 3 },
	}, {
		.clk	= {
			.name		= "sclk_isp_spi0_spi_ext_clk",
		},
		.sources = &exynos5_clkset_sclk_isp_spi0_spi_ext_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_ISP1, .shift = 8, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_ISP1, .shift = 8, .size = 3 },
	}, {
		.clk	= {
			.name		= "sclk_isp_uart_ext_uclk_user",
		},
		.sources = &exynos5_clkset_sclk_isp_uart_ext_uclk_clk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_ISP1, .shift = 16, .size = 1 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_ISP1, .shift = 16, .size = 3 },
	}, {
		.clk	= {
			.name		= "sclk_pcm1_pcm_sclk_in",
		},
		.sources = &exynos5_clkset_sclk_pcm1_pcm_sclk_in,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_PERI1, .shift = 4, .size = 2 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_PERI1, .shift = 4, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_PERI, .shift = 0, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_i2s",	/* sclk_i2scodclk */
		},
		.sources = &exynos5_clkset_sclk_sclk_i2scodclk,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_PERI1, .shift = 12, .size = 2 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_PERI1, .shift = 12, .size = 3 },
		.reg_div = { .reg = EXYNOS5260_CLKDIV_PERI, .shift = 8, .size = 6 },
	}, {
		.clk	= {
			.name		= "sclk_spdif",	/* sclk_spdif_i_mclk_int */
			.enable		= exynos5_clk_mout_peri1,
			.ctrlbit	= (1 << 20),
		},
		.sources = &exynos5_clkset_sclk_spdif_i_mclk_int,
		.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_PERI1, .shift = 20, .size = 2 },
		.reg_src_stat = { .reg = EXYNOS5260_CLKSRC_STAT_PERI1, .shift = 20, .size = 3 },
	},
};

static int exynos5_gate_clk_set_parent(struct clk *clk, struct clk *parent)
{
	clk->parent = parent;
	return 0;
}

static struct clk_ops exynos5_gate_clk_ops = {
	.set_parent = exynos5_gate_clk_set_parent
};

static struct clk exynos5_init_clocks[] = {
	{
		.name		= "lcd",
		.devname	= "exynos5-fb.1",
		.enable		= exynos5_clk_ip_disp_ctrl,
		.ctrlbit	= (1 << 7) | (3 << 17),
	}, {
		.name		= "dsim1",
		.enable		= exynos5_clk_ip_disp_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "hdmi",
		.devname	= "exynos5-hdmi",
		.enable		= exynos5_clk_ip_disp_ctrl,
		.ctrlbit	= (3 << 8),
	}, {
		.name		= "sclk_usbdrd30",
		.parent		= &exynos5_clk_dout_sclk_fsys_usbdrd30_suspend_clk.clk,
	}, {
		.name		= "sclk_fsys_usbdrd30_suspend_clk",
		.parent		= &exynos5_clk_dout_sclk_fsys_usbdrd30_suspend_clk.clk,
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.0",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 19),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.1",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.2",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name           = "spi",
		.devname        = "s3c64xx-spi.3",
		.enable         = &exynos5_clk_ip_isp1_ctrl,
		.ctrlbit        = (1 << 12), 
	}, {
		.name		= "timers",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 3),
	},
};

static struct clk exynos5_clk_pdma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.0",
	.enable		= exynos5_clk_ip_fsys_ctrl,
	.ctrlbit	= (1 << 9),
};

static struct clk exynos5_clk_mdma = {
	.name		= "dma",
	.devname	= "dma-pl330.1",
};

static struct clk exynos5_clk_adma = {
	.name		= "dma",
	.devname	= "dma-pl330.2",
};


/* Clock initialization code */
static struct clksrc_clk *exynos5_sysclks[] = {
	&exynos5_clk_sclk_mem_pll,
	&exynos5_clk_sclk_bus_pll,
	&exynos5_clk_sclk_media_pll,
	&exynos5_clk_sclk_aud_pll,
	&exynos5_clk_sclk_disp_pll,
	&exynos5_clk_sclk_memtop_pll,
	&exynos5_clk_sclk_bustop_pll,
	&exynos5_clk_sclk_audtop_pll,
	&exynos5_clk_sclk_mediatop_pll,
	&exynos5_clk_mout_disp_disp_333,
	&exynos5_clk_aclk_disp_333,
	&exynos5_clk_mout_disp_disp_222,
	&exynos5_clk_aclk_disp_222,
	&exynos5_clk_mout_disp_media_pixel,
	&exynos5_clk_sclk_disp_pixel,
	&exynos5_clk_aclk_fsys_200,
	&exynos5_clk_mout_sclk_fsys_mmc0_sdclkin_a,
	&exynos5_clk_mout_sclk_fsys_mmc1_sdclkin_a,
	&exynos5_clk_mout_sclk_fsys_mmc2_sdclkin_a,
	&exynos5_clk_dout_sclk_fsys_mmc0_sdclkin_a,
	&exynos5_clk_dout_sclk_fsys_mmc1_sdclkin_a,
	&exynos5_clk_dout_sclk_fsys_mmc2_sdclkin_a,
	&exynos5_clk_aclk_66_peri,
	&exynos5_clk_sclk_peri_aud,
	&exynos5_clk_dout_sclk_fsys_usbdrd30_suspend_clk,
	&exynos5_clk_aclk_disp_333_user,
	&exynos5_clk_aclk_g3d,
	&exynos5_clk_sclk_hpm_targetclk,
	&exynos5_clk_aclk_bus1d_400,
	&exynos5_clk_aclk_bus1p_100,
	&exynos5_clk_aclk_bus2d_400,
	&exynos5_clk_aclk_bus2p_100,
	&exynos5_clk_aclk_bus3d_400,
	&exynos5_clk_aclk_bus3p_100,
	&exynos5_clk_aclk_bus4d_400,
	&exynos5_clk_aclk_bus4p_100,
	&exynos5_clk_mout_g2d_bustop_333,
	&exynos5_clk_aclk_g2d_333,
	&exynos5_clk_mout_isp1_media_266,
	&exynos5_clk_aclk_isp_266,
	&exynos5_clk_mout_isp1_media_400,
	&exynos5_clk_aclk_isp_400,
	&exynos5_clk_dout_sclk_isp1_spi0_a,
	&exynos5_clk_dout_sclk_isp1_spi1_a,
	&exynos5_clk_sclk_isp_spi0_spi_ext_clk,
	&exynos5_clk_sclk_isp_spi1_spi_ext_clk,
	&exynos5_clk_sclk_isp_uart_ext_uclk,
	&exynos5_clk_mout_gscaler_bustop_333,
	&exynos5_clk_aclk_gscl_333,
	&exynos5_clk_mout_m2m_media_400,
	&exynos5_clk_aclk_gscl_400,
	&exynos5_clk_mout_gscaler_bustop_fimc,
	&exynos5_clk_aclk_gscl_fimc,
	&exynos5_clk_mout_mfc_bustop_333,
	&exynos5_clk_aclk_mfc_333,
	&exynos5_clk_dout_sclk_peri_spi0_a_ratio,
	&exynos5_clk_sclk_peri_spi0_spi_ext_clk,
	&exynos5_clk_dout_sclk_peri_spi1_a_ratio,
	&exynos5_clk_sclk_peri_spi1_spi_ext_clk,
	&exynos5_clk_dout_sclk_peri_spi2_a_ratio,
	&exynos5_clk_sclk_peri_spi2_spi_ext_clk,
	&exynos5_clk_sclk_peri_spi3_spi_ext_clk,
	&exynos5_clk_aclk_disp_222_user,
	&exynos5_clk_pclk_disp_111,
	&exynos5_clk_phyclk_hdmi_link_i_tmds_clk,
	&exynos5_clk_mout_sclk_disp_pixel_user,
	&exynos5_clk_phyclk_hdmi_phy_ref_cko_24m,
	&exynos5_clk_aclk_g2d_333_user,
	&exynos5_clk_pclk_g2d_83,
	&exynos5_clk_pclk_g3d_150,
	&exynos5_clk_sclk_kfc_pll,
	&exynos5_clk_dout_pclk_kfc,
	&exynos5_clk_dout_aclk_kfc,
	&exynos5_clk_dout_pclk_dbg_kfc,
	&exynos5_clk_dout_atclk_kfc,
	&exynos5_clk_dout_kfc2,
	&exynos5_clk_dout_kfc1,
	&exynos5_clk_mout_kfc,
	&exynos5_clk_mout_kfc_pll,
	&exynos5_clk_dout_pclk_dbg_egl,
	&exynos5_clk_dout_pclk_egl,
	&exynos5_clk_dout_atclk_egl,
	&exynos5_clk_dout_aclk_egl,
	&exynos5_clk_sclk_egl_pll,
	&exynos5_clk_dout_egl2,
	&exynos5_clk_dout_egl1,
	&exynos5_clk_mout_egl_b,
	&exynos5_clk_mout_egl_a,
	&exynos5_clk_mout_egl_dpll,
	&exynos5_clk_mout_egl_pll,
	&exynos5_clk_aclk_mfc_333_user,
	&exynos5_clk_pclk_mfc_83,
	&exynos5_clk_aclk_m2m_400,
	&exynos5_clk_aclk_gscl_fimc_user,
	&exynos5_clk_dout_aclk_csis_200,
	&exynos5_clk_sclk_csis,
	&exynos5_clk_mout_mif_drex,
	&exynos5_clk_mout_mif_drex2x,
	&exynos5_clk_sclk_clkm_phy,
	&exynos5_clk_aclk_clk2x_phy,
	&exynos5_clk_aclk_mif_466,
	&exynos5_clk_aclk_bus_200,
	&exynos5_clk_aclk_bus_100,
	&exynos5_clk_mout_aud_pll_user,
	&exynos5_clk_aclk_isp_266_user,
	&exynos5_clk_aclk_ca5_clkin,
	&exynos5_clk_pclk_isp_133,
	&exynos5_clk_pclk_isp_66,
	&exynos5_clk_sclk_mpwm_isp,
	&exynos5_clk_aclk_ca5_atclkin,
	&exynos5_clk_pclk_ca5_pclkdbg,
	&exynos5_clk_dout_isp1_sensor0_a,
	&exynos5_clk_dout_isp1_sensor0_b,
	&exynos5_clk_dout_isp1_sensor1_a,
	&exynos5_clk_dout_isp1_sensor1_b,
	&exynos5_clk_dout_isp1_sensor2_a,
	&exynos5_clk_dout_isp1_sensor2_b,
	&exynos5_clk_clkout,
};

static struct clk exynos5_i2cs_clocks[] = {
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.0",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.1",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.2",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.4",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.5",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.6",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.7",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.0",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.1",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 21),

	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.2",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 22),

	}, {
		.name		= "i2c",
		.devname	= "exynos5-hs-i2c.3",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-hdmiphy-i2c",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 15)
	}, {
		.name		= "watchdog",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "watchdog.kfc",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 25),
	}
};

static struct clk *exynos5_clk_cdev[] = {
	&exynos5_clk_pdma0,
	&exynos5_clk_mdma,
	&exynos5_clk_adma,
};

static struct clk *exynos5_clks[] __initdata = {
	&clk_fout_kpll,
	&clk_fout_bpll,
	&clk_fout_cpll,
	&exynos5_clk_phyclk_dptx_pyh_clk_div2,
	&exynos5_clk_phyclk_dptx_pyh_ch0_txd_clk,
	&exynos5_clk_phyclk_dptx_pyh_ch1_txd_clk,
	&exynos5_clk_phyclk_dptx_pyh_ch2_txd_clk,
	&exynos5_clk_phyclk_dptx_pyh_ch3_txd_clk,
	&exynos5_clk_phyclk_dptx_pyh_o_ref_clk_24m,
	&exynos5_clk_phyclk_usbhost20_phy_freeclk,
	&exynos5_clk_phyclk_usbdrd30_udrd30_phyclock,
	&exynos5_clk_phyclk_usbdrd30_udrd30_pipe_pclk,
	&exynos5_clk_phyclk_usbhost20_phy_phyclock,
	&exynos5_clk_phyclk_usbhost20_phy_clk48mohci,
	&exynos5_clk_phyclk_mipi_dphy_4l_m_txbyteclkhs,
	&exynos5_clk_phyclk_mipi_dphy_4l_m_rxclkesc0,
	&exynos5_clk_phyclk_hdmi_phy_tmds_clko,
	&exynos5_clk_sclk_dsim1_txclkesc0,
	&exynos5_clk_sclk_dsim1_txclkesc1,
	&exynos5_clk_sclk_dsim1_txclkesc2,
	&exynos5_clk_sclk_dsim1_txclkesc3,
	&exynos5_clk_phyclk_hdmi_link_o_tmds_clkhi,
	&exynos5_clk_ioclk_spdif_extclk,
	&exynos5_clk_phyclk_hdmi_phy_ref_cko,
	&exynos5_clk_sclk_dsim1_txclkescclk,
	&exynos5_clk_phyclk_hdmi_phy_pixel_clko,
};

static void setup_pll(const char *pll_name,
		struct clk *parent_clk, unsigned long rate)
{
	struct clk *tmp_clk;

	clk_set_rate(parent_clk, rate);
	tmp_clk = clk_get(NULL, pll_name);
	clk_set_parent(tmp_clk, parent_clk);
	clk_put(tmp_clk);
}

static void __init exynos5260_pll_init(void)
{
	setup_pll("fout_dpll", &clk_fout_dpll, 532000000);
	setup_pll("fout_vpll", &clk_fout_vpll, 667000000);
	setup_pll("fout_epll", &clk_fout_epll, 393216000);
}

static inline unsigned long exynos5260_pll2650_get_pll(unsigned long baseclk, u32 pll_con0, u32 pll_con2)
{
	u32 mdiv, pdiv, sdiv, k;
	u64 fvco = baseclk;
	u64 tmp;

	mdiv = (pll_con0 >> EXYNOS5260_PLL2650_MDIV_SHIFT) & EXYNOS5260_PLL2650_MDIV_MASK;
	pdiv = (pll_con0 >> EXYNOS5260_PLL2650_PDIV_SHIFT) & EXYNOS5260_PLL2650_PDIV_MASK;
	sdiv = (pll_con0 >> EXYNOS5260_PLL2650_SDIV_SHIFT) & EXYNOS5260_PLL2650_SDIV_MASK;
	k = (pll_con2 >> EXYNOS5260_PLL2650_KDIV_SHIFT) & EXYNOS5260_PLL2650_K_MASK;

	tmp = k;
	do_div(tmp, 65536);
	tmp += mdiv;
	fvco *= tmp;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}
static inline unsigned long exynos5260_pll2550_get_pll(unsigned long baseclk, u32 pll_con)
{
	u32 mdiv, pdiv, sdiv;
	u64 fvco = baseclk;

	mdiv = (pll_con >> EXYNOS5260_PLL2550_MDIV_SHIFT) & EXYNOS5260_PLL2550_MDIV_MASK;
	pdiv = (pll_con >> EXYNOS5260_PLL2550_PDIV_SHIFT) & EXYNOS5260_PLL2550_PDIV_MASK;
	sdiv = (pll_con >> EXYNOS5260_PLL2550_SDIV_SHIFT) & EXYNOS5260_PLL2550_SDIV_MASK;

	fvco *= mdiv;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}

static unsigned long exynos5_pll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos5260_pll2650_set_rate(struct clk *clk, struct pll_2650 *pll, unsigned long rate)
{
	unsigned int pll_con0, pll_con2;
	unsigned int locktime;
	unsigned int tmp, tmp2, tmp_pms = 0, tmp_k = 0;
	unsigned int i;
	unsigned int k;

	tmp = __raw_readl(EXYNOS5260_EPLL_CON0);
	pll_con0 = tmp & ~(EXYNOS5260_PLL2650_MDIV_MASK << EXYNOS5260_PLL2650_MDIV_SHIFT |
			EXYNOS5260_PLL2650_PDIV_MASK << EXYNOS5260_PLL2650_PDIV_SHIFT |
			EXYNOS5260_PLL2650_SDIV_MASK << EXYNOS5260_PLL2650_SDIV_SHIFT);
	tmp &= (EXYNOS5260_PLL2650_MDIV_MASK << EXYNOS5260_PLL2650_MDIV_SHIFT |
                        EXYNOS5260_PLL2650_PDIV_MASK << EXYNOS5260_PLL2650_PDIV_SHIFT |
                        EXYNOS5260_PLL2650_SDIV_MASK << EXYNOS5260_PLL2650_SDIV_SHIFT);

	tmp2 = __raw_readl(EXYNOS5260_EPLL_CON2);
	pll_con2 = tmp2 & ~(EXYNOS5260_PLL2650_K_MASK << EXYNOS5260_PLL2650_KDIV_SHIFT);
	tmp2 &= (EXYNOS5260_PLL2650_K_MASK << EXYNOS5260_PLL2650_KDIV_SHIFT);

	for (i = 0; i < pll->pms_cnt; i++) {
		if (pll->pms_div[i].rate == rate) {
			k = (~(pll->pms_div[i].k) + 1) & EXYNOS5260_PLL2650_K_MASK;
			tmp_k |= k << 0;
			pll_con2 |= tmp_k;
			tmp_pms |= pll->pms_div[i].mdiv << EXYNOS5260_PLL2650_MDIV_SHIFT;
			tmp_pms |= pll->pms_div[i].pdiv << EXYNOS5260_PLL2650_PDIV_SHIFT;
			tmp_pms |= pll->pms_div[i].sdiv << EXYNOS5260_PLL2650_SDIV_SHIFT;
			pll_con0 |= tmp_pms;
			pll_con0 |= 1 << EXYNOS5260_PLL2650_PLL_ENABLE_SHIFT;
			pll_con0 |= 1 << EXYNOS5260_PLL2650_PLL_FOUTMASK_SHIFT;
			break;
		}
	}

	if (i == pll->pms_cnt) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}

	if (tmp == tmp_pms &&
		tmp2 == tmp_k)
		return 0;

	/* 1500 max_cycls : specification data */
	locktime = 3000 * pll->pms_div[i].pdiv;
	__raw_writel(locktime, EXYNOS5260_EPLL_LOCK);

	__raw_writel(pll_con0, EXYNOS5260_EPLL_CON0);
	__raw_writel(pll_con2, EXYNOS5260_EPLL_CON2);

	do {
		tmp = __raw_readl(EXYNOS5260_EPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5260_PLL2650_PLL_LOCKTIME_SHIFT)));

	clk->rate = rate;

	return 0;
}

static int exynos5260_pll2550_set_rate(struct clk *clk, struct pll_2550 *pll, unsigned long rate)
{
	unsigned int pll_con0;
	unsigned int locktime;
	unsigned int tmp, tmp_pms = 0;
	unsigned int i;

	tmp = __raw_readl(pll->pll_con0);
	pll_con0 = tmp & ~(EXYNOS5260_PLL2550_MDIV_MASK << EXYNOS5260_PLL2550_MDIV_SHIFT |
			EXYNOS5260_PLL2550_PDIV_MASK << EXYNOS5260_PLL2550_PDIV_SHIFT |
			EXYNOS5260_PLL2550_SDIV_MASK << EXYNOS5260_PLL2550_SDIV_SHIFT);
	tmp &= (EXYNOS5260_PLL2550_MDIV_MASK << EXYNOS5260_PLL2550_MDIV_SHIFT |
                        EXYNOS5260_PLL2550_PDIV_MASK << EXYNOS5260_PLL2550_PDIV_SHIFT |
                        EXYNOS5260_PLL2550_SDIV_MASK << EXYNOS5260_PLL2550_SDIV_SHIFT);

	for (i = 0; i < pll->pms_cnt; i++) {
		if (pll->pms_div[i].rate == rate) {
			tmp_pms |= pll->pms_div[i].pdiv << EXYNOS5260_PLL2550_PDIV_SHIFT;
			tmp_pms |= pll->pms_div[i].mdiv << EXYNOS5260_PLL2550_MDIV_SHIFT;
			tmp_pms |= pll->pms_div[i].sdiv << EXYNOS5260_PLL2550_SDIV_SHIFT;
			pll_con0 |= tmp_pms;
			pll_con0 |= 1 << EXYNOS5260_PLL2550_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == pll->pms_cnt) {
		pr_err("%s: Invalid Clock %s Frequency\n", __func__, clk->name);
		return -EINVAL;
	}

	if (tmp_pms == tmp)
		return 0;

	if (pll->force_locktime)
		locktime = pll->force_locktime;
	else
		locktime = 200 * pll->pms_div[i].pdiv;

	__raw_writel(locktime, pll->pll_lock);
	__raw_writel(pll_con0, pll->pll_con0);

	do {
		tmp = __raw_readl(pll->pll_con0);
	} while (!(tmp & (0x1 << EXYNOS5260_PLL2550_PLLCON0_LOCKED_SHIFT)));

	clk->rate = rate;

	return 0;
}

/* For APLL and KPLL */
static int exynos5_fout_simple_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos5_fout_simple_ops = {
	.set_rate = exynos5_fout_simple_set_rate
};

/* DPLL (DISP_PLL)*/
static struct pll_div_data exynos5260_dpll_div[] = {
	{532000000, 3, 266, 2, 0, 0, 0},
	{800000000, 3, 200, 1, 0, 0, 0},
	{600000000, 4, 400, 2, 0, 0, 0},
};

PLL_2550(dpll_data, DPLL, exynos5260_dpll_div);

static int exynos5260_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2550_set_rate(clk, &dpll_data, rate);
}

static struct clk_ops exynos5260_dpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_dpll_set_rate,
};

/* VPLL(G3D_PLL) */
static struct pll_div_data exynos5260_vpll_div[] = {
	{700000000, 3, 175, 1, 0, 0, 0},
	{667000000, 12, 667, 1, 0, 0, 0},
	{620000000, 3, 310, 2, 0, 0, 0},
	{560000000, 3, 280, 2, 0, 0, 0},
	{533000000, 6, 533, 2, 0, 0, 0},
	{450000000, 4, 300, 2, 0, 0, 0},
	{350000000, 3, 175, 2, 0, 0, 0},
	{266000000, 3, 266, 3, 0, 0, 0},
	{160000000, 3, 160, 3, 0, 0, 0},
};

PLL_2550(vpll_data, VPLL, exynos5260_vpll_div);

static int exynos5260_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2550_set_rate(clk, &vpll_data, rate);
}

static struct clk_ops exynos5260_vpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_vpll_set_rate,
};

/* BPLL(MEM_PLL) */
static struct pll_div_data exynos5260_bpll_div[] = {
	{825000000, 4, 275, 1, 0, 0, 0},
	{725000000, 12, 725, 1, 0, 0, 0},
	{543000000, 4, 362, 2, 0, 0, 0},
	{413000000, 6, 413, 2, 0, 0, 0},
	{300000000, 4, 400, 3, 0, 0, 0},
	{275000000, 3, 275, 3, 0, 0, 0},
	{206000000, 3, 206, 3, 0, 0, 0},
	{165000000, 4, 220, 3, 0, 0, 0},
	{138000000, 4, 368, 4, 0, 0, 0},
	{103000000, 3, 206, 4, 0, 0, 0},
};

PLL_2550(bpll_data, BPLL, exynos5260_bpll_div);

static int exynos5260_bpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2550_set_rate(clk, &bpll_data, rate);
}

static struct clk_ops exynos5260_bpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_bpll_set_rate,
};

/* MPLL(BUS_PLL) */
static struct pll_div_data exynos5260_mpll_div[] = {
	{800000000, 3, 200, 1, 0, 0, 0},
};

PLL_2550(mpll_data, MPLL, exynos5260_mpll_div);

static int exynos5260_mpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2550_set_rate(clk, &mpll_data, rate);
}

static struct clk_ops exynos5260_mpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_mpll_set_rate,
};

/* CPLL(MEDIA_PLL) */
static struct pll_div_data exynos5260_cpll_div[] = {
	{667000000, 12, 667, 1, 0, 0, 0},
};

PLL_2550(cpll_data, CPLL, exynos5260_cpll_div);

static int exynos5260_cpll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2550_set_rate(clk, &cpll_data, rate);
}

static struct clk_ops exynos5260_cpll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_cpll_set_rate,
};

/* EPLL(AUD_PLL) */
static struct vpll_div_data exynos5260_epll_div[] = {
	{393216000, 7, 459, 2, 49282, 0, 0, 0},
	{196608000, 7, 459, 3, 16192, 0, 0, 0},
	{192000000, 1, 64, 3, 0, 0, 0, 0},
};

PLL_2650(epll_data, EPLL, exynos5260_epll_div);

static int exynos5260_epll_set_rate(struct clk *clk, unsigned long rate)
{
	return exynos5260_pll2650_set_rate(clk, &epll_data, rate);
}
static struct clk_ops exynos5260_epll_ops = {
	.get_rate = exynos5_pll_get_rate,
	.set_rate = exynos5260_epll_set_rate,
};

void __init_or_cpufreq exynos5260_setup_clocks(void)
{
	struct clk *xtal_clk;

	unsigned long xtal;
	unsigned long apll;
	unsigned long kpll;
	unsigned long mpll;
	unsigned long bpll;
	unsigned long cpll;
	unsigned long vpll;
	unsigned long dpll;
	unsigned long epll;
	unsigned long egl_dpll;
	unsigned int i;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	clk_fout_apll.ops = &exynos5_fout_simple_ops;
	clk_fout_kpll.ops = &exynos5_fout_simple_ops;
	clk_fout_dpll.ops = &exynos5260_dpll_ops;
	clk_fout_vpll.ops = &exynos5260_vpll_ops;
	clk_fout_bpll.ops = &exynos5260_bpll_ops;
	clk_fout_mpll.ops = &exynos5260_mpll_ops;
	clk_fout_cpll.ops = &exynos5260_cpll_ops;
	clk_fout_epll.ops = &exynos5260_epll_ops;

	clk_fout_cpll.ctrlbit = (1 << 23);

	bpll_data.force_locktime = 2400;
	cpll_data.force_locktime = 2400;
	mpll_data.force_locktime = 2400;

	exynos5260_pll_init();

	apll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_APLL_CON0));
	kpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_KPLL_CON0));
	mpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_MPLL_CON0));
	bpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_BPLL_CON0));
	cpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_CPLL_CON0));
	vpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_VPLL_CON0));
	dpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_DPLL_CON0));
	epll = exynos5260_pll2650_get_pll(xtal, __raw_readl(EXYNOS5260_EPLL_CON0),
						__raw_readl(EXYNOS5260_EPLL_CON2));
	egl_dpll = exynos5260_pll2550_get_pll(xtal, __raw_readl(EXYNOS5260_EGL_DPLL_CON0));

	clk_fout_apll.rate = apll;
	clk_fout_bpll.rate = bpll;
	clk_fout_cpll.rate = cpll;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.rate = vpll;
	clk_fout_kpll.rate = kpll;
	clk_fout_dpll.rate = dpll;
	clk_fout_egl_dpll.rate = egl_dpll;

	for (i = 0; i < ARRAY_SIZE(exynos5_sysclks); i++)
		s3c_set_clksrc(exynos5_sysclks[i], true);

	clk_set_rate(&exynos5_clk_sclk_hpm_targetclk.clk, 267 * 1000 * 1000);
}

static struct clk *exynos5_clks_off[] __initdata = {
	&clk_fout_epll,
};

static struct clk_lookup exynos5260_clk_lookup[] = {
	CLKDEV_INIT("dma-pl330.0", "apb_pclk", &exynos5_clk_pdma0),
	CLKDEV_INIT("dma-pl330.1", "apb_pclk", &exynos5_clk_mdma),
	CLKDEV_INIT("dma-pl330.2", "apb_pclk", &exynos5_clk_adma),
};

/* CLOCK GATING OFF LISTS */
static struct clk exynos5260_init_clocks_off[] = {
	{
		.name		= "g3d",
		.devname	= "mali.0",
		.enable		= exynos5_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 2) | (1 << 7),
	}, {
		.name		= "mfc",
#ifdef CONFIG_S5P_DEV_MFC
		.devname	= "s3c-mfc",
#endif
		.enable		= &exynos5_clk_ip_mfc_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= ((3 << 4) | (1 << 1)),
	}, {
		.name		= "ppmu.mfc",
		.enable		= &exynos5_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mfc_lr, 0),
		.enable		= &exynos5_clk_ip_mfc_secure_smmu2_mfc_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (3 << 6),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(tv, 2),
		.enable		= &exynos5_clk_ip_disp_ctrl, /* embedded in disp1 ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 25)
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mjpeg, 20),
		.enable		= &exynos5_clk_ip_g2d_ctrl, /* embedded in g2d ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc0, 5),
		.enable		= &exynos5_clk_ip_gscl_secure_smmu_gscl0_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc1, 6),
		.enable		= &exynos5_clk_ip_gscl_secure_smmu_gscl1_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp0, 9),
		.enable		= &exynos5_clk_ip_isp1_ctrl, /* embedded in isp1 ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (0x1F << 21),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd1, 11),
		.enable		= &exynos5_clk_ip_disp_ctrl, /* embedded in disp1 ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd1a, 30),
		.enable		= &exynos5_clk_ip_disp_ctrl, /* embedded in disp1 ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif0, 12),
		.enable		= &exynos5_clk_ip_gscl_fimc_ctrl, /* embedded in gscl_fimc ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif1, 13),
		.enable		= &exynos5_clk_ip_gscl_fimc_ctrl, /* embedded in gscl_fimc ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 6),
	}, {
		.name           = SYSMMU_CLOCK_NAME,
		.devname        = SYSMMU_CLOCK_DEVNAME(camif2, 14),
		.enable         = &exynos5_clk_ip_gscl_fimc_ctrl, /* embedded in gscl_fimc ctrl */
		.ops            = &exynos5_gate_clk_ops,
		.ctrlbit        = (1 << 7),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(2d, 15),
		.enable		= &exynos5_clk_ip_g2d_secure_smmu_g2d_g2d_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 15)
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp2, 17),
		.enable		= &exynos5_clk_ip_isp1_ctrl, /* embedded in isp1 ctrl */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 26),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler, 18),
		.enable		= &exynos5_clk_ip_gscl_secure_smmu_mscl0_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 19),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler1, 31),
		.enable		= &exynos5_clk_ip_gscl_secure_smmu_mscl1_ctrl, /* sysmmu own clk gate */
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.0",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.1",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "dwmci",
		.devname	= "dw_mmc.2",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "usbdrd30",
		.devname	= "exynos-dwc3.0",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "usbhost",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 15),
	}, {
		.name		= "tsi",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "rtic",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_secure_rtic,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "sysmmu-rtic",
		.parent		= &exynos5_clk_aclk_fsys_200.clk,
		.enable		= exynos5_clk_ip_fsys_secure_smmu_rtic,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "gscl",
#ifdef CONFIG_EXYNOS5_DEV_GSC
		.devname	= "exynos-gsc.0",
#endif
		.parent		= &exynos5_clk_aclk_gscl_333.clk,
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 2) | ( 1 << 13),
	}, {
		.name		= "gscl",
#ifdef CONFIG_EXYNOS5_DEV_GSC
		.devname	= "exynos-gsc.1",
#endif
		.parent		= &exynos5_clk_aclk_gscl_333.clk,
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 3) | ( 1 << 14),
	}, {
		.name		= "fimg2d",
		.devname	= "s5p-fimg2d",
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ctrlbit	= ((1 << 4) | (1 << 9)),
	}, {
		.name		= "jpeg-hx",
		.enable		= exynos5_clk_ip_g2d_ctrl,
		.ops		= &exynos5_gate_clk_ops,
		.ctrlbit	= (1 << 5) | (1 << 10),
	}, {
		.name		= "secss",
		.devname	= "s5p-sss.0",
		.parent		= &exynos5_clk_aclk_g2d_333_user.clk,
		.enable		= exynos5_clk_ip_sss_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "sysmmu-secss",
		.devname	= "s5p-sss.0",
		.parent		= &exynos5_clk_aclk_g2d_333_user.clk,
		.enable		= exynos5_clk_ip_g2d_secure_smmu_sss_g2d_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "slimsss",
		.devname	= "s5p-slimsss.0",
		.parent		= &exynos5_clk_aclk_g2d_333_user.clk,
		.enable		= exynos5_clk_ip_slimsss_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "sysmmu-slimsss",
		.devname	= "s5p-slimsss.0",
		.parent		= &exynos5_clk_aclk_g2d_333_user.clk,
		.enable		= exynos5_clk_ip_g2d_secure_smmu_slim_sss_g2d_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "mscl",
		.devname	= "exynos5-scaler.0",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 4) | (1 << 15),
	}, {
		.name		= "mscl",
		.devname	= "exynos5-scaler.1",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 5) | (1 << 16),
	}, {
		.name		= "isp1",
		.devname	= FIMC_IS_DEV_NAME,
		.enable		= exynos5_clk_ip_isp1_ctrl,
		.ctrlbit	= ((0x1F) << 27) || ((0x1FFFFF << 0)),
	}, {
		.name		= "isp1_sensor",
		.enable		= exynos5_clk_ip_top_ctrl,
		.ctrlbit	= ((1 << 16) | (1 << 20) | (1 << 24)),
	}, {
		.name		= "gscl_flite.0",
		.enable		= exynos5_clk_ip_gscl_fimc_ctrl,
		.parent		= &exynos5_clk_aclk_gscl_fimc_user.clk,
		.ctrlbit	= (1 << 10) | (1 << 2),
	}, {
		.name		= "gscl_flite.1",
		.enable		= exynos5_clk_ip_gscl_fimc_ctrl,
		.parent		= &exynos5_clk_aclk_gscl_fimc_user.clk,
		.ctrlbit	= (1 << 11) | (1 << 3),
	}, {
		.name		= "gscl_flite.2",
		.enable		= exynos5_clk_ip_gscl_fimc_ctrl,
		.parent		= &exynos5_clk_aclk_gscl_fimc_user.clk,
		.ctrlbit	= ((1 << 12) | (1 << 7) | (1 << 4)),
	}, {
		.name		= "gscl_wrap0",
		.devname	= "s5p-mipi-csis.0",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "gscl_wrap1",
		.devname	= "s5p-mipi-csis.1",
		.enable		= exynos5_clk_ip_gscl_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "mixer",
		.devname	= "s5p-mixer",
		.enable		= exynos5_clk_ip_disp_ctrl,
		.ctrlbit	= ((7 << 11) | (1 << 16) | (3 << 20)),
	}, {
		.name		= "dp",
		.devname	= "s5p-dp",
		.enable		= exynos5_clk_ip_disp_ctrl,
		.ctrlbit	= (3 << 4),
	}, {
		.name		= "abb",
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "efuse_writer",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "hdmicec",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.1",
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.1",
		.enable		= exynos5_clk_ip_peri0_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "asv_tbl_apbif",
		.enable		= exynos5_clk_ip_peri1_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name		= "spdif",
		.devname	= "samsung-spdif",
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.2",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "adc",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri2_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "rtc",
		.parent		= &exynos5_clk_aclk_66_peri.clk,
		.enable		= exynos5_clk_ip_peri_secure_top_rtc_ctrl,
		.ctrlbit	= (1 << 4),
	},
};

#ifdef CONFIG_PM
static int exynos5260_clock_suspend(void)
{
	s3c_pm_do_save(exynos5260_clock_save, ARRAY_SIZE(exynos5260_clock_save));
	s3c_pm_do_save(exynos5260_cpll_save, ARRAY_SIZE(exynos5260_cpll_save));
	s3c_pm_do_save(exynos5260_mpll_save, ARRAY_SIZE(exynos5260_mpll_save));
	s3c_pm_do_save(exynos5260_bpll_save, ARRAY_SIZE(exynos5260_bpll_save));
	s3c_pm_do_save(exynos5260_epll_save, ARRAY_SIZE(exynos5260_epll_save));
	s3c_pm_do_save(exynos5260_dpll_save, ARRAY_SIZE(exynos5260_dpll_save));

	return 0;
}

static void exynos5_pll_wait_locktime(void __iomem *con_reg, int shift_value)
{
	unsigned int tmp;

	do {
		tmp = __raw_readl(con_reg);
	} while (tmp >> EXYNOS5260_PLL_ENABLE_SHIFT && !(tmp & (0x1 << EXYNOS5260_PLLCON0_LOCKED_SHIFT)));
}

static void exynos5260_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos5260_clock_save, ARRAY_SIZE(exynos5260_clock_save));
	s3c_pm_do_restore_core(exynos5260_cpll_save, ARRAY_SIZE(exynos5260_cpll_save));
	s3c_pm_do_restore_core(exynos5260_mpll_save, ARRAY_SIZE(exynos5260_mpll_save));
	s3c_pm_do_restore_core(exynos5260_bpll_save, ARRAY_SIZE(exynos5260_bpll_save));
	s3c_pm_do_restore_core(exynos5260_epll_save, ARRAY_SIZE(exynos5260_epll_save));
	s3c_pm_do_restore_core(exynos5260_dpll_save, ARRAY_SIZE(exynos5260_dpll_save));

	exynos5_pll_wait_locktime(EXYNOS5260_CPLL_CON0, EXYNOS5260_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5260_MPLL_CON0, EXYNOS5260_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5260_BPLL_CON0, EXYNOS5260_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5260_EPLL_CON0, EXYNOS5260_PLLCON0_LOCKED_SHIFT);
	exynos5_pll_wait_locktime(EXYNOS5260_DPLL_CON0, EXYNOS5260_PLLCON0_LOCKED_SHIFT);

}
#else
#define exynos5260_clock_suspend NULL
#define exynos5260_clock_resume NULL
#endif

struct syscore_ops exynos5260_clock_syscore_ops = {
	.suspend	= exynos5260_clock_suspend,
	.resume		= exynos5260_clock_resume,
};

void __init exynos5260_register_clocks(void)
{
	int ptr;

	s3c24xx_register_clocks(exynos5_clks_off, ARRAY_SIZE(exynos5_clks_off));
	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_clks_off); ptr++)
		s3c_disable_clocks(exynos5_clks_off[ptr], 1);

	s3c24xx_register_clocks(exynos5_clks, ARRAY_SIZE(exynos5_clks));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_sysclks); ptr++)
		s3c_register_clksrc(exynos5_sysclks[ptr], 1);

	s3c24xx_register_clocks(exynos5_clk_cdev, ARRAY_SIZE(exynos5_clk_cdev));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos5_clk_cdev); ptr++)
		s3c_disable_clocks(exynos5_clk_cdev[ptr], 1);

	s3c_register_clksrc(exynos5_clksrcs, ARRAY_SIZE(exynos5_clksrcs));
	s3c_register_clocks(exynos5_init_clocks, ARRAY_SIZE(exynos5_init_clocks));

	clkdev_add_table(exynos5260_clk_lookup, ARRAY_SIZE(exynos5260_clk_lookup));

	s3c_register_clocks(exynos5260_init_clocks_off, ARRAY_SIZE(exynos5260_init_clocks_off));
	s3c_disable_clocks(exynos5260_init_clocks_off, ARRAY_SIZE(exynos5260_init_clocks_off));

	s3c_register_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));
	s3c_disable_clocks(exynos5_i2cs_clocks, ARRAY_SIZE(exynos5_i2cs_clocks));

	register_syscore_ops(&exynos5260_clock_syscore_ops);
#ifdef CONFIG_SAMSUNG_DEV_PWM
	s3c_pwmclk_init();
#endif
}
