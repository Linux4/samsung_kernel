/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4415 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/pm.h>

#include <mach/map.h>
#include <mach/regs-clock-exynos4415.h>
#include <mach/sysmmu.h>

#include "common.h"
#include "clock-exynos4415.h"

#define clk_fin_isp_pll clk_ext_xtal_mux

#ifdef CONFIG_PM_SLEEP
static struct sleep_save exynos4415_clock_save[] = {
	SAVE_ITEM(EXYNOS4415_CLKDIV_ACP0),
	SAVE_ITEM(EXYNOS4415_CLKDIV_ACP1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_CAM),
	SAVE_ITEM(EXYNOS4415_CLKDIV_CAM1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_CPU0),
	SAVE_ITEM(EXYNOS4415_CLKDIV_CPU1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_DMC1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_FSYS0),
	SAVE_ITEM(EXYNOS4415_CLKDIV_FSYS1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_FSYS2),
	SAVE_ITEM(EXYNOS4415_CLKDIV_FSYS3),
	SAVE_ITEM(EXYNOS4415_CLKDIV_G3D),
	SAVE_ITEM(EXYNOS4415_CLKDIV_LCD),
	SAVE_ITEM(EXYNOS4415_CLKDIV_LEFTBUS),
	SAVE_ITEM(EXYNOS4415_CLKDIV_MAUDIO),
	SAVE_ITEM(EXYNOS4415_CLKDIV_MFC),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL0),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL1),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL2),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL3),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL4),
	SAVE_ITEM(EXYNOS4415_CLKDIV_PERIL5),
	SAVE_ITEM(EXYNOS4415_CLKDIV_RIGHTBUS),
	SAVE_ITEM(EXYNOS4415_CLKDIV_TOP),
	SAVE_ITEM(EXYNOS4415_CLKDIV_TV),
	SAVE_ITEM(EXYNOS4415_CLKSRC_ACP),
	SAVE_ITEM(EXYNOS4415_CLKSRC_CAM),
	SAVE_ITEM(EXYNOS4415_CLKSRC_CAM1),
	SAVE_ITEM(EXYNOS4415_CLKSRC_CPU),
	SAVE_ITEM(EXYNOS4415_CLKSRC_DMC),
	SAVE_ITEM(EXYNOS4415_CLKSRC_FSYS),
	SAVE_ITEM(EXYNOS4415_CLKSRC_G3D),
	SAVE_ITEM(EXYNOS4415_CLKSRC_LCD),
	SAVE_ITEM(EXYNOS4415_CLKSRC_LEFTBUS),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MAUDIO),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MFC),
	SAVE_ITEM(EXYNOS4415_CLKSRC_PERIL0),
	SAVE_ITEM(EXYNOS4415_CLKSRC_PERIL1),
	SAVE_ITEM(EXYNOS4415_CLKSRC_RIGHTBUS),
	SAVE_ITEM(EXYNOS4415_CLKSRC_TOP0),
	SAVE_ITEM(EXYNOS4415_CLKSRC_TOP1),
	SAVE_ITEM(EXYNOS4415_CLKSRC_TV),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_ACP),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_LCD),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_PERIL0),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_PERIL1),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_TOP),
	SAVE_ITEM(EXYNOS4415_CLKSRC_MASK_TV),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_ACP0),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_ACP1),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_CAM),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_CPU),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_DMC0),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_DMC1),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_G3D),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_IMAGE),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_LCD),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_LEFTBUS),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_MAUDIO),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_PERIL),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_PERIR),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_RIGHTBUS),
	SAVE_ITEM(EXYNOS4415_CLKGATE_IP_TV),
};
#endif

/* Clock gating function */
static int exynos4415_clksrc_mask_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_TOP, clk, enable);
}

static int exynos4415_clksrc_mask_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_CAM, clk, enable);
}

static int exynos4415_clksrc_mask_peril0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_PERIL0, clk, enable);
}

static int exynos4415_clksrc_mask_peril1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_PERIL1, clk, enable);
}

static int exynos4415_clksrc_mask_maudio_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_MAUDIO, clk, enable);
}

static int exynos4415_clksrc_mask_lcd_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_LCD, clk, enable);
}

static int exynos4415_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_FSYS, clk, enable);
}

static int exynos4415_clk_ip_g3d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_G3D, clk, enable);
}

static int exynos4415_clksrc_mask_tv_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKSRC_MASK_TV, clk, enable);
}

static int exynos4415_clk_ip_peril_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_PERIL, clk, enable);
}

static int exynos4415_clk_ip_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_CAM, clk, enable);
}

static int exynos4415_clk_ip_tv_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_TV, clk, enable);
}

static int exynos4415_clk_ip_perir_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_PERIR, clk, enable);
}

static int exynos4415_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos4415_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_MFC, clk, enable);
}

static int exynos4415_clk_ip_lcd_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_LCD, clk, enable);
}

static int exynos4415_clk_ip_image_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4415_CLKGATE_IP_IMAGE, clk, enable);
}

static int exynos4415_clk_clkout_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS_PMU_DEBUG, clk, !enable);
}

/* APLL */
static struct clksrc_clk exynos4415_clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CPU, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos4415_clk_sclk_apll = {
	.clk	= {
		.name		= "sclk_apll",
		.parent		= &exynos4415_clk_mout_apll.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 24, .size = 3 },
};

/* MPLL related clocks */
/* mout_mpll */
struct clksrc_clk exynos4415_clk_mout_mpll = {
	.clk	= {
		.name		= "mout_mpll",
	},
	.sources = &clk_src_mpll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_DMC, .shift = 12, .size = 1 },
};

/* sclk_mpll */
struct clksrc_clk exynos4415_clk_sclk_mpll = {
	.clk	= {
		.name		= "sclk_mpll",
		.parent		= &exynos4415_clk_mout_mpll.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 8, .size = 2 },
};

/* mpll_user_cpu */
static struct clk *exynos4415_clkset_mout_mpll_user_cpu_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4415_clk_sclk_mpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_mpll_user_cpu = {
	.sources	= exynos4415_clkset_mout_mpll_user_cpu_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_mpll_user_cpu_list),
};

static struct clksrc_clk exynos4415_clk_mout_mpll_user_cpu = {
	.clk = {
		.name		= "mout_mpll_user_cpu",	/* sclk_mpll_user_cpu */
	},
	.sources	= &exynos4415_clkset_mout_mpll_user_cpu,
	.reg_src	= { .reg = EXYNOS4415_CLKSRC_CPU, .shift = 24, .size = 1 },
};

/* mpll_user_top */
static struct clk *exynos4415_clkset_mout_mpll_user_top_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4415_clk_sclk_mpll.clk,	/* sclk_mpll */
};

static struct clksrc_sources exynos4415_clkset_mout_mpll_user_top = {
	.sources	= exynos4415_clkset_mout_mpll_user_top_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_mpll_user_top_list),
};

static struct clksrc_clk exynos4415_clk_mout_mpll_user_top = {
	.clk	= {
		.name		= "mout_mpll_user_top", /* sclk_mpll_user_top */
	},
	.sources = &exynos4415_clkset_mout_mpll_user_top,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP1, .shift = 12, .size = 1},
};

/* mpll_user_acp */
static struct clk *exynos4415_clkset_mpll_user_acp_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4415_clk_sclk_mpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mpll_user_acp = {
	.sources        = exynos4415_clkset_mpll_user_acp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mpll_user_acp_list),
};

static struct clksrc_clk exynos4415_clk_mout_mpll_user_acp = {
	.clk    = {
		.name           = "mout_mpll_user_acp",
	},
	.sources = &exynos4415_clkset_mpll_user_acp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 13, .size = 1 },
};

/* BPLL related clocks */
/* mout_bpll */
struct clksrc_clk exynos4415_clk_mout_bpll = {
	.clk	= {
		.name		= "mout_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_DMC, .shift = 10, .size = 1 },
};

/* sclk_bpll */
struct clksrc_clk exynos4415_clk_sclk_bpll = {
	.clk	= {
		.name		= "sclk_bpll",
		.parent		= &exynos4415_clk_mout_bpll.clk,
	},
	/* This clock divider is hardware 1/2 divider, SW not control */
};

/* bpll_user_top */
static struct clk *exynos4415_clkset_bpll_user_acp_list[] = {
	[0] = &clk_fin_bpll,
	[1] = &exynos4415_clk_sclk_bpll.clk,
};

static struct clksrc_sources exynos4415_clkset_bpll_user_acp = {
	.sources        = exynos4415_clkset_bpll_user_acp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_bpll_user_acp_list),
};

static struct clksrc_clk exynos4415_clk_mout_bpll_user_acp = {
	.clk    = {
		.name           = "mout_bpll_user_acp",
	},
	.sources = &exynos4415_clkset_bpll_user_acp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 11, .size = 1 },
};

/* EPLL */
/* mout_epll */
static struct clksrc_clk exynos4415_clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 4, .size = 1 },
};

/* sclk_epll */
static struct clksrc_clk exynos4415_clk_sclk_epll = {
	.clk	= {
		.name		= "sclk_epll",
		.parent		= &exynos4415_clk_mout_epll.clk,
	},
};

/* VPLL */
/* vpllsrc */
static struct clk *exynos4415_clkset_vpllsrc_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_vpllsrc = {
	.sources	= exynos4415_clkset_vpllsrc_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_vpllsrc_list),
};

static struct clksrc_clk exynos4415_clk_vpllsrc = {
	.clk	= {
		.name		= "vpll_src",
		.enable		= exynos4415_clksrc_mask_top_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset_vpllsrc,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP1, .shift = 0, .size = 1 },
};

/* sclk_vpll */
static struct clk *exynos4415_clkset_sclk_vpll_list[] = {
	[0] = &exynos4415_clk_vpllsrc.clk,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources exynos4415_clkset_sclk_vpll = {
	.sources	= exynos4415_clkset_sclk_vpll_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_vpll_list),
};

