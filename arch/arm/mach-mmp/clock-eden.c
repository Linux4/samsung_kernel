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

#define PMUA_CC				APMU_REG(0x4)
#define PMUA_CC_2			APMU_REG(0x150)
#define PMUA_CC_3			APMU_REG(0x188)
#define PMUA_BUS			APMU_REG(0x6c)
#define PMUA_DM_CC			APMU_REG(0xc)
#define PMUA_DM_2_CC			APMU_REG(0x158)

#define CLK_SET_BITS(set, clear)	{	\
	unsigned long tmp;			\
	tmp = __raw_readl(clk->clk_rst);	\
	tmp &= ~(clear);			\
	tmp |= (set);				\
	__raw_writel(tmp, clk->clk_rst);	\
}

struct periph_clk_tbl {
	unsigned long clk_rate;	/* clk rate */
	struct clk	*parent;/* clk parent */
	unsigned long src_val;	/* clk src field reg val */
	unsigned long div_val;	/* clk div field reg val */

	/* combined clck rate, such as bus clk that will changed with fclk */
	unsigned long comclk_rate;
};

static struct clk ref_vco = {
	.name = "ref_vco",
	.rate = 26000000,
	.ops = NULL,
};

static struct clk pll1_416 = {
	.name = "pll1_416",
	.rate = 416000000,
	.ops = NULL,
};

static struct clk pll1_624 = {
	.name = "pll1",
	.rate = 624000000,
	.ops = NULL,
};

static struct clk pll1_1248 = {
	.name = "pll1_1248",
	.rate = 1248000000,
	.ops = NULL,
};

static int gate_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(clk->enable_val, 0);
	return 0;
}

static void gate_clk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, clk->enable_val);
}

struct clkops gate_clk_ops = {
	.enable		= gate_clk_enable,
	.disable	= gate_clk_disable,
};

#define DEFINE_GATE_CLK(_name, _reg, _eval, _dev_id, _con_id)	\
	static struct clk _name = {				\
		.name = #_name,					\
		.clk_rst = (void __iomem *)_reg,		\
		.enable_val = _eval,				\
		.ops = &gate_clk_ops,				\
		.lookup = {					\
			.dev_id = _dev_id,			\
			.con_id = _con_id,			\
		},						\
	}							\


/*
 * TODO:
 * PLL2, PLL2P ~ PLL5, PLL5P section
 * Fill these functions according to EDEN spec
 */
static void clk_pll2_vco_init(struct clk *clk)
{
}

static int clk_pll2_vco_enable(struct clk *clk)
{
	return 0;
}

static void clk_pll2_vco_disable(struct clk *clk)
{
}

static int clk_pll2_vco_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll2_vco_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll2_vco_ops = {
	.init = clk_pll2_vco_init,
	.enable = clk_pll2_vco_enable,
	.disable = clk_pll2_vco_disable,
	.setrate = clk_pll2_vco_setrate,
	.getrate = clk_pll2_vco_getrate,
};

static struct clk pll2_vco = {
	.name = "pll2_vco",
	.parent = &ref_vco,
	.ops = &clk_pll2_vco_ops,
};

/* do nothing only used to adjust proper clk->refcnt */
static int clk_pll_dummy_enable(struct clk *clk)
{
	return 0;
}

static void clk_pll_dummy_disable(struct clk *clk)
{
}

static void clk_pll2_init(struct clk *clk)
{
}

static int clk_pll2_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll2_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll2_ops = {
	.init = clk_pll2_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll2_setrate,
	.getrate = clk_pll2_getrate,
};

static struct clk pll2 = {
	.name = "pll2",
	.parent = &pll2_vco,
	.ops = &clk_pll2_ops,
};

static void clk_pll2p_init(struct clk *clk)
{
}

static int clk_pll2p_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll2p_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll2p_ops = {
	.init = clk_pll2p_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll2p_setrate,
	.getrate = clk_pll2p_getrate,
};

static struct clk pll2p = {
	.name = "pll2p",
	.parent = &pll2_vco,
	.ops = &clk_pll2p_ops,
};

static void clk_pll3_vco_init(struct clk *clk)
{
}

static int clk_pll3_vco_enable(struct clk *clk)
{
	return 0;
}

static void clk_pll3_vco_disable(struct clk *clk)
{
}

static int clk_pll3_vco_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll3_vco_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll3_vco_ops = {
	.init = clk_pll3_vco_init,
	.enable = clk_pll3_vco_enable,
	.disable = clk_pll3_vco_disable,
	.setrate = clk_pll3_vco_setrate,
	.getrate = clk_pll3_vco_getrate,
};

static struct clk pll3_vco = {
	.name = "pll3_vco",
	.parent = &ref_vco,
	.ops = &clk_pll3_vco_ops,
};

static void clk_pll3_init(struct clk *clk)
{
}

static int clk_pll3_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll3_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll3_ops = {
	.init = clk_pll3_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll3_setrate,
	.getrate = clk_pll3_getrate,
};

static struct clk pll3 = {
	.name = "pll3",
	.parent = &pll3_vco,
	.ops = &clk_pll3_ops,
};

static void clk_pll3p_init(struct clk *clk)
{
}

static int clk_pll3p_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll3p_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll3p_ops = {
	.init = clk_pll3p_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll3p_setrate,
	.getrate = clk_pll3p_getrate,
};

static struct clk pll3p = {
	.name = "pll3p",
	.parent = &pll3_vco,
	.ops = &clk_pll3p_ops,
};

static void clk_pll4_vco_init(struct clk *clk)
{
}

