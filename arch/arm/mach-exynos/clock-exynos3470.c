/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/string.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/pm.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/sysmmu.h>
#include <mach/regs-pmu.h>
#include <mach/exynos-fimc-is.h>

#include "common.h"
#include "clock-exynos4.h"

#ifdef CONFIG_PM_SLEEP
static struct sleep_save exynos4_clock_save[] = {
	SAVE_ITEM(EXYNOS4_CLKDIV_LEFTBUS),
	SAVE_ITEM(EXYNOS4_CLKDIV_RIGHTBUS),
	SAVE_ITEM(EXYNOS4_CLKDIV_CAM),
	SAVE_ITEM(EXYNOS4_CLKDIV_TV),
	SAVE_ITEM(EXYNOS4_CLKDIV_MFC),
	SAVE_ITEM(EXYNOS4_CLKDIV_G3D),
	SAVE_ITEM(EXYNOS4_CLKDIV_LCD0),
	SAVE_ITEM(EXYNOS4_CLKDIV_MAUDIO),
	SAVE_ITEM(EXYNOS4_CLKDIV_FSYS0),
	SAVE_ITEM(EXYNOS4_CLKDIV_FSYS1),
	SAVE_ITEM(EXYNOS4_CLKDIV_FSYS2),
	SAVE_ITEM(EXYNOS4_CLKDIV_FSYS3),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL0),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL1),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL2),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL3),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL4),
	SAVE_ITEM(EXYNOS4_CLKDIV_PERIL5),
	SAVE_ITEM(EXYNOS4_CLKDIV_CPU),
	SAVE_ITEM(EXYNOS4_CLKDIV_CPU + 0x4),
	SAVE_ITEM(EXYNOS4_CLKDIV_TOP),
	SAVE_ITEM(EXYNOS4_CLKDIV2_RATIO),
	SAVE_ITEM(EXYNOS4_CLKSRC_TOP0),
	SAVE_ITEM(EXYNOS4_CLKSRC_TOP1),
	SAVE_ITEM(EXYNOS4_CLKSRC_CAM),
	SAVE_ITEM(EXYNOS4_CLKSRC_CPU),
	SAVE_ITEM(EXYNOS4_CLKSRC_TV),
	SAVE_ITEM(EXYNOS4_CLKSRC_MFC),
	SAVE_ITEM(EXYNOS4_CLKSRC_G3D),
	SAVE_ITEM(EXYNOS4_CLKSRC_LCD0),
	SAVE_ITEM(EXYNOS4_CLKSRC_MAUDIO),
	SAVE_ITEM(EXYNOS4_CLKSRC_FSYS),
	SAVE_ITEM(EXYNOS4_CLKSRC_PERIL0),
	SAVE_ITEM(EXYNOS4_CLKSRC_PERIL1),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_TOP),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_TV),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_LCD0),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_PERIL0),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_PERIL1),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_CAM),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_TV),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_G3D),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_LCD0),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_PERIL),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_IMAGE),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_CPU),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_LEFTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_RIGHTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_BLOCK),
	SAVE_ITEM(EXYNOS4_CLKGATE_SCLKCPU),
	SAVE_ITEM(EXYNOS4_CLKGATE_SCLKCAM),
	SAVE_ITEM(EXYNOS4270_CLKDIV_DMC1),
	SAVE_ITEM(EXYNOS4270_CLKDIV_ACP0),
	SAVE_ITEM(EXYNOS4270_CLKSRC_ACP),
	SAVE_ITEM(EXYNOS4270_CLKSRC_DMC),
	SAVE_ITEM(EXYNOS4270_CLKGATE_IP_DMC0),
	SAVE_ITEM(EXYNOS4270_CLKGATE_IP_ACP0),
	SAVE_ITEM(EXYNOS4270_CLKSRC_MASK_ACP),
	SAVE_ITEM(EXYNOS4270_CLKGATE_SCLK_LCD),
	SAVE_ITEM(EXYNOS4270_CLKGATE_BUS_LCD),
};
#endif

static struct clk exynos4_clk_sclk_hdmi27m = {
	.name		= "sclk_hdmi27m",
	.rate		= 27000000,
};

static struct clk exynos4_clk_sclk_hdmiphy = {
	.name		= "sclk_hdmiphy",
};

static struct clk exynos4_clk_sclk_usbphy0 = {
	.name		= "sclk_usbphy0",
	.rate		= 27000000,
};

static struct clk exynos4_clk_sclk_usbphy1 = {
	.name		= "sclk_usbphy1",
};

static struct clk exynos4_clk_audiocdclk1 = {
	.name		= "audiocdclk",
};

static struct clk exynos4_clk_audiocdclk2 = {
	.name		= "audiocdclk",
};

static struct clk exynos4_clk_spdifcdclk = {
	.name		= "spdifcdclk",
};

static struct clk dummy_apb_pclk = {
	.name		= "apb_pclk",
	.id		= -1,
};

static int exynos4_clksrc_mask_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_TOP, clk, enable);
}

static int exynos4_clksrc_mask_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_CAM, clk, enable);
}

static int exynos4_clksrc_mask_lcd0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_LCD0, clk, enable);
}

int exynos4_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_FSYS, clk, enable);
}

static int exynos4_clksrc_mask_peril0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_PERIL0, clk, enable);
}

static int exynos4_clksrc_mask_peril1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_PERIL1, clk, enable);
}

static int exynos4_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_MFC, clk, enable);
}

static int exynos4_clksrc_mask_tv_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_TV, clk, enable);
}

int exynos4_clk_ip_dmc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_DMC, clk, enable);
}

int exynos4_clk_sclk_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_SCLKCAM, clk, enable);
}

int exynos4_clk_ip_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_CAM, clk, enable);
}

static int exynos4_clk_ip_tv_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_TV, clk, enable);
}

static int exynos4_clk_ip_image_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_IMAGE, clk, enable);
}

static int exynos4_clk_ip_lcd0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_LCD0, clk, enable);
}

static int exynos4_clk_sclk_lcd0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4270_CLKGATE_SCLK_LCD, clk, enable);
}

static int exynos4_clk_gate_bus_lcd0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4270_CLKGATE_BUS_LCD, clk, enable);
}

int exynos4_clk_ip_lcd1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4210_CLKGATE_IP_LCD1, clk, enable);
}

int exynos4_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos4_clk_ip_g3d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_G3D, clk, enable);
}

int exynos4_clk_ip_peril_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_PERIL, clk, enable);
}

static int exynos4_clk_ip_perir_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_PERIR, clk, enable);
}

static int exynos4_clk_ip_maudio_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_MAUDIO, clk, enable);
}

static int exynos4_clksrc_mask_maudio_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKSRC_MASK_MAUDIO, clk, enable);
}

static int exynos4_clk_hdmiphy_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS_HDMI_PHY_CONTROL, clk, enable);
}

static int exynos4_clk_dac_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4210_DAC_PHY_CONTROL, clk, enable);
}

static int exynos4_clk_clkout_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS_PMU_DEBUG, clk, !enable);
}

/* boyko : copy from exynos3470 clock files */
static int exynos4_clk_ip_acp_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4270_CLKGATE_IP_ACP0, clk, enable);
}

static int exynos4_clk_ip_isp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_ISP0, clk, enable);
}

static int exynos4_clk_ip_isp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_ISP1, clk, enable);
}

static int exynos4_clk_ip_isp_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4_CLKGATE_IP_ISP, clk, enable);
}

static int exynos4_clk_ip_acp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4270_CLKGATE_IP_ACP0, clk, enable);
}

static int exynos4_clk_ip_dmc0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS4270_CLKGATE_IP_DMC0, clk, enable);
}

/* Core list of CMU_CPU side */

static struct clksrc_clk exynos4_clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS4_CLKSRC_CPU, .shift = 0, .size = 1 },
};

static struct clksrc_clk exynos4_clk_sclk_apll = {
	.clk	= {
		.name		= "sclk_apll",
		.parent		= &exynos4_clk_mout_apll.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 24, .size = 3 },
};

struct clksrc_clk exynos4_clk_audiocdclk0 = {
	.clk	= {
		.name		= "audiocdclk",
		.rate		= 16934400,
	},
};

struct clksrc_clk exynos4_clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
		.parent		= &clk_fout_epll,
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 4, .size = 1 },
};

struct clksrc_clk exynos4_clk_mout_mpll = {
	.clk	= {
		.name		= "mout_mpll",
	},
	.sources = &clk_src_mpll,

	/* reg_src will be added in each SoCs' clock */
};

static struct clk *exynos4_clkset_moutcore_list[] = {
	[0] = &exynos4_clk_mout_apll.clk,
	[1] = &exynos4_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos4_clkset_moutcore = {
	.sources	= exynos4_clkset_moutcore_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_moutcore_list),
};

static struct clksrc_clk exynos4_clk_moutcore = {
	.clk	= {
		.name		= "moutcore",
	},
	.sources = &exynos4_clkset_moutcore,
	.reg_src = { .reg = EXYNOS4_CLKSRC_CPU, .shift = 16, .size = 1 },
};