static struct clksrc_clk exynos4415_clk_sclk_vpll = {
	.clk	= {
		.name		= "sclk_vpll",
	},
	.sources = &exynos4415_clkset_sclk_vpll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 8, .size = 1 },
};

/* ISP PLL */
struct clk clk_fout_isp_pll = {
	.name		= "fout_isp_pll",
	.id		= -1,
	.ctrlbit	= (1 << 31),
};

/* mout_isp_pll */
static struct clk *clk_src_isp_pll_list[] = {
	[0] = &clk_fin_isp_pll,
	[1] = &clk_fout_isp_pll,
};

struct clksrc_sources clk_src_isp_pll = {
	.sources	= clk_src_isp_pll_list,
	.nr_sources	= ARRAY_SIZE(clk_src_isp_pll_list),
};

static struct clksrc_clk exynos4415_clk_mout_isp_pll = {
	.clk    = {
		.name           = "mout_isp_pll",	/* sclk_isp_pll */
	},
	.sources = &clk_src_isp_pll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP1, .shift = 28, .size = 1 },
};

/* DISP PLL */
/* mout_disp_pll */
static struct clksrc_clk exynos4415_clk_mout_disp_pll = {
	.clk    = {
		.name           = "mout_disp_pll",	/* sclk_disp_pll */
	},
	.sources = &clk_src_dpll,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP1, .shift = 16, .size = 1 },
};

/* CMU CPU */
/* moutcore */
static struct clk *exynos4415_clkset_moutcore_list[] = {
	[0] = &exynos4415_clk_mout_apll.clk,
	[1] = &exynos4415_clk_mout_mpll_user_cpu.clk,
};

static struct clksrc_sources exynos4415_clkset_moutcore = {
	.sources	= exynos4415_clkset_moutcore_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_moutcore_list),
};

static struct clksrc_clk exynos4415_clk_moutcore = {
	.clk	= {
		.name		= "moutcore",
	},
	.sources = &exynos4415_clkset_moutcore,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CPU, .shift = 16, .size = 1 },
};

/* core_clk */
static struct clksrc_clk exynos4415_clk_coreclk = {
	.clk	= {
		.name		= "core_clk",
		.parent		= &exynos4415_clk_moutcore.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 0, .size = 3 },
};

/* core2_clk */
static struct clksrc_clk exynos4415_clk_armclk = {
	.clk	= {
		.name		= "armclk",
		.parent		= &exynos4415_clk_coreclk.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 28, .size = 3 },
};

/* corem0_clk */
static struct clksrc_clk exynos4415_clk_corerem0 = {
	.clk	= {
		.name		= "aclk_corem0",
		.parent		= &exynos4415_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 4, .size = 3 },
};

/* corem1_clk */
static struct clksrc_clk exynos4415_clk_corerem1 = {
	.clk	= {
		.name		= "aclk_corem1",
		.parent		= &exynos4415_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 8, .size = 3 },
};

/* atclk */
static struct clksrc_clk exynos4415_clk_atb = {
	.clk = {
		.name		= "atclk",
		.parent		= &exynos4415_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 16, .size = 3 },
};

/* pclk_dbg */
static struct clksrc_clk exynos4415_clk_pclk_dbg = {
	.clk = {
		.name		= "pclk_dbg",
		.parent		= &exynos4415_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU0, .shift = 20, .size = 3 },
};

/* dout_copy */
static struct clk *exynos4415_clkset_dout_copy_list[] = {
	[0] = &exynos4415_clk_mout_apll.clk,
	[1] = &exynos4415_clk_mout_mpll_user_cpu.clk,
};

static struct clksrc_sources exynos4415_clkset_dout_copy = {
	.sources	= exynos4415_clkset_dout_copy_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_dout_copy_list),
};

static struct clksrc_clk exynos4415_clk_dout_copy = {
	.clk = {
		.name		= "dout_copy",
	},
	.sources = &exynos4415_clkset_dout_copy,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CPU, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CPU1, .shift = 0, .size = 3 },
};

/* sclk_hpm */
static struct clksrc_clk exynos4415_clk_sclk_hpm = {
	.clk = {
		.name		= "sclk_hpm",
		.parent		= &exynos4415_clk_dout_copy.clk,
	},
	.reg_div	= { .reg = EXYNOS4415_CLKDIV_CPU1, .shift = 4, .size = 3 },
};

/* CMU_DMC */
/* mout_dmc_bus */
static struct clk *exynos4415_clkset_mout_dmc_bus_list[] = {
	[0] = &exynos4415_clk_mout_mpll.clk,
	[1] = &exynos4415_clk_mout_bpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_dmc_bus = {
	.sources	= exynos4415_clkset_mout_dmc_bus_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_dmc_bus_list),
};

static struct clksrc_clk exynos4415_clk_mout_dmc_bus = {
	.clk	= {
		.name		= "mout_dmc_bus",
	},
	.sources = &exynos4415_clkset_mout_dmc_bus,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_DMC, .shift = 4, .size = 1 },
};

/* dout_dmc_pre */
static struct clksrc_clk exynos4415_clk_dout_dmc_pre = {
	.clk	= {
		.name		= "dout_dmc_pre",
		.parent		= &exynos4415_clk_mout_dmc_bus.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 19, .size = 2 },
};

/* sclk_dmc */
static struct clksrc_clk exynos4415_clk_sclk_dmc = {
	.clk	= {
		.name		= "sclk_dmc",
		.parent		= &exynos4415_clk_dout_dmc_pre.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 27, .size = 3 },
};

/* aclk_dmcd */
static struct clksrc_clk exynos4415_clk_aclk_dmcd = {
	.clk	= {
		.name		= "aclk_dmcd",
		.parent		= &exynos4415_clk_sclk_dmc.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 11, .size = 3 },
};

/* aclk_dmcp */
static struct clksrc_clk exynos4415_clk_aclk_dmcp = {
	.clk	= {
		.name		= "aclk_dmcp",
		.parent		= &exynos4415_clk_aclk_dmcd.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 15, .size = 3 },
};

/* mout_dphy */
static struct clk *exynos4415_clkset_sclk_dphy_list[] = {
	[0] = &exynos4415_clk_mout_mpll.clk,
	[1] = &exynos4415_clk_mout_bpll.clk,
};

static struct clksrc_sources exynos4415_clkset_sclk_dphy = {
	.sources	= exynos4415_clkset_sclk_dphy_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_dphy_list),
};

static struct clksrc_clk exynos4415_clk_sclk_dphy = {
	.clk	= {
		.name		= "sclk_dphy", /* sclk_dphy */
	},
	.sources = &exynos4415_clkset_sclk_dphy,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_DMC, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_DMC1, .shift = 23, .size = 3 },
	/* This divider is fix 1/2 dividing value, SW not control */
};

/* CMU_ACP */
/* mout_dmc_bus_acp */
static struct clk *exynos4415_clkset_dmc_bus_acp_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_acp.clk,
	[1] = &exynos4415_clk_mout_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4415_clkset_dmc_bus_acp = {
	.sources        = exynos4415_clkset_dmc_bus_acp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_dmc_bus_acp_list),
};

static struct clksrc_clk exynos4415_clk_mout_dmc_bus_acp = {
	.clk    = {
		.name           = "mout_dmc_bus_acp",
	},
	.sources = &exynos4415_clkset_dmc_bus_acp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 4, .size = 1 },
};

/* dout_acp_dmc */
static struct clksrc_clk exynos4415_clk_dout_acp_dmc = {
	.clk    = {
		.name           = "dout_acp_dmc",
		.parent		= &exynos4415_clk_mout_dmc_bus_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ACP0, .shift = 8, .size = 3 },
};

/* aclk_acp_dmcd */
static struct clksrc_clk exynos4415_clk_aclk_acp_dmcd = {
	.clk    = {
		.name           = "aclk_acp_dmcd",
		.parent		= &exynos4415_clk_dout_acp_dmc.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ACP0, .shift = 12, .size = 3 },
};

/* aclk_acp_dmcp */
static struct clksrc_clk exynos4415_clk_aclk_acp_dmcp = {
	.clk    = {
		.name           = "aclk_acp_dmcp",
		.parent		= &exynos4415_clk_aclk_acp_dmcd.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ACP0, .shift = 16, .size = 3 },
};

/* dout_pwi */
static struct clk *exynos4415_clkset_pwi_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = NULL,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos4415_clk_mout_mpll_user_acp.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_mout_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4415_clkset_pwi = {
	.sources        = exynos4415_clkset_pwi_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_pwi_list),
};

static struct clksrc_clk exynos4415_clk_sclk_pwi = {
	.clk    = {
		.name           = "sclk_pwi",	/* sclk_pwm */
	},
	.sources = &exynos4415_clkset_pwi,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ACP1, .shift = 5, .size = 6 },
};

/* mout_g2d_acp_0 */
static struct clk *exynos4415_clkset_g2d_acp_0_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_acp.clk,
	[1] = &exynos4415_clk_mout_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4415_clkset_g2d_acp_0 = {
	.sources        = exynos4415_clkset_g2d_acp_0_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_g2d_acp_0_list),
};

static struct clksrc_clk exynos4415_clk_mout_g2d_acp_0 = {
	.clk    = {
		.name           = "mout_g2d_acp_0",
	},
	.sources = &exynos4415_clkset_g2d_acp_0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 20, .size = 1 },
};

/* mout_g2d_acp_1 */
static struct clk *exynos4415_clkset_g2d_acp_1_list[] = {
	[0] = &exynos4415_clk_sclk_epll.clk,	/* sclk_epll */
	[1] = &exynos4415_clk_mout_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4415_clkset_g2d_acp_1 = {
	.sources        = exynos4415_clkset_g2d_acp_1_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_g2d_acp_1_list),
};

static struct clksrc_clk exynos4415_clk_mout_g2d_acp_1 = {
	.clk    = {
		.name           = "mout_g2d_acp_1",
	},
	.sources = &exynos4415_clkset_g2d_acp_1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 24, .size = 1 },
};

