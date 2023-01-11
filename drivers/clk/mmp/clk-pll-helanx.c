/*
 * helanx PLL clock operation source file
 *
 * Copyright (C) 2012 Marvell
 * Zhoujie Wu <zjwu@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <trace/events/power.h>

#include "clk.h"
#include "clk-pll-helanx.h"

#define pll_readl(reg) readl_relaxed(reg)
#define pll_readl_cr(p) pll_readl(p->params->cr_reg)
#define pll_readl_pll_swcr(p) pll_readl(p->params->pll_swcr)

#define pll_writel(val, reg) writel_relaxed(val, reg)
#define pll_writel_cr(val, p) pll_writel(val, p->params->cr_reg)
#define pll_writel_pll_swcr(val, p) pll_writel(val, p->params->pll_swcr)

#define pll_readl_sscctrl(ssc_params)	pll_readl(ssc_params->ssc_ctrl)
#define pll_readl_ssccfg(ssc_params)	pll_readl(ssc_params->ssc_cfg)

#define pll_writel_sscctrl(val, ssc_params) \
	pll_writel(val, ssc_params->ssc_ctrl)
#define pll_writel_ssccfg(val, ssc_params) \
	pll_writel(val, ssc_params->ssc_cfg)

/* unified pllx_cr */
union pllx_cr {
	struct {
		unsigned int refdiv:5;
		unsigned int fbdiv:9;
		unsigned int reserved:5;
		unsigned int pu:1;
		unsigned int reserved1:12;
	} b;
	unsigned int v;
};

/* unified pll SW control register */
union pllx_swcr {
	struct {
		unsigned int avvd1815_sel:1;
		unsigned int vddm:2;
		unsigned int vddl:3;
		unsigned int icp:4;
		unsigned int pll_bw_sel:1;
		unsigned int kvco:4;
		unsigned int ctune:2;
		unsigned int diff_div_sel:3;
		unsigned int se_div_sel:3;
		unsigned int diff_en:1;
		unsigned int bypass_en:1;
		unsigned int se_gating_en:1;
		unsigned int fd:3;
		unsigned int reserved:3;
	} b;
	unsigned int v;
};

/* 28nm PLL ssc related */
union pllx_ssc_ctrl {
	struct {
		unsigned int pi_en:1;
		unsigned int reset_pi:1;
		unsigned int ssc_mode:1;
		unsigned int ssc_clk_en:1;
		unsigned int reset_ssc:1;
		unsigned int pi_loop_mode:1;
		unsigned int clk_det_en:1;
		unsigned int reserved:9;
		unsigned int intpi:4;
		unsigned int intpr:3;
		unsigned int reserved1:9;
	} b;
	unsigned int v;
};

union pllx_ssc_conf {
	struct {
		unsigned int ssc_rnge:11;
		unsigned int reserved:5;
		unsigned int ssc_freq_div:16;
	} b;
	unsigned int v;
};

static unsigned int __get_vco_freq(struct clk_hw *hw);

static struct intpi_range pll_intpi_tbl[] = {
	{2500, 3000, 8},
	{2000, 2500, 6},
	{1500, 2000, 5},
};

static unsigned int __clk_pll_vco2intpi(struct clk_vco *vco)
{
	unsigned int intpi, vco_rate, i, itp_tbl_size;
	struct intpi_range *itp_tbl;
	vco_rate = __get_vco_freq(&vco->hw) / MHZ;
	intpi = 6;

	itp_tbl = pll_intpi_tbl;
	itp_tbl_size = ARRAY_SIZE(pll_intpi_tbl);
	for (i = 0; i < itp_tbl_size; i++) {
		if ((vco_rate >= itp_tbl[i].vco_min) &&
		    (vco_rate <= itp_tbl[i].vco_max)) {
			intpi = itp_tbl[i].value;
			break;
		}
	}
	if (i == itp_tbl_size)
		BUG_ON("Unsupported vco frequency for INTPI!\n");

	return intpi;
}

