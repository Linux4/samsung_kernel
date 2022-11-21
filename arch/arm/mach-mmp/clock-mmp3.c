/*
 *  linux/arch/arm/mach-mmp/clock-mmp3.c
 *
 *  Author:	Raul Xiong <xjian@marvell.com>
 *		Alan Zhu <wzhu10@marvell.com>
 *  Copyright:	(C) 2011 Marvell International Ltd.
 *
 *  based on arch/arm/mach-tegra/tegra2_clocks.c
 *	 Copyright (C) 2010 Google, Inc. by Colin Cross <ccross@google.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/regs-audio.h>
#include <mach/clock-audio.h>
#include <plat/clock.h>
#include "common.h"

#define CORE_NUM			3
#define PMUA_CC				APMU_REG(0x4)
#define PMUA_CC_2			APMU_REG(0x150)
#define PMUA_CC_3			APMU_REG(0x188)
#define PMUA_BUS			APMU_REG(0x6c)
#define PMUA_DM_CC			APMU_REG(0xc)
#define PMUA_DM_2_CC			APMU_REG(0x158)

/*#define MPMU_FCCR			MPMU_REG(0x0008)
#define MPMU_PLL2CR			MPMU_REG(0x0034)
#define MPMU_PLL3CR			MPMU_REG(0x001c)
#define MPMU_PLL2_CTRL1			MPMU_REG(0x0414)
#define MPMU_PLL2_CTRL2			MPMU_REG(0x0418)
#define PMUM_PLL3_CR			MPMU_REG(0x0050)
#define PMUM_POSR2			MPMU_REG(0x0054)
#define PMUM_PLL3_CTRL1			MPMU_REG(0x0058)
#define PMUM_PLL3_CTRL2			MPMU_REG(0x0060)
#define PMUM_PLL3_CTRL3			MPMU_REG(0x0064)
#define PMUM_PLL_DIFF_CTRL		MPMU_REG(0x0068)*/

#define MMP3_PROTECT_CC(x)		(((x) & 0x0003fe3f) | 0x00bc0000)
#define MMP3_PROTECT_CC2(x)		((x) & 0xfffffe07)
#define MMP3_PROTECT_CC3(x)		((x) & 0x0effff1f)
#define MMP3_PROTECT_BUSCLKRST(x)	((x) & 0x000001c3)
#define MMP3_PROTECT_FCCR(x)		((x) & 0xff83ffff)


/* Dynamic Frequency Change Part */
enum {
	MMP3_FREQ_OP_GET = 0,
	MMP3_FREQ_OP_UPDATE = 1,
	MMP3_FREQ_OP_SHOW = 2,

	MMP3_FREQ_PH_D_BY_4 = 1,
	MMP3_FREQ_PH_D_BY_6 = 2,
	MMP3_FREQ_PH_D_BY_8 = 3,
	MMP3_FREQ_PH_D_BY_10 = 4,
	MMP3_FREQ_PH_D_BY_12 = 5,
	MMP3_FREQ_PH_D_BY_14 = 6,
	MMP3_FREQ_PH_D_BY_15 = 7,

	MMP3_FREQCH_VOLTING = (1u << 27),
	MMP3_FREQCH_CORE = (1u << 24),
	MMP3_FREQCH_DRAM = (9u << 22),	/* both channel */
	MMP3_FREQCH_AXI = (1u << 26),
};

static struct clk mmp3_clk_pll1_d_2 = {
	.name = "pll1_d_2",
	.rate = 400000000,
	.ops = NULL,
};

static struct clk mmp3_clk_pll1 = {
	.name = "pll1",
	.rate = 800000000,
	.ops = NULL,
};

static int mmp3_clk_pll2_enable(struct clk *clk)
{
	u32 value;

	/* disable PLL2 */
	value = __raw_readl(MPMU_PLL2CR);

	if (value & (1 << 8))
		return 0; /* already enabled */

	value &= ~(1 << 8);
	__raw_writel(value, MPMU_PLL2CR);

	/* select VCO as clock source */
	value = __raw_readl(MPMU_PLL2_CTRL2);
	value |= 1 << 0;
	__raw_writel(value, MPMU_PLL2_CTRL2);

	/* Program PLL2 for 1200Mhz, (VCO 2.4G)*/
	__raw_writel(0x05390699, MPMU_PLL2_CTRL1);
	__raw_writel(0x001C5200, MPMU_PLL2CR);

	/* enable PLL2 */
	value = __raw_readl(MPMU_PLL2CR);
	value |= 1 << 8;
	__raw_writel(value, MPMU_PLL2CR);

	udelay(500);

	/* take PLL2 out of reset */
	value = __raw_readl(MPMU_PLL2_CTRL1);
	value |= 1 << 29;
	__raw_writel(value, MPMU_PLL2_CTRL1);

	udelay(500);

	return 0;
}

static void mmp3_clk_pll2_disable(struct clk *clk)
{
	u32 value;

	/* PLL2 Control register, disable SW PLL2 */
	value = __raw_readl(MPMU_PLL2CR);
	value &= ~(1 << 8);
	__raw_writel(value, MPMU_PLL2CR);

	/* wait for PLLs to lock */
	udelay(500);

	/* MPMU_PLL2_CTRL1: put PLL2 into reset */
	value = __raw_readl(MPMU_PLL2_CTRL1);
	value = ~(1 << 29);
	__raw_writel(value, MPMU_PLL2_CTRL1);

	udelay(500);
}

static struct clkops mmp3_clk_pll2_ops = {
	.enable = mmp3_clk_pll2_enable,
	.disable = mmp3_clk_pll2_disable,
};

static struct clk mmp3_clk_pll2 = {
	.name = "pll2",
	.rate = 1200000000,
	.ops = &mmp3_clk_pll2_ops,
};

static int mmp3_clk_pll1_clkoutp_enable(struct clk *clk)
{
	u32 value = __raw_readl(PMUM_PLL_DIFF_CTRL);
	/* Set PLL1 CLKOUTP post VCO divider as 1.5 */
	value &= ~(0xf << 0);
	value |= 1 << 0;
	value |= 1 << 4;
	__raw_writel(value, PMUM_PLL_DIFF_CTRL);

	return 0;
}

static void mmp3_clk_pll1_clkoutp_disable(struct clk *clk)
{
	u32 value = __raw_readl(PMUM_PLL_DIFF_CTRL);
	__raw_writel(value & ~(1 << 4), PMUM_PLL_DIFF_CTRL);
}

static struct clkops mmp3_clk_pll1_clkoutp_ops = {
	.enable = mmp3_clk_pll1_clkoutp_enable,
	.disable = mmp3_clk_pll1_clkoutp_disable,
};

/*
 * NOTE: actually pll1_clkoutp and pll1 share the same clock source
 * pll1_VCO, which is 1600MHz. And pll1 has a post didiver 2, pll1_clkoutp
 * has a post didiver 1.5. Since we don't expose pll1_VCO as a visible
 * clock, here we use pll1 as its parent to workaround. So we have to
 * set the div as 3 and mul as 4.
 */
static struct clk mmp3_clk_pll1_clkoutp = {
	.name = "pll1_clkoutp",
	.rate = 1066000000,
	.parent = &mmp3_clk_pll1,
	.mul = 4,
	.div = 3,
	.ops = &mmp3_clk_pll1_clkoutp_ops,
};

static int mmp3_clk_pll2_clkoutp_enable(struct clk *clk)
{
	u32 value = __raw_readl(PMUM_PLL_DIFF_CTRL);
	/* Set PLL2 CLKOUTP post VCO divider as 2.5 */
	value &= ~(0xf << 5);
	value |= 3u << 5;
	value |= 1u << 9;
	__raw_writel(value, PMUM_PLL_DIFF_CTRL);

	return 0;
}

static void mmp3_clk_pll2_clkoutp_disable(struct clk *clk)
{
	u32 value = __raw_readl(PMUM_PLL_DIFF_CTRL);
	__raw_writel(value & ~(1 << 9), PMUM_PLL_DIFF_CTRL);
}

static struct clkops mmp3_clk_pll2_clkoutp_ops = {
	.enable = mmp3_clk_pll2_clkoutp_enable,
	.disable = mmp3_clk_pll2_clkoutp_disable,
};

/*
 * NOTE: pll2_clkoutp and pll2 both divided from PLL2 VCO, which is 2.4G
 *.in current configuration. And pll2 has a post didiver 2, pll2_clkoutp
 * has a post didiver 2.5. Since we don't expose pll2_VCO as a visible
 * clock, here we use pll2 as its parent to workaround. So we have to
 * set the div as 5 and mul as 4.
 */
static struct clk mmp3_clk_pll2_clkoutp = {
	.name = "pll2_clkoutp",
	.rate = 960000000,
	.parent = &mmp3_clk_pll2,
	.mul = 4,
	.div = 5,
	.ops = &mmp3_clk_pll2_clkoutp_ops,
};


static struct clk mmp3_clk_vctcxo = {
	.name = "vctcxo",
	.rate = 26000000,
	.ops = NULL,
};

static struct clk mmp3_clk_32k = {
	.name = "32k",
	.rate = 32768,
	.ops = NULL,
};

static struct clk mmp3_clk_pll1_d_4 = {
	.name = "pll1_d_4",
	.rate = 200000000,
	.ops = NULL,
};

static int mmp3_clk_pll3_enable(struct clk *clk)
{
	u64 div_result;
	u32 tmp, pll3_ctrl1, pll3_cr, FBDIV;
	u8 VCODIV_SEL_SE, KVCO, VCO_VRNG, REFDIV = 0x3;
	unsigned long parent_rate, rate = clk->rate;

	tmp = (__raw_readl(APMU_FSIC3_CLK_RES_CTRL) >> 8) & 0xF;
	if (tmp == 0xD)
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
	else {
		pr_err("Fatal error: pll3 has error clock input config\n");
		BUG_ON(1);
	}

	/* PLL3 control register 1 - program ICP = 4 */
	pll3_ctrl1 = 0x01010099;

	/* 1.2GHz ~2.4GHz */
	if ((rate >= 1200000000UL) && (rate <= 2400000000UL))
		VCODIV_SEL_SE = 0x0 ;
	else if (rate >= 800000000UL) { /* 800MHz */
		rate /= 2;
		VCODIV_SEL_SE = 0x1;
	} else if (rate >= 600000000UL) /* 600MHz */
		VCODIV_SEL_SE = 0x2;
	else if (rate >= 480000000UL) { /* 480MHz */
		rate /= 2;
		VCODIV_SEL_SE = 0x3;
	} else if (rate >= 400000000UL) /* 400MHz */
		VCODIV_SEL_SE = 0x4;
	else if (rate >= 300000000UL)   /* 300MHz */
		VCODIV_SEL_SE = 0x5;
	else if (rate >= 240000000UL)   /* 240MHz */
		VCODIV_SEL_SE = 0x6;
	else if (rate >= 200000000UL)   /* 200MHz */
		VCODIV_SEL_SE = 0x7;
	else if (rate >= 150000000UL)   /* 150MHz */
		VCODIV_SEL_SE = 0x8;
	else {
		pr_err("Fatal error: pll3 config out of range\n");
		BUG_ON(1);
	}

	/* rate = VCO/post_div, post_div = clk->div / REFDIV / (?2:1)*/
	rate *= (clk->div / REFDIV);
	if (rate > 1200000000UL && rate <= 1360000000UL)
		KVCO = 0x1;
	else if (rate > 1360000000UL && rate <= 1530000000UL)
		KVCO = 0x2;
	else if (rate > 1530000000UL && rate <= 1700000000UL)
		KVCO = 0x3;
	else if (rate > 1700000000UL && rate <= 1900000000UL)
		KVCO = 0x4;
	else if (rate > 1900000000UL && rate <= 2100000000UL)
		KVCO = 0x5;
	else if (rate > 2100000000UL && rate <= 2300000000UL)
		KVCO = 0x6;
	else if (rate > 2300000000UL && rate <= 2400000000UL)
		KVCO = 0x7;
	else
		KVCO = 0x6;

	if (KVCO >= 0x1 && KVCO <= 0x6)
		VCO_VRNG = KVCO - 1;
	else
		VCO_VRNG = 0x5;

	pll3_ctrl1 |= (VCODIV_SEL_SE << 25 | KVCO << 19 | VCO_VRNG << 8);

	div_result = (u64)rate * REFDIV;
	do_div(div_result, parent_rate);
	FBDIV = div_result & 0x1ff;
	pll3_cr = (REFDIV << 19) | (FBDIV << 10) | (0x1 << 9);

	tmp = __raw_readl(PMUM_PLL3_CTRL2);
	/* set SEL_VCO_CLK_SE in PMUM_PLL3_CTRL2 register */
	__raw_writel(tmp | 0x00000001, PMUM_PLL3_CTRL2);

	/* PLL3 control register 1 ICP = 4.*/
	__raw_writel(pll3_ctrl1, PMUM_PLL3_CTRL1);

	/* MPMU_PLL3CR: Program PLL3 VCO */
	__raw_writel(pll3_cr, PMUM_PLL3_CR);

	/* PLL3 Control register -Enable SW PLL3 */
	tmp = __raw_readl(PMUM_PLL3_CR);
	__raw_writel(tmp | 0x00000100, PMUM_PLL3_CR);

	/* wait for PLLs to lock */
	udelay(500);

	/* PMUM_PLL3_CTRL1: take PLL3 out of reset */
	tmp = __raw_readl(PMUM_PLL3_CTRL1);
	__raw_writel(tmp | 0x20000000, PMUM_PLL3_CTRL1);

	udelay(500);

	return 0;
}