/* mout_g2d_acp */
static struct clk *exynos4415_clkset_g2d_acp_list[] = {
	[0] = &exynos4415_clk_mout_g2d_acp_0.clk,
	[1] = &exynos4415_clk_mout_g2d_acp_1.clk,
};

static struct clksrc_sources exynos4415_clkset_g2d_acp = {
	.sources        = exynos4415_clkset_g2d_acp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_g2d_acp_list),
};

static struct clksrc_clk exynos4415_clk_sclk_g2d_acp = {
	.clk    = {
		.name           = "sclk_g2d_acp",
	},
	.sources = &exynos4415_clkset_g2d_acp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ACP, .shift = 28, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ACP1, .shift = 0, .size = 4 },
};

/* CMU_TOP */
/* mout_aclk list */
static struct clk *exynos4415_clkset_mout_aclk_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_top.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_mout_aclk = {
	.sources        = exynos4415_clkset_mout_aclk_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mout_aclk_list),
};

/* aclk_266_pre */
static struct clk *exynos4415_clkset_mout_aclk_266_pre_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_top.clk,
	[1] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_aclk_266_pre = {
	.sources        = exynos4415_clkset_mout_aclk_266_pre_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mout_aclk_266_pre_list),
};

static struct clksrc_clk exynos4415_clk_aclk_266_pre = {
	.clk    = {
		.name           = "aclk_266_pre",
	},
	.sources = &exynos4415_clkset_mout_aclk_266_pre,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 12, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 0, .size = 3 },
};

/* aclk_266 */
static struct clk *exynos4415_clkset_mout_aclk_266_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_aclk_266_pre.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_aclk_266 = {
	.sources        = exynos4415_clkset_mout_aclk_266_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mout_aclk_266_list),
};

static struct clksrc_clk exynos4415_clk_aclk_266 = {
	.clk    = {
		.name           = "aclk_266",
	},
	.sources = &exynos4415_clkset_mout_aclk_266,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 20, .size = 1 },
};

/* aclk_100 */
static struct clksrc_clk exynos4415_clk_aclk_100 = {
	.clk    = {
		.name           = "aclk_100",
	},
	.sources = &exynos4415_clkset_mout_aclk,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 4, .size = 4 },
};

/* aclk_160 */
static struct clksrc_clk exynos4415_clk_aclk_160 = {
	.clk    = {
		.name           = "aclk_160",
	},
	.sources = &exynos4415_clkset_mout_aclk,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 8, .size = 3 },
};

/* aclk_200 */
static struct clksrc_clk exynos4415_clk_aclk_200 = {
	.clk    = {
		.name           = "aclk_200",
	},
	.sources = &exynos4415_clkset_mout_aclk,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 24, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 12, .size = 3 },
};

/* aclk_isp0_400_pre */
static struct clksrc_clk exynos4415_clk_dout_aclk_400_mcuisp_pre = {
	.clk    = {
		.name           = "aclk_isp0_400_pre",
	},
	.sources = &exynos4415_clkset_mout_aclk,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP1, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 24, .size = 3 },
};

/* aclk_isp0_400 */
static struct clk *exynos4415_clkset_aclk_400_mcuisp_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_dout_aclk_400_mcuisp_pre.clk,
};

static struct clksrc_sources exynos4415_clkset_aclk_400_mcuisp = {
	.sources        = exynos4415_clkset_aclk_400_mcuisp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_aclk_400_mcuisp_list),
};

static struct clksrc_clk exynos4415_clk_aclk_400_mcuisp = {
	.clk    = {
		.name           = "aclk_isp0_400",
	},
	.sources = &exynos4415_clkset_aclk_400_mcuisp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 24, .size = 1 },
};

/* mout_ebi_pre */
static struct clk *exynos4415_clkset_mout_ebi_pre_list[] = {
	[0] = &exynos4415_clk_aclk_200.clk,
	[1] = &exynos4415_clk_aclk_160.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_ebi_pre = {
	.sources        = exynos4415_clkset_mout_ebi_pre_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mout_ebi_pre_list),
};

static struct clksrc_clk exynos4415_clk_mout_ebi_pre = {
	.clk    = {
		.name           = "mout_ebi_pre",
	},
	.sources = &exynos4415_clkset_mout_ebi_pre,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 28, .size = 1 },
};

/* mout_ebi_1 */
static struct clk *exynos4415_clkset_sclk_ebi_list[] = {
	[0] = &exynos4415_clk_mout_ebi_pre.clk,
	[1] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_sclk_ebi = {
	.sources        = exynos4415_clkset_sclk_ebi_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_sclk_ebi_list),
};

static struct clksrc_clk exynos4415_clk_sclk_ebi = {
	.clk    = {
		.name           = "sclk_ebi",
	},
	.sources = &exynos4415_clkset_sclk_ebi,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TOP0, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TOP, .shift = 16, .size = 3 },
};

/* CMU_LEFT_BUS */
/* mout_mpll_user_leftbus */
static struct clk *exynos4415_clkset_mpll_user_leftbus_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_mpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mpll_user_leftbus = {
	.sources        = exynos4415_clkset_mpll_user_leftbus_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mpll_user_leftbus_list),
};

static struct clksrc_clk exynos4415_clk_mout_mpll_user_leftbus = {
	.clk    = {
		.name           = "mout_mpll_user_leftbus",
	},
	.sources = &exynos4415_clkset_mpll_user_leftbus,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_LEFTBUS, .shift = 4, .size = 1 },
};

/* aclk_gdl */
static struct clk *exynos4415_clkset_gdl_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_leftbus.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_gdl = {
	.sources        = exynos4415_clkset_gdl_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_gdl_list),
};

static struct clksrc_clk exynos4415_clk_aclk_gdl = {
	.clk    = {
		.name           = "aclk_gdl",
	},
	.sources = &exynos4415_clkset_gdl,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_LEFTBUS, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_LEFTBUS, .shift = 0, .size = 4 },
};

/* aclk_gpl */
static struct clksrc_clk exynos4415_clk_aclk_gpl = {
	.clk    = {
		.name           = "aclk_gpl",
		.parent		= &exynos4415_clk_aclk_gdl.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_LEFTBUS, .shift = 4, .size = 3 },

};

/* CMU_RIGHT_BUS */
/* mout_mpll_user_rightbus */
static struct clk *exynos4415_clkset_mpll_user_rightbus_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_mpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mpll_user_rightbus = {
	.sources        = exynos4415_clkset_mpll_user_rightbus_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mpll_user_rightbus_list),
};

static struct clksrc_clk exynos4415_clk_mout_mpll_user_rightbus = {
	.clk    = {
		.name           = "mout_mpll_user_rightbus",
	},
	.sources = &exynos4415_clkset_mpll_user_rightbus,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_RIGHTBUS, .shift = 4, .size = 1 },
};

/* aclk_gdr */
static struct clk *exynos4415_clkset_gdr_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_rightbus.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_gdr = {
	.sources        = exynos4415_clkset_gdr_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_gdr_list),
};

static struct clksrc_clk exynos4415_clk_aclk_gdr = {
	.clk    = {
		.name           = "aclk_gdr",
	},
	.sources = &exynos4415_clkset_gdr,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_RIGHTBUS, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_RIGHTBUS, .shift = 0, .size = 4 },
};

/* aclk_gpr */
static struct clksrc_clk exynos4415_clk_aclk_gpr = {
	.clk    = {
		.name           = "aclk_gpr",
		.parent		= &exynos4415_clk_aclk_gdr.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_RIGHTBUS, .shift = 4, .size = 3 },
};

/* PERIL BLK */
/* uart1~3 */
static struct clk *exynos4415_clkset_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset = {
	.sources        = exynos4415_clkset_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_list),
};

static struct clksrc_clk exynos4415_clk_sclk_uart0 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.0",
		.enable		= exynos4415_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL0, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL0, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_sclk_uart1 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.1",
		.enable		= exynos4415_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL0, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL0, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_sclk_uart2 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.2",
		.enable		= exynos4415_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 8),
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL0, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL0, .shift = 8, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_sclk_uart3 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.3",
		.enable		= exynos4415_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 12),
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL0, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL0, .shift = 12, .size = 4 },
};

/* dout_sclk_spi0~2 */
static struct clksrc_clk exynos4415_clk_dout_spi0 = {
	.clk		= {
		.name		= "dout_spi0",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_spi1 = {
	.clk		= {
		.name		= "dout_spi1",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 20, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_spi2 = {
	.clk		= {
		.name		= "dout_spi2",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 24, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL2, .shift = 0, .size = 4 },
};

/* sclk_spi0~2 */
static struct clksrc_clk exynos4415_clk_sclk_spi0 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos4415_clk_dout_spi0.clk,
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 16),
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL1, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4415_clk_sclk_spi1 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos4415_clk_dout_spi1.clk,
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 20),
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL1, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos4415_clk_sclk_spi2 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.2",
		.parent		= &exynos4415_clk_dout_spi2.clk,
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 24),
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL2, .shift = 8, .size = 8 },
};

/* G3D block */
/* mout_g3d0 */
static struct clk *exynos4415_clkset_mout_g3d0_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_top.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_mout_g3d0 = {
	.sources    = exynos4415_clkset_mout_g3d0_list,
	.nr_sources = ARRAY_SIZE(exynos4415_clkset_mout_g3d0_list),
};

static struct clksrc_clk exynos4415_clk_mout_g3d0 = {
	.clk    = {
		.name       = "mout_g3d0",
	},
	.sources = &exynos4415_clkset_mout_g3d0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_G3D, .shift = 0, .size = 1 },
};

/* mout_g3d1 */
static struct clk *exynos4415_clkset_mout_g3d1_list[] = {
	[0] = &exynos4415_clk_sclk_epll.clk,
	[1] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_g3d1 = {
	.sources    = exynos4415_clkset_mout_g3d1_list,
	.nr_sources = ARRAY_SIZE(exynos4415_clkset_mout_g3d1_list),
};

static struct clksrc_clk exynos4415_clk_mout_g3d1 = {
	.clk    = {
		.name       = "mout_g3d1",
	},
	.sources = &exynos4415_clkset_mout_g3d1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_G3D, .shift = 4, .size = 1 },
};