struct clksrc_clk exynos4_clk_coreclk = {
	.clk	= {
		.name		= "core_clk",
		.parent		= &exynos4_clk_moutcore.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 0, .size = 3 },
};

struct clksrc_clk exynos4_clk_armclk = {
	.clk	= {
		.name		= "armclk",
		.parent		= &exynos4_clk_coreclk.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 28, .size = 3 },
};

static struct clksrc_clk exynos4_clk_aclk_corem0 = {
	.clk	= {
		.name		= "aclk_corem0",
		.parent		= &exynos4_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 4, .size = 3 },
};

/* Core list of CMU_CORE side */

static struct clk *exynos4_clkset_corebus_list[] = {
	[0] = &exynos4_clk_mout_mpll.clk,
	[1] = &exynos4_clk_sclk_apll.clk,
};

struct clksrc_sources exynos4_clkset_mout_corebus = {
	.sources	= exynos4_clkset_corebus_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_corebus_list),
};

struct clksrc_clk exynos4_clk_mout_corebus = {
	.clk	= {
		.name		= "mout_corebus",
	},
	.sources = &exynos4_clkset_mout_corebus,
	.reg_src = { .reg = EXYNOS_DMC_CLKREG(0x00300), .shift = 4, .size = 1 },
};

/* Core list of CMU_TOP side */

struct clk *exynos4_clkset_aclk_top_list[] = {
	[0] = &exynos4_clk_mout_mpll.clk,
	[1] = &exynos4_clk_sclk_apll.clk,
};

struct clksrc_sources exynos4_clkset_aclk = {
	.sources	= exynos4_clkset_aclk_top_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_aclk_top_list),
};

struct clksrc_clk exynos4_clk_aclk_200 = {
	.clk	= {
		.name		= "aclk_200",
	},
	.sources = &exynos4_clkset_aclk,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 12, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 0, .size = 3 },
};

struct clksrc_clk exynos4_clk_aclk_100 = {
	.clk	= {
		.name		= "aclk_100",
	},
	.sources = &exynos4_clkset_aclk,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos4_clk_aclk_160 = {
	.clk	= {
		.name		= "aclk_160",
	},
	.sources = &exynos4_clkset_aclk,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 8, .size = 3 },
};

struct clksrc_clk exynos4_clk_aclk_133 = {
	.clk	= {
		.name		= "aclk_133",
	},
	.sources = &exynos4_clkset_aclk,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 24, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 12, .size = 3 },
};

struct clk *exynos4_clkset_vpllsrc_list[] = {
	[0] = &clk_fin_vpll,
	[1] = &exynos4_clk_sclk_hdmi27m,
};

static struct clksrc_sources exynos4_clkset_vpllsrc = {
	.sources	= exynos4_clkset_vpllsrc_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_vpllsrc_list),
};

static struct clksrc_clk exynos4_clk_vpllsrc = {
	.clk	= {
		.name		= "vpll_src",
		.enable		= exynos4_clksrc_mask_top_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_vpllsrc,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP1, .shift = 0, .size = 1 },
};

static struct clk *exynos4_clkset_sclk_vpll_list[] = {
	[0] = &clk_fin_vpll,
	[1] = &clk_fout_vpll,
};

static struct clksrc_sources exynos4_clkset_sclk_vpll = {
	.sources	= exynos4_clkset_sclk_vpll_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_vpll_list),
};

struct clksrc_clk exynos4_clk_sclk_vpll = {
	.clk	= {
		.name		= "sclk_vpll",
	},
	.sources = &exynos4_clkset_sclk_vpll,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 8, .size = 1 },
};

/* copy from exynos3470 files */
static struct clk *exynos4_clk_src_bpll_list[] = {
	[0] = &clk_fin_bpll,
	[1] = &clk_fout_bpll,
};

static struct clksrc_sources exynos4_clk_src_bpll = {
	.sources	= exynos4_clk_src_bpll_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clk_src_bpll_list),
};

static struct clksrc_clk exynos4_clk_mout_bpll = {
	.clk	= {
		.name		= "mout_bpll",
		.parent		= &clk_fout_bpll,
	},
	.sources = &exynos4_clk_src_bpll,
	.reg_div = { .reg = EXYNOS4270_CLKSRC_DMC, .shift = 10, .size = 1 },
};

static struct clksrc_clk exynos4_clk_sclk_mpll = {
	.clk	= {
		.name		= "sclk_mpll",
		.parent		= &exynos4_clk_mout_mpll.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_DMC1, .shift = 8, .size = 2 },
};

static struct clk *clk_src_mpll_user_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4_clk_mout_mpll.clk,
};

static struct clksrc_sources clk_src_mpll_user = {
	.sources	= clk_src_mpll_user_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mpll_user_list),
};

static struct clksrc_clk clk_mout_mpll_user = {
	.clk	= {
		.name		= "mout_mpll_user",
	},
	.sources	= &clk_src_mpll_user,
	.reg_src	= { .reg = EXYNOS4_CLKSRC_CPU, .shift = 24, .size = 1 },
};

struct clksrc_clk exynos4_clk_mout_mpll_user_top = {
	.clk	= {
		.name		= "mout_mpll_user_top",
	},
	.sources	= &clk_src_mpll_user,
	.reg_src	= { .reg = EXYNOS4_CLKSRC_TOP1, .shift = 12, .size = 1 },
};

static struct clksrc_clk exynos4_clk_atclk = {
	.clk	= {
		.name		= "atclk",
		.parent		= &exynos4_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos4_clk_pclk_dbg = {
	.clk	= {
		.name		= "pclk_dbg",
		.parent		= &exynos4_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_CPU, .shift = 20, .size = 3 },
};

static struct clksrc_clk exynos4_clk_sclk_dmc_pre = {
	.clk	= {
		.name		= "sclk_dmc_pre",
		.parent		= &exynos4_clk_mout_corebus.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_DMC1, .shift = 19, .size = 2 },
};

static struct clksrc_clk exynos4_clk_sclk_dmc = {
	.clk	= {
		.name		= "sclk_dmc",
		.parent		= &exynos4_clk_sclk_dmc_pre.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_DMC1, .shift = 27, .size = 3 },
};

static struct clksrc_clk exynos4_clk_aclk_cored = {
	.clk	= {
		.name		= "aclk_cored",
		.parent		= &exynos4_clk_sclk_dmc.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_DMC1, .shift = 11, .size = 3 },
};

static struct clksrc_clk exynos4_clk_aclk_corep = {
	.clk	= {
		.name		= "aclk_corep",
		.parent		= &exynos4_clk_aclk_cored.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_DMC1, .shift = 15, .size = 3 },
};

static struct clk *exynos4_clkset_sclk_mpll_user_acp_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4_clk_mout_mpll.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_mpll_user_acp = {
	.sources	= exynos4_clkset_sclk_mpll_user_acp_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_mpll_user_acp_list),
};

static struct clksrc_clk exynos4_clk_sclk_mpll_user_acp = {
	.clk	= {
		.name		= "sclk_mpll_user_acp",
	},
	.sources = &exynos4_clkset_sclk_mpll_user_acp,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 13, .size = 1 },
};

static struct clk *exynos4_clkset_sclk_bpll_user_acp_list[] = {
	[0] = &clk_fin_bpll,
	[1] = &exynos4_clk_mout_bpll.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_bpll_user_acp = {
	.sources	= exynos4_clkset_sclk_bpll_user_acp_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_bpll_user_acp_list),
};

static struct clksrc_clk exynos4_clk_sclk_bpll_user_acp = {
	.clk	= {
		.name		= "sclk_bpll_user_acp",
	},
	.sources = &exynos4_clkset_sclk_bpll_user_acp,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 11, .size = 1 },
};

static struct clk *exynos4_clkset_mout_dmcbus_acp_list[] = {
	[0] = &exynos4_clk_sclk_mpll_user_acp.clk,
	[1] = &exynos4_clk_sclk_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4_clkset_mout_dmcbus_acp = {
	.sources	= exynos4_clkset_mout_dmcbus_acp_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_dmcbus_acp_list),
};

static struct clksrc_clk exynos4_clk_mout_dmcbus_acp = {
	.clk	= {
		.name		= "mout_dmcbus",
	},
	.sources = &exynos4_clkset_mout_dmcbus_acp,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 4, .size = 1 },
};