static void mmp3_clk_pll3_disable(struct clk *clk)
{
	u32 tmp = __raw_readl(PMUM_PLL3_CR);

	/* PLL3 Control register, disable SW PLL3 */
	__raw_writel(tmp & ~0x00000100, PMUM_PLL3_CR);

	/* wait for PLLs to lock */
	udelay(500);

	/* PMUM_PLL3_CTRL1: put PLL3 into reset */
	tmp = __raw_readl(PMUM_PLL3_CTRL1);
	__raw_writel(tmp & ~0x20000000, PMUM_PLL3_CTRL1);

	udelay(500);
}

static long mmp3_clk_pll3_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned int fb_div, post_div, ref_div = 0x3;
	unsigned long parent_rate, rate_rounded;
	u64 div_result;
	u32 tmp = (__raw_readl(APMU_FSIC3_CLK_RES_CTRL) >> 8) & 0xF;

	if (tmp == 0xD)
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
	else {
		pr_err("Fatal error: pll3 has error clock input config");
		BUG_ON(1);
	}

	/* 1.2GHz ~2.4GHz */
	if (rate >= 1200000000UL && rate <= 2400000000UL)
		post_div = 1;
	else if (rate >= 800000000UL) /* 800MHz */
		post_div = 3;         /* post divider actually is 1.5; */
	else if (rate >= 600000000UL) /* 600MHz */
		post_div = 2;
	else if (rate >= 480000000UL) /* 480MHz */
		post_div = 5;         /* post divider actually is 2.5; */
	else if (rate >= 400000000UL) /* 400MHz */
		post_div = 3;
	else if (rate >= 300000000UL) /* 300MHz */
		post_div = 4;
	else if (rate >= 240000000UL) /* 240MHz */
		post_div = 5;
	else if (rate >= 200000000UL) /* 200MHz */
		post_div = 6;
	else if (rate >= 150000000UL) /* 150MHz */
		post_div = 8;
	else {
		pr_err("Fatal error: pll3 config out of range: %lu\n", rate);
		BUG_ON(1);
	}

	div_result = ((u64)rate) * ref_div * post_div;
	div_result += parent_rate / 2; /* to round the FBDIV */
	do_div(div_result, parent_rate);
	fb_div = div_result;

	div_result = ((u64)parent_rate) * fb_div;
	do_div(div_result, (ref_div * post_div));
	rate_rounded = div_result;

	return rate_rounded;
}

static int mmp3_clk_pll3_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate;
	u64 div_result;
	unsigned int ref_div = 0x3;
	clk->div = 0;

	parent_rate = clk_get_rate(&mmp3_clk_vctcxo);

	if (rate >= 1200000000UL && rate <= 2400000000UL)  /* 1.2GHz ~2.4GHz */
		clk->div = 1;
	else if (rate >= 800000000UL) /* 800MHz */
		clk->div = 3;
	else if (rate >= 600000000UL) /* 600MHz */
		clk->div = 2;
	else if (rate >= 480000000UL) /* 480MHz */
		clk->div = 5;
	else if (rate >= 400000000UL) /* 400MHz */
		clk->div = 3;
	else if (rate >= 300000000UL) /* 300MHz */
		clk->div = 4;
	else if (rate >= 240000000UL) /* 240MHz */
		clk->div = 5;
	else if (rate >= 200000000UL) /* 200MHz */
		clk->div = 6;
	else if (rate >= 150000000UL) /* 150MHz */
		clk->div = 8;
	else {
		pr_err("Fatal error: pll3 config out of range\n");
		BUG_ON(1);
	}
	clk->div *= ref_div;
	div_result = ((u64)rate) * clk->div;
	div_result += parent_rate / 2; /* to round the mul */
	do_div(div_result, parent_rate);
	clk->mul = div_result;

	return 0;
}

static struct clk_mux_sel mux_pll1_pll2_vctcxo[] = {
	{.input = &mmp3_clk_pll1_d_2, .value = 0},
	{.input = &mmp3_clk_pll1, .value = 1},
	{.input = &mmp3_clk_pll2, .value = 2},
	{.input = &mmp3_clk_pll1_clkoutp, .value = 3},
	{.input = &mmp3_clk_vctcxo, .value = 4},
	{0, 0},
};

static void mmp3_core_clk_trigger_change(void)
{
	u32 val;
	val = __raw_readl(PMUA_CC);
	val = MMP3_PROTECT_CC(val);	/* set reserved */
	val = val | MMP3_FREQCH_CORE | MMP3_FREQCH_VOLTING;
	/* A0 need to all cores run, we use coherent broadcasts */
	dsb();
	/* trigger change */
	__raw_writel(val, PMUA_CC);
	/* done, PJ_RD_STATUS should have been cleared by HW */
}

static void mmp3_clk_source_init(struct clk *c)
{
	u32 val, source;
	const struct clk_mux_sel *sel;

	c->dynamic_change = 1;
	c->mul = 1;
	c->div = 1;

	val = __raw_readl(c->reg_data[SOURCE][STATUS].reg);
	source = (val >> c->reg_data[SOURCE][STATUS].reg_shift)
	    & c->reg_data[SOURCE][STATUS].reg_mask;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->value == source)
			break;
	}
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;
}

static int mmp3_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;

	val = __raw_readl(c->reg_data[SOURCE][CONTROL].reg);
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->reg_data[SOURCE][CONTROL].reg == MPMU_FCCR)
				val = MMP3_PROTECT_FCCR(val);
			else if (c->reg_data[SOURCE][CONTROL].reg == PMUA_BUS)
				val = MMP3_PROTECT_BUSCLKRST(val);
			val &= ~(c->reg_data[SOURCE][CONTROL].reg_mask
				 << c->reg_data[SOURCE][CONTROL].reg_shift);
			val |= sel->value
			    << c->reg_data[SOURCE][CONTROL].reg_shift;
			__raw_writel(val, c->reg_data[SOURCE][CONTROL].reg);
			/*
			 * FIXME: DO NOT trigger here since
			 * we will triggered the changes together later
			 */
			/* mmp3_core_clk_trigger_change(); */
			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clkops mmp3_clk_root_ops = {
	.init = mmp3_clk_source_init,
	.set_parent = mmp3_clk_set_parent,
};

static struct clk mmp3_clk_core_root = {
	.name = "core_root",
	.inputs = mux_pll1_pll2_vctcxo,
	.ops = &mmp3_clk_root_ops,
	.reg_data = {
		     { {MPMU_FCCR, 29, 0x7}, {MPMU_FCCR, 29, 0x7} },
		     { {0, 0, 0}, {0, 0, 0} } },
};

static void mmp3_clk_div_init(struct clk *c)
{
	u32 val;

	c->dynamic_change = 1;
	c->mul = 1;

	val = __raw_readl(c->reg_data[DIV][STATUS].reg);
	c->div = ((val >> c->reg_data[DIV][STATUS].reg_shift)
		  & c->reg_data[DIV][STATUS].reg_mask) + 1;
}

static int mmp3_clk_set_rate(struct clk *c, unsigned long rate)
{
	int i;
	int max_div = c->reg_data[DIV][CONTROL].reg_mask + 1;
	u32 val = __raw_readl(c->reg_data[DIV][CONTROL].reg);
	unsigned long parent_rate = clk_get_rate(c->parent);

	for (i = 1; i <= max_div; i++) {
		if (rate == parent_rate / i) {
			if (c->reg_data[DIV][CONTROL].reg == PMUA_CC)
				val = MMP3_PROTECT_CC(val);
			else if (c->reg_data[DIV][CONTROL].reg == PMUA_CC_2)
				val = MMP3_PROTECT_CC2(val);
			else if (c->reg_data[DIV][CONTROL].reg == PMUA_CC_3)
				val = MMP3_PROTECT_CC3(val);

			val &= ~(c->reg_data[DIV][CONTROL].reg_mask
				 << c->reg_data[DIV][CONTROL].reg_shift);
			val |= (i - 1) << c->reg_data[DIV][CONTROL].reg_shift;
			__raw_writel(val, c->reg_data[DIV][CONTROL].reg);
			/*
			 * FIXME: DO NOT trigger here since
			 * we will triggered the changes together later
			 */
			/* mmp3_core_clk_trigger_change(); */
			c->div = i;
			c->mul = 1;
			return 0;
		}
	}
	return -EINVAL;
}

static struct clkops mmp3_clk_div_ops = {
	.init = mmp3_clk_div_init,
	.setrate = mmp3_clk_set_rate,
};

static struct clk mmp3_clk_virtual_pj = {
	.name = "pj",
	.parent = &mmp3_clk_core_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_CC, 0, 0x7}, {PMUA_CC, 0, 0x7} } },
};

static struct clk mmp3_clk_mp1 = {
	.name = "mp1",
	.parent = &mmp3_clk_virtual_pj,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 9, 0xf}, {PMUA_CC_2, 9, 0xf} } },
};

static struct clk mmp3_clk_mp2 = {
	.name = "mp2",
	.parent = &mmp3_clk_virtual_pj,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 13, 0xf}, {PMUA_CC_2, 13, 0xf} } },
};

static struct clk mmp3_clk_mm = {
	.name = "mm",
	.parent = &mmp3_clk_virtual_pj,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 17, 0xf}, {PMUA_CC_2, 17, 0xf} } },
};

static struct clk mmp3_clk_aclk = {
	.name = "aclk",
	.parent = &mmp3_clk_virtual_pj,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 21, 0xf}, {PMUA_CC_2, 21, 0xf} } },
};

static void mmp3_core_periph_init(struct clk *c)
{
	u32 val, div_val;

	c->dynamic_change = 1;
	c->mul = 1;

	val = __raw_readl(c->reg_data[DIV][STATUS].reg);
	div_val = (val >> c->reg_data[DIV][STATUS].reg_shift)
	    & c->reg_data[DIV][STATUS].reg_mask;
	if (div_val != 7)
		c->div = (div_val + 1) * 2;
	else
		c->div = 15;
}

static int mmp3_core_periph_set_rate(struct clk *c, unsigned long rate)
{
	int i;
	int max_div = 15;
	u32 val = __raw_readl(c->reg_data[DIV][CONTROL].reg);
	unsigned long parent_rate = clk_get_rate(c->parent);

	for (i = 4; i < max_div; i += 2) {
		if (rate == parent_rate / i) {
			val = MMP3_PROTECT_CC(val);
			val &= ~(c->reg_data[DIV][CONTROL].reg_mask
				 << c->reg_data[DIV][CONTROL].reg_shift);
			val |= (i / 2 - 1)
			    << c->reg_data[DIV][CONTROL].reg_shift;
			__raw_writel(val, c->reg_data[DIV][CONTROL].reg);
			/*
			 * FIXME: DO NOT trigger here since
			 * we will triggered the changes together later
			 */
			/* mmp3_core_clk_trigger_change(); */
			c->div = i;
			c->mul = 1;
			return 0;
		}
	}

	if (rate == parent_rate / max_div) {
		val = MMP3_PROTECT_CC(val);
		val &= ~(c->reg_data[DIV][CONTROL].reg_mask
			 << c->reg_data[DIV][CONTROL].reg_shift);
		val |= 7 << c->reg_data[DIV][CONTROL].reg_shift;
		__raw_writel(val, c->reg_data[DIV][CONTROL].reg);
		/*
		 * FIXME: DO NOT trigger here since
		 * we will triggered the changes together later
		 */
		/* mmp3_core_clk_trigger_change(); */
		c->div = max_div;
		c->mul = 1;
		return 0;
	}

	return -EINVAL;
}

static struct clkops mmp3_core_periph_ops = {
	.init = mmp3_core_periph_init,
	.setrate = mmp3_core_periph_set_rate,
};

static struct clk mmp3_clk_core_periph = {
	.name = "periph",
	.parent = &mmp3_clk_core_root,
	.ops = &mmp3_core_periph_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 25, 0x7}, {PMUA_CC, 9, 0x7} } },
};

static struct clk mmp3_clk_atclk = {
	.name = "atclk",
	.parent = &mmp3_clk_core_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_CC, 3, 0x7}, {PMUA_CC, 3, 0x7} } },
};

static struct {
	struct clk *source_clk;
	unsigned long pj_rate;
	unsigned long mp1_rate;
	unsigned long mp2_rate;
	unsigned long mm_rate;
	unsigned long aclk_rate;
	unsigned long ph_rate;
	unsigned long atclk_rate;
} mmp3_op_table[] = {
	{
	&mmp3_clk_pll1_d_2, 200000000, 200000000,
		    200000000, 200000000, 200000000, 100000000, 200000000}, {
	&mmp3_clk_pll1, 400000000, 400000000,
		    400000000, 400000000, 400000000, 200000000, 400000000}, {
	&mmp3_clk_pll1, 800000000, 800000000,
		    800000000, 400000000, 400000000, 200000000, 400000000}, {
	&mmp3_clk_pll1_clkoutp, 1062000000, 1062000000,
		    1062000000, 531000000, 531000000, 177000000, 354000000}, {
	    /* must be the last line! */
	NULL, 0, 0, 0, 0, 0, 0, 0} };

static void mmp3_cpu_clk_init(struct clk *c)
{
	c->dynamic_change = 1;
	c->mul = 1;
	c->div = 1;
}