/* Real amplitude percentage = amplitude / base */
static void __clk_get_sscdivrng(enum ssc_mode mode,
				unsigned long rate,
				unsigned int amplitude, unsigned int base,
				unsigned long vco, unsigned int *div,
				unsigned int *rng)
{
	unsigned int vco_avg;
	if (amplitude > (50 * base / 1000))
		BUG_ON("Amplitude can't exceed 5\%\n");
	switch (mode) {
	case CENTER_SPREAD:
		/* VCO_CLK_AVG = REFCLK * (4*N /M) */
		vco_avg = vco;
		/* SSC_FREQ_DIV = VCO_CLK _AVG /
		   (4*4 * Desired_Modulation_Frequency) */
		*div = (vco_avg / rate) >> 4;
		break;
	case DOWN_SPREAD:
		/* VCO_CLK_AVG= REFCLK * (4*N /M)*(1-Desired_SSC_Amplitude/2) */
		vco_avg = vco - (vco >> 1) / base * amplitude;
		/* SSC_FREQ_DIV = VCO_CLK_AVG /
		   (4*2 * Desired_Modulation_Frequency) */
		*div = (vco_avg / rate) >> 3;
		break;
	default:
		pr_err("Unsupported SSC MODE!\n");
		return;
	}
	if (*div == 0)
		*div = 1;
	/* SSC_RNGE = Desired_SSC_Amplitude / (SSC_FREQ_DIV * 2^-26) */
	*rng = (1 << 26) / (*div * base / amplitude);
}

/* only change config when vco output changes */
static void config_ssc(struct clk_vco *vco, unsigned long new_rate)
{
	struct ssc_params *ssc_params = vco->params->ssc_params;
	unsigned int div = 0, rng = 0;
	union pllx_ssc_conf ssc_conf;

	__clk_get_sscdivrng(ssc_params->ssc_mode,
			    ssc_params->desired_mod_freq,
			    ssc_params->amplitude,
			    ssc_params->base, new_rate, &div, &rng);

	ssc_conf.v = pll_readl_ssccfg(ssc_params);
	ssc_conf.b.ssc_freq_div = div & 0xfff0;
	ssc_conf.b.ssc_rnge = rng;
	pll_writel_ssccfg(ssc_conf.v, ssc_params);
}

static void __enable_ssc(struct ssc_params *ssc_params,
	unsigned int intpi)
{
	union pllx_ssc_ctrl ssc_ctrl;

	ssc_ctrl.v = pll_readl_sscctrl(ssc_params);
	ssc_ctrl.b.intpi = intpi;
	ssc_ctrl.b.intpr = 4;
	ssc_ctrl.b.ssc_mode = ssc_params->ssc_mode;
	ssc_ctrl.b.pi_en = 1;
	ssc_ctrl.b.clk_det_en = 1;
	ssc_ctrl.b.reset_pi = 1;
	ssc_ctrl.b.reset_ssc = 1;
	ssc_ctrl.b.pi_loop_mode = 0;
	ssc_ctrl.b.ssc_clk_en = 0;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);

	udelay(2);
	ssc_ctrl.b.reset_ssc = 0;
	ssc_ctrl.b.reset_pi = 0;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);

	udelay(2);
	ssc_ctrl.b.pi_loop_mode = 1;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);

	udelay(2);
	ssc_ctrl.b.ssc_clk_en = 1;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);
}

static void enable_pll_ssc(struct clk_vco *vco)
{
	struct ssc_params *ssc_params = vco->params->ssc_params;
	unsigned int intpi = __clk_pll_vco2intpi(vco);

	__enable_ssc(ssc_params, intpi);
}

static void __disable_ssc(struct ssc_params *ssc_params)
{
	union pllx_ssc_ctrl ssc_ctrl;

	ssc_ctrl.v = pll_readl_sscctrl(ssc_params);
	ssc_ctrl.b.ssc_clk_en = 0;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);
	udelay(100);

	ssc_ctrl.b.pi_loop_mode = 0;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);
	udelay(2);

	ssc_ctrl.b.pi_en = 0;
	pll_writel_sscctrl(ssc_ctrl.v, ssc_params);
}

static void disable_pll_ssc(struct clk_vco *vco)
{
	struct ssc_params *ssc_params = vco->params->ssc_params;

	__disable_ssc(ssc_params);
}

static struct kvco_range kvco_rng_table_28nm[] = {
	{2600, 3000, 0xf, 0},
	{2400, 2600, 0xe, 0},
	{2200, 2400, 0xd, 0},
	{2000, 2200, 0xc, 0},
	{1750, 2000, 0xb, 0},
	{1500, 1750, 0xa, 0},
	{1350, 1500, 0x9, 0},
	{1200, 1350, 0x8, 0},
};