static struct clksrc_clk exynos4_clk_sclk_dmc_acp = {
	.clk	= {
		.name		= "sclk_dmc_acp",
		.parent		= &exynos4_clk_mout_dmcbus_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos4_clk_sclk_dmcd_acp = {
	.clk	= {
		.name		= "sclk_dmcd_acp",
		.parent		= &exynos4_clk_sclk_dmc_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP0, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos4_clk_sclk_dmcp_acp = {
	.clk	= {
		.name		= "sclk_dmcp_acp",
		.parent		= &exynos4_clk_sclk_dmcd_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP0, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos4_clk_aclk_acp = {
	.clk	= {
		.name		= "aclk_acp",
		.parent		= &exynos4_clk_mout_dmcbus_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos4_clk_pclk_acp = {
	.clk	= {
		.name		= "pclk_acp",
		.parent		= &exynos4_clk_aclk_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP0, .shift = 4, .size = 3 },
};

static struct clk *exynos4_clkset_mout_g2d_acp_0_list[] = {
	[0] = &exynos4_clk_sclk_mpll_user_acp.clk,
	[1] = &exynos4_clk_sclk_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g2d_acp_0 = {
	.sources	= exynos4_clkset_mout_g2d_acp_0_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_g2d_acp_0_list),
};

static struct clksrc_clk exynos4_clk_mout_g2d_acp_0 = {
	.clk	= {
		.name		= "mout_g2d_acp_0",
		.parent		= &exynos4_clk_sclk_mpll_user_acp.clk,
	},
	.sources = &exynos4_clkset_mout_g2d_acp_0,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 20, .size = 1 },
};

static struct clk *exynos4_clkset_mout_g2d_acp_1_list[] = {
	[0] = &exynos4_clk_mout_epll.clk,
	[1] = &exynos4_clk_sclk_bpll_user_acp.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g2d_acp_1 = {
	.sources	= exynos4_clkset_mout_g2d_acp_1_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_g2d_acp_1_list),
};

static struct clksrc_clk exynos4_clk_mout_g2d_acp_1 = {
	.clk	= {
		.name		= "mout_g2d_acp_1",
		.parent		= &exynos4_clk_sclk_bpll_user_acp.clk,
	},
	.sources = &exynos4_clkset_mout_g2d_acp_1,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 24, .size = 1 },
};

static struct clk *exynos4_clkset_mout_g2d_acp_list[] = {
	[0] = &exynos4_clk_mout_g2d_acp_0.clk,
	[1] = &exynos4_clk_mout_g2d_acp_1.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g2d_acp = {
	.sources	= exynos4_clkset_mout_g2d_acp_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_g2d_acp_list),
};

static struct clksrc_clk exynos4_clk_mout_g2d_acp = {
	.clk	= {
		.name		= "mout_g2d_acp",
		.parent		= &exynos4_clk_mout_g2d_acp_0.clk,
	},
	.sources = &exynos4_clkset_mout_g2d_acp,
	.reg_src = { .reg = EXYNOS4270_CLKSRC_ACP, .shift = 28, .size = 1 },
};

static struct clksrc_clk exynos4_clk_sclk_g2d_acp = {
	.clk	= {
		.name		= "sclk_fimg2d",
		.parent		= &exynos4_clk_mout_g2d_acp.clk,
	},
	.reg_div = { .reg = EXYNOS4270_CLKDIV_ACP1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_pre_aclk_400_mcuisp = {
	.clk	= {
		.name		= "pre_aclk_400_mcuisp",
	},
	.sources = &exynos4_clkset_aclk,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP1, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 24, .size = 3 },
};

struct clk *exynos4_clkset_aclk_400_mcuisp_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4_clk_pre_aclk_400_mcuisp.clk,
};

struct clksrc_sources exynos4_clkset_aclk_400_mcuisp = {
	.sources	= exynos4_clkset_aclk_400_mcuisp_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_aclk_400_mcuisp_list),
};

struct clksrc_clk exynos4_clk_aclk_400_mcuisp = {
	.clk	= {
		.name		= "aclk_400_mcuisp",
		.parent		= &exynos4_clk_pre_aclk_400_mcuisp.clk,
	},
	.sources = &exynos4_clkset_aclk_400_mcuisp,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP1, .shift = 24, .size = 1 },
};

struct clk *exynos4_clkset_pre_aclk_266_list[] = {
	[0] = &exynos4_clk_mout_mpll.clk,
	[1] = &exynos4_clk_sclk_vpll.clk,
};

struct clksrc_sources exynos4_clkset_pre_aclk_266 = {
	.sources	= exynos4_clkset_pre_aclk_266_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_pre_aclk_266_list),
};

static struct clksrc_clk exynos4_clk_pre_aclk_266 = {
	.clk	= {
		.name		= "pre_aclk_266",
	},
	.sources = &exynos4_clkset_pre_aclk_266,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP0, .shift = 12, .size = 1 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_TOP, .shift = 0, .size = 3 },
};

struct clk *exynos4_clkset_aclk_266_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos4_clk_pre_aclk_266.clk,
};

struct clksrc_sources exynos4_clkset_aclk_266 = {
	.sources	= exynos4_clkset_aclk_266_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_aclk_266_list),
};

struct clksrc_clk exynos4_clk_aclk_266 = {
	.clk	= {
		.name		= "aclk_266",
		.parent		= &exynos4_clk_pre_aclk_266.clk,
	},
	.sources = &exynos4_clkset_aclk_266,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TOP1, .shift = 20, .size = 1 },
};

static struct clksrc_clk exynos4_clk_aclk_266_div1 = {
	.clk = {
		.name		= "aclk_266_div1",
		.parent		= &exynos4_clk_aclk_266.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_ISP0, .shift = 4, .size = 3 },
};

static struct clk *exynos4_clkset_mout_jpeg0_list[] = {
	[0] = &exynos4_clk_mout_mpll_user_top.clk,
};

static struct clksrc_sources exynos4_clkset_mout_jpeg0 = {
	.sources	= exynos4_clkset_mout_jpeg0_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_jpeg0_list),
};

static struct clksrc_clk exynos4_clk_mout_jpeg0 = {
	.clk	= {
		.name		= "mout_jpeg0",
		.parent		= &exynos4_clk_mout_mpll_user_top.clk,
	},
	.sources = &exynos4_clkset_mout_jpeg0,
	.reg_src = { .reg = EXYNOS4_CLKSRC_CAM1, .shift = 0, .size = 1 },
};

static struct clk *exynos4_clkset_mout_jpeg_list[] = {
	[0] = &exynos4_clk_mout_jpeg0.clk,
};

static struct clksrc_sources exynos4_clkset_mout_jpeg = {
	.sources	= exynos4_clkset_mout_jpeg_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_jpeg_list),
};