static int mmp3_cpu_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret, index, i;

	for (i = 0; mmp3_op_table[i].source_clk != NULL; i++) {
		if (mmp3_op_table[i].mp1_rate >= rate) {
			index = i;
			break;
		}
	}
	if (mmp3_op_table[i].source_clk == NULL)
		return -EINVAL;

	/* obtain FC onwership, should always pass */
	i = 1000;
	while ((__raw_readl(PMUA_DM_CC) & (1u << 24)) != 0) {
		i--;
		if (i <= 0) {
			pr_err("Cannot gain owner of PMU DFC\n");
			return -EAGAIN;
		}
	}

	ret = clk_set_parent(&mmp3_clk_core_root,
			     mmp3_op_table[index].source_clk);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_virtual_pj, mmp3_op_table[index].pj_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_mp1, mmp3_op_table[index].mp1_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_mp2, mmp3_op_table[index].mp2_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_mm, mmp3_op_table[index].mm_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_aclk, mmp3_op_table[index].aclk_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_core_periph, mmp3_op_table[index].ph_rate);
	if (ret)
		return ret;

	ret = clk_set_rate(&mmp3_clk_atclk, mmp3_op_table[index].atclk_rate);
	if (ret)
		return ret;

	mmp3_core_clk_trigger_change();

	return 0;
}

static struct clkops mmp3_cpu_ops = {
	.init = mmp3_cpu_clk_init,
	.setrate = mmp3_cpu_clk_set_rate,
};

static struct clk mmp3_clk_virtual_cpu __maybe_unused = {
	.name = "cpu",
	.parent = &mmp3_clk_virtual_pj,
	.ops = &mmp3_cpu_ops,
};

/* ddr clock definitions */
static struct clk mmp3_clk_ddr_root = {
	.name = "ddr_root",
	.inputs = mux_pll1_pll2_vctcxo,
	.ops = &mmp3_clk_root_ops,
	.reg_data = {
		     { {MPMU_FCCR, 23, 0x7}, {MPMU_FCCR, 23, 0x7} },
		     { {0, 0, 0}, {0, 0, 0} } },
};

static struct clk mmp3_clk_ddr1 = {
	.name = "ddr1",
	.parent = &mmp3_clk_ddr_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_CC, 12, 0x7}, {PMUA_CC, 12, 0x7} } },
};

static struct clk mmp3_clk_ddr2 = {
	.name = "ddr2",
	.parent = &mmp3_clk_ddr_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_CC, 9, 0x7}, {PMUA_CC_3, 17, 0x7} } },
};

/* axi clock definitions */
static struct clk mmp3_clk_axi_root = {
	.name = "axi_root",
	.inputs = mux_pll1_pll2_vctcxo,
	.ops = &mmp3_clk_root_ops,
	.reg_data = {
		     { {PMUA_BUS, 6, 0x7}, {PMUA_BUS, 6, 0x7} },
		     { {0, 0, 0}, {0, 0, 0} } },
};

static struct clk mmp3_clk_axi1 = {
	.name = "axi1",
	.parent = &mmp3_clk_axi_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_CC, 15, 0x7}, {PMUA_CC, 15, 0x7} } },
};

static struct clk mmp3_clk_axi2 = {
	.name = "axi2",
	.parent = &mmp3_clk_axi_root,
	.ops = &mmp3_clk_div_ops,
	.reg_data = {
		     { {0, 0, 0}, {0, 0, 0} },
		     { {PMUA_DM_2_CC, 0, 0x7}, {PMUA_CC_2, 0, 0x7} } },
};

static int clk_cpu_setrate(struct clk *clk, unsigned long val)
{
	/*
	 * FIXME this need change when smp process id to real process id
	 * mapping is ready, then we can do frequency table for each core
	 * currently the mapping depends on which core to boot.
	 * Also currently we assume only MP1/2 are active and they always runs
	 * at the same frequency. so we only use and trigger DFC target on MP1
	 */
	/* mmp3_setfreq(MMP3_CLK_MP1, val);*/
	return 0;
}

static unsigned long clk_cpu_getrate(struct clk *clk)
{
	/* return mmp3_getfreq(MMP3_CLK_MP1); */
	return 0;
}

static struct clkops clk_cpu_ops = {
	.setrate = clk_cpu_setrate,
	.getrate = clk_cpu_getrate,
};

static struct clk mmp3_clk_cpu = {
	.name = "cpu",
	.ops = &clk_cpu_ops,
	.dynamic_change = 1,
};

#define GC2D_CLK_DIV(n)		((n & 0xF) << 28)
#define GC2D_CLK_DIV_MSK	GC2D_CLK_DIV(0xF)
#define GC2D_CLK_SRC_SEL(n)	((n & 3) << 12)
#define GC2D_CLK_SRC_SEL_MSK	GC2D_CLK_SRC_SEL(3)
#define GC2D_AXICLK_EN		(1u << 19)

#define GC3D_CLK_DIV(n)		((n & 0xF) << 24)
#define GC3D_CLK_DIV_MSK	GC3D_CLK_DIV(0xF)
#define GC3D_CLK_SRC_SEL(n)	((n & 3) << 6)
#define GC3D_CLK_SRC_SEL_MSK	GC3D_CLK_SRC_SEL(3)
#define GC3D_ACLK_SEL(n)	((n & 3) << 4)
#define GC3D_ACLK_SEL_MSK	GC3D_ACLK_SEL(3)
#define GC3D_AXICLK_EN		(1u << 2)

#define GC2D3D_CLK_EN		(1u << 3)

#define GC_PWRUP(n)		((n & 3) << 9)
#define GC_PWRUP_MSK		GC_PWRUP(3)
#define GC_ISB			(1u << 8)

#define GC_CLK_RATE(div, src, aclk)				\
	(GC2D_CLK_DIV(div) | GC2D_CLK_SRC_SEL(src)		\
	| GC3D_CLK_DIV(div) | GC3D_CLK_SRC_SEL(src)		\
	| GC3D_ACLK_SEL(aclk))

#define GC_CLK_RATE_MSK						\
	(GC2D_CLK_DIV_MSK | GC2D_CLK_SRC_SEL_MSK		\
	| GC3D_CLK_DIV_MSK | GC3D_CLK_SRC_SEL_MSK		\
	| GC3D_ACLK_SEL_MSK)

#define		CS_PLL1		0
#define		CS_PLL2		1
#define		CS_PLL1_COP	2
#define		CS_PLL2_COP	3
#define		PLL1D4		0
#define		PLL1D6		1
#define		PLL1D2		2
#define		PLL2D2		3



#define GC_SET_BITS(set, clear)	{\
	unsigned long tmp;\
	tmp = __raw_readl(clk->clk_rst);\
	tmp &= ~(clear);\
	tmp |= set;\
	__raw_writel(tmp, clk->clk_rst);\
}

static void gc_clk_init(struct clk *clk)
{
	clk->rate = clk_get_rate(&mmp3_clk_pll1_clkoutp) / 2;
	clk->enable_val = PLL1D2; /* reuse this field for the ACLK setting */
	clk->div = 2;
	clk->mul = 1;
	clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
}

static int gc_clk_enable(struct clk *clk)
{
	unsigned long gc_rate_cfg;
	unsigned long i;

	/* TODO may need to request for different core voltage according to
	 * different gc clock rate.
	 */

	i = 0;
	while ((clk->inputs[i].input != clk->parent) && clk->inputs[i].input)
		i++;

	if (clk->inputs[i].input == 0) {
		pr_err("%s: unexpected gc clock source\n", __func__);
		return -1;
	}

	gc_rate_cfg = GC_CLK_RATE(clk->div,
		clk->inputs[i].value, clk->enable_val);
	gc_rate_cfg &= GC_CLK_RATE_MSK;

	GC_SET_BITS(gc_rate_cfg, GC_CLK_RATE_MSK);
	GC_SET_BITS(GC2D3D_CLK_EN | GC2D_AXICLK_EN | GC3D_AXICLK_EN, 0);

	return 0;
}

static void gc_clk_disable(struct clk *clk)
{
	GC_SET_BITS(0, GC2D_AXICLK_EN | GC3D_AXICLK_EN | GC2D3D_CLK_EN);
}

static long gc_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (rate <= clk_get_rate(&mmp3_clk_pll1)/8)
		return clk_get_rate(&mmp3_clk_pll1)/8; /* 100M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1)/4)
		return clk_get_rate(&mmp3_clk_pll1)/4; /* 200M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1_clkoutp)/3)
		return clk_get_rate(&mmp3_clk_pll1_clkoutp)/3; /* 354M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1)/2)
		return clk_get_rate(&mmp3_clk_pll1)/2; /* 400M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2_clkoutp)/2)
		return clk_get_rate(&mmp3_clk_pll2_clkoutp)/2; /* 480M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1_clkoutp)/2)
		return clk_get_rate(&mmp3_clk_pll1_clkoutp)/2; /* 531M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2)/2)
		return clk_get_rate(&mmp3_clk_pll2)/2; /* 600M */
	else
		return clk_get_rate(&mmp3_clk_pll1); /* 800M */
}

static int gc_clk_setrate(struct clk *clk, unsigned long rate)
{
	clk->mul = 1;
	if (rate == clk_get_rate(&mmp3_clk_pll1)/8) {
		clk->enable_val = PLL1D6;
		clk->div = 8;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)/4) {
		clk->enable_val = PLL1D4;
		clk->div = 4;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1_clkoutp)/3) {
		clk->enable_val = PLL1D2;
		clk->div = 3;
		clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)/2) {
		clk->enable_val = PLL1D2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2_clkoutp)/2) {
		clk->enable_val = PLL1D2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll2_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1_clkoutp)/2) {
		clk->enable_val = PLL1D2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2)/2) {
		clk->enable_val = PLL2D2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll2);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)) {
		clk->enable_val = PLL1D2;
		clk->div = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else {
		pr_err("%s: unexpected gc clock rate %ld\n", __func__, rate);
		BUG();
	}

	return 0;
}

struct clkops gc_clk_ops = {
	.init		= gc_clk_init,
	.enable		= gc_clk_enable,
	.disable	= gc_clk_disable,
	.setrate	= gc_clk_setrate,
	.round_rate	= gc_clk_round_rate,
	.set_parent	= mmp3_clk_set_parent,
};

static struct clk_mux_sel gc_mux_pll1_pll2[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 1},
	{.input = &mmp3_clk_pll1_clkoutp, .value = 2},
	{.input = &mmp3_clk_pll2_clkoutp, .value = 3},
	{0, 0},
};

static struct clk mmp3_clk_gc = {
	.name = "gc",
	.inputs = gc_mux_pll1_pll2,
	.lookup = {
		.con_id = "GCCLK",
	},
	.clk_rst = (void __iomem *)APMU_GC,
	.ops = &gc_clk_ops,
};

static int disp1_axi_clk_enable(struct clk *clk)
{
	u32 val = __raw_readl(clk->clk_rst);

	/* enable Display1 AXI clock */
	val |= (1<<3);
	__raw_writel(val, clk->clk_rst);

	/* release from reset */
	val |= 1 |  (1 << 1);
	__raw_writel(val, clk->clk_rst);
	return 0;
}

static void disp1_axi_clk_disable(struct clk *clk)
{
	u32 val = __raw_readl(clk->clk_rst);

	/*
	 * disable display1 AXI clock:
	 * In MMP3 A0, display1 AXI clock enable and reset
	 * are connected opposite at the bus clock module,
	 * so on A0, bit[0]: control axi clock enable/disable;
	 * bit[3]: axi clock reset control. From B0, will fix back.
	 */
	val &= ~(1<<3);
	__raw_writel(val, clk->clk_rst);
}

struct clkops disp1_axi_clk_ops = {
	.enable		= disp1_axi_clk_enable,
	.disable	= disp1_axi_clk_disable,
};

static struct clk mmp3_clk_disp1_axi = {
	.name = "disp1_axi",
	.lookup = {
		.con_id = "DISP1AXI",
	},
	.clk_rst = (void __iomem *)APMU_LCD_CLK_RES_CTRL,
	.ops = &disp1_axi_clk_ops,
};

static int disp1_clk_enable(struct clk *clk)
{
	u32 val;

	val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	/* enable Display1 peripheral clock */
	val |= (1 << 4);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	/* release from reset */
	val |= (1 << 1);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	return 0;
}

static void disp1_clk_disable(struct clk *clk)
{
	u32 val;

	val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	/* disable Display1 peripheral clock */
	val &= ~(1<<4);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
}

static long disp1_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/*
	 * disp1 clock actually has four clock source: pll1, pll1/16, pll2
	 * and vctcxo.The range of the divider is 1 to 15.
	 * here the policy is try to not use pll2 as possile as it can.
	 * since the divider can be as large as 15 so it don't need pll1/16
	 * as the clock source. For the very low rate requirement, we can
	 * just use vctcxo as the clock source.
	 */
	int i;
	unsigned long parent_rate;

	/* for those which is less than 26M, use vctcxo as clock source */
	if (rate <= clk_get_rate(&mmp3_clk_vctcxo)) {
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}

		return parent_rate / (i - 1);
	/* for those which is less than 800M, use pll1 as clock source */
	} else if (rate <= clk_get_rate(&mmp3_clk_pll1)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}

		return parent_rate / (i - 1);
	/* for those which is larger than 800M, use pll2 as clock source */
	} else
		return clk_get_rate(&mmp3_clk_pll2);
}

