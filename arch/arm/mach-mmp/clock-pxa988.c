/*
 *  linux/arch/arm/mach-mmp/clock-pxa988.c
 *
 *  Author:	Zhoujie Wu <zjwu@marvell.com>
 *		Raul Xiong <xjian@marvell.com>
 *  Copyright:	(C) 2012 Marvell International Ltd.
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
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-ciu.h>
#include <mach/cputype.h>
#include <mach/clock-pxa988.h>
#include <plat/pxa_trace.h>
#include <plat/clock.h>
#include <plat/debugfs.h>
#include <plat/pm.h>

static int t32clock;

static int __clk_periph_set_rate(struct clk *clk, unsigned long rate);
static unsigned long __clk_periph_get_rate(struct clk *clk);
/*
 * README:
 * 1. For clk which has fc_request bit, two step operation
 * is safer to enable clock with taget frequency
 * 1) set enable&rst bit
 * 2) set mux, div and fc to do FC, get target rate.
 */

struct periph_clk_tbl {
	unsigned long clk_rate;	/* clk rate */
	struct clk	*parent;/* clk parent */
	unsigned long src_val;	/* clk src field reg val */
	unsigned long div_val;	/* clk div field reg val */

	/* combined clck rate, such as bus clk that will changed with fclk */
	unsigned long comclk_rate;
};

union pmum_pll2cr {
	struct {
		unsigned int reserved0:6;
		unsigned int reserved1:2;
		unsigned int en:1;
		unsigned int ctrl:1;
		unsigned int pll2fbd:9;
		unsigned int pll2refd:5;
		unsigned int reserved2:8;
	} b;
	unsigned int v;
};

union pmum_pll3cr {
	struct {
		unsigned int pll3refd:5;
		unsigned int pll3fbd:9;
		unsigned int reserved0:4;
		unsigned int pclk_1248_sel:1;
		unsigned int pll3_pu:1;
		unsigned int reserved1:12;
	} b;
	unsigned int v;
};

union apb_spare_pllswcr {
	struct {
		unsigned int lineupen:1;
		unsigned int gatectl:1;
		unsigned int bypassen:1;
		unsigned int diffclken:1;
		unsigned int divselse:4;
		unsigned int divseldiff:4;
		unsigned int ctune:2;
		unsigned int vcovnrg:3;
		unsigned int kvco:4;
		unsigned int icp:3;
		unsigned int vreg_ivreg:2;
		unsigned int vddl:4;
		unsigned int vddm:2;
	} b;
	unsigned int v;
};

/*
 * peripheral clock source:
 * 0x0 = PLL1 416 MHz
 * 0x1 = PLL1 624 MHz
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
enum periph_clk_src {
	CLK_PLL1_416 = 0x0,
	CLK_PLL1_624 = 0x1,
	CLK_PLL2 = 0x2,
	CLK_PLL2P = 0x3,
};

struct pll_post_div {
	unsigned int div;	/* PLL divider value */
	unsigned int divselval;	/* PLL corresonding reg setting */
};

#define APB_SPARE_PLL2CR	(APB_VIRT_BASE + 0x90104)
#define APB_SPARE_PLL3CR	(APB_VIRT_BASE + 0x90108)
#define POSR_PLL2_LOCK		(1 << 29)
#define POSR_PLL3_LOCK		(1 << 30)

static DEFINE_SPINLOCK(ccic_lock);
static DEFINE_SPINLOCK(lcd_ci_share_lock);
static DEFINE_SPINLOCK(pll2_lock);
static DEFINE_SPINLOCK(pll3_lock);
static DEFINE_SPINLOCK(gc_lock);
static DEFINE_SPINLOCK(vpu_lock);

#define MHZ	(1000000UL)
#define MHZ_TO_KHZ	(1000)
static unsigned long pll2_vco_default;
static unsigned long pll2_default;
static unsigned long pll2p_default;
static unsigned long pll3_vco_default;
static unsigned long pll3_default;
static unsigned long pll3p_default;

#ifdef CONFIG_DEBUG_FS
static LIST_HEAD(clk_dcstat_list);
#endif

/* PLL post divider table */
static struct pll_post_div pll_post_div_tbl[] = {
	/* divider, reg vaule */
	{1, 0},
	{2, 2},
	{3, 4},
	{4, 5},
	{6, 7},
	{8, 8},
};

#define CLK_SET_BITS(set, clear)	{	\
	unsigned long tmp;			\
	tmp = __raw_readl(clk->clk_rst);	\
	tmp &= ~(clear);			\
	tmp |= (set);				\
	__raw_writel(tmp, clk->clk_rst);	\
}						\

void sdh_clk_dump(struct clk *clk)
{
	printk("clk_ctl: %x\n", __raw_readl(clk->clk_rst));
}
EXPORT_SYMBOL_GPL(sdh_clk_dump);

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
	.name = "pll1_624",
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

static int apbc_clk_enable(struct clk *clk)
{
	unsigned int data;

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
	unsigned int data;

	data = __raw_readl(clk->clk_rst) & ~(APBC_FNCLK | APBC_FNCLKSEL(7));
	__raw_writel(data, clk->clk_rst);
	udelay(10);

	data &= ~APBC_APBCLK;
	__raw_writel(data, clk->clk_rst);
}

struct clkops apbc_clk_ops = {
	.enable		= apbc_clk_enable,
	.disable	= apbc_clk_disable,
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

/* convert post div reg setting to divider val */
static unsigned int __pll_divsel2div(unsigned int divselval)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pll_post_div_tbl); i++) {
		if (divselval == pll_post_div_tbl[i].divselval)
			return pll_post_div_tbl[i].div;
	}
	BUG_ON(i == ARRAY_SIZE(pll_post_div_tbl));
	return 0;
}

/* PLL range 1.2G~2.5G, vco_vrng = kvco */
static void __clk_pll_rate2rng(unsigned long rate,
	unsigned int *kvco, unsigned int *vco_rng)
{
	if (rate >= 2400 && rate <= 2500)
		*kvco = 7;
	else if (rate >= 2150)
		*kvco = 6;
	else if (rate >= 1950)
		*kvco = 5;
	else if (rate >= 1750)
		*kvco = 4;
	else if (rate >= 1550)
		*kvco = 3;
	else if (rate >= 1350)
		*kvco = 2;
	else if (rate >= 1200)
		*kvco = 1;
	else
		pr_info("%s rate %lu out of range!\n",
			__func__, rate);

	*vco_rng = *kvco;
}

static unsigned int __clk_pll_calc_div(unsigned long rate,
	unsigned long parent_rate, unsigned int *div)
{
	unsigned int i;
	*div = 0;

	rate /= MHZ;
	parent_rate /= MHZ;

	for (i = 0; i < ARRAY_SIZE(pll_post_div_tbl); i++) {
		if (rate == (parent_rate / pll_post_div_tbl[i].div)) {
			*div = pll_post_div_tbl[i].div;
			return pll_post_div_tbl[i].divselval;
		}
	}
	BUG_ON(i == ARRAY_SIZE(pll_post_div_tbl));
	return 0;
}

static unsigned int __pll2_is_enabled(void)
{
	union pmum_pll2cr pll2cr;
	pll2cr.v = __raw_readl(MPMU_PLL2CR);

	/* ctrl = 0(hw enable) or ctrl = 1&&en = 1(sw enable) */
	/* ctrl = 1&&en = 0(sw disable) */
	if (pll2cr.b.ctrl && (!pll2cr.b.en))
		return 0;
	else
		return 1;
}

/* frequency unit Mhz, return pll2 vco freq */
static unsigned int __get_pll2_freq(unsigned int *pll2_freq,
			unsigned int *pll2p_freq)
{
	union pmum_pll2cr pll2cr;
	union apb_spare_pllswcr pll2_sw_ctl;
	unsigned int pll2_vco, pll2_div, pll2p_div, pll2refd;

	/* return 0 if pll2 is disabled(ctrl = 1, en = 0) */
	if (!__pll2_is_enabled()) {
		pr_info("%s PLL2 is not enabled!\n", __func__);
		*pll2_freq = 0;
		*pll2p_freq = 0;
		return 0;
	}

	pll2cr.v = __raw_readl(MPMU_PLL2CR);
	pll2refd = pll2cr.b.pll2refd;
	BUG_ON(pll2refd == 1);

	if (pll2refd == 0)
		pll2refd = 1;
	pll2_vco = DIV_ROUND_UP(26 * pll2cr.b.pll2fbd, pll2refd);

	pll2_sw_ctl.v = __raw_readl(APB_SPARE_PLL2CR);
	pll2_div = __pll_divsel2div(pll2_sw_ctl.b.divselse);
	pll2p_div = __pll_divsel2div(pll2_sw_ctl.b.divseldiff);
	*pll2_freq = pll2_vco / pll2_div;
	*pll2p_freq = pll2_vco / pll2p_div;

	return pll2_vco;
}

/*
 * 1. Whenever PLL2 is enabled, ensure it's set as HW activation.
 * 2. When PLL2 is disabled (no one uses PLL2 as source),
 * set it as SW activation.
 */
static void clk_pll2_vco_init(struct clk *clk)
{
	unsigned int pll2, pll2p, pll2vco;
	unsigned int pll2_rngl, pll2_rngh, tmp;

	BUG_ON(!pll2_vco_default);
	clk->rate = pll2_vco_default;
	if (__pll2_is_enabled()) {
		pll2vco = __get_pll2_freq(&pll2, &pll2p);
		pr_info("PLL2_VCO is already enabled @ %lu, Expected @ %lu\n",
			pll2vco * MHZ, pll2_vco_default);
		/* check whether pll2 is in the range of 2% our expectation */
		tmp = pll2_vco_default / MHZ;
		if (tmp != pll2vco) {
			pll2_rngh = tmp + tmp * 2 / 100;
			pll2_rngl = tmp - tmp * 2 / 100;
			BUG_ON(!((pll2_rngl <= pll2vco) && \
				(pll2vco <= pll2_rngh)));
		}
		return;
	}
	pr_info("PLL2 VCO default rate %lu\n", clk->rate);
}

static int clk_pll2_vco_enable(struct clk *clk)
{
	union pmum_pll2cr pll2cr;
	unsigned long flags;
	unsigned int delaytime = 14;

	if (__pll2_is_enabled())
		return 0;

	spin_lock_irqsave(&pll2_lock, flags);

	pll2cr.v = __raw_readl(MPMU_PLL2CR);
	/* we must lock refd/fbd first before enabling PLL2 */
	pll2cr.b.ctrl = 1;
	__raw_writel(pll2cr.v, MPMU_PLL2CR);
	pll2cr.b.ctrl = 0;	/* Let HW control PLL2 */
	__raw_writel(pll2cr.v, MPMU_PLL2CR);

	spin_unlock_irqrestore(&pll2_lock, flags);

	if (cpu_pxa98x_stepping() >= PXA98X_A0) {
		udelay(30);
		while ((!(__raw_readl(MPMU_POSR) & POSR_PLL2_LOCK)) &&\
			delaytime) {
			udelay(5); delaytime--;
		}
		if (unlikely(!delaytime))
			BUG_ON("PLL2 is NOT locked after 100us enable!\n");
	} else
		udelay(100);

	trace_pxa_pll_vco_enable(2, __raw_readl(APB_SPARE_PLL2CR), pll2cr.v);
	pr_debug("%s SWCR[%x] PLLCR[%x]\n", __func__, \
		__raw_readl(APB_SPARE_PLL2CR), pll2cr.v);
	return 0;
}

static void clk_pll2_vco_disable(struct clk *clk)
{
	union pmum_pll2cr pll2cr;

	/* For Z1/2 safe PP solution, never shutdown pll2 */
	if (cpu_is_z1z2())
		return;

	pr_debug("Disable pll2 as it is not used!\n");
	trace_pxa_pll_vco_disable(2);

	pll2cr.v = __raw_readl(MPMU_PLL2CR);
	pll2cr.b.ctrl = 1;	/* Let SW control PLL2 */
	pll2cr.b.en = 0;	/* disable PLL2 by en bit */
	__raw_writel(pll2cr.v, MPMU_PLL2CR);
}

/*
 * pll2 rate change requires sequence:
 * clock off -> change rate setting -> clock on
 * This function doesn't really change rate, but cache the config
 */
static int clk_pll2_vco_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int kvco, vcovnrg;
	union pmum_pll2cr pll2cr;
	union apb_spare_pllswcr pll2_sw_ctrl;
	unsigned long flags, cur_rate;

	if (__pll2_is_enabled()) {
		pr_info("%s pll2 vco is enabled, tgt rate %lu\n",\
			__func__, rate);
		return -EPERM;
	}

	rate = rate / MHZ;
	if (rate > 2500 || rate < 1200)	{
		pr_err("%lu rate out of range!\n", rate);
		return -EINVAL;
	}

	pr_debug("PLL2_VCO rate %lu -> %lu\n",
		clk->rate/MHZ, rate);

	spin_lock_irqsave(&pll2_lock, flags);
	__clk_pll_rate2rng(rate, &kvco, &vcovnrg);

	/* The default configuration of pll2 */
	pll2_sw_ctrl.v = __raw_readl(APB_SPARE_PLL2CR);
	pll2_sw_ctrl.b.vddm = 1;
	pll2_sw_ctrl.b.vddl = 9;
	pll2_sw_ctrl.b.vreg_ivreg = 2;
	pll2_sw_ctrl.b.icp = 4;
	pll2_sw_ctrl.b.ctune = 1;
	pll2_sw_ctrl.b.bypassen = 0;
	pll2_sw_ctrl.b.gatectl = 0;
	pll2_sw_ctrl.b.lineupen = 0;
	pll2_sw_ctrl.b.diffclken = 1;
	pll2_sw_ctrl.b.kvco = kvco;
	pll2_sw_ctrl.b.vcovnrg = vcovnrg;
	__raw_writel(pll2_sw_ctrl.v, APB_SPARE_PLL2CR);

	/* Refclk/Refdiv = pll2freq/Fbdiv Refclk = 26M */
	pll2cr.v = __raw_readl(MPMU_PLL2CR);
	pll2cr.b.pll2refd = 3;
	pll2cr.b.pll2fbd = rate * pll2cr.b.pll2refd / 26;
	pll2cr.b.ctrl = 1;
	pll2cr.b.en = 0;
	__raw_writel(pll2cr.v, MPMU_PLL2CR);

	clk->rate = rate * MHZ;
	spin_unlock_irqrestore(&pll2_lock, flags);

	cur_rate = 26 * pll2cr.b.pll2fbd / pll2cr.b.pll2refd;
	if (cur_rate != rate)
		pr_warning("Real PLL2 rate %luMHZ\n", cur_rate);
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
	unsigned int pll2, pll2p;

	BUG_ON(!pll2_default);
	clk->rate = pll2_default;
	if (__pll2_is_enabled()) {
		__get_pll2_freq(&pll2, &pll2p);
		pr_info("PLL2 is already enabled @ %lu\n",
			pll2 * MHZ);
		return;
	}
	pr_info("PLL2 default rate %lu\n", clk->rate);
}

static int clk_pll2_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int div_val;
	union apb_spare_pllswcr pll2_sw_ctrl;
	unsigned long flags;

	if (__pll2_is_enabled())
		return -EPERM;

	pr_debug("PLL2 rate %lu -> %lu\n", clk->rate, rate);

	spin_lock_irqsave(&pll2_lock, flags);
	div_val = __clk_pll_calc_div(rate, clk->parent->rate, &clk->div);
	pll2_sw_ctrl.v = __raw_readl(APB_SPARE_PLL2CR);
	pll2_sw_ctrl.b.divselse = div_val;
	__raw_writel(pll2_sw_ctrl.v, APB_SPARE_PLL2CR);
	clk->rate = rate;
	spin_unlock_irqrestore(&pll2_lock, flags);
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
	unsigned int pll2, pll2p;

	BUG_ON(!pll2p_default);
	clk->rate = pll2p_default;
	if (__pll2_is_enabled()) {
		__get_pll2_freq(&pll2, &pll2p);
		pr_info("PLL2P is already enabled @ %lu\n",
			pll2p * MHZ);
		return;
	}
	pr_info("PLL2P default rate %lu\n", clk->rate);
}