/* mout_g3d source list */
static struct clk *exynos4415_clkset_mout_g3d_list[] = {
	[0] = &exynos4415_clk_mout_g3d0.clk,
	[1] = &exynos4415_clk_mout_g3d1.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_g3d = {
	.sources    = exynos4415_clkset_mout_g3d_list,
	.nr_sources = ARRAY_SIZE(exynos4415_clkset_mout_g3d_list),
};

/* MFC block */
/* mout_mfc0 */
static struct clk *exynos4415_clkset_mout_mfc0_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_top.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_mout_mfc0 = {
	.sources	= exynos4415_clkset_mout_mfc0_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_mfc0_list),
};

static struct clksrc_clk exynos4415_clk_mout_mfc0 = {
	.clk	= {
		.name		= "mout_mfc0",
	},
	.sources = &exynos4415_clkset_mout_mfc0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_MFC, .shift = 0, .size = 1 },
};

/* mout_mfc1 */
static struct clk *exynos4415_clkset_mout_mfc1_list[] = {
	[0] = &exynos4415_clk_sclk_epll.clk,
	[1] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_mfc1 = {
	.sources	= exynos4415_clkset_mout_mfc1_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_mfc1_list),
};

static struct clksrc_clk exynos4415_clk_mout_mfc1 = {
	.clk	= {
		.name		= "mout_mfc1",
	},
	.sources = &exynos4415_clkset_mout_mfc1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_MFC, .shift = 4, .size = 1 },
};

/* mout_mfc source list */
static struct clk *exynos4415_clkset_mout_mfc_list[] = {
	[0] = &exynos4415_clk_mout_mfc0.clk,
	[1] = &exynos4415_clk_mout_mfc1.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_mfc = {
	.sources	= exynos4415_clkset_mout_mfc_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_mfc_list),
};

/* scclk_audio 0~2 */
static	struct clk exynos4415_clk_audiocdclk0 = {
	.name		= "audiocdclk0",
};

static struct clk exynos4415_clk_audiocdclk1 = {
	.name		= "audiocdclk1",
};

static struct clk exynos4415_clk_audiocdclk2 = {
	.name		= "audiocdclk2",
};

/* sclk_audio0 */
static struct clk *exynos4415_clkset_sclk_audio0_list[] = {
	[0] = &exynos4415_clk_audiocdclk0,
	[1] = NULL,	/* reseved */
	[2] = NULL,	/* No clock */
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = &clk_xusbxti,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_sclk_audio0 = {
	.sources	= exynos4415_clkset_sclk_audio0_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_audio0_list),
};

static struct clksrc_clk exynos4415_clk_sclk_audio0 = {
	.clk	= {
		.name		= "sclk_audio0",
		.enable		= exynos4415_clksrc_mask_maudio_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset_sclk_audio0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_MAUDIO, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_MAUDIO, .shift = 0, .size = 4 },
};

/* sclk_audio1 */
static struct clk *exynos4415_clkset_sclk_audio1_list[] = {
	[0] = &exynos4415_clk_audiocdclk1,
	[1] = NULL,	/* reseved */
	[2] = NULL,	/* No clock */
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = &clk_xusbxti,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_sclk_audio1 = {
	.sources	= exynos4415_clkset_sclk_audio1_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_audio1_list),
};

static struct clksrc_clk exynos4415_clk_sclk_audio1 = {
	.clk	= {
		.name		= "sclk_audio1",
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset_sclk_audio1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL4, .shift = 0, .size = 4 },
};

/* sclk_audio2 */
static struct clk *exynos4415_clkset_sclk_audio2_list[] = {
	[0] = &exynos4415_clk_audiocdclk2,
	[1] = NULL,	/* reseved */
	[2] = NULL,	/* No clock */
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = &clk_xusbxti,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_sclk_audio2 = {
	.sources	= exynos4415_clkset_sclk_audio2_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_audio2_list),
};

static struct clksrc_clk exynos4415_clk_sclk_audio2 = {
	.clk	= {
		.name		= "sclk_audio2",
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos4415_clkset_sclk_audio2,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL4, .shift = 16, .size = 4 },
};

/* sclk_spdif */
static struct clk exynos4415_clk_spdif_extclk = {
	.name		= "spdif_extclk",
};

static struct clk *exynos4415_clkset_sclk_spdif_list[] = {
	[0] = &exynos4415_clk_sclk_audio0.clk,
	[1] = &exynos4415_clk_sclk_audio1.clk,
	[2] = &exynos4415_clk_sclk_audio2.clk,
	[3] = &exynos4415_clk_spdif_extclk,
};

static struct clksrc_sources exynos4415_clkset_sclk_spdif = {
	.sources	= exynos4415_clkset_sclk_spdif_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_spdif_list),
};

static struct clksrc_clk exynos4415_clk_sclk_spdif = {
	.clk	= {
		.name		= "sclk_spdif",
		.enable		= exynos4415_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 8),
	},
	.sources = &exynos4415_clkset_sclk_spdif,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_PERIL1, .shift = 8, .size = 2 },
};

/* dout_mipi0 */
static struct clk *exynos4415_clkset_mipi0_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = NULL,
	[5] = &exynos4415_clk_mout_disp_pll.clk,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,

};

static struct clksrc_sources exynos4415_clkset_mipi0 = {
	.sources        = exynos4415_clkset_mipi0_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_mipi0_list),
};

static struct clksrc_clk exynos4415_clk_dout_mipi0 = {
	.clk    = {
		.name           = "dout_mipi0",
	},
	.sources = &exynos4415_clkset_mipi0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_LCD, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_LCD, .shift = 16, .size = 4 },
};

/* sclk_mipi */
static struct clksrc_clk exynos4415_clk_sclk_mipi0 = {
	.clk    = {
		.name           = "sclk_mipi0",
		.parent		= &exynos4415_clk_dout_mipi0.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_LCD, .shift = 20, .size = 4 },
};

/* FSYS_CLK */
/* dout_mmc0 */
static struct clksrc_clk exynos4415_clk_dout_mmc0 = {
	.clk	= {
		.name		= "dout_mmc0",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_FSYS, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS1, .shift = 0, .size = 4 },
};

/* dout_mmc1 */
static struct clksrc_clk exynos4415_clk_dout_mmc1 = {
	.clk	= {
		.name		= "dout_mmc1",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_FSYS, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS1, .shift = 16, .size = 4 },
};

/* dout_mmc2 */
static struct clksrc_clk exynos4415_clk_dout_mmc2 = {
	.clk	= {
		.name		= "dout_mmc2",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_FSYS, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS2, .shift = 0, .size = 4 },
};

/* dout_tsadc */
static struct clksrc_clk exynos4415_clk_dout_tsadc = {
	.clk    = {
		.name           = "dout_tsadc",
	},
	.sources = &exynos4415_clkset,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_FSYS, .shift = 28, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS0, .shift = 0, .size = 4 },
};

/* sclk_tsadc */
static struct clksrc_clk exynos4415_clk_sclk_tsadc = {
	.clk    = {
		.name           = "sclk_tsadc",
		.parent		= &exynos4415_clk_dout_tsadc.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS0, .shift = 8, .size = 8 },
};

/* TV_BLK */
/* sclk_hdmiphy */
static struct clk exynos4415_clk_sclk_hdmiphy = {
	.name		= "sclk_hdmiphy",
};

/* sclk_pixel */
static struct clksrc_clk exynos4415_clk_sclk_pixel = {
	.clk		= {
		.name		= "sclk_pixel",
		.parent		= &exynos4415_clk_sclk_vpll.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_TV, .shift = 0, .size = 4 },
};

static struct clk *exynos4415_clkset_sclk_hdmi_list[] = {
	[0] = &exynos4415_clk_sclk_pixel.clk,
	[1] = &exynos4415_clk_sclk_hdmiphy,
};

/* sclk_hdmi */
static struct clksrc_sources exynos4415_clkset_sclk_hdmi = {
	.sources	= exynos4415_clkset_sclk_hdmi_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_sclk_hdmi_list),
};

static struct clksrc_clk exynos4415_clk_sclk_hdmi = {
	.clk		= {
		.name		= "sclk_hdmi",
		.enable		= exynos4415_clksrc_mask_tv_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset_sclk_hdmi,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_TV, .shift = 0, .size = 1 },
};

/* clkout */
struct clk *exynos4415_clkset_clk_clkout_list[] = {
	/* Others are for debugging */
	[8] = &clk_xxti,
	[9] = &clk_xusbxti,
};

struct clksrc_sources exynos4415_clkset_clk_clkout = {
	.sources	= exynos4415_clkset_clk_clkout_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_clk_clkout_list),
};