static int clk_pll4_vco_enable(struct clk *clk)
{
	return 0;
}

static void clk_pll4_vco_disable(struct clk *clk)
{
}

static int clk_pll4_vco_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll4_vco_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll4_vco_ops = {
	.init = clk_pll4_vco_init,
	.enable = clk_pll4_vco_enable,
	.disable = clk_pll4_vco_disable,
	.setrate = clk_pll4_vco_setrate,
	.getrate = clk_pll4_vco_getrate,
};

static struct clk pll4_vco = {
	.name = "pll4_vco",
	.parent = &ref_vco,
	.ops = &clk_pll4_vco_ops,
};

static void clk_pll4_init(struct clk *clk)
{
}

static int clk_pll4_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll4_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll4_ops = {
	.init = clk_pll4_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll4_setrate,
	.getrate = clk_pll4_getrate,
};

static struct clk pll4 = {
	.name = "pll4",
	.parent = &pll4_vco,
	.ops = &clk_pll4_ops,
};

static void clk_pll4p_init(struct clk *clk)
{
}

static int clk_pll4p_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll4p_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll4p_ops = {
	.init = clk_pll4p_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll4p_setrate,
	.getrate = clk_pll4p_getrate,
};

static struct clk pll4p = {
	.name = "pll4p",
	.parent = &pll4_vco,
	.ops = &clk_pll4p_ops,
};

static void clk_pll5_vco_init(struct clk *clk)
{
}

static int clk_pll5_vco_enable(struct clk *clk)
{
	return 0;
}

static void clk_pll5_vco_disable(struct clk *clk)
{
}

static int clk_pll5_vco_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll5_vco_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll5_vco_ops = {
	.init = clk_pll5_vco_init,
	.enable = clk_pll5_vco_enable,
	.disable = clk_pll5_vco_disable,
	.setrate = clk_pll5_vco_setrate,
	.getrate = clk_pll5_vco_getrate,
};

static struct clk pll5_vco = {
	.name = "pll5_vco",
	.parent = &ref_vco,
	.ops = &clk_pll5_vco_ops,
};

static void clk_pll5_init(struct clk *clk)
{
}

static int clk_pll5_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll5_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll5_ops = {
	.init = clk_pll5_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll5_setrate,
	.getrate = clk_pll5_getrate,
};

static struct clk pll5 = {
	.name = "pll5",
	.parent = &pll5_vco,
	.ops = &clk_pll5_ops,
};

static void clk_pll5p_init(struct clk *clk)
{
}

static int clk_pll5p_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long clk_pll5p_getrate(struct clk *clk)
{
	return clk->rate;
}

static struct clkops clk_pll5p_ops = {
	.init = clk_pll5p_init,
	.enable = clk_pll_dummy_enable,
	.disable = clk_pll_dummy_disable,
	.setrate = clk_pll5p_setrate,
	.getrate = clk_pll5p_getrate,
};

static struct clk pll5p = {
	.name = "pll5p",
	.parent = &pll5_vco,
	.ops = &clk_pll5p_ops,
};

static void __clk_fill_periph_tbl(struct clk *clk,
	struct periph_clk_tbl *clk_tbl, unsigned int clk_tbl_size)
{
	unsigned int i = 0;
	unsigned long prate = 0;
	const struct clk_mux_sel *sel;

	pr_info("************** clk_%s_tbl  **************\n", clk->name);

	for (i = 0; i < clk_tbl_size; i++) {
		for (sel = clk->inputs; sel->input != NULL; sel++) {
			if (sel->input == clk_tbl[i].parent) {
				prate = clk_get_rate(sel->input);
				if (((prate % clk_tbl[i].clk_rate) > 3) || !prate)
					break;
				clk_tbl[i].src_val = sel->value;
				clk_tbl[i].div_val =
					prate / clk_tbl[i].clk_rate;
				pr_info("clk[%lu] src[%lu:%lu] div[%lu]\n",
					clk_tbl[i].clk_rate, prate, \
					clk_tbl[i].src_val, \
					clk_tbl[i].div_val);
				break;
			}
		}
	}
}

static long __clk_round_rate_bytbl(struct clk *clk, unsigned long rate,
	struct periph_clk_tbl *clk_tbl, unsigned int clk_tbl_size)
{
	unsigned int i;

	if (unlikely(rate > clk_tbl[clk_tbl_size - 1].clk_rate))
		return clk_tbl[clk_tbl_size - 1].clk_rate;

	for (i = 0; i < clk_tbl_size; i++) {
		if (rate <= clk_tbl[i].clk_rate)
			return clk_tbl[i].clk_rate;
	}

	return clk->rate;
}

static long __clk_set_mux_div(struct clk *clk, struct clk *best_parent,
	unsigned int mux, unsigned int div)
{
	unsigned int muxmask, divmask;
	unsigned int muxval, divval;
	unsigned int regval;

	BUG_ON(!div);

	clk->div = div;
	clk->mul = 1;

	muxval = (mux > clk->reg_data[SOURCE][CONTROL].reg_mask) ? \
		clk->reg_data[SOURCE][CONTROL].reg_mask : mux;
	divval = (div > clk->reg_data[DIV][CONTROL].reg_mask) ? \
		clk->reg_data[DIV][CONTROL].reg_mask : div;

	muxmask = clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;
	divmask = clk->reg_data[DIV][CONTROL].reg_mask << \
		clk->reg_data[DIV][CONTROL].reg_shift;

	muxval = muxval << clk->reg_data[SOURCE][CONTROL].reg_shift;
	divval = divval << clk->reg_data[DIV][CONTROL].reg_shift;

	regval = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);
	regval &= ~(muxmask | divmask);
	regval |= (muxval | divval);
	__raw_writel(regval, clk->reg_data[SOURCE][CONTROL].reg);

	clk_reparent(clk, best_parent);

	pr_debug("\n%s clk:%s [%x] = [%x]\n", __func__, clk->name, \
		(unsigned int)clk->reg_data[SOURCE][CONTROL].reg, regval);

	return 0;
}

