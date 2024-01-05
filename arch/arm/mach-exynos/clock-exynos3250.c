/* linux/arch/arm/mach-exynos/clock-exynos3250.c
 *
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS3250 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/jiffies.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/pm.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/sysmmu.h>
#include "board-espresso3250.h"

void wait_clkdiv_stable_time(void __iomem *reg,
			unsigned int mask, unsigned int val)
{
	unsigned long timeout;
	unsigned int temp;
	bool ret;

	timeout = jiffies + HZ;
	do {
		temp = __raw_readl(reg);
		ret = val ? temp == mask : !(temp & mask);
		if (ret)
			goto done;

		cpu_relax();
	} while (time_before(jiffies, timeout));

	/*
	 * If register status is not changed for at most 1 second
	 * (200 jiffies), system occurs kernel panic.
	 */
	pr_err("Register(0x%x) status is not changed, status : 0x%x\n",
			(unsigned int)(reg), temp);
	BUG();

done:
	return;
}

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

/*
   will be called exynos3250_setup_clocks, ops should be set before this routine
   apll, mpll, bpll does not need to be set here, they are set in bootloader , fout_*pll
   is the top level input clock. simply one initialization code
*/
static void __init exynos3_pll_init(void)
{
#ifdef CONFIG_EXYNOS3_VPLL_ENABLE
	struct clk *tmp_clk;
	tmp_clk = clk_get(NULL, "mout_vpllsrc");
	clk_set_parent(tmp_clk, &clk_fin_vpll);
	clk_put(tmp_clk);
	setup_pll("mout_vpll", &clk_fout_vpll, 266000000);
#endif
	setup_pll("mout_epll", &clk_fout_epll, 45158400);
}

#ifdef CONFIG_PM
static struct sleep_save exynos3250_clock_save[] = {
	SAVE_ITEM(EXYNOS3_CLKSRC_TOP0),
	SAVE_ITEM(EXYNOS3_CLKSRC_TOP1),
	SAVE_ITEM(EXYNOS3_CLKSRC_CAM),
	SAVE_ITEM(EXYNOS3_CLKSRC_MFC),
	SAVE_ITEM(EXYNOS3_CLKSRC_G3D),
	SAVE_ITEM(EXYNOS3_CLKSRC_LCD),
	SAVE_ITEM(EXYNOS3_CLKSRC_ISP),
	SAVE_ITEM(EXYNOS3_CLKSRC_FSYS),
	SAVE_ITEM(EXYNOS3_CLKSRC_PERIL0),
	SAVE_ITEM(EXYNOS3_CLKSRC_PERIL1),
	SAVE_ITEM(EXYNOS3_CLKSRC_CPU),
	SAVE_ITEM(EXYNOS3_CLKSRC_DMC),
	SAVE_ITEM(EXYNOS3_CLKSRC_ACP),
	SAVE_ITEM(EXYNOS3_CLKSRC_EPLL),
	SAVE_ITEM(EXYNOS3_CLKDIV_LEFTBUS),
	SAVE_ITEM(EXYNOS3_CLKDIV_RIGHTBUS),
	SAVE_ITEM(EXYNOS3_CLKDIV_TOP),
	SAVE_ITEM(EXYNOS3_CLKDIV_CAM),
	SAVE_ITEM(EXYNOS3_CLKDIV_MFC),
	SAVE_ITEM(EXYNOS3_CLKDIV_G3D),
	SAVE_ITEM(EXYNOS3_CLKDIV_LCD),
	SAVE_ITEM(EXYNOS3_CLKDIV_ISP),
	SAVE_ITEM(EXYNOS3_CLKDIV_FSYS0),
	SAVE_ITEM(EXYNOS3_CLKDIV_FSYS1),
	SAVE_ITEM(EXYNOS3_CLKDIV_FSYS2),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL0),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL1),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL2),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL3),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL4),
	SAVE_ITEM(EXYNOS3_CLKDIV_PERIL5),
	SAVE_ITEM(EXYNOS3_CLKDIV_CAM1),
	SAVE_ITEM(EXYNOS3_CLKDIV2_RATIO),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_LEFTBUS),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_RIGHTBUS),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_PERIR),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_CAM),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_G3D),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_LCD),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_ISP),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_PERIL),
	SAVE_ITEM(EXYNOS3_CLKGATE_BLOCK),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_CPU),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_ACP0),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_ACP1),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_DMC0),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_DMC1),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_LCD),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_PERIL0),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_PERIL1),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_TOP),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_ISP),
};

static struct sleep_save exynos3250_epll_save[] = {
	SAVE_ITEM(EXYNOS3_EPLL_LOCK),
	SAVE_ITEM(EXYNOS3_EPLL_CON0),
	SAVE_ITEM(EXYNOS3_EPLL_CON1),
};

static struct sleep_save exynos3250_vpll_save[] = {
	SAVE_ITEM(EXYNOS3_VPLL_LOCK),
	SAVE_ITEM(EXYNOS3_VPLL_CON0),
	SAVE_ITEM(EXYNOS3_VPLL_CON1),
};
#endif

static int exynos3_clk_ip_perir_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_PERIR, clk, enable);
}

static int exynos3_clksrc_mask_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_TOP, clk, enable);
}

static int exynos3_clksrc_mask_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_CAM, clk, enable);
}

static int exynos3_clksrc_mask_lcd_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_LCD, clk, enable);
}

static int exynos3_clksrc_mask_isp_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_ISP, clk, enable);
}

static int exynos3_clksrc_mask_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_FSYS, clk, enable);
}

static int exynos3_clksrc_mask_peril0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_PERIL0, clk, enable);
}

static int exynos3_clksrc_mask_peril1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKSRC_MASK_PERIL1, clk, enable);
}

static int exynos3_clksrc_mask_clkout_top_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_TOP, clk, enable);
}

static int exynos3_clksrc_mask_clkout_core_l_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_DMC, clk, enable);
}

static int exynos3_clksrc_mask_clkout_core_r_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_ACP, clk, enable);
}

static int exynos3_clksrc_mask_clkout_cpu_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_CPU, clk, enable);
}

static int exynos3_clksrc_mask_clkout_left_bus_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_LEFTBUS, clk, enable);
}

static int exynos3_clksrc_mask_clkout_right_bus_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKOUT_CMU_RIGHTBUS, clk, enable);
}

static int exynos3_clksrc_mask_clk_out_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS_PMU_DEBUG, clk, !enable);
}

static int exynos3_clk_ip_cam_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_CAM, clk, enable);
}

static int exynos3_clk_ip_mfc_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_MFC, clk, enable);
}

static int exynos3_clk_ip_g3d_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_G3D, clk, enable);
}

static int exynos3_clk_ip_lcd_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_LCD, clk, enable);
}

static int exynos3_clk_ip_fsys_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_FSYS, clk, enable);
}

static int exynos3_clk_ip_peril_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_PERIL, clk, enable);
}

static int exynos3_clk_ip_isp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_ISP0, clk, enable);
}

static int exynos3_clk_ip_isp1_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_ISP1, clk, enable);
}

static int exynos3_clk_ip_acp0_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_IP_ACP0, clk, enable);
}

static int exynos3_clk_bus_lcd_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_BUS_LCD, clk, enable);
}

static int exynos3_clk_top_mfc_sclk_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_SCLK_MFC, clk, enable);
}

static int exynos3_clk_top_g3d_sclk_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_CLKGATE_SCLK_G3D, clk, enable);
}

/*
PLL Control:
	MPLL/BPLL has no control operations
	APLL/EPLL uses bypass mode first
	VPLL uses enable mode first
*/

static int exynos3_apll_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_APLL_CON1, clk, !enable);
}

static int exynos3_epll_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_EPLL_CON2, clk, !enable);
}

static int exynos3_vpll_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS3_VPLL_CON2, clk, !enable);
}

/* Part1. PLL & Related SCLK */

/* Mux output for APLL */
static struct clksrc_clk exynos3_clk_mout_apll = {
	.clk	= {
		.name		= "mout_apll",
	},
	.sources = &clk_src_apll,
	.reg_src = { .reg = EXYNOS3_CLKSRC_CPU, .shift = 0, .size = 1 },
};

/* SCLKapll and SCLK_APLL are the same*/
static struct clksrc_clk exynos3_clk_sclk_apll = {
	.clk	= {
		.name		= "sclk_apll",
		.parent		= &exynos3_clk_mout_apll.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 24, .size = 3 },
};

/* Mux output for MPLL, it is the same as  SCLKmpll/SCLKmpll_1600*/
static struct clksrc_clk exynos3_clk_mout_mpll = {
	.clk = {
		.name		= "mout_mpll",
	},
	.sources = &clk_src_mpll,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP1, .shift = 12, .size = 1 },
};

/* special ops to get 1/2 of mout_mpll, to CORE_L  */
static struct clksrc_clk exynos3_clk_sclk_mpll_mif = {
	.clk = {
		.name		= "sclk_mpll_mif",
		.parent	= &exynos3_clk_mout_mpll.clk,
	},
};

/* to CORE_R, CPU, LEFTBUS, RIGHTBUS */
static struct clksrc_clk exynos3_clk_sclk_mpll_pre_div = {
	.clk	= {
		.name		= "sclk_mpll_pre_div",
		.parent		= &exynos3_clk_sclk_mpll_mif.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 28, .size = 2 },
};

static struct clk *exynos3_clkset_moutcore_list[] = {
	[0] = &exynos3_clk_mout_apll.clk,
	[1] = &exynos3_clk_sclk_mpll_pre_div.clk,
};

static struct clksrc_sources exynos3_clkset_moutcore = {
	.sources	= exynos3_clkset_moutcore_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clkset_moutcore_list),
};

static struct clksrc_clk exynos3_clk_moutcore = {
	.clk	= {
		.name		= "moutcore",
	},
	.sources = &exynos3_clkset_moutcore,
	.reg_src = { .reg = EXYNOS3_CLKSRC_CPU, .shift = 16, .size = 1 },
};

struct clksrc_clk exynos3_clk_coreclk = {
	.clk	= {
		.name		= "core_clk",
		.parent		= &exynos3_clk_moutcore.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 0, .size = 3 },
};