static struct clksrc_clk exynos4415_clk_clkout = {
	.clk	= {
		.name		= "clkout",
		.enable		= exynos4415_clk_clkout_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4415_clkset_clk_clkout,
	.reg_src = { .reg = EXYNOS_PMU_DEBUG, .shift = 8, .size = 5 },
};

static struct clk exynos4415_init_clocks_on[] = {
	{
		.name		= "timers",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "csis",
		.devname	= "s5p-mipi-csis.0",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "csis",
		.devname	= "s5p-mipi-csis.1",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "jpeg",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.0",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.1",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.2",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.3",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "pxl_async0",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "pxl_async1",
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "hsmmc",
		.devname	= "dw_mmc.0",
		.parent		= &exynos4415_clk_aclk_200.clk,
		.enable		= exynos4415_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "hsmmc",
		.devname	= "dw_mmc.1",
		.parent		= &exynos4415_clk_aclk_200.clk,
		.enable		= exynos4415_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "hsmmc",
		.devname	= "dw_mmc.2",
		.parent		= &exynos4415_clk_aclk_200.clk,
		.enable		= exynos4415_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "mixer",
		.devname	= "s5p-mixer",
		.enable		= exynos4415_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "vp",
		.devname	= "s5p-mixer",
		.enable		= exynos4415_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "hdmi",
		.devname	= "exynos4-hdmi",
		.enable		= exynos4415_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "keypad",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "rtc",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 15),
	}, {
		.name		= "watchdog",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "usbhost",
		.enable		= exynos4415_clk_ip_fsys_ctrl ,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "otg",
		.enable		= exynos4415_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.0",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.1",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.2",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.1",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.1",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.2",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "spdif",
		.devname	= "samsung-spdif",
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 26),
	}, {
		.name		= "mfc",
		.devname	= "s5p-mfc",
		.enable		= exynos4415_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.0",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.1",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.2",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.4",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.5",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.6",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.7",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-hdmiphy-i2c",
		.parent		= &exynos4415_clk_aclk_100.clk,
		.enable		= exynos4415_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "adc",
		.parent		= &exynos4415_clk_aclk_200.clk,
		.enable		= exynos4415_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mfc_lr, 0),
		.enable		= exynos4415_clk_ip_mfc_ctrl,
		.ctrlbit	= (3 << 1),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(tv, 2),
		.enable		= exynos4415_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(jpeg, 3),
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(rot, 4),
		.enable		= exynos4415_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc0, 5),
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc1, 6),
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc2, 7),
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc3, 8),
		.enable		= exynos4415_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd1, 10),
		.enable		= exynos4415_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "ppmug3d",
		.enable		= exynos4415_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "dsim1",
		.enable		= exynos4415_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 3),
	},
};

static struct clk exynos4415_init_clocks_off[] = {
};

/* dma */
static struct clk exynos4415_clk_pdma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.0",
	.enable		= exynos4415_clk_ip_fsys_ctrl,
	.ctrlbit	= (1 << 0),
};

static struct clk exynos4415_clk_pdma1 = {
	.name		= "dma",
	.devname	= "dma-pl330.1",
	.enable		= exynos4415_clk_ip_fsys_ctrl,
	.ctrlbit	= (1 << 1),
};

static struct clk exynos4415_clk_mdma1 = {
	.name		= "dma",
	.devname	= "dma-pl330.2",
	.enable		= exynos4415_clk_ip_image_ctrl,
	.ctrlbit	= ((1 << 8) | (1 << 5) | (1 << 2)),
};

static struct clk exynos4415_clk_adma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.3",
};

/* fimd0 */
static struct clk exynos4415_clk_fimd1 = {
	.name		= "fimd",
	.devname	= "exynos4-fb.1",
	.enable		= exynos4415_clk_ip_lcd_ctrl,
	.ctrlbit	= (1 << 0),
};

static struct clk *exynos4415_clk_cdev[] = {
	&clk_fout_epll,
	&exynos4415_clk_pdma0,
	&exynos4415_clk_pdma1,
	&exynos4415_clk_mdma1,
	&exynos4415_clk_adma0,
	&exynos4415_clk_fimd1,
};

/* CAM BLOCK */
/* mout_jpeg_0 */
static struct clk *exynos4415_clkset_jpeg_0_list[] = {
	[0] = &exynos4415_clk_mout_mpll_user_top.clk,
	[1] = NULL,
};

static struct clksrc_sources exynos4415_clkset_jpeg_0 = {
	.sources        = exynos4415_clkset_jpeg_0_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_jpeg_0_list),
};

static struct clksrc_clk exynos4415_mout_jpeg_0 = {
	.clk    = {
		.name           = "mout_jpeg_0",
	},
	.sources = &exynos4415_clkset_jpeg_0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM1, .shift = 0, .size = 1 },
};

/* mout_jpeg_1 */
static struct clk *exynos4415_clkset_jpeg_1_list[] = {
	[0] = &exynos4415_clk_sclk_epll.clk,
	[1] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_jpeg_1 = {
	.sources        = exynos4415_clkset_jpeg_1_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_jpeg_1_list),
};

static struct clksrc_clk exynos4415_mout_jpeg_1 = {
	.clk    = {
		.name           = "mout_jpeg_1",
	},
	.sources = &exynos4415_clkset_jpeg_1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM1, .shift = 4, .size = 1 },
};

/* mout_jpeg */
static struct clk *exynos4415_clkset_jpeg_list[] = {
	[0] = &exynos4415_mout_jpeg_0.clk,
	[1] = &exynos4415_mout_jpeg_1.clk,
};

static struct clksrc_sources exynos4415_clkset_jpeg = {
	.sources        = exynos4415_clkset_jpeg_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_jpeg_list),
};

static struct clksrc_clk exynos4415_clk_mout_jpeg = {
	.clk    = {
		.name           = "mout_jpeg",
	},
	.sources = &exynos4415_clkset_jpeg,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM1, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM1, .shift = 0, .size = 4 },
};

static struct clk *exynos4415_clkset_cam_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = NULL,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_cam = {
	.sources        = exynos4415_clkset_cam_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_cam_list),
};

/* dout_csis0 */
static struct clksrc_clk exynos4415_sclk_csis0 = {
	.clk    = {
		.name           = "sclk_csis0",
	},
	.sources = &exynos4415_clkset_cam,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 24, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 24, .size = 4 },
};

/* sclk_csis1 */
static struct clksrc_clk exynos4415_sclk_csis1 = {
	.clk    = {
		.name           = "sclk_csis1",
	},
	.sources = &exynos4415_clkset_cam,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 28, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 28, .size = 4 },
};

/* sclk_cam1 */
static struct clksrc_clk exynos4415_sclk_cam1 = {
	.clk    = {
		.name           = "sclk_cam0",
	},
	.sources = &exynos4415_clkset_cam,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 20, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 20, .size = 4 },
};

/* sclk_pxlasync_csis0_fimc */
static struct clk *exynos4415_clkset_pxlasync_csis_fimc_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = NULL,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_pxlasync_csis_fimc = {
	.sources        = exynos4415_clkset_pxlasync_csis_fimc_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_pxlasync_csis_fimc_list),
};

static struct clksrc_clk exynos4415_sclk_pxlasync_csis0_fimc = {
	.clk    = {
		.name           = "sclk_pxlasync_csis0_fimc",
	},
	.sources = &exynos4415_clkset_pxlasync_csis_fimc,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM1, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM1, .shift = 20, .size = 4 },
};

/* sclk_pxlasync_csis1_fimc */
static struct clksrc_clk exynos4415_sclk_pxlasync_csis1_fimc = {
	.clk    = {
		.name           = "sclk_pxlasync_csis1_fimc",
	},
	.sources = &exynos4415_clkset_pxlasync_csis_fimc,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM1, .shift = 20, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM1, .shift = 24, .size = 4 },
};

/* ISP_BLK */
/* dout_tsadc_isp */
static struct clk *exynos4415_clkset_isp_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = NULL,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4415_clkset_isp = {
	.sources        = exynos4415_clkset_isp_list,
	.nr_sources     = ARRAY_SIZE(exynos4415_clkset_isp_list),
};

static struct clksrc_clk exynos4415_clk_dout_tsadc_isp = {
	.clk    = {
		.name           = "dout_tsadc_isp",
	},
	.sources = &exynos4415_clkset_isp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP1, .shift = 0, .size = 4 },
};

/* sclk_tsadc_isp */
static struct clksrc_clk exynos4415_clk_sclk_tsadc_isp = {
	.clk    = {
		.name           = "sclk_tsadc_isp",
		.parent		= &exynos4415_clk_dout_tsadc_isp.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP1, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4415_clk_sclk_pwm_isp = {
	.clk = {
		.name		= "sclk_pwm_isp",
	},
	.sources = &exynos4415_clkset_isp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_spi0_isp = {
	.clk = {
		.name		= "dout_spi0_isp",
	},
	.sources = &exynos4415_clkset_isp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_spi1_isp = {
	.clk = {
		.name		= "dout_spi1_isp",
	},
	.sources = &exynos4415_clkset_isp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 16, .size = 4 },
};

static struct clksrc_clk eyxnos4415_clk_sclk_uart_isp = {
	.clk = {
		.name		= "sclk_uart_isp",
	},
	.sources = &exynos4415_clkset_isp,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 28, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_sclk_spi0_isp = {
	.clk = {
		.name		= "sclk_spi0_isp",
		.parent		= &exynos4415_clk_dout_spi0_isp.clk
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4415_clk_sclk_spi1_isp = {
	.clk = {
		.name		= "sclk_spi1_isp",
		.parent 	= &exynos4415_clk_dout_spi1_isp.clk
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 20, .size = 8 },
};

/* ISP_BLK */
static struct clk *exynos4415_clkset_mout_aclk_isp_300_list[] = {
	[0] = &exynos4415_clk_mout_isp_pll.clk,
	[1] = &exynos4415_clk_sclk_mpll.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_aclk_isp0_300 = {
	.sources	= exynos4415_clkset_mout_aclk_isp_300_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_aclk_isp_300_list),
};

static struct clksrc_clk exynos4415_clk_mout_aclk_isp0_300 = {
	.clk	= {
		.name		= "mout_aclk_isp0_300",
	},
	.sources = &exynos4415_clkset_mout_aclk_isp0_300,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP0, .shift = 8, .size = 1 },
};

static struct clksrc_sources exynos4415_clkset_mout_aclk_isp1_300 = {
	.sources	= exynos4415_clkset_mout_aclk_isp_300_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_aclk_isp_300_list),
};

static struct clksrc_clk exynos4415_clk_mout_aclk_isp1_300 = {
	.clk	= {
		.name		= "mout_aclk_isp1_300",
	},
	.sources = &exynos4415_clkset_mout_aclk_isp1_300,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP1, .shift = 4, .size = 1 },
};

static struct clksrc_clk exynos4415_clk_dout_aclk_isp1_300 = {
	.clk	= {
		.name		= "dout_aclk_isp1_300",
		.parent		= &exynos4415_clk_mout_aclk_isp1_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_aclk_isp0_300 = {
	.clk	= {
		.name		= "dout_aclk_isp0_300",
		.parent		= &exynos4415_clk_mout_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 0, .size = 4 },
};

static struct clk *exynos4415_clkset_aclk_isp0_300_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_dout_aclk_isp0_300.clk,
};

static struct clksrc_sources exynos4415_clkset_aclk_isp0_300 = {
	.sources	= exynos4415_clkset_aclk_isp0_300_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_aclk_isp0_300_list),
};

static struct clksrc_clk exynos4415_clk_aclk_isp0_300 = {
	.clk	= {
		.name		= "aclk_isp0_300",
	},
	.sources = &exynos4415_clkset_aclk_isp0_300,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP0, .shift = 0, .size = 1 },
};

static struct clk *exynos4415_clkset_aclk_isp1_300_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_dout_aclk_isp1_300.clk,
};

static struct clksrc_sources exynos4415_clkset_aclk_isp1_300 = {
	.sources	= exynos4415_clkset_aclk_isp1_300_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_aclk_isp1_300_list),
};