static unsigned int __clk_parent_to_mux(struct clk *clk, struct clk *parent)
{
	unsigned int i;

	i = 0;
	while ((clk->inputs[i].input) && (clk->inputs[i].input != parent))
		i++;
	BUG_ON(!clk->inputs[i].input);

	return clk->inputs[i].value;
}

static void __clk_periph_init(struct clk *clk,
	struct clk *parent, unsigned int div, bool dyn_chg)
{
	unsigned int mux = 0;

	clk->dynamic_change = dyn_chg;
	clk->mul = 1;
	clk->div = div;
	mux  = __clk_parent_to_mux(clk, parent);
	__clk_set_mux_div(clk, parent, mux, div);
	clk->rate = clk_get_rate(clk->parent) * clk->mul / clk->div;
}

#ifdef CONFIG_MACH_EDEN_FPGA

#define GC_CLK_EN               ((1u << 3) | (1u << 15))
#define GC_AXICLK_EN            ((1u << 2) | (1u << 19))

static void gc_clk_init(struct clk *clk)
{
}

static int gc_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(GC_CLK_EN, 0);
	udelay(100);

	CLK_SET_BITS(GC_AXICLK_EN, 0);
	udelay(100);

	return 0;
}

static void gc_clk_disable(struct clk *clk)
{
}

static long gc_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static int gc_clk_setrate(struct clk *clk, unsigned long rate)
{
	return 0;
}

struct clkops gc_clk_ops = {
	.init		= gc_clk_init,
	.enable		= gc_clk_enable,
	.disable	= gc_clk_disable,
	.setrate	= gc_clk_setrate,
	.round_rate	= gc_clk_round_rate,
};

static struct clk_mux_sel gc_mux_pll[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll2, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll2p, .value = 3},
	{0, 0},
};

static struct clk eden_clk_gc = {
	.name = "gc",
	.inputs = gc_mux_pll,
	.lookup = {
		.con_id = "GCCLK",
	},
	.clk_rst = (void __iomem *)APMU_GC,
	.ops = &gc_clk_ops,
};

#endif

#define GC2D_CLK_EN				(1u << 15)
#define GC2D_CLK_RST			(1u << 14)
#define GC2D_ACLK_EN			(1u << 19)

#define GC3D_CLK_EN			(1u << 3)
#define GC3D_CLK_RST		(1u << 1)
#define GC3D_ACLK_EN		(1u << 2)

static DEFINE_SPINLOCK(gc3d_lock);

static struct clk_mux_sel gc3d_mux_pll[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll2, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll2p, .value = 3},
	{NULL, 0},
};

static struct clk_mux_sel gc2d_mux_pll[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll2, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll5p, .value = 3},
	{NULL, 0},
};

static struct clk_mux_sel gc_aclk_mux_pll[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll2, .value = 1},
	{.input = &pll5p, .value = 2},
	{.input = &pll1_416, .value = 3},
	{NULL, 0},
};

/* TODO: add more GC freq point */
static struct periph_clk_tbl gc_aclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
	},
};

static struct periph_clk_tbl gc3d_clk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
};

static struct periph_clk_tbl gc2d_clk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
};


/* GC3D AXI clock, it's the fabric clock shared by both 2D and 3D */
static void gc3d_aclk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, gc_aclk_tbl,
		ARRAY_SIZE(gc_aclk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&gc3d_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&gc3d_lock);
}

static int gc3d_aclk_enable(struct clk *clk)
{
	spin_lock(&gc3d_lock);
	CLK_SET_BITS(GC3D_ACLK_EN, 0);
	spin_unlock(&gc3d_lock);

	return 0;
}

static void gc3d_aclk_disable(struct clk *clk)
{
	spin_lock(&gc3d_lock);
	CLK_SET_BITS(0, GC3D_ACLK_EN);
	spin_unlock(&gc3d_lock);
}

static long gc_aclk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		gc_aclk_tbl, ARRAY_SIZE(gc_aclk_tbl));
}

static int gc3d_aclk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (gc_aclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc_aclk_tbl));

	spin_lock(&gc3d_lock);
	enable_bit = readl(clk->clk_rst) & GC3D_ACLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, GC3D_ACLK_EN);

	/* change frequency */
	__clk_set_mux_div(clk, gc_aclk_tbl[i].parent,
		gc_aclk_tbl[i].src_val, gc_aclk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(GC3D_ACLK_EN, 0);
	spin_unlock(&gc3d_lock);

	return 0;
}

struct clkops gc3d_aclk_ops = {
	.init		= gc3d_aclk_init,
	.enable		= gc3d_aclk_enable,
	.disable	= gc3d_aclk_disable,
	.setrate	= gc3d_aclk_setrate,
	.round_rate	= gc_aclk_round_rate,
};

static struct clk eden_clk_gc3d_axi = {
	.name = "gc3d_axi",
	.inputs = gc_aclk_mux_pll,
	.lookup = {
		.con_id = "GC3D_ACLK",
	},
	.clk_rst = (void __iomem *)APMU_GC_CLK_RES_CTRL,
	.ops = &gc3d_aclk_ops,
	.reg_data = {
		     { {APMU_GC_CLK_RES_CTRL, 4, 0x3},
			{APMU_GC_CLK_RES_CTRL, 4, 0x3} },
		     { {APMU_GC_CLK_RES_CTRL, 23, 0x7},
			{APMU_GC_CLK_RES_CTRL, 23, 0x7} } }
};