static void __clk_vco_rate2rng(struct clk_vco *vco, unsigned long rate,
			       unsigned int *kvco, unsigned int *vco_rng)
{
	struct kvco_range *kvco_rng_tbl;
	int i, size;

	kvco_rng_tbl = kvco_rng_table_28nm;
	size = ARRAY_SIZE(kvco_rng_table_28nm);

	for (i = 0; i < size; i++) {
		if (rate >= kvco_rng_tbl[i].vco_min &&
		    rate <= kvco_rng_tbl[i].vco_max) {
			*kvco = kvco_rng_tbl[i].kvco;
			*vco_rng = kvco_rng_tbl[i].vrng;
			return;
		}
	}
	BUG_ON(i == size);
	return;
}

static int __pll_is_enabled(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	union pllx_cr pllx_cr;

	pllx_cr.v = pll_readl_cr(vco);
	/*
	 * PLLxCR[19:18] = 0x1, 0x2, 0x3 means PLL3 is enabled.
	 * PLLxCR[19:18] = 0x0 means PLL3 is disabled
	 */
	if (pllx_cr.b.pu)
		return 1;
	return 0;
}

/* frequency unit Mhz, return pll vco freq */
static unsigned int __get_vco_freq(struct clk_hw *hw)
{
	u32 val;
	unsigned int pllx_vco, pllxrefd, pllxfbd;
	struct clk_vco *vco = to_clk_vco(hw);
	union pllx_cr pllx_cr;

	if (!__pll_is_enabled(hw))
		return 0;

	val = pll_readl_cr(vco);
	pllx_cr.v = val;
	pllxrefd = pllx_cr.b.refdiv;
	pllxfbd = pllx_cr.b.fbdiv;

	if (pllxrefd == 0)
		pllxrefd = 1;
	pllx_vco = DIV_ROUND_UP(4 * 26 * pllxfbd, pllxrefd);

	hw->clk->rate = pllx_vco * MHZ;
	return pllx_vco * MHZ;
}

static void __pll_vco_cfg(struct clk_vco *vco)
{
	union pllx_swcr pllx_swcr;
	pllx_swcr.v = pll_readl_pll_swcr(vco);
	pllx_swcr.b.avvd1815_sel = 1;
	pllx_swcr.b.vddm = 1;
	pllx_swcr.b.vddl = 4;
	pllx_swcr.b.icp = 3;
	pllx_swcr.b.pll_bw_sel = 0;
	pllx_swcr.b.ctune = 1;
	pllx_swcr.b.diff_en = 1;
	pllx_swcr.b.bypass_en = 0;
	pllx_swcr.b.se_gating_en = 0;
	pllx_swcr.b.fd = 4;
	pll_writel_pll_swcr(pllx_swcr.v, vco);
}

static void clk_pll_vco_init(struct clk_hw *hw)
{
	struct clk_vco *vco = to_clk_vco(hw);
	unsigned long vco_rate;
	unsigned int vco_rngl, vco_rngh, tmp;
	struct mmp_vco_params *params = vco->params;

	if (!__pll_is_enabled(hw)) {
		pr_info("%s is not enabled\n", hw->clk->name);
		__pll_vco_cfg(vco);
	} else {
		vco_rate = __get_vco_freq(hw) / MHZ;
		if (vco->flags & HELANX_PLL_SKIP_DEF_RATE) {
			hw->clk->rate = vco_rate * MHZ;
		} else {
			/* check whether vco is in the range of 2% our expectation */
			tmp = params->default_rate / MHZ;
			if (tmp != vco_rate) {
				vco_rngh = tmp + tmp * 2 / 100;
				vco_rngl = tmp - tmp * 2 / 100;
				BUG_ON(!((vco_rngl <= vco_rate) &&
					 (vco_rate <= vco_rngh)));
			}
			hw->clk->rate = params->default_rate;
		}
		/* Make sure SSC is enabled if pll is on */
		if (vco->flags & HELANX_PLL_SSC_FEAT) {
			config_ssc(vco, hw->clk->rate);
			enable_pll_ssc(vco);
			params->ssc_enabled = true;
		}
		pr_info("%s is enabled @ %lu\n", hw->clk->name, vco_rate * MHZ);
	}
}