static struct clksrc_clk exynos4415_clk_aclk_isp1_300 = {
	.clk	= {
		.name		= "aclk_isp1_300",
	},
	.sources = &exynos4415_clkset_aclk_isp1_300,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP1, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos4415_clk_dout_sclk_csis1 = {
	.clk	= {
		.name		= "dout_sclk_csis1",		/* dout_sclk_csis1 */
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0_SUB0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos4415_clk_dout_aclk_3aa1 = {
	.clk	= {
		.name		= "dout_aclk_3aa1",		/* dout_aclk_3aa1 */
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0_SUB0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos4415_clk_dout_aclk_lite_b = {
	.clk	= {
		.name		= "dout_aclk_lite_b",		/* dout_aclk_lite_b */
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0_SUB0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos4415_clk_dout_pclk_isp0_a_150 = {
	.clk	= {
		.name		= "dout_pclk_isp0_a_150",
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 20, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_pclk_isp0_b_75 = {
	.clk	= {
		.name		= "dout_pclk_isp0_b_75",
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_sclk_isp_mpwm = {
	.clk	= {
		.name		= "sclk_isp_mpwm",
		.parent		= &exynos4415_clk_dout_pclk_isp0_b_75.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_pclk_isp0_b_150 = {
	.clk	= {
		.name		= "dout_pclk_isp0_b_150",
		.parent		= &exynos4415_clk_aclk_isp0_300.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_atclk = {
	.clk	= {
		.name		= "dout_atclk",
		.parent		= &exynos4415_clk_aclk_400_mcuisp.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 8, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_pclkdbg = {
	.clk	= {
		.name		= "dout_pclkdbg",
		.parent		= &exynos4415_clk_aclk_400_mcuisp.clk,
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP0, .shift = 12, .size = 4 },
};

/* PHCLK_ISP_BLK */
static struct clk exynos4415_clk_phyclk_rxbyte_clkhs0_s2a = {
	.name	= "phyclk_rxbyte_clkhs0_s2a",
	.id	= -1,
};

static struct clk exynos4415_clk_phyclk_rxbyte_clkhs0_s4 = {
	.name	= "phyclk_rxbyte_clkhs0_s4",
	.id	= -1,
};

static struct clk *exynos4415_clkset_phyclk_rxbyte_clkhs0_s2a_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_phyclk_rxbyte_clkhs0_s2a,
};

static struct clksrc_sources exynos4415_clkset_phyclk_rxbyte_clkhs0_s2a = {
	.sources	= exynos4415_clkset_phyclk_rxbyte_clkhs0_s2a_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_phyclk_rxbyte_clkhs0_s2a_list),
};

static struct clksrc_clk exynos4415_clk_mout_phyclk_rxbyte_clkhs0_s2a = {
	.clk	= {
		.name		= "mout_phyclk_rxbyte_clkhs0_s2a",
	},
	.sources = &exynos4415_clkset_phyclk_rxbyte_clkhs0_s2a,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 24, .size = 1 },
};

static struct clk *exynos4415_clkset_phyclk_rxbyte_clkhs0_s4_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_phyclk_rxbyte_clkhs0_s4,
};

static struct clksrc_sources exynos4415_clkset_phyclk_rxbyte_clkhs0_s4 = {
	.sources	= exynos4415_clkset_phyclk_rxbyte_clkhs0_s4_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_phyclk_rxbyte_clkhs0_s4_list),
};

static struct clksrc_clk exynos4415_clk_mout_phyclk_rxbyte_clkhs0_s4 = {
	.clk	= {
		.name		= "mout_phyclk_rxbyte_clkhs0_s4",
	},
	.sources = &exynos4415_clkset_phyclk_rxbyte_clkhs0_s4,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 20, .size = 1 },
};

static struct clk *exynos4415_clkset_mout_sclk_isp_pwm_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_pwm_isp.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_sclk_isp_pwm = {
	.sources	= exynos4415_clkset_mout_sclk_isp_pwm_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_sclk_isp_pwm_list),
};

static struct clksrc_clk exynos4415_clk_mout_sclk_isp_pwm = {
	.clk	= {
		.name		= "mout_sclk_isp_pwm",
	},
	.sources = &exynos4415_clkset_mout_sclk_isp_pwm,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 16, .size = 1 },
};

static struct clk *exynos4415_clkset_mout_sclk_isp_mtcadc_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_tsadc_isp.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_sclk_isp_mtcadc = {
	.sources	= exynos4415_clkset_mout_sclk_isp_mtcadc_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_sclk_isp_mtcadc_list),
};

static struct clksrc_clk exynos4415_clk_mout_sclk_isp_mtcadc = {
	.clk	= {
		.name		= "mout_sclk_isp_mtcadc",
	},
	.sources = &exynos4415_clkset_mout_sclk_isp_mtcadc,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 12, .size = 1 },
};

static struct clk *exynos4415_clkset_mout_sclk_isp_spi0_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_spi0_isp.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_sclk_isp_spi0 = {
	.sources	= exynos4415_clkset_mout_sclk_isp_spi0_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_sclk_isp_spi0_list),
};

static struct clksrc_clk exynos4415_clk_mout_sclk_isp_spi0 = {
	.clk	= {
		.name		= "mout_sclk_isp_spi0",
	},
	.sources = &exynos4415_clkset_mout_sclk_isp_spi0,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 8, .size = 1 },
};

static struct clk *exynos4415_clkset_mout_sclk_isp_spi1_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos4415_clk_sclk_spi1_isp.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_sclk_isp_spi1 = {
	.sources	= exynos4415_clkset_mout_sclk_isp_spi1_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_sclk_isp_spi1_list),
};

static struct clksrc_clk exynos4415_clk_mout_sclk_isp_spi1 = {
	.clk	= {
		.name		= "mout_sclk_isp_spi1",
	},
	.sources = &exynos4415_clkset_mout_sclk_isp_spi1,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 4, .size = 1 },
};

static struct clk *exynos4415_clkset_mout_sclk_isp_uart_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &eyxnos4415_clk_sclk_uart_isp.clk,
};

static struct clksrc_sources exynos4415_clkset_mout_sclk_isp_uart = {
	.sources	= exynos4415_clkset_mout_sclk_isp_uart_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_mout_sclk_isp_uart_list),
};

static struct clksrc_clk exynos4415_clk_mout_sclk_isp_uart = {
	.clk	= {
		.name		= "mout_sclk_isp_uart",
	},
	.sources = &exynos4415_clkset_mout_sclk_isp_uart,
	.reg_src = { .reg = EXYNOS4415_CLKSRC_CMU_ISP0, .shift = 0, .size = 1 },
};

/* CLK_ISP1 */
static struct clksrc_clk exynos4415_clk_dout_pclk_isp1_150 = {
	.clk	= {
		.name		= "dout_pclk_isp1_150",
		.parent		= &exynos4415_clk_aclk_isp1_300.clk
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4415_clk_dout_pclk_isp1_75 = {
	.clk	= {
		.name		= "dout_pclk_isp1_75",
		.parent		= &exynos4415_clk_aclk_isp1_300.clk
	},
	.reg_div = { .reg = EXYNOS4415_CLKDIV_CMU_ISP1, .shift = 4, .size = 4 },
};

/* Clock initialization code */
static struct clksrc_clk *exynos4415_sysclks[] = {
	&exynos4415_clk_mout_apll,
	&exynos4415_clk_sclk_apll,
	&exynos4415_clk_mout_mpll,
	&exynos4415_clk_sclk_mpll,
	&exynos4415_clk_mout_mpll_user_cpu,
	&exynos4415_clk_mout_mpll_user_top,
	&exynos4415_clk_mout_mpll_user_acp,
	&exynos4415_clk_mout_bpll,
	&exynos4415_clk_sclk_bpll,
	&exynos4415_clk_mout_bpll_user_acp,
	&exynos4415_clk_mout_epll,
	&exynos4415_clk_sclk_epll,
	&exynos4415_clk_vpllsrc,
	&exynos4415_clk_sclk_vpll,
	&exynos4415_clk_moutcore,
	&exynos4415_clk_coreclk,
	&exynos4415_clk_armclk,
	&exynos4415_clk_corerem0,
	&exynos4415_clk_corerem1,
	&exynos4415_clk_atb,
	&exynos4415_clk_pclk_dbg,
	&exynos4415_clk_dout_copy,
	&exynos4415_clk_sclk_hpm,
	&exynos4415_clk_mout_dmc_bus,
	&exynos4415_clk_dout_dmc_pre,
	&exynos4415_clk_sclk_dmc,
	&exynos4415_clk_aclk_dmcd,
	&exynos4415_clk_aclk_dmcp,
	&exynos4415_clk_sclk_dphy,
	&exynos4415_clk_mout_dmc_bus_acp,
	&exynos4415_clk_dout_acp_dmc,
	&exynos4415_clk_aclk_acp_dmcd,
	&exynos4415_clk_aclk_acp_dmcp,
	&exynos4415_clk_sclk_pwi,
	&exynos4415_clk_mout_g2d_acp_0,
	&exynos4415_clk_mout_g2d_acp_1,
	&exynos4415_clk_sclk_g2d_acp,
	&exynos4415_clk_mout_isp_pll,
	&exynos4415_clk_mout_disp_pll,
	&exynos4415_clk_aclk_266_pre,
	&exynos4415_clk_aclk_266,
	&exynos4415_clk_aclk_100,
	&exynos4415_clk_aclk_160,
	&exynos4415_clk_aclk_200,
	&exynos4415_clk_dout_aclk_400_mcuisp_pre,
	&exynos4415_clk_aclk_400_mcuisp,
	&exynos4415_clk_mout_ebi_pre,
	&exynos4415_clk_sclk_ebi,
	&exynos4415_clk_sclk_uart0,
	&exynos4415_clk_sclk_uart1,
	&exynos4415_clk_sclk_uart2,
	&exynos4415_clk_sclk_uart3,
	&exynos4415_clk_dout_spi0,
	&exynos4415_clk_dout_spi1,
	&exynos4415_clk_dout_spi2,
	&exynos4415_clk_sclk_spi0,
	&exynos4415_clk_sclk_spi1,
	&exynos4415_clk_sclk_spi2,
	&exynos4415_clk_sclk_audio0,
	&exynos4415_clk_sclk_audio1,
	&exynos4415_clk_sclk_audio2,
	&exynos4415_clk_sclk_spdif,
	&exynos4415_clk_dout_mipi0,
	&exynos4415_clk_sclk_mipi0,
	&exynos4415_clk_dout_mmc0,
	&exynos4415_clk_dout_mmc1,
	&exynos4415_clk_dout_mmc2,
	&exynos4415_clk_dout_tsadc,
	&exynos4415_clk_sclk_tsadc,
	&exynos4415_clk_mout_mpll_user_leftbus,
	&exynos4415_clk_aclk_gdl,
	&exynos4415_clk_aclk_gpl,
	&exynos4415_clk_mout_mpll_user_rightbus,
	&exynos4415_clk_aclk_gdr,
	&exynos4415_clk_aclk_gpr,
	&exynos4415_clk_mout_g3d0,
	&exynos4415_clk_mout_g3d1,
	&exynos4415_clk_mout_mfc0,
	&exynos4415_clk_mout_mfc1,
	&exynos4415_clk_sclk_hdmi,
	&exynos4415_clk_sclk_pixel,
	&exynos4415_clk_mout_jpeg,
	&exynos4415_mout_jpeg_0,
	&exynos4415_mout_jpeg_1,
	&exynos4415_sclk_csis0,
	&exynos4415_sclk_csis1,
	&exynos4415_sclk_cam1,
	&exynos4415_sclk_pxlasync_csis0_fimc,
	&exynos4415_sclk_pxlasync_csis1_fimc,
	&exynos4415_clk_dout_tsadc_isp,
	&exynos4415_clk_sclk_tsadc_isp,
	&exynos4415_clk_dout_spi0_isp,
	&exynos4415_clk_dout_spi1_isp,
	&exynos4415_clk_clkout,
	&exynos4415_clk_mout_aclk_isp0_300,
	&exynos4415_clk_mout_aclk_isp1_300,
	&exynos4415_clk_aclk_isp0_300,
	&exynos4415_clk_aclk_isp1_300,
	&exynos4415_clk_dout_sclk_csis1,
	&exynos4415_clk_dout_aclk_3aa1,
	&exynos4415_clk_dout_aclk_lite_b,
	&exynos4415_clk_dout_pclk_isp0_a_150,
	&exynos4415_clk_dout_pclk_isp0_b_75,
	&exynos4415_clk_dout_pclk_isp0_b_150,
	&exynos4415_clk_dout_atclk,
	&exynos4415_clk_dout_pclkdbg,
	&exynos4415_clk_mout_phyclk_rxbyte_clkhs0_s2a,
	&exynos4415_clk_mout_phyclk_rxbyte_clkhs0_s4,
	&exynos4415_clk_mout_sclk_isp_pwm,
	&exynos4415_clk_mout_sclk_isp_mtcadc,
	&exynos4415_clk_mout_sclk_isp_spi0,
	&exynos4415_clk_mout_sclk_isp_spi1,
	&exynos4415_clk_mout_sclk_isp_uart,
	&exynos4415_clk_dout_pclk_isp1_150,
	&exynos4415_clk_dout_pclk_isp1_75,
	&eyxnos4415_clk_sclk_uart_isp,
	&exynos4415_clk_sclk_spi0_isp,
	&exynos4415_clk_sclk_spi1_isp,
	&exynos4415_clk_dout_sclk_isp_mpwm,
	&exynos4415_clk_sclk_pwm_isp,
};

/* phy_mipi4l */
static struct clk exynos4415_clk_phy_mipi4l = {
	.name		= "phy_mipi4l",
};

struct clk *exynos4415_clkset_fimd_group_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = &exynos4415_clk_phy_mipi4l,
	[3] = &exynos4415_clk_mout_isp_pll.clk,
	[4] = &exynos4415_clk_mout_disp_pll.clk,
	[5] = &exynos4415_clk_sclk_hdmiphy,
	[6] = &exynos4415_clk_sclk_mpll.clk,
	[7] = &exynos4415_clk_sclk_epll.clk,
	[8] = &exynos4415_clk_sclk_vpll.clk,
};

struct clksrc_sources exynos4415_clkset_fimd_group = {
	.sources	= exynos4415_clkset_fimd_group_list,
	.nr_sources	= ARRAY_SIZE(exynos4415_clkset_fimd_group_list),
};

static struct clksrc_clk exynos4415_clksrcs[] = {
	{
		.clk	= {
			.name		= "sclk_pcm",
			.parent		= &exynos4415_clk_sclk_audio0.clk,
		},
			.reg_div = { .reg = EXYNOS4415_CLKDIV_MAUDIO, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pcm1",
			.parent		= &exynos4415_clk_sclk_audio1.clk,
		},
			.reg_div = { .reg = EXYNOS4415_CLKDIV_MAUDIO, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pcm2",
			.parent		= &exynos4415_clk_sclk_audio2.clk,
		},
			.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL4, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_i2s1",
			.parent		= &exynos4415_clk_sclk_audio1.clk,
		},
			.reg_div = { .reg = EXYNOS4415_CLKDIV_PERIL4, .shift = 20, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_fimd",
			.devname	= "exynos4-fb.1",
			.enable		= exynos4415_clksrc_mask_lcd_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos4415_clkset_fimd_group,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_LCD, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_LCD, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "dw_mmc.0",
			.parent		= &exynos4415_clk_dout_mmc0.clk,
			.enable		= exynos4415_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS1, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "dw_mmc.1",
			.parent         = &exynos4415_clk_dout_mmc1.clk,
			.enable		= exynos4415_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS1, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "dw_mmc.2",
			.parent         = &exynos4415_clk_dout_mmc2.clk,
			.enable		= exynos4415_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS4415_CLKDIV_FSYS2, .shift = 8, .size = 8 },
	}, {
		.clk    = {
			.name		= "sclk_g3d",
			.enable		= exynos4415_clk_ip_g3d_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos4415_clkset_mout_g3d,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_G3D, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_G3D, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mfc",
			.devname	= "s5p-mfc",
		},
		.sources = &exynos4415_clkset_mout_mfc,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_MFC, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_MFC, .shift = 0, .size = 4 },
	}, {
		.clk = {
			.name		= "sclk_uart_isp",
		},
		.sources = &exynos4415_clkset_isp,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_ISP, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 28, .size = 4 },
	}, {
		.clk = {
			.name		= "sclk_spi0_isp",
			.parent		= &exynos4415_clk_dout_spi0_isp.clk
		},
		.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 8, .size = 8 },
	}, {
		.clk = {
			.name		= "sclk_spi1_isp",
			.parent 	= &exynos4415_clk_dout_spi1_isp.clk
		},
		.reg_div = { .reg = EXYNOS4415_CLKDIV_ISP, .shift = 20, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.0",
			.enable		= exynos4415_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos4415_clkset_cam,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.1",
			.enable		= exynos4415_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos4415_clkset_cam,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 4, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 4, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.2",
			.enable		= exynos4415_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos4415_clkset_cam,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 8, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 8, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.3",
			.enable		= exynos4415_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos4415_clkset_cam,
		.reg_src = { .reg = EXYNOS4415_CLKSRC_CAM, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS4415_CLKDIV_CAM, .shift = 12, .size = 4 },
	},
};