struct clksrc_clk exynos3_clk_armclk = {
	.clk	= {
		.name		= "armclk",
		.parent		= &exynos3_clk_coreclk.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 28, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_corem0 = {
	.clk	= {
		.name		= "aclk_corem0",
		.parent		= &exynos3_clk_armclk.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 4, .size = 3 },
};

/* Mux output for BPLL, it is the same as SCLKbpll_in */
static struct clksrc_clk exynos3_clk_mout_bpll = {
	.clk	= {
		.name		= "mout_bpll",
	},
	.sources = &clk_src_bpll,
	.reg_src = { .reg = EXYNOS3_CLKSRC_DMC, .shift = 10, .size = 1 },
};

/* special ops to get 1/2 of mout bpll , to CORE_R*/
static struct clksrc_clk exynos3_clk_sclk_bpll = {
	.clk = {
		.name		= "sclk_bpll",
		.parent	= &exynos3_clk_mout_bpll.clk,
	},
};

/* Mux output for EPLL, sclk_epll is the same with mout_epll */
static struct clksrc_clk exynos3_clk_mout_epll = {
	.clk	= {
		.name		= "mout_epll",
	},
	.sources = &clk_src_epll,
	.reg_src = { .reg = EXYNOS3_CLKSRC_EPLL, .shift = 4, .size = 1 }, /* CORE_L's SRC */
};

static struct clk *clk_src_sclk_epll_muxed_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos3_clk_mout_epll.clk,
};

struct clksrc_sources clk_src_sclk_epll_muxed = {
	.sources	= clk_src_sclk_epll_muxed_list,
	.nr_sources	= ARRAY_SIZE(clk_src_sclk_epll_muxed_list),
};

static struct clksrc_clk exynos3_clk_sclk_epll_muxed = {
	.clk	= {
		.name		= "sclk_epll_muxed",
	},
	.sources = &clk_src_sclk_epll_muxed,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 4, .size = 1 },
};

/* VPLL , it is special  according to sfr and um, design doc not present */
static struct clk *clk_src_vpllsrc_list[] = {
	[0] = &clk_fin_vpll,
	[1] = NULL, /* tie to 1 */
};

static struct clksrc_sources clk_src_vpllsrc = {
	.sources	= clk_src_vpllsrc_list,
	.nr_sources	= ARRAY_SIZE(clk_src_vpllsrc_list),
};

static struct clksrc_clk exynos3_clk_mout_vpllsrc = {
	.clk	= {
		.name		= "mout_vpllsrc",
		.ctrlbit		= (1<<0),
		.enable		= exynos3_clksrc_mask_top_ctrl,
	},
	.sources = &clk_src_vpllsrc,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP1, .shift = 0, .size = 1 },
};

/* SCLKvpll is the same with mout_vpll */
static struct clksrc_clk exynos3_clk_mout_vpll = {
	.clk	= {
		.name		= "mout_vpll",
	},
	.sources = &clk_src_vpll,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 8, .size = 1 },
};

/* Part 2 CPU part */
static struct clk *clk_src_mpll_ctrl_user_cpu_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos3_clk_sclk_mpll_pre_div.clk,
};

struct clksrc_sources clk_src_mpll_ctrl_user_cpu = {
	.sources	= clk_src_mpll_ctrl_user_cpu_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mpll_ctrl_user_cpu_list),
};

static struct clksrc_clk exynos3_clk_mout_mpll_ctrl_user_cpu = {
	.clk	= {
		.name		= "mout_mpll_ctrl_user_cpu",
	},
	.sources = &clk_src_mpll_ctrl_user_cpu,
	.reg_src = { .reg = EXYNOS3_CLKSRC_CPU, .shift = 24, .size = 1 },
};

static struct clk *clk_src_dout_core_list[] = {
	[0] = &exynos3_clk_mout_apll.clk,
	[1] = &exynos3_clk_mout_mpll_ctrl_user_cpu.clk,
};

struct clksrc_sources clk_src_dout_core = {
	.sources	= clk_src_dout_core_list,
	.nr_sources	= ARRAY_SIZE(clk_src_dout_core_list),
};

