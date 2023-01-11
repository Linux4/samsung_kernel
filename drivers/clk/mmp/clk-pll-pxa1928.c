/*
 * PXA1928 PLL clock operation source file
 *
 * Copyright (C) 2013 Marvell
 * Guoqing Li <ligq@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "clk-pll-pxa1928.h"

#define PLL_PI_LOOP_MODE(x)	((x) << 22)
#define PLL_OUT_EN	(1 << 9)
#define PLL_NOT_DIV_3	(1 << 30)
#define PLL_SYNC_DDR	(1 << 30)
#define TO_MHZ(x)	((x) >> 20)


struct pxa1928_clk_pll_vco {
	struct clk_hw	hw;
	unsigned long	flags;
	spinlock_t	*lock;
	void __iomem	*mpmu_base;
	void __iomem	*apmu_base;
	struct pxa1928_clk_pll_vco_params	*params;
	struct pxa1928_clk_pll_vco_table	*freq_table;
	struct pxa1928_clk_pll_vco_table	freq_rounded;
};

#define to_pxa1928_clk_vco(_hw) \
	container_of(_hw, struct pxa1928_clk_pll_vco, hw)
#define to_pxa1928_clk_pll(_hw) \
	container_of(_hw, struct pxa1928_clk_pll_out, hw)
#define PLL_MASK(n) ((1 << (n)) - 1)

#define readl_mpmu(pll, offset) \
	readl_relaxed(pll->mpmu_base + pll->params->offset)
#define writel_mpmu(val, pll, offset) \
	writel_relaxed(val, pll->mpmu_base + pll->params->offset)

#define readl_apmu(pll, offset) \
	readl_relaxed(pll->apmu_base + pll->params->offset)
#define writel_apmu(val, pll, offset) \
	writel_relaxed(val, pll->apmu_base + pll->params->offset)

union pxa1928_pll_cr {
	struct {
		u32 reserved0:8;  /* 7:0 */
		u32 sw_en:1;      /* 8 */
		u32 ctrl:1;       /* 9 */
		u32 fbdiv:9;      /* 18:10 */
		u32 refdiv:9;     /* 27:19 */
		u32 reserved1:4;  /* 31:28 */
	} bit;
	u32 val;
};

union pxa1928_pll_ctrl1 {
	struct {
		u32 vddl:3;          /* 2:0 */
		u32 bw_sel:1;        /* 3 */
		u32 vddm:2;          /* 5:4 */
		u32 reserved0:9;     /* 14:6 */
		u32 ctune:2;         /* 16:15 */
		u32 reserved1:1;     /* 17 */
		u32 kvco:4;          /* 21:18 */
		u32 icp:4;           /* 15:22 */
		u32 post_div_sel:3;    /* 28:26 */
		u32 pll_rst:1;       /* 29 */
		u32 ctrl1_pll_bit:1; /* 30 */
		u32 bypass_en:1;     /* 31 */
	} bit;
	u32 val;
};

union pxa1928_pll_ctrl2 {
	struct {
		u32 src_sel:1;           /* 0 */
		u32 intpi:4;             /* 4:1 */
		u32 pi_en:1;             /* 5 */
		u32 ssc_clk_en:1;        /* 6 */
		u32 clk_det_en:1;        /* 7 */
		u32 freq_offset_en:1;    /* 8 */
		u32 freq_offset:17;      /* 25:9 */
		u32 freq_offset_mode:1;  /* 26 */
		u32 freq_offset_valid:1; /* 27 */
		u32 reserved:4;          /* 31:28 */
	} bit;
	u32 val;
};

union pxa1928_pll_ctrl3 {
	struct {
		u32 reserved0:1;     /* 0 */
		u32 ssc_mode:1;      /* 1 */
		u32 ssc_freq_div:16; /* 17:2 */
		u32 ssc_rnge:11;     /* 28:18 */
		u32 reserved1:3;     /* 31:29 */
	} bit;
	u32 val;
};

static unsigned int __pxa1928_clk_vco_is_enabled(
			struct pxa1928_clk_pll_vco *vco)
{
	union pxa1928_pll_cr pll_cr;
	unsigned long flags;