static int clk_pll_vco_enable(struct clk_hw *hw)
{
	unsigned int delaytime = 14;
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pllx_cr pllx_cr;

	if (__pll_is_enabled(hw))
		return 0;

	spin_lock_irqsave(vco->lock, flags);
	pllx_cr.v = pll_readl_cr(vco);
	pllx_cr.b.pu = 1;
	pll_writel_cr(pllx_cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);

	/* check lock status */
	udelay(30);
	while ((!(__raw_readl(params->lock_reg) & params->lock_enable_bit))
	       && delaytime) {
		udelay(5);
		delaytime--;
	}
	if (unlikely(!delaytime))
		BUG_ON(1);

	trace_clock_enable(hw->clk->name, 1, 0);
	if (vco->flags & HELANX_PLL_SSC_FEAT)
		if (((vco->flags & HELANX_PLL_SSC_AON) && !params->ssc_enabled)
		    || !(vco->flags & HELANX_PLL_SSC_AON)) {
			enable_pll_ssc(vco);
			params->ssc_enabled = true;
		}

	return 0;
}

static void clk_pll_vco_disable(struct clk_hw *hw)
{
	unsigned long flags;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pllx_cr pllx_cr;

	spin_lock_irqsave(vco->lock, flags);
	pllx_cr.v = pll_readl_cr(vco);
	pllx_cr.b.pu = 0;
	pll_writel_cr(pllx_cr.v, vco);
	spin_unlock_irqrestore(vco->lock, flags);
	trace_clock_disable(hw->clk->name, 0, 0);

	if ((vco->flags & HELANX_PLL_SSC_FEAT) &&
	    !(vco->flags & HELANX_PLL_SSC_AON)) {
		disable_pll_ssc(vco);
		params->ssc_enabled = false;
	}
}

/*
 * pll rate change requires sequence:
 * clock off -> change rate setting -> clock on
 * This function doesn't really change rate, but cache the config
 */
static int clk_pll_vco_setrate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	unsigned int i, kvco = 0, vcovnrg, refd, fbd;
	unsigned long flags;
	unsigned long new_rate = rate, old_rate = hw->clk->rate;
	struct clk_vco *vco = to_clk_vco(hw);
	struct mmp_vco_params *params = vco->params;
	union pllx_swcr pllx_swcr;
	union pllx_cr pllx_cr;

	if (__pll_is_enabled(hw)) {
		pr_err("%s %s is enabled, ignore the setrate!\n",
		       __func__, hw->clk->name);
		return 0;
	}

	rate /= MHZ;
	/* setp 1: calculate fbd refd kvco and vcovnrg */
	if (params->freq_table) {
		for (i = 0; i < params->freq_table_size; i++) {
			if (rate == params->freq_table[i].output_rate) {
				kvco = params->freq_table[i].kvco;
				vcovnrg = params->freq_table[i].vcovnrg;
				refd = params->freq_table[i].refd;
				fbd = params->freq_table[i].fbd;
				break;
			}
		}
		BUG_ON(i == params->freq_table_size);
	} else {
		__clk_vco_rate2rng(vco, rate, &kvco, &vcovnrg);
		/* FIXME */
		/* refd need be calculated by certain function
		   other than a fixed number */
		refd = 3;
		fbd = rate * refd / 104;
	}

	spin_lock_irqsave(vco->lock, flags);
	/* setp 2: set pll kvco setting */
	pllx_swcr.v = pll_readl_pll_swcr(vco);
	pllx_swcr.b.kvco = kvco;
	pll_writel_pll_swcr(pllx_swcr.v, vco);

	/* setp 3: set pll fbd and refd setting */
	pllx_cr.v = pll_readl_cr(vco);
	pllx_cr.b.refdiv = refd;
	pllx_cr.b.fbdiv = fbd;
	pll_writel_cr(pllx_cr.v, vco);

	hw->clk->rate = new_rate;
	spin_unlock_irqrestore(vco->lock, flags);

	if (vco->flags & HELANX_PLL_SSC_FEAT)
		config_ssc(vco, new_rate);

	pr_debug("%s %s rate %lu->%lu!\n", __func__,
		 hw->clk->name, old_rate, new_rate);
	trace_clock_set_rate(hw->clk->name, new_rate, 0);
	return 0;
}

static unsigned long clk_vco_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	return hw->clk->rate;
}