static int disp1_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate;
	const struct clk_mux_sel *sel;
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	if (rate <= clk_get_rate(&mmp3_clk_vctcxo)) {
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_vctcxo);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_vctcxo)
				break;
		}
		if (sel->input == 0) {
			pr_err("lcd: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	} else if (rate <= clk_get_rate(&mmp3_clk_pll1)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll1);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll1)
				break;
		}
		if (sel->input == 0) {
			pr_err("lcd: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	} else if (rate <= clk_get_rate(&mmp3_clk_pll2)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll2);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll2);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll2)
				break;
		}
		if (sel->input == 0) {
			pr_err("lcd: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	}

	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	return 0;
}

struct clkops disp1_clk_ops = {
	.enable = disp1_clk_enable,
	.disable = disp1_clk_disable,
	.round_rate = disp1_clk_round_rate,
	.setrate = disp1_clk_setrate,
};

static struct clk_mux_sel disp1_clk_mux[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 2},
	{.input = &mmp3_clk_vctcxo, .value = 3},
	{0, 0},
};

/* Disp1 clock can be one of the source for lcd */
static struct clk mmp3_clk_disp1 = {
	.name = "disp1",
	.lookup = {
		.con_id = "DISP1_CLK",
	},
	.ops = &disp1_clk_ops,
	.inputs = disp1_clk_mux,
	.reg_data = {
		     { {APMU_LCD_CLK_RES_CTRL, 6, 0x3},
			{APMU_LCD_CLK_RES_CTRL, 6, 0x3} },
		     {{APMU_LCD_CLK_RES_CTRL, 8, 0xf},
			{APMU_LCD_CLK_RES_CTRL, 8, 0xf} } },
};

static struct clk *disp_depend_clk[] = {
	&mmp3_clk_disp1_axi,
	&mmp3_clk_disp1,
};

#define LCD_PN1_DSI_PHYSLOW_PRER	0x1A
#define LCD_PN1_DSI_PHYSLOW_PRER_SHIFT	15
#define LCD_PN1_DSI_PHYSLOW_PRER_MASK	0x1F

static int lcd_pn1_clk_enable(struct clk *clk)
{
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	if (clk->parent == &mmp3_clk_vctcxo) {
		/* enable DSI PHY ESC/SLOW clock */
		val |= (1 << 12) | (1 << 5);
		__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

		/* use fixed prescaler for DSI PHY slow clock */
		val &= ~(LCD_PN1_DSI_PHYSLOW_PRER_MASK <<
			LCD_PN1_DSI_PHYSLOW_PRER_SHIFT);
		val |= (LCD_PN1_DSI_PHYSLOW_PRER <<
				LCD_PN1_DSI_PHYSLOW_PRER_SHIFT);
		__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

		/* release DSI PHY SLOW clock from reset */
		val |= (1 << 2);
		__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
		/*
		 * Here we fix the lcd clock souce as DSI PLL(PLL3). Because it
		 * provides a more flexible clocking options for the DSI
		 * interface which has very peculiar frequency requirements
		 * driven by the display panel parameters. So enable the pll3
		 * clk here.
		 */
		mmp3_clk_pll3_enable(clk);
	} else if (clk->parent == &mmp3_clk_pll1) {
		/* select PLL1 as clock source and disable unused clocks */
		val &= ~((3 << 6) | (1 << 12) | (1 << 5) | (1 << 2));
		/* divider select 1 */
		val &= ~(3 << 8);
		val |= 1 << 8;

		__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
	} else {
		pr_err("%s clk->parent not inited\n", __func__);
		return -EAGAIN;
	}

	/* release from reset */
	val |= (1 << 1);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	pr_info("%s PMUA_DISPLAY1 = 0x%x, clk from %s", __func__, val,
		(clk->parent == &mmp3_clk_vctcxo) ?  "pll3" : "pll1");

	return 0;
}

static void lcd_pn1_clk_disable(struct clk *clk)
{
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	if (clk->parent == &mmp3_clk_vctcxo) {
		/*
		 * lcd clock source is fixed as DSI PLL(PLL3),
		 * so disable pll3 clk
		 */
		mmp3_clk_pll3_disable(clk);

		/* disable DSI clock */
		val &= ~((1<<12) | (1<<5));
		__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	}

	pr_info("%s PMUA_DISPLAY1 = 0x%x, clk from %s", __func__, val,
		(clk->parent == &mmp3_clk_vctcxo) ?  "pll3" : "pll1");
}

static long lcd_clk_round_rate(struct clk *clk, unsigned long rate)
{
	switch (rate) {
	case 800000000:
		clk_reparent(clk, &mmp3_clk_pll1);
		break;
	default:
		clk_reparent(clk, &mmp3_clk_vctcxo);
		if (clk->dependence_count > 1)
			/* remove depend clk disp1 */
			clk->dependence_count--;
		rate = mmp3_clk_pll3_round_rate(clk, rate);
		break;
	}
	pr_debug("%s line %d rate %lu\n\n", __func__, __LINE__, rate);

	return rate;
}

static int lcd_clk_setrate(struct clk *clk, unsigned long rate)
{
	int ret = 0;

	switch (rate) {
	case 800000000:
		break;
	default:
		/*
		 * lcd clock source is fixed as DSI PLL(PLL3),
		 * so set pll3 clk rate
		 */
		ret = mmp3_clk_pll3_setrate(clk, rate);
		break;
	}
	pr_debug("%s line %d rate %lu\n\n", __func__, __LINE__, rate);

	return ret;
}

struct clkops lcd_pn1_clk_ops = {
	.enable = lcd_pn1_clk_enable,
	.disable = lcd_pn1_clk_disable,
	.round_rate = lcd_clk_round_rate,
	.setrate = lcd_clk_setrate,
};

static struct clk_mux_sel lcd_pn1_clk_mux[] = {
	{.input = &mmp3_clk_vctcxo, 0},
	{0, 0},
};

/*
 * lcd clk actually has five clock souce: axi, display 1,
 * display 2, HDMI PLL and DSI PLL(PLL3). Here we just fix it to be
 * PLL3 since it provides a more flexible clocking options for the
 * DSI interface which has very peculiar frequency requirements driven
 * by the display panel parameters. And PLL3 satisfy the LCD/DSI very well.
 */
static struct clk mmp3_clk_lcd1 = {
	.name = "lcd1",
	.lookup = {
		.con_id = "LCDCLK",
	},
	.ops = &lcd_pn1_clk_ops,
	.dependence = disp_depend_clk,
	.dependence_count = ARRAY_SIZE(disp_depend_clk),
	.inputs = lcd_pn1_clk_mux,
	.reg_data = {
		     {{APMU_LCD_CLK_RES_CTRL, 6, 0x3},
			{APMU_LCD_CLK_RES_CTRL, 6, 0x3} },
		     {{APMU_LCD_CLK_RES_CTRL, 8, 0xf},
			{APMU_LCD_CLK_RES_CTRL, 8, 0xf} } },
};

static int hdmi_clk_enable(struct clk *clk)
{
	/*
	 * hdmi pll clock enable is done by user space,
	 * to control it's dependence clock disp1_axi
	 * here we just enable hdmi ref clock
	 */
	u32 val = __raw_readl(clk->clk_rst);
	val |= (1 << 13);
	__raw_writel(val, clk->clk_rst);
	return 0;
};

static void hdmi_clk_disable(struct clk *clk)
{
	/*
	 * hdmi pll clock disable is done by user space,
	 * to control it's dependence clock disp1_axi
	 * here we just enable hdmi ref clock
	 */

	u32 val = __raw_readl(clk->clk_rst);
	val &= ~(1 << 13);
	__raw_writel(val, clk->clk_rst);
	return;
};

struct clkops hdmi_clk_ops = {
	.enable = hdmi_clk_enable,
	.disable = hdmi_clk_disable,
};

static struct clk mmp3_clk_hdmi = {
	.name = "hdmi",
	.lookup = {
		.con_id = "HDMICLK",
	},
	.clk_rst = (void __iomem *)APMU_LCD_CLK_RES_CTRL,
	.ops = &hdmi_clk_ops,
	.dependence = disp_depend_clk,
	.dependence_count = ARRAY_SIZE(disp_depend_clk),
};

static int thermal_clk_enable(struct clk *clk)
{
	uint32_t clk_rst, i;
	ulong inc[4] = {0, 0x8, 0xc, 0x10};
	ulong base;

	for (i = 0; i < 4; i++) {
		base = (ulong)clk->clk_rst + inc[i];
		clk_rst = __raw_readl(base);
		clk_rst |= APBC_FNCLK | APBC_FNCLKSEL(0x0);
		__raw_writel(clk_rst, base);
		/*
		 * delay two cycles of the solwest clock between the APB
		 * bus clock and the functional module clock.
		 */
		udelay(10);

		clk_rst |= APBC_APBCLK;
		__raw_writel(clk_rst, base);
		udelay(10);

		clk_rst &= ~(APBC_RST);
		__raw_writel(clk_rst, base);
	}

	return 0;
}

static void thermal_clk_disable(struct clk *clk)
{
	int inc[4] = {0, 0x8, 0xc, 0x10}, i;

	for (i = 0; i < 4; i++)
		__raw_writel(0, clk->clk_rst + inc[i]);
	mdelay(1);
}

struct clkops thermal_clk_ops = {
	.enable = thermal_clk_enable,
	.disable = thermal_clk_disable,
};

static struct clk mmp3_clk_thermal = {
	.name = "mmp-thermal",
	.lookup = {
		.con_id = "THERMALCLK",
	},
	.clk_rst = (void __iomem *)APBC_MMP2_THSENS1,
	.ops = &thermal_clk_ops,
};

static void sdhc_clk_init(struct clk *clk)
{
	const struct clk_mux_sel *sel;
	u32 val = 0;

	clk->mul = 1;
	clk->div = 1;

	clk_reparent(clk, &mmp3_clk_pll1_d_4);

	val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
		 << clk->reg_data[DIV][CONTROL].reg_shift);
	val |= clk->div
	    << clk->reg_data[DIV][CONTROL].reg_shift;

	for (sel = clk->inputs; sel->input != 0; sel++) {
		if (sel->input == &mmp3_clk_pll1_d_4)
			break;
	}
	if (sel->input == 0) {
		pr_err("sdh: no matched input for this parent!\n");
		BUG();
	}

	val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
		<< clk->reg_data[SOURCE][CONTROL].reg_shift);
	val |= sel->value
		<< clk->reg_data[SOURCE][CONTROL].reg_shift;

	clk->enable_val = val;
}

static int sdhc_clk_enable(struct clk *clk)
{
	u32 val;

	val = clk->enable_val | 0x1b;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	return 0;
}

static void sdhc_clk_disable(struct clk *clk)
{
	__raw_writel(0, clk->reg_data[SOURCE][CONTROL].reg);
}

static long sdhc_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/*
	 * SDH has four clock source: PLL1, PLL1/2, PLL1/4 and PLL2.
	 * Here PLL1/2 is not used since PLL1 and PLL1/4 can cover it.
	 * Only use PLL2 as clock source if the rate is larger than PLL1.
	 */
	int i;
	unsigned long parent_rate;

	/* for those which is less than pll1/4, use pll1/4 as clock source */
	if (rate <= clk_get_rate(&mmp3_clk_pll1_d_4)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1_d_4);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}

		return parent_rate / (i - 1);
	/* else for those which is less than pll1, use pll1 as clock source */
	} else if (rate <= clk_get_rate(&mmp3_clk_pll1)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}

		return parent_rate / (i - 1);
	/* else for those which is larger than pll1, use pll2 as clock source */
	} else {
		parent_rate = clk_get_rate(&mmp3_clk_pll2);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}
	}
		return parent_rate / (i - 1);
}