static int clk_pll2p_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int div_val;
	union apb_spare_pllswcr pll2_sw_ctrl;
	unsigned long flags;

	if (__pll2_is_enabled())
		return -EPERM;

	pr_debug("PLL2P rate %lu -> %lu\n", clk->rate, rate);

	spin_lock_irqsave(&pll2_lock, flags);
	div_val = __clk_pll_calc_div(rate, clk->parent->rate, &clk->div);
	pll2_sw_ctrl.v = __raw_readl(APB_SPARE_PLL2CR);
	pll2_sw_ctrl.b.divseldiff = div_val;
	__raw_writel(pll2_sw_ctrl.v, APB_SPARE_PLL2CR);
	clk->rate = rate;
	spin_unlock_irqrestore(&pll2_lock, flags);
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

static unsigned int __pll3_is_enabled(void)
{
	union pmum_pll3cr pll3cr;
	pll3cr.v = __raw_readl(MPMU_PLL3CR);

	/*
	 * PLL3CR[19:18] = 0x1, 0x2, 0x3 means PLL3 is enabled.
	 * PLL3CR[19:18] = 0x0 means PLL3 is disabled
	 */
	if ((!pll3cr.b.pll3_pu) && (!pll3cr.b.pclk_1248_sel))
		return 0;
	else
		return 1;
}

/* frequency unit Mhz, return pll3 vco freq */
static unsigned int __get_pll3_freq(unsigned int *pll3_freq,
	unsigned int *pll3p_freq)
{
	union pmum_pll3cr pll3cr;
	union apb_spare_pllswcr pll3_sw_ctl;
	unsigned int pll3_vco, pll3_div, pll3p_div, pll3refd;

	/* return 0 if pll3 is disabled */
	if (!__pll3_is_enabled()) {
		pr_info("%s PLL3 is not enabled!\n", __func__);
		*pll3_freq = 0;
		*pll3p_freq = 0;
		return 0;
	}

	pll3cr.v = __raw_readl(MPMU_PLL3CR);
	pll3refd = pll3cr.b.pll3refd;
	BUG_ON(pll3refd == 1);
	if (pll3refd == 0)
		pll3refd = 1;
	pll3_vco = 26 * pll3cr.b.pll3fbd / pll3refd;

	pll3_sw_ctl.v = __raw_readl(APB_SPARE_PLL3CR);
	pll3_div = __pll_divsel2div(pll3_sw_ctl.b.divselse);
	pll3p_div = __pll_divsel2div(pll3_sw_ctl.b.divseldiff);
	*pll3_freq = pll3_vco / pll3_div;
	*pll3p_freq = pll3_vco / pll3p_div;

	return pll3_vco;
}

/* FIXME: default pll3_vco 2000M, pll3 500M(dsi), pll3p 1000M(cpu) */
static void clk_pll3_vco_init(struct clk *clk)
{
	unsigned int pll3, pll3p, pll3vco;

	BUG_ON(!pll3_vco_default);
	clk->rate = pll3_vco_default;
	if (__pll3_is_enabled()) {
		pll3vco = __get_pll3_freq(&pll3, &pll3p);
		pr_info("PLL3_VCO is already enabled @ %lu\n",
			pll3vco * MHZ);
		return;
	}
	pr_info("PLL3 VCO default rate %lu\n", clk->rate);
}

/* PLL3CR[19:18] = 0x0 shutdown */
/* PLL3CR[19:18] = 0x3 enable */
static int clk_pll3_vco_enable(struct clk *clk)
{
	union pmum_pll3cr pll3cr;
	unsigned long flags;
	unsigned int delaytime = 14;

	if (__pll3_is_enabled())
		return 0;

	spin_lock_irqsave(&pll3_lock, flags);
	pll3cr.v = __raw_readl(MPMU_PLL3CR);
	pll3cr.b.pclk_1248_sel = 1;
	pll3cr.b.pll3_pu = 1;
	__raw_writel(pll3cr.v, MPMU_PLL3CR);
	spin_unlock_irqrestore(&pll3_lock, flags);

	if (cpu_pxa98x_stepping() >= PXA98X_A0) {
		udelay(30);
		while ((!(__raw_readl(MPMU_POSR) & POSR_PLL3_LOCK)) &&\
			delaytime) {
			udelay(5); delaytime--;
		}
		if (unlikely(!delaytime))
			BUG_ON("PLL3 is NOT locked after 100us enable!\n");
	} else
		udelay(100);

	trace_pxa_pll_vco_enable(3, __raw_readl(APB_SPARE_PLL3CR), pll3cr.v);
	pr_debug("%s SWCR3[%x] PLL3CR[%x]\n", __func__, \
		__raw_readl(APB_SPARE_PLL3CR), pll3cr.v);
	return 0;
}

static void clk_pll3_vco_disable(struct clk *clk)
{
	union pmum_pll3cr pll3cr;

	pr_debug("Disable pll3 as it is not used!\n");
	trace_pxa_pll_vco_disable(3);

	pll3cr.v = __raw_readl(MPMU_PLL3CR);
	pll3cr.b.pclk_1248_sel = 0;
	pll3cr.b.pll3_pu = 0;
	__raw_writel(pll3cr.v, MPMU_PLL3CR);
}

static int clk_pll3_vco_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int kvco, vcovnrg;
	union pmum_pll3cr pll3cr;
	union apb_spare_pllswcr pll3_sw_ctrl;
	unsigned long flags, cur_rate;

	/* do nothing if pll3 is already enabled or no rate change */
	if (__pll3_is_enabled()) {
		pr_info("%s pll3 vco is enabled, tgt rate %lu\n",\
			__func__, rate);
		return -EPERM;
	}

	rate = rate / MHZ;
	if (rate > 2500 || rate < 1200)	{
		pr_err("%lu rate out of range!\n", rate);
		return -EINVAL;
	}

	pr_debug("PLL3_VCO rate %lu -> %lu\n",
		clk->rate/MHZ, rate);

	spin_lock_irqsave(&pll3_lock, flags);
	__clk_pll_rate2rng(rate, &kvco, &vcovnrg);

	/* The default configuration of pll3 */
	pll3_sw_ctrl.v = __raw_readl(APB_SPARE_PLL3CR);
	pll3_sw_ctrl.b.vddm = 1;
	pll3_sw_ctrl.b.vddl = 9;
	pll3_sw_ctrl.b.vreg_ivreg = 2;
	pll3_sw_ctrl.b.icp = 4;
	pll3_sw_ctrl.b.ctune = 1;
	pll3_sw_ctrl.b.bypassen = 0;
	pll3_sw_ctrl.b.gatectl = 0;
	pll3_sw_ctrl.b.lineupen = 0;
	pll3_sw_ctrl.b.kvco = kvco;
	pll3_sw_ctrl.b.vcovnrg = vcovnrg;
	pll3_sw_ctrl.b.diffclken = 1;
	__raw_writel(pll3_sw_ctrl.v, APB_SPARE_PLL3CR);

	/* Refclk/Refdiv = pll2freq/Fbdiv Refclk = 26M */
	pll3cr.v = __raw_readl(MPMU_PLL3CR);
	pll3cr.b.pll3refd = 3;
	pll3cr.b.pll3fbd = rate * pll3cr.b.pll3refd / 26;
	pll3cr.b.pclk_1248_sel = 0;
	pll3cr.b.pll3_pu = 0;
	__raw_writel(pll3cr.v, MPMU_PLL3CR);

	clk->rate = rate * MHZ;
	spin_unlock_irqrestore(&pll3_lock, flags);

	cur_rate = 26 * pll3cr.b.pll3fbd / pll3cr.b.pll3refd;
	if (cur_rate != rate)
		pr_warning("Real PLL3 rate %luMHZ\n", cur_rate);
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
	unsigned int pll3, pll3p;

	BUG_ON(!pll3_default);
	clk->rate = pll3_default;
	if (__pll3_is_enabled()) {
		__get_pll3_freq(&pll3, &pll3p);
		pr_info("PLL3 is already enabled @ %lu\n",
			pll3 * MHZ);
		return;
	}
	pr_info("PLL3 default rate %lu\n", clk->rate);
}

static int clk_pll3_setrate(struct clk *clk, unsigned long rate)
{
	union apb_spare_pllswcr pll3_sw_ctrl;
	unsigned int div_val;
	unsigned long flags;

	if (__pll3_is_enabled())
		return -EPERM;

	pr_debug("PLL3 rate %lu -> %lu\n", clk->rate, rate);

	spin_lock_irqsave(&pll3_lock, flags);
	div_val = __clk_pll_calc_div(rate, clk->parent->rate, &clk->div);
	pll3_sw_ctrl.v = __raw_readl(APB_SPARE_PLL3CR);
	pll3_sw_ctrl.b.divselse = div_val;
	__raw_writel(pll3_sw_ctrl.v, APB_SPARE_PLL3CR);
	clk->rate = rate;
	spin_unlock_irqrestore(&pll3_lock, flags);
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
	unsigned int pll3, pll3p;

	BUG_ON(!pll3p_default);
	clk->rate = pll3p_default;
	if (__pll3_is_enabled()) {
		__get_pll3_freq(&pll3, &pll3p);
		pr_info("PLL3P is already enabled @ %lu\n",
			pll3p * MHZ);
		return;
	}
	pr_info("PLL3P default rate %lu\n", clk->rate);
}

static int clk_pll3p_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int div_val;
	union apb_spare_pllswcr pll3_sw_ctrl;
	unsigned long flags;

	if (__pll3_is_enabled())
		return -EPERM;

	pr_debug("PLL3P rate %lu -> %lu\n", clk->rate, rate);

	spin_lock_irqsave(&pll3_lock, flags);
	div_val = __clk_pll_calc_div(rate, clk->parent->rate, &clk->div);
	pll3_sw_ctrl.v = __raw_readl(APB_SPARE_PLL3CR);
	pll3_sw_ctrl.b.divseldiff = div_val;
	__raw_writel(pll3_sw_ctrl.v, APB_SPARE_PLL3CR);
	clk->rate = rate;
	spin_unlock_irqrestore(&pll3_lock, flags);
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

#define SDH_ACLK_EN		(1 << 3)
/*
 * SDH_ALL_RESET(1<<0) is only valid for APMU_SDH0
 * But clear it can reset all 3 SD Hosts
 * This bit need to be 1 when any SD Host is working
 */
#define SDH_ALL_RESET	(1 << 0)
#define SDH_FCLK_EN		((1 << 4) | (1 << 1))
#define SDH_FCLK_SEL(n)		((n & 0x1) << 6)
#define SDH_FCLK_SEL_MASK	SDH_FCLK_SEL(0x1)
#define SDH_FCLK_DIV(n)		((n & 0x7) << 8)
#define SDH_FCLK_DIV_MASK	SDH_FCLK_DIV(0x7)
#define SDH_FCLK_REQ		(1 << 11)
#define SDH_CLK_RATE_MASK	\
	(SDH_FCLK_SEL_MASK | SDH_FCLK_DIV_MASK)

/* dummy clock used for 3 SDH shared AXI bus enable */
DEFINE_GATE_CLK(sdh_shared_axi, APMU_SDH0, SDH_ACLK_EN, NULL, "sdh_shared_axi");

static struct clk *sdhc_share_clk[] = {
	&sdh_shared_axi,
};

static struct clk_mux_sel sdhc_clk_mux[] = {
	{.input = &pll1_416, .value = CLK_PLL1_416},
	{.input = &pll1_624, .value = CLK_PLL1_624},
	{NULL, 0},
};

static void sdhc_reset_all(void)
{
	unsigned int reg_tmp;

	reg_tmp = __raw_readl(APMU_SDH0);
	/* set SDH_ALL_RESET bit to 0: reset all SD Hosts is enabled */
	__raw_writel(reg_tmp & (~SDH_ALL_RESET), APMU_SDH0);
	udelay(10);
	/* set SDH_ALL_RESET bit to 1: reset all SD Hosts is disabled */
	__raw_writel(reg_tmp | SDH_ALL_RESET, APMU_SDH0);
}

static void sdhc_clk_init(struct clk *clk)
{
	unsigned int mux, i;

	clk->mul = 1;
	clk->div = (clk->enable_val & SDH_FCLK_DIV_MASK) >> \
		(__ffs(SDH_FCLK_DIV_MASK));
	mux = (clk->enable_val & SDH_FCLK_SEL_MASK) >> \
		(__ffs(SDH_FCLK_SEL_MASK));

	i = 0;
	while ((clk->inputs[i].input) && (clk->inputs[i].value != mux))
		i++;
	BUG_ON(!clk->inputs[i].input);

	clk_reparent(clk, clk->inputs[i].input);
	clk->div += 1;
	clk->rate = clk_get_rate(clk->parent) * clk->mul / clk->div;
	CLK_SET_BITS(clk->enable_val, SDH_CLK_RATE_MASK);
	clk->enable_val = SDH_FCLK_REQ;
	clk->dynamic_change = 1;
}

static int sdhc_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(SDH_FCLK_EN, 0);
	CLK_SET_BITS(SDH_FCLK_REQ, 0);
	return 0;
}

static void sdhc_clk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, SDH_FCLK_EN);

}

static int sdhc_clk_setrate(struct clk *clk, unsigned long rate)
{
	__clk_periph_set_rate(clk, rate);
	return 0;
}

static unsigned long sdhc_clk_getrate(struct clk *clk)
{
	return __clk_periph_get_rate(clk);
}

struct clkops sdhc_clk_ops = {
	.init = sdhc_clk_init,
	.enable = sdhc_clk_enable,
	.disable = sdhc_clk_disable,
	.setrate = sdhc_clk_setrate,
	.getrate = sdhc_clk_getrate,
};

static struct clk pxa988_clk_sdh0 = {
	.name = "sdh0",
	.lookup = {
		.dev_id = "sdhci-pxav3.0",
		.con_id = "PXA-SDHCLK",
	},
	.clk_rst = (void __iomem *)APMU_SDH0,
	/* SDcard 208M */
	.enable_val =
		SDH_FCLK_SEL(CLK_PLL1_416)|SDH_FCLK_DIV(1)|SDH_FCLK_REQ,
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.dependence = sdhc_share_clk,
	.dependence_count = 1,
	.reg_data = {
		{ {APMU_SDH0, 6, 0x1}, {APMU_SDH0, 6, 0x1} },
		{ {APMU_SDH0, 8, 0x7}, {APMU_SDH0, 8, 0x7} }
	}
};

static struct clk pxa988_clk_sdh1 = {
	.name = "sdh1",
	.lookup = {
		.dev_id = "sdhci-pxav3.1",
		.con_id = "PXA-SDHCLK",
	},
	.clk_rst = (void __iomem *)APMU_SDH1,
	/* Wifi 89.1M */
	.enable_val =
		SDH_FCLK_SEL(CLK_PLL1_624)|SDH_FCLK_DIV(6)|SDH_FCLK_REQ,
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.dependence = sdhc_share_clk,
	.dependence_count = 1,
	.reg_data = {
		{ {APMU_SDH1, 6, 0x1}, {APMU_SDH1, 6, 0x1} },
		{ {APMU_SDH1, 8, 0x7}, {APMU_SDH1, 8, 0x7} }
	}
};