/* GC3D 1x clock */
static struct clk *gc3d_1x_depend_clk[] = {
	&eden_clk_gc3d_axi,
};

static void gc3d_clk1x_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, gc3d_clk_tbl,
		ARRAY_SIZE(gc3d_clk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&gc3d_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&gc3d_lock);
}

static int gc3d_clk1x_enable(struct clk *clk)
{
	spin_lock(&gc3d_lock);
	CLK_SET_BITS(GC3D_CLK_EN, 0);
	spin_unlock(&gc3d_lock);

	return 0;
}

static void gc3d_clk1x_disable(struct clk *clk)
{
	spin_lock(&gc3d_lock);
	CLK_SET_BITS(0, GC3D_CLK_EN);
	spin_unlock(&gc3d_lock);
}

static long gc3d_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		gc3d_clk_tbl, ARRAY_SIZE(gc3d_clk_tbl));
}

static int gc3d_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (gc3d_clk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc3d_clk_tbl));

	spin_lock(&gc3d_lock);
	enable_bit = readl(clk->clk_rst) & GC3D_CLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, GC3D_CLK_EN);

	/* change frequency */
	__clk_set_mux_div(clk, gc3d_clk_tbl[i].parent,
		gc3d_clk_tbl[i].src_val, gc3d_clk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(GC3D_CLK_EN, 0);
	spin_unlock(&gc3d_lock);

	/* TODO: change shared fabric clock frequency
	 * according to GC2D and GC3D frequency.
	 */

	return 0;
}

struct clkops gc3d_clk1x_ops = {
	.init		= gc3d_clk1x_init,
	.enable		= gc3d_clk1x_enable,
	.disable	= gc3d_clk1x_disable,
	.setrate	= gc3d_clk_setrate,
	.round_rate	= gc3d_clk_round_rate,
};

static struct clk eden_clk_gc3d_1x = {
	.name = "gc3d_1x",
	.inputs = gc3d_mux_pll,
	.lookup = {
		.con_id = "GC3D_CLK1X",
	},
	.clk_rst = (void __iomem *)APMU_GC_CLK_RES_CTRL,
	.ops = &gc3d_clk1x_ops,
	.dependence = gc3d_1x_depend_clk,
	.dependence_count = ARRAY_SIZE(gc3d_1x_depend_clk),
	.reg_data = {
		     { {APMU_GC_CLK_RES_CTRL, 6, 0x3},
			{APMU_GC_CLK_RES_CTRL, 6, 0x3} },
		     { {APMU_GC_CLK_RES_CTRL, 26, 0x7},
			{APMU_GC_CLK_RES_CTRL, 26, 0x7} } }
};

/* GC3D 2x clock, only implement clk init/setrate/roundrate.
 * clk enable/disable is controled by GC3D_CLK1X since they
 * share same one enable bit. */
static struct clk *gc3d_2x_depend_clk[] = {
	&eden_clk_gc3d_1x,
};

static void gc3d_clk2x_init(struct clk *clk)
{
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&gc3d_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&gc3d_lock);
}

struct clkops gc3d_clk2x_ops = {
	.init		= gc3d_clk2x_init,
	.setrate	= gc3d_clk_setrate,
	.round_rate	= gc3d_clk_round_rate,
};

static struct clk eden_clk_gc3d_2x = {
	.name = "gc3d_2x",
	.inputs = gc3d_mux_pll,
	.lookup = {
		.con_id = "GC3D_CLK2X",
	},
	.clk_rst = (void __iomem *)APMU_GC_CLK_RES_CTRL,
	.ops = &gc3d_clk2x_ops,
	.dependence = gc3d_2x_depend_clk,
	.dependence_count = ARRAY_SIZE(gc3d_2x_depend_clk),
	.reg_data = {
		     { {APMU_GC_CLK_RES_CTRL, 12, 0x3},
			{APMU_GC_CLK_RES_CTRL, 12, 0x3} },
		     { {APMU_GC_CLK_RES_CTRL, 29, 0x7},
			{APMU_GC_CLK_RES_CTRL, 29, 0x7} } }
};

/* GC 2D axi clock */
static void gc2d_aclk_init(struct clk *clk)
{
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	__clk_periph_init(clk, &pll1_624, 4, 1);
}

static int gc2d_aclk_enable(struct clk *clk)
{
	CLK_SET_BITS(GC2D_ACLK_EN, 0);
	return 0;
}

static void gc2d_aclk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, GC2D_ACLK_EN);
}

static int gc2d_aclk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (gc_aclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc_aclk_tbl));

	enable_bit = readl(clk->clk_rst) & GC2D_ACLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, GC2D_ACLK_EN);

	/* change frequency */
	__clk_set_mux_div(clk, gc_aclk_tbl[i].parent,
		gc_aclk_tbl[i].src_val, gc_aclk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(GC2D_ACLK_EN, 0);

	return 0;
}

struct clkops gc2d_aclk_ops = {
	.init		= gc2d_aclk_init,
	.enable		= gc2d_aclk_enable,
	.disable	= gc2d_aclk_disable,
	.setrate	= gc2d_aclk_setrate,
	.round_rate	= gc_aclk_round_rate,
};