	spin_lock_irqsave(vco->lock, flags);
	pll_cr.val = readl_mpmu(vco, reg_pll_cr);
	spin_unlock_irqrestore(vco->lock, flags);
	/*
	 * ctrl = 0(hw enable) or ctrl = 1&&en = 1(sw enable)
	 * ctrl = 1&&en = 0(sw disable)
	 */
	return (pll_cr.bit.ctrl && (!pll_cr.bit.sw_en)) ? 0 : 1;
}

static int pxa1928_clk_pll_vco_is_enabled(struct clk_hw *hw)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);

	return __pxa1928_clk_vco_is_enabled(vco);
}

static int pxa1928_clk_pll_vco_enable(struct clk_hw *hw)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);
	union pxa1928_pll_cr pll_cr;
	union pxa1928_pll_ctrl1 pll_ctrl1;
	unsigned long flags;
	u32 val, count = 1000;

	if (__pxa1928_clk_vco_is_enabled(vco)) {
		spin_lock_irqsave(vco->lock, flags);
		pll_cr.val = readl_mpmu(vco, reg_pll_cr);
		if (pll_cr.bit.ctrl) {
			/* maybe enabled in u-boot, clear ctrl bit */
			pll_cr.bit.ctrl = 0;
			writel_mpmu(pll_cr.val, vco, reg_pll_cr);
		}
		spin_unlock_irqrestore(vco->lock, flags);

		return 0;
	}

	spin_lock_irqsave(vco->lock, flags);
	/* 1. Enable PLL. */
	pll_cr.val = readl_mpmu(vco, reg_pll_cr);
	pll_cr.bit.ctrl = 1;
	pll_cr.bit.sw_en = 1;
	writel_mpmu(pll_cr.val, vco, reg_pll_cr);

	/* 2. Release the PLL out of reset. */
	pll_ctrl1.val = readl_mpmu(vco, reg_pll_ctrl1);
	pll_ctrl1.bit.pll_rst = 1;
	writel_mpmu(pll_ctrl1.val, vco, reg_pll_ctrl1);

	/* 3. Wait 50us (PLL stable time) */
	udelay(30);
	while ((!(readl_mpmu(vco, reg_pll_lock) & vco->params->lock_bit))
			&& count)
		count--;
	if (unlikely(!count))
		BUG_ON("PLL could not be locked after enabled!\n");

	/* 4. Release PLL clock gating */
	val = readl_apmu(vco, reg_pll_gate);
	val &= ~(PLL_MASK(vco->params->gate_width) << vco->params->gate_shift);
	writel_apmu(val, vco, reg_pll_gate);
	pll_cr.bit.ctrl = 0;
	writel_mpmu(pll_cr.val, vco, reg_pll_cr);
	if (vco->flags & PXA1928_PLL_POST_ENABLE) {
		val = readl_apmu(vco, reg_glb_clk_ctrl) | (0xff << 8);
		writel_apmu(val, vco, reg_glb_clk_ctrl);
	}
	spin_unlock_irqrestore(vco->lock, flags);

	return 0;
}

static void pxa1928_clk_pll_vco_disable(struct clk_hw *hw)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);
	union pxa1928_pll_cr pll_cr;
	union pxa1928_pll_ctrl1 pll_ctrl1;
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(vco->lock, flags);
	if (vco->flags & PXA1928_PLL_POST_ENABLE) {
		val = readl_apmu(vco, reg_glb_clk_ctrl) & (~(0xff << 8));
		writel_apmu(val, vco, reg_glb_clk_ctrl);
	}
	/* 1. Gate PLL. */
	val = readl_apmu(vco, reg_pll_gate);
	val |= PLL_MASK(vco->params->gate_width) << vco->params->gate_shift;
	writel_apmu(val, vco, reg_pll_gate);

	/* 2. Reset PLL. */
	pll_ctrl1.val = readl_mpmu(vco, reg_pll_ctrl1);
	pll_ctrl1.bit.pll_rst = 0;
	writel_mpmu(pll_ctrl1.val, vco, reg_pll_ctrl1);

	/* 3. Disable PLL. */
	pll_cr.val = readl_mpmu(vco, reg_pll_cr);
	pll_cr.bit.ctrl = 1;
	pll_cr.bit.sw_en = 0;
	writel_mpmu(pll_cr.val, vco, reg_pll_cr);
	spin_unlock_irqrestore(vco->lock, flags);
}