static struct clk pxa988_clk_sdh2 = {
	.name = "sdh2",
	.lookup = {
		.dev_id = "sdhci-pxav3.2",
		.con_id = "PXA-SDHCLK",
	},
	.clk_rst = (void __iomem *)APMU_SDH2,
	/* Emmc 208M */
	.enable_val =
		SDH_FCLK_SEL(CLK_PLL1_416)|SDH_FCLK_DIV(1)|SDH_FCLK_REQ,
	.ops = &sdhc_clk_ops,
	.inputs = sdhc_clk_mux,
	.dependence = sdhc_share_clk,
	.dependence_count = 1,
	.reg_data = {
		{ {APMU_SDH2, 6, 0x1}, {APMU_SDH2, 6, 0x1} },
		{ {APMU_SDH2, 8, 0x7}, {APMU_SDH2, 8, 0x7} }
	}
};

static struct clk_mux_sel periph_mux_sel[] = {
	{.input = &pll1_416, .value = CLK_PLL1_416},
	{.input = &pll1_624, .value = CLK_PLL1_624},
	{.input = &pll2, .value = CLK_PLL2},
	{.input = &pll2p, .value = CLK_PLL2P},
	{NULL, 0},
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
				if ((prate % clk_tbl[i].clk_rate) || !prate)
					break;
				clk_tbl[i].src_val = sel->value;
				clk_tbl[i].div_val =
					prate / clk_tbl[i].clk_rate - 1;
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

/*
 * This help function can get the rate close to the required rate,
 * we'd better not use it for clock which need to dynamic change
 * as efficiency consideration.
 */
static long __clk_sel_mux_div(struct clk *clk, unsigned long rate,
	unsigned int *mux, unsigned int *div, struct clk **best_parent)
{
	const struct clk_mux_sel *sel;
	struct clk *parent_selh = NULL, *parent_sell = NULL;
	unsigned long i, parent_rate, now, maxdiv;
	unsigned long bestl = 0, besth = ULONG_MAX;
	unsigned int bestdivl = 0, bestmuxl = 0, bestdivh = 0, bestmuxh = 0;

	maxdiv = clk->reg_data[DIV][CONTROL].reg_mask + 1;
	if (rate < (clk_get_rate(clk->inputs[0].input) / maxdiv))
		rate = clk_get_rate(clk->inputs[0].input) / maxdiv;
	/*
	 * The maximum divider we can use without overflowing
	 * unsigned long in rate * i below
	 */
	maxdiv = min(ULONG_MAX / rate, maxdiv);
	for (sel = clk->inputs; sel->input != NULL; sel++) {
		parent_rate = clk_get_rate(sel->input);
		for (i = 1; i <= maxdiv; i++) {
			now = parent_rate / i;
			/* condition to select a best closest rate >= rate */
			if (now >= rate && now < besth) {
				bestdivh = i;
				besth = now;
				parent_selh = sel->input;
				bestmuxh = sel->value;
			/* condition to select a best closest rate <= rate */
			} else if (now <= rate && now > bestl) {
				bestdivl = i;
				bestl = now;
				parent_sell = sel->input;
				bestmuxl = sel->value;
			}
		}
	}

	now = ((besth - rate) <= (rate - bestl)) ? besth : bestl;
	if (now == besth) {
		*div = bestdivh;
		*mux = bestmuxh;
		*best_parent = parent_selh;
	} else {
		*div = bestdivl;
		*mux = bestmuxl;
		*best_parent = parent_sell;
	}
	BUG_ON(!(*div));

	pr_debug("%s clk:%s mux:%u div:%u, %lu\n", __func__, \
		clk->name, *mux, *div, clk_get_rate(*best_parent));

	return clk_get_rate(*best_parent)/(*div);
}

static void __clk_wait_fc_done(struct clk *clk)
{
	/* fc done should be asserted in several clk cycles */
	unsigned int i = 50;
	udelay(2);
	while ((__raw_readl(clk->reg_data[SOURCE][CONTROL].reg)\
		& clk->enable_val) && i) {
		udelay(10);
		i--;
	}
	if (i == 0) {
		pr_info("CLK[%s] fc req failed![%x]= 0x%x,"\
			"fc_req = 0x%x\n", clk->name,
			(unsigned int)clk->reg_data[SOURCE][CONTROL].reg,
			__raw_readl(clk->reg_data[SOURCE][CONTROL].reg),
			clk->enable_val);
		BUG_ON(1);
	}
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

	div = div - 1;	/* rate = parent_rate / (div_regval + 1) */

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

	/*
	 * mux and div are from the same reg, if clk is enabled,
	 * set mux, div and trigger(clk->enable_val) at the same time.
	 * If clock is disabled, we still set mux, div and fc_request here.
	 */
	regval = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);
	regval &= ~(muxmask | divmask);
	regval |= (muxval | divval);

	/*
	 * For smooth mux clock src switch, must make sure both clocks on
	 * or smooth mux can not finish clock switch.
	 */
	clk_enable(best_parent);
	if (!clk->refcnt && clk->parent)
		clk_enable(clk->parent);
	regval |= clk->enable_val;
	__raw_writel(regval, clk->reg_data[SOURCE][CONTROL].reg);
	__clk_wait_fc_done(clk);

	/*
	 * disable its parent if clk change done, no matter clk is enabled
	 * or not
	 */
	if (clk->parent)
		clk_disable(clk->parent);
	if (!clk->refcnt)
		clk_disable(best_parent);
	clk_reparent(clk, best_parent);

	pr_debug("\n%s clk:%s [%x] = [%x]\n", __func__, clk->name, \
		(unsigned int)clk->reg_data[SOURCE][CONTROL].reg, regval);

	return 0;
}

static void __clk_get_mux_div(struct clk *clk,
		unsigned int *mux, unsigned int *div)
{
	unsigned int muxmask, divmask;
	unsigned int muxval, divval;
	unsigned int regval;

	regval = __raw_readl(clk->reg_data[SOURCE][CONTROL].reg);
	muxmask = clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;
	divmask = clk->reg_data[DIV][CONTROL].reg_mask << \
		clk->reg_data[DIV][CONTROL].reg_shift;

	muxval = (regval & muxmask) >> __ffs(muxmask);
	divval = (regval & divmask) >> __ffs(divmask);

	pr_debug("\n%s clk:%s [%x]val%x mux[%d] div[%d]\n", __func__,
		clk->name, (unsigned int)clk->reg_data[SOURCE][CONTROL].reg,
		regval, muxval, divval+1);

	*mux = muxval;
	*div = divval + 1;
}

static struct clk *__clk_mux_to_parent(struct clk *clk, unsigned int mux)
{
	unsigned int i;

	i = 0;
	while ((clk->inputs[i].input) && (clk->inputs[i].value != mux))
		i++;
	BUG_ON(!clk->inputs[i].input);

	return clk->inputs[i].input;
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

static unsigned long __clk_periph_get_rate(struct clk *clk)
{
	struct clk *cur_parent;
	unsigned int mux, div = 1;

	__clk_get_mux_div(clk, &mux, &div);
	cur_parent = __clk_mux_to_parent(clk, mux);

	return clk_get_rate(cur_parent) / div;
}

static int __clk_periph_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long new_rate;
	unsigned int mux, div;
	struct clk *best_parent;

	new_rate = __clk_sel_mux_div(clk, rate, &mux, &div, &best_parent);
	if (rate != new_rate)
		pr_warning("clk[%s] required rate %lu, set as %lu\n", \
			clk->name, rate, new_rate);
	__clk_set_mux_div(clk, best_parent, mux, div);
	return 0;
}

#ifdef Z1_MCK4_SYNC_WORKAROUND
int mck4_wr_enabled = 1;
static int __init mck4_wr_disable(char *str)
{
	mck4_wr_enabled = 0;
	return 1;
}
__setup("no_mck4_wr", mck4_wr_disable);

static LIST_HEAD(ddr_combined_clk_list);

struct ddr_combined_clk {
	struct clk *clk;
	unsigned long maxrate;
	struct list_head node;
};

static int register_clk_bind2ddr(struct clk *clk,
	unsigned long max_freq)
{
	struct ddr_combined_clk *comclk;

	/* search the list of the registation for this clk */
	list_for_each_entry(comclk, &ddr_combined_clk_list, node)
		if (comclk->clk == clk)
			break;

	/* if clk wasn't in the list, allocate new dcstat info */
	if (comclk->clk != clk) {
		comclk = kzalloc(sizeof(struct ddr_combined_clk), GFP_KERNEL);
		if (!comclk)
			return -ENOMEM;

		comclk->clk = clk;
		comclk->maxrate = max_freq;
		list_add(&comclk->node, &ddr_combined_clk_list);
	}
	return 0;
}

int trigger_bind2ddr_clk_rate(unsigned long ddr_rate)
{
	struct ddr_combined_clk *comclk;
	unsigned long tgt, cur;
	int ret = 0;

	if (!mck4_wr_enabled)
		return 0;

	list_for_each_entry(comclk, &ddr_combined_clk_list, node) {
		if (ddr_rate > comclk->maxrate)
			tgt = ddr_rate / 2;
		else
			tgt = ddr_rate;
		pr_debug("%s Start rate change to %lu\n",
			comclk->clk->name, tgt);
		ret = clk_set_rate(comclk->clk, tgt);
		if (ret) {
			pr_info("%s failed to change clk %s rate\n",
				__func__, comclk->clk->name);
			return ret;
		}
		cur = clk_get_rate(comclk->clk);
		if (cur != tgt) {
			pr_info("clk %s: cur %lu, tgt %lu, reg[%x]\n",
					comclk->clk->name, cur, tgt,
					__raw_readl(comclk->clk->clk_rst));
			WARN_ON(1);
		}
	}

	return ret;
}
#endif

#define GC_ACLK_EN	(0x1 << 3)
#define GC_FCLK_EN	(0x1 << 4)
#define GC_HCLK_EN	(0x1 << 5)

#define GC_FCLK_SEL(n)		((n & 0x3) << 6)
#define GC_FCLK_SEL_MASK	GC_FCLK_SEL(0x3)
#define GC_FCLK_DIV(n)		((n & 0x7) << 12)
#define GC_FCLK_DIV_MASK	GC_FCLK_DIV(0x7)
#define GC_FCLK_REQ		(0x1 << 15)

#define GC_ACLK_SEL(n)		((n & 0x3) << 20)
#define GC_ACLK_SEL_MASK	GC_ACLK_SEL(0x3)
#define GC_ACLK_DIV(n)		((n & 0x7) << 17)
#define GC_ACLK_DIV_MASK	GC_ACLK_DIV(0x7)
#define GC_ACLK_REQ		(0x1 << 16)

#define GC_FCLK_RATE(fsrc, fdiv)	\
	(GC_FCLK_SEL(fsrc) | GC_FCLK_DIV(fdiv))

#define GC_ACLK_RATE(asrc, adiv)	\
	(GC_ACLK_SEL(asrc) | GC_ACLK_DIV(adiv))

#define GC_FCLK_RATE_MSK				\
	(GC_FCLK_SEL_MASK | GC_FCLK_DIV_MASK)		\

#define GC_ACLK_RATE_MSK				\
	(GC_ACLK_SEL_MASK | GC_ACLK_DIV_MASK)		\


/*
 * 1. sort ascending
 * 2. FIXME: If DDR 533M is used, 400M bus clk could not be
 * supported due to clk src issue.
 * 3. For 988 Z1/Z2, only uses gc aclk 150/300/400
 * 4. For 988 Z3, only uses gc aclk 156/312/416
 * 5. For 1088, temporarily use 988 Z3 table, will be updated
 *    once DE/SV give the table
 */
static struct periph_clk_tbl gc_aclk_tbl[] = {
	{.clk_rate = 150000000, .parent = &pll2},
	{.clk_rate = 156000000, .parent = &pll1_624},
	{.clk_rate = 208000000, .parent = &pll1_416},
	{.clk_rate = 300000000, .parent = &pll2},
	{.clk_rate = 312000000, .parent = &pll1_624},
	{.clk_rate = 400000000, .parent = &pll2p},
	{.clk_rate = 416000000, .parent = &pll1_416},
};

static void gc_aclk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, gc_aclk_tbl,
		ARRAY_SIZE(gc_aclk_tbl));
	/* default GC aclk = 312M sel = pll1_624, div = 2 */
	__clk_periph_init(clk, &pll1_624, 2, 1);
#ifdef Z1_MCK4_SYNC_WORKAROUND
	register_clk_bind2ddr(clk,
		gc_aclk_tbl[ARRAY_SIZE(gc_aclk_tbl) - 1].clk_rate);
#endif
}

static int gc_aclk_enable(struct clk *clk)
{
	spin_lock(&gc_lock);
	CLK_SET_BITS(GC_ACLK_EN, 0);
	CLK_SET_BITS(GC_ACLK_REQ, 0);
	__clk_wait_fc_done(clk);
	spin_unlock(&gc_lock);
	pr_debug("%s GC_ACLK %x\n", __func__, __raw_readl(clk->clk_rst));
	return 0;
}

static void gc_aclk_disable(struct clk *clk)
{
	spin_lock(&gc_lock);
	CLK_SET_BITS(0, GC_ACLK_EN);
	spin_unlock(&gc_lock);
	return;
}

static long gc_aclk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		gc_aclk_tbl, ARRAY_SIZE(gc_aclk_tbl));
}

static int gc_aclk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long old_rate;
	unsigned int i;
	struct clk *new_parent;

	old_rate = clk->rate;
	if (rate == old_rate)
		return 0;

	i = 0;
	while (gc_aclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc_aclk_tbl));

	new_parent = gc_aclk_tbl[i].parent;
	spin_lock(&gc_lock);
	__clk_set_mux_div(clk, new_parent,
		gc_aclk_tbl[i].src_val, (gc_aclk_tbl[i].div_val + 1));
	spin_unlock(&gc_lock);

	pr_debug("%s rate %lu->%lu\n", clk->name, old_rate, rate);
	return 0;
}

static unsigned long gc_aclk_getrate(struct clk *clk)
{
	return __clk_periph_get_rate(clk);
}

struct clkops gc_aclk_ops = {
	.init		= gc_aclk_init,
	.enable		= gc_aclk_enable,
	.disable	= gc_aclk_disable,
	.round_rate	= gc_aclk_round_rate,
	.setrate	= gc_aclk_setrate,
	.getrate	= gc_aclk_getrate,
};

/*
 * GC aclk node is internal clk node, and
 * can only be used by GC fclk
 */
static struct clk gc_aclk = {
	.name = "gc_aclk",
	.lookup = {
		.con_id = "GC_ACLK",
	},
	.clk_rst = (void __iomem *)APMU_GC,
	.enable_val = GC_ACLK_REQ,
	.inputs = periph_mux_sel,
	.ops = &gc_aclk_ops,
	.reg_data = {
		     { {APMU_GC, 20, 0x3}, {APMU_GC, 20, 0x3} },
		     { {APMU_GC, 17, 0x7}, {APMU_GC, 17, 0x7} } }
};

static struct clk *gc_clk_depend[] = {
	&gc_aclk,
};

/* sort ascending */
static struct periph_clk_tbl gc_fclk_tbl_z1z2[] = {
	{
		.clk_rate = 150000000,
		.parent = &pll2,
	},
	{
		.clk_rate = 300000000,
		.parent = &pll2,
	},
	{
		.clk_rate = 400000000,
		.parent = &pll2p,
	},
	{
		.clk_rate = 600000000,
		.parent = &pll2,
	},
};

static struct periph_clk_tbl gc_fclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
	{
		.clk_rate = 312000000,
		.parent = &pll1_624,
		.comclk_rate = 312000000,
	},
	{
		.clk_rate = 416000000,
		.parent = &pll1_416,
		.comclk_rate = 312000000,
	},
	{
		.clk_rate = 624000000,
		.parent = &pll1_624,
		.comclk_rate = 416000000,
	},
};