static struct clksrc_clk exynos4_clk_dout_tsadc = {
	.clk	= {
		.name		= "dout_tsadc",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 28, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS0, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_tsadc = {
	.clk	= {
		.name		= "sclk_tsadc",
		.parent		= &exynos4_clk_dout_tsadc.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS0, .shift = 8, .size = 8 },
};

/* For CLKOUT */
struct clk *exynos4_clkset_clk_clkout_list[] = {
	/* Others are for debugging */
	[8] = &clk_xxti,
	[9] = &clk_xusbxti,
};

struct clksrc_sources exynos4_clkset_clk_clkout = {
	.sources        = exynos4_clkset_clk_clkout_list,
	.nr_sources     = ARRAY_SIZE(exynos4_clkset_clk_clkout_list),
};

static struct clksrc_clk exynos4_clk_clkout = {
	.clk	= {
		.name		= "clkout",
		.enable         = exynos4_clk_clkout_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_clk_clkout,
	.reg_src = { .reg = EXYNOS_PMU_DEBUG, .shift = 8, .size = 5 },
};

static struct clk exynos4_init_clocks_off[] = {
	/* Add from exynos3470 */
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(2d, 15),
		.enable		= exynos4_clk_ip_acp0_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp0, 9),
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (7 << 8),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp1, 16),
		.enable		= exynos4_clk_ip_isp1_ctrl,
		.ctrlbit	= (3 << 17) | (1 << 4),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif0, 12),
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif1, 13),
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif2, 14),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "hdmiphy",
		.devname	= "exynos4-hdmi",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "fimg2d",
		.devname	= "s5p-fimg2d",
		.enable		= exynos4_clk_ip_acp_ctrl,
		.ctrlbit	= (1 << 23) | (1 << 25),
	}, {
		.name		= "secss",
		.parent		= &exynos4_clk_aclk_acp.clk,
		.enable		= exynos4_clk_ip_acp0_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "acp_sss",
		.enable		= exynos4_clk_ip_acp0_ctrl,
		.ctrlbit	= (1 << 12) | (1 << 15),
	}, {
		.name		= "c2c",
		.enable		= exynos4_clk_ip_dmc0_ctrl,
		.ctrlbit	= (1 << 26) | (1 << 30),
	}, {
		.name		= "gate_isp0",
		.devname	= FIMC_IS_DEV_NAME,
		.parent		= &exynos4_clk_aclk_266.clk,
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= ((0x3 << 30) | (0x1 << 28) |
				   (0x3 << 25) | (0x1 << 23) |
				   (0x1 << 7)  | (0x3F << 0))
	}, {
		.name		= "gate_isp1",
		.devname	= FIMC_IS_DEV_NAME,
		.parent		= &exynos4_clk_aclk_266.clk,
		.enable		= exynos4_clk_ip_isp1_ctrl,
		.ctrlbit	= ((0x3 << 15) | (0x1 << 0)),
	}, {
		.name		= "camif",
		.devname	= "exynos-fimc-lite.0",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "camif",
		.devname	= "exynos-fimc-lite.1",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "camif",
		.devname	= "exynos-fimc-lite.2",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "spi0_isp",
		.enable		= exynos4_clk_ip_isp_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "spi1_isp",
		.enable		= exynos4_clk_ip_isp_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "uart_isp",
		.enable		= exynos4_clk_ip_isp_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "tsadc_isp",
		.enable		= exynos4_clk_ip_isp_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "spi0_isp1",
		.enable		= exynos4_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "spi1_isp1",
		.enable		= exynos4_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "mpwm_isp",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 24),
	}, {
		.name		= "adc_isp",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 27),
	}, {
		.name		= "adc",
		.enable		= exynos4_clk_ip_fsys_ctrl ,
		.ctrlbit	= (1 << 20),
	},

	{
		.name		= "timers",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1<<24),
	}, {
		.name		= "csis",
		.devname	= "s5p-mipi-csis.0",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "csis",
		.devname	= "s5p-mipi-csis.1",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "jpeg",
		.id		= 0,
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.0",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.1",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.2",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "fimc",
		.devname	= "exynos4-fimc.3",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "cam1",
		.enable		= exynos4_clk_sclk_cam_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "pxl_async0",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "pxl_async1",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "hsmmc",
		.devname	= "exynos4-sdhci.0",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "hsmmc",
		.devname	= "exynos4-sdhci.1",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "hsmmc",
		.devname	= "exynos4-sdhci.2",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "hsmmc",
		.devname	= "exynos4-sdhci.3",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "dwmci",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "dac",
		.devname	= "s5p-sdo",
		.enable		= exynos4_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "mixer",
		.devname	= "s5p-mixer",
		.enable		= exynos4_clk_ip_tv_ctrl,
		.ctrlbit	= ((1 << 1) | (1 << 5)),
	}, {
		.name		= "vp",
		.devname	= "s5p-mixer",
		.enable		= exynos4_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "hdmi",
		.devname	= "exynos4-hdmi",
		.enable		= exynos4_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "hdmiphy",
		.devname	= "exynos4-hdmi",
		.enable		= exynos4_clk_hdmiphy_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "dacphy",
		.devname	= "s5p-sdo",
		.enable		= exynos4_clk_dac_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name           = "tmu",
		.enable         = exynos4_clk_ip_perir_ctrl,
		.ctrlbit        = (1 << 17),
	}, {
		.name           = "hdmicec",
		.enable         = exynos4_clk_ip_perir_ctrl,
		.ctrlbit        = (1 << 11),
	}, {
		.name		= "keypad",
		.enable		= exynos4_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "rtc",
		.enable		= exynos4_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 15),
	}, {
		.name		= "watchdog",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= "usbhost",
		.enable		= exynos4_clk_ip_fsys_ctrl ,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "otg",
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.0",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.1",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 17),
	}, {
		.name		= "spi",
		.devname	= "s3c64xx-spi.2",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.1",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 20),
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.2",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 21),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.1",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 22),
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.2",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 23),
	}, {
		.name		= "slimbus",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 25),
	}, {
		.name		= "spdif",
		.devname	= "samsung-spdif",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 26),
	}, {
		.name		= "ac97",
		.devname	= "samsung-ac97",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 27),
	}, {
		.name		= "mfc",
		.devname	= "s5p-mfc",
		.enable		= exynos4_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 0) | (3 << 3),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.0",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 6),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.1",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.2",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.4",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.5",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.6",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 12),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-i2c.7",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 13),
	}, {
		.name		= "i2c",
		.devname	= "s3c2440-hdmiphy-i2c",
		.parent		= &exynos4_clk_aclk_100.clk,
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 14),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mfc_lr, 0),
		.enable		= exynos4_clk_ip_mfc_ctrl,
		.ctrlbit	= (3 << 1),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(tv, 2),
		.enable		= exynos4_clk_ip_tv_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(jpeg, 3),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 11),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(rot, 4),
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc0, 5),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc1, 6),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 8),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc2, 7),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimc3, 8),
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.4",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "ppmug3d",
		.enable		= exynos4_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "fimc-lite0",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "fimc-lite1",
		.enable		= exynos4_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "rotator",
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "ppmu_image",
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "qe_image",
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 7),
	}, {
		.name		= "qe_cam",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15) | (1 << 19) | (1 << 21),
	}, {
		.name		= "ppmu_cam",
		.enable		= exynos4_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "mie0",
		.enable		= exynos4_clk_ip_lcd0_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "pcm0",
		.enable		= exynos4_clk_ip_maudio_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "nfcon",
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 16),
	}, {
		.name		= "audio_sss",
		.enable		= exynos4_clk_ip_maudio_ctrl,
		.ctrlbit	= (1 << 0),
	},
};

static struct clk *clkset_sclk_audio0_list[] = {
	[0] = &exynos4_clk_audiocdclk0.clk,
	[1] = NULL,
	[2] = &exynos4_clk_sclk_hdmi27m,
	[3] = &exynos4_clk_sclk_usbphy0,
	[4] = &clk_ext_xtal_mux,
	[5] = &clk_xusbxti,
	[6] = &exynos4_clk_mout_mpll.clk,
	[7] = &exynos4_clk_mout_epll.clk,
	[8] = &exynos4_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_audio0 = {
	.sources	= clkset_sclk_audio0_list,
	.nr_sources	= ARRAY_SIZE(clkset_sclk_audio0_list),
};

static struct clksrc_clk exynos4_clk_sclk_audio0 = {
	.clk	= {
		.name		= "sclk_audio",
		.enable		= exynos4_clksrc_mask_maudio_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_sclk_audio0,
	.reg_src = { .reg = EXYNOS4_CLKSRC_MAUDIO, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_MAUDIO, .shift = 0, .size = 4 },
};

static struct clk *exynos4_clkset_sclk_audio1_list[] = {
	[0] = &exynos4_clk_audiocdclk1,
	[1] = NULL,
	[2] = &exynos4_clk_sclk_hdmi27m,
	[3] = &exynos4_clk_sclk_usbphy0,
	[4] = &clk_ext_xtal_mux,
	[5] = &clk_xusbxti,
	[6] = &exynos4_clk_mout_mpll.clk,
	[7] = &exynos4_clk_mout_epll.clk,
	[8] = &exynos4_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_audio1 = {
	.sources	= exynos4_clkset_sclk_audio1_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_audio1_list),
};

static struct clksrc_clk exynos4_clk_sclk_audio1 = {
	.clk	= {
		.name		= "sclk_audio1",
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_sclk_audio1,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL4, .shift = 0, .size = 8 },
};

static struct clk *exynos4_clkset_sclk_audio2_list[] = {
	[0] = &exynos4_clk_audiocdclk2,
	[1] = NULL,
	[2] = &exynos4_clk_sclk_hdmi27m,
	[3] = &exynos4_clk_sclk_usbphy0,
	[4] = &clk_ext_xtal_mux,
	[5] = &clk_xusbxti,
	[6] = &exynos4_clk_mout_mpll.clk,
	[7] = &exynos4_clk_mout_epll.clk,
	[8] = &exynos4_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_audio2 = {
	.sources	= exynos4_clkset_sclk_audio2_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_audio2_list),
};

static struct clksrc_clk exynos4_clk_sclk_audio2 = {
	.clk	= {
		.name		= "sclk_audio2",
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos4_clkset_sclk_audio2,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL4, .shift = 16, .size = 4 },
};

static struct clk *exynos4_clkset_sclk_spdif_list[] = {
	[0] = &exynos4_clk_sclk_audio0.clk,
	[1] = &exynos4_clk_sclk_audio1.clk,
	[2] = &exynos4_clk_sclk_audio2.clk,
	[3] = &exynos4_clk_spdifcdclk,
};

static struct clksrc_sources exynos4_clkset_sclk_spdif = {
	.sources	= exynos4_clkset_sclk_spdif_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_spdif_list),
};

static struct clksrc_clk exynos4_clk_sclk_spdif = {
	.clk	= {
		.name		= "sclk_spdif",
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 8),
		.ops		= &s5p_sclk_spdif_ops,
	},
	.sources = &exynos4_clkset_sclk_spdif,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 8, .size = 2 },
};

static struct clk exynos4_init_clocks_on[] = {
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.0",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 0),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.1",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.2",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.3",
		.enable		= exynos4_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd0, 10),
		.enable		= exynos4_clk_ip_lcd0_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "apb_mdnie0",
		.enable		= exynos4_clk_gate_bus_lcd0_ctrl,
		.ctrlbit	= (1 << 2) | (1 << 16),
	}, {
		.name		= "sclk_mdnie0",
		.enable		= exynos4_clk_sclk_lcd0_ctrl,
		.ctrlbit	= (1 << 1),
	}, {
		.name		= "mdnie0",
		.enable		= exynos4_clk_ip_lcd0_ctrl,
		.ctrlbit	= (1 << 2),
	}, {
		.name		= "dsim0",
		.enable		= exynos4_clk_ip_lcd0_ctrl,
		.ctrlbit	= (1 << 3),
	}
};