static int __pxa1928_vco_get_table_rate(struct pxa1928_clk_pll_vco *vco,
		unsigned long rate, unsigned long prate)
{
	struct pxa1928_clk_pll_vco_table *sel;
	struct pxa1928_clk_pll_vco_table *freq_tbl = &vco->freq_rounded;

	for (sel = vco->freq_table; sel->input_rate != 0; sel++)
		if ((TO_MHZ(sel->input_rate)) == (TO_MHZ(prate)) &&
			(TO_MHZ(sel->output_rate)) == TO_MHZ(rate))
			break;
	if (sel->input_rate == 0)
		return -EINVAL;

	freq_tbl->input_rate = sel->input_rate;
	freq_tbl->output_rate = sel->output_rate;
	freq_tbl->output_rate_offseted = sel->output_rate_offseted;
	freq_tbl->refdiv = sel->refdiv;
	freq_tbl->fbdiv = sel->fbdiv;
	freq_tbl->icp = sel->icp;
	freq_tbl->kvco = sel->kvco;
	freq_tbl->ssc_en = sel->ssc_en;
	freq_tbl->offset_en = sel->offset_en;

	return 0;
}

static int __pxa1928_vco_get_kvco(unsigned long rate)
{
	if (rate < 1350000000)
		return 0x8;
	else if (rate >= 1350000000 && rate < 1500000000)
		return 0x9;
	else if (rate >= 1500000000 && rate < 1750000000)
		return 0xa;
	else if (rate >= 1750000000 && rate < 2000000000)
		return 0xb;
	else if (rate >= 2000000000 && rate < 2200000000UL)
		return 0xc;
	else if (rate >= 2200000000UL && rate < 2400000000UL)
		return 0xd;
	else if (rate >= 2400000000UL && rate < 2600000000UL)
		return 0xe;
	else
		return 0xf;
}

static int __pxa1928_vco_calc_rate(struct pxa1928_clk_pll_vco *vco,
		unsigned long rate, unsigned long prate)
{
	struct pxa1928_clk_pll_vco_table *freq_tbl = &vco->freq_rounded;
	int kvco;
	u64 div_result;

	if (rate < vco->params->vco_min ||
			rate > vco->params->vco_max) {
		pr_err("vco rate out of range\n");
		return -EINVAL;
	}

	freq_tbl->input_rate = prate;
	freq_tbl->refdiv = 3;
	div_result = (u64)rate * freq_tbl->refdiv;
	do_div(div_result, prate << 2);
	freq_tbl->fbdiv = (u16)div_result;
	div_result = (u64)(prate << 2) * freq_tbl->fbdiv;
	do_div(div_result, freq_tbl->refdiv);
	freq_tbl->output_rate = (unsigned long)div_result;
	freq_tbl->output_rate_offseted = rate;
	freq_tbl->icp = 3;
	kvco = __pxa1928_vco_get_kvco(freq_tbl->output_rate);
	if (kvco < 0) {
		pr_err("could not get kvco\n");
		return -EINVAL;
	}
	freq_tbl->kvco = kvco;
	freq_tbl->ssc_en = 0;
	freq_tbl->offset_en = 0;

	return 0;
}

static void __pxa1928_vco_set_offset(struct pxa1928_clk_pll_vco *vco,
		struct pxa1928_clk_pll_vco_table *freq_tbl)
{
	union pxa1928_pll_ctrl2 pll_ctrl2;
	u32 freq_offset_up, freq_offset_value;
	int offset;
	unsigned long flags;

	spin_lock_irqsave(vco->lock, flags);
	pll_ctrl2.val = readl_mpmu(vco, reg_pll_ctrl2);
	/*
	 * enable freq offset
	 *    fvco = 4*refclk*fbdiv/refdiv/(1+offset_percent)
	 */
	/* 1. offset valid mode */
	pll_ctrl2.bit.freq_offset_mode = 0;

	/* 2. set freq_offset */
	offset = freq_tbl->output_rate - freq_tbl->output_rate_offseted;
	freq_offset_up = offset > 0 ? 0 : 0x10000;