static struct clk eden_clk_gc2d_axi = {
	.name = "gc2d_axi",
	.inputs = gc_aclk_mux_pll,
	.lookup = {
		.con_id = "GC2D_ACLK",
	},
	.clk_rst = (void __iomem *)APMU_GC_CLK_RES_CTRL2,
	.ops = &gc2d_aclk_ops,
	.reg_data = {
		     { {APMU_GC_CLK_RES_CTRL2, 0, 0x3},
			{APMU_GC_CLK_RES_CTRL2, 0, 0x3} },
		     { {APMU_GC_CLK_RES_CTRL2, 2, 0x7},
			{APMU_GC_CLK_RES_CTRL2, 2, 0x7} } }
};


/* GC 2D functional clock */
static struct clk *gc2d_depend_clk[] = {
	&eden_clk_gc3d_axi,
	&eden_clk_gc2d_axi,
};

static void gc2d_clk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, gc2d_clk_tbl,
		ARRAY_SIZE(gc2d_clk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	__clk_periph_init(clk, &pll1_624, 4, 1);
}

static int gc2d_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(GC2D_CLK_EN, 0);
	return 0;
}

static void gc2d_clk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, GC2D_CLK_EN);
}

static long gc2d_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		gc2d_clk_tbl, ARRAY_SIZE(gc2d_clk_tbl));
}

static int gc2d_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (gc2d_clk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc2d_clk_tbl));

	enable_bit = readl(clk->clk_rst) & GC2D_CLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, GC2D_ACLK_EN | GC2D_CLK_EN);

	/* change frequency */
	__clk_set_mux_div(clk, gc2d_clk_tbl[i].parent,
		gc2d_clk_tbl[i].src_val, gc2d_clk_tbl[i].div_val);
	/* update axi clock */
	clk_set_rate(&eden_clk_gc2d_axi, gc2d_clk_tbl[i].comclk_rate);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(GC2D_ACLK_EN | GC2D_CLK_EN, 0);

	/* TODO: change shared fabric clock frequency
	 * according to GC2D and GC3D frequency.
	 */

	return 0;
}

struct clkops gc2d_clk_ops = {
	.init		= gc2d_clk_init,
	.enable		= gc2d_clk_enable,
	.disable	= gc2d_clk_disable,
	.setrate	= gc2d_clk_setrate,
	.round_rate	= gc2d_clk_round_rate,
};

static struct clk eden_clk_gc2d = {
	.name = "gc2d",
	.inputs = gc2d_mux_pll,
	.lookup = {
		.con_id = "GC2D_CLK",
	},
	.clk_rst = (void __iomem *)APMU_GC_CLK_RES_CTRL2,
	.ops = &gc2d_clk_ops,
	.dependence = gc2d_depend_clk,
	.dependence_count = ARRAY_SIZE(gc2d_depend_clk),
	.reg_data = {
		     { {APMU_GC_CLK_RES_CTRL2, 16, 0x3},
			{APMU_GC_CLK_RES_CTRL2, 16, 0x3} },
		     { {APMU_GC_CLK_RES_CTRL2, 20, 0x7},
			{APMU_GC_CLK_RES_CTRL2, 20, 0x7} } }
};

#ifdef CONFIG_UIO_HANTRO

#define VPU_DECODER_CLK_EN			(1 << 27)
#define VPU_AXI_CLK_EN				(1 << 3)
#define VPU_ENCODER_CLK_EN			(1 << 4)

static DEFINE_SPINLOCK(vpu_lock);

static struct clk_mux_sel vpu_mux_pll[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll5p, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll1_416, .value = 3},
	{NULL, 0},
};

/* TODO: add more freq point */
/* VPU AXI clock table */
static struct periph_clk_tbl vpu_aclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
	},
};

/* VPU decoder clock table */
static struct periph_clk_tbl vpu_dclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
};

/* VPU encoder clock table */
static struct periph_clk_tbl vpu_eclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
};


/* vpu axi clock */
static void vpu_axi_clk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, vpu_aclk_tbl,
		ARRAY_SIZE(vpu_aclk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&vpu_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&vpu_lock);
}

static int vpu_axi_clk_enable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(VPU_AXI_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

static void vpu_axi_clk_disable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(0, VPU_AXI_CLK_EN);
	spin_unlock(&vpu_lock);
}

static long vpu_axi_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		vpu_aclk_tbl, ARRAY_SIZE(vpu_aclk_tbl));
}

static int vpu_axi_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (vpu_aclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(vpu_aclk_tbl));

	spin_lock(&vpu_lock);
	enable_bit = readl(clk->clk_rst) & VPU_AXI_CLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, VPU_AXI_CLK_EN);

	/* change frequency */
	__clk_set_mux_div(clk, vpu_aclk_tbl[i].parent,
		vpu_aclk_tbl[i].src_val, vpu_aclk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(VPU_AXI_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

struct clkops vpu_axi_clk_ops = {
	.init		= vpu_axi_clk_init,
	.enable		= vpu_axi_clk_enable,
	.disable	= vpu_axi_clk_disable,
	.setrate	= vpu_axi_clk_setrate,
	.round_rate	= vpu_axi_clk_round_rate,
};

static struct clk eden_clk_vpu_axi = {
	.name = "vpu_axi",
	.inputs = vpu_mux_pll,
	.lookup = {
		.con_id = "VPU_AXI_CLK",
	},
	.clk_rst = (void __iomem *)APMU_VPU_CLK_RES_CTRL,
	.ops = &vpu_axi_clk_ops,
	.reg_data = {
		     { {APMU_VPU_CLK_RES_CTRL, 12, 0x3},
			{APMU_VPU_CLK_RES_CTRL, 12, 0x3} },
		     { {APMU_VPU_CLK_RES_CTRL, 19, 0x7},
			{APMU_VPU_CLK_RES_CTRL, 19, 0x7} } }
};

static struct clk *vpu_depend_clk[] = {
	&eden_clk_vpu_axi,
};

/* vpu decoder clock */
static void vpu_decoder_clk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, vpu_dclk_tbl,
		ARRAY_SIZE(vpu_dclk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&vpu_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&vpu_lock);
}

static int vpu_decoder_clk_enable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(VPU_DECODER_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

static void vpu_decoder_clk_disable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(0, VPU_DECODER_CLK_EN);
	spin_unlock(&vpu_lock);
}

static long vpu_decoder_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		vpu_dclk_tbl, ARRAY_SIZE(vpu_dclk_tbl));
}