static int sdhc_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate;
	const struct clk_mux_sel *sel;
	u32 val = 0;

	if (rate <= clk_get_rate(&mmp3_clk_pll1_d_4)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1_d_4);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll1_d_4);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll1_d_4)
				break;
		}
		if (sel->input == 0) {
			pr_err("sdh: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	} else if (rate <= clk_get_rate(&mmp3_clk_pll1)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll1);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll1)
				break;
		}
		if (sel->input == 0) {
			pr_err("sdh: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	} else if (rate <= clk_get_rate(&mmp3_clk_pll2)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll2);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll2);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll2)
				break;
		}
		if (sel->input == 0) {
			pr_err("sdh: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	}

	clk->enable_val = val;

	return 0;
}

struct clkops sdhc_clk_ops = {
	.init = sdhc_clk_init,
	.enable = sdhc_clk_enable,
	.disable = sdhc_clk_disable,
	.round_rate = sdhc_clk_round_rate,
	.setrate = sdhc_clk_setrate,
};

static struct clk_mux_sel sdhc_clk_mux[] = {
	{.input = &mmp3_clk_pll1_d_4, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 1},
	{.input = &mmp3_clk_pll1, .value = 3},
	{0, 0},
};

static struct clk mmp3_clk_sdh0 = {
	.name = "sdh0",
	.lookup = {
		.dev_id = "sdhci-pxav3.0",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.reg_data = {
		     { {APMU_SDH0, 8, 0x3},
			{APMU_SDH0, 8, 0x3} },
		     {{APMU_SDH0, 10, 0xf},
			{APMU_SDH0, 10, 0xf} } },
};

static struct clk mmp3_clk_sdh1 = {
	.name = "sdh1",
	.lookup = {
		.dev_id = "sdhci-pxav3.1",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.reg_data = {
		     { {APMU_SDH1, 8, 0x3},
			{APMU_SDH1, 8, 0x3} },
		     {{APMU_SDH1, 10, 0xf},
			{APMU_SDH1, 10, 0xf} } },
};

static struct clk mmp3_clk_sdh2 = {
	.name = "sdh2",
	.lookup = {
		.dev_id = "sdhci-pxav3.2",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.reg_data = {
		     { {APMU_SDH2, 8, 0x3},
			{APMU_SDH2, 8, 0x3} },
		     {{APMU_SDH2, 10, 0xf},
			{APMU_SDH2, 10, 0xf} } },
};

static struct clk mmp3_clk_sdh3 = {
	.name = "sdh3",
	.lookup = {
		.dev_id = "sdhci-pxav3.3",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.reg_data = {
		     { {APMU_SDH3, 8, 0x3},
			{APMU_SDH3, 8, 0x3} },
		     {{APMU_SDH3, 10, 0xf},
			{APMU_SDH3, 10, 0xf} } },
};

#ifdef CONFIG_MMP3_HSI

#define HSI_PLL1_DIV_2		0
#define HSI_PLL1			1
#define HSI_PLL2			2

static void hsi_clk_init(struct clk *clk)
{
	/* HSI Clock default value is PLL1/2: 400MHz */
	clk->rate = clk_get_rate(&mmp3_clk_pll1)/2; /* 400MHz */
	clk->enable_val = HSI_PLL1;
	clk->div = 2;
	clk->mul = 1;
	clk_reparent(clk, &mmp3_clk_pll1);
}

static int hsi_clk_enable(struct clk *clk)
{
	int reg;

	clk_reparent(clk, clk->inputs[clk->enable_val].input);

	/* Configure HSI Controller Clock */
	reg = readl(MPMU_HSI_CLK_RES_CTRL);
	reg &= ~0x3F;
	reg |= (clk->enable_val);
	reg |= (clk->div) << 2;
	writel(reg, MPMU_HSI_CLK_RES_CTRL);

	/* Release HSI Controller Reset */
	reg = readl(MPMU_HSI_CLK_RES_CTRL);
	reg |= 0x1 << 7;
	reg |= 0x1 << 6;
	writel(reg, MPMU_HSI_CLK_RES_CTRL);

	/* enable APMU HSI bus and release HSI reset */
	reg = readl(APMU_BUS);
	reg |= 3 << 14;
	writel(reg, APMU_BUS);

	mdelay(10);

	return 0;
}

static void hsi_clk_disable(struct clk *clk)
{
	int reg;

	/* Reset HSI Controller */
	reg = readl(MPMU_HSI_CLK_RES_CTRL);
	reg &= ~(0x1 << 7);
	reg &= ~(0x1 << 6);
	writel(reg, MPMU_HSI_CLK_RES_CTRL);
}

static long hsi_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (rate <= clk_get_rate(&mmp3_clk_pll1)/4)
		return clk_get_rate(&mmp3_clk_pll1)/4; /* 200M */
	else
		return clk_get_rate(&mmp3_clk_pll1)/2; /* 400M */
}

static int hsi_clk_setrate(struct clk *clk, unsigned long rate)
{
	clk->mul = 1;
	if (rate == clk_get_rate(&mmp3_clk_pll1)/4) {
		clk->enable_val = HSI_PLL1_DIV_2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)/2) {
		clk->enable_val = HSI_PLL1;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else {
		pr_err("%s: unexpected hsi clock rate %ld\n",
			__func__, rate);
		BUG();
	}

	return 0;
}

struct clkops hsi_clk_ops = {
	.init		= hsi_clk_init,
	.enable		= hsi_clk_enable,
	.disable	= hsi_clk_disable,
	.setrate	= hsi_clk_setrate,
	.round_rate	= hsi_clk_round_rate,
};

static struct clk_mux_sel hsi_mux_pll1_pll2[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 1},
	{0, 0},
};

static struct clk mmp3_clk_hsi = {
	.name = "hsiclk",
	.inputs = hsi_mux_pll1_pll2,
	.lookup = {
		.con_id = "hsi-clk",
	},
	.clk_rst = (void __iomem *)MPMU_HSI_CLK_RES_CTRL,
	.ops = &hsi_clk_ops,
};
#endif
#ifdef CONFIG_VIDEO_MVISP

#define DXOISP_PLL1		1
#define DXOISP_PLL2		2

int mmp3_isp_reset_hw(void *param)
{
	int reg;

	param = param;

	reg = readl(APMU_ISPCLK);
	reg &= ~(0x1 << 1);
	writel(reg, APMU_ISPCLK);
	mdelay(20);
	reg |= 0x1 << 1;
	writel(reg, APMU_ISPCLK);

	return 0;
}

static void dxoisp_clk_init(struct clk *clk)
{
	clk->rate = clk_get_rate(&mmp3_clk_pll1)/2; /* 400MHz */
	clk->enable_val = DXOISP_PLL1;
	clk->div = 2;
	clk->mul = 1;
	clk_reparent(clk, &mmp3_clk_pll1);
}

static int dxoisp_clk_enable(struct clk *clk)
{
	int reg;

	/*clock source and clock divider */
	reg = readl(APMU_ISPCLK);
	reg &= ~0xF00;
	reg |= (clk->div) << 8;
	writel(reg, APMU_ISPCLK);
	reg &= ~0xC0;
	reg |= (clk->enable_val) << 6;
	writel(reg, APMU_ISPCLK);

	/*enable ISP AXI clock*/
	reg |= 0x1 << 3;
	writel(reg, APMU_ISPCLK);

	/*enable ISP clk*/
	reg |= 0x1 << 4;
	writel(reg, APMU_ISPCLK);

	/*enable CCIC AXI Arbiter clock*/
	reg = readl(APMU_CCIC_RST);
	reg |= 0x1 << 15;
	writel(reg, APMU_CCIC_RST);

	return 0;
}

static void dxoisp_clk_disable(struct clk *clk)
{
	int reg;

	/*disable ccic AXI Arbiter Clock*/
	reg = readl(APMU_CCIC_RST);
	reg &= ~(0x1 << 15);
	writel(reg, APMU_CCIC_RST);

	/*Disable ISP AXI clock*/
	reg = readl(APMU_ISPCLK);
	reg &= ~(0x1 << 4);
	writel(reg, APMU_ISPCLK);

	/*Disable ISP clock*/
	reg &= ~(0x1 << 3);
	writel(reg, APMU_ISPCLK);
}

static long dxoisp_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (rate <= clk_get_rate(&mmp3_clk_pll1)/4)
		return clk_get_rate(&mmp3_clk_pll1)/4; /* 200M */
	else
		return clk_get_rate(&mmp3_clk_pll1)/2; /* 400M */
}

static int dxoisp_clk_setrate(struct clk *clk, unsigned long rate)
{
	clk->mul = 1;
	if (rate == clk_get_rate(&mmp3_clk_pll1)/4) {
		clk->enable_val = DXOISP_PLL1;
		clk->div = 4;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)/2) {
		clk->enable_val = DXOISP_PLL1;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else {
		pr_err("%s: unexpected dxoisp clock rate %ld\n",
			__func__, rate);
		BUG();
	}

	return 0;
}

struct clkops dxoisp_clk_ops = {
	.init		= dxoisp_clk_init,
	.enable		= dxoisp_clk_enable,
	.disable	= dxoisp_clk_disable,
	.setrate	= dxoisp_clk_setrate,
	.round_rate	= dxoisp_clk_round_rate,
};

static struct clk_mux_sel dxoisp_mux_pll1_pll2[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 1},
	{0, 0},
};

static struct clk mmp3_clk_dxoisp = {
	.name = "dxoisp",
	.inputs = dxoisp_mux_pll1_pll2,
	.lookup = {
		.con_id = "ISP-CLK",
	},
	.clk_rst = (void __iomem *)APMU_ISPCLK,
	.ops = &dxoisp_clk_ops,
};

#define DXOCCIC_PLL1_DIV2		0

static void dxoccic_clk_init(struct clk *clk)
{
	clk->rate = clk_get_rate(&mmp3_clk_pll1)/2; /* 400MHz */
	clk->enable_val = DXOCCIC_PLL1_DIV2;
	clk->div = 1;
	clk->mul = 1;
	clk_reparent(clk, &mmp3_clk_pll1);
}

static int dxoccic_clk_enable(struct clk *clk)
{
	int reg;

	reg = readl(clk->clk_rst);

	/* Select PLL1/2 as CCIC clock source */
	reg &= ~(0x3 << 6);
	reg |= (clk->enable_val) << 6;
	/* Select CCIC Clock Divider to 1 */
	reg |= (clk->div) << 17;
	/* Select PHY SLOW Divider to 26 */
	reg |= (0x1A << 10);
	writel(reg, clk->clk_rst);

	/* Enable PHY SLOW clock */
	reg |= (0x1 << 9);
	/* Enable PHY clock */
	reg |= (0x1 << 5);
	/* Enable CCIC clock */
	reg |= (0x1 << 4);
	/* Enable AXI clock */
	reg |= (0x1 << 3);
	writel(reg, clk->clk_rst);

	/* Deassert RST for PHY SLOW clock */
	reg |= (0x1 << 8);
	/* Deassert RST for PHY clock */
	reg |= (0x1 << 2);
	/* Deassert RST for CCIC clock */
	reg |= (0x1 << 1);
	/* Deassert RST for AXI clock */
	reg |= (0x1 << 0);
	writel(reg, clk->clk_rst);


	reg = readl(APMU_CCIC_DBG);
	reg |= (1 << 25) | (1 << 27);
	writel(reg, APMU_CCIC_DBG);

	reg = 0xFFFF;
	writel(reg, APMU_CCIC_GATE);

	return 0;
}

static void dxoccic_clk_disable(struct clk *clk)
{
	int reg;

	reg = readl(APMU_CCIC_GATE);
	reg &= ~(0xFFFF);
	writel(reg, APMU_CCIC_GATE);

	reg = readl(APMU_CCIC_DBG);
	reg &= ~((1 << 25) | (1 << 27));
	writel(reg, APMU_CCIC_DBG);

	reg = readl(clk->clk_rst);
	/* Assert RST for PHY SLOW clock */
	reg &= ~(0x1 << 8);
	/* Assert RST for PHY clock */
	reg &= ~(0x1 << 2);
	/* Assert RST for CCIC clock */
	reg &= ~(0x1 << 1);
	/* Assert RST for AXI clock */
	reg &= ~(0x1 << 0);
	writel(reg, clk->clk_rst);

	reg = readl(clk->clk_rst);
	/* Disable PHY SLOW clock */
	reg &= ~(0x1 << 9);
	/* Disable PHY clock */
	reg &= ~(0x1 << 5);
	/* Disable CCIC clock */
	reg &= ~(0x1 << 4);
	/* Disable AXI clock */
	reg &= ~(0x1 << 3);
	writel(reg, clk->clk_rst);
}

static long dxoccic_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk_get_rate(&mmp3_clk_pll1)/2; /* 400M */
}

static int dxoccic_clk_setrate(struct clk *clk, unsigned long rate)
{
	clk->mul = 1;
	if (rate == clk_get_rate(&mmp3_clk_pll1)/2) {
		clk->enable_val = DXOCCIC_PLL1_DIV2;
		clk->div = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else {
		pr_err("%s: unexpected dxoccic clock rate %ld\n",
			__func__, rate);
		BUG();
	}

	return 0;
}

struct clkops dxoccic_clk_ops = {
	.init		= dxoccic_clk_init,
	.enable		= dxoccic_clk_enable,
	.disable	= dxoccic_clk_disable,
	.setrate	= dxoccic_clk_setrate,
	.round_rate	= dxoccic_clk_round_rate,
};

static struct clk_mux_sel dxoccic_mux_pll1_pll2[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{0, 0},
};

static struct clk mmp3_clk_dxoccic = {
	.name = "dxoccic",
	.inputs = dxoccic_mux_pll1_pll2,
	.lookup = {
		.con_id = "CCIC-CLK",
	},
	.clk_rst = (void __iomem *)APMU_CCIC_RST,
	.ops = &dxoccic_clk_ops,
};
#endif

#ifdef CONFIG_UIO_VMETA

#define VMETA_PLL1		0
#define VMETA_PLL2		1
#define VMETA_PLL1_OUTP		2
#define VMETA_PLL2_OUTP		3

static void vmeta_clk_init(struct clk *clk)
{
	clk->rate = clk_get_rate(&mmp3_clk_pll1_clkoutp) / 2;
	clk->enable_val = VMETA_PLL1_OUTP;
	clk->div = 2;
	clk->mul = 1;
	clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
}

static int vmeta_clk_enable(struct clk *clk)
{
	int reg;

	clk_reparent(clk, clk->inputs[clk->enable_val].input);

	reg = readl(clk->clk_rst);
	reg &= ~APMU_VMETA_CLK_SEL_MASK;
	reg &= ~APMU_VMETA_CLK_DIV_MASK;
	reg |= (clk->inputs[clk->enable_val].value) << APMU_VMETA_CLK_SEL_SHIFT;
	reg |= (clk->div) << APMU_VMETA_CLK_DIV_SHIFT;
	reg &= ~(3u << 11); /* vmeta BUS PLL1/2*/
	reg |= (2u << 11);
	writel(reg, clk->clk_rst);

	reg = readl(clk->clk_rst);
	reg |= (APMU_VMETA_AXICLK_EN | APMU_VMETA_CLK_EN);
	writel(reg, clk->clk_rst);

	return 0;
}

static void vmeta_clk_disable(struct clk *clk)
{
	int reg;

	reg = readl(clk->clk_rst);
	reg &= ~(APMU_VMETA_CLK_EN | APMU_VMETA_AXICLK_EN);
	writel(reg, clk->clk_rst);
}

static long vmeta_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (rate <= clk_get_rate(&mmp3_clk_pll1)/4)
		return clk_get_rate(&mmp3_clk_pll1)/4; /* 200M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1_clkoutp)/4)
		return clk_get_rate(&mmp3_clk_pll1_clkoutp)/4; /* 266M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1)/2)
		return clk_get_rate(&mmp3_clk_pll1)/2; /* 400M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2_clkoutp)/2)
		return clk_get_rate(&mmp3_clk_pll2_clkoutp)/2; /* 480M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1_clkoutp)/2)
		return clk_get_rate(&mmp3_clk_pll1_clkoutp)/2; /* 533M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2)/2)
		return clk_get_rate(&mmp3_clk_pll2)/2; /* 600M */
	else
		return clk_get_rate(&mmp3_clk_pll1); /* 800M */
}

static int vmeta_clk_setrate(struct clk *clk, unsigned long rate)
{
	clk->mul = 1;
	if (rate == clk_get_rate(&mmp3_clk_pll1)/4) {
		clk->enable_val = VMETA_PLL1;
		clk->div = 4;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1_clkoutp)/4) {
		clk->enable_val = VMETA_PLL1_OUTP;
		clk->div = 4;
		clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)/2) {
		clk->enable_val = VMETA_PLL1;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2_clkoutp)/2) {
		clk->enable_val = VMETA_PLL2_OUTP;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll2_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1_clkoutp)/2) {
		clk->enable_val = VMETA_PLL1_OUTP;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll1_clkoutp);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2)/2) {
		clk->enable_val = VMETA_PLL2;
		clk->div = 2;
		clk_reparent(clk, &mmp3_clk_pll2);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1)) {
		clk->enable_val = VMETA_PLL1;
		clk->div = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else {
		pr_err("%s: unexpected vmeta clock rate %ld\n", __func__, rate);
		BUG();
	}

	return 0;
}