	/*
	 * offset_percent = (fvco-fvco_offseted)/fvco_offseted
	 * freq_offset_value =
	 *     2^20*(abs(offset_percent)/(1+offset_percent)
	 *     2^20*(abs(offset)/fvco)
	 * offset_percent should be less than +-5%.
	 */
	if (abs(offset) * 100 / freq_tbl->output_rate_offseted > 5) {
		freq_offset_value = 49932;
		pr_warn("vco offset too large\n");
	} else
		freq_offset_value = 0xffff &
			((1 << 20) *
			 (abs(offset) / freq_tbl->output_rate));
	freq_offset_value |= freq_offset_up;
	pll_ctrl2.bit.freq_offset_en = 1;
	pll_ctrl2.bit.freq_offset_valid = 1;
	pll_ctrl2.bit.freq_offset = freq_offset_value;
	writel_mpmu(pll_ctrl2.val, vco, reg_pll_ctrl2);
	spin_unlock_irqrestore(vco->lock, flags);
}

static void __pxa1928_vco_set_ssc(struct pxa1928_clk_pll_vco *vco,
		struct pxa1928_clk_pll_vco_table *freq_tbl)
{
	union pxa1928_pll_ctrl2 pll_ctrl2;
	union pxa1928_pll_ctrl3 pll_ctrl3;
	u32 desired_modu_freq, desired_ssc_amp;
	unsigned long flags;

	spin_lock_irqsave(vco->lock, flags);
	pll_ctrl3.val = readl_mpmu(vco, reg_pll_ctrl3);
	/* select desired_modu_freq 30K as default */
	desired_modu_freq = 30000;
	/* select desired_ssc_amplitude 2.5% */
	desired_ssc_amp = 25;
	/* center mode */
	pll_ctrl3.bit.ssc_mode = 0;
	/* ssc_freq_div = fvco/(4*4*desired_modu_freq) */
	pll_ctrl3.bit.ssc_freq_div =
		(freq_tbl->output_rate >> 4) / desired_modu_freq;
	/* ssc_rnge = desired_ssc_amp/(ssc_freq_div*2^(-28)) */
	pll_ctrl3.bit.ssc_rnge = desired_ssc_amp * ((1 << 28) /
		(1000 * pll_ctrl3.bit.ssc_freq_div));
	writel_mpmu(pll_ctrl3.val, vco, reg_pll_ctrl3);

	pll_ctrl2.val = readl_mpmu(vco, reg_pll_ctrl2);
	pll_ctrl2.bit.ssc_clk_en = 1;
	writel_mpmu(pll_ctrl2.val, vco, reg_pll_ctrl2);
	spin_unlock_irqrestore(vco->lock, flags);
	/* FIXME: wait >1us (SSC reset time) */
	udelay(5);
}

static unsigned int __pxa1928_vco_get_intpi(unsigned long output_rate)
{
	if (output_rate <= 2000000000UL)
		return 0x5;
	else if (output_rate <= 2500000000UL)
		return 0x6;
	else
		return 0x8;
}

static void __pxa1928_vco_set_pi(struct pxa1928_clk_pll_vco *vco,
		struct pxa1928_clk_pll_vco_table *freq_tbl, u32 pi_en)
{
	union pxa1928_pll_ctrl2 pll_ctrl2;
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(vco->lock, flags);
	pll_ctrl2.val = readl_mpmu(vco, reg_pll_ctrl2);
	pll_ctrl2.bit.pi_en = pi_en;
	pll_ctrl2.bit.clk_det_en = pi_en;
	pll_ctrl2.bit.intpi = __pxa1928_vco_get_intpi(freq_tbl->output_rate);
	writel_mpmu(pll_ctrl2.val, vco, reg_pll_ctrl2);

	val = readl_mpmu(vco, reg_pll_ctrl4);
	val &= ~PLL_PI_LOOP_MODE(1);
	val |= PLL_PI_LOOP_MODE(pi_en);
	writel_mpmu(val, vco, reg_pll_ctrl4);
	spin_unlock_irqrestore(vco->lock, flags);
	if (pi_en)
		/* wait 5us (PI stable time) */
		udelay(10);
}

static unsigned long pxa1928_clk_vco_recalc_rate(struct clk_hw *hw,
			unsigned long prate)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);
	union pxa1928_pll_cr pll_cr;
	unsigned long flags;
	u64 div_result;

	spin_lock_irqsave(vco->lock, flags);
	pll_cr.val = readl_mpmu(vco, reg_pll_cr);
	spin_unlock_irqrestore(vco->lock, flags);

	div_result = (u64)(prate << 2) * pll_cr.bit.fbdiv;
	do_div(div_result, pll_cr.bit.refdiv);

	return (unsigned long)div_result;
}