static struct clksrc_clk exynos3_clk_dout_core = {
	.clk	= {
		.name		= "dout_core",
	},
	.sources = &clk_src_dout_core,
	.reg_src = { .reg = EXYNOS3_CLKSRC_CPU, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 0,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_dout_core2 = {
	.clk	= {
		.name		= "dout_core2",
		.parent		= &exynos3_clk_dout_core.clk
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 28,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_arm_clk = {
	.clk	= {
		.name		= "armclk",
		.parent		= &exynos3_clk_dout_core2.clk
	},
};

static struct clksrc_clk exynos3_clk_aclk_corem = {
	.clk	= {
		.name		= "aclk_corem",
		.parent		= &exynos3_clk_dout_core2.clk
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 4,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_dout_atclk = {
	.clk	= {
		.name		= "dout_atclk",
		.parent		= &exynos3_clk_dout_core2.clk
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 16,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_dout_pclk_dbg = {
	.clk	= {
		.name		= "dout_pclk_dbg",
		.parent		= &exynos3_clk_dout_core2.clk
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU0, .shift = 20,  .size = 3 },
};

static struct clk *clk_src_mout_hpm_list[] = {
	[0] = &exynos3_clk_mout_apll.clk,
	[1] = &exynos3_clk_mout_mpll_ctrl_user_cpu.clk,
};

struct clksrc_sources clk_src_mout_hmp = {
	.sources	= clk_src_mout_hpm_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_hpm_list),
};

static struct clksrc_clk exynos3_clk_dout_copy = {
	.clk	= {
		.name		= "dout_copy",
	},
	.sources = &clk_src_mout_hmp,
	.reg_src = { .reg = EXYNOS3_CLKSRC_CPU, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU1, .shift = 0,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_sclk_hpm = {
	.clk	= {
		.name		= "sclk_hpm",
		.parent		= &exynos3_clk_dout_copy.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_CPU1, .shift = 4,  .size = 3 },
};

/* CMU_CORE_L */

/* multiple use */
static struct clk *clk_src_mpll_bpllin_list[] = {
	[0] = &exynos3_clk_sclk_mpll_mif.clk,
	[1] = &exynos3_clk_mout_bpll.clk, /* before div 2 */
};

struct clksrc_sources clk_src_mpll_bpllin = {
	.sources	= clk_src_mpll_bpllin_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mpll_bpllin_list),
};

static struct clksrc_clk exynos3_clk_dout_dmc_pre = {
	.clk    = {
		.name		= "dout_dmc_pre",
	},
	.sources = &clk_src_mpll_bpllin,
	.reg_src = { .reg = EXYNOS3_CLKSRC_DMC, .shift = 4, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_DMC1, .shift = 19,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_sclk_dmc = {
	.clk    = {
		.name		= "sclk_dmc",
		.parent		= &exynos3_clk_dout_dmc_pre.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_DMC1, .shift = 27,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_dmcd = {
	.clk    = {
		.name		= "aclk_dmcd",
		.parent		= &exynos3_clk_sclk_dmc.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_DMC1, .shift = 11,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_dmcp = {
	.clk    = {
		.name		= "aclk_dmcp",
		.parent		= &exynos3_clk_aclk_dmcd.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_DMC1, .shift = 15,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_sclk_dphy = {
	.clk    = {
		.name		= "sclk_dphy",
	},
	.sources = &clk_src_mpll_bpllin,
	.reg_src = { .reg = EXYNOS3_CLKSRC_DMC, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_DMC1, .shift = 23,  .size = 3 },
};

/* CORE_R PART */
static struct clk *clk_src_mout_mpll_user_acp_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos3_clk_sclk_mpll_pre_div.clk,
};

struct clksrc_sources clk_src_mout_mpll_user_acp = {
	.sources	= clk_src_mout_mpll_user_acp_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_mpll_user_acp_list),
};

static struct clksrc_clk exynos3_clk_mout_mpll_user_acp = {
	.clk    = {
		.name		= "mout_mpll_user_acp",
	},
	.sources = &clk_src_mout_mpll_user_acp,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ACP, .shift = 13, .size = 1 },
};

static struct clk *clk_src_mout_bpll_user_acp_list[] = {
	[0] = &clk_fin_mpll,
	[1] = &exynos3_clk_sclk_bpll.clk,
};

struct clksrc_sources clk_src_mout_bpll_user_acp = {
	.sources	= clk_src_mout_bpll_user_acp_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_bpll_user_acp_list),
};

static struct clksrc_clk exynos3_clk_mout_bpll_user_acp = {
	.clk    = {
		.name		= "mout_bpll_user_acp",
	},
	.sources = &clk_src_mout_bpll_user_acp,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ACP, .shift = 11, .size = 1 },
};

static struct clk *clk_src_mout_core_r_bus_list[] = {
	[0] = &exynos3_clk_mout_mpll_user_acp.clk,
	[1] = &exynos3_clk_mout_bpll_user_acp.clk,
};

struct clksrc_sources clk_src_mout_core_core_r_bus = {
	.sources	= clk_src_mout_core_r_bus_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_core_r_bus_list),
};

static struct clksrc_clk exynos3_clk_mout_core_r_bus = {
	.clk    = {
		.name		= "mout_core_r_bus",
	},
	.sources = &clk_src_mout_core_core_r_bus,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ACP, .shift = 4, .size = 1 },
};

static struct clksrc_clk exynos3_clk_dout_acp_dmc = {
	.clk    = {
		.name		= "dout_acp_dmc",
		.parent		= &exynos3_clk_mout_core_r_bus.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP0, .shift = 8, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_cored = { /* aclk_acp_dmcd in um graph */
	.clk    = {
		.name		= "aclk_cored",
		.parent		= &exynos3_clk_dout_acp_dmc.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP0, .shift = 12, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_corep = { /* aclk_acp_dmcp in um graph */
	.clk    = {
		.name		= "aclk_corep",
		.parent		= &exynos3_clk_aclk_cored.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP0, .shift = 16, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_acp = {
	.clk    = {
		.name		= "aclk_acp",
		.parent		= &exynos3_clk_mout_core_r_bus.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos3_clk_pclk_acp = {
	.clk    = {
		.name		= "pclk_acp",
		.parent		= &exynos3_clk_aclk_acp.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP0, .shift = 4, .size = 3 },
};

static struct clk *exynos3_clk_sclk_pwi_list[] = {
	[0] = &clk_xxti,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = NULL,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[7] = &exynos3_clk_mout_epll.clk, /* From CORE_L */
	[8] = &exynos3_clk_sclk_bpll.clk,
};

struct clksrc_sources clk_src_sclk_pwi = {
	.sources	= exynos3_clk_sclk_pwi_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clk_sclk_pwi_list),
};

static struct clksrc_clk exynos3_clk_sclk_pwi = {
	.clk    = {
		.name		= "sclk_pwi",
	},
	.sources = &clk_src_sclk_pwi,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ACP, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_ACP1, .shift = 5,  .size = 6 },
};

/* LEFT BUS Part */

static struct clk *clk_src_mout_left_bus_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos3_clk_sclk_mpll_pre_div.clk,
};

struct clksrc_sources clk_src_left_bus_user = {
	.sources	= clk_src_mout_left_bus_user_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_left_bus_user_list),
};

static struct clksrc_clk exynos3_clk_mout_left_bus_user = {
	.clk    = {
		.name		= "mout_left_bus_user",
	},
	.sources = &clk_src_left_bus_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_LEFTBUS, .shift = 4, .size = 1 },
};

static struct clk *clk_src_mout_aclk_gdl_list[] = {
	[0] = &exynos3_clk_mout_left_bus_user.clk,
	[1] = NULL, /* tie to 0 */
};

struct clksrc_sources clk_src_aclk_gdl = {
	.sources	= clk_src_mout_aclk_gdl_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_aclk_gdl_list),
};

static struct clksrc_clk exynos3_clk_aclk_gdl = {
	.clk    = {
		.name		= "aclk_gdl",
	},
	.sources = &clk_src_aclk_gdl,
	.reg_src = { .reg = EXYNOS3_CLKSRC_LEFTBUS, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_LEFTBUS, .shift = 0,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_aclk_gpl = {
	.clk    = {
		.name		= "aclk_gpl",
		.parent		= &exynos3_clk_aclk_gdl.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_LEFTBUS, .shift = 4, .size = 3 },
};

/* Right BUS Part */

static struct clk *clk_src_mout_right_bus_user_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos3_clk_sclk_mpll_pre_div.clk,
};

struct clksrc_sources clk_src_right_bus_user = {
	.sources	= clk_src_mout_right_bus_user_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_right_bus_user_list),
};

static struct clksrc_clk exynos3_clk_mout_right_bus_user = {
	.clk    = {
		.name		= "mout_right_bus_user",
	},
	.sources = &clk_src_right_bus_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_RIGHTBUS, .shift = 4, .size = 1 },
};

static struct clk *clk_src_mout_aclk_gdr_list[] = {
	[0] = &exynos3_clk_mout_right_bus_user.clk,
	[1] = NULL, /* tie to 0 */
};

struct clksrc_sources clk_src_aclk_gdr = {
	.sources	= clk_src_mout_aclk_gdr_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_aclk_gdr_list),
};

static struct clksrc_clk exynos3_clk_aclk_gdr = {
	.clk    = {
		.name		= "aclk_gdr",
	},
	.sources = &clk_src_aclk_gdr,
	.reg_src = { .reg = EXYNOS3_CLKSRC_RIGHTBUS, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_RIGHTBUS, .shift = 0,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_aclk_gpr = {
	.clk    = {
		.name		= "aclk_gpr",
		.parent		= &exynos3_clk_aclk_gdr.clk,
	},
	.reg_div = { .reg = EXYNOS3_CLKDIV_RIGHTBUS, .shift = 4, .size = 3 },
};

/* CMU_TOP according to design graph */

static struct clk *clk_src_mout_top_user_list[] = {
	[0] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[1] = NULL,
};

struct clksrc_sources clk_src_mout_top_user = {
	.sources	= clk_src_mout_top_user_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_top_user_list),
};

static struct clksrc_clk exynos3_clk_aclk_200 = {
	.clk    = {
		.name		= "aclk_200",
	},
	.sources = &clk_src_mout_top_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 24, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 12,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_160 = {
	.clk    = {
		.name		= "aclk_160",
	},
	.sources = &clk_src_mout_top_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 20, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 8,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_100 = {
	.clk    = {
		.name		= "aclk_100",
	},
	.sources = &clk_src_mout_top_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 16, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 4,  .size = 4 }, /* 7:4 */
};

static struct clk *clk_src_mout_ebi_list[] = {
	[0] = &exynos3_clk_aclk_200.clk,
	[1] = &exynos3_clk_aclk_160.clk,
};

struct clksrc_sources clk_src_mout_ebi = {
	.sources	= clk_src_mout_ebi_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_ebi_list),
};

static struct clksrc_clk exynos3_clk_mout_ebi = {
	.clk    = {
		.name		= "mout_ebi",
	},
	.sources = &clk_src_mout_ebi,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 28, .size = 1 },
};

static struct clk *clk_src_sclk_ebi_list[] = {
	[0] = &exynos3_clk_mout_ebi.clk,
	[1] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_src_sclk_ebi = {
	.sources	= clk_src_sclk_ebi_list,
	.nr_sources	= ARRAY_SIZE(clk_src_sclk_ebi_list),
};

static struct clksrc_clk exynos3_clk_sclk_ebi = {
	.clk    = {
		.name		= "sclk_ebi", /* to FSYS, always on */
	},
	.sources = &clk_src_sclk_ebi,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 0, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 16,  .size = 3 },
};

static struct clksrc_clk exynos3_clk_dout_aclk_400 = {
	.clk    = {
		.name		= "dout_aclk_400",
	},
	.sources = &clk_src_mout_top_user,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP1, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 24,  .size = 3 },
};

static struct clk *clk_src_aclk_400_mcuisp_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos3_clk_dout_aclk_400.clk,
};

struct clksrc_sources clk_src_aclk_400_mcuisp = {
	.sources	= clk_src_aclk_400_mcuisp_list,
	.nr_sources	= ARRAY_SIZE(clk_src_aclk_400_mcuisp_list),
};

static struct clksrc_clk exynos3_clk_aclk_400_mcuisp = {
	.clk    = {
		.name		= "aclk_400_mcuisp",
	},
	.sources = &clk_src_aclk_400_mcuisp,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP1, .shift = 24, .size = 1 },
};

static struct clk *clk_src_mout_aclk_266_0_list[] = {
	[0] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[1] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_src_mout_aclk_266_0 = {
	.sources	= clk_src_mout_aclk_266_0_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_aclk_266_0_list),
};

static struct clksrc_clk exynos3_clk_mout_aclk_266_0 = {
	.clk    = {
		.name		= "mout_aclk_266_0",
	},
	.sources = &clk_src_mout_aclk_266_0,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 13, .size = 1 },
};

static struct clk *clk_src_mout_aclk_266_1_list[] = {
	[0] = &exynos3_clk_sclk_epll_muxed.clk,
	[1] = NULL,
};

struct clksrc_sources clk_src_mout_aclk_266_1 = {
	.sources	= clk_src_mout_aclk_266_1_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_aclk_266_1_list),
};

static struct clksrc_clk exynos3_clk_mout_aclk_266_1 = {
	.clk    = {
		.name		= "mout_aclk_266_1",
	},
	.sources = &clk_src_mout_aclk_266_1,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 14, .size = 1 },
};

static struct clk *clk_src_dout_aclk_266_list[] = {
	[0] = &exynos3_clk_mout_aclk_266_0.clk,
	[1] = &exynos3_clk_mout_aclk_266_1.clk,
};

struct clksrc_sources clk_src_dout_aclk_266 = {
	.sources	= clk_src_dout_aclk_266_list,
	.nr_sources	= ARRAY_SIZE(clk_src_dout_aclk_266_list),
};

static struct clksrc_clk exynos3_clk_dout_aclk_266 = {
	.clk    = {
		.name		= "dout_aclk_266",
	},
	.sources = &clk_src_dout_aclk_266,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP0, .shift = 12, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_TOP, .shift = 0,  .size = 3 },
};

static struct clk *clk_src_aclk_266_list[] = {
	[0] = &clk_ext_xtal_mux,
	[1] = &exynos3_clk_dout_aclk_266.clk,
};

struct clksrc_sources clk_src_aclk_266 = {
	.sources	= clk_src_aclk_266_list,
	.nr_sources	= ARRAY_SIZE(clk_src_aclk_266_list),
};

static struct clksrc_clk exynos3_clk_aclk_266 = {
	.clk    = {
		.name		= "aclk_266",
	},
	.sources = &clk_src_aclk_266,
	.reg_src = { .reg = EXYNOS3_CLKSRC_TOP1, .shift = 20, .size = 1 },
};

/* special ops to get 1/5 of mout bpll */
static struct clksrc_clk exynos3_clk_cam_blk_320 = {
	.clk    = {
		.name		= "cam_blk_320",
		.parent		= &exynos3_clk_mout_mpll.clk,
	},
};

static struct clk *exynos3_clk_func_clk_cam_blk_list[] = { /* according um 0.2 */
	[0] = &clk_xxti,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = NULL,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[7] = &exynos3_clk_mout_epll.clk,
	[8] = &exynos3_clk_mout_vpll.clk,
	[9] = NULL,
	[10] = NULL,
	[11] = NULL,
	[12] = &exynos3_clk_cam_blk_320.clk,
};

struct clksrc_sources clk_src_func_clk_cam_blk = {
	.sources	= exynos3_clk_func_clk_cam_blk_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clk_func_clk_cam_blk_list),
};

static struct clk exynos3_clk_pdma0 = {
	.name           = "dma",
	.devname        = "dma-pl330.0",
	.enable         = exynos3_clk_ip_fsys_ctrl,
	.ctrlbit        = (1 << 0),
};

static struct clk exynos3_clk_pdma1 = {
	.name           = "dma",
	.devname        = "dma-pl330.1",
	.enable         = exynos3_clk_ip_fsys_ctrl,
	.ctrlbit        = (1 << 1),
};

static struct clk exynos3_clk_fimd0 = {
	.name           = "fimd",
	.devname        = "exynos3-fb.0",
	.enable         = exynos3_clk_ip_lcd_ctrl,
	.ctrlbit        = (1 << 0),
};

static struct clk *exynos3_clkset_group_list[] = {
	[0] = &clk_xxti,
	[1] = &clk_xusbxti,
	[2] = NULL,
	[3] = NULL,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[7] = &exynos3_clk_mout_epll.clk,
	[8] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_clkset_group = {
	.sources	= exynos3_clkset_group_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clkset_group_list),
};

static struct clksrc_clk exynos3_clk_dout_sclk_tsadc = {
	.clk    = {
		.name		= "dout_sclk_tsadc",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_FSYS, .shift = 28, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS0, .shift = 0,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_spi0 = {
	.clk    = {
		.name		= "dout_sclk_spi0",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL1, .shift = 16, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL1, .shift = 0,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_spi1 = {
	.clk    = {
		.name		= "dout_sclk_spi1",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL1, .shift = 20, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL1, .shift = 16,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_spi0_isp = {
	.clk    = {
		.name 		= "dout_sclk_spi0_isp",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ISP, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_ISP, .shift = 4,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_spi1_isp = {
	.clk    = {
		.name 		= "dout_sclk_spi1_isp",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_ISP, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_ISP, .shift = 16,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_sclk_mipidphy4l = {
	.clk    = {
		.name		= "sclk_mipidphy4l",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_LCD, .shift = 12, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_LCD, .shift = 16,  .size = 4 },
};

/* MMC PART */
static struct clksrc_clk exynos3_clk_dout_sclk_mmc0 = {
	.clk    = {
		.name		= "dout_sclk_mmc0",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_FSYS, .shift = 0, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS1, .shift = 0,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_mmc1 = {
	.clk    = {
		.name		= "dout_sclk_mmc1",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_FSYS, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS1, .shift = 16,  .size = 4 },
};

static struct clksrc_clk exynos3_clk_dout_sclk_mmc2 = {
	.clk    = {
		.name		= "dout_sclk_mmc2",
	},
	.sources = &clk_clkset_group,
	.reg_src = { .reg = EXYNOS3_CLKSRC_FSYS, .shift = 8, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS2, .shift = 0,  .size = 4 },
};

/*csis0,csis1,cam1,uart0~4,pwm_isp, uart_isp, pxlasync_csis0,1_fimc in clk_src */

/* special ops to get 1/11 of sclk_mpll_1600  */
static struct clksrc_clk exynos3_clk_lcd_blk_145 = {
	.clk    = {
		.name		= "sclk_lcd_blk",
		.id		= -1,
		.parent	= &exynos3_clk_mout_mpll.clk,
	},
};

static struct clk *exynos3_clk_sclk_fimd0_list[] = { /* according um 0.2 */
	[0] = &clk_xxti,
	[1] = &clk_xusbxti,
	[2] = &exynos3_clk_sclk_mipidphy4l.clk,
	[3] = NULL,
	[4] = NULL,
	[5] = NULL,
	[6] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[7] = &exynos3_clk_mout_epll.clk,
	[8] = &exynos3_clk_mout_vpll.clk,
	[9] = NULL,
	[10] = NULL,
	[11] = NULL,
	[12] = &exynos3_clk_lcd_blk_145.clk,
};

struct clksrc_sources clk_src_sclk_fimd0 = {
	.sources	= exynos3_clk_sclk_fimd0_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clk_sclk_fimd0_list),
};

/*sclk_fimd0 in clk_src */

/* Audio Part */
static struct clk exynos3_clk_audiocdclk = { /* does not register since never select it */
	.name           = "audiocdclk",
};

static struct clk *exynos3_clk_sclk_audio_list[] = {
	[0] = &exynos3_clk_audiocdclk,
	[1] = NULL,
	[2] = NULL,
	[3] = NULL,
	[4] = &clk_xxti,
	[5] = &clk_xusbxti,
	[6] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[7] = &exynos3_clk_sclk_epll_muxed.clk,
	[8] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_src_sclk_audio = {
	.sources	= exynos3_clk_sclk_audio_list,
	.nr_sources	= ARRAY_SIZE(exynos3_clk_sclk_audio_list),
};

static struct clksrc_clk exynos3_clk_sclk_audio = {
	.clk    = {
		.name		= "sclk_audio",
		.enable		= exynos3_clksrc_mask_peril1_ctrl,
		.ctrlbit		= (1<<0),
	},
	.sources = &clk_src_sclk_audio,
	.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL1, .shift = 4, .size = 4 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL4, .shift = 0,  .size = 4 },
};

/* MFC PART */
static struct clk *clk_src_mout_mfc0_list[] = {
	[0] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[1] = NULL,
};

struct clksrc_sources clk_src_mout_mfc0 = {
	.sources	= clk_src_mout_mfc0_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_mfc0_list),
};

static struct clksrc_clk exynos3_clk_mout_mfc0 = {
	.clk	= {
		.name		= "mout_mfc0",
	},
	.sources = &clk_src_mout_mfc0,
	.reg_src = { .reg = EXYNOS3_CLKSRC_MFC, .shift = 0, .size = 1 },
};

static struct clk *clk_src_mout_mfc1_list[] = {
	[0] = &exynos3_clk_sclk_epll_muxed.clk,
	[1] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_src_mout_mfc1 = {
	.sources	= clk_src_mout_mfc1_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_mfc1_list),
};

static struct clksrc_clk exynos3_clk_mout_mfc1 = {
	.clk	= {
		.name		= "mout_mfc1",
	},
	.sources = &clk_src_mout_mfc1,
	.reg_src = { .reg = EXYNOS3_CLKSRC_MFC, .shift = 4, .size = 1 },
};

static struct clk *clk_src_mout_mfc_list[] = {
	[0] = &exynos3_clk_mout_mfc0.clk,
	[1] = &exynos3_clk_mout_mfc1.clk,
};

struct clksrc_sources clk_src_mout_mfc = {
	.sources	= clk_src_mout_mfc_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_mfc_list),
};

static struct clksrc_clk exynos3_clk_sclk_mfc = {
	.clk	= {
		.name		= "sclk_mfc",
		.devname	= "s5p-mfc",
		.enable		= exynos3_clk_top_mfc_sclk_ctrl,
		.ctrlbit		= (1 << 0),
	},
	.sources = &clk_src_mout_mfc,
	.reg_src = { .reg = EXYNOS3_CLKSRC_MFC, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_MFC, .shift = 0,  .size = 4 },
};

/*G3D Part */

static struct clk *clk_src_mout_g3d0_list[] = {
	[0] = &exynos3_clk_sclk_mpll_pre_div.clk,
	[1] = NULL,
};

struct clksrc_sources clk_src_mout_g3d0 = {
	.sources	= clk_src_mout_g3d0_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_g3d0_list),
};

static struct clksrc_clk exynos3_clk_mout_g3d0 = {
	.clk	= {
		.name		= "mout_g3d0",
	},
	.sources = &clk_src_mout_g3d0,
	.reg_src = { .reg = EXYNOS3_CLKSRC_G3D, .shift = 0, .size = 1 },
};

static struct clk *clk_src_mout_g3d1_list[] = {
	[0] = &exynos3_clk_sclk_epll_muxed.clk,
	[1] = &exynos3_clk_mout_vpll.clk,
};

struct clksrc_sources clk_src_mout_g3d1 = {
	.sources	= clk_src_mout_g3d1_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_g3d1_list),
};

static struct clksrc_clk exynos3_clk_mout_g3d1 = {
	.clk	= {
		.name		= "mout_g3d1",
	},
	.sources = &clk_src_mout_g3d1,
	.reg_src = { .reg = EXYNOS3_CLKSRC_G3D, .shift = 4, .size = 1 },
};

static struct clk *clk_src_mout_g3d_list[] = {
	[0] = &exynos3_clk_mout_g3d0.clk,
	[1] = &exynos3_clk_mout_g3d1.clk,
};

struct clksrc_sources clk_src_mout_g3d = {
	.sources	= clk_src_mout_g3d_list,
	.nr_sources	= ARRAY_SIZE(clk_src_mout_g3d_list),
};

static struct clksrc_clk exynos3_clk_sclk_g3d = {
	.clk	= {
		.name		= "sclk_g3d",
		.enable		= exynos3_clk_top_g3d_sclk_ctrl,
		.ctrlbit		= (1 << 0),
	},
	.sources = &clk_src_mout_g3d,
	.reg_src = { .reg = EXYNOS3_CLKSRC_G3D, .shift = 8, .size = 1 },
	.reg_div = { .reg = EXYNOS3_CLKDIV_G3D, .shift = 0,  .size = 4 },
};

/* ISP PART */

/* ACLK_266_MUXED is the same as ACLK_200 */

static struct clksrc_clk exynos3_clk_aclk_div0 = {
	.clk		= {
		.name		= "aclk_div0",
		.parent		= &exynos3_clk_aclk_266.clk,
	},
	.reg_div	= { .reg = EXYNOS3_CLKDIV_ISP0, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_div1 = {
	.clk		= {
		.name		= "aclk_div1",
		.parent		= &exynos3_clk_aclk_266.clk,
	},
	.reg_div	= { .reg = EXYNOS3_CLKDIV_ISP0, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_div2 = {
	.clk		= {
		.name		= "aclk_div2",
		.parent		= &exynos3_clk_aclk_div1.clk,
	},
	/* sfr uses MPWMDIV_RATIO */
	.reg_div	= { .reg = EXYNOS3_CLKDIV_ISP1, .shift = 0, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_mcuisp_div0 = {
	.clk		= {
		.name		= "aclk_mcuisp_div0",
		.parent		= &exynos3_clk_aclk_400_mcuisp.clk,
	},
	.reg_div	= { .reg = EXYNOS3_CLKDIV_ISP1, .shift = 4, .size = 3 },
};

static struct clksrc_clk exynos3_clk_aclk_mcuisp_div1 = {
	.clk		= {
		.name		= "aclk_mcuisp_div1",
		.parent		= &exynos3_clk_aclk_400_mcuisp.clk,
	},
	.reg_div	= { .reg = EXYNOS3_CLKDIV_ISP1, .shift = 8, .size = 3 },
};

/*CLK_OUT PART*/
/* top */
static struct clk *clk_src_clkout_top_list[] = {
	[0] = &clk_fout_mpll, /* mpll_fout/2 */
	[1] = &clk_fout_vpll, /* vpll_fout/2 */
	[2] = NULL, /* upll_fout/2 */
	[3] = NULL, /* m_bitclkhsdiv4_2l */
	[4] = NULL,
	[5] = NULL,
	[6] = NULL,
	[7] = &exynos3_clk_audiocdclk,
	[8] = NULL,
	[9] = NULL,
	[10] = &exynos3_clk_aclk_160.clk,
	[11] = &exynos3_clk_aclk_200.clk,
	[12] = &exynos3_clk_aclk_266.clk,
	[13] = &exynos3_clk_aclk_100.clk,
	[14] = &exynos3_clk_sclk_mfc.clk,
	[15] = &exynos3_clk_sclk_g3d.clk,
	[16] = &exynos3_clk_aclk_400_mcuisp.clk,
	[17] = NULL, /* sclk_tsadc, it is in the clksrc */
	[18] = NULL, /* cam_b_pclk */
	[19] = NULL,
	[20] = NULL, /* s_rxbyteclkhs0_2l */
	[21] = NULL,
	[22] = NULL,
	[23] = NULL, /* func_clk_cam_blk, it is in the clksrc */
	[24] = NULL,
	[25] = NULL, /* sclk_spi0_isp */
	[26] = NULL, /* sclk_spi1_isp */
	[27] = NULL, /* sclk_uart_isp */
	[28] = NULL,
	[29] = NULL,
	[30] = NULL, /* sclk_fimd0 */
	[31] = NULL, /* reserved */
};

struct clksrc_sources clk_src_clkout_top = {
	.sources	= clk_src_clkout_top_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_top_list),
};

static struct clksrc_clk exynos3_clk_clkout_top = {
	.clk    = {
		.name		= "clkout_top",
		.enable		= exynos3_clksrc_mask_clkout_top_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_top,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_TOP, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_TOP, .shift = 8,  .size = 6 },
};

/* core_l */
static struct clk *clk_src_clkout_core_l_list[] = {
	[0] = &exynos3_clk_aclk_dmcd.clk,
	[1] = &exynos3_clk_aclk_dmcp.clk,
	[2] = NULL,
	[3] = NULL,
	[4] = &exynos3_clk_sclk_dmc.clk,
	[5] = &exynos3_clk_sclk_dphy.clk,
	[6] = &clk_fout_epll, /* fout_epll/2 */
	[7] = NULL,
	[8] = NULL,
	[9] = NULL,
	[10] = NULL,
	[11] = &clk_fout_bpll, /* fout_bpll/2 */
	[12] = NULL,
};

struct clksrc_sources clk_src_clkout_core_l = {
	.sources	= clk_src_clkout_core_l_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_core_l_list),
};

static struct clksrc_clk exynos3_clk_clkout_core_l = {
	.clk    = {
		.name		= "clkout_core_l",
		.enable		= exynos3_clksrc_mask_clkout_core_l_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_core_l,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_DMC, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_DMC, .shift = 8,  .size = 6 },
};

/* core_r */
static struct clk *clk_src_clkout_core_r_list[] = {
	[0] = &exynos3_clk_aclk_cored.clk,
	[1] = &exynos3_clk_aclk_corep.clk,
	[2] = &exynos3_clk_aclk_acp.clk,
	[3] = &exynos3_clk_pclk_acp.clk,
	[4] = NULL,
	[5] = NULL,
	[6] = NULL,
	[7] = NULL, /* pwi output */
	[8] = NULL,
};

struct clksrc_sources clk_src_clkout_core_r = {
	.sources	= clk_src_clkout_core_r_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_core_r_list),
};

static struct clksrc_clk exynos3_clk_clkout_core_r = {
	.clk    = {
		.name		= "clkout_core_r",
		.enable		= exynos3_clksrc_mask_clkout_core_r_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_core_r,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_ACP, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_ACP, .shift = 8,  .size = 6 },
};

/* cpu */
static struct clk *clk_src_clkout_cpu_list[] = {
	[0] = &clk_fout_apll,  /* fout_apll/2 */
	[1] = NULL,
	[2] = NULL,
	[3] = NULL,
	[4] = &exynos3_clk_arm_clk.clk, /* armclk/2 */
	[5] = &exynos3_clk_aclk_corem.clk,
	[6] = NULL,
	[7] = NULL,
	[8] = &exynos3_clk_dout_atclk.clk,
	[9] = NULL,
	[10] = &exynos3_clk_dout_pclk_dbg.clk,
	[11] = NULL, /* sclk_hpm */
};

struct clksrc_sources clk_src_clkout_cpu = {
	.sources	= clk_src_clkout_cpu_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_cpu_list),
};

static struct clksrc_clk exynos3_clk_clkout_cpu = {
	.clk    = {
		.name		= "clkout_cpu",
		.enable		= exynos3_clksrc_mask_clkout_cpu_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_cpu,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_CPU, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_CPU, .shift = 8,  .size = 6 },
};

/* left bus */
static struct clk *clk_src_clkout_left_bus_list[] = {
	[0] = &exynos3_clk_mout_mpll.clk,
	[1] = NULL,
	[2] = &exynos3_clk_aclk_gdl.clk,
	[3] = &exynos3_clk_aclk_gpl.clk,
};

struct clksrc_sources clk_src_clkout_left_bus = {
	.sources	= clk_src_clkout_left_bus_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_left_bus_list),
};

static struct clksrc_clk exynos3_clk_clkout_left_bus = {
	.clk    = {
		.name		= "clkout_left_bus",
		.enable		= exynos3_clksrc_mask_clkout_left_bus_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_left_bus,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_LEFTBUS, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_LEFTBUS, .shift = 8,  .size = 6 },
};

/* right bus */
static struct clk *clk_src_clkout_right_bus_list[] = {
	[0] = &exynos3_clk_mout_mpll.clk,
	[1] = NULL,
	[2] = &exynos3_clk_aclk_gdr.clk,
	[3] = &exynos3_clk_aclk_gpr.clk,
};

struct clksrc_sources clk_src_clkout_right_bus = {
	.sources	= clk_src_clkout_right_bus_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_right_bus_list),
};

static struct clksrc_clk exynos3_clk_clkout_right_bus = {
	.clk    = {
		.name		= "clkout_right_bus",
		.enable		= exynos3_clksrc_mask_clkout_right_bus_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_right_bus,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_RIGHTBUS, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_RIGHTBUS, .shift = 8,  .size = 6 },
};

/* isp */
static struct clk *clk_src_clkout_isp_list[] = {
	[0] = NULL, /* aclk_mcuisp */
	[1] = NULL, /* pclkdbg_mcuisp */
	[2] = &exynos3_clk_aclk_div0.clk,
	[3] = &exynos3_clk_aclk_div1.clk,
	[4] = NULL, /* sclk_mpwm_isp */
};

struct clksrc_sources clk_src_clkout_isp = {
	.sources	= clk_src_clkout_isp_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_isp_list),
};

static struct clksrc_clk exynos3_clk_clkout_isp = {
	.clk    = {
		.name		= "clkout_isp",
		.enable		= exynos3_clksrc_mask_clkout_right_bus_ctrl,
		.ctrlbit		= (1<<16),
	},
	.sources = &clk_src_clkout_isp,
	.reg_src = { .reg = EXYNOS3_CLKOUT_CMU_ISP, .shift = 0, .size = 5 },
	.reg_div = { .reg = EXYNOS3_CLKOUT_CMU_ISP, .shift = 8,  .size = 6 },
};

/*  total, it is PMU_DEBUG register EXYNOS3_PMU_DEBUG */
static struct clk *clk_src_clkout_list[] = {
	[0] = &exynos3_clk_clkout_core_l.clk,
	[1] = &exynos3_clk_clkout_top.clk,
	[2] = &exynos3_clk_clkout_left_bus.clk,
	[3] = &exynos3_clk_clkout_right_bus.clk,
	[4] = &exynos3_clk_clkout_cpu.clk,
	[5] = &exynos3_clk_clkout_isp.clk,
	[6] = &exynos3_clk_clkout_core_r.clk,
	[7] = NULL,
	[8] = &clk_xxti,
	[9] = &clk_xusbxti,
	[10] = NULL,
	[11] = NULL,
	[12] = NULL, /* RTC tick */
	[13] = NULL, /* RTC clk */
	[14] = NULL, /* CLKOUT debug */
};

struct clksrc_sources clk_src_clk_out = {
	.sources	= clk_src_clkout_list,
	.nr_sources	= ARRAY_SIZE(clk_src_clkout_list),
};

static struct clksrc_clk exynos3_clk_clk_out = {
	.clk	= {
		.name		= "clk_out",
		.enable		= exynos3_clksrc_mask_clk_out_ctrl,
		.ctrlbit		= (1 << 0),
	},
	.sources = &clk_src_clk_out,
	.reg_src = { .reg = EXYNOS_PMU_DEBUG, .shift = 8, .size = 4 },
};

static struct clk exynos3_init_clocks[] = {
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.3",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 3),
	},
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.2",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 2),
	},
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.1",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 1),
	},
	{
		.name		= "uart",
		.devname	= "exynos4210-uart.0",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 0),
	},
	/*move top g3d gating here ,since driver will not operate it */
	{
		.name		= "g3d",
		.enable		= exynos3_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 2),
	},

	/* RIGHT BUS */
	{
		.name		= "monocnt",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 22),
	},
	{
		.name		= "tzpc6",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 21),
	},
	{
		.name		= "provisionkey",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 19) | (1 << 20),
	},
	/*tmu, keyif, rtc, wtd, to off list*/
	{
		.name		= "mct", /* OK "st" in t50 */
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 13),
	},
	{
		.name		= "seckey",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 12),
	},
	{
		.name		= "tzpc5",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 10),
	},
	{
		.name		= "tzpc4",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 9),
	},
	{
		.name		= "tzpc3",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 8),
	},
	{
		.name		= "tzpc2",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 7),
	},
	{
		.name		= "tzpc1",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 6),
	},
	{
		.name		= "tzpc0",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 5),
	},
	{
		.name		= "corepart",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 4),
	},
	{
		.name		= "toppart",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 3),
	},
	{
		.name		= "pmu_apbif",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 2),
	},
	{
		.name		= "sysreg",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 1),
	},
	{
		.name		= "axi_disp1", /* the name can be changed after it is verified */
		.devname	= "exynos3-fb.0",
		.enable		= exynos3_clk_bus_lcd_ctrl,
		.ctrlbit	= (1 << 3),
	},
};