static struct clk exynos4_clk_pdma0 = {
	.name		= "dma",
	.devname	= "dma-pl330.0",
	.enable		= exynos4_clk_ip_fsys_ctrl,
	.ctrlbit	= (1 << 0),
};

static struct clk exynos4_clk_pdma1 = {
	.name		= "dma",
	.devname	= "dma-pl330.1",
	.enable		= exynos4_clk_ip_fsys_ctrl,
	.ctrlbit	= (1 << 1),
};

static struct clk exynos4_clk_mdma1 = {
	.name		= "dma",
	.devname	= "dma-pl330.2",
	.enable		= exynos4_clk_ip_image_ctrl,
	.ctrlbit	= ((1 << 8) | (1 << 5) | (1 << 2)),
};

static struct clk exynos4_clk_fimd0 = {
	.name		= "fimd",
	.devname	= "exynos4-fb.0",
	.enable		= exynos4_clk_ip_lcd0_ctrl,
	.ctrlbit	= (1 << 0),
};

struct clk *exynos4_clkset_group_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &clk_xusbxti,
	[2] = &exynos4_clk_sclk_hdmi27m,
	[3] = &exynos4_clk_sclk_usbphy0,
	[4] = &exynos4_clk_sclk_usbphy1,
	[5] = &exynos4_clk_sclk_hdmiphy,
	[6] = &exynos4_clk_mout_mpll.clk,
	[7] = &exynos4_clk_mout_epll.clk,
	[8] = &exynos4_clk_sclk_vpll.clk,
};

struct clksrc_sources exynos4_clkset_group = {
	.sources	= exynos4_clkset_group_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_group_list),
};

static struct clk *exynos4_clkset_mout_mfc0_list[] = {
	[0] = &exynos4_clk_mout_mpll.clk,
	[1] = &exynos4_clk_sclk_apll.clk,
};

static struct clksrc_sources exynos4_clkset_mout_mfc0 = {
	.sources	= exynos4_clkset_mout_mfc0_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_mfc0_list),
};

static struct clksrc_clk exynos4_clk_mout_mfc0 = {
	.clk	= {
		.name		= "mout_mfc0",
	},
	.sources = &exynos4_clkset_mout_mfc0,
	.reg_src = { .reg = EXYNOS4_CLKSRC_MFC, .shift = 0, .size = 1 },
};

static struct clk *exynos4_clkset_mout_mfc1_list[] = {
	[0] = &exynos4_clk_mout_epll.clk,
	[1] = &exynos4_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4_clkset_mout_mfc1 = {
	.sources	= exynos4_clkset_mout_mfc1_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_mfc1_list),
};

static struct clksrc_clk exynos4_clk_mout_mfc1 = {
	.clk	= {
		.name		= "mout_mfc1",
	},
	.sources = &exynos4_clkset_mout_mfc1,
	.reg_src = { .reg = EXYNOS4_CLKSRC_MFC, .shift = 4, .size = 1 },
};

static struct clk *exynos4_clkset_mout_mfc_list[] = {
	[0] = &exynos4_clk_mout_mfc0.clk,
	[1] = &exynos4_clk_mout_mfc1.clk,
};

static struct clksrc_sources exynos4_clkset_mout_mfc = {
	.sources	= exynos4_clkset_mout_mfc_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_mout_mfc_list),
};

static struct clk *exynos4_clkset_mout_g3d0_list[] = {
	[0] = &exynos4_clk_mout_mpll.clk,
	[1] = &exynos4_clk_sclk_apll.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g3d0 = {
	.sources    = exynos4_clkset_mout_g3d0_list,
	.nr_sources = ARRAY_SIZE(exynos4_clkset_mout_g3d0_list),
};

static struct clksrc_clk exynos4_clk_mout_g3d0 = {
	.clk    = {
		.name       = "mout_g3d0",
	},
	.sources = &exynos4_clkset_mout_g3d0,
	.reg_src = { .reg = EXYNOS4_CLKSRC_G3D, .shift = 0, .size = 1 },
};

static struct clk *exynos4_clkset_mout_g3d1_list[] = {
	[0] = &exynos4_clk_mout_epll.clk,
	[1] = &exynos4_clk_sclk_vpll.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g3d1 = {
	.sources    = exynos4_clkset_mout_g3d1_list,
	.nr_sources = ARRAY_SIZE(exynos4_clkset_mout_g3d1_list),
};

static struct clksrc_clk exynos4_clk_mout_g3d1 = {
	.clk    = {
		.name       = "mout_g3d1",
	},
	.sources = &exynos4_clkset_mout_g3d1,
	.reg_src = { .reg = EXYNOS4_CLKSRC_G3D, .shift = 4, .size = 1 },
};

static struct clk *exynos4_clkset_mout_g3d_list[] = {
	[0] = &exynos4_clk_mout_g3d0.clk,
	[1] = &exynos4_clk_mout_g3d1.clk,
};

static struct clksrc_sources exynos4_clkset_mout_g3d = {
	.sources    = exynos4_clkset_mout_g3d_list,
	.nr_sources = ARRAY_SIZE(exynos4_clkset_mout_g3d_list),
};

static struct clk *exynos4_clkset_sclk_dac_list[] = {
	[0] = &exynos4_clk_sclk_vpll.clk,
	[1] = &exynos4_clk_sclk_hdmiphy,
};

static struct clksrc_sources exynos4_clkset_sclk_dac = {
	.sources	= exynos4_clkset_sclk_dac_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_dac_list),
};

static struct clksrc_clk exynos4_clk_sclk_dac = {
	.clk		= {
		.name		= "sclk_dac",
		.enable		= exynos4_clksrc_mask_tv_ctrl,
		.ctrlbit	= (1 << 8),
	},
	.sources = &exynos4_clkset_sclk_dac,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TV, .shift = 8, .size = 1 },
};

static struct clksrc_clk exynos4_clk_sclk_pixel = {
	.clk		= {
		.name		= "sclk_pixel",
		.parent		= &exynos4_clk_sclk_vpll.clk,
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_TV, .shift = 0, .size = 4 },
};

static struct clk *exynos4_clkset_sclk_hdmi_list[] = {
	[0] = &exynos4_clk_sclk_pixel.clk,
	[1] = &exynos4_clk_sclk_hdmiphy,
};

static struct clksrc_sources exynos4_clkset_sclk_hdmi = {
	.sources	= exynos4_clkset_sclk_hdmi_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_hdmi_list),
};

static struct clksrc_clk exynos4_clk_sclk_hdmi = {
	.clk		= {
		.name		= "sclk_hdmi",
		.enable		= exynos4_clksrc_mask_tv_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_sclk_hdmi,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TV, .shift = 0, .size = 1 },
};

static struct clk *exynos4_clkset_sclk_mixer_list[] = {
	[0] = &exynos4_clk_sclk_dac.clk,
	[1] = &exynos4_clk_sclk_hdmi.clk,
};

static struct clksrc_sources exynos4_clkset_sclk_mixer = {
	.sources	= exynos4_clkset_sclk_mixer_list,
	.nr_sources	= ARRAY_SIZE(exynos4_clkset_sclk_mixer_list),
};