static int pxa1928_clk_pll_vco_setrate(struct clk_hw *hw, unsigned long rate,
			unsigned long prate)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);
	struct pxa1928_clk_pll_vco_table *freq_tbl = &vco->freq_rounded;
	union pxa1928_pll_cr pll_cr;
	union pxa1928_pll_ctrl1 pll_ctrl1;
	union pxa1928_pll_ctrl2 pll_ctrl2;
	unsigned long flags;
	u32 pi_en = 0;

	if (unlikely(__pxa1928_clk_vco_is_enabled(vco))) {
		pr_info("%s already enabled, could not set rate\n",
				 __clk_get_name(hw->clk));
		return -EPERM;
	}

	if (TO_MHZ(rate) == TO_MHZ(pxa1928_clk_vco_recalc_rate(hw, prate)))
		return 0;

	spin_lock_irqsave(vco->lock, flags);
	/* 1. Program REFDIV and FBDIV value. */
	pll_cr.val = readl_mpmu(vco, reg_pll_cr);
	pll_cr.bit.refdiv = freq_tbl->refdiv;
	pll_cr.bit.fbdiv = freq_tbl->fbdiv;
	writel_mpmu(pll_cr.val, vco, reg_pll_cr);

	/* 2. Program BYPASS_EN, ICP, KVCO, BW_SEL */
	pll_ctrl1.val = readl_mpmu(vco, reg_pll_ctrl1);
	pll_ctrl1.bit.bypass_en = 0;
	pll_ctrl1.bit.bw_sel = 0;
	pll_ctrl1.bit.icp = freq_tbl->icp;
	pll_ctrl1.bit.kvco = freq_tbl->kvco;
	writel_mpmu(pll_ctrl1.val, vco, reg_pll_ctrl1);
	pll_ctrl2.val = readl_mpmu(vco, reg_pll_ctrl2);
	pll_ctrl2.bit.ssc_clk_en = 0;
	pll_ctrl2.bit.freq_offset_en = 0;
	writel_mpmu(pll_ctrl2.val, vco, reg_pll_ctrl2);
	spin_unlock_irqrestore(vco->lock, flags);

	if (freq_tbl->offset_en || freq_tbl->ssc_en)
		pi_en = 1;
	__pxa1928_vco_set_pi(vco, freq_tbl, pi_en);

	if (freq_tbl->offset_en)
		__pxa1928_vco_set_offset(vco, freq_tbl);

	if (freq_tbl->ssc_en)
		__pxa1928_vco_set_ssc(vco, freq_tbl);


	return 0;
}

static long pxa1928_clk_vco_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *prate)
{
	struct pxa1928_clk_pll_vco *vco = to_pxa1928_clk_vco(hw);

	if (__pxa1928_vco_get_table_rate(vco, rate, *prate) &&
		__pxa1928_vco_calc_rate(vco, rate, *prate)) {
		pr_warn("vco round rate failed\n");
		return -EINVAL;
	}

	return vco->freq_rounded.output_rate;
}

static struct clk_ops pxa1928_clk_vco_ops = {
	.enable = pxa1928_clk_pll_vco_enable,
	.disable = pxa1928_clk_pll_vco_disable,
	.set_rate = pxa1928_clk_pll_vco_setrate,
	.recalc_rate = pxa1928_clk_vco_recalc_rate,
	.round_rate = pxa1928_clk_vco_round_rate,
	.is_enabled = pxa1928_clk_pll_vco_is_enabled,
};

struct clk *pxa1928_clk_register_pll_vco(const char *name,
		const char *parent_name,
		unsigned long flags, unsigned long vco_flags, spinlock_t *lock,
		void __iomem *mpmu_base, void __iomem *apmu_base,
		struct pxa1928_clk_pll_vco_params *params,
		struct pxa1928_clk_pll_vco_table *freq_table)
{
	struct pxa1928_clk_pll_vco *vco;
	struct clk *clk;
	struct clk_init_data init;

	vco = kzalloc(sizeof(*vco), GFP_KERNEL);
	if (!vco)
		return NULL;