static long clk_vco_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *prate)
{
	struct clk_vco *vco = to_clk_vco(hw);
	int fbd, refd = 3, div;
	unsigned long max_rate = 0;
	int i;
	struct mmp_vco_params *params = vco->params;

	if (rate > params->vco_max || rate < params->vco_min) {
		pr_err("%lu rate out of range!\n", rate);
		return -EINVAL;
	}

	if (params->freq_table) {
		for (i = 0; i < params->freq_table_size; i++) {
			if (params->freq_table[i].output_rate <= rate) {
				if (max_rate <
				    params->freq_table[i].output_rate)
					max_rate =
					    params->freq_table[i].output_rate;
			}
		}
	} else {
		div = 104;
		rate = rate / MHZ;
		fbd = rate * refd / div;
		max_rate = DIV_ROUND_UP(div * fbd, refd);
		max_rate *= MHZ;
	}
	return max_rate;
}

static struct clk_ops clk_vco_ops = {
	.init = clk_pll_vco_init,
	.enable = clk_pll_vco_enable,
	.disable = clk_pll_vco_disable,
	.set_rate = clk_pll_vco_setrate,
	.recalc_rate = clk_vco_recalc_rate,
	.round_rate = clk_vco_round_rate,
	.is_enabled = __pll_is_enabled,
};

/* PLL post divider table for 28nm pll */
static struct div_map pll_post_div_tbl_28nm[] = {
	/* divider, reg vaule */
	{1, 0x0},
	{2, 0x1},
	{4, 0x2},
	{8, 0x3},
	{16, 0x4},
	{32, 0x5},
	{64, 0x6},
	{128, 0x7},
};

static unsigned int __clk_pll_calc_div(struct clk_pll *pll,
				       unsigned long rate,
				       unsigned long parent_rate,
				       unsigned int *div)
{
	struct div_map *div_map;
	int i, size;

	div_map = pll_post_div_tbl_28nm;
	size = ARRAY_SIZE(pll_post_div_tbl_28nm);

	*div = 0;
	rate /= MHZ;
	parent_rate /= MHZ;

	/* Choose the min divider, max freq */
	for (i = 1; i < size; i++) {
		if ((rate <= (parent_rate / div_map[i - 1].div)) &&
		    (rate > (parent_rate / div_map[i].div))) {
			*div = div_map[i - 1].div;
			return div_map[i - 1].hw_val;
		}
	}
	/* rate is higher than all acceptable rates, uses the min divider */
	*div = div_map[0].div;
	return div_map[0].hw_val;
}

/* convert post div reg setting to divider val */
static unsigned int __pll_div_hwval2div(struct clk_vco *vco,
					unsigned int hw_val)
{
	struct div_map *div_map;
	int i, size;

	div_map = pll_post_div_tbl_28nm;
	size = ARRAY_SIZE(pll_post_div_tbl_28nm);

	for (i = 0; i < size; i++) {
		if (hw_val == div_map[i].hw_val)
			return div_map[i].div;
	}
	BUG_ON(i == size);
	return 0;
}

static void clk_pll_init(struct clk_hw *hw)
{
	unsigned long parent_rate;
	struct clk_pll *pll = to_clk_pll(hw);
	int div, div_hw;
	union pllx_swcr pllx_swcr;
	struct clk *parent = hw->clk->parent;
	struct clk_vco *vco = to_clk_vco(parent->hw);

	if (!__pll_is_enabled(parent->hw)) {
		pr_info("%s is not enabled\n", hw->clk->name);
		return;
	}

	if (!(pll->flags & (HELANX_PLLOUT | HELANX_PLLOUTP)))
		BUG_ON("unknow pll type!\n");

	parent_rate = clk_get_rate(parent) / MHZ;

	pllx_swcr.v = pll_readl_pll_swcr(pll);
	if (pll->flags & HELANX_PLLOUT)
		div_hw = pllx_swcr.b.se_div_sel;
	else
		div_hw = pllx_swcr.b.diff_div_sel;
	div = __pll_div_hwval2div(vco, div_hw);
	hw->clk->rate = parent_rate / div * MHZ;
	pr_info("%s is enabled @ %lu\n", hw->clk->name, hw->clk->rate);
}