static int vpu_decoder_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (vpu_dclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(vpu_dclk_tbl));

	spin_lock(&vpu_lock);
	enable_bit = readl(clk->clk_rst) & VPU_DECODER_CLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, VPU_DECODER_CLK_EN);

	/* change freq */
	__clk_set_mux_div(clk, vpu_dclk_tbl[i].parent,
		vpu_dclk_tbl[i].src_val, vpu_dclk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(VPU_DECODER_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

struct clkops vpu_decoder_clk_ops = {
	.init		= vpu_decoder_clk_init,
	.enable		= vpu_decoder_clk_enable,
	.disable	= vpu_decoder_clk_disable,
	.setrate	= vpu_decoder_clk_setrate,
	.round_rate	= vpu_decoder_clk_round_rate,
};

static struct clk eden_clk_vpu_decoder = {
	.name = "vpu_decoder",
	.inputs = vpu_mux_pll,
	.lookup = {
		.con_id = "VPU_DEC_CLK",
	},
	.clk_rst = (void __iomem *)APMU_VPU_CLK_RES_CTRL,
	.ops = &vpu_decoder_clk_ops,
	.dependence = vpu_depend_clk,
	.dependence_count = ARRAY_SIZE(vpu_depend_clk),
	.reg_data = {
		     { {APMU_VPU_CLK_RES_CTRL, 22, 0x3},
			{APMU_VPU_CLK_RES_CTRL, 22, 0x3} },
		     { {APMU_VPU_CLK_RES_CTRL, 24, 0x7},
			{APMU_VPU_CLK_RES_CTRL, 24, 0x7} } }
};

/* vpu encoder clock */
static void vpu_encoder_clk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, vpu_eclk_tbl,
		ARRAY_SIZE(vpu_eclk_tbl));
	/* default setting:src = pll1_624, div = 4 , dynamic change*/
	spin_lock(&vpu_lock);
	__clk_periph_init(clk, &pll1_624, 4, 1);
	spin_unlock(&vpu_lock);
}

static int vpu_encoder_clk_enable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(VPU_ENCODER_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

static void vpu_encoder_clk_disable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(0, VPU_ENCODER_CLK_EN);
	spin_unlock(&vpu_lock);
}

static long vpu_encoder_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		vpu_eclk_tbl, ARRAY_SIZE(vpu_eclk_tbl));
}

static int vpu_encoder_clk_setrate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned int enable_bit;

	if (rate == clk->rate)
		return 0;

	i = 0;
	while (vpu_eclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(vpu_eclk_tbl));

	spin_lock(&vpu_lock);
	enable_bit = readl(clk->clk_rst) & VPU_ENCODER_CLK_EN;
	/* disable clk before change frequency */
	if (enable_bit)
		CLK_SET_BITS(0, VPU_ENCODER_CLK_EN);

	/* change freq */
	__clk_set_mux_div(clk, vpu_eclk_tbl[i].parent,
		vpu_eclk_tbl[i].src_val, vpu_eclk_tbl[i].div_val);

	/* restore enable bit */
	if (enable_bit)
		CLK_SET_BITS(VPU_ENCODER_CLK_EN, 0);
	spin_unlock(&vpu_lock);

	return 0;
}

struct clkops vpu_encoder_clk_ops = {
	.init		= vpu_encoder_clk_init,
	.enable		= vpu_encoder_clk_enable,
	.disable	= vpu_encoder_clk_disable,
	.setrate	= vpu_encoder_clk_setrate,
	.round_rate	= vpu_encoder_clk_round_rate,
};

static struct clk eden_clk_vpu_encoder = {
	.name = "vpu_encoder",
	.inputs = vpu_mux_pll,
	.lookup = {
		.con_id = "VPU_ENC_CLK",
	},
	.clk_rst = (void __iomem *)APMU_VPU_CLK_RES_CTRL,
	.ops = &vpu_encoder_clk_ops,
	.dependence = vpu_depend_clk,
	.dependence_count = ARRAY_SIZE(vpu_depend_clk),
	.reg_data = {
		     { {APMU_VPU_CLK_RES_CTRL, 6, 0x3},
			{APMU_VPU_CLK_RES_CTRL, 6, 0x3} },
		     { {APMU_VPU_CLK_RES_CTRL, 16, 0x7},
			{APMU_VPU_CLK_RES_CTRL, 16, 0x7} } }
};

#endif

/*
 * TODO:
 * Display section
 */
static int disp1_axi_clk_enable(struct clk *clk)
{
	u32 val = __raw_readl(clk->clk_rst);

	/* enable Display1 AXI clock */
	val |= (1 << 3);
	__raw_writel(val, clk->clk_rst);

	/* release from reset */
	val |= 1;
	__raw_writel(val, clk->clk_rst);
	return 0;
}

static void disp1_axi_clk_disable(struct clk *clk)
{
	u32 val = __raw_readl(clk->clk_rst);

	/* reset display1 AXI clock */
	val &= ~1;
	__raw_writel(val, clk->clk_rst);

	/* disable display1 AXI clock */
	val &= ~(1 << 3);
	__raw_writel(val, clk->clk_rst);
}