static struct clksrc_clk exynos4_clk_sclk_mixer = {
	.clk	= {
		.name		= "sclk_mixer",
		.enable		= exynos4_clksrc_mask_tv_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos4_clkset_sclk_mixer,
	.reg_src = { .reg = EXYNOS4_CLKSRC_TV, .shift = 4, .size = 1 },
};

static struct clksrc_clk *exynos4_sclk_tv[] = {
	&exynos4_clk_sclk_dac,
	&exynos4_clk_sclk_pixel,
	&exynos4_clk_sclk_hdmi,
	&exynos4_clk_sclk_mixer,
};

static struct clksrc_clk exynos4_clk_dout_mmc0 = {
	.clk	= {
		.name		= "dout_mmc0",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_mmc1 = {
	.clk	= {
		.name		= "dout_mmc1",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_mmc2 = {
	.clk	= {
		.name		= "dout_mmc2",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_mmc3 = {
	.clk	= {
		.name		= "dout_mmc3",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_mmc4 = {
	.clk		= {
		.name		= "dout_mmc4",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS3, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clksrcs[] = {
	/* Add from exynos3470 */
	{
		.clk	= {
			.name		= "aclk_clk_jpeg",
		},
		.sources = &exynos4_clkset_mout_jpeg,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM1, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM1, .shift = 0, .size = 4 },
	}, {
		.clk = {
			.name		= "aclk_400_mcuisp_div0",
			.parent		= &exynos4_clk_aclk_400_mcuisp.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_ISP1, .shift = 4, .size = 3 },
	}, {
		.clk = {
			.name		= "aclk_400_mcuisp_div1",
			.parent		= &exynos4_clk_aclk_400_mcuisp.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_ISP1, .shift = 8, .size = 3 },
	}, {
		.clk = {
			.name		= "aclk_266_div0",
			.parent		= &exynos4_clk_aclk_266.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_ISP0, .shift = 0, .size = 3 },
	}, {
		.clk = {
			.name		= "aclk_266_div2",
			.parent		= &exynos4_clk_aclk_266_div1.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_ISP1, .shift = 0, .size = 3 },
	}, {
		.clk = {
			.name		= "sclk_uart_isp",
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_ISP, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_ISP, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_pwm",
			.enable		= exynos4_clksrc_mask_peril0_ctrl,
			.ctrlbit	= (1 << 24),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL0, .shift = 24, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL3, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_csis",
			.devname	= "s5p-mipi-csis.0",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 24),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 24, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 24, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_csis",
			.devname	= "s5p-mipi-csis.1",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 28),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 28, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 28, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_cam0",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 16, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 16, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_cam1",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 20),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 20, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 20, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.0",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.1",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 4, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 4, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.2",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 8, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 8, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimc",
			.devname	= "exynos4-fimc.3",
			.enable		= exynos4_clksrc_mask_cam_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_CAM, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_CAM, .shift = 12, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_fimd",
			.devname	= "exynos4-fb.0",
			.enable		= exynos4_clksrc_mask_lcd0_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &exynos4_clkset_group,
		.reg_src = { .reg = EXYNOS4_CLKSRC_LCD0, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_LCD0, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mfc",
			.devname	= "s5p-mfc",
		},
		.sources = &exynos4_clkset_mout_mfc,
		.reg_src = { .reg = EXYNOS4_CLKSRC_MFC, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_MFC, .shift = 0, .size = 4 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.0",
			.parent		= &exynos4_clk_dout_mmc0.clk,
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.1",
			.parent         = &exynos4_clk_dout_mmc1.clk,
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.2",
			.parent         = &exynos4_clk_dout_mmc2.clk,
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_mmc",
			.devname	= "s3c-sdhci.3",
			.parent         = &exynos4_clk_dout_mmc3.clk,
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 24, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc",
			.parent         = &exynos4_clk_dout_mmc4.clk,
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 16),
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS3, .shift = 8, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pcm",
			.parent		= &exynos4_clk_sclk_audio0.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_MAUDIO, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pcm",
			.parent		= &exynos4_clk_sclk_audio1.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL4, .shift = 4, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_pcm",
			.parent		= &exynos4_clk_sclk_audio2.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL4, .shift = 20, .size = 8 },
	}, {
		.clk	= {
			.name		= "sclk_i2s",
			.parent		= &exynos4_clk_sclk_audio1.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL5, .shift = 0, .size = 6 },
	}, {
		.clk	= {
			.name		= "sclk_i2s",
			.parent		= &exynos4_clk_sclk_audio2.clk,
		},
		.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL5, .shift = 8, .size = 6 },
	}, {
		.clk    = {
			.name		= "sclk_g3d",
			.enable		= exynos4_clk_ip_g3d_ctrl,
			.ctrlbit	= (1 << 0) | (1 << 2),
		},
		.sources = &exynos4_clkset_mout_g3d,
		.reg_src = { .reg = EXYNOS4_CLKSRC_G3D, .shift = 8, .size = 1 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_G3D, .shift = 0, .size = 4 },
	}
};

static struct clksrc_clk exynos4_clk_sclk_uart0 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.0",
		.enable		= exynos4_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL0, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL0, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_uart1 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.1",
		.enable		= exynos4_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL0, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL0, .shift = 4, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_uart2 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.2",
		.enable		= exynos4_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 8),
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL0, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL0, .shift = 8, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_uart3 = {
	.clk	= {
		.name		= "uclk1",
		.devname	= "exynos4210-uart.3",
		.enable		= exynos4_clksrc_mask_peril0_ctrl,
		.ctrlbit	= (1 << 12),
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL0, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL0, .shift = 12, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_mmc0 = {
	.clk	= {
		.name		= "sclk_mmc",
		.devname	= "exynos4-sdhci.0",
		.parent		= &exynos4_clk_dout_mmc0.clk,
		.enable		= exynos4_clksrc_mask_fsys_ctrl,
		.ctrlbit	= (1 << 0),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_mmc1 = {
	.clk	= {
		.name		= "sclk_mmc",
		.devname	= "exynos4-sdhci.1",
		.parent		= &exynos4_clk_dout_mmc1.clk,
		.enable		= exynos4_clksrc_mask_fsys_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS1, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_mmc2 = {
	.clk	= {
		.name		= "sclk_mmc",
		.devname	= "exynos4-sdhci.2",
		.parent		= &exynos4_clk_dout_mmc2.clk,
		.enable		= exynos4_clksrc_mask_fsys_ctrl,
		.ctrlbit	= (1 << 8),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_mmc3 = {
	.clk	= {
		.name		= "sclk_mmc",
		.devname	= "exynos4-sdhci.3",
		.parent		= &exynos4_clk_dout_mmc3.clk,
		.enable		= exynos4_clksrc_mask_fsys_ctrl,
		.ctrlbit	= (1 << 12),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS2, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_mmc4 = {
	.clk	= {
		.name		= "sclk_dwmci",
		.parent         = &exynos4_clk_dout_mmc4.clk,
		.enable		= exynos4_clksrc_mask_fsys_ctrl,
		.ctrlbit	= (1 << 16),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS3, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4_clk_dout_spi0 = {
	.clk		= {
		.name		= "dout_spi0",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_spi1 = {
	.clk		= {
		.name		= "dout_spi1",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 20, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL1, .shift = 16, .size = 4 },
};

static struct clksrc_clk exynos4_clk_dout_spi2 = {
	.clk		= {
		.name		= "dout_spi2",
	},
	.sources = &exynos4_clkset_group,
	.reg_src = { .reg = EXYNOS4_CLKSRC_PERIL1, .shift = 24, .size = 4 },
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL2, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos4_clk_sclk_spi0 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.0",
		.parent		= &exynos4_clk_dout_spi0.clk,
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 16),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL1, .shift = 8, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_spi1 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.1",
		.parent		= &exynos4_clk_dout_spi1.clk,
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 20),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL1, .shift = 24, .size = 8 },
};

static struct clksrc_clk exynos4_clk_sclk_spi2 = {
	.clk	= {
		.name		= "sclk_spi",
		.devname	= "s3c64xx-spi.2",
		.parent		= &exynos4_clk_dout_spi2.clk,
		.enable		= exynos4_clksrc_mask_peril1_ctrl,
		.ctrlbit	= (1 << 24),
	},
	.reg_div = { .reg = EXYNOS4_CLKDIV_PERIL2, .shift = 8, .size = 8 },
};

/* Clock initialization code */
static struct clksrc_clk *exynos4_sysclks[] = {
	/* Add from exynos3470 */
	&clk_mout_mpll_user,
	&exynos4_clk_mout_bpll,
	&exynos4_clk_sclk_mpll,
	&exynos4_clk_mout_mpll_user_top,
	&exynos4_clk_atclk,
	&exynos4_clk_pclk_dbg,
	&exynos4_clk_sclk_dmc_pre,
	&exynos4_clk_sclk_dmc,
	&exynos4_clk_aclk_cored,
	&exynos4_clk_aclk_corep,
	&exynos4_clk_sclk_mpll_user_acp,
	&exynos4_clk_sclk_bpll_user_acp,
	&exynos4_clk_mout_dmcbus_acp,
	&exynos4_clk_sclk_dmc_acp,
	&exynos4_clk_sclk_dmcd_acp,
	&exynos4_clk_sclk_dmcp_acp,
	&exynos4_clk_aclk_acp,
	&exynos4_clk_pclk_acp,
	&exynos4_clk_mout_g2d_acp_0,
	&exynos4_clk_mout_g2d_acp_1,
	&exynos4_clk_mout_g2d_acp,
	&exynos4_clk_sclk_g2d_acp,
	&exynos4_clk_pre_aclk_400_mcuisp,
	&exynos4_clk_aclk_400_mcuisp,
	&exynos4_clk_pre_aclk_266,
	&exynos4_clk_aclk_266,
	&exynos4_clk_aclk_266_div1,
	&exynos4_clk_mout_jpeg0,
	&exynos4_clk_dout_tsadc,
	&exynos4_clk_sclk_tsadc,
	&exynos4_clk_mout_apll,
	&exynos4_clk_sclk_apll,
	&exynos4_clk_mout_epll,
	&exynos4_clk_mout_mpll,
	&exynos4_clk_moutcore,
	&exynos4_clk_coreclk,
	&exynos4_clk_armclk,
	&exynos4_clk_aclk_corem0,
	&exynos4_clk_mout_corebus,
	&exynos4_clk_vpllsrc,
	&exynos4_clk_sclk_vpll,
	&exynos4_clk_aclk_200,
	&exynos4_clk_aclk_100,
	&exynos4_clk_aclk_160,
	&exynos4_clk_aclk_133,
	&exynos4_clk_dout_mmc0,
	&exynos4_clk_dout_mmc1,
	&exynos4_clk_dout_mmc2,
	&exynos4_clk_dout_mmc3,
	&exynos4_clk_dout_mmc4,
	&exynos4_clk_dout_spi0,
	&exynos4_clk_dout_spi1,
	&exynos4_clk_dout_spi2,
	&exynos4_clk_sclk_audio0,
	&exynos4_clk_sclk_audio1,
	&exynos4_clk_sclk_audio2,
	&exynos4_clk_sclk_spdif,
	&exynos4_clk_mout_mfc0,
	&exynos4_clk_mout_mfc1,
	&exynos4_clk_mout_g3d0,
	&exynos4_clk_mout_g3d1,
	&exynos4_clk_clkout,
};

static struct clk *exynos4_clk_cdev[] = {
	&exynos4_clk_pdma0,
	&exynos4_clk_pdma1,
	&exynos4_clk_mdma1,
	&exynos4_clk_fimd0,
};

static struct clksrc_clk *exynos4_clksrc_cdev[] = {
	&exynos4_clk_sclk_uart0,
	&exynos4_clk_sclk_uart1,
	&exynos4_clk_sclk_uart2,
	&exynos4_clk_sclk_uart3,
	&exynos4_clk_sclk_mmc0,
	&exynos4_clk_sclk_mmc1,
	&exynos4_clk_sclk_mmc2,
	&exynos4_clk_sclk_mmc3,
	&exynos4_clk_sclk_mmc4,
	&exynos4_clk_sclk_spi0,
	&exynos4_clk_sclk_spi1,
	&exynos4_clk_sclk_spi2,

};

static struct clk_lookup exynos4_clk_lookup[] = {
	CLKDEV_INIT("exynos4210-uart.0", "clk_uart_baud0", &exynos4_clk_sclk_uart0.clk),
	CLKDEV_INIT("exynos4210-uart.1", "clk_uart_baud0", &exynos4_clk_sclk_uart1.clk),
	CLKDEV_INIT("exynos4210-uart.2", "clk_uart_baud0", &exynos4_clk_sclk_uart2.clk),
	CLKDEV_INIT("exynos4210-uart.3", "clk_uart_baud0", &exynos4_clk_sclk_uart3.clk),
	CLKDEV_INIT("exynos4-sdhci.0", "mmc_busclk.2", &exynos4_clk_sclk_mmc0.clk),
	CLKDEV_INIT("exynos4-sdhci.1", "mmc_busclk.2", &exynos4_clk_sclk_mmc1.clk),
	CLKDEV_INIT("exynos4-sdhci.2", "mmc_busclk.2", &exynos4_clk_sclk_mmc2.clk),
	CLKDEV_INIT("exynos4-sdhci.3", "mmc_busclk.2", &exynos4_clk_sclk_mmc3.clk),
	CLKDEV_INIT("exynos4-fb.0", "lcd", &exynos4_clk_fimd0),
	CLKDEV_INIT("dma-pl330.0", "apb_pclk", &exynos4_clk_pdma0),
	CLKDEV_INIT("dma-pl330.1", "apb_pclk", &exynos4_clk_pdma1),
	CLKDEV_INIT("dma-pl330.2", "apb_pclk", &exynos4_clk_mdma1),
	CLKDEV_INIT("s3c64xx-spi.0", "spi_busclk0", &exynos4_clk_sclk_spi0.clk),
	CLKDEV_INIT("s3c64xx-spi.1", "spi_busclk0", &exynos4_clk_sclk_spi1.clk),
	CLKDEV_INIT("s3c64xx-spi.2", "spi_busclk0", &exynos4_clk_sclk_spi2.clk),
};

static int xtal_rate;

static unsigned long exynos4_fout_apll_get_rate(struct clk *clk)
{
	return s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS4_APLL_CON0));
}

static int exynos4_fout_apll_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos4_fout_apll_ops = {
	.get_rate = exynos4_fout_apll_get_rate,
	.set_rate = exynos4_fout_apll_set_rate,
};

void __init clock_print(struct clksrc_clk *clk, int size)
{
	struct clksrc_sources *srcs;
	u32 clksrc;
	u32 mask;

	for (; size > 0; size--, clk++) {
		srcs = clk->sources;
		mask = bit_mask(clk->reg_src.shift, clk->reg_src.size);

		if (!clk->reg_src.reg) {
			if (!clk->clk.parent)
				pr_err("%s: no parent clock specified\n",
					clk->clk.name);
			else {
				pr_info("%s: source is %s, rate is %ld\n",
					clk->clk.name, clk->clk.parent->name,
					clk_get_rate(&clk->clk));
			}
			return;
		}

		clksrc = __raw_readl(clk->reg_src.reg);
		clksrc &= mask;
		clk->clk.orig_src = clksrc;
		clksrc >>= clk->reg_src.shift;

		if (clksrc > srcs->nr_sources || !srcs->sources[clksrc]) {
			pr_err("%s: bad source %d\n",
				clk->clk.name, clksrc);
			return;
		}

		pr_info("%s: source is %s (%d), rate is %ld\n",
			clk->clk.name, clk->clk.parent->name, clksrc,
			clk_get_rate(&clk->clk));
	}
}


void __init_or_cpufreq exynos3470_setup_clocks(void)
{
	struct clk *xtal_clk;

	unsigned long apll = 0;
	unsigned long bpll = 0;
	unsigned long mpll = 0;
	unsigned long epll = 0;
	unsigned long vpll = 0;
	unsigned long vpllsrc;
	unsigned long xtal;
	unsigned long armclk = 0;
	unsigned long aclk_200;
	unsigned long aclk_100;
	unsigned long aclk_160;
	unsigned int ptr;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal");
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	apll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4_APLL_CON0));
	bpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4270_BPLL_CON0));
	mpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS4270_MPLL_CON0));
	epll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS4_EPLL_CON0),
			__raw_readl(EXYNOS4_EPLL_CON1));
	vpllsrc = clk_get_rate(&exynos4_clk_vpllsrc.clk);
	vpll = s5p_get_pll36xx(vpllsrc, __raw_readl(EXYNOS4_VPLL_CON0),
			__raw_readl(EXYNOS4_VPLL_CON1));

	clk_fout_apll.ops = &exynos4_fout_apll_ops;

	clk_fout_apll.rate = apll;
	clk_fout_bpll.rate = bpll;
	clk_fout_mpll.rate = mpll;
	clk_fout_epll.rate = epll;

	printk(KERN_INFO "EXYNOS4: PLL settings, A=%ld, B=%ld, M=%ld, E=%ld V=%ld\n",
			apll, bpll, mpll, epll, vpll);

	armclk = clk_get_rate(&exynos4_clk_armclk.clk);

	aclk_200 = clk_get_rate(&exynos4_clk_aclk_200.clk);
	aclk_100 = clk_get_rate(&exynos4_clk_aclk_100.clk);
	aclk_160 = clk_get_rate(&exynos4_clk_aclk_160.clk);

	pr_info("EXYNOS4: ARMCLK=%ld, ACLK200=%ld\n"
			 "ACLK100=%ld, ACLK160=%ld\n",
			armclk, aclk_200, aclk_100, aclk_160);

	clk_set_parent(&exynos4_clk_mout_epll.clk, &clk_fout_epll);
	clk_set_parent(&exynos4_clk_sclk_vpll.clk, &clk_fout_vpll);

	pr_info("EXYNOS4: ACLK400=%ld ACLK266=%ld\n",
			clk_get_rate(&exynos4_clk_aclk_400_mcuisp.clk),
			clk_get_rate(&exynos4_clk_aclk_266.clk));

	clk_f.rate = armclk;
	clk_p.rate = aclk_100;

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_sysclks); ptr++)
		clock_print(exynos4_sysclks[ptr], 1);
	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_sclk_tv); ptr++)
		clock_print(exynos4_sclk_tv[ptr], 1);
	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_clksrc_cdev); ptr++)
		clock_print(exynos4_clksrc_cdev[ptr], 1);