/* interface used by GC driver to get avaliable GC frequencies, unit HZ */
int get_gcu_freqs_table(unsigned long *gcu_freqs_table,
	unsigned int *item_counts, unsigned int max_item_counts)
{
	unsigned int index;
	*item_counts = 0;

	if (!gcu_freqs_table) {
		pr_err("%s NULL ptr!\n", __func__);
		return -EINVAL;
	}

	if (max_item_counts < ARRAY_SIZE(gc_fclk_tbl)) {
		pr_err("%s Too many GC frequencies %u!\n", __func__,
			max_item_counts);
		return -EINVAL;
	}

	for (index = 0; index < ARRAY_SIZE(gc_fclk_tbl); index++)
		gcu_freqs_table[index] = gc_fclk_tbl[index].clk_rate;
	*item_counts = index;
	return 0;
}
EXPORT_SYMBOL(get_gcu_freqs_table);

/* used for GC LPM constraint */
static struct pm_qos_request gc_lpm_cons;

static void gc_clk_init(struct clk *clk)
{
#ifdef CONFIG_DEBUG_FS
	unsigned int i;
	unsigned long op[ARRAY_SIZE(gc_fclk_tbl)];
#endif
	if (cpu_is_z1z2())
		memcpy(&gc_fclk_tbl, &gc_fclk_tbl_z1z2,
			sizeof(struct periph_clk_tbl) *\
				ARRAY_SIZE(gc_fclk_tbl));

	__clk_fill_periph_tbl(clk, gc_fclk_tbl, ARRAY_SIZE(gc_fclk_tbl));

	if (cpu_is_z1z2())
		/* default GC fclk = 400M sel = pll2 = 1200M, div = 4 */
		__clk_periph_init(clk, &pll2, 4, 1);
	else
		/* default GC fclk = 416M sel = pll1_416, div = 1 */
		__clk_periph_init(clk, &pll1_416, 1, 1);


#ifdef CONFIG_DEBUG_FS
	for (i = 0; i < ARRAY_SIZE(gc_fclk_tbl); i++)
		op[i] = gc_fclk_tbl[i].clk_rate;
	pxa988_clk_register_dcstat(clk, op, ARRAY_SIZE(gc_fclk_tbl));
#endif

	/* initialize the qos list at the first time */
	gc_lpm_cons.name = "GC";
	pm_qos_add_request(&gc_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static int gc_clk_enable(struct clk *clk)
{
	/* block LPM D1P and deeper than D1P */
	pm_qos_update_request(&gc_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);

	spin_lock(&gc_lock);
	CLK_SET_BITS((GC_FCLK_EN | GC_HCLK_EN), 0);
	CLK_SET_BITS(GC_FCLK_REQ, 0);
	__clk_wait_fc_done(clk);
	spin_unlock(&gc_lock);
	trace_pxa_gc_clk(CLK_ENABLE, __raw_readl(clk->clk_rst));
	pr_debug("%s GC_CLK %x\n", __func__, __raw_readl(clk->clk_rst));
#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_STATE_ON, 0);
#endif
	return 0;
}

static void gc_clk_disable(struct clk *clk)
{
	spin_lock(&gc_lock);
	CLK_SET_BITS(0, (GC_FCLK_EN | GC_HCLK_EN));
	spin_unlock(&gc_lock);
	trace_pxa_gc_clk(CLK_DISABLE, __raw_readl(clk->clk_rst));
	pr_debug("%s GC_CLK : %x\n", __func__, __raw_readl(clk->clk_rst));
#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_STATE_OFF, 0);
#endif

	/* release D1P LPM constraint */
	pm_qos_update_request(&gc_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static long gc_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		gc_fclk_tbl, ARRAY_SIZE(gc_fclk_tbl));
}

static int gc_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long old_rate;
	unsigned int i;
	struct clk *new_fparent;

	old_rate = clk->rate;
	if (rate == old_rate)
		return 0;

	i = 0;
	while (gc_fclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(gc_fclk_tbl));

	/* set GC function clk rate */
	new_fparent = gc_fclk_tbl[i].parent;
	spin_lock(&gc_lock);
	__clk_set_mux_div(clk, new_fparent,
		gc_fclk_tbl[i].src_val, (gc_fclk_tbl[i].div_val + 1));
	spin_unlock(&gc_lock);

	/* set GC bus clk rate here if aclk is bound with fclk */
	if (!cpu_is_z1z2())
		clk_set_rate(&gc_aclk, gc_fclk_tbl[i].comclk_rate);

	trace_pxa_gc_clk_chg(clk->rate, rate);
	pr_debug("%s GC_CLK %x\n", __func__, __raw_readl(clk->clk_rst));
	pr_debug("%s rate %lu->%lu\n", __func__, old_rate, rate);

#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_RATE_CHANGE, i);
#endif
	return 0;
}

static unsigned long gc_clk_getrate(struct clk *clk)
{
	/* can only get fclk here */
	return __clk_periph_get_rate(clk);
}

struct clkops gc_clk_ops = {
	.init		= gc_clk_init,
	.enable		= gc_clk_enable,
	.disable	= gc_clk_disable,
	.round_rate	= gc_clk_round_rate,
	.setrate	= gc_clk_setrate,
	.getrate	= gc_clk_getrate,
};

static struct clk pxa988_clk_gc = {
	.name = "gc",
	.lookup = {
		.con_id = "GCCLK",
	},
	.clk_rst = (void __iomem *)APMU_GC,
	.enable_val = GC_FCLK_REQ,
	.inputs = periph_mux_sel,
	.ops = &gc_clk_ops,
	.dependence = gc_clk_depend,
	.dependence_count = 1,
	.reg_data = {
		     { {APMU_GC, 6, 0x3}, {APMU_GC, 6, 0x3} },
		     { {APMU_GC, 12, 0x7}, {APMU_GC, 12, 0x7} } },
	.is_combined_fc = 1,
};

#define VPU_ACLK_EN	(0x1 << 3)
#define VPU_FCLK_EN	(0x1 << 4)
#define	VPU_AHBCLK_EN	(0x1 << 5)

#define VPU_FCLK_SEL(n)		((n & 0x3) << 6)
#define	VPU_FCLK_SEL_MASK	VPU_FCLK_SEL(0x3)
#define VPU_FCLK_DIV(n)		((n & 0x7) << 8)
#define VPU_FCLK_DIV_MASK	VPU_FCLK_DIV(0x7)
#define VPU_FCLK_REQ		(0x1 << 20)

#define VPU_ACLK_SEL(n)		((n & 0x3) << 11)
#define VPU_ACLK_SEL_MASK	VPU_ACLK_SEL(0x3)
#define VPU_ACLK_DIV(n)		((n & 0x7) << 13)
#define VPU_ACLK_DIV_MASK	VPU_ACLK_DIV(0x7)
#define VPU_ACLK_REQ		(0x1 << 21)

#define VPU_FCLK_RATE(fsrc, fdiv) \
	(VPU_FCLK_SEL(fsrc) | VPU_FCLK_DIV(fdiv))

#define VPU_ACLK_RATE(asrc, adiv) \
	(VPU_ACLK_SEL(asrc) | VPU_ACLK_DIV(adiv))

#define VPU_CLK_RATE_MSK				\
	(VPU_FCLK_SEL_MASK | VPU_FCLK_DIV_MASK		\
	| VPU_ACLK_SEL_MASK | VPU_ACLK_DIV_MASK)	\

/*
 * 1. sort ascending
 * 2. FIXME: If DDR 533M is used, 400M bus clk could not be
 * supported due to clk src issue.
 * 3. For 988 Z1/Z2, only uses vpu aclk 150/300/400
 * 4. For 988 Z3, only uses vpu aclk 156/208/312/416
 * 5. For 1088, temporarily use Z3 table, will be updated
 *    once DE/SV table is given
 */
static struct periph_clk_tbl vpu_aclk_tbl[] = {
	{.clk_rate = 150000000, .parent = &pll2},
	{.clk_rate = 156000000, .parent = &pll1_624},
	{.clk_rate = 208000000, .parent = &pll1_416},
	{.clk_rate = 300000000, .parent = &pll2},
	{.clk_rate = 312000000, .parent = &pll1_624},
	{.clk_rate = 400000000, .parent = &pll2p},
	{.clk_rate = 416000000, .parent = &pll1_416},
};

static void vpu_aclk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, vpu_aclk_tbl,
		ARRAY_SIZE(vpu_aclk_tbl));
	/* default VPU aclk = 312M sel = pll1_624, div = 2 */
	__clk_periph_init(clk, &pll1_624, 2, 1);
#ifdef Z1_MCK4_SYNC_WORKAROUND
	register_clk_bind2ddr(clk,
		vpu_aclk_tbl[ARRAY_SIZE(vpu_aclk_tbl) - 1].clk_rate);
#endif
}

static int vpu_aclk_enable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(VPU_ACLK_EN, 0);
	CLK_SET_BITS(VPU_ACLK_REQ, 0);
	__clk_wait_fc_done(clk);
	spin_unlock(&vpu_lock);
	pr_debug("%s VPU_ACLK %x\n", __func__, __raw_readl(clk->clk_rst));
	return 0;
}

static void vpu_aclk_disable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(0, VPU_ACLK_EN);
	spin_unlock(&vpu_lock);
	return;
}

static long vpu_aclk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		vpu_aclk_tbl, ARRAY_SIZE(vpu_aclk_tbl));
}

static int vpu_aclk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long old_rate;
	unsigned int i;
	struct clk *new_parent;

	old_rate = clk->rate;
	if (rate == old_rate)
		return 0;

	i = 0;
	while (vpu_aclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(vpu_aclk_tbl));

	new_parent = vpu_aclk_tbl[i].parent;
	spin_lock(&vpu_lock);
	__clk_set_mux_div(clk, new_parent,
		vpu_aclk_tbl[i].src_val, (vpu_aclk_tbl[i].div_val + 1));
	spin_unlock(&vpu_lock);

	pr_debug("%s rate %lu->%lu\n", clk->name, old_rate, rate);
	return 0;
}

static unsigned long vpu_aclk_getrate(struct clk *clk)
{
	return __clk_periph_get_rate(clk);
}

struct clkops vpu_aclk_ops = {
	.init		= vpu_aclk_init,
	.enable		= vpu_aclk_enable,
	.disable	= vpu_aclk_disable,
	.round_rate	= vpu_aclk_round_rate,
	.setrate	= vpu_aclk_setrate,
	.getrate	= vpu_aclk_getrate,
};

/*
 * VPU aclk node is internal clk node, and
 * can only be used by VPU fclk
 */
static struct clk vpu_aclk = {
	.name = "vpu_aclk",
	.lookup = {
		.con_id = "VPUACLK",
	},
	.clk_rst = (void __iomem *)APMU_VPU_CLK_RES_CTRL,
	.enable_val = VPU_ACLK_REQ,
	.inputs = periph_mux_sel,
	.ops = &vpu_aclk_ops,
	.reg_data = {
		     { {APMU_VPU_CLK_RES_CTRL, 11, 0x3},
			{APMU_VPU_CLK_RES_CTRL, 11, 0x3} },
		     { {APMU_VPU_CLK_RES_CTRL, 13, 0x7},
			{APMU_VPU_CLK_RES_CTRL, 13, 0x7} } }
};

static struct clk *vpu_clk_depend[] = {
	&vpu_aclk,
};

/* sort ascending */
static struct periph_clk_tbl vpu_fclk_tbl_z1z2[] = {
	{
		.clk_rate = 150000000,
		.parent = &pll2,
	},
	{
		.clk_rate = 200000000,
		.parent = &pll2p,
	},
	{
		.clk_rate = 300000000,
		.parent = &pll2,
	},
	{
		.clk_rate = 400000000,
		.parent = &pll2p,
	},
};

static struct periph_clk_tbl vpu_fclk_tbl[] = {
	{
		.clk_rate = 156000000,
		.parent = &pll1_624,
		.comclk_rate = 156000000,
	},
	{
		.clk_rate = 208000000,
		.parent = &pll1_416,
		.comclk_rate = 208000000,
	},
	{
		.clk_rate = 312000000,
		.parent = &pll1_624,
		.comclk_rate = 312000000,
	},
	{
		.clk_rate = 416000000,
		.parent = &pll1_416,
		.comclk_rate = 416000000
	},
};

unsigned int pxa988_get_vpu_op_num(void)
{
	return ARRAY_SIZE(vpu_fclk_tbl);
}

/* unit Khz */
unsigned int pxa988_get_vpu_op_rate(unsigned int index)
{
	if (index >= ARRAY_SIZE(vpu_fclk_tbl)) {
		pr_err("%s index out of range!\n", __func__);
		return -EINVAL;
	}

	return vpu_fclk_tbl[index].clk_rate / MHZ_TO_KHZ;
}

static void vpu_clk_init(struct clk *clk)
{
#ifdef CONFIG_DEBUG_FS
	unsigned int i;
	unsigned long op[ARRAY_SIZE(vpu_fclk_tbl)];
#endif
	if (cpu_is_z1z2())
		memcpy(&vpu_fclk_tbl, &vpu_fclk_tbl_z1z2,
			sizeof(struct periph_clk_tbl) *\
				ARRAY_SIZE(vpu_fclk_tbl));

	__clk_fill_periph_tbl(clk, vpu_fclk_tbl, ARRAY_SIZE(vpu_fclk_tbl));

	if (cpu_is_z1z2())
		/* default VPU fclk = 300M sel = pll2 = 1200M, div = 4 */
		__clk_periph_init(clk, &pll2, 4, 1);
	else
		/* default VPU fclk = 312M sel = pll1_624, div = 2 */
		__clk_periph_init(clk, &pll1_624, 2, 1);

#ifdef CONFIG_DEBUG_FS
	for (i = 0; i < ARRAY_SIZE(vpu_fclk_tbl); i++)
		op[i] = vpu_fclk_tbl[i].clk_rate;
	pxa988_clk_register_dcstat(clk, op, ARRAY_SIZE(vpu_fclk_tbl));
#endif
}

static int vpu_clk_enable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS((VPU_FCLK_EN | VPU_AHBCLK_EN), 0);
	CLK_SET_BITS(VPU_FCLK_REQ, 0);
	__clk_wait_fc_done(clk);
	spin_unlock(&vpu_lock);
	trace_pxa_vpu_clk(CLK_ENABLE, __raw_readl(clk->clk_rst));
	pr_debug("%s VPU_CLK %x\n", __func__, __raw_readl(clk->clk_rst));

#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_STATE_ON, 0);
#endif
	return 0;
}

static void vpu_clk_disable(struct clk *clk)
{
	spin_lock(&vpu_lock);
	CLK_SET_BITS(0, (VPU_FCLK_EN | VPU_AHBCLK_EN));
	spin_unlock(&vpu_lock);
	trace_pxa_vpu_clk(CLK_DISABLE, __raw_readl(clk->clk_rst));
	pr_debug("%s VPU_CLK %x\n", __func__, __raw_readl(clk->clk_rst));

#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_STATE_OFF, 0);
#endif
}

static long vpu_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		vpu_fclk_tbl, ARRAY_SIZE(vpu_fclk_tbl));
}

static int vpu_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long old_rate;
	unsigned int i;
	struct clk *new_fparent;

	old_rate = clk->rate;
	if (rate == old_rate)
		return 0;

	i = 0;
	while (vpu_fclk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(vpu_fclk_tbl));

	/* set VPU function clk rate */
	new_fparent = vpu_fclk_tbl[i].parent;
	spin_lock(&vpu_lock);
	__clk_set_mux_div(clk, new_fparent,
		vpu_fclk_tbl[i].src_val, (vpu_fclk_tbl[i].div_val + 1));
	spin_unlock(&vpu_lock);

	/* set VPU bus clk rate here if aclk is bound with fclk */
	if (!cpu_is_z1z2())
		clk_set_rate(&vpu_aclk, vpu_fclk_tbl[i].comclk_rate);

	trace_pxa_vpu_clk_chg(clk->rate, rate);
	pr_debug("%s VPU_CLK : %x\n", __func__,  __raw_readl(clk->clk_rst));
	pr_debug("%s rate %lu->%lu\n", __func__, clk->rate, rate);