struct clkops disp1_axi_clk_ops = {
	.enable		= disp1_axi_clk_enable,
	.disable	= disp1_axi_clk_disable,
};

static struct clk eden_clk_disp1_axi = {
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

	/* reset display1 peripheral clock */
	val &= ~(1 << 1);
	__raw_writel(val, clk->clk_rst);

	/* disable Display1 peripheral clock */
	val &= ~(1 << 4);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
}

static long disp1_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/* Fill in code for real silicon here */
	return 26000000;
}

static int disp1_clk_setrate(struct clk *clk, unsigned long rate)
{
#ifdef CONFIG_MACH_EDEN_FPGA
	return 0;
#else
	unsigned long parent_rate;
	const struct clk_mux_sel *sel;
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

	if (rate <= clk_get_rate(&pll1_416)) {
		parent_rate = clk_get_rate(&pll1_416);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &pll1_416);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &pll1_416)
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
	} else if (rate <= clk_get_rate(&pll5p)) {
		parent_rate = clk_get_rate(&pll5p);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &pll5p);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &pll5p)
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
	} else if (rate <= clk_get_rate(&pll1_624)) {
		parent_rate = clk_get_rate(&pll1_624);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &pll1_624);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &pll1_624)
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
	} else if (rate <= clk_get_rate(&pll5)) {
		parent_rate = clk_get_rate(&pll5);
		clk->mul = 1;
		clk->div = parent_rate / rate;

		clk_reparent(clk, &pll5);

		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
			 << clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
		    << clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &pll5)
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
	clk->rate = rate;

	return 0;
#endif /* CONFIG_MACH_EDEN_FPGA */
}

struct clkops disp1_clk_ops = {
	.enable = disp1_clk_enable,
	.disable = disp1_clk_disable,
	.round_rate = disp1_clk_round_rate,
	.setrate = disp1_clk_setrate,
};

static struct clk_mux_sel disp1_clk_mux[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll5p, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll1_416, .value = 3},
	{0, 0},
};

/* Disp1 clock can be one of the source for lcd */
static struct clk eden_clk_disp1 = {
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
	&eden_clk_disp1_axi,
	&eden_clk_disp1,
};

static int lcd_pn1_clk_enable(struct clk *clk)
{
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

#ifdef CONFIG_MACH_EDEN_FPGA
	/* release display1 phy from reset */
	val |= (1 << 2);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
#else
	/* Fill in code for real silicon here */
#endif

	return 0;
}

static void lcd_pn1_clk_disable(struct clk *clk)
{
	u32 val = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);

#ifdef CONFIG_MACH_EDEN_FPGA
	val &= ~(1 << 2);
	__raw_writel(val, clk->reg_data[SOURCE][CONTROL].reg);
#else
	/* Fill in code for real silicon here */
#endif
}

static long lcd_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/* Fill in code for real silicon here */
	return 26000000;
}

static int lcd_clk_setrate(struct clk *clk, unsigned long rate)
{
	/* Fill in code for real silicon here */
	return 0;
}

struct clkops lcd_pn1_clk_ops = {
	.enable = lcd_pn1_clk_enable,
	.disable = lcd_pn1_clk_disable,
	.round_rate = lcd_clk_round_rate,
	.setrate = lcd_clk_setrate,
};

static struct clk eden_clk_lcd1 = {
	.name = "lcd1",
	.lookup = {
		.con_id = "LCDCLK",
	},
	.ops = &lcd_pn1_clk_ops,
	.dependence = disp_depend_clk,
	.dependence_count = ARRAY_SIZE(disp_depend_clk),
	.reg_data = {
		     {{APMU_LCD_CLK_RES_CTRL, 6, 0x3},
			{APMU_LCD_CLK_RES_CTRL, 6, 0x3} },
		     {{APMU_LCD_CLK_RES_CTRL, 8, 0xf},
			{APMU_LCD_CLK_RES_CTRL, 8, 0xf} } },
};

/*
 * sdh clock section
 */