static struct clk exynos3_i2cs_clocks[] = {
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.7",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 13),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.6",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 12),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.5",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 11),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.4",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 10),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2440-i2c.3",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 9),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2410-i2c.2",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 8),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2410-i2c.1",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 7),
	},
	{
		.name		= "i2c",
		.devname	= "s3c2410-i2c.0",
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 6),
	},

};

static int exynos3_gate_clk_set_parent(struct clk *clk, struct clk *parent)
{
	clk->parent = parent;
	return 0;
}

static struct clk_ops exynos3_gate_clk_ops = {
	.set_parent = exynos3_gate_clk_set_parent
};

static struct clk exynos3_init_clocks_off[] = {
	/*TOP: CAM */
	{
		.name		= "qe.jpeg",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 19),
	},
	{
		.name		= "pixelasyncm1",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 18),
	},
	{
		.name		= "pixelasyncm0",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 17),
	},
	{
		.name		= "camif.ppmu",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 16),
	},
	{
		.name		= "qe.mscl",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 14),
	},
	{
		.name		= "qe.gscaler1",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 13),
	},
	{
		.name		= "qe.gscaler0",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 12),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(mjpeg, 20),
		.enable		= &exynos3_clk_ip_cam_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 11),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(scaler, 18),
		.enable		= &exynos3_clk_ip_cam_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 9),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc1, 6),
		.enable		= &exynos3_clk_ip_cam_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 8),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(gsc0, 5),
		.enable		= &exynos3_clk_ip_cam_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 7),
	},
	{
		.name		= "jpeg-hx",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 6) | (1 << 19),
	},
	/*csis1,csis0 are reserved[bit5, bit4]*/
	{
		.name		= "mscl",
		.devname	= "exynos5-scaler.0",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 2) | (1 << 14),
	},
	{
		.name		= "gscl",
		.devname	= "exynos-gsc.1",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 1) | (1 << 13)
	},
	{
		.name		= "gscl",
		.devname	= "exynos-gsc.0",
		.enable		= exynos3_clk_ip_cam_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 12),
	},
	/* TOP: MFC */
	{
		.name		= "qe.mfc",
		.enable		= exynos3_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 5),
	},
	{
		.name		= "mfc.ppmu", /* include mfcr, & mfcl */
		.enable		= exynos3_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 3),
	},
	{
		.name		= SYSMMU_CLOCK_NAME, /* include mfcr & mfcl */
		.devname	= SYSMMU_CLOCK_DEVNAME(mfc_lr, 0),
		.enable		= &exynos3_clk_ip_mfc_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 1),
	},
	{
		.name		= "mfc",
		.devname	= "s5p-mfc", /* exynos_smdk3_media_init use it */
		.enable		= exynos3_clk_ip_mfc_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 5),
	},
	/*TOP: G3D*/
	{
		.name		= "qe.g3d",
		.enable		= exynos3_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 2),
	},
	{
		.name		= "g3d.ppmu",
		.enable		= exynos3_clk_ip_g3d_ctrl,
		.ctrlbit	= (1 << 1),
	},
	/*TOP: LCD */
	{
		.name		= "qe.lcd", /* include ch0 & ch1 */
		.enable		= exynos3_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 6) | (1 << 7),
	},
	{
		.name		= "lcd.ppmu",
		.enable		= exynos3_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 5),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(fimd0, 10),
		.enable		= &exynos3_clk_ip_lcd_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 4),
	},
	{
		.name		= "smie",
		.enable		= exynos3_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 2),
	},
	{
		.name		= "dsim0",
		.enable		= exynos3_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 3),
	},
	{
		.name		= "lcd",/* use lcd as con_id, it is fimd0 in sfr. */
		.devname	= "exynos3-fb.0",
		.enable		= exynos3_clk_ip_lcd_ctrl,
		.ctrlbit	= (1 << 0) | (1 << 6) | (1 << 7),
	},
	/*TOP: FSYS*/
	/*i2c is always on*/
	{
		.name		= "adc",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 20),
	},
	{
		.name		= "file.ppmu",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 17),
	},
	{
		.name		= "otg",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 13),
	},
	{
		.name		= "usbhost",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 12),
	},
	{
		.name		= "sromc",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 11),
	},
	{/* Fix_Me: for Xyref B'd SDcard */
		.name		= "dwmci",
		.devname	= "dw_mmc.2",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 7),
	},
	{
		.name		= "dwmci",
		.devname	= "dw_mmc.1",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 6),
	},
	{
		.name		= "dwmci",
		.devname	= "dw_mmc.0",
		.enable		= exynos3_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 5),
	},
	/*TOP: PERIL*/
	{
		.name		= "timers", /* pwm is named timers */
		.parent		= &exynos3_clk_aclk_100.clk,
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 24),
	},
	{
		.name		= "pcm",
		.devname	= "samsung-pcm.2",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 23),
	},
	{
		.name		= "iis",
		.devname	= "samsung-i2s.2",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 21),
	},
	{
		.name		= "spi",
		.devname	= "s3c64xx-spi.1",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 17),
	},
	{
		.name		= "spi",
		.devname	= "s3c64xx-spi.0",
		.enable		= exynos3_clk_ip_peril_ctrl,
		.ctrlbit	= (1 << 16),
	},
	{
		.name		= "acp.ppmu",
		.enable		= exynos3_clk_ip_acp0_ctrl,
		.ctrlbit	= (1 << 16),
	},
	/*Fix_Me: GIC(1 << 20),  ID_REMAPPER(1 << 13) need to init clocks ? */

	/*Fix_Me: clk_sss can be gated??*/

	/*LEFT Bus suppose it is all not gated */

	/*Right Bus IP_RIGHT_BUS suppose not gated*/
	{
		.name		= "tmu_apbif",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 17),
	},
	{
		.name		= "keypad",
		.devname	= "s3c3-keypad",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 16),
	},
	{
		.name		= "rtc",
		.devname	= "s3c64xx-rtc",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 15),
	},
	{
		.name		= "watchdog",
		.parent		= &exynos3_clk_aclk_gpr.clk,
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 14),
	},
	{
		.name		= "chipid", /* OK "chipid_apbif" in t50 */
		.enable		= exynos3_clk_ip_perir_ctrl,
		.ctrlbit	= (1 << 0),
	},
	/*ISP Part*/
	{
		.name		= "isp0_ctrl",
		.devname	= "exynos-fimc-is",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= ((0x3 << 30) | (0x1 << 28) | (0xF << 23) | (0xF << 7) | (0x7 << 0)),
	},
	{
		.name		= "isp.ppmu",  /* ppmuispx, ppmuispmx */
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= ((1 << 21) | (1 << 20)),
	},
	{
		.name		= "qe.lite1",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 18),
	},
	{
		.name		= "qe.lite0",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 17),
	},
	{
		.name		= "qe.isp", /* QE_FD, QE_DRC, QE_ISP */
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (0x7 << 14),
	},
	{
		.name		= "csis1",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 13),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif1, 13),
		.enable		= &exynos3_clk_ip_isp0_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 12),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(camif0, 12),
		.enable		= &exynos3_clk_ip_isp0_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (1 << 11),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp0, 9),
		.enable		= &exynos3_clk_ip_isp0_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (7 << 8),
	},
	{
		.name		= SYSMMU_CLOCK_NAME,
		.devname	= SYSMMU_CLOCK_DEVNAME(isp1, 16),
		.enable		= &exynos3_clk_ip_isp1_ctrl,
		.ops		= &exynos3_gate_clk_ops,
		.ctrlbit	= (3 << 17) | (1 << 4),
	},
	{
		.name		= "csis0",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 6),
	},
	{
		.name		= "mcuisp",
		.devname	= "exynos-fimc-is",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 5),
	},
	{
		.name		= "lite1",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (1 << 4) | (1 << 18),
	},
	{
		.name		= "lite0",
		.enable		= exynos3_clk_ip_isp0_ctrl,
		.ctrlbit	= (0x1 << 3) | (1 << 17),
	},

	{
		.name		= "qe.ispcx",
		.enable		= exynos3_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 21),
	},
	{
		.name		= "qe.scalerp",
		.enable		= exynos3_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 20),
	},
	{
		.name		= "qe.scalerc",
		.enable		= exynos3_clk_ip_isp1_ctrl,
		.ctrlbit	= (1 << 19),
	},
	{
		.name		= "isp1_ctrl",
		.devname	= "exynos-fimc-is",
		.enable		= exynos3_clk_ip_isp1_ctrl,
		.ctrlbit	= (0x3 << 15) | (0x3 << 12) | (1 << 0),
	},
};