#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_RATE_CHANGE, i);
#endif
	return 0;
}

static unsigned long vpu_clk_getrate(struct clk *clk)
{
	/* can only get fclk here */
	return __clk_periph_get_rate(clk);
}

struct clkops vpu_clk_ops = {
	.init		= vpu_clk_init,
	.enable		= vpu_clk_enable,
	.disable	= vpu_clk_disable,
	.round_rate	= vpu_clk_round_rate,
	.setrate	= vpu_clk_setrate,
	.getrate	= vpu_clk_getrate,
};

static struct clk pxa988_clk_vpu = {
	.name = "vpu",
	.lookup = {
		.con_id = "VPUCLK",
	},
	.inputs = periph_mux_sel,
	.clk_rst = (void __iomem *)APMU_VPU_CLK_RES_CTRL,
	.enable_val = VPU_FCLK_REQ,
	.ops = &vpu_clk_ops,
	.dependence = vpu_clk_depend,
	.dependence_count = 1,
	.reg_data = {
		     { {APMU_VPU_CLK_RES_CTRL, 6, 0x3},
			{APMU_VPU_CLK_RES_CTRL, 6, 0x3} },
		     { {APMU_VPU_CLK_RES_CTRL, 8, 0x7},
			{APMU_VPU_CLK_RES_CTRL, 8, 0x7} } },
	.is_combined_fc = 1,
};

#define LCD_CI_ISP_ACLK_REQ		(1 << 22)
#define LCD_CI_ISP_ACLK_EN		(1 << 3)
#define LCD_CI_ISP_ACLK_RST		(1 << 16)

/*
 * 1. This AXI clock is shared by LCD/CI/ISP
 * 2. Separate bit in LCD/CI/ISP_CLK_RES_REG is used
 * to enable its own bus clock
 * 3. Register setting is defined in LCD_CLK_RES_REG
 * 4. The safe maximum rate is 312M per DE's suggestion on Z0
 * 5. Use 208M for Z0 bringup at first
 */
static struct periph_clk_tbl lcd_ci_isp_axi_clk_tbl[] = {
	{.clk_rate = 150000000, .parent = &pll2},
	{.clk_rate = 156000000, .parent = &pll1_624},
	{.clk_rate = 200000000, .parent = &pll2p},
	{.clk_rate = 208000000, .parent = &pll1_416},
	{.clk_rate = 300000000, .parent = &pll2},
	{.clk_rate = 312000000, .parent = &pll1_624},
};

static void lcd_ci_isp_axi_clk_init(struct clk *clk)
{
	__clk_fill_periph_tbl(clk, lcd_ci_isp_axi_clk_tbl,\
		ARRAY_SIZE(lcd_ci_isp_axi_clk_tbl));
	__clk_periph_init(clk, &pll1_416, 2, 1);
#ifdef Z1_MCK4_SYNC_WORKAROUND
	register_clk_bind2ddr(clk,
		lcd_ci_isp_axi_clk_tbl[ARRAY_SIZE(lcd_ci_isp_axi_clk_tbl) - 1].clk_rate);
#endif
}

static int lcd_ci_isp_axi_clk_enable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS(LCD_CI_ISP_ACLK_RST | LCD_CI_ISP_ACLK_EN, 0);
	CLK_SET_BITS(LCD_CI_ISP_ACLK_REQ, 0);
	__clk_wait_fc_done(clk);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
	return 0;
}

static void lcd_ci_isp_axi_clk_disable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS(0, LCD_CI_ISP_ACLK_EN);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
}

static long lcd_ci_isp_axi_clk_roundrate(struct clk *clk, unsigned long rate)
{
	return __clk_round_rate_bytbl(clk, rate, \
		lcd_ci_isp_axi_clk_tbl,
		ARRAY_SIZE(lcd_ci_isp_axi_clk_tbl));
}

static int lcd_ci_isp_axi_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	unsigned int i;
	struct clk *new_parent;

	i = 0;
	while (lcd_ci_isp_axi_clk_tbl[i].clk_rate != rate)
		i++;
	BUG_ON(i == ARRAY_SIZE(lcd_ci_isp_axi_clk_tbl));

	new_parent = lcd_ci_isp_axi_clk_tbl[i].parent;
	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	__clk_set_mux_div(clk, new_parent,
		lcd_ci_isp_axi_clk_tbl[i].src_val,
		(lcd_ci_isp_axi_clk_tbl[i].div_val + 1));
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
	return 0;
}

static unsigned long lcd_ci_isp_axi_clk_getrate(struct clk *clk)
{
	unsigned long rate, flags;

	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	rate = __clk_periph_get_rate(clk);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
	return rate;
}

struct clkops lcd_ci_isp_axi_clk_ops = {
	.init		= lcd_ci_isp_axi_clk_init,
	.enable		= lcd_ci_isp_axi_clk_enable,
	.disable	= lcd_ci_isp_axi_clk_disable,
	.round_rate	= lcd_ci_isp_axi_clk_roundrate,
	.setrate	= lcd_ci_isp_axi_clk_setrate,
	.getrate	= lcd_ci_isp_axi_clk_getrate,
};

/* bus clock shared by lcd, ci and isp */
static struct clk lcd_ci_isp_axi_clk = {
	.name = "lcd_ci_isp_axi",
	.lookup = {
		.con_id = "LCDCIISPAXI",
	},
	.clk_rst = (void __iomem *)APMU_LCD,
	.enable_val = LCD_CI_ISP_ACLK_REQ,
	.inputs = periph_mux_sel,
	.ops = &lcd_ci_isp_axi_clk_ops,
	.reg_data = {
		     { {APMU_LCD, 17, 0x3}, {APMU_LCD, 17, 0x3} },
		     { {APMU_LCD, 19, 0x7}, {APMU_LCD, 19, 0x7} } }
};

#define LCD_CI_HCLK_EN		(1 << 5)
#define LCD_CI_HCLK_RST		(1 << 2)

static int lcd_ci_hclk_enable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS((LCD_CI_HCLK_EN | LCD_CI_HCLK_RST) , 0);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
	return 0;
}

static void lcd_ci_hclk_disable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS(0, LCD_CI_HCLK_EN);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
}

struct clkops  lcd_ci_hclk_ops = {
	.enable		= lcd_ci_hclk_enable,
	.disable	= lcd_ci_hclk_disable,
};

static struct clk pxa988_lcd_ci_hclk = {
	.name = "lcd_ci_hclk",
	.lookup = {
		.con_id = "LCDCIHCLK",
	},
	.clk_rst = (void __iomem *)APMU_LCD,
	.ops = &lcd_ci_hclk_ops,
};

static inline int __ccic_clk_common_enable(struct clk *clk, unsigned int bits)
{
	unsigned long flags;
	spin_lock_irqsave(&ccic_lock, flags);
	CLK_SET_BITS(bits, 0);
	spin_unlock_irqrestore(&ccic_lock, flags);
	return 0;
}

static inline void __ccic_clk_common_disable(struct clk *clk, unsigned int bits)
{
	unsigned long flags;
	spin_lock_irqsave(&ccic_lock, flags);
	CLK_SET_BITS(0, bits);
	spin_unlock_irqrestore(&ccic_lock, flags);
}

#define CCIC_AXI_EN	((1 << 0) | (1 << 3))
#define CCIC_AXI_DIS	(1 << 3)

static int ccic_axi_clk_enable(struct clk *clk)
{
	return __ccic_clk_common_enable(clk, CCIC_AXI_EN);
}

static void ccic_axi_clk_disable(struct clk *clk)
{
	__ccic_clk_common_disable(clk, CCIC_AXI_DIS);
}

struct clkops ccic_axi_clk_ops = {
	.enable		= ccic_axi_clk_enable,
	.disable	= ccic_axi_clk_disable,
};

static struct clk pxa988_ccic_axi_clk = {
	.name = "ccic_axi",
	.lookup = {
		.con_id = "CCICAXICLK",
	},
	.clk_rst = (void __iomem *)APMU_CCIC_RST,
	.parent = &lcd_ci_isp_axi_clk,
	.ops = &ccic_axi_clk_ops,
};

#define CCIC_PHYSLOW_PRER	(0x1A << 10)
#define CCIC_PHYCLK_SEL		(0x1 << 7)
#define CCIC_PHYCLK_SELDIV	\
	(CCIC_PHYSLOW_PRER | CCIC_PHYCLK_SEL)
#define CCIC_PHYCLK_SELDIV_MSK	((1 << 7) | (0x1f << 10))
#define CCIC_PHY_EN	((1 << 5)|(1 << 8)|(1 << 9))
#define CCIC_PHY_DIS	((1 << 5)|(1 << 9))
#define CSI_DPHY_RST	(1 << 2)

static void ccic_phy_clk_init(struct clk *clk)
{
	/* default sel:52M  div : 0x1f */
	CLK_SET_BITS(CCIC_PHYCLK_SELDIV,
		CCIC_PHYCLK_SELDIV_MSK);
}

static int ccic_phy_clk_enable(struct clk *clk)
{
	__ccic_clk_common_enable(clk, CSI_DPHY_RST);
	__ccic_clk_common_enable(clk, CCIC_PHY_EN);
	__raw_writel(0x06000000 | __raw_readl(APMU_CCIC_DBG),
			APMU_CCIC_DBG);
	return 0;
}

static void ccic_phy_clk_disable(struct clk *clk)
{
	__ccic_clk_common_disable(clk, CCIC_PHY_DIS);
	__raw_writel((~0x06000000) & __raw_readl(APMU_CCIC_DBG),
		APMU_CCIC_DBG);
	__ccic_clk_common_disable(clk, CSI_DPHY_RST);
}

struct clkops ccic_phy_clk_ops = {
	.init		= ccic_phy_clk_init,
	.enable		= ccic_phy_clk_enable,
	.disable	= ccic_phy_clk_disable,
};

static struct clk pxa988_ccic_phy_clk = {
	.name = "ccic_phy",
	.lookup = {
		.con_id = "CCICPHYCLK",
	},
	.clk_rst = (void __iomem *)APMU_CCIC_RST,
	.ops = &ccic_phy_clk_ops,
};

#define CI_FUNC_CLK_REQ		(1 << 15)
#define CI_FUNC_CLK_EN		((1 << 1) | (1 << 4))
#define CI_FUNC_CLK_DIS		(1 << 4)

static void ccic_func_clk_init(struct clk *clk)
{
	/* default 312M pll1_624/2 */
	__clk_periph_init(clk, &pll1_624, 2, 0);
}

static int ccic_func_clk_enable(struct clk *clk)
{
	__ccic_clk_common_enable(clk, CI_FUNC_CLK_EN);
	return __ccic_clk_common_enable(clk, \
		CI_FUNC_CLK_REQ);
}

static void ccic_func_clk_disable(struct clk *clk)
{
	__ccic_clk_common_disable(clk, CI_FUNC_CLK_DIS);
}

static int ccic_func_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;

	spin_lock_irqsave(&ccic_lock, flags);
	__clk_periph_set_rate(clk, rate);
	spin_unlock_irqrestore(&ccic_lock, flags);
	return 0;
}

static unsigned long ccic_func_clk_getrate(struct clk *clk)
{
	unsigned long rate, flags;

	spin_lock_irqsave(&ccic_lock, flags);
	rate = __clk_periph_get_rate(clk);
	spin_unlock_irqrestore(&ccic_lock, flags);
	return rate;
}

struct clkops ccic_func_clk_ops = {
	.init		= ccic_func_clk_init,
	.enable		= ccic_func_clk_enable,
	.disable	= ccic_func_clk_disable,
	.setrate	= ccic_func_clk_setrate,
	.getrate	= ccic_func_clk_getrate,
};

static struct clk pxa988_ccic_func_clk = {
	.name = "ccic_func",
	.lookup = {
		.con_id = "CCICFUNCLK",
	},
	.clk_rst = (void __iomem *)APMU_CCIC_RST,
	.inputs = periph_mux_sel,
	.ops = &ccic_func_clk_ops,
	.reg_data = {
		     { {APMU_CCIC_RST, 16, 0x3}, {APMU_CCIC_RST, 16, 0x3} },
		     { {APMU_CCIC_RST, 18, 0x7}, {APMU_CCIC_RST, 18, 0x7} } }
};

#define DSI_PHYSLOW_PRER	(0x1A << 6)
#define DSI_ESC_SEL		(0x0)
#define DSI_PHYESC_SELDIV	\
	(DSI_PHYSLOW_PRER | DSI_ESC_SEL)
#define DSI_PHYESC_SELDIV_MSK	((0x1f << 6) | 0x3)
#define DSI_PHY_CLK_EN	((1 << 2) | (1 << 5))
#define DSI_PHY_CLK_RST	((1 << 3) | (1 << 4))

static void dsi_phy_clk_init(struct clk *clk)
{
	/* default sel 78M, div 0x1A */
	CLK_SET_BITS(DSI_PHYESC_SELDIV,
		DSI_PHYESC_SELDIV_MSK);
}

static int dsi_phy_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(DSI_PHY_CLK_EN, 0);
	CLK_SET_BITS(DSI_PHY_CLK_RST, 0);
	return 0;
}

static void dsi_phy_clk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, DSI_PHY_CLK_EN);
}

struct clkops dsi_phy_clk_ops = {
	.init = dsi_phy_clk_init,
	.enable = dsi_phy_clk_enable,
	.disable = dsi_phy_clk_disable,
};

static struct clk lcd_dsi_phy_clk = {
	.name = "lcd_dsi_phy",
	.lookup = {
		.con_id = "DSIPHYCLK",
	},
	.clk_rst = (void __iomem *)APMU_DSI,
	.ops = &dsi_phy_clk_ops,
};

#define LCD_PST_CKEN		(1 << 9)
#define LCD_PST_OUTDIS		(1 << 8)
#define LCD_CLK_EN		(1 << 4)
#define LCD_CLK_RST		(1 << 1 | 1 << 0)
#define LCD_DEF_FCLK_SEL	(1 << 6)
#define LCD_FCLK_SEL_MASK	(1 << 6)

/* Actually this clock is the src of LCD controller and DSI */
/* Will be further divided in LCD */
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
static void lcd_func_clk_init(struct clk *clk)
{
	clk_reparent(clk, &pll1_624);
	clk->mul = clk->div = 1;
	clk->rate = clk_get_rate(clk->parent);
}
#else
static void lcd_func_clk_init(struct clk *clk)
{

	/* 1 --- 416M by default */
	CLK_SET_BITS(LCD_DEF_FCLK_SEL, LCD_FCLK_SEL_MASK);
	/* Default enable the post divider */
	CLK_SET_BITS(LCD_PST_CKEN, LCD_PST_OUTDIS);

	clk_reparent(clk, &pll1_416);
	clk->mul = clk->div = 1;
	clk->rate = clk_get_rate(clk->parent);
}
#endif
static int lcd_func_clk_enable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS((LCD_CLK_EN | LCD_CLK_RST), 0);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
	return 0;
}

static void lcd_func_clk_disable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&lcd_ci_share_lock, flags);
	CLK_SET_BITS(0, LCD_CLK_EN);
	spin_unlock_irqrestore(&lcd_ci_share_lock, flags);
}

static long lcd_func_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/* Do nothing here, but select the correct rate in set rate ops */
	return rate;
}

/*
 * LCD post divider is special one, div 0:
 * input = output, div 2~16 output = input / div
 */