	if (clk_set_parent(&exynos4_clk_sclk_mmc0.clk,
				&exynos4_clk_mout_mpll_user_top.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos4_clk_mout_mpll_user_top.clk.name,
				exynos4_clk_sclk_mmc2.clk.name);
	if (clk_set_parent(&exynos4_clk_sclk_mmc1.clk,
				&exynos4_clk_mout_mpll_user_top.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos4_clk_mout_mpll_user_top.clk.name,
				exynos4_clk_sclk_mmc1.clk.name);
	if (clk_set_parent(&exynos4_clk_sclk_mmc2.clk,
				&exynos4_clk_mout_mpll_user_top.clk))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				exynos4_clk_mout_mpll_user_top.clk.name,
				exynos4_clk_sclk_mmc2.clk.name);

	clk_set_rate(&exynos4_clk_dout_mmc0.clk, 800*MHZ);
	clk_set_rate(&exynos4_clk_dout_mmc1.clk, 800*MHZ);
	clk_set_rate(&exynos4_clk_dout_mmc2.clk, 800*MHZ);

	clk_set_rate(&exynos4_clk_sclk_mmc0.clk, 800*MHZ);
	clk_set_rate(&exynos4_clk_sclk_mmc1.clk, 800*MHZ);
	clk_set_rate(&exynos4_clk_sclk_mmc2.clk, 800*MHZ);
}

static struct clk *exynos4_clks[] __initdata = {
	&clk_fout_epll,
	&exynos4_clk_sclk_hdmi27m,
	&exynos4_clk_sclk_hdmiphy,
	&exynos4_clk_sclk_usbphy0,
	&exynos4_clk_sclk_usbphy1,
};

#ifdef CONFIG_PM_SLEEP
static int exynos4_clock_suspend(void)
{
	s3c_pm_do_save(exynos4_clock_save,
				ARRAY_SIZE(exynos4_clock_save));
	return 0;
}

static void exynos4_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos4_clock_save,
				ARRAY_SIZE(exynos4_clock_save));
}