/* CLK_SRC ARRAY */
static struct clksrc_clk exynos3_clksrcs_uart[] = {
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.0",
			.enable		= exynos3_clksrc_mask_peril0_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL0, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL0, .shift = 0,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.1",
			.enable		= exynos3_clksrc_mask_peril0_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL0, .shift = 4, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL0, .shift = 4,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.2",
			.enable		= exynos3_clksrc_mask_peril0_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL0, .shift = 8, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL0, .shift = 8,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "uclk1",
			.devname	= "s5pv210-uart.3",
			.enable		= exynos3_clksrc_mask_peril0_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_PERIL0, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL0, .shift = 12,  .size = 4 },
	},
};

static struct clksrc_clk exynos3_clksrcs_spi[] = {
	{
		.clk    = {
			.name		= "sclk_spi0",
			.devname	= "s3c64xx-spi.0",
			.parent		= &exynos3_clk_dout_sclk_spi0.clk,
			.enable		= exynos3_clksrc_mask_peril1_ctrl,
			.ctrlbit		= (1 << 16),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL1, .shift = 8,  .size = 8 },
	},
	{
		.clk    = {
			.name		= "sclk_spi1",
			.devname	= "s3c64xx-spi.1",
			.parent		= &exynos3_clk_dout_sclk_spi1.clk,
			.enable		= exynos3_clksrc_mask_peril1_ctrl,
			.ctrlbit		= (1 << 20),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL1, .shift = 24,  .size = 8 },
	},
};