static struct clk_lookup exynos4415_clk_lookup[] = {
	CLKDEV_INIT("exynos4210-uart.0", "clk_uart_baud0", &exynos4415_clk_sclk_uart0.clk),
	CLKDEV_INIT("exynos4210-uart.1", "clk_uart_baud0", &exynos4415_clk_sclk_uart1.clk),
	CLKDEV_INIT("exynos4210-uart.2", "clk_uart_baud0", &exynos4415_clk_sclk_uart2.clk),
	CLKDEV_INIT("exynos4210-uart.3", "clk_uart_baud0", &exynos4415_clk_sclk_uart3.clk),
	CLKDEV_INIT("exynos4-fb.1", "lcd", &exynos4415_clk_fimd1),
	CLKDEV_INIT("dma-pl330.0", "apb_pclk", &exynos4415_clk_pdma0),
	CLKDEV_INIT("dma-pl330.1", "apb_pclk", &exynos4415_clk_pdma1),
	CLKDEV_INIT("dma-pl330.2", "apb_pclk", &exynos4415_clk_mdma1),
	CLKDEV_INIT("dma-pl330.3", "apb_pclk", &exynos4415_clk_adma0),
	CLKDEV_INIT("s3c64xx-spi.0", "spi_busclk0", &exynos4415_clk_sclk_spi0.clk),
	CLKDEV_INIT("s3c64xx-spi.1", "spi_busclk0", &exynos4415_clk_sclk_spi1.clk),
	CLKDEV_INIT("s3c64xx-spi.2", "spi_busclk0", &exynos4415_clk_sclk_spi2.clk),
};