#else
#define exynos4_clock_suspend NULL
#define exynos4_clock_resume NULL
#endif

static struct syscore_ops exynos4_clock_syscore_ops = {
	.suspend	= exynos4_clock_suspend,
	.resume		= exynos4_clock_resume,
};

/* Add from exynos3470 */
static struct vpll_div_data exynos4_vpll_div[] = {
	{100000000, 3, 200, 4, 0, 0, 0, 0},
	{160000000, 2, 107, 3, -21485, 0, 0, 0},
	{266000000, 3, 266, 3, 0, 0, 0, 0},
	{300000000, 2, 100, 2, 0, 0, 0, 0},
	{340000000, 3, 170, 2, 0, 0, 0, 0},
	{350000000, 2, 117, 2, -21485, 0, 0, 0},
	{440000000, 2, 147, 2, -21485, 0, 0, 0},
	{500000000, 2, 167, 2, -21485, 0, 0, 0},
	{533000000, 3, 267, 2, -32768, 0, 0, 0},
	{600000000, 2, 100, 1, 0, 0, 0, 0},
	{900000000, 2, 150, 1, 0, 0, 0, 0},
};

static unsigned long exynos4_vpll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos4_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int vpll_con0, vpll_con1;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;
	unsigned int k;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	vpll_con0 = __raw_readl(EXYNOS4_VPLL_CON0);
	vpll_con0 &= ~(PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT | \
			PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT | \
			PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT);

	vpll_con1 = __raw_readl(EXYNOS4_VPLL_CON1);
	vpll_con1 &= ~(0xffff << 0);

	for (i = 0; i < ARRAY_SIZE(exynos4_vpll_div); i++) {
		if (exynos4_vpll_div[i].rate == rate) {
			k = exynos4_vpll_div[i].k & 0xFFFF;
			vpll_con1 |= k << 0;
			vpll_con0 |= exynos4_vpll_div[i].pdiv
							<< PLL36XX_PDIV_SHIFT;
			vpll_con0 |= exynos4_vpll_div[i].mdiv
							<< PLL36XX_MDIV_SHIFT;
			vpll_con0 |= exynos4_vpll_div[i].sdiv
							<< PLL36XX_SDIV_SHIFT;
			vpll_con0 |= 1 << 31;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos4_vpll_div)) {
		pr_err("%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}

	/* 3000 max_cycls : specification data */
	locktime = 3000 * exynos4_vpll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS4_VPLL_LOCK);
	__raw_writel(vpll_con0, EXYNOS4_VPLL_CON0);
	__raw_writel(vpll_con1, EXYNOS4_VPLL_CON1);

	do {
		tmp = __raw_readl(EXYNOS4_VPLL_CON0);
	} while (!(tmp & (0x1 << 29)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos4_vpll_ops = {
	.get_rate = exynos4_vpll_get_rate,
	.set_rate = exynos4_vpll_set_rate,
};

static struct pll_div_data exynos4_mpll_div[] = {
	{800000000, 3, 200, 1, 0, 0, 0},
	{400000000, 3, 200, 2, 0, 0, 0},
};

static unsigned long exynos4_mpll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos4_mpll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int mpll_con0;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	mpll_con0 = __raw_readl(EXYNOS4270_MPLL_CON0);
	mpll_con0 &= ~(PLL35XX_MDIV_MASK << PLL35XX_MDIV_SHIFT |\
			PLL35XX_PDIV_MASK << PLL35XX_PDIV_SHIFT |\
			PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT);

	for (i = 0; i < ARRAY_SIZE(exynos4_mpll_div); i++) {
		if (exynos4_mpll_div[i].rate == rate) {
			mpll_con0 |= exynos4_mpll_div[i].pdiv
							<< PLL35XX_PDIV_SHIFT;
			mpll_con0 |= exynos4_mpll_div[i].mdiv
							<< PLL35XX_MDIV_SHIFT;
			mpll_con0 |= exynos4_mpll_div[i].sdiv
							<< PLL35XX_SDIV_SHIFT;
			mpll_con0 |= 1 << 31;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos4_mpll_div)) {
		printk(KERN_ERR "%s: Invalid Clock MPLL Frequency\n", __func__);
		return -EINVAL;
	}

	/* 3000 max_cycls : specification data */
	locktime = 3000 * exynos4_mpll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS4270_MPLL_LOCK);
	__raw_writel(mpll_con0, EXYNOS4270_MPLL_CON0);

	do {
		tmp = __raw_readl(EXYNOS4270_MPLL_CON0);
	} while (!(tmp & (0x1 << 29)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos4_mpll_ops = {
	.get_rate = exynos4_mpll_get_rate,
	.set_rate = exynos4_mpll_set_rate,
};

/* This setup_pll function will set rate and set parent the pll */
static void setup_pll(const char *pll_name,
			struct clk *parent_clk, unsigned long rate)
{
	struct clk *tmp_clk;

	clk_set_rate(parent_clk, rate);
	tmp_clk = clk_get(NULL, pll_name);
	clk_set_parent(tmp_clk, parent_clk);
	clk_put(tmp_clk);
}

void __init exynos3470_register_clocks(void)
{
	int ptr, ret;

	/* Add from exynos3470 */
	/* aclk_200 for EXYNOS4270 */
	exynos4_clk_aclk_200.reg_src.shift = 24;
	exynos4_clk_aclk_200.reg_div.shift = 12;

	exynos4_clk_mout_corebus.reg_src.reg = EXYNOS4270_CLKSRC_DMC;
	exynos4_clk_mout_corebus.reg_src.shift = 4;
	exynos4_clk_mout_corebus.reg_src.size = 1;

	exynos4_clk_mout_mpll.reg_src.reg = EXYNOS4270_CLKSRC_DMC;
	exynos4_clk_mout_mpll.reg_src.shift = 12;
	exynos4_clk_mout_mpll.reg_src.size = 1;

	exynos4_clkset_aclk_top_list[0] = &exynos4_clk_mout_mpll_user_top.clk;
	exynos4_clkset_aclk_top_list[1] = NULL;
	exynos4_clkset_group_list[2] = NULL;
	exynos4_clkset_group_list[5] = NULL;
	exynos4_clkset_group_list[6] = &exynos4_clk_mout_mpll_user_top.clk;
	exynos4_clkset_vpllsrc_list[1] = NULL;

	clk_fout_vpll.ops = &exynos4_vpll_ops;
	setup_pll("mout_vpll", &clk_fout_vpll, 500000000);

	clk_fout_mpll.ops = &exynos4_mpll_ops;

	s3c24xx_register_clocks(exynos4_clks, ARRAY_SIZE(exynos4_clks));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_sysclks); ptr++)
		s3c_register_clksrc(exynos4_sysclks[ptr], 1);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_sclk_tv); ptr++)
		s3c_register_clksrc(exynos4_sclk_tv[ptr], 1);

	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_clksrc_cdev); ptr++)
		s3c_register_clksrc(exynos4_clksrc_cdev[ptr], 1);

	s3c_register_clksrc(exynos4_clksrcs, ARRAY_SIZE(exynos4_clksrcs));
	s3c_register_clocks(exynos4_init_clocks_on,
					ARRAY_SIZE(exynos4_init_clocks_on));

	s3c24xx_register_clocks(exynos4_clk_cdev, ARRAY_SIZE(exynos4_clk_cdev));
	for (ptr = 0; ptr < ARRAY_SIZE(exynos4_clk_cdev); ptr++) {
		if (!strcmp(exynos4_clk_cdev[ptr]->name, "fimd"))
			continue;

		s3c_disable_clocks(exynos4_clk_cdev[ptr], 1);
	}
	s3c_register_clocks(exynos4_init_clocks_off,
					ARRAY_SIZE(exynos4_init_clocks_off));
	s3c_disable_clocks(exynos4_init_clocks_off,
					ARRAY_SIZE(exynos4_init_clocks_off));
	clkdev_add_table(exynos4_clk_lookup, ARRAY_SIZE(exynos4_clk_lookup));

	register_syscore_ops(&exynos4_clock_syscore_ops);
	ret = s3c24xx_register_clock(&dummy_apb_pclk);
	if (ret < 0)
		pr_err("dummy_apb_pclk clock do not register\n");

	s3c_pwmclk_init();
}

/* Following null function is to need on mach init */
void __init exynos4_register_clocks(void) {}
void __init_or_cpufreq exynos4_setup_clocks(void) {}