static struct clksrc_clk exynos3_clksrcs[] = {
	{
		.clk    = {
			.name		= "sclk_cam_blk",
			.enable		= exynos3_clksrc_mask_cam_ctrl,
			.ctrlbit		= (1 << 0),
		},
		.sources = &clk_src_func_clk_cam_blk,
		.reg_src = { .reg = EXYNOS3_CLKSRC_CAM, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_CAM, .shift = 0,  .size = 4 },
	},
	{
		.clk    = {
			.name		= "sclk_tsadc",
			.parent		= &exynos3_clk_dout_sclk_tsadc.clk,
			.enable		= exynos3_clksrc_mask_fsys_ctrl,
			.ctrlbit		= (1 << 28),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS0, .shift = 8,  .size = 8 },
	},
	{
		.clk    = {
			.name		= "sclk_spi0_isp",
			.parent		= &exynos3_clk_dout_sclk_spi0_isp.clk,
			.enable		= exynos3_clksrc_mask_isp_ctrl,
			.ctrlbit		= (1 << 4),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_ISP, .shift = 8,  .size = 8 },
	},
	{
		.clk    = {
			.name		= "sclk_spi1_isp",
			.parent		= &exynos3_clk_dout_sclk_spi1_isp.clk,
			.enable		= exynos3_clksrc_mask_isp_ctrl,
			.ctrlbit		= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_ISP, .shift = 20,  .size = 8 },
	},
	{
		.clk    = {
			.name		= "sclk_mipi0",
			.parent		= &exynos3_clk_sclk_mipidphy4l.clk,
			.enable		= exynos3_clksrc_mask_lcd_ctrl,
			.ctrlbit		= (1 << 12),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_LCD, .shift = 20,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.0",
			.parent		= &exynos3_clk_dout_sclk_mmc0.clk,
			.enable		= exynos3_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 0),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS1, .shift = 8,  .size = 8 },
	},
	{
		.clk	= {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.1",
			.parent		= &exynos3_clk_dout_sclk_mmc1.clk,
			.enable		= exynos3_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 4),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS1, .shift = 24,  .size = 8 },
	},
	{/* Fix_Me: for Xyref B'd SDcard */
		.clk    = {
			.name		= "sclk_dwmci",
			.devname	= "dw_mmc.2",
			.parent		= &exynos3_clk_dout_sclk_mmc2.clk,
			.enable		= exynos3_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 8),
		},
		.reg_div = { .reg = EXYNOS3_CLKDIV_FSYS2, .shift = 8,  .size = 8 },
	},
	{
		.clk    = {
			.name		= "sclk_cam1",
			.enable		= exynos3_clksrc_mask_cam_ctrl,
			.ctrlbit		= (1 << 20),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_CAM, .shift = 20, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_CAM, .shift = 20,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "sclk_uart_isp",
			.enable		= exynos3_clksrc_mask_isp_ctrl,
			.ctrlbit	= (1 << 12),
		},
		.sources = &clk_clkset_group,
		.reg_src = { .reg = EXYNOS3_CLKSRC_ISP, .shift = 12, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_ISP, .shift = 28,  .size = 4 },
	},
	{
		.clk    = {
			.name		= "sclk_fimd",
			.devname	= "exynos3-fb.0",
			.enable		= exynos3_clksrc_mask_lcd_ctrl,
			.ctrlbit		= (1 << 0),
		},
		.sources = &clk_src_sclk_fimd0,
		.reg_src = { .reg = EXYNOS3_CLKSRC_LCD, .shift = 0, .size = 4 },
		.reg_div = { .reg = EXYNOS3_CLKDIV_LCD, .shift = 0,  .size = 4 },
	},
	{
		.clk	= {
			.name		= "sclk_pcm",
			.parent		= &exynos3_clk_sclk_audio.clk,
		},
			.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL4, .shift = 20, .size = 8 },
	},

	{
		.clk	= {
			.name		= "sclk_i2s",
			.parent		= &exynos3_clk_sclk_audio.clk,
		},
			.reg_div = { .reg = EXYNOS3_CLKDIV_PERIL5, .shift = 8, .size = 6 },
	},

};