	init.name = name;
	init.ops = &pxa1928_clk_vco_ops;
	init.flags = flags;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	vco->flags = vco_flags;
	vco->lock = lock;
	vco->hw.init = &init;
	vco->mpmu_base = mpmu_base;
	vco->apmu_base = apmu_base;
	vco->params = params;
	vco->freq_table = freq_table;

	clk = clk_register(NULL, &vco->hw);
	if (IS_ERR(clk))
		kfree(vco);

	return clk;
}

static void __pxa1928_clk_pll_out_enable(struct pxa1928_clk_pll_out *pll,
					int en)
{
	unsigned long flags;
	u32 val;

	if (pll->flags & PXA1928_PLL_USE_ENABLE_BIT) {
		spin_lock_irqsave(pll->lock, flags);
		val = readl_mpmu(pll, reg_pll_out_div);
		val &= ~PLL_OUT_EN;
		if (en)
			val |= PLL_OUT_EN;
		writel_mpmu(val, pll, reg_pll_out_div);
		spin_unlock_irqrestore(pll->lock, flags);
	}
}

static int pxa1928_clk_pll_out_enable(struct clk_hw *hw)
{
	struct pxa1928_clk_pll_out *pll = to_pxa1928_clk_pll(hw);

	__pxa1928_clk_pll_out_enable(pll, 1);
	return 0;
}

static void pxa1928_clk_pll_out_disable(struct clk_hw *hw)
{
	struct pxa1928_clk_pll_out *pll = to_pxa1928_clk_pll(hw);

	__pxa1928_clk_pll_out_enable(pll, 0);
}

static int __pxa1928_get_pll_out_table_rate(struct pxa1928_clk_pll_out *pll,
		struct pxa1928_clk_pll_out_table *freq_tbl,
		unsigned long rate, unsigned long prate)
{
	struct pxa1928_clk_pll_out_table *sel;

	for (sel = pll->freq_table; sel->input_rate != 0; sel++)
		if (TO_MHZ(sel->input_rate) == TO_MHZ(prate) &&
			TO_MHZ(sel->output_rate) == TO_MHZ(rate))
			break;
	if (sel->input_rate == 0)
		return -EINVAL;

	freq_tbl->input_rate = sel->input_rate;
	freq_tbl->output_rate = sel->output_rate;
	freq_tbl->div_sel = sel->div_sel;

	return 0;
}

static int __pxa1928_pll_out_calc_rate(struct pxa1928_clk_pll_out *pll,
		struct pxa1928_clk_pll_out_table *freq_tbl,
		unsigned long rate, unsigned long prate)
{
	int div_sel;

	for (div_sel = 0; div_sel < PLL_MASK(pll->params->div_width) + 1;
			div_sel++) {
		if (TO_MHZ(rate * 3) == TO_MHZ(prate)) {
			freq_tbl->input_rate = prate;
			freq_tbl->output_rate = rate;
			freq_tbl->div_sel = PXA1928_PLL_DIV_3;
			return 0;
		}
		if (TO_MHZ(rate << div_sel) == TO_MHZ(prate))
			break;
	}

	if (div_sel == PLL_MASK(pll->params->div_width) + 1) {
		div_sel = 0;
		while ((rate << div_sel) < pll->params->input_rate_min)
			div_sel++;
		if (unlikely(div_sel > PLL_MASK(pll->params->div_width))) {
			pr_warn("pll output rate was too low\n");
			return -EINVAL;
		}
	}

	if (!div_sel)
		/*
		 * FIXME: according to the DE's suggestion,
		 * it's not allowed for divider (1 << 0), so set it
		 * as 1 << 1 at least.
		 */
		div_sel = 1;
	freq_tbl->input_rate = rate << div_sel;
	freq_tbl->output_rate = rate;
	freq_tbl->div_sel = div_sel;

	return 0;
}

static unsigned long pxa1928_clk_out_recalc_rate(struct clk_hw *hw,
			unsigned long prate)
{
	struct pxa1928_clk_pll_out *pll = to_pxa1928_clk_pll(hw);
	unsigned long flags;
	u32 val, div_sel;

	spin_lock_irqsave(pll->lock, flags);
	val = readl_mpmu(pll, reg_pll_out_ctrl);
	if (pll->flags & PXA1928_PLL_USE_DIV_3) {
		if (!(val & PLL_NOT_DIV_3)) {
			spin_unlock_irqrestore(pll->lock, flags);
			return prate / 3;
		}
	}

	if (pll->flags & PXA1928_PLL_USE_SYNC_DDR)
		val = readl_apmu(pll, reg_pll_out_div);
	else
		val = readl_mpmu(pll, reg_pll_out_div);
	spin_unlock_irqrestore(pll->lock, flags);
	div_sel = (val >> pll->params->div_shift) &
		PLL_MASK(pll->params->div_width);

	return prate >> div_sel;
}