static int lcd_func_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int mux = 0, div = 0;
	struct clk *best_parent;
	unsigned long new_rate;
	unsigned int set, clear;

	if (rate == clk->rate)
		return 0;

	new_rate =
		__clk_sel_mux_div(clk, rate, &mux, &div, &best_parent);
	if (1 == div)
		div = 0;

	set = (mux << clk->reg_data[SOURCE][CONTROL].reg_shift) | \
		(div << clk->reg_data[DIV][CONTROL].reg_shift);
	clear = (clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift) | \
		(clk->reg_data[DIV][CONTROL].reg_mask << \
		clk->reg_data[DIV][CONTROL].reg_shift);
	CLK_SET_BITS(set, clear);
	clk_reparent(clk, best_parent);
	if (new_rate != rate)
		pr_debug("%s tgt%lu sel%lu\n", __func__, rate, new_rate);
	return 0;
}

static unsigned long lcd_func_clk_getrate(struct clk *clk)
{
	unsigned int mux, div;
	__clk_get_mux_div(clk, &mux, &div);
	div = div - 1;
	if (0 == div)
		div = 1;
	return clk_get_rate(clk->parent) / div;
}

static struct clk *lcd_depend_clk[] = {
	&lcd_ci_isp_axi_clk,
	&lcd_dsi_phy_clk,
	&pxa988_lcd_ci_hclk,
};

static struct clk_mux_sel lcd_fclk_clk_mux[] = {
	{.input = &pll1_416, .value = 1},
	{.input = &pll1_624, .value = 0},
	{0, 0},
};

struct clkops lcd_fclk_clk_ops = {
	.init = lcd_func_clk_init,
	.enable = lcd_func_clk_enable,
	.disable = lcd_func_clk_disable,
	.round_rate = lcd_func_clk_round_rate,
	.setrate = lcd_func_clk_setrate,
	.getrate = lcd_func_clk_getrate,
};

static struct clk pxa988_lcd_clk = {
	.name = "lcd",
	.lookup = {
		.con_id = "LCDCLK",
	},
	.clk_rst = (void __iomem *)APMU_LCD,
	.dependence = lcd_depend_clk,
	.dependence_count = ARRAY_SIZE(lcd_depend_clk),
	.inputs = lcd_fclk_clk_mux,
	.ops = &lcd_fclk_clk_ops,
	.reg_data = {
		{ {APMU_LCD, 6, 0x1}, {APMU_LCD, 6, 0x1} },
		{ {APMU_LCD, 10, 0x1f}, {APMU_LCD, 10, 0x1f} } }
};

#define ISP_DXO_CLK_EN		\
	((1 << 1) | (1 << 9) | (1 << 11))
#define ISP_DXO_CLK_RST		\
	((1 << 0) | (1 << 8) | (1 << 10))
#define ISP_DXO_CLK_REQ		(1 << 7)

static void isp_dxo_clk_init(struct clk *clk)
{
	/* default 312M pll1_624/2 */
	__clk_periph_init(clk, &pll1_624, 2, 0);
}

static int isp_dxo_clk_enable(struct clk *clk)
{
	CLK_SET_BITS(ISP_DXO_CLK_EN | ISP_DXO_CLK_RST, 0);
	CLK_SET_BITS(ISP_DXO_CLK_REQ, 0);
	trace_pxa_isp_dxo_clk(CLK_ENABLE);
	return 0;
}

static void isp_dxo_clk_disable(struct clk *clk)
{
	CLK_SET_BITS(0, ISP_DXO_CLK_EN);
	trace_pxa_isp_dxo_clk(CLK_DISABLE);
}

static int isp_dxo_clk_setrate(struct clk *clk, unsigned long rate)
{
	__clk_periph_set_rate(clk, rate);
	trace_pxa_isp_dxo_clk_chg(rate);
	return 0;
}

static unsigned long isp_dxo_clk_getrate(struct clk *clk)
{
	return __clk_periph_get_rate(clk);
}

struct clkops isp_dxo_clk_ops = {
	.init		= isp_dxo_clk_init,
	.enable		= isp_dxo_clk_enable,
	.disable	= isp_dxo_clk_disable,
	.setrate	= isp_dxo_clk_setrate,
	.getrate	= isp_dxo_clk_getrate,
};

static struct clk *isp_dxo_depend_clk[] = {
	&lcd_ci_isp_axi_clk,
};

static struct clk pxa988_isp_dxo_clk = {
	.name = "isp_dxo",
	.lookup = {
		.con_id = "ISP-CLK",
	},
	.dependence = isp_dxo_depend_clk,
	.dependence_count = ARRAY_SIZE(isp_dxo_depend_clk),
	.clk_rst = (void __iomem *)APMU_ISPDXO,
	.inputs = periph_mux_sel,
	.ops = &isp_dxo_clk_ops,
	.reg_data = {
		     { {APMU_ISPDXO, 2, 0x3}, {APMU_ISPDXO, 2, 0x3} },
		     { {APMU_ISPDXO, 4, 0x7}, {APMU_ISPDXO, 4, 0x7} } }
};

static int nand_clk_enable(struct clk *clk)
{
	__raw_writel(0x19b, clk->clk_rst);
	return 0;
}

static void nand_clk_disable(struct clk *clk)
{
	/* only disable peripheral clock */
	__raw_writel(0x18b, clk->clk_rst);
}

struct clkops nand_clk_ops = {
	.enable = nand_clk_enable,
	.disable = nand_clk_disable,
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