static int clk_pll_setrate(struct clk_hw *hw, unsigned long new_rate,
			   unsigned long best_parent_rate)
{
	unsigned int div_hwval, div, swcr;
	unsigned long flags, old_rate = hw->clk->rate;
	struct clk_pll *pll = to_clk_pll(hw);
	struct clk *parent = hw->clk->parent;
	union pllx_swcr pllx_swcr;

	if (__pll_is_enabled(parent->hw)) {
		pr_info("%s %s is enabled, ignore the setrate!\n",
			__func__, hw->clk->name);
		return 0;
	}

	if (!(pll->flags & (HELANX_PLLOUT | HELANX_PLLOUTP)))
		BUG_ON("unknow pll type!\n");

	div_hwval = __clk_pll_calc_div(pll, new_rate, best_parent_rate, &div);

	spin_lock_irqsave(pll->lock, flags);
	pllx_swcr.v = pll_readl_pll_swcr(pll);
	if (pll->flags & HELANX_PLLOUT)
		pllx_swcr.b.se_div_sel = div_hwval;
	else
		pllx_swcr.b.diff_div_sel = div_hwval;
	swcr = pllx_swcr.v;
	pll_writel_pll_swcr(swcr, pll);
	hw->clk->rate = new_rate;
	spin_unlock_irqrestore(pll->lock, flags);

	pr_debug("%s %s rate %lu->%lu!\n", __func__,
		 hw->clk->name, old_rate, new_rate);
	trace_clock_set_rate(hw->clk->name, new_rate, 0);
	return 0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	return hw->clk->rate;
}

static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *prate)
{
	struct clk *parent = hw->clk->parent;
	struct clk_vco *vco = to_clk_vco(parent->hw);
	struct mmp_vco_params *vco_params = vco->params;
	int i;
	unsigned long delta, new_rate, max_rate = 0, parent_rate;
	bool need_chg_prate = false;
	u32 div_map_size;
	struct div_map *div_map;

	div_map = pll_post_div_tbl_28nm;
	div_map_size = ARRAY_SIZE(pll_post_div_tbl_28nm);
	delta = 104 / 3;

	parent_rate = *prate / MHZ;
	rate /= MHZ;

	if (rate <= parent_rate) {
		for (i = 0; i < div_map_size; i++) {
			new_rate = parent_rate / div_map[i].div;
			if (new_rate <= rate)
				if (max_rate < new_rate)
					max_rate = new_rate;
		}
		if (hw->clk->flags & CLK_SET_RATE_PARENT) {
			if (abs(rate - max_rate) <= delta)
				return max_rate * MHZ;
			else
				need_chg_prate = true;
		} else
			return max_rate * MHZ;
	}
	if ((rate > parent_rate) || need_chg_prate) {
		if (!(hw->clk->flags & CLK_SET_RATE_PARENT)) {
			WARN_ON(1);
			return parent_rate;
		}
		for (i = 0; i < div_map_size; i++) {
			max_rate = rate * div_map[i].div * MHZ;
			if (max_rate <= vco_params->vco_max &&
			    max_rate >= vco_params->vco_min)
				break;
		}
		*prate = rate * MHZ * div_map[i].div;
	}
	return rate * MHZ;
}

static int clk_pll_is_enabled(struct clk_hw *hw)
{
	struct clk *parent = hw->clk->parent;
	return __pll_is_enabled(parent->hw);
}

static struct clk_ops clk_pll_ops = {
	.init = clk_pll_init,
	.set_rate = clk_pll_setrate,
	.recalc_rate = clk_pll_recalc_rate,
	.round_rate = clk_pll_round_rate,
	.is_enabled = clk_pll_is_enabled,
};

struct clk *helanx_clk_register_vco(const char *name,
				    const char *parent_name,
				    unsigned long flags, u32 vco_flags,
				    spinlock_t *lock,
				    struct mmp_vco_params *params)
{
	struct clk_vco *vco;
	struct clk *clk;
	struct clk_init_data init;

	vco = kzalloc(sizeof(*vco), GFP_KERNEL);
	if (!vco)
		return NULL;

	init.name = name;
	init.ops = &clk_vco_ops;
	init.flags = flags | CLK_SET_RATE_GATE;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	vco->flags = vco_flags;
	vco->lock = lock;
	vco->hw.init = &init;
	vco->params = params;

	clk = clk_register(NULL, &vco->hw);
	if (IS_ERR(clk))
		kfree(vco);

	return clk;
}

struct clk *helanx_clk_register_pll(const char *name,
				    const char *parent_name,
				    unsigned long flags, u32 pll_flags,
				    spinlock_t *lock,
				    struct mmp_pll_params *params)
{
	struct clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return NULL;

	init.name = name;
	init.ops = &clk_pll_ops;
	init.flags = flags | CLK_SET_RATE_GATE;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->flags = pll_flags;
	pll->lock = lock;
	pll->params = params;
	pll->hw.init = &init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}