static int pxa1928_clk_pll_out_setrate(struct clk_hw *hw,
		unsigned long rate, unsigned long prate)
{
	struct pxa1928_clk_pll_out *pll = to_pxa1928_clk_pll(hw);
	struct pxa1928_clk_pll_out_table freq_tbl;
	unsigned long flags;
	int is_ddr_sync = 0;
	u32 val;

	if (TO_MHZ(rate) == TO_MHZ(pxa1928_clk_out_recalc_rate(hw, prate)))
		return 0;

	if (__pxa1928_get_pll_out_table_rate(pll, &freq_tbl, rate, prate) &&
		__pxa1928_pll_out_calc_rate(pll, &freq_tbl, rate, prate)) {
		pr_warn("pll set rate failed\n");
		return -EINVAL;
	}

	spin_lock_irqsave(pll->lock, flags);
	val = readl_mpmu(pll, reg_pll_out_ctrl);
	if (pll->flags & PXA1928_PLL_USE_DIV_3) {
		val |= PLL_NOT_DIV_3;
		if (freq_tbl.div_sel == PXA1928_PLL_DIV_3)
			val &= ~PLL_NOT_DIV_3;
		writel_mpmu(val, pll, reg_pll_out_ctrl);
	}

	if (pll->flags & PXA1928_PLL_USE_SYNC_DDR) {
		val |= PLL_SYNC_DDR;
		writel_mpmu(val, pll, reg_pll_out_ctrl);
		is_ddr_sync = 1;
	}

	if (is_ddr_sync)
		val = readl_apmu(pll, reg_pll_out_div);
	else
		val = readl_mpmu(pll, reg_pll_out_div);
	val &= ~(PLL_MASK(pll->params->div_width) << pll->params->div_shift);
	val |= (freq_tbl.div_sel & PLL_MASK(pll->params->div_width)) <<
		pll->params->div_shift;
	if (is_ddr_sync)
		writel_apmu(val, pll, reg_pll_out_div);
	else
		writel_mpmu(val, pll, reg_pll_out_div);
	spin_unlock_irqrestore(pll->lock, flags);

	return 0;
}

static long pxa1928_clk_out_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *prate)
{
	struct pxa1928_clk_pll_out *pll = to_pxa1928_clk_pll(hw);
	struct pxa1928_clk_pll_out_table freq_tbl;

	if (__pxa1928_get_pll_out_table_rate(pll, &freq_tbl, rate, *prate) &&
		__pxa1928_pll_out_calc_rate(pll, &freq_tbl, rate, *prate)) {
		pr_warn("pll round rate failed\n");
		return -EINVAL;
	}

	*prate = freq_tbl.input_rate;

	return freq_tbl.output_rate;
}

static struct clk_ops pxa1928_clk_pll_out_ops = {
	.enable = pxa1928_clk_pll_out_enable,
	.disable = pxa1928_clk_pll_out_disable,
	.set_rate = pxa1928_clk_pll_out_setrate,
	.recalc_rate = pxa1928_clk_out_recalc_rate,
	.round_rate = pxa1928_clk_out_round_rate,
};

struct clk *pxa1928_clk_register_pll_out(const char *name,
		const char *parent_name,
		unsigned long pll_flags, spinlock_t *lock,
		void __iomem *mpmu_base, void __iomem *apmu_base,
		struct pxa1928_clk_pll_out_params *params,
		struct pxa1928_clk_pll_out_table *freq_table)
{
	struct pxa1928_clk_pll_out *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return NULL;

	init.name = name;
	init.ops = &pxa1928_clk_pll_out_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->flags = pll_flags;
	pll->lock = lock;
	pll->hw.init = &init;
	pll->mpmu_base = mpmu_base;
	pll->apmu_base = apmu_base;
	pll->params = params;
	pll->freq_table = freq_table;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}