	if (!strcmp(clk->name, "pwm0")) {
		clk_share = clk_get_sys("pxa910-pwm.1", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm1")) {
		clk_share = clk_get_sys("pxa910-pwm.0", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	} else if (!strcmp(clk->name, "pwm2")) {
		clk_share = clk_get_sys("pxa910-pwm.3", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm3")) {
		clk_share = clk_get_sys("pxa910-pwm.2", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	}
	if (clk_share && clk_apb && (clk->refcnt + clk_share->refcnt) == 1) {
		data = __raw_readl(clk_apb->clk_rst);
		data |= APBC_APBCLK;
		__raw_writel(data, clk_apb->clk_rst);
		udelay(10);
		if (!strcmp(clk->name, clk_apb->name)) {
			data = __raw_readl(clk->clk_rst);
			data &= ~APBC_RST;
			__raw_writel(data, clk->clk_rst);
		} else {
			data = __raw_readl(clk->clk_rst);
			data &= ~APBC_RST;
			__raw_writel(data, clk->clk_rst);
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

	if (!strcmp(clk->name, "pwm0")) {
		clk_share = clk_get_sys("pxa910-pwm.1", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm1")) {
		clk_share = clk_get_sys("pxa910-pwm.0", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	} else if (!strcmp(clk->name, "pwm2")) {
		clk_share = clk_get_sys("pxa910-pwm.3", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk;
	} else if (!strcmp(clk->name, "pwm3")) {
		clk_share = clk_get_sys("pxa910-pwm.2", NULL);
		BUG_ON(IS_ERR(clk_share));
		clk_apb = clk_share;
	}

	if (clk_share && clk_apb && (clk->refcnt + clk_share->refcnt) == 0) {
		data = __raw_readl(clk_apb->clk_rst);
		data &= ~APBC_APBCLK;
		__raw_writel(data, clk_apb->clk_rst);
	}
}

struct clkops pwm_clk_ops = {
	.enable = pwm_clk_enable,
	.disable = pwm_clk_disable,
};

static void ssp1_clk_init(struct clk *clk)
{
	/*
	 * ISCCR1: for low power mp3 playback.
	 * Configure to 44.1K frame clock
	 * BITCLK_DIV_468: value 1, bit clock is sysclk divided by 2.
	 * DENOM: 0x529
	 * NOM: 0xbe2
	 */
	__raw_writel(0x4a948be2, MPMU_ISCCRX1);
	/* enable ssp1 clock dynamic change */
	clk->dynamic_change = 1;
}

static int ssp1_clk_enable(struct clk *clk)
{
	return apbc_clk_enable(clk);
}

static void ssp1_clk_disable(struct clk *clk)
{
	apbc_clk_disable(clk);
}

static int ssp1_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int val;
	/*
	 * currently rate is used to as an indication of clock enable or
	 * not. since fixed 44.1K is used. in future if multi-rate is
	 * necessary, we could use rate to set different ISCCRx1 register.
	 * value 0: disable isccr1. other value: enable isccr1.
	 */

	if (rate) {
		val = __raw_readl(MPMU_ISCCRX1);
		/* set bit 31 & 29 to enable sysclk & bitclk */
		val |= (0x5 << 29);
		__raw_writel(val, MPMU_ISCCRX1);
	} else {
		val = __raw_readl(MPMU_ISCCRX1);
		/* clear bit 31 & 29 to disable sysclk & bitclk */
		val &= ~(0x5 << 29);
		__raw_writel(val, MPMU_ISCCRX1);
	}

	pr_debug("%s: ISCCRx1 %x\n", __func__, __raw_readl(MPMU_ISCCRX1));

	return 0;
}

struct clkops ssp1_clk_ops = {
	.init		= ssp1_clk_init,
	.enable		= ssp1_clk_enable,
	.disable	= ssp1_clk_disable,
	.setrate	= ssp1_clk_setrate,
};

static void gssp_clk_init(struct clk *clk)
{
	/*
	 * ISCCR0: gssp I2S clock generation control register.
	 * Config to 8k frame clock.
	 * BITCLK_DIV_468: value 1, bit clock is sysclk divided by 4.
	 * DENOM: 0x180
	 * NOM: 0x659
	 */
	__raw_writel(0x00c00659, MPMU_ISCCRX0);
	/* enable gssp clock dynamic change */
	clk->dynamic_change = 1;
}

/* gssp clk ops: gssp is shared between AP and CP */
static int gssp_clk_enable(struct clk *clk)
{
	unsigned int gcer;
	/* GPB bus select: choose APB */
	__raw_writel(0x1, APBC_PXA988_GBS);
	/* GSSP clock control register: GCER */
	gcer = __raw_readl(clk->clk_rst) & ~(0x3 << 8);
	gcer |= APBC_RST;
	__raw_writel(gcer, clk->clk_rst);
	udelay(1);
	gcer &= ~APBC_RST;
	__raw_writel(gcer, clk->clk_rst);
	udelay(1);
	/* choose I2S clock */
	gcer |= APBC_FNCLK | (0x0 << 8);
	__raw_writel(gcer, clk->clk_rst);
	udelay(10);
	gcer |= APBC_APBCLK;
	__raw_writel(gcer, clk->clk_rst);
	udelay(10);
	gcer &= ~APBC_RST;
	__raw_writel(gcer, clk->clk_rst);
	pr_debug("gssp clk is open\n");

	return 0;
}

static void gssp_clk_disable(struct clk *clk)
{
	unsigned int gcer;
	gcer = __raw_readl(clk->clk_rst);
	gcer &= ~APBC_APBCLK;
	__raw_writel(gcer, clk->clk_rst);
	__raw_writel(0x0, APBC_PXA988_GBS);
	pr_debug("gssp clk is closed\n");
}

static int gssp_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned int val, nom, denom, factor;
	static int enable_count;
	/*
	 * currently rate is used to as an indication of clock enable or
	 * not. value 0: disable isccr1. other value: enable isccr1.
	 */

	nom = 0x659;
	denom = 0x180;
	factor = 1;

	switch (rate) {
	case 0:
		break;
	case 8000:
		factor = 1;
		break;
	case 16000:
		factor = 2;
		break;
	case 24000:
		factor = 3;
		break;
	case 32000:
		factor = 4;
		break;
	case 48000:
		factor = 6;
		break;
	default:
		pr_info("rate %ld is not supported, use 8k by default\n", rate);
		break;
	}

	if (rate) {
		if (enable_count++ == 0) {
			denom = (denom * factor) << 15;
			val = __raw_readl(MPMU_ISCCRX0);
			/* set ISCCRX0 since CP may modify*/
			val = (val & (~0x5fffffff)) | denom  | nom;
			/* set bit 31 & 29 to enable sysclk & bitclk */
			val |= (0x5 << 29);
			__raw_writel(val, MPMU_ISCCRX0);
		}
	} else {
		if (--enable_count == 0) {
			denom = (denom * factor) << 15;
			val = __raw_readl(MPMU_ISCCRX0);
			/* set ISCCRX0 since CP may modify*/
			val = (val & (~0x5fffffff)) | denom  | nom;
			/* clear bit 31 & 29 to disable sysclk & bitclk */
			val &= ~(0x5 << 29);
			__raw_writel(val, MPMU_ISCCRX0);
		}
	}

	pr_debug("%s: ISCCRx0 %x\n", __func__, __raw_readl(MPMU_ISCCRX0));

	return 0;
}

struct clkops gssp_clk_ops = {
	.init = gssp_clk_init,
	.enable = gssp_clk_enable,
	.disable = gssp_clk_disable,
	.setrate = gssp_clk_setrate,
};

#define USB_AXICLK_EN	(1 << 3)
#define USB_AXI_RST		(1 << 0)
static int udc_clk_enable(struct clk *clk)
{
	__raw_writel((USB_AXICLK_EN | USB_AXI_RST),\
				clk->clk_rst);
	return 0;
}

static void udc_clk_disable(struct clk *clk)
{
	if (cpu_is_z1z2())
		__raw_writel(USB_AXI_RST, clk->clk_rst);
	else
		__raw_writel(0x0, clk->clk_rst);
}

struct clkops udc_clk_ops = {
	.enable = udc_clk_enable,
	.disable = udc_clk_disable,
};

/* interface for I2C controller */
static void twsi_clk_init(struct clk *clk)
{
	unsigned int regval;
	unsigned int fnclkmask, fnclkval;

	/*
	 * i2c clock has been enable in uboot.
	 * disable clk in case set clk rate is needed.
	 */

	regval = __raw_readl(clk->clk_rst) & ~APBC_FNCLK;
	__raw_writel(regval, clk->clk_rst);
	udelay(10);

	regval &= ~APBC_APBCLK;
	__raw_writel(regval, clk->clk_rst);

	/* set i2c clk to default value */
	fnclkmask = clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;
	fnclkval = clk->fnclksel << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;

	regval = __raw_readl(clk->clk_rst) & ~fnclkmask;
	regval |= fnclkval;
	__raw_writel(regval, clk->clk_rst);
}

static int twsi_clk_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long old_rate;
	unsigned int fnclkmask, fnclkval;
	unsigned int regval;

	old_rate = clk->rate;
	if (rate == old_rate)
		return 0;

	if ((rate != 33000000) && (rate != 52000000) && (rate != 62400000)) {
		pr_warning("clk[%s] rate is invalid,make no change\n",
				clk->name);
		return 0;
	}

	if (rate == 33000000)
		clk->fnclksel = 0;
	else if (rate == 62400000)
		clk->fnclksel = 2;
	else
		clk->fnclksel = 1;

	fnclkmask = clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;
	fnclkval = clk->fnclksel << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;

	regval = __raw_readl(clk->clk_rst) & ~fnclkmask;
	regval |= fnclkval;
	__raw_writel(regval, clk->clk_rst);

	pr_debug("%s rate %lu->%lu\n", clk->name, old_rate, rate);
	return 0;
}

static unsigned long twsi_clk_getrate(struct clk *clk)
{
	unsigned int fnclkmask, fnclkval;
	unsigned int regval;

	regval = __raw_readl(clk->clk_rst);
	fnclkmask = clk->reg_data[SOURCE][CONTROL].reg_mask << \
		clk->reg_data[SOURCE][CONTROL].reg_shift;
	fnclkval = (regval & fnclkmask) >> \
		clk->reg_data[SOURCE][CONTROL].reg_shift;

	if (fnclkval == 0)
		return 33000000;
	else if (fnclkval == 2)
		return 62400000;
	else if (fnclkval == 1)
		return 52000000;
	else
		return 0;
}

static int twsi_clk_enable(struct clk *clk)
{
	unsigned int regval;

	regval = __raw_readl(clk->clk_rst) | APBC_FNCLK;
	__raw_writel(regval, clk->clk_rst);

	/*
	 * delay two cycles of the solwest clock between the APB bus clock
	 * and the functional module clock.
	 */
	udelay(10);

	regval |= APBC_APBCLK;
	__raw_writel(regval, clk->clk_rst);
	udelay(10);

	regval &= ~APBC_RST;
	__raw_writel(regval, clk->clk_rst);

	return 0;
}

static void twsi_clk_disable(struct clk *clk)
{
	unsigned int regval;

	regval = __raw_readl(clk->clk_rst) & ~APBC_FNCLK;
	__raw_writel(regval, clk->clk_rst);
	udelay(10);

	regval &= ~APBC_APBCLK;
	__raw_writel(regval, clk->clk_rst);
}

struct clkops twsi_clk_ops = {
	.init		= twsi_clk_init,
	.enable		= twsi_clk_enable,
	.disable	= twsi_clk_disable,
	.setrate	= twsi_clk_setrate,
	.getrate	= twsi_clk_getrate,
};

static struct clk twsi0_clk = {
	.name = "twsi0",
	.lookup = {
		.dev_id = "pxa910-i2c.0",
	},
	.clk_rst = (void __iomem *)APBC_PXA988_TWSI0,
	.fnclksel = 0,
	.ops = &twsi_clk_ops,
	.reg_data = {
		{ {APBC_PXA988_TWSI0, 4, 0x7}, {APBC_PXA988_TWSI0, 4, 0x7} }
	}
};/* ci2c */

static struct clk twsi1_clk = {
	.name = "twsi1",
	.lookup = {
		.dev_id = "pxa910-i2c.1",
	},
	.clk_rst = (void __iomem *)APBC_PXA988_TWSI1,
	.fnclksel = 0,
	.ops = &twsi_clk_ops,
	.reg_data = {
		{ {APBC_PXA988_TWSI1, 4, 0x7}, {APBC_PXA988_TWSI1, 4, 0x7} }
	}
};/* ci2c1 */

static struct clk twsi2_clk = {
	.name = "twsi2",
	.lookup = {
		.dev_id = "pxa910-i2c.2",
	},
	.clk_rst = (void __iomem *)APBC_PXA988_PWRTWSI,
	.fnclksel = 0,
	.ops = &twsi_clk_ops,
	.reg_data = {
		{ {APBC_PXA988_PWRTWSI, 3, 0x3}, {APBC_PXA988_PWRTWSI, 3, 0x3} }
	}
};/* pwr_i2c */

#define APBC_CLK(_name, _dev, _con, _reg, _fnclksel, _rate, _parent)\
{							\
	.name = _name,					\
	.lookup = {					\
		.dev_id = _dev,\
		.con_id = _con,\
	},						\
	.clk_rst = (void __iomem *)_reg,		\
	.fnclksel = _fnclksel,				\
	.rate = _rate,					\
	.ops = &apbc_clk_ops,				\
	.parent = _parent,				\
}

#define APBC_CLK_OPS(_name, _dev, _con, _reg, _fnclksel, _rate, _parent, _ops)\
{							\
	.name = _name,					\
	.lookup = {					\
		.dev_id = _dev,\
		.con_id = _con,\
	},						\
	.clk_rst = (void __iomem *)_reg,		\
	.fnclksel = _fnclksel,				\
	.rate = _rate,					\
	.ops = _ops,					\
	.parent = _parent,				\
}

#define APMU_CLK(_name, _dev, _con, _reg, _eval, _rate, _parent)\
{								\
	.name = _name,						\
	.lookup = {						\
		.dev_id = _dev,					\
		.con_id = _con,					\
	},							\
	.clk_rst = (void __iomem *)_reg,			\
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
	.clk_rst = (void __iomem *)_reg,			\
	.enable_val = _eval,					\
	.rate = _rate,						\
	.parent = _parent,					\
	.ops = _ops,						\
}

DEFINE_GATE_CLK(VCTCXO, MPMU_VRCR, 1, NULL, "VCTCXO");
DEFINE_GATE_CLK(dbgclk, APMU_TRACE, (1 << 3), NULL, "DBGCLK");
DEFINE_GATE_CLK(traceclk, APMU_TRACE, (1 << 4), NULL, "TRACECLK");

/* all clk src on the board */
static struct clk *pxa988_clks_src[] = {
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
};

/* soc peripheral clk on the board */
static struct clk *pxa988_clks_peri[] = {
	&pxa988_clk_sdh0,
	&pxa988_clk_sdh1,
	&pxa988_clk_sdh2,
	&gc_aclk,	/* internal clk node */
	&pxa988_clk_gc,
	&vpu_aclk,	/* internal clk node */
	&pxa988_clk_vpu,
	&lcd_ci_isp_axi_clk,
	&pxa988_lcd_ci_hclk,
	&pxa988_ccic_axi_clk,
	&pxa988_ccic_phy_clk,
	&pxa988_ccic_func_clk,
	&lcd_dsi_phy_clk,
	&pxa988_lcd_clk,
	&pxa988_isp_dxo_clk,
	&dbgclk,
	&traceclk,
	&twsi0_clk,
	&twsi1_clk,
	&twsi2_clk,
};

/* This clock is used to enable RTC module register r/w */
DEFINE_GATE_CLK(rtc_pe, APBC_PXA988_RTC, (1 << 7), NULL, "rtc_pe");

/* APB and some simple APMU clock */
static struct clk pxa988_list_clks[] = {
	/* APBC: _name, _dev, _con, _reg, _fnclksel, _rate, _parent*/
	APBC_CLK("uart0", "pxa2xx-uart.0", NULL,
		APBC_PXA988_UART0, 1, 14745600, NULL),/* CP uart */
	APBC_CLK("uart1", "pxa2xx-uart.1", NULL,
		APBC_PXA988_UART1, 1, 14745600, NULL),	/* AP uart0*/
	APBC_CLK("uart2", "pxa2xx-uart.2", NULL,
		APBC_PXA988_UART2, 1, 14745600, NULL),	/* AP uart1*/
	APBC_CLK("gpio", "pxa-gpio", NULL,
		APBC_PXA988_GPIO, 0, 13000000, NULL),
	APBC_CLK("ssp0", "pxa988-ssp.0", NULL,
		APBC_PXA988_SSP0, 4, 3250000, NULL),
#ifdef CONFIG_ISDBT
	APBC_CLK("ssp2", "pxa988-ssp.2", NULL,
		APBC_PXA988_SSP2, 1, 13000000, NULL),
#else
	APBC_CLK("ssp2", "pxa988-ssp.2", NULL,
		APBC_PXA988_SSP2, 2, 26000000, NULL),
#endif		
	APBC_CLK("keypad", "pxa27x-keypad", NULL,
		APBC_PXA988_KPC, 0, 32000, NULL),
	APBC_CLK("rtc", "sa1100-rtc", NULL,
		APBC_PXA988_RTC, 0, 32000, &rtc_pe),
	APBC_CLK("1wire", NULL, "PXA-W1",
		APBC_PXA988_ONEWIRE, 0, 26000000, NULL),
	APBC_CLK("thermal", NULL, "THERMALCLK",
		APBC_PXA988_DROTS, 0, 13000000, NULL),

	/* APBC_OPS: _name, _dev, _con, _reg, _fnclksel, _rate, _parent*/
	APBC_CLK_OPS("pwm0", "pxa910-pwm.0", NULL,
		APBC_PXA988_PWM0, 0, 13000000, NULL, &pwm_clk_ops),
	APBC_CLK_OPS("pwm1", "pxa910-pwm.1", NULL,
		APBC_PXA988_PWM1, 0, 13000000, NULL, &pwm_clk_ops),
	APBC_CLK_OPS("pwm2", "pxa910-pwm.2", NULL,
		APBC_PXA988_PWM2, 0, 13000000, NULL, &pwm_clk_ops),
	APBC_CLK_OPS("pwm3", "pxa910-pwm.3", NULL,
		APBC_PXA988_PWM3, 0, 13000000, NULL, &pwm_clk_ops),
	APBC_CLK_OPS("gssp", "pxa988-ssp.4", NULL,
		APBC_PXA988_GCER, 0, 0, NULL, &gssp_clk_ops),

	/* APMU: _name, _dev, _con, _reg, _eval, _rate, _parent */
	APBC_CLK_OPS("udc", NULL, "UDCCLK", APMU_USB,
			0x9, 480000000, NULL, &udc_clk_ops),
	APBC_CLK_OPS("ssp1", "pxa988-ssp.1", NULL,
		APBC_PXA988_SSP1, 0, 26000000, NULL, &ssp1_clk_ops),
	APMU_CLK("ire", "pxa910-ire.0", NULL, APMU_IRE,
			0x9, 480000000, NULL),
	APMU_CLK("aes", NULL, "AESCLK", APMU_GEU,
			0x9, 480000000, NULL),

	/* APMU: _name, _dev, _con, _reg, _eval, _rate, _parent , ops */
	APMU_CLK_OPS("nand", "pxa3xx-nand", NULL, APMU_NAND,
			0x19b, 156000000, NULL, &nand_clk_ops),
};

static void __init clk_misc_init(void)
{
	unsigned int dcg_regval = 0;
	/*
	 * pll2 default rate is different when using LPDDR400 and LPDDR533
	 * For max DDR rate 400M case
	 * 988 Z1/Z2 Safe PP solution:
	 * pll2 1200M for PP150/300/600/1200M
	 * pll2p 800M for PP800M
	 * 988 Z3/Ax decoupled PP solution:
	 * pll2 800M for CPU
	 * pll2p 800M for DDR
	 *
	 * For max DDR rate 500M case
	 * pll2 1066M for DDR and CPU
	 * pll2p 533M for other peripherals
	 * pll2/pll2p = pll2_vco/div (div = 1,2,3,4,6,8)
	 *
	 * pll3 VCO 2000M, pll3 500M for DSI, pll3p 1000M for CPU
	 * pll3/pll3p = pll3_vco/div (div = 1,2,3,4,6,8)
	 *
	 *  1088 temproraily uses Z3/Ax setting.
	 */
	if (cpu_is_z1z2()) {
		pll2_vco_default = 2400 * MHZ;
		pll2_default = 1200 * MHZ;
		pll2p_default = 800 * MHZ;
	} else {
		if (ddr_mode == 0) {
			pll2_vco_default = 1600 * MHZ;
			pll2_default = 800 * MHZ;
			pll2p_default = 800 * MHZ;
		} else if (ddr_mode == 1) {
			pll2_vco_default = 2132 * MHZ;
			pll2_default = 1066 * MHZ;
			pll2p_default = 1066 * MHZ;
		} else {
			/* use 800Mhz by default */
			pll2_vco_default = 1600 * MHZ;
			pll2_default = 800 * MHZ;
			pll2p_default = 800 * MHZ;
		}
	}

	pll3_vco_default = 1205 * MHZ;
	pll3_default = 1205 * MHZ;
	pll3p_default = 1205 * MHZ;

	/* DE suggest:enable SQU MP3 playback sleep mode */
	__raw_writel(__raw_readl(APMU_SQU_CLK_GATE_CTRL) | (1 << 30),
			APMU_SQU_CLK_GATE_CTRL);

	/* select i2s clock from VCTCXO , LP audio playback support */
	__raw_writel(__raw_readl(MPMU_FCCR) | (1 << 28), MPMU_FCCR);

	/* components' clock should always keep enabled */
	__raw_writel(0x3, APBC_PXA988_IPC);	/* ACIPC */
	__raw_writel(0x0, APBC_PXA988_RIPC);	/* RIPC */
	__raw_writel(0x3, APMU_MCK4_CTL);	/* MCK4 AHB */

	/* reset all SD Hosts to avoid protential IRQ storm */
	sdhc_reset_all();

	/* enable MC4 and AXI fabric dynamic clk gating */
	dcg_regval = __raw_readl(CIU_MC_CONF);
	/* disable cp fabric clk gating */
	dcg_regval &= ~(1 << 16);
	/* enable dclk gating */
	dcg_regval &= ~(1 << 19);
	if (cpu_pxa98x_stepping() <= PXA98X_Z3) {
		dcg_regval |= (1 << 9) | (1 << 18) | /* Seagull */
			(1 << 12) | (1 << 27) |  /* Fabric #2 */
			(1 << 15) | (1 << 20) | (1 << 21) | /* VPU*/
			(1 << 17) | (1 << 26); /* Fabric#1 CA9 */
	} else {
		dcg_regval |= (0xff << 8) | /* MCK4 P0~P7*/
			(1 << 17) | (1 << 18) | /* Fabric 0 */
			(1 << 20) | (1 << 21) |	/* VPU fabric */
			(1 << 26) | (1 << 27);  /* Fabric 0/1 */
	}
	__raw_writel(dcg_regval, CIU_MC_CONF);
}

static int trace_clock_setting_state(char *str)
{
	get_option(&str, &t32clock);
	printk(KERN_INFO "%s: APMU_Trace Clock : %d\n", __func__, t32clock);

	return t32clock;
}
__setup("traceclk=", trace_clock_setting_state);

static void __init clk_disable_unused_clock(void)
{
	unsigned int regval;
	/*
	 * disable nand controller clock as it is not used
	 * on 988
	 */
	__raw_writel(0, APMU_NAND);
	/*
	 * disable ase clock at init stage and security will
	 * enable it prior to use it
	 */
	__raw_writel(0, APMU_GEU);

	if (t32clock) {
		/* Do Nothing */
	} else {
		/* disable trace/debug clock */
		regval = __raw_readl(APMU_TRACE);
		regval &=~ ((1 << 3) | (1 << 4) | (1 << 16));
		__raw_writel(regval, APMU_TRACE);
	}
}
/*
 * init pll default output that used for pxa988
 * MUST call this function after pll2 and pll3 clock node is inited
 */
static void __init clk_pll_init(void)
{
	clk_set_rate(&pll2_vco, pll2_vco_default);
	clk_set_rate(&pll2, pll2_default);
	clk_set_rate(&pll2p, pll2p_default);

	clk_set_rate(&pll3_vco, pll3_vco_default);
	clk_set_rate(&pll3, pll3_default);
	clk_set_rate(&pll3p, pll3p_default);

	pr_info("PLL2 SWCR[%x] PLLCR[%x]\n",\
		__raw_readl(APB_SPARE_PLL2CR),
		__raw_readl(MPMU_PLL2CR));
	pr_info("PLL3 SWCR[%x] PLLCR[%x]\n",
		__raw_readl(APB_SPARE_PLL3CR),
		__raw_readl(MPMU_PLL3CR));
}

void pxa988_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);
}
EXPORT_SYMBOL(pxa988_init_one_clock);

static int __init pxa988_clk_init(void)
{
	int i;

#ifdef Z1_MCK4_SYNC_WORKAROUND
	/*
	 * Only Z1/Z2 needs mck4 workaround, disable this workaround
	 * for other chips
	 */
	if (!cpu_is_z1z2())
		mck4_wr_enabled = 0;
#endif
	clk_misc_init();

	for (i = 0; i < ARRAY_SIZE(pxa988_clks_src); i++)
		pxa988_init_one_clock(pxa988_clks_src[i]);
	for (i = 0; i < ARRAY_SIZE(pxa988_clks_peri); i++)
		pxa988_init_one_clock(pxa988_clks_peri[i]);
	for (i = 0; i < ARRAY_SIZE(pxa988_list_clks); i++)
		pxa988_init_one_clock(&pxa988_list_clks[i]);

	clk_pll_init();
	clk_disable_unused_clock();
	return 0;
}
core_initcall(pxa988_clk_init);

#ifdef CONFIG_DEBUG_FS
static void clk_dutycycle_stats(struct clk *clk,
	enum clk_stat_msg msg,
	struct clk_dc_stat_info *dc_stat_info,
	unsigned int tgtstate)
{
	struct timespec cur_ts, prev_ts;
	long time_ms;
	struct op_dcstat_info *cur, *tgt;

	/* do nothing if no stat operation is issued */
	if (!dc_stat_info->stat_start)
		return ;

	cur = &dc_stat_info->ops_dcstat[dc_stat_info->curopindex];
	getnstimeofday(&cur_ts);
	prev_ts = cur->prev_ts;
	time_ms = ts2ms(cur_ts, prev_ts);
	switch (msg) {
	case CLK_STAT_START:
		/* duty cycle stat start */
		cur->prev_ts = cur_ts;
		break;
	case CLK_STAT_STOP:
		/* duty cycle stat stop */
		if (clk->refcnt)
			cur->busy_time += time_ms;
		else
			cur->idle_time += time_ms;
		break;
	case CLK_STATE_ON:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		cur->idle_time += time_ms;
		break;
	case CLK_STATE_OFF:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		cur->busy_time += time_ms;
		break;
	case CLK_RATE_CHANGE:
		/* rate change from old->new */
		cur->prev_ts = cur_ts;
		if (clk->refcnt)
			cur->busy_time += time_ms;
		else
			cur->idle_time += time_ms;
		BUG_ON(tgtstate >= dc_stat_info->ops_stat_size);
		tgt = &dc_stat_info->ops_dcstat[tgtstate];
		tgt->prev_ts = cur_ts;
		break;
	default:
		break;
	}
}

int pxa988_clk_register_dcstat(struct clk *clk,
	unsigned long *opt, unsigned int opt_size)
{
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *clk_dcstat;
	unsigned int i, curpp_index = 0;

	/* search the list of the registation for this clk */
	list_for_each_entry(cdcs, &clk_dcstat_list, node)
		if (cdcs->clk == clk)
			break;

	/* if clk wasn't in the list, allocate new dcstat info */
	if (cdcs->clk != clk) {
		cdcs = kzalloc(sizeof(struct clk_dcstat), GFP_KERNEL);
		if (!cdcs)
			goto out;

		cdcs->clk = clk;
		/* allocate and fill dc stat information */
		clk_dcstat = &cdcs->clk_dcstat;
		clk_dcstat->ops_dcstat = kzalloc(opt_size * \
			sizeof(struct op_dcstat_info), GFP_KERNEL);
		if (!clk_dcstat->ops_dcstat) {
			pr_err("%s clk %s memory allocate failed!\n",
				__func__, clk->name);
			goto out1;
		}
		for (i = 0; i < opt_size; i++) {
			clk_dcstat->ops_dcstat[i].ppindex = i;
			clk_dcstat->ops_dcstat[i].pprate = opt[i];
			if (clk->rate == opt[i])
				curpp_index = i;
		}
		clk_dcstat->ops_stat_size = opt_size;
		clk_dcstat->stat_start = false;
		clk_dcstat->curopindex = curpp_index;

		list_add(&cdcs->node, &clk_dcstat_list);
	}

	return 0;
out1:
	kfree(cdcs);
out:
	return -ENOMEM;
}
EXPORT_SYMBOL(pxa988_clk_register_dcstat);

int pxa988_clk_dcstat_event(struct clk *clk,
	enum clk_stat_msg msg, unsigned int tgtstate)
{
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dcstat_info;
	int ret = 0;

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
		if (cdcs->clk == clk) {
			dcstat_info = &cdcs->clk_dcstat;
			clk_dutycycle_stats(clk, msg,
				dcstat_info, tgtstate);
			/*
			 * always update curopindex, no matter stat
			 * is started or not
			 */
			if (msg == CLK_RATE_CHANGE)
				dcstat_info->curopindex = tgtstate;
			break;
		}
	return ret;
}
EXPORT_SYMBOL(pxa988_clk_dcstat_event);

int pxa988_show_dc_stat_info(struct clk *clk, char *buf, ssize_t size)
{
	int len = 0;
	unsigned int i, dc_int, dc_fraction;
	long total_time = 0, run_total = 0, idle_total = 0;
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dc_stat_info = NULL;

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
		if (cdcs->clk == clk) {
			dc_stat_info = &cdcs->clk_dcstat;
			break;
		}

	if (!dc_stat_info) {
		pr_err("clk %s NULL dc stat info\n", clk->name);
		return -EINVAL;
	}

	if (dc_stat_info->stat_start) {
		len += snprintf(buf + len, size - len,
			"Please stop the %s duty cycle stats at first\n",
			clk->name);
		return len;
	}

	for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
		run_total += dc_stat_info->ops_dcstat[i].busy_time;
		idle_total += dc_stat_info->ops_dcstat[i].idle_time;
	}
	total_time = run_total + idle_total;
	if (!total_time) {
		len += snprintf(buf + len, size - len,
			"No stat information! ");
		len += snprintf(buf + len, size - len,
			"Help information :\n");
		len += snprintf(buf + len, size - len,
			"1. echo 1 to start duty cycle stat:\n");
		len += snprintf(buf + len, size - len,
			"2. echo 0 to stop duty cycle stat:\n");
		len += snprintf(buf + len, size - len,
			"3. cat to check duty cycle info from start to stop:\n\n");
		return len;
	}

	len += snprintf(buf + len, size - len, "\n");
	dc_int = total_time ?
		calculate_dc(run_total, total_time, &dc_fraction) : 0;
	dc_fraction = total_time ? dc_fraction : 0;
	len += snprintf(buf + len, size - len,
		"| CLK %s | %10s %lums| %10s %lums| %10s %2u.%2u%%|\n",
		clk->name, "idle time", idle_total,
		"total time",  total_time,
		"duty cycle", dc_int, dc_fraction);
	len += snprintf(buf + len, size - len,
		"| %3s | %12s | %15s | %15s | %15s |\n", "OP#",
		"rate(HZ)", "run time(ms)", "idle time(ms)", "rt ratio");
	for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
		dc_int = total_time ?
			calculate_dc(dc_stat_info->ops_dcstat[i].busy_time,
			total_time, &dc_fraction) : 0;
		dc_fraction = total_time ? dc_fraction : 0;
		len += snprintf(buf + len, size - len,
			"| %3u | %12lu | %15ld | %15ld | %12u.%2u%%|\n",
			dc_stat_info->ops_dcstat[i].ppindex,
			dc_stat_info->ops_dcstat[i].pprate,
			dc_stat_info->ops_dcstat[i].busy_time,
			dc_stat_info->ops_dcstat[i].idle_time,
			dc_int, dc_fraction);
	}
	return len;
}
EXPORT_SYMBOL(pxa988_show_dc_stat_info);

int pxa988_start_stop_dc_stat(struct clk *clk, unsigned int start)
{
	unsigned int i;
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dc_stat_info = NULL;

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
		if (cdcs->clk == clk) {
			dc_stat_info = &cdcs->clk_dcstat;
			break;
		}

	if (!dc_stat_info) {
		pr_err("clk %s NULL dc stat info\n", clk->name);
		return -EINVAL;
	}

	start = !!start;
	if (start == dc_stat_info->stat_start) {
		pr_err("[WARNING]%s stat is already %s\n",
			clk->name,
			dc_stat_info->stat_start ?\
			"started" : "stopped");
		return -EINVAL;
	}

	/*
	 * hold the same lock of clk_enable, disable, set_rate ops
	 * here to avoid the status change when start/stop and lead
	 * to incorrect stat info
	 */
	clk_get_lock(clk);
	if (start) {
		/* clear old stat information */
		for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
			dc_stat_info->ops_dcstat[i].idle_time = 0;
			dc_stat_info->ops_dcstat[i].busy_time = 0;
		}
		dc_stat_info->stat_start = true;
		clk_dutycycle_stats(clk, CLK_STAT_START,
			dc_stat_info, 0);
	} else {
		clk_dutycycle_stats(clk, CLK_STAT_STOP,
			dc_stat_info, 0);
		dc_stat_info->stat_start = false;
	}
	clk_release_lock(clk);
	return 0;
}
EXPORT_SYMBOL(pxa988_start_stop_dc_stat);

static ssize_t pxa988_gc_dc_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *p;
	int len = 0;
	size_t ret, size = PAGE_SIZE - 1;