static void sdhc_clk_init(struct clk *clk)
{
	const struct clk_mux_sel *sel;
	u32 val = 0;

	clk->mul = 1;
	/* set base clock to 104Mhz to guarantee hold time */
	clk->div = 4;

	clk_reparent(clk, &pll1_416);

	if (!strcmp(clk->name, "sdh0")) {
		val &= ~(clk->reg_data[DIV][CONTROL].reg_mask
				<< clk->reg_data[DIV][CONTROL].reg_shift);
		val |= clk->div
			<< clk->reg_data[DIV][CONTROL].reg_shift;

		for (sel = clk->inputs; sel->input != 0; sel++) {
			if (sel->input == &pll1_416)
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
}

static int sdhc_clk_enable(struct clk *clk)
{
	u32 val;

	val = clk->enable_val | 0x1b;
	__raw_writel(val, clk->clk_rst);

	return 0;
}

static void sdhc_clk_disable(struct clk *clk)
{
	__raw_writel(0, clk->clk_rst);
}

struct clkops sdhc_clk_ops = {
	.init = sdhc_clk_init,
	.enable = sdhc_clk_enable,
	.disable = sdhc_clk_disable,
};

static struct clk_mux_sel sdhc_clk_mux[] = {
	{.input = &pll1_624, .value = 0},
	{.input = &pll5p, .value = 1},
	{.input = &pll5, .value = 2},
	{.input = &pll1_416, .value = 3},
	{NULL, 0},
};

/*
 * All sdhx share the same clock which will be enabled when any of sdhx clock
 * enable is set. But the clock source select and devider ratio is controlled
 * by sdh0. So currently the sdh base clock is fixed during bootup and can't
 * changed dynamically.
 */
static struct clk eden_clk_sdh0 = {
	.name = "sdh0",
	.lookup = {
		.dev_id = "sdhci-pxav3.0",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.reg_data = {
		{ {APMU_SDH0, 8, 0x3}, {APMU_SDH0, 8, 0x3} },
		{ {APMU_SDH0, 10, 0xf}, {APMU_SDH0, 10, 0xf} }
			},
	.clk_rst = (void __iomem *)APMU_SDH0,
};

static struct clk eden_clk_sdh1 = {
	.name = "sdh1",
	.lookup = {
		.dev_id = "sdhci-pxav3.1",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.clk_rst = (void __iomem *)APMU_SDH1,
};

static struct clk eden_clk_sdh2 = {
	.name = "sdh2",
	.lookup = {
		.dev_id = "sdhci-pxav3.2",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.clk_rst = (void __iomem *)APMU_SDH2,
};

static struct clk eden_clk_sdh3 = {
	.name = "sdh3",
	.lookup = {
		.dev_id = "sdhci-pxav3.3",
		.con_id = "PXA-SDHCLK",
	},
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.clk_rst = (void __iomem *)APMU_SDH3,
};

DEFINE_GATE_CLK(VCTCXO, MPMU_VRCR, 1, NULL, "VCTCXO");

/* all clk src on the board */
static struct clk *eden_clks_src[] = {
	&VCTCXO,
	&pll1_416,
	&pll1_624,
	&pll1_1248,
	&pll2_vco,
	&pll2,
	&pll2p,
	&pll3_vco,
	&pll3,
	&pll3p,
	&pll4_vco,
	&pll4,
	&pll4p,
	&pll5_vco,
	&pll5,
	&pll5p,
};


static struct clk *eden_clks_periph[] = {
	&eden_clk_sdh0,
	&eden_clk_sdh1,
	&eden_clk_sdh2,
	&eden_clk_sdh3,
	&eden_clk_lcd1,
	&eden_clk_disp1,
	&eden_clk_disp1_axi,
#ifdef CONFIG_MACH_EDEN_FPGA
	&eden_clk_gc,
#endif
	&eden_clk_gc3d_axi,
	&eden_clk_gc3d_1x,
	&eden_clk_gc3d_2x,
	&eden_clk_gc2d_axi,
	&eden_clk_gc2d,
#ifdef CONFIG_UIO_HANTRO
	&eden_clk_vpu_axi,
	&eden_clk_vpu_decoder,
	&eden_clk_vpu_encoder,
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


static struct clk eden_list_clks[] = {
	APBC_CLK("twsi1", "pxa2xx-i2c.0", NULL, MMP2_TWSI1,
			0, 26000000, NULL),
	APBC_CLK("twsi2", "pxa2xx-i2c.1", NULL, MMP2_TWSI2,
			0, 26000000, NULL),
	APBC_CLK("twsi3", "pxa2xx-i2c.2", NULL, MMP2_TWSI3,
			0, 26000000, NULL),
	APBC_CLK("twsi4", "pxa2xx-i2c.3", NULL, MMP2_TWSI4,
			0, 26000000, NULL),
	APBC_CLK("twsi5", "pxa2xx-i2c.4", NULL, MMP2_TWSI5,
			0, 26000000, NULL),
	APBC_CLK("twsi6", "pxa2xx-i2c.5", NULL, MMP2_TWSI6,
			0, 26000000, NULL),
	APBC_CLK("gpio", "pxa-gpio", NULL, MMP2_GPIO,
			0, 26000000, NULL),
	APBC_CLK("keypad", "pxa27x-keypad", NULL, MMP2_KPC,
			0, 32768, NULL),
#ifdef CONFIG_MACH_EDEN_FPGA
	APBC_CLK("uart1", "pxa2xx-uart.0", NULL, MMP2_UART1,
			1, 13000000, NULL),
#else
	APBC_CLK("uart1", "pxa2xx-uart.0", NULL, MMP2_UART1,
			1, 26000000, NULL),
#endif
	APBC_CLK("uart2", "pxa2xx-uart.1", NULL, MMP2_UART2,
			1, 26000000, NULL),
	APBC_CLK("uart3", "pxa2xx-uart.2", NULL, MMP2_UART3,
			1, 26000000, NULL),
	APBC_CLK("ssp.1", "eden-ssp.1", NULL, MMP2_SSP1,
		0, 6500000, NULL),
	APBC_CLK("ssp.2", "eden-ssp.2", NULL, MMP2_SSP2,
		0, 6500000, NULL),
	APBC_CLK("ssp.3", "eden-ssp.3", NULL, MMP2_SSP3,
		0, 6500000, NULL),
#ifdef CONFIG_RTC_DRV_SA1100
	APBC_CLK("rtc", "sa1100-rtc", NULL, MMP2_RTC,
			0x8, 32768, NULL),
#endif
};

static void eden_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);
}

static int __init eden_clk_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(eden_clks_src); i++)
		eden_init_one_clock(eden_clks_src[i]);
	for (i = 0; i < ARRAY_SIZE(eden_clks_periph); i++)
		eden_init_one_clock(eden_clks_periph[i]);
	for (i = 0; i < ARRAY_SIZE(eden_list_clks); i++)
		eden_init_one_clock(&eden_list_clks[i]);

	/* enable CP2AP clock */
	__raw_writel(0x1, APBC_EDEN_IPC_CP);

	return 0;
}

core_initcall(eden_clk_init);