static struct clksrc_clk *exynos3_sysclks[] = {
	&exynos3_clk_mout_apll,
	&exynos3_clk_sclk_apll,
	&exynos3_clk_mout_mpll,
	&exynos3_clk_moutcore,
	&exynos3_clk_coreclk,
	&exynos3_clk_armclk,
	&exynos3_clk_aclk_corem0,
	&exynos3_clk_sclk_mpll_mif,
	&exynos3_clk_sclk_mpll_pre_div,
	&exynos3_clk_mout_bpll,
	&exynos3_clk_sclk_bpll,
	&exynos3_clk_mout_epll,
	&exynos3_clk_mout_vpllsrc,
	&exynos3_clk_mout_vpll,
	&exynos3_clk_mout_mpll_ctrl_user_cpu,
	&exynos3_clk_dout_core,
	&exynos3_clk_dout_core2,
	&exynos3_clk_arm_clk,
	&exynos3_clk_aclk_corem,
	&exynos3_clk_dout_atclk,
	&exynos3_clk_dout_pclk_dbg,
	&exynos3_clk_dout_copy,
	&exynos3_clk_sclk_hpm,
	&exynos3_clk_dout_dmc_pre,
	&exynos3_clk_sclk_dmc,
	&exynos3_clk_aclk_dmcd,
	&exynos3_clk_aclk_dmcp,
	&exynos3_clk_sclk_dphy,
	&exynos3_clk_mout_mpll_user_acp,
	&exynos3_clk_mout_bpll_user_acp,
	&exynos3_clk_mout_core_r_bus,
	&exynos3_clk_dout_acp_dmc,
	&exynos3_clk_aclk_cored,
	&exynos3_clk_aclk_corep,
	&exynos3_clk_aclk_acp,
	&exynos3_clk_pclk_acp,
	&exynos3_clk_sclk_pwi,
	&exynos3_clk_mout_left_bus_user,
	&exynos3_clk_aclk_gdl,
	&exynos3_clk_aclk_gpl,
	&exynos3_clk_mout_right_bus_user,
	&exynos3_clk_aclk_gdr,
	&exynos3_clk_aclk_gpr,
	&exynos3_clk_sclk_epll_muxed,
	&exynos3_clk_aclk_200,
	&exynos3_clk_aclk_160,
	&exynos3_clk_aclk_100,
	&exynos3_clk_mout_ebi,
	&exynos3_clk_sclk_ebi,
	&exynos3_clk_dout_aclk_400,
	&exynos3_clk_aclk_400_mcuisp,
	&exynos3_clk_mout_aclk_266_0,
	&exynos3_clk_mout_aclk_266_1,
	&exynos3_clk_dout_aclk_266,
	&exynos3_clk_aclk_266,
	&exynos3_clk_cam_blk_320,
	&exynos3_clk_dout_sclk_tsadc,
	&exynos3_clk_dout_sclk_spi0,
	&exynos3_clk_dout_sclk_spi1,
	&exynos3_clk_dout_sclk_spi0_isp,
	&exynos3_clk_dout_sclk_spi1_isp,
	&exynos3_clk_sclk_mipidphy4l,
	&exynos3_clk_dout_sclk_mmc0,
	&exynos3_clk_dout_sclk_mmc1,
	&exynos3_clk_dout_sclk_mmc2,
	&exynos3_clk_lcd_blk_145,
	&exynos3_clk_sclk_audio,
	&exynos3_clk_mout_mfc0,
	&exynos3_clk_mout_mfc1,
	&exynos3_clk_sclk_mfc,
	&exynos3_clk_mout_g3d0,
	&exynos3_clk_mout_g3d1,
	&exynos3_clk_sclk_g3d,
	&exynos3_clk_aclk_div0,
	&exynos3_clk_aclk_div1,
	&exynos3_clk_aclk_div2,
	&exynos3_clk_aclk_mcuisp_div0,
	&exynos3_clk_aclk_mcuisp_div1,
};

static struct clk *exynos3_clk_cdev[] = {
	&exynos3_clk_pdma0,
	&exynos3_clk_pdma1,
	&exynos3_clk_fimd0,
};

static struct clksrc_clk *exynos3_clkout_clks[]  = {
	&exynos3_clk_clkout_top,
	&exynos3_clk_clkout_core_l,
	&exynos3_clk_clkout_core_r,
	&exynos3_clk_clkout_cpu,
	&exynos3_clk_clkout_left_bus,
	&exynos3_clk_clkout_right_bus,
	&exynos3_clk_clkout_isp,
	&exynos3_clk_clk_out,
};

static struct vpll_div_data exynos3_epll_div[] = {
	{ 45158400, 3, 167, 5, 0xBD1C, 0, 0, 0},
	{ 49152000, 3, 181, 5, 31740, 0, 0, 0},
	{ 67737600, 3, 250, 5, 7082, 0, 0, 0},
	{ 73728000, 3, 136, 4, 7421, 0, 0, 0},
	{192000000, 3, 177, 3,  15124, 0, 0, 0},
};

static unsigned long xtal_rate;

static unsigned long exynos3_epll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos3_epll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int epll_con0, epll_con1;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;
	unsigned int k;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	epll_con0 = __raw_readl(EXYNOS3_EPLL_CON0);
	epll_con0 &= ~(PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT |\
			PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT |\
			PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT);

	epll_con1 = __raw_readl(EXYNOS3_EPLL_CON1);
	epll_con1 &= ~(0xffff << 0);

	for (i = 0; i < ARRAY_SIZE(exynos3_epll_div); i++) {
		if (exynos3_epll_div[i].rate == rate) {
			k = (~(exynos3_epll_div[i].k) + 1) & EXYNOS3_EPLLCON1_K_MASK;
			epll_con1 |= k << 0;
			epll_con0 |= exynos3_epll_div[i].pdiv << PLL36XX_PDIV_SHIFT;
			epll_con0 |= exynos3_epll_div[i].mdiv << PLL36XX_MDIV_SHIFT;
			epll_con0 |= exynos3_epll_div[i].sdiv << PLL36XX_SDIV_SHIFT;
			epll_con0 |= 1 << EXYNOS3_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos3_epll_div)) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}

	/* 1500 max_cycls : specification data */
	locktime = 3000 * exynos3_epll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS3_EPLL_LOCK);
	__raw_writel(epll_con0, EXYNOS3_EPLL_CON0);
	__raw_writel(epll_con1, EXYNOS3_EPLL_CON1);

	do {
		tmp = __raw_readl(EXYNOS3_EPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS3_PLLCON0_LOCKED_SHIFT)));

	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos3_epll_ops = {
	.get_rate = exynos3_epll_get_rate,
	.set_rate = exynos3_epll_set_rate,
};

static struct vpll_div_data exynos3_vpll_div[] = {

	{533000000, 3, 267, 2, 32768, 0, 0, 0},
	{340000000, 3, 170, 2, 0, 0, 0},
	{300000000, 2, 100, 2, 0, 0, 0, 0},
	{266000000, 3, 266, 3, 0, 0, 0, 0},
	{160000000, 3, 160, 3, 0, 0,  0, 0},
	{ 80000000, 3, 160, 4, 0, 0, 0, 0},
};

static unsigned long exynos3_vpll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos3_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int vpll_con0, vpll_con1;
	unsigned int locktime;
	unsigned int tmp;
	unsigned int i;
	unsigned int k;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	/* Change MUX_VPLL_SEL 0: FINPLL */
	tmp = __raw_readl(EXYNOS3_CLKSRC_TOP0);
	tmp &= ~(1 << 16);
	__raw_writel(tmp, EXYNOS3_CLKSRC_TOP0);

	vpll_con0 = __raw_readl(EXYNOS3_VPLL_CON0);
	vpll_con0 &= ~(PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT |\
			PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT |\
			PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT);

	vpll_con1 = __raw_readl(EXYNOS3_VPLL_CON1);
	vpll_con1 &= ~(0xffff << 0);

	for (i = 0; i < ARRAY_SIZE(exynos3_vpll_div); i++) {
		if (exynos3_vpll_div[i].rate == rate) {
			k = (~(exynos3_vpll_div[i].k) + 1) & EXYNOS3_VPLLCON1_K_MASK;
			vpll_con1 |= k << 0;
			vpll_con0 |= exynos3_vpll_div[i].pdiv << PLL36XX_PDIV_SHIFT;
			vpll_con0 |= exynos3_vpll_div[i].mdiv << PLL36XX_MDIV_SHIFT;
			vpll_con0 |= exynos3_vpll_div[i].sdiv << PLL36XX_SDIV_SHIFT;
			vpll_con0 |= 1 << EXYNOS3_PLL_ENABLE_SHIFT;
			break;
		}
	}

	if (i == ARRAY_SIZE(exynos3_vpll_div)) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n", __func__);
		return -EINVAL;
	}

	/* 3000 max_cycls : specification data */
	locktime = 3000 * exynos3_vpll_div[i].pdiv;

	__raw_writel(locktime, EXYNOS3_VPLL_LOCK);
	__raw_writel(vpll_con0, EXYNOS3_VPLL_CON0);
	__raw_writel(vpll_con1, EXYNOS3_VPLL_CON1);

	do {
		tmp = __raw_readl(EXYNOS3_VPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS3_VPLLCON0_LOCKED_SHIFT)));

	clk->rate = rate;

	/* Change MUX_VPLL_SEL 1: FOUTVPLL */
	tmp = __raw_readl(EXYNOS3_CLKSRC_TOP0);
	tmp |= (1 << 8);
	__raw_writel(tmp, EXYNOS3_CLKSRC_TOP0);

	return 0;
}

static struct clk_ops exynos3_vpll_ops = {
	.get_rate = exynos3_vpll_get_rate,
	.set_rate = exynos3_vpll_set_rate,
};

static unsigned long exynos3_fout_apll_get_rate(struct clk *clk)
{
	return s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS3_APLL_CON0));
}

static int exynos3_fout_apll_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;

	return 0;
}

static struct clk_ops exynos3_apll_ops = {
	.get_rate = exynos3_fout_apll_get_rate,
	.set_rate = exynos3_fout_apll_set_rate,
};

static unsigned long exynos3_fout_bpll_get_rate(struct clk *clk)
{
	return s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS3_BPLL_CON0_ALIVE));
}

static struct clk_ops exynos3_bpll_ops = {
	.get_rate = exynos3_fout_bpll_get_rate,
};

static int exynos3_fixed_clk_dummy_set_rate(struct clk *clk, unsigned long rate)
{
	printk(KERN_ERR "invalid operation detected\n");
	return -1;
}

static unsigned long exynos3_sclk_mpll_mif_get_rate(struct clk *clk)
{
	return clk_get_rate(&exynos3_clk_mout_mpll.clk) / 2;
}

static struct clk_ops exynos3_sclk_mpll_mif_ops = {
	.get_rate = exynos3_sclk_mpll_mif_get_rate,
	.set_rate = exynos3_fixed_clk_dummy_set_rate,
};

static unsigned long exynos3_sclk_bpll_get_rate(struct clk *clk)
{
	return clk_get_rate(&exynos3_clk_mout_bpll.clk) / 2;
}

static struct clk_ops exynos3_sclk_bpll_ops = {
	.get_rate = exynos3_sclk_bpll_get_rate,
	.set_rate = exynos3_fixed_clk_dummy_set_rate,
};