	p = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!p)
		return -ENOMEM;

	len = pxa988_show_dc_stat_info(&pxa988_clk_gc, p, size);
	if (len == size)
		pr_warn("%s The dump buf is not large enough!\n", __func__);

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);
	free_pages((unsigned long)p, 0);
	return ret;
}

static ssize_t pxa988_gc_dc_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int start;
	char buf[10] = { 0 };
	size_t ret = 0;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%d", &start);
	ret = pxa988_start_stop_dc_stat(&pxa988_clk_gc, start);
	if (ret < 0)
		return ret;
	return count;
}

static const struct file_operations pxa988_gc_dc_ops = {
	.owner = THIS_MODULE,
	.read = pxa988_gc_dc_read,
	.write = pxa988_gc_dc_write,
};

static ssize_t pxa988_vpu_dc_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *p;
	int len = 0;
	size_t ret, size = PAGE_SIZE - 1;

	p = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!p)
		return -ENOMEM;

	len = pxa988_show_dc_stat_info(&pxa988_clk_vpu, p, size);
	if (len == size)
		pr_warn("%s The dump buf is not large enough!\n", __func__);

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);
	free_pages((unsigned long)p, 0);
	return ret;
}

static ssize_t pxa988_vpu_dc_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int start;
	char buf[10] = { 0 };
	size_t ret = 0;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%d", &start);
	ret = pxa988_start_stop_dc_stat(&pxa988_clk_vpu, start);
	if (ret < 0)
		return ret;
	return count;
}

static const struct file_operations pxa988_vpu_dc_ops = {
	.owner = THIS_MODULE,
	.read = pxa988_vpu_dc_read,
	.write = pxa988_vpu_dc_write,
};

static ssize_t pxa988_clk_stats_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	int len = 0, i, enabled, ret;
	unsigned int reg, size = PAGE_SIZE - 1;
	struct clk *temp;

	buf = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, size,
		       "|---------------|-------|\n|%14s\t|%s|\n"
		       "|---------------|-------|\n", "Clock Name", " Status");

	reg = __raw_readl(MPMU_PLL2CR);
	if (((reg & (3 << 8)) >> 8) == 2)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PLL2", "off");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PLL2", "on");

	reg = __raw_readl(MPMU_PLL3CR);
	if (((reg & (3 << 18)) >> 18) == 0)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PLL3", "off");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PLL3", "on");

	reg = __raw_readl(APMU_GC_CLK_RES_CTRL);
	if (((reg & (3 << 4)) >> 4) == 3)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "GC FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "GC FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "GC ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "GC ACLK", "off");

	reg = __raw_readl(APMU_VPU_CLK_RES_CTRL);
	if (((reg & (3 << 4)) >> 4) == 3)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "VPU FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "VPU FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "VPU ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "VPU ACLK", "off");

	reg = __raw_readl(APMU_LCD_CLK_RES_CTRL);
	if ((((reg & (1 << 1)) >> 1) == 1) && (((reg & (1 << 4)) >> 4) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD ACLK", "off");
	if ((((reg & (1 << 5)) >> 5) == 1) && (((reg & (1 << 2)) >> 2) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD HCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "LCD HCLK", "off");

	reg = __raw_readl(APMU_SDH0);
	if ((((reg & (1 << 3)) >> 3) == 1) && (((reg & (1 << 0)) >> 0) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH ACLK", "off");

	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH0 FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH0 FCLK", "off");

	reg = __raw_readl(APMU_SDH1);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH1 FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH1 FCLK", "off");

	reg = __raw_readl(APMU_SDH2);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH2 FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "SDH2 FCLK", "off");

	reg = __raw_readl(APMU_CCIC_RST);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC FCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC FCLK", "off");

	if ((((reg & (1 << 3)) >> 3) == 1) && (((reg & (1 << 0)) >> 0) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC ACLK", "off");
	if ((((reg & (1 << 5)) >> 5) == 1) && (((reg & (1 << 2)) >> 2) == 1))
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC PHYCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "CCIC PHYCLK", "off");

	reg = __raw_readl(APMU_DSI);
	if (((reg & (0xf << 2)) >> 2) == 0xf)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "DSI PHYCLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "DSI PHYCLK", "off");

	reg = __raw_readl(APMU_ISPDXO);
	if ((reg & 0xf03) == 0xf03)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "ISP_DXO CLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "ISP_DXO CLK", "off");

	reg = __raw_readl(APMU_TRACE);
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "DBG CLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "DBG CLK", "off");
	if (((reg & (1 << 4)) >> 4) == 1)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "TRACE CLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "TRACE CLK", "off");

	for (i = 0; i < ARRAY_SIZE(pxa988_list_clks); i++) {
		temp = &pxa988_list_clks[i];
		if (temp->ops == &apbc_clk_ops) {
			enabled = ((__raw_readl(temp->clk_rst) & 0x5) == 0x1);
			len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", temp->name,
				enabled ? "on" : "off");
		} else if (temp->ops == &apmu_clk_ops) {
			enabled = ((__raw_readl(temp->clk_rst) &
				  (temp->enable_val)) == (temp->enable_val));
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", temp->name,
					enabled ? "on" : "off");
		}
	}

	reg = __raw_readl(APBC_PXA988_PWM0);
	if ((reg & 0x1) == 0x1) {
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM0 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM0 CLK", "off");
		reg = __raw_readl(APBC_PXA988_PWM1);
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM1 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM1 CLK", "off");
	} else {
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PWM0 CLK", "off");
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PWM1 CLK", "off");
	}

	reg = __raw_readl(APBC_PXA988_PWM2);
	if ((reg & 0x1) == 0x1) {
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM2 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM2 CLK", "off");
		reg = __raw_readl(APBC_PXA988_PWM3);
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM3 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM3 CLK", "off");
	} else {
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PWM2 CLK", "off");
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "PWM3 CLK", "off");
	}

	reg = __raw_readl(APMU_USB);
	if ((reg & 0x9) == 0x9)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "USB ACLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "USB ACLK", "off");

	reg = __raw_readl(APMU_NAND);
	if ((reg & 0x19b) == 0x19b)
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "NAND CLK", "on");
	else
		len += snprintf(buf + len, size,
			       "|%14s\t|%5s\t|\n", "NAND CLK", "off");

	len += snprintf(buf + len, size, "|---------------|-------|\n\n");

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, 0);
	return ret;
}

static const struct file_operations pxa988_clk_stats_ops = {
	.owner = THIS_MODULE,
	.read = pxa988_clk_stats_read,
};

struct dentry *stat;
static int __init __init_dcstat_debugfs_node(void)
{
	struct dentry *gc_dc_stat, *vpu_dc_stat, *clock_status;

	stat = debugfs_create_dir("stat", pxa);
	if (!stat)
		return -ENOENT;

	gc_dc_stat = debugfs_create_file("gc_dc_stat", 0664,
		stat, NULL, &pxa988_gc_dc_ops);
	if (!gc_dc_stat)
		return -ENOENT;

	vpu_dc_stat = debugfs_create_file("vpu_dc_stat", 0664,
		stat, NULL, &pxa988_vpu_dc_ops);
	if (!vpu_dc_stat)
		goto err_vpu_dc_stat;
	clock_status = debugfs_create_file("clock_status", 0444,
					   pxa, NULL, &pxa988_clk_stats_ops);
	if (!clock_status)
		goto err_clk_stats;

	return 0;

err_clk_stats:
	debugfs_remove(vpu_dc_stat);
err_vpu_dc_stat:
	debugfs_remove(gc_dc_stat);
	return -ENOENT;
}
late_initcall(__init_dcstat_debugfs_node);
#endif