static struct clk *exynos4415_clks[] __initdata = {
	&clk_fout_isp_pll,
	&exynos4415_clk_audiocdclk0,
	&exynos4415_clk_audiocdclk1,
	&exynos4415_clk_audiocdclk2,
	&exynos4415_clk_spdif_extclk,
	&exynos4415_clk_phy_mipi4l,
	&exynos4415_clk_sclk_hdmiphy,
	&exynos4415_clk_phyclk_rxbyte_clkhs0_s2a,
	&exynos4415_clk_phyclk_rxbyte_clkhs0_s4,
};

/* VPLL(G3D PLL) Setup function */
static struct pll_div_data exynos4415_vpll_div[] = {
	{100000000, 3, 200, 4, 0, 0, 0},
	{160000000, 2, 107, 3, -21485, 0, 0},
	{266000000, 3, 266, 3, 0, 0, 0},
	{300000000, 2, 100, 2, 0, 0, 0},
	{340000000, 3, 170, 2, 0, 0, 0},
	{350000000, 2, 117, 2, -21485, 0, 0},
	{440000000, 2, 147, 2, -21485, 0, 0},
	{500000000, 2, 167, 2, -21485, 0, 0},
	{533000000, 3, 267, 2, -32768, 0, 0},
	{600000000, 2, 100, 1, 0, 0, 0},
	{900000000, 2, 150, 1, 0, 0, 0},
};

static unsigned long exynos4415_vpll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos4415_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int vpll_con0;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	vpll_con0 = __raw_readl(EXYNOS4415_G3DPLL_CON0);
	vpll_con0 &= ~(PLL35XX_MDIV_MASK << PLL35XX_MDIV_SHIFT |\
			PLL35XX_PDIV_MASK << PLL35XX_PDIV_SHIFT |\
			PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT);

	for (i = 0; i < ARRAY_SIZE(exynos4415_vpll_div); i++) {
		if (exynos4415_vpll_div[i].rate == rate) {
			vpll_con0 |= exynos4415_vpll_div[i].pdiv << PLL35XX_PDIV_SHIFT;
			vpll_con0 |= exynos4415_vpll_div[i].mdiv << PLL35XX_MDIV_SHIFT;
			vpll_con0 |= exynos4415_vpll_div[i].sdiv << PLL35XX_SDIV_SHIFT;
			vpll_con0 |= 1 << EXYNOS4415_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos4415_vpll_div)) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}
	/* 1500 max_cycls : specification data */
	locktime = 200 * exynos4415_vpll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS4415_G3DPLL_LOCK);
	__raw_writel(vpll_con0, EXYNOS4415_G3DPLL_CON0);

	do {
		tmp = __raw_readl(EXYNOS4415_G3DPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS4415_PLL_LOCKED_SHIFT)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos4415_vpll_ops = {
	.get_rate = exynos4415_vpll_get_rate,
	.set_rate = exynos4415_vpll_set_rate,
};

static struct vpll_div_data exynos4415_epll_div[] = {
	{192000000, 2, 128, 3, 0, 0, 0, 0},
};

static unsigned long exynos4415_epll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos4415_epll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int epll_con0, epll_con1;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;
	unsigned int k;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	epll_con0 = __raw_readl(EXYNOS4415_EPLL_CON0);
	epll_con0 &= ~(PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT |\
			PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT |\
			PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT);

	epll_con1 = __raw_readl(EXYNOS4415_EPLL_CON1);
	epll_con1 &= ~(0xffff << 0);

	for (i = 0; i < ARRAY_SIZE(exynos4415_epll_div); i++) {
		if (exynos4415_epll_div[i].rate == rate) {
			k = exynos4415_epll_div[i].k & 0xFFFF;
			epll_con1 |= k << 0;
			epll_con0 |= exynos4415_epll_div[i].pdiv
							<< PLL36XX_PDIV_SHIFT;
			epll_con0 |= exynos4415_epll_div[i].mdiv
							<< PLL36XX_MDIV_SHIFT;
			epll_con0 |= exynos4415_epll_div[i].sdiv
							<< PLL36XX_SDIV_SHIFT;
			epll_con0 |= 1 << 31;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos4415_epll_div)) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}

	/* 3000 max_cycls : specification data */
	locktime = 3000 * exynos4415_epll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS4415_EPLL_LOCK);
	__raw_writel(epll_con0, EXYNOS4415_EPLL_CON0);
	__raw_writel(epll_con1, EXYNOS4415_EPLL_CON1);

	do {
		tmp = __raw_readl(EXYNOS4415_EPLL_CON0);
	} while (!(tmp & (0x1 << 29)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos4415_epll_ops = {
	.get_rate = exynos4415_epll_get_rate,
	.set_rate = exynos4415_epll_set_rate,
};

static int xtal_rate;

void __init_or_cpufreq exynos4415_setup_clocks(void)
{
	struct clk *xtal_clk;

	unsigned long apll = 0;
	unsigned long bpll = 0;
	unsigned long dpll = 0;
	unsigned long mpll = 0;
	unsigned long epll = 0;
	unsigned long vpll = 0;
	unsigned long isppll = 0;
	unsigned long vpllsrc;
	unsigned long xtal;
	unsigned long armclk;
	unsigned long sclk_dmc;
	unsigned long aclk_200;
	unsigned long aclk_100;
	unsigned long aclk_160;
	unsigned long aclk_266;
	unsigned long aclk_400_mcuisp;
	unsigned int ptr;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	apll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4415_APLL_CON0));
	bpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4415_BPLL_CON0));
	mpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4415_MPLL_CON0));
	epll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS4415_EPLL_CON0),
					__raw_readl(EXYNOS4415_EPLL_CON1));

	vpllsrc = clk_get_rate(&exynos4415_clk_vpllsrc.clk);
	vpll = s5p_get_pll35xx(vpllsrc, __raw_readl(EXYNOS4415_G3DPLL_CON0));
	isppll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4415_ISPPLL_CON0));
	dpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4415_DPLL_CON0));

	/* Insert pll rate */
	clk_fout_apll.rate = apll;
	clk_fout_bpll.rate = bpll;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.ops = &exynos4415_epll_ops;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.ops = &exynos4415_vpll_ops;
	clk_fout_vpll.rate = vpll;
	clk_fout_isp_pll.rate = isppll;
	clk_fout_dpll.rate = dpll;

	/* Print PLL rate information */
	printk(KERN_INFO "EXYNOS4415: PLL settings, A=%ld, B=%ld, M=%ld, E=%ld V=%ld ISP=%ld DISP=%ld",
			apll, bpll, mpll, epll, vpll, isppll, dpll);

	armclk = clk_get_rate(&exynos4415_clk_armclk.clk);
	sclk_dmc = clk_get_rate(&exynos4415_clk_sclk_dmc.clk);

	aclk_400_mcuisp = clk_get_rate(&exynos4415_clk_aclk_400_mcuisp.clk);
	aclk_200 = clk_get_rate(&exynos4415_clk_aclk_200.clk);
	aclk_100 = clk_get_rate(&exynos4415_clk_aclk_100.clk);
	aclk_160 = clk_get_rate(&exynos4415_clk_aclk_160.clk);
	aclk_266 = clk_get_rate(&exynos4415_clk_aclk_266.clk);

	printk(KERN_INFO "EXYNOS4415: ARMCLK=%ld, DMC=%ld, ACLK200=%ld\n"
			 "ACLK100=%ld, ACLK160=%ld, ACLK266=%ld, ACLK_400=%ld\n",
			armclk, sclk_dmc, aclk_200,
			aclk_100, aclk_160, aclk_266, aclk_400_mcuisp);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4415_clksrcs); ptr++)
		s3c_set_clksrc(&exynos4415_clksrcs[ptr], true);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4415_sysclks); ptr++)
		s3c_set_clksrc(exynos4415_sysclks[ptr], true);
};

#ifdef CONFIG_PM_SLEEP
static int exynos4415_clock_suspend(void)
{
	s3c_pm_do_save(exynos4415_clock_save, ARRAY_SIZE(exynos4415_clock_save));
	return 0;
}

static void exynos4415_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos4415_clock_save, ARRAY_SIZE(exynos4415_clock_save));
}

#else
#define exynos4415_clock_suspend NULL
#define exynos4415_clock_resume NULL
#endif

static struct syscore_ops exynos4415_clock_syscore_ops = {
	.suspend	= exynos4415_clock_suspend,
	.resume		= exynos4415_clock_resume,
};

void __init exynos4415_register_clocks(void)
{
	int ptr;

	s3c24xx_register_clocks(exynos4415_clks, ARRAY_SIZE(exynos4415_clks));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4415_sysclks); ptr++)
		s3c_register_clksrc(exynos4415_sysclks[ptr], 1);

	s3c_register_clksrc(exynos4415_clksrcs, ARRAY_SIZE(exynos4415_clksrcs));

	s3c_register_clocks(exynos4415_init_clocks_on, ARRAY_SIZE(exynos4415_init_clocks_on));

	s3c24xx_register_clocks(exynos4415_clk_cdev, ARRAY_SIZE(exynos4415_clk_cdev));

	s3c_register_clocks(exynos4415_init_clocks_off, ARRAY_SIZE(exynos4415_init_clocks_off));
	s3c_disable_clocks(exynos4415_init_clocks_off, ARRAY_SIZE(exynos4415_init_clocks_off));

	clkdev_add_table(exynos4415_clk_lookup, ARRAY_SIZE(exynos4415_clk_lookup));

	register_syscore_ops(&exynos4415_clock_syscore_ops);

	s3c_pwmclk_init();
}