static unsigned long exynos3_clk_cam_blk_320_get_rate(struct clk *clk)
{
	return clk_get_rate(&exynos3_clk_mout_mpll.clk) / 5;
}

static struct clk_ops exynos3_clk_cam_blk_320_ops = {
	.get_rate = exynos3_clk_cam_blk_320_get_rate,
	.set_rate = exynos3_fixed_clk_dummy_set_rate,
};

static unsigned long exynos3_clk_lcd_blk_145_get_rate(struct clk *clk)
{
	return clk_get_rate(&exynos3_clk_mout_mpll.clk) / 11;
}

static struct clk_ops exynos3_clk_lcd_blk_145_ops = {
	.get_rate = exynos3_clk_lcd_blk_145_get_rate,
	.set_rate = exynos3_fixed_clk_dummy_set_rate,
};

static int exynos3_set_fixed_clk_ops(void)
{
	exynos3_clk_sclk_mpll_mif.clk.ops = &exynos3_sclk_mpll_mif_ops;
	exynos3_clk_sclk_bpll.clk.ops = &exynos3_sclk_bpll_ops;
	exynos3_clk_cam_blk_320.clk.ops = &exynos3_clk_cam_blk_320_ops;
	exynos3_clk_lcd_blk_145.clk.ops = &exynos3_clk_lcd_blk_145_ops;

	return 0;
}

static struct clk_lookup exynos3_clk_lookup[] = {
	CLKDEV_INIT("exynos4210-uart.0", "clk_uart_baud0", &exynos3_clksrcs_uart[0].clk),
	CLKDEV_INIT("exynos4210-uart.1", "clk_uart_baud0", &exynos3_clksrcs_uart[1].clk),
	CLKDEV_INIT("exynos4210-uart.2", "clk_uart_baud0", &exynos3_clksrcs_uart[2].clk),
	CLKDEV_INIT("exynos4210-uart.3", "clk_uart_baud0", &exynos3_clksrcs_uart[3].clk),
	CLKDEV_INIT("exynos3-fb.0", "lcd", &exynos3_clk_fimd0),
	CLKDEV_INIT("dma-pl330.0", "apb_pclk", &exynos3_clk_pdma0),
	CLKDEV_INIT("dma-pl330.1", "apb_pclk", &exynos3_clk_pdma1),
	CLKDEV_INIT("s3c64xx-spi.0", "spi_busclk0", &exynos3_clksrcs_spi[0].clk),
	CLKDEV_INIT("s3c64xx-spi.1", "spi_busclk0", &exynos3_clksrcs_spi[1].clk),
};

#ifdef CONFIG_PM
static int exynos3250_clock_suspend(void)
{
	s3c_pm_do_save(exynos3250_clock_save, ARRAY_SIZE(exynos3250_clock_save));
	s3c_pm_do_save(exynos3250_epll_save, ARRAY_SIZE(exynos3250_epll_save));
	s3c_pm_do_save(exynos3250_vpll_save, ARRAY_SIZE(exynos3250_vpll_save));

	return 0;
}

static void exynos3250_pll_wait_locktime(void __iomem *con_reg, int shift_value)
{
	unsigned int tmp;

	do {
		tmp = __raw_readl(con_reg);
	} while (tmp >> EXYNOS3_PLL_ENABLE_SHIFT && !(tmp & (0x1 << EXYNOS3_PLLCON0_LOCKED_SHIFT)));
}

static void exynos3250_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos3250_clock_save, ARRAY_SIZE(exynos3250_clock_save));

	s3c_pm_do_restore_core(exynos3250_epll_save, ARRAY_SIZE(exynos3250_epll_save));
	s3c_pm_do_restore_core(exynos3250_vpll_save, ARRAY_SIZE(exynos3250_vpll_save));

	exynos3250_pll_wait_locktime(EXYNOS3_EPLL_CON0, EXYNOS3_PLLCON0_LOCKED_SHIFT);
	exynos3250_pll_wait_locktime(EXYNOS3_VPLL_CON0, EXYNOS3_PLLCON0_LOCKED_SHIFT);
	exynos3250_pll_wait_locktime(EXYNOS3_UPLL_CON0, EXYNOS3_PLLCON0_LOCKED_SHIFT);

	clk_fout_apll.rate = s5p_get_pll35xx(xtal_rate, __raw_readl(EXYNOS3_APLL_CON0));
}
#else
#define exynos3250_clock_suspend NULL
#define exynos3250_clock_resume NULL
#endif

struct syscore_ops exynos3_clock_syscore_ops = {
	.suspend	= exynos3250_clock_suspend,
	.resume		= exynos3250_clock_resume,
};

void __init_or_cpufreq exynos3250_setup_clocks(void)
{
	struct clk *xtal_clk, *vpllsrc_clk;

	unsigned long xtal;
	unsigned long apll;
	unsigned long mpll;
	unsigned long bpll;
	unsigned long vpll;
	unsigned long vpllsrc;
	unsigned long epll;
	unsigned long arm_clk;
	unsigned long sclk_dmc;

	unsigned int i;

	printk(KERN_DEBUG "%s: registering clocks\n", __func__);

	xtal_clk = clk_get(NULL, "xtal"); /* it is set in initialization code */
	BUG_ON(IS_ERR(xtal_clk));

	xtal = clk_get_rate(xtal_clk);

	xtal_rate = xtal;

	clk_put(xtal_clk);

	printk(KERN_DEBUG "%s: xtal is %ld\n", __func__, xtal);

	clk_fout_apll.ops = &exynos3_apll_ops;
	clk_fout_epll.ops = &exynos3_epll_ops;
	clk_fout_vpll.ops = &exynos3_vpll_ops;
	clk_fout_bpll.ops = &exynos3_bpll_ops;

	exynos3_set_fixed_clk_ops();

	exynos3_pll_init();

	vpllsrc_clk = clk_get(NULL, "mout_vpllsrc");
	BUG_ON(IS_ERR(vpllsrc_clk));

	vpllsrc = clk_get_rate(vpllsrc_clk);
	clk_put(vpllsrc_clk);

	/* Set and check PLLs , here the pll have already been set: exynos3_pll_init */
	apll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS3_APLL_CON0));
	mpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS3_MPLL_CON0));
	bpll = s5p_get_pll35xx(xtal, __raw_readl(EXYNOS3_BPLL_CON0_ALIVE));
	epll = s5p_get_pll36xx(xtal, __raw_readl(EXYNOS3_EPLL_CON0),
			__raw_readl(EXYNOS3_EPLL_CON1));
	vpll = s5p_get_pll36xx(vpllsrc, __raw_readl(EXYNOS3_VPLL_CON0),
			__raw_readl(EXYNOS3_VPLL_CON1));

	clk_fout_mpll.rate = mpll;
	clk_fout_bpll.rate = bpll;
	clk_fout_epll.rate = epll;
	clk_fout_vpll.rate = vpll;

	arm_clk = clk_get_rate(&exynos3_clk_arm_clk.clk);
	sclk_dmc = clk_get_rate(&exynos3_clk_sclk_dmc.clk);

	printk(KERN_INFO "EXYNOS3: apll=%ld\n mpll=%ld\n bpll=%ld\n epll=%ld\n vpll=%ld",
					apll, mpll, bpll, epll, vpll);
	printk(KERN_INFO "EXYNOS3: arm_clk=%ld, sclk_dmc=%ld\n",
			arm_clk, sclk_dmc);

	for (i = 0; i < ARRAY_SIZE(exynos3_sysclks); i++)
		s3c_set_clksrc(exynos3_sysclks[i], true);

	for (i = 0; i < ARRAY_SIZE(exynos3_clksrcs); i++)
		s3c_set_clksrc(&exynos3_clksrcs[i], true);
}

/*
 * Clock Types in Exynos3:
 * exynos3_pll_clks_off: boot off plls

 * exynos3_sysclks: clksrcs with names
 * exynos3_clksrcs_off: current is null, since div enable is not used

 * exynos3_clksrcs_uart: uart clksrcs
 * exynos3_clksrcs_spi: spi clksrcs
 * exynos3_clksrcs: common clksrcs without names
 *
 * exynos3_init_clocks: uart/right bus - not gated clocks
 * exynos3_init_clocks_off: boot off clocks

 * eyxnos3_dma_clocks: dma clks
 * exynos3_i2cs_clocks: i2c clks

 * exynos3_clkout_clks: clkout clksrcs
*/

void __init exynos3250_register_clocks(void)
{
	int ptr;
	struct clksrc_clk *clksrc;

	clk_fout_apll.enable = exynos3_apll_ctrl; /* bypass */
	clk_fout_apll.ctrlbit = (1 << 22);

	clk_fout_epll.enable = exynos3_epll_ctrl; /* bypass */
	clk_fout_epll.ctrlbit = (1 << 4);
	clk_fout_epll.rate = 12000000; /* check whether it can be changed */

	clk_fout_vpll.enable = exynos3_vpll_ctrl; /* bypass */
	clk_fout_vpll.ctrlbit = (1 << 4);
	clk_fout_vpll.rate = 12000000; /* check whether it can be changed */

	for (ptr = 0; ptr < ARRAY_SIZE(exynos3_sysclks); ptr++)
		s3c_register_clksrc(exynos3_sysclks[ptr], 1);

	s3c_register_clksrc(exynos3_clksrcs_uart, ARRAY_SIZE(exynos3_clksrcs_uart));
	s3c_register_clksrc(exynos3_clksrcs, ARRAY_SIZE(exynos3_clksrcs));

	for (ptr = 0; ptr < ARRAY_SIZE(exynos3_clkout_clks); ptr++) {
		clksrc = exynos3_clkout_clks[ptr];
		s3c_register_clksrc(clksrc, 1);
		s3c_disable_clocks(&clksrc->clk, 1);
	}

	for (ptr = 0; ptr < ARRAY_SIZE(exynos3_clksrcs_spi); ptr++) {
		s3c_register_clksrc(&exynos3_clksrcs_spi[ptr], 1);
		s3c_disable_clocks(&exynos3_clksrcs_spi[ptr].clk, 1);
	}

	s3c_register_clocks(exynos3_init_clocks, ARRAY_SIZE(exynos3_init_clocks));

	s3c24xx_register_clocks(exynos3_clk_cdev, ARRAY_SIZE(exynos3_clk_cdev));

	s3c_register_clocks(exynos3_init_clocks_off, ARRAY_SIZE(exynos3_init_clocks_off));
	s3c_disable_clocks(exynos3_init_clocks_off, ARRAY_SIZE(exynos3_init_clocks_off));

	s3c_register_clocks(exynos3_i2cs_clocks, ARRAY_SIZE(exynos3_i2cs_clocks));
	s3c_disable_clocks(exynos3_i2cs_clocks, ARRAY_SIZE(exynos3_i2cs_clocks));

	clkdev_add_table(exynos3_clk_lookup, ARRAY_SIZE(exynos3_clk_lookup));

	register_syscore_ops(&exynos3_clock_syscore_ops);

	s3c_pwmclk_init();
}