struct clkops vmeta_clk_ops = {
	.init		= vmeta_clk_init,
	.enable		= vmeta_clk_enable,
	.disable	= vmeta_clk_disable,
	.setrate	= vmeta_clk_setrate,
	.round_rate	= vmeta_clk_round_rate,
};

static struct clk_mux_sel vmeta_mux_pll1_pll2[] = {
	{.input = &mmp3_clk_pll1, .value = 0},
	{.input = &mmp3_clk_pll2, .value = 1},
	{.input = &mmp3_clk_pll1_clkoutp, .value = 2},
	{.input = &mmp3_clk_pll2_clkoutp, .value = 3},
	{0, 0},
};

static struct clk mmp3_clk_vmeta = {
	.name = "vmeta",
	.inputs = vmeta_mux_pll1_pll2,
	.lookup = {
		.con_id = "VMETA_CLK",
	},
	.clk_rst = (void __iomem *)APMU_VMETA_CLK_RES_CTRL,
	.ops = &vmeta_clk_ops,
};
#endif

static void ccic_clk_init(struct clk *clk)
{
	const struct clk_mux_sel *sel;
	u32 val = 0;

	/* by default select pll1/2 as clock source and divider 1 */
	clk->mul = 1;
	clk->div = 1;

	clk_reparent(clk, &mmp3_clk_pll1_d_2);

	val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
		 << clk->reg_data[DIV][CONTROL].reg_shift);
	val |= clk->div
	    << clk->reg_data[DIV][CONTROL].reg_shift;

	for (sel = clk->inputs; sel->input != 0; sel++) {
		if (sel->input == &mmp3_clk_pll1_d_2)
			break;
	}
	if (sel->input == 0) {
		pr_err("ccic: no matched input for this parent!\n");
		BUG();
	}

	val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
		<< clk->reg_data[SOURCE][CONTROL].reg_shift);
	val |= sel->value
		<< clk->reg_data[SOURCE][CONTROL].reg_shift;

	/* use fixed value 0x1a for CCIC_PHYSLOW_PRER */
	clk->enable_val = val | (0x1a << 10);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
}

static int ccic_clk_enable(struct clk *clk)
{
	u32 val;

	val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg) & 0x18000;
	val |= clk->enable_val;
	/* enable clocks */
	val |= 0x38;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	/* release reset */
	val |= 0x307;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	/* enable MIPI DPHY CSI2's DVDD and AVDD */
	val = __raw_readl(APMU_CCIC_DBG);
	if (strncmp(clk->name, "ccic1", 5) == 0)
		val |= (1 << 25) | (1 << 27);
	else
		val |= (1 << 26) | (1 << 28);
	__raw_writel(val, APMU_CCIC_DBG);

	return 0;
}

static void ccic_clk_disable(struct clk *clk)
{
	u32 val;

	val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);
	val &= 0x18000;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	/* disable MIPI DPHY CSI2's DVDD and AVDD */
	val = __raw_readl(APMU_CCIC_DBG);
	if (strncmp(clk->name, "ccic1", 5) == 0)
		val &= ~((1 << 25) | (1 << 27));
	else
		val &= ~((1 << 26) | (1 << 28));
	__raw_writel(val, APMU_CCIC_DBG);

}

static long ccic_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/*
	 * CCIC has four clock source: PLL1/2, PLL1/16, and VCTCXO.
	 * Here PLL1/16 is not used since PLL1/2 and VCTCXO can cover it.
	 * Only use PLL1/2 as clock source if the rate is larger than VCTCXO.
	 */
	int i;
	unsigned long parent_rate;

	/* for those which is less than vctcxo, use vctcxo as clock source */
	if (rate <= clk_get_rate(&mmp3_clk_vctcxo)) {
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}

		return parent_rate / (i - 1);
	/* else use pll1/2 as clock source */
	} else {
		parent_rate = clk_get_rate(&mmp3_clk_pll1_d_2);
		for (i = 2; i < 16; i++) {
			if (rate > parent_rate / i)
				break;
		}
	}
		return parent_rate / (i - 1);
}

static int ccic_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate;
	const struct clk_mux_sel *sel;
	u32 val = 0;

	if (rate <= clk_get_rate(&mmp3_clk_vctcxo)) {
		parent_rate = clk_get_rate(&mmp3_clk_vctcxo);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_vctcxo);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_vctcxo)
				break;
		}
		if (sel->input == 0) {
			pr_err("ccic: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	} else if (rate <= clk_get_rate(&mmp3_clk_pll1_d_2)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1_d_2);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &mmp3_clk_pll1_d_2);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &mmp3_clk_pll2)
				break;
		}
		if (sel->input == 0) {
			pr_err("ccic: no matched input for this parent!\n");
			BUG();
		}

		val &= ~(clk->reg_data[SOURCE][CONTROL].reg_mask
			<< clk->reg_data[SOURCE][CONTROL].reg_shift);
		val |= sel->value
			<< clk->reg_data[SOURCE][CONTROL].reg_shift;
	}

	/* use fixed value 0x1a for CCIC_PHYSLOW_PRER */
	clk->enable_val = val | (0x1a << 10);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	return 0;
}


struct clkops ccic_clk_ops = {
	.init = ccic_clk_init,
	.enable = ccic_clk_enable,
	.disable = ccic_clk_disable,
	.round_rate = ccic_clk_round_rate,
	.setrate = ccic_clk_setrate,
};

static struct clk_mux_sel ccic_clk_mux[] = {
	{.input = &mmp3_clk_pll1_d_2, .value = 0},
	{.input = &mmp3_clk_vctcxo, .value = 2},
	{0, 0},
};

static int ccic_share_clk_enable(struct clk *clk)
{
	u32 val;

	/* ccic axi enable clocks */
	val = 0x10000;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	/* ccic axi clock release reset */
	val |= 0x8000;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);

	return 0;
}

static void ccic_share_clk_disable(struct clk *clk)
{
	u32 val;

	val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);
	val &= ~0x18000;
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
}

struct clkops ccic_share_clk_ops = {
	.init = NULL,
	.enable = ccic_share_clk_enable,
	.disable = ccic_share_clk_disable,
	.round_rate = NULL,
	.setrate = NULL,
};

static struct clk mmp3_clk_ccic0 = {
	.name = "ccic0",
	.ops = &ccic_share_clk_ops,
	.inputs = ccic_clk_mux,
	.reg_data = {
		     { {APMU_CCIC_RST, 6, 0x3},
			{APMU_CCIC_RST, 6, 0x3} },
		     {{APMU_CCIC_RST, 17, 0xf},
			{APMU_CCIC_RST, 17, 0xf} } },
};

static struct clk *ccic_depend_clk[] = {
	&mmp3_clk_ccic0,
};

static struct clk mmp3_clk_ccic1 = {
	.name = "ccic1",
	.lookup = {
		.dev_id = "mmp-camera.0",
		.con_id = "CCICRSTCLK",
	},
	.dependence = ccic_depend_clk,
	.dependence_count = ARRAY_SIZE(ccic_depend_clk),
	.ops = &ccic_clk_ops,
	.inputs = ccic_clk_mux,
	.reg_data = {
		     { {APMU_CCIC_RST, 6, 0x3},
			{APMU_CCIC_RST, 6, 0x3} },
		     {{APMU_CCIC_RST, 17, 0xf},
			{APMU_CCIC_RST, 17, 0xf} } },
};

static struct clk mmp3_clk_ccic2 = {
	.name = "ccic2",
	.lookup = {
		.dev_id = "mmp-camera.1",
		.con_id = "CCICRSTCLK",
	},
	.dependence = ccic_depend_clk,
	.dependence_count = ARRAY_SIZE(ccic_depend_clk),
	.ops = &ccic_clk_ops,
	.inputs = ccic_clk_mux,
	.reg_data = {
		     { {APMU_CCIC2_RST, 6, 0x3},
			{APMU_CCIC2_RST, 6, 0x3} },
		     {{APMU_CCIC2_RST, 17, 0xf},
			{APMU_CCIC2_RST, 17, 0xf} } },
};

static struct clk *mmp3_clks_ptr[] = {
	&mmp3_clk_pll1_d_2,
	&mmp3_clk_pll1,
	&mmp3_clk_pll2,
	&mmp3_clk_pll1_clkoutp,
	&mmp3_clk_pll2_clkoutp,
	&mmp3_clk_vctcxo,
	&mmp3_clk_32k,
	&mmp3_clk_core_root,
	&mmp3_clk_virtual_pj,
	&mmp3_clk_mp1,
	&mmp3_clk_mp2,
	&mmp3_clk_mm,
	&mmp3_clk_cpu,
	&mmp3_clk_aclk,
	&mmp3_clk_core_periph,
	&mmp3_clk_atclk,
	&mmp3_clk_ddr_root,
	&mmp3_clk_ddr1,
	&mmp3_clk_ddr2,
	&mmp3_clk_axi_root,
	&mmp3_clk_axi1,
	&mmp3_clk_axi2,
	&mmp3_clk_pll1_d_4,
	&mmp3_clk_gc,
	&mmp3_clk_disp1,
	&mmp3_clk_lcd1,
	&mmp3_clk_sdh0,
	&mmp3_clk_sdh1,
	&mmp3_clk_sdh2,
	&mmp3_clk_sdh3,
	&mmp3_clk_disp1_axi,
	&mmp3_clk_hdmi,
	&mmp3_clk_thermal,
	&mmp3_clk_ccic1,
	&mmp3_clk_ccic2,
#ifdef CONFIG_UIO_VMETA
	&mmp3_clk_vmeta,
#endif
#ifdef CONFIG_VIDEO_MVISP
	&mmp3_clk_dxoisp,
	&mmp3_clk_dxoccic,
#endif
#ifdef CONFIG_MMP3_HSI
	&mmp3_clk_hsi,
#endif
};

static int apbc_clk_enable(struct clk *clk)
{
	unsigned long data;

	data = __raw_readl(clk->clk_rst) & ~(APBC_FNCLKSEL(7));
	data |= APBC_FNCLK | APBC_FNCLKSEL(clk->fnclksel);
	__raw_writel(data, clk->clk_rst);
	/*
	 * delay two cycles of the solwest clock between the APB bus clock
	 * and the functional module clock.
	 */
	udelay(10);

	data |= APBC_APBCLK;
	__raw_writel(data, clk->clk_rst);
	udelay(10);

	data &= ~APBC_RST;
	__raw_writel(data, clk->clk_rst);

	return 0;
}

static void apbc_clk_disable(struct clk *clk)
{
	unsigned long data;

	data = __raw_readl(clk->clk_rst) & ~(APBC_FNCLK | APBC_FNCLKSEL(7));
	__raw_writel(data, clk->clk_rst);
	udelay(10);

	data &= ~APBC_APBCLK;
	__raw_writel(data, clk->clk_rst);
}

struct clkops apbc_clk_ops = {
	.enable = apbc_clk_enable,
	.disable = apbc_clk_disable,
};

#define APBC_CLK(_name, _dev, _con, _reg, _fnclksel, _rate, _parent)\
{							\
	.name = _name,					\
	.lookup = {					\
		.dev_id = _dev,				\
		.con_id = _con,				\
	},						\
	.clk_rst = (void __iomem *)APBC_##_reg,		\
	.fnclksel = _fnclksel,				\
	.rate = _rate,					\
	.ops = &apbc_clk_ops,				\
	.parent = _parent,				\
}

#define APBC_CLK_OPS(_name, _dev, _con, _reg, _fnclksel, _rate, _parent, _ops)\
{							\
	.name = _name,					\
	.lookup = {					\
		.dev_id = _dev,				\
		.con_id = _con,				\
	},						\
	.clk_rst = (void __iomem *)APBC_##_reg,		\
	.fnclksel = _fnclksel,				\
	.rate = _rate,					\
	.ops = _ops,					\
	.parent = _parent,				\
}

#define AUD_CLK_OPS(_name, _dev, _con, _ops)				\
{							\
	.name = _name,					\
	.lookup = {					\
		.dev_id = _dev,				\
		.con_id = _con,				\
	},						\
	.ops = _ops,					\
	.dynamic_change = 1,			\
}

static void uart_clk_init(struct clk *clk)
{
	clk->mul = clk->div = 1;
	/*
	 * Bit(s) PMUM_SUCCR_RSRV_31_29 reserved
	 * UART Clock Generation Programmable Divider
	 * Numerator Value
	 */
	clk->reg_data[DIV][CONTROL].reg = MPMU_SUCCR;
	clk->reg_data[DIV][CONTROL].reg_shift = 16;
	clk->reg_data[DIV][CONTROL].reg_mask = 0x1fff;
	/*
	 * Bit(s) PMUM_SUCCR_RSRV_15_13 reserved
	 * UART Clock Generation Programmable Divider
	 * Denominator Value
	 */
	clk->reg_data[MUL][CONTROL].reg = MPMU_SUCCR;
	clk->reg_data[MUL][CONTROL].reg_shift = 0;
	clk->reg_data[MUL][CONTROL].reg_mask = 0x1fff;
}

static int uart_clk_enable(struct clk *clk)
{
	uint32_t clk_rst;

	clk_rst = __raw_readl(clk->clk_rst);
	clk_rst |= APBC_FNCLK;
	__raw_writel(clk_rst, clk->clk_rst);
	mdelay(1);

	clk_rst |= APBC_APBCLK;
	__raw_writel(clk_rst, clk->clk_rst);
	mdelay(1);

	clk_rst &= ~(APBC_RST);
	__raw_writel(clk_rst, clk->clk_rst);

	if (clk->rate == clk_get_rate(&mmp3_clk_vctcxo)) {
		/* choose vctcxo */
		clk_rst = __raw_readl(clk->clk_rst);
		clk_rst &= ~(APBC_FNCLKSEL(0x7));
		clk_rst |= APBC_FNCLKSEL(0x1);
		__raw_writel(clk_rst, clk->clk_rst);
	} else {
		/* choose programmable clk */
		clk_rst = __raw_readl(clk->clk_rst);
		clk_rst &= ~(APBC_FNCLKSEL(0x7));
		__raw_writel(clk_rst, clk->clk_rst);
	}

	return 0;
}

static void uart_clk_disable(struct clk *clk)
{
	__raw_writel(0, clk->clk_rst);
	mdelay(1);
}

static long uart_clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate;

	if (rate >= clk_get_rate(&mmp3_clk_vctcxo)) {
		parent_rate = clk_get_rate(&mmp3_clk_pll1_d_4);
		return parent_rate * 16 / 27 / 2;
	} else
		return clk_get_rate(&mmp3_clk_vctcxo);
}

static int uart_clk_setrate(struct clk *clk, unsigned long val)
{
	uint32_t clk_rst;

	if (val == clk_get_rate(&mmp3_clk_vctcxo)) {
		/* choose vctcxo */
		clk_rst = __raw_readl(clk->clk_rst);
		clk_rst &= ~(APBC_FNCLKSEL(0x7));
		clk_rst |= APBC_FNCLKSEL(0x1);
		__raw_writel(clk_rst, clk->clk_rst);

		clk->div = clk->mul = 1;

		clk_reparent(clk, &mmp3_clk_vctcxo);
	} else {
		/* set m/n for high speed */
		unsigned int numer = 27;
		unsigned int denom = 16;

		/*
		 * n/d = base_clk/(2*out_clk)
		 * base_clk = 199.33M, out_clk=199.33*16/27/2=59.06M
		 * buadrate = clk/(16*divisor)
		 */

		clk_rst = __raw_readl(clk->reg_data[DIV][CONTROL].reg);
		clk_rst &= ~(clk->reg_data[DIV][CONTROL].reg_mask <<
				clk->reg_data[DIV][CONTROL].reg_shift);
		clk_rst |= numer << clk->reg_data[DIV][CONTROL].reg_shift;
		clk_rst &= ~(clk->reg_data[MUL][CONTROL].reg_mask <<
				clk->reg_data[MUL][CONTROL].reg_shift);
		clk_rst |= denom << clk->reg_data[MUL][CONTROL].reg_shift;
		__raw_writel(clk_rst, clk->reg_data[DIV][CONTROL].reg);

		/* choose programmable clk */
		clk_rst = __raw_readl(clk->clk_rst);
		clk_rst &= ~(APBC_FNCLKSEL(0x7));
		__raw_writel(clk_rst, clk->clk_rst);

		clk->div = numer * 2;
		clk->mul = denom;

		clk_reparent(clk, &mmp3_clk_pll1_d_4);
	}

	return 0;
}

struct clkops uart_clk_ops = {
	.init = uart_clk_init,
	.enable = uart_clk_enable,
	.disable = uart_clk_disable,
	.round_rate = uart_clk_round_rate,
	.setrate = uart_clk_setrate,
};

static void nand_clk_init(struct clk *clk)
{
	clk->div = 8;
	clk->mul = 1;

	/* by default select pll1 as clock source and divider 8 */
	clk->enable_val = 0x80;
}

static int nand_clk_enable(struct clk *clk)
{
	__raw_writel(clk->enable_val | 0x2d, clk->clk_rst);

	return 0;
}

static void nand_clk_disable(struct clk *clk)
{
	__raw_writel(0x0, clk->clk_rst);
}

static long nand_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (rate <= clk_get_rate(&mmp3_clk_vctcxo) / 2)
		return clk_get_rate(&mmp3_clk_vctcxo) / 2; /* 13M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1) / 12)
		return clk_get_rate(&mmp3_clk_pll1) / 12; /* 67M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1) / 8)
		return clk_get_rate(&mmp3_clk_pll1) / 8; /* 100M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2) / 12)
		return clk_get_rate(&mmp3_clk_pll2) / 12; /* 111M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1) / 6)
		return clk_get_rate(&mmp3_clk_pll1) / 6; /* 133M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll2) / 8)
		return clk_get_rate(&mmp3_clk_pll2) / 8; /* 167M */
	else if (rate <= clk_get_rate(&mmp3_clk_pll1) / 4)
		return clk_get_rate(&mmp3_clk_pll1) / 4; /* 200M */
	else
		return clk_get_rate(&mmp3_clk_pll2) / 6; /* 222M */
}

static int nand_clk_setrate(struct clk *clk, unsigned long rate)
{
	if (rate == clk_get_rate(&mmp3_clk_vctcxo) / 2) {
		clk->enable_val = 0x1c0;
		clk->div = 2;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_vctcxo);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1) / 12) {
		clk->enable_val = 0xc0;
		clk->div = 12;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1) / 8) {
		clk->enable_val = 0x80;
		clk->div = 8;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2) / 12) {
		clk->enable_val = 0x180;
		clk->div = 12;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll2);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1) / 6) {
		clk->enable_val = 0x40;
		clk->div = 6;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2) / 8) {
		clk->enable_val = 0x140;
		clk->div = 8;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll2);
	} else if (rate == clk_get_rate(&mmp3_clk_pll1) / 4) {
		clk->enable_val = 0x0;
		clk->div = 4;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll1);
	} else if (rate == clk_get_rate(&mmp3_clk_pll2) / 6) {
		clk->enable_val = 0x100;
		clk->div = 6;
		clk->mul = 1;
		clk_reparent(clk, &mmp3_clk_pll2);
	} else {
		pr_err("%s: unexpected clock rate %ld\n", __func__, rate);
		BUG();
	}

	return 0;
}

struct clkops nand_clk_ops = {
	.init = nand_clk_init,
	.enable = nand_clk_enable,
	.disable = nand_clk_disable,
	.round_rate = nand_clk_round_rate,
	.setrate = nand_clk_setrate,
};

static int apmu_clk_enable(struct clk *clk)
{
	__raw_writel(clk->enable_val, clk->clk_rst);

	return 0;
}

static void apmu_clk_disable(struct clk *clk)
{
	__raw_writel(0, clk->clk_rst);
}

static int apmu_clk_setrate(struct clk *clk, unsigned long rate)
{
	__raw_writel(rate, clk->clk_rst);

	return 0;
}

struct clkops apmu_clk_ops = {
	.enable = apmu_clk_enable,
	.disable = apmu_clk_disable,
	.setrate = apmu_clk_setrate,
};

#define APMU_CLK(_name, _dev, _con, _reg, _eval, _rate, _parent)\
{								\
	.name = _name,						\
	.lookup = {						\
		.dev_id = _dev,					\
		.con_id = _con,					\
	},							\
	.clk_rst = (void __iomem *)APMU_##_reg,			\
	.enable_val = _eval,					\
	.rate = _rate,						\
	.ops = &apmu_clk_ops,					\
	.parent = _parent,					\
}

#define APMU_CLK_OPS(_name, _dev, _con, _reg, _eval, _rate, _parent, _ops)\
{								\
	.name = _name,						\
	.lookup = {						\
		.dev_id = _dev,					\
		.con_id = _con,					\
	},							\
	.clk_rst = (void __iomem *)APMU_##_reg,			\
	.enable_val = _eval,					\
	.rate = _rate,						\
	.parent = _parent,					\
	.ops = _ops,						\
}


/* usb: hsic clock */
static int hsic_clk_enable(struct clk *clk)
{
	uint32_t clk_rst;

	clk_rst  =  __raw_readl(clk->clk_rst);
	clk_rst |= 0x1b;
	__raw_writel(clk_rst, clk->clk_rst);

	return 0;
}

static void hsic_clk_disable(struct clk *clk)
{
	uint32_t clk_rst;

	clk_rst  =  __raw_readl(clk->clk_rst);
	clk_rst &= ~0x18;
	__raw_writel(clk_rst, clk->clk_rst);
}

struct clkops hsic_clk_ops = {
	.enable		= hsic_clk_enable,
	.disable	= hsic_clk_disable,
};

static int pwm_clk_enable(struct clk *clk)
{
	struct clk *clk_apb = NULL, *clk_share = NULL;
	unsigned long data;

	data = __raw_readl(clk->clk_rst) & ~(APBC_FNCLKSEL(7));
	data |= APBC_FNCLK | APBC_FNCLKSEL(clk->fnclksel);
	__raw_writel(data, clk->clk_rst);
	/*
	 * delay two cycles of the solwest clock between the APB bus clock
	 * and the functional module clock.
	 */
	udelay(10);

	/*
	 * A dependence exists between pwm1 and pwm2.pwm1 can controll its
	 * apb bus clk independently, while pwm2 apb bus clk is controlled
	 * by pwm1's. The same relationship exists between pwm3 and pwm4.
	 */
	if (!strcmp(clk->name, "pwm1")) {
		clk_share = clk_get_sys("mmp2-pwm.1", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm2")) {
		clk_share = clk_get_sys("mmp2-pwm.0", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	} else if (!strcmp(clk->name, "pwm3")) {
		clk_share = clk_get_sys("mmp2-pwm.3", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm4")) {
		clk_share = clk_get_sys("mmp2-pwm.2", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	}

	if ((clk->refcnt + clk_share->refcnt) == 0) {
		data = __raw_readl(clk_apb->clk_rst);
		data |= APBC_APBCLK;
		__raw_writel(data, clk_apb->clk_rst);
		udelay(10);
		data = __raw_readl(clk->clk_rst);
		data &= ~APBC_RST;
		__raw_writel(data, clk->clk_rst);
		if (strcmp(clk->name, clk_apb->name)) {
			data = __raw_readl(clk_apb->clk_rst);
			data &= ~APBC_RST;
			__raw_writel(data, clk_apb->clk_rst);
		}
	}

	return 0;
}

static void pwm_clk_disable(struct clk *clk)
{
	struct clk *clk_apb = NULL, *clk_share = NULL;
	unsigned long data;

	data = __raw_readl(clk->clk_rst) & ~(APBC_FNCLK | APBC_FNCLKSEL(7));
	__raw_writel(data, clk->clk_rst);
	udelay(10);

	if (!strcmp(clk->name, "pwm1")) {
		clk_share = clk_get_sys("mmp2-pwm.1", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm2")) {
		clk_share = clk_get_sys("mmp2-pwm.0", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	} else if (!strcmp(clk->name, "pwm3")) {
		clk_share = clk_get_sys("mmp2-pwm.3", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm4")) {
		clk_share = clk_get_sys("mmp2-pwm.2", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	}

	if ((clk->refcnt + clk_share->refcnt) == 1) {
		data = __raw_readl(clk_apb->clk_rst);
		data &= ~APBC_APBCLK;
		__raw_writel(data, clk_apb->clk_rst);
	}
}

struct clkops pwm_clk_ops = {
	.enable = pwm_clk_enable,
	.disable = pwm_clk_disable,
};

static unsigned long aclk_rate = (-1);
#define MMP_VCXO_VAL	26000000

/* audio pll -> audio clk */
static struct aclk_set ac_set_135mhz[] = {
/*   aclk    pdiv    o_m    o_p  *
 * --------- -----  -----  ----- */
{ 135475200,   1,     0,     0 },
{  33868800,   4,     0,     0 },
{  22579200,   6,     1,     1 },
{  16934400,   8,     1,     0 },
{  11289600,  12,     2,     1 },
{   8467200,  16,     2,     0 },
{   5644800,  24,     4,     1 },
};

static struct aclk_set ac_set_147mhz[] = {
/*   aclk    pdiv    o_m    o_p  *
 * --------- -----  -----  ----- */
{ 147456000,   1,     0,     0 },
{  36864000,   4,     0,     0 },
{  24576000,   6,     1,     1 },
{  18432000,   8,     1,     0 },
{  16384000,   9,     1,     2 },
{  12288000,  12,     2,     1 },
{   9216000,  16,     2,     0 },
{   8192000,  18,     2,     2 },
{   6144000,  24,     4,     1 },
{   4096000,  36,     4,     2 },
{   3072000,  48,     6,     1 },
{   2048000,  72,     6,     2 },
};

/* vcxo -> audio pll */
static struct apll_set ap_set[] = {
/*   vcxo    mclk  fbc    fra       aclk_set                    *
 * --------  ----  ----  -------  ----------------------------- */
{ 26000000,   0,    0x1f,   0x8a18,  ARRAY_AND_SIZE(ac_set_135mhz) },
{ 26000000,   0,    0x22,   0x0da1,  ARRAY_AND_SIZE(ac_set_147mhz) },
{ 38400000,   1,    0x23,   0x8208,  ARRAY_AND_SIZE(ac_set_135mhz) },
{ 38400000,   1,    0x26,   0xaaab,  ARRAY_AND_SIZE(ac_set_147mhz) },
};

static int audio_clk_enable(struct clk *clk)
{
	unsigned int val = 0;

	/*
	 * must set pwr_up bit firstly; otherwise
	 * after enter suspend state, then cannot
	 * resume back properly.
	 */
	val = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	val |= APMU_AUDIO_PWR_UP;
	__raw_writel(val, APMU_AUDIO_CLK_RES_CTRL);

	val |= APMU_AUDIO_RST_DIS | APMU_AUDIO_ISO_DIS |
		APMU_AUDIO_CLK_ENA;
	__raw_writel(val, APMU_AUDIO_CLK_RES_CTRL);

	return 0;
}

static void audio_clk_disable(struct clk *clk)
{
	unsigned int val;

	val = __raw_readl(APMU_AUDIO_CLK_RES_CTRL);
	val &= ~(APMU_AUDIO_RST_DIS | APMU_AUDIO_ISO_DIS |
		 APMU_AUDIO_CLK_ENA);
	__raw_writel(val, APMU_AUDIO_CLK_RES_CTRL);

	val &= ~APMU_AUDIO_PWR_UP;
	__raw_writel(val, APMU_AUDIO_CLK_RES_CTRL);
}

static int audio_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i, j;
	struct apll_set *ap_s;
	struct aclk_set *ac_s;
	unsigned long val;

	pr_debug("%s: rate %ld\n", __func__, rate);

	for (i = 0; i < ARRAY_SIZE(ap_set); i++) {

		/* find audio pll setting */
		ap_s = &ap_set[i];
		if (ap_s->vcxo != MMP_VCXO_VAL)
			continue;

		/* find audio clk setting */
		for (j = 0; j < ap_s->ac_num; j++) {
			ac_s = &ap_s->ac_set[j];
			pr_debug("%s: aclk %d\n", __func__, ac_s->aclk);
			if (ac_s->aclk == rate)
				goto found;
		}
	}

	/* not found setting for the rate */
	return -EINVAL;

found:
	val = AUD_PLL_CTL0_DIV_MCLK1((ac_s->postdiv & 0x2) >> 1) |
	      AUD_PLL_CTL0_DIV_OCLK_MODULO(ac_s->oclk_modulo) |
	      AUD_PLL_CTL0_FRACT(ap_s->fract) |
	      AUD_PLL_CTL0_ENA_DITHER |
	      AUD_PLL_CTL0_DIV_FBCCLK0_1(ap_s->fbcclk & 0x3) |
	      AUD_PLL_CTL0_DIV_MCLK(ap_s->mclk) |
	      AUD_PLL_CTL0_PU;
	__raw_writel(val, AUD_PLL_CTL0);

	val = AUD_PLL_CTL0_DIV_MCLK2_3((ac_s->postdiv & 0xc) >> 2) |
	      AUD_PLL_CTL0_DIV_FBCCLK2_5((ap_s->fbcclk & 0x3c) >> 2) |
	      AUD_PLL_CTL1_CLK_SEL_AUDIO_PLL |
	      AUD_PLL_CTL1_PLL_LOCK |
	      AUD_PLL_CTL1_DIV_OCLK_PATTERN(ac_s->oclk_pattern);
	__raw_writel(val, AUD_PLL_CTL1);

	clk->rate = rate;
	aclk_rate = rate;
	return 0;
}

/* Audio Bus Clock/Reset Controler */
struct clkops audio_clk_ops = {
	.enable		= audio_clk_enable,
	.disable	= audio_clk_disable,
	.setrate	= audio_clk_setrate,
};

static int sysclk_enable(struct clk *clk)
{
	unsigned int val;

	/* enable sysclk */
	val = __raw_readl(AUD_CTL);
	val |= AUD_CTL_SYSCLK_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	return 0;
}

static void sysclk_disable(struct clk *clk)
{
	unsigned int val;

	/* disable sysclk */
	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_SYSCLK_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));
}

static int sysclk_setrate(struct clk *clk, unsigned long rate)
{
	int val, div;

	/* aclk not init yet */
	if (aclk_rate < 0)
		return 0;

	div = aclk_rate;
	do_div(div, rate);
	pr_debug("%s: aclk_rate %ld rate %ld devider = %d\n",
		__func__, aclk_rate, rate, div);

	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_SYSCLK_DIV_MASK;
	val |= AUD_CTL_SYSCLK_DIV(div);
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	clk->rate = rate;
	return 0;
}

/* SSPA Audio Control Sysclk Divider */
struct clkops sysclk_ops = {
	.enable		= sysclk_enable,
	.disable	= sysclk_disable,
	.setrate	= sysclk_setrate,
};

static int sspa1_clk_enable(struct clk *clk)
{
	unsigned int val;

	val = __raw_readl(AUD_CTL);
	val |= AUD_CTL_S1_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	return 0;
}

static void sspa1_clk_disable(struct clk *clk)
{
	unsigned int val;

	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_S1_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));
}

static int sspa1_clk_setrate(struct clk *clk, unsigned long rate)
{
	int val, div;

	/* aclk not init yet */
	if (aclk_rate < 0)
		return 0;

	div = aclk_rate;
	do_div(div, rate);
	pr_debug("%s: aclk_rate %ld rate %ld divider %d\n",
		__func__, aclk_rate, rate, div);

	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_S1_CLK_DIV_MASK;
	val |= AUD_CTL_S1_CLK_DIV(div);
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	clk->rate = rate;
	return 0;
}

struct clkops sspa1_clk_ops = {
	.enable		= sspa1_clk_enable,
	.disable	= sspa1_clk_disable,
	.setrate	= sspa1_clk_setrate,
};

static int sspa2_clk_enable(struct clk *clk)
{
	unsigned int val;

	val = __raw_readl(AUD_CTL);
	val |= AUD_CTL_S2_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	return 0;
}

static void sspa2_clk_disable(struct clk *clk)
{
	unsigned int val;

	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_S2_ENA;
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));
}

static int sspa2_clk_setrate(struct clk *clk, unsigned long rate)
{
	int val, div;

	/* aclk not init yet */
	if (aclk_rate < 0)
		return 0;

	div = aclk_rate;
	do_div(div, rate);
	pr_debug("%s: aclk_rate %ld rate %ld devider %d\n",
		__func__, aclk_rate, rate, div);

	val = __raw_readl(AUD_CTL);
	val &= ~AUD_CTL_S2_CLK_DIV_MASK;
	val |= AUD_CTL_S2_CLK_DIV(div);
	__raw_writel(val, AUD_CTL);
	pr_debug("%s: actrl %x\n", __func__, __raw_readl(AUD_CTL));

	clk->rate = rate;
	return 0;
}

struct clkops sspa2_clk_ops = {
	.enable		= sspa2_clk_enable,
	.disable	= sspa2_clk_disable,
	.setrate	= sspa2_clk_setrate,
};

static struct clk mmp3_list_clks[] = {
	APBC_CLK("twsi1", "pxa2xx-i2c.0", NULL, MMP2_TWSI1,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("twsi2", "pxa2xx-i2c.1", NULL, MMP2_TWSI2,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("twsi3", "pxa2xx-i2c.2", NULL, MMP2_TWSI3,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("twsi4", "pxa2xx-i2c.3", NULL, MMP2_TWSI4,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("twsi5", "pxa2xx-i2c.4", NULL, MMP2_TWSI5,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("twsi6", "pxa2xx-i2c.5", NULL, MMP2_TWSI6,
			0, 26000000, &mmp3_clk_vctcxo),
	APBC_CLK("keypad", "pxa27x-keypad", NULL, MMP2_KPC,
			0, 32768, &mmp3_clk_32k),

	APBC_CLK("ssp.1", "mmp-ssp.1", NULL, MMP2_SSP1,
		0, 6500000, &mmp3_clk_vctcxo),
	APBC_CLK("ssp.2", "mmp-ssp.2", NULL, MMP2_SSP2,
		0, 6500000, &mmp3_clk_vctcxo),
	APBC_CLK("ssp.3", "mmp-ssp.3", NULL, MMP2_SSP3,
		0, 6500000, &mmp3_clk_vctcxo),
	APBC_CLK("ssp.4", "mmp-ssp.4", NULL, MMP2_SSP4,
		0, 6500000, &mmp3_clk_vctcxo),
	APBC_CLK("gpio", "pxa-gpio", NULL, MMP2_GPIO,
		0, 26000000, &mmp3_clk_vctcxo),
	/*
	 * Bit 7 in APBC_RTC_CLK_RST must be set before enabling
	 * RTC operations.
	 */
	APBC_CLK("rtc", "mmp-rtc", NULL, MMP2_RTC,
			0x8, 32768, &mmp3_clk_32k),

	APBC_CLK_OPS("pwm1", "mmp2-pwm.0", NULL, MMP2_PWM0,
			0, 26000000, &mmp3_clk_vctcxo, &pwm_clk_ops),
	APBC_CLK_OPS("pwm2", "mmp2-pwm.1", NULL, MMP2_PWM1,
			0, 26000000, &mmp3_clk_vctcxo, &pwm_clk_ops),
	APBC_CLK_OPS("pwm3", "mmp2-pwm.2", NULL, MMP2_PWM2,
			0, 26000000, &mmp3_clk_vctcxo, &pwm_clk_ops),
	APBC_CLK_OPS("pwm4", "mmp2-pwm.3", NULL, MMP2_PWM3,
			0, 26000000, &mmp3_clk_vctcxo, &pwm_clk_ops),
	APBC_CLK_OPS("uart1", "pxa2xx-uart.0", NULL, MMP2_UART1,
			1, 26000000, &mmp3_clk_vctcxo, &uart_clk_ops),
	APBC_CLK_OPS("uart2", "pxa2xx-uart.1", NULL, MMP2_UART2,
			1, 26000000, &mmp3_clk_vctcxo, &uart_clk_ops),
	APBC_CLK_OPS("uart3", "pxa2xx-uart.2", NULL, MMP2_UART3,
			1, 26000000, &mmp3_clk_vctcxo, &uart_clk_ops),
	APBC_CLK_OPS("uart4", "pxa2xx-uart.3", NULL, MMP2_UART4,
			1, 26000000, &mmp3_clk_vctcxo, &uart_clk_ops),
	APMU_CLK("u2o", NULL, "U2OCLK", USB,
			0x9, 480000000, NULL),
	APMU_CLK_OPS("nand", "pxa3xx-nand", NULL, NAND,
			0xbf, 100000000, &mmp3_clk_pll1, &nand_clk_ops),
	APMU_CLK_OPS("hsic1", NULL, "HSIC1CLK", USBHSIC1,
			0x1b, 480000000, NULL, &hsic_clk_ops),
	APMU_CLK_OPS("hsic2", NULL, "HSIC2CLK", USBHSIC2,
			0x1b, 480000000, NULL, &hsic_clk_ops),
	AUD_CLK_OPS("audio", NULL, "mmp-audio", &audio_clk_ops),
	AUD_CLK_OPS("sysclk", NULL, "mmp-sysclk", &sysclk_ops),
	AUD_CLK_OPS("sspa1", "mmp-sspa-dai.0", NULL, &sspa1_clk_ops),
	AUD_CLK_OPS("sspa2", "mmp-sspa-dai.1", NULL, &sspa2_clk_ops),
};

static void mmp3_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);
}

static int __init mmp3_clk_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mmp3_clks_ptr); i++)
		mmp3_init_one_clock(mmp3_clks_ptr[i]);
	for (i = 0; i < ARRAY_SIZE(mmp3_list_clks); i++)
		mmp3_init_one_clock(&mmp3_list_clks[i]);

	return 0;
}

core_initcall(mmp3_clk_init);

