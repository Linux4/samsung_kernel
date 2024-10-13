/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_lock.h>

#include "clk-whale.h"

#ifdef CONFIG_OF

#define clk_debug(format, arg...) pr_debug("clk: " "@@@%s: " format, __func__, ## arg)
#define clk_info(format, arg...) pr_info("clk: " "@@@%s: " format, __func__, ## arg)

struct cfg_reg {
	void __iomem *reg;
	u32 msk;
};

struct clk_sprd {
	struct clk_hw hw;
	struct cfg_reg enb;
	u8 flags;
	union {
		unsigned long fixed_rate;
		u32 c_mul;
		struct cfg_reg mul, sel;
		struct clk_hw *mux_hw;
	} m;
	union {
		u32 c_div;
		struct cfg_reg div, pre;
		struct clk_hw *div_hw;
	} d;
};

#define to_clk_sprd(_hw) container_of(_hw, struct clk_sprd, hw)

#define in_range(b, first, len)	((b) >= (first) && (b) <= (first) + (len) - 1)
#define to_range(b, first, base) ((b) - (first) + (base))

static inline unsigned long cfg_reg_p2v(const u32 regp)
{
	return SPRD_DEV_P2V(regp);
/*	u32 regv = 0;

	if (0) {
	} else if (in_range(regp, SPRD_AHB_PHYS, SPRD_AHB_SIZE)) {
		regv = to_range(regp, SPRD_AHB_PHYS, SPRD_AHB_BASE);
	} else if (in_range(regp, SPRD_PMU_PHYS, SPRD_PMU_SIZE)) {
		regv = to_range(regp, SPRD_PMU_PHYS, SPRD_PMU_BASE);
	} else if (in_range(regp, SPRD_AONAPB_PHYS, SPRD_AONAPB_SIZE)) {
		regv = to_range(regp, SPRD_AONAPB_PHYS, SPRD_AONAPB_BASE);
	} else if (in_range(regp, SPRD_AONCKG_PHYS, SPRD_AONCKG_SIZE)) {
		regv = to_range(regp, SPRD_AONCKG_PHYS, SPRD_AONCKG_BASE);
	} else if (in_range(regp, SPRD_GPUCKG_PHYS, SPRD_GPUCKG_SIZE)) {
		regv = to_range(regp, SPRD_GPUCKG_PHYS, SPRD_GPUCKG_BASE);
	} else if (in_range(regp, SPRD_GPUAPB_PHYS, SPRD_GPUAPB_SIZE)) {
		regv = to_range(regp, SPRD_GPUAPB_PHYS, SPRD_GPUAPB_BASE);
	} else if (in_range(regp, SPRD_MMCKG_PHYS, SPRD_MMCKG_SIZE)) {
		regv = to_range(regp, SPRD_MMCKG_PHYS, SPRD_MMCKG_BASE);
	} else if (in_range(regp, SPRD_MMAHB_PHYS, SPRD_MMAHB_SIZE)) {
		regv = to_range(regp, SPRD_MMAHB_PHYS, SPRD_MMAHB_BASE);
	} else if (in_range(regp, SPRD_APBREG_PHYS, SPRD_APBREG_SIZE)) {
		regv = to_range(regp, SPRD_APBREG_PHYS, SPRD_APBREG_BASE);
	} else if (in_range(regp, SPRD_APBCKG_PHYS, SPRD_APBCKG_SIZE)) {
		regv = to_range(regp, SPRD_APBCKG_PHYS, SPRD_APBCKG_BASE);
	} else {
		WARN(1, "regp %08x\n", regp);
	}

	return regv;*/
}

static inline void of_read_reg(struct cfg_reg *cfg, const __be32 * cell)
{
	if (!cell)
		return;
	cfg->reg = (void *)cfg_reg_p2v(be32_to_cpu(*(cell++)));
	cfg->msk = be32_to_cpu(*(cell++));
}

static inline void __glbreg_setclr(struct clk_hw *hw, void *reg, u32 msk,
				   int is_set)
{
	unsigned long flags;
	if (!reg)
		return;

	clk_debug("%s %s %p[%x]\n", (hw) ? __clk_get_name(hw->clk) : NULL,
		  (is_set) ? "SET" : "CLR", reg, (u32) msk);

	__arch_default_lock(HWLOCK_GLB, &flags);

	if (is_set)
		__raw_writel(__raw_readl(reg) | msk, reg);
	else
		__raw_writel(__raw_readl(reg) & ~msk, reg);

	__arch_default_unlock(HWLOCK_GLB, &flags);
}

#define __glbreg_set(hw, reg, msk)	__glbreg_setclr(hw, reg, msk, 1)
#define __glbreg_clr(hw, reg, msk)	__glbreg_setclr(hw, reg, msk, 0)

static inline unsigned int __pll_get_refin_rate(void *reg, u32 msk)
{
	const unsigned long refin[4] = { 2000000, 4000000, 13000000, 26000000 };
	u32 i;
	i = (__raw_readl(reg) & msk) >> __ffs(msk);
	return refin[i]/1000000;
}

static int sprd_clk_prepare(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int set = !!(c->flags & CLK_GATE_SET_TO_DISABLE);

	__glbreg_setclr(hw, c->d.pre.reg, (u32) c->d.pre.msk, set ^ 1);
	return 0;
}

static int sprd_pll_clk_prepare(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int set = !!(c->flags & CLK_GATE_SET_TO_DISABLE);

	__glbreg_setclr(hw, c->d.pre.reg, (u32) c->d.pre.msk, set ^ 1);
	udelay(1000);

	return 0;
}

static void sprd_clk_unprepare(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int set = !!(c->flags & CLK_GATE_SET_TO_DISABLE);
	__glbreg_setclr(hw, c->d.pre.reg, (u32) c->d.pre.msk, set ^ 0);
}

static int sprd_clk_is_prepared(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int ret, set = !!(c->flags & CLK_GATE_SET_TO_DISABLE);

	if (!c->d.pre.reg)
		return 0;

	/* if a set bit prepare this gate, flip it before masking */
	ret = !!(__raw_readl(c->d.pre.reg) & BIT(c->d.pre.msk));
	return set ^ ret;
}

#define __SPRD_MM_TIMEOUT            (3 * 1000)

/* FIXME: sharkls no chip macro */
#if defined(CONFIG_MACH_SP9830I) || defined(CONFIG_ARCH_SCX30G2) || defined(CONFIG_ARCH_SCX35LT8)
static int sprd_clk_coda7_is_ready(void)
{
	u32 power_state1, power_state2, power_state3;
	unsigned long timeout = jiffies + msecs_to_jiffies(__SPRD_MM_TIMEOUT);

	do {
		cpu_relax();
		power_state1 =
			__raw_readl((void *)REG_CODEC_AHB_CODA7_STAT) & BIT_CODA7_RUN;
		power_state2 =
			__raw_readl((void *)REG_CODEC_AHB_CODA7_STAT) & BIT_CODA7_RUN;
		power_state3 =
			__raw_readl((void *)REG_CODEC_AHB_CODA7_STAT) & BIT_CODA7_RUN;
		BUG_ON(time_after(jiffies, timeout));
	} while (power_state1 != power_state2 || power_state2 != power_state3);

	if (!power_state1)
		return 1;

	return 0;
}
#else
static int sprd_clk_coda7_is_ready(void)
{
	return 1;
}
#endif

static int sprd_clk_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct clk_sprd *c = to_clk_sprd(hw);

	if (!strcmp(__clk_get_name(hw->clk), "clk_coda7_axi") ||
			!strcmp(__clk_get_name(hw->clk), "clk_coda7_cc") ||
			!strcmp(__clk_get_name(hw->clk), "clk_coda7_apb")) {
		__arch_default_lock(HWLOCK_GLB, &flags);
		if (!strcmp(__clk_get_name(hw->clk), "clk_coda7_apb"))
			__raw_writel(__raw_readl(c->enb.reg) & (~((u32)c->enb.msk)), c->enb.reg);
		else
			__raw_writel(__raw_readl(c->enb.reg) | (u32)c->enb.msk, c->enb.reg);

		__arch_default_unlock(HWLOCK_GLB, &flags);
		BUG_ON(!sprd_clk_coda7_is_ready());
	} else {
		if (c->enb.reg)
			__raw_writel(__raw_readl(c->enb.reg) | (u32)c->enb.msk, c->enb.reg);
	}

	return 0;
}

static void sprd_clk_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct clk_sprd *c = to_clk_sprd(hw);

	if (!strcmp(__clk_get_name(hw->clk), "clk_coda7_axi") ||
			!strcmp(__clk_get_name(hw->clk), "clk_coda7_cc") ||
			!strcmp(__clk_get_name(hw->clk), "clk_coda7_apb")) {
		__arch_default_lock(HWLOCK_GLB, &flags);
		if (!strcmp(__clk_get_name(hw->clk), "clk_coda7_apb")) {
			__raw_writel(__raw_readl(c->enb.reg) | (u32)c->enb.msk, c->enb.reg);
		} else {
			__raw_writel(__raw_readl(c->enb.reg) & (~((u32)c->enb.msk)), c->enb.reg);
		}
		__arch_default_unlock(HWLOCK_GLB, &flags);
	} else {
		__glbreg_clr(hw, c->enb.reg, (u32) c->enb.msk);
	}
}

static int sprd_clk_is_enable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int ret = !!(__raw_readl(c->enb.reg) & BIT(c->enb.msk));
	return ret;
}

static unsigned long sprd_clk_fixed_pll_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	return to_clk_sprd(hw)->m.fixed_rate;
}

/*
 How To look PLL Setting
 div_s = 1	 sdm_en = 1  Fout = Fin * ( Nint + Kint/1048576)
 div_s = 1	 sdm_en = 0  Fout = Fin * Nint
 div_s = 0	 sdm_en = x  Fout = Fin * N
 */

static long sprd_clk_adjustable_pll_round_rate(struct clk_hw *hw,
					       unsigned long rate,
					       unsigned long *prate)
{
	/*struct clk_sprd *pll = to_clk_sprd(hw);*/
	clk_debug("rate %lu, %lu\n", rate, *prate);
	return rate;
}

static unsigned long sprd_clk_whale_pll_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	unsigned long rate;
	unsigned long kint = 0, nint, cfg1, cfg2, n, refin;
	struct whale_pll_config *pwhale_pll_config = whale_pll_config_table;

	for (; pwhale_pll_config->name != NULL; pwhale_pll_config++) {
		if (!strcmp(pwhale_pll_config->name, __clk_get_name(hw->clk))) {
			break;
		}
	}

	if (pwhale_pll_config->name == NULL) {
		pr_err("clk: %s cannot not get pll %s\n", __func__, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	cfg2 = __raw_readl((u32 *)pll->m.mul.reg + 1);
	cfg1 = __raw_readl(pll->m.mul.reg);

	refin = __pll_get_refin_rate(pll->m.mul.reg, pwhale_pll_config->refin_msk);

	if (pwhale_pll_config->prediv_msk)
		if (!(cfg1 & pwhale_pll_config->prediv_msk))
			refin = refin * 2;

	if (!(cfg1 & pwhale_pll_config->div_s)) {
		n = cfg1 & pwhale_pll_config->pll_n_msk;
		rate = refin * n * 1000000;
	} else {
		nint = (cfg2 & pwhale_pll_config->nint_msk) >> __ffs(pwhale_pll_config->nint_msk);

		if (cfg1 & pwhale_pll_config->sdm_en)
			kint = cfg2 & pwhale_pll_config->kint_msk;

		rate = refin * (nint) * 1000000 + DIV_ROUND_CLOSEST(refin * kint * 100,
						1048576) * 10000;
	}

	return (unsigned long)rate;
}

static unsigned long sprd_clk_whale_adjustable_pll_recalc_rate(struct clk_hw *hw,
							unsigned long parent_rate) 
{
	return sprd_clk_whale_pll_recalc_rate(hw, parent_rate);
}

static unsigned long sprd_clk_whale_adjustable_mpll1_recalc_rate(struct clk_hw *hw,
							unsigned long parent_rate) 
{
	return sprd_clk_whale_pll_recalc_rate(hw, parent_rate);
}

static unsigned long sprd_clk_whale_adjustable_rpll_recalc_rate(struct clk_hw *hw,
							unsigned long parent_rate)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	unsigned int rate;
	unsigned int kint, nint, cfg1, cfg2, cfg3, n;

	cfg2 = __raw_readl((u32 *)pll->m.mul.reg + 1);
	cfg1 = __raw_readl(pll->m.mul.reg);
	if (!(cfg1 & BIT_RPLL_DIV_S)) {
		n = (cfg1 & BITS_RPLL_N(~0)) >> __ffs(BITS_RPLL_N(~0));
		rate = 26 * n * 1000000;
		printk(KERN_ERR "pll %s rate = %d\n", __clk_get_name(hw->clk), rate);
	} else {
		nint = (cfg1 & BITS_RPLL_NINT(~0)) >> __ffs(BITS_RPLL_NINT(~0));

		cfg3 = __raw_readl((u32 *)pll->m.mul.reg + 2);
		if (cfg3 & BIT_RPLL_SDM_EN)
			kint = cfg2 & BITS_RPLL_KINT(~0);

		rate = 26 * (nint) * 1000000 + DIV_ROUND_CLOSEST(26 * kint *100,
						1048576) * 10000;
		clk_debug("rate %u, kint %u, nint %u\n", rate, kint, nint);
	}

	return (unsigned long)rate;
}

static void __pllreg_write(void *reg, u32 val, u32 msk)
{
	__raw_writel((__raw_readl(reg) & ~msk) | val, reg);
}

static int __pll_enable_time(struct clk_hw *hw, unsigned long old_rate)
{
	/* FIXME: for mpll, each step (100MHz) takes 50us */
	u32 rate = sprd_clk_whale_adjustable_pll_recalc_rate(hw, 0) / 1000000;
	int dly = abs(rate - old_rate) * 50 / 100;
	WARN_ON(dly > 1000);
	udelay(dly);

	return 0;
}

static int sprd_clk_whale_pll_set_rate(struct clk_hw *hw,
				       unsigned long rate,
				       unsigned long parent_rate,
				       u8 ibias)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	u32 old_rate = sprd_clk_whale_adjustable_pll_recalc_rate(hw, 0) / 1000000;
	u32 kint, nint, cfg1 = 0, cfg2 = 0;
	unsigned int refin;
	struct whale_pll_config *pwhale_pll_config = whale_pll_config_table;

	for (; pwhale_pll_config->name != NULL; pwhale_pll_config++) {
		if (!strcmp(pwhale_pll_config->name, __clk_get_name(hw->clk))) {
			break;
		}
	}

	if (pwhale_pll_config->name == NULL) {
		pr_err("clk: %s cannot not get pll %s\n", __func__, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	refin = __pll_get_refin_rate(pll->m.mul.reg, pwhale_pll_config->refin_msk);

	if (pwhale_pll_config->prediv_msk)
		if (!(cfg1 & pwhale_pll_config->prediv_msk))
			refin = refin * 2;

	nint = (rate / 1000000) / refin;
	kint = DIV_ROUND_CLOSEST(((rate / 10000) - refin * nint * 100) * 1048576,
				 refin * 100);

	clk_debug("%s rate %u, kint %u, nint %u\n", __clk_get_name(hw->clk),
						(u32) rate, kint, nint);
	cfg2 = (nint << __ffs(pwhale_pll_config->nint_msk)) & pwhale_pll_config->nint_msk;
	if (kint) {
		cfg2 |= kint & pwhale_pll_config->kint_msk;
		cfg1 |= pwhale_pll_config->sdm_en;
	}

	cfg1 |= (ibias << __ffs(pwhale_pll_config->ibias_msk)) & pwhale_pll_config->ibias_msk;

	__pllreg_write((u32 *)pll->m.mul.reg + 1, cfg2, pwhale_pll_config->nint_msk | pwhale_pll_config->kint_msk);
	/* FIXME: pll clock set should not two-step */
	__pllreg_write(pll->m.mul.reg, cfg1, pwhale_pll_config->sdm_en);

	__pll_enable_time(hw, old_rate);

	return 0;
}

static u8 __whale_get_ibias_from_ibias_table(unsigned long rate, struct whale_ibias_table *table)
{
	for (; table->rate == WHALE_PLL_MAX_RATE; table++) {
		if (rate <= table->rate) {
			break;
		}
	}
	return table->ibias;
}

static int sprd_clk_whale_adjustable_pll_set_rate(struct clk_hw *hw,
						unsigned long rate,
						unsigned long parent_rate) 
{
	u8 ibias;

	ibias = __whale_get_ibias_from_ibias_table(rate, whale_adjustable_pll_table);
	return sprd_clk_whale_pll_set_rate(hw, rate, parent_rate, ibias);
}

static int sprd_clk_whale_adjustable_mpll1_set_rate(struct clk_hw *hw,
						unsigned long rate,
						unsigned long parent_rate) 
{
	u8 ibias;

	ibias = __whale_get_ibias_from_ibias_table(rate, whale_mpll1_table);
	return sprd_clk_whale_pll_set_rate(hw, rate, parent_rate, ibias);
}

static int sprd_clk_whale_adjustable_rpll_set_rate(struct clk_hw *hw,
				       unsigned long rate,
				       unsigned long parent_rate)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	unsigned long old_rate = sprd_clk_whale_adjustable_pll_recalc_rate(hw, 0) / 1000000;
	u32 kint, nint, cfg1 = 0, cfg2 = 0, cfg3 = 0;
	u8 ibias = 0;

	nint = (rate / 1000000) / 26;
	kint = DIV_ROUND_CLOSEST(((rate / 10000) - 26 * nint * 100) * 1048576,
				 26 * 100);

	clk_debug("%s rate %u, kint %u, nint %u\n", __clk_get_name(hw->clk),
				(u32) rate, kint, nint);

	cfg1 = BITS_RPLL_NINT(nint);
	if (kint) {
		cfg3 |= BIT_RPLL_SDM_EN;
		cfg2 |= BITS_RPLL_KINT(kint);
	}

	ibias = __whale_get_ibias_from_ibias_table(rate, whale_rpll_table);

	cfg1 |= BITS_RPLL_IBIAS(ibias);

	__pllreg_write((u32 *)pll->m.mul.reg + 1, cfg2, BITS_RPLL_KINT(~0));
	__pllreg_write(pll->m.mul.reg, cfg1, BITS_RPLL_IBIAS(~0) | BITS_RPLL_NINT(~0));
	__pllreg_write((u32 *)pll->m.mul.reg + 2, cfg3, BIT_RPLL_SDM_EN);

	__pll_enable_time(hw, old_rate);

	return 0;
}

/* FIXME:
 * Inherit from clk-mux.c
 */
static u8 sprd_clk_mux_get_parent(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (!c->m.mux_hw->clk)
		c->m.mux_hw->clk = c->hw.clk;
	clk_debug("%s\n", __clk_get_name(hw->clk));
	return clk_mux_ops.get_parent(c->m.mux_hw);
}

static int sprd_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (!c->m.mux_hw->clk)
		c->m.mux_hw->clk = c->hw.clk;
	clk_debug("%s %d\n", __clk_get_name(hw->clk), (u32) index);
	return clk_mux_ops.set_parent(c->m.mux_hw, index);
}

/* FIXME:
 * Inherit from clk-divider.c
 */

static unsigned long sprd_clk_divider_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (!c->d.div_hw->clk)
		c->d.div_hw->clk = c->hw.clk;
	clk_debug("%s %lu\n", __clk_get_name(hw->clk), parent_rate);
	return clk_divider_ops.recalc_rate(c->d.div_hw, parent_rate);
}

static long sprd_clk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *prate)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (!c->d.div_hw->clk)
		c->d.div_hw->clk = c->hw.clk;
	clk_debug("%s rate %lu %lu\n", __clk_get_name(hw->clk), rate,
		  (prate) ? *prate : 0);

	/* FIXME: see clk_divider_bestdiv()
	 * bestdiv = DIV_ROUND_UP(parent_rate, rate);
	 * so round rate would be lower than rate
	 */
	return rate;
	//return clk_divider_ops.round_rate(c->d.div_hw, rate, prate);
}

static int sprd_clk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long parent_rate)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (!c->d.div_hw->clk)
		c->d.div_hw->clk = c->hw.clk;
	clk_debug("%s %lu %lu\n", __clk_get_name(hw->clk), rate, parent_rate);
	return clk_divider_ops.set_rate(c->d.div_hw, rate, parent_rate);
}

const struct clk_ops sprd_clk_fixed_pll_ops = {
	.prepare = sprd_pll_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.is_prepared = sprd_clk_is_prepared,
	.recalc_rate = sprd_clk_fixed_pll_recalc_rate,
};

const struct clk_ops sprd_clk_whale_adjustable_pll_ops = {
	.prepare = sprd_pll_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.round_rate = sprd_clk_adjustable_pll_round_rate,
	.set_rate = sprd_clk_whale_adjustable_pll_set_rate,
	.recalc_rate = sprd_clk_whale_adjustable_pll_recalc_rate,
};

const struct clk_ops sprd_clk_whale_adjustable_mpll1_ops = {
	.prepare = sprd_pll_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.round_rate = sprd_clk_adjustable_pll_round_rate,
	.set_rate = sprd_clk_whale_adjustable_mpll1_set_rate,
	.recalc_rate = sprd_clk_whale_adjustable_mpll1_recalc_rate,
};

const struct clk_ops sprd_clk_whale_adjustable_rpll_ops = {
	.prepare = sprd_pll_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.round_rate = sprd_clk_adjustable_pll_round_rate,
	.set_rate = sprd_clk_whale_adjustable_rpll_set_rate,
	.recalc_rate = sprd_clk_whale_adjustable_rpll_recalc_rate,
};

const struct clk_ops sprd_clk_gate_ops = {
	.prepare = sprd_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.enable = sprd_clk_enable,
	.disable = sprd_clk_disable,
	.is_enabled = sprd_clk_is_enable,
};

const struct clk_ops sprd_clk_mux_ops = {
	.enable = sprd_clk_enable,
	.disable = sprd_clk_disable,
	.get_parent = sprd_clk_mux_get_parent,
	.set_parent = sprd_clk_mux_set_parent,
	.prepare = sprd_clk_prepare,
	.unprepare = sprd_clk_unprepare,
};

const struct clk_ops sprd_clk_divider_ops = {
	.enable = sprd_clk_enable,
	.disable = sprd_clk_disable,
	.recalc_rate = sprd_clk_divider_recalc_rate,
	.round_rate = sprd_clk_divider_round_rate,
	.set_rate = sprd_clk_divider_set_rate,
};

const struct clk_ops sprd_clk_composite_ops = {
	.enable = sprd_clk_enable,
	.disable = sprd_clk_disable,
	.get_parent = sprd_clk_mux_get_parent,
	.set_parent = sprd_clk_mux_set_parent,
	.recalc_rate = sprd_clk_divider_recalc_rate,
	.round_rate = sprd_clk_divider_round_rate,
	.set_rate = sprd_clk_divider_set_rate,
};

static void __init file_clk_data(struct clk *clk, const char *clk_name);
static void __init sprd_clk_register(struct device *dev,
				     struct device_node *node,
				     struct clk_sprd *c,
				     struct clk_init_data *init);

/**
 * of_sprd_fixed_clk_setup() - Setup function for simple sprd fixed rate clock
 */
static void __init of_sprd_fixed_clk_setup(struct device_node *node)
{
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	u32 rate;

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return;

	of_property_read_string(node, "clock-output-names", &clk_name);

	clk = clk_register_fixed_rate(NULL, clk_name, NULL, CLK_IS_ROOT, rate);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, 0);
		file_clk_data(clk, clk_name);
	}
	clk_debug("[%p]%s fixed-rate %d\n", clk, clk_name, rate);
}

/**
 * of_sprd_fixed_factor_clk_setup() - Setup function for simple sprd fixed factor clock
 */
void __init of_sprd_fixed_factor_clk_setup(struct device_node *node)
{
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	const char *parent_name;
	u32 div, mult;

	if (of_property_read_u32(node, "clock-div", &div)) {
		pr_err
		    ("%s Fixed factor clock <%s> must have a clock-div property\n",
		     __func__, node->name);
		return;
	}

	if (of_property_read_u32(node, "clock-mult", &mult)) {
		pr_err
		    ("%s Fixed factor clock <%s> must have a clokc-mult property\n",
		     __func__, node->name);
		return;
	}

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	clk = clk_register_fixed_factor(NULL, clk_name, parent_name, 0,
					mult, div);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, 0);
		file_clk_data(clk, clk_name);
	}
	clk_debug("[%p]%s parent %s mult %d div %d\n", clk, clk_name,
		  parent_name, mult, div);

	clk_debug("%s RATE %lu\n", __clk_get_name(clk), clk_get_rate(clk));
}

/**
 * of_sprd_fixed_pll_clk_setup() - Setup function for simple sprd fixed-pll clock
 */
static void __init of_sprd_fixed_pll_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	struct clk_init_data init = {
		.name = clk_name,
		.ops = &sprd_clk_fixed_pll_ops,
		.flags = CLK_IS_ROOT,
		.num_parents = 0,
	};
	u32 rate;
	const __be32 *prereg;

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return;

	prereg = of_get_address(node, 0, NULL, NULL);
	if (!prereg) {
		pr_err
		    ("%s Fixed pll clock <%s> must have a prepare reg property\n",
		     __func__, node->name);
		return;
	}

	of_property_read_string(node, "clock-output-names", &clk_name);

	/* allocate fixed-pll clock */
	c = kzalloc(sizeof(struct clk_sprd), GFP_KERNEL);
	if (!c) {
		pr_err("%s: could not allocate sprd fixed-pll clk\n", __func__);
		return;
	}

	/* struct clk_fixed_rate assignments */
	c->m.fixed_rate = rate;

	/* set fixed pll regs */
	of_read_reg(&c->d.pre, prereg);

	sprd_clk_register(NULL, node, c, &init);

	clk_debug("[%p]%s fixed-pll-rate %d, prepare %p[%x]\n", clk, clk_name,
		  rate, c->d.pre.reg, c->d.pre.msk);

	clk_debug("%s RATE %lu\n", __clk_get_name(c->hw.clk),
		  clk_get_rate(c->hw.clk));
}

static void __init of_sprd_pll_clk_setup(struct device_node *node, const struct clk_ops *clk_ops)
{
	struct clk_sprd *c;
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	struct clk_init_data init = {
		.name = clk_name,
		.ops = clk_ops,
		.flags = CLK_IS_ROOT | CLK_GET_RATE_NOCACHE,
		.num_parents = 0,
	};
	const __be32 *mulreg, *prereg;

	mulreg = of_get_address(node, 0, NULL, NULL);
	if (!mulreg) {
		pr_err
		    ("%s adjustable clock <%s> must have a mul reg property\n",
		     __func__, node->name);
		return;
	}

	prereg = of_get_address(node, 1, NULL, NULL);
	if (!prereg) {
		pr_err
		    ("%s adjustable pll clock <%s> should have a prepare reg property\n",
		     __func__, node->name);
		//return;
	}

	of_property_read_string(node, "clock-output-names", &clk_name);

	/* allocate adjustable-pll clock */
	c = kzalloc(sizeof(struct clk_sprd), GFP_KERNEL);
	if (!c) {
		pr_err("%s: could not allocate adjustable-pll clk\n", __func__);
		return;
	}

	/* struct clk_adjustable_pll assignments */
	of_read_reg(&c->m.mul, mulreg);
	if (prereg)
		of_read_reg(&c->d.pre, prereg);

	/* Flags:
	 * CLK_GATE_SET_TO_DISABLE - by default this clock sets the bit at bit_idx to
	 *  enable the clock.  Setting this flag does the opposite: setting the bit
	 *  disable the clock and clearing it enables the clock
	 */
	if ((unsigned long) c->d.pre.reg & 1) {
		*(u32 *) & c->d.pre.reg &= ~3;
		c->flags |= CLK_GATE_SET_TO_DISABLE;
	}
	sprd_clk_register(NULL, node, c, &init);

	if (prereg)
		clk_debug("[%p]%s mul %p[%x], prepare %p[%x]\n", clk, clk_name,
			  c->m.mul.reg, c->m.mul.msk, c->d.pre.reg,
			  c->d.pre.msk);
	else
		clk_debug("[%p]%s mul %p[%x]\n", clk, clk_name,
			  c->m.mul.reg, c->m.mul.msk);

	clk_debug("%s RATE %lu\n", __clk_get_name(c->hw.clk),
		  clk_get_rate(c->hw.clk));
}

static void __init of_sprd_whale_adjustable_pll_clk_setup(struct device_node *node)
{
	of_sprd_pll_clk_setup(node, &sprd_clk_whale_adjustable_pll_ops);
}

static void __init of_sprd_whale_adjustable_mpll1_clk_setup(struct device_node *node)
{
	of_sprd_pll_clk_setup(node, &sprd_clk_whale_adjustable_mpll1_ops);
}

static void __init of_sprd_whale_adjustable_rpll_clk_setup(struct device_node *node)
{
	of_sprd_pll_clk_setup(node, &sprd_clk_whale_adjustable_rpll_ops);
}

/**
 * of_sprd_gate_clk_setup() - Setup function for simple gate rate clock
 */
static void __init of_sprd_gate_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	const char *parent_name;
	struct clk_init_data init = {
		.name = clk_name,
		.ops = &sprd_clk_gate_ops,
		.flags = CLK_IGNORE_UNUSED,
	};
	const __be32 *enbreg, *prereg;

	enbreg = of_get_address(node, 0, NULL, NULL);
	if (!enbreg) {
		pr_err
		    ("%s gate clock <%s> must have a reg-enb property\n",
		     __func__, node->name);
		return;
	}

	prereg = of_get_address(node, 1, NULL, NULL);
	if (!prereg) {
		/*prepare reg is optional*/
	}

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	/* allocate the gate */
	c = kzalloc(sizeof(struct clk_sprd), GFP_KERNEL);
	if (!c) {
		pr_err("%s: could not allocate gated clk\n", __func__);
		return;
	}

	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_gate assignments */
	of_read_reg(&c->enb, enbreg);
	of_read_reg(&c->d.pre, prereg);
	/* Flags:
	 * CLK_GATE_SET_TO_DISABLE - by default this clock sets the bit at bit_idx to
	 *  enable the clock.  Setting this flag does the opposite: setting the bit
	 *  disable the clock and clearing it enables the clock
	 */
	if ((unsigned long) c->d.pre.reg & 1) {
		*(u32 *) & c->d.pre.reg &= ~3;
		c->flags |= CLK_GATE_SET_TO_DISABLE;
	}

	sprd_clk_register(NULL, node, c, &init);

	if (prereg)
		clk_debug("[%p]%s enable %p[%x] prepare %p[%x]\n", clk,
			  clk_name, c->enb.reg, c->enb.msk, c->d.pre.reg,
			  c->d.pre.msk);
	else
		clk_debug("[%p]%s enable %p[%x]\n", clk, clk_name,
			  c->enb.reg, c->enb.msk);
}

/**
 * of_sprd_composite_clk_setup() - Setup function for simple composite clock
 */
static struct clk_sprd *__init __of_sprd_composite_clk_setup(struct device_node
							     *node, int has_mux,
							     int has_div)
{
	struct clk_sprd *c;
	const char *clk_name = node->name;
	const char *parent_name;
	struct clk_init_data init = {
		.name = clk_name,
		.flags = CLK_IGNORE_UNUSED,
	};
	const __be32 *selreg = NULL, *divreg = NULL, *enbreg, *prereg = NULL;
	int idx = 0;

	if (has_mux)
		selreg = of_get_address(node, idx++, NULL, NULL);

	if (has_div)
		divreg = of_get_address(node, idx++, NULL, NULL);

	enbreg = of_get_address(node, idx++, NULL, NULL);
	if (enbreg) {
		prereg = of_get_address(node, idx++, NULL, NULL);
	}

	of_property_read_string(node, "clock-output-names", &clk_name);

	/* allocate the clock */
	c = kzalloc(sizeof(struct clk_sprd), GFP_KERNEL);
	if (!c) {
		pr_err("%s: could not allocate sprd clk\n", __func__);
		return NULL;
	}

	/* struct clk_sprd assignments */
	of_read_reg(&c->enb, enbreg);

	if (selreg) {
		int i, num_parents;
		struct clk_mux *mux;

		of_read_reg(&c->m.sel, selreg);
		of_read_reg(&c->d.pre, prereg);

		/* Flags:
		* CLK_GATE_SET_TO_DISABLE - by default this clock sets the bit at bit_idx to
		*  enable the clock.  Setting this flag does the opposite: setting the bit
		*  disable the clock and clearing it enables the clock
		*/
	#if defined(CONFIG_ARCH_SCX35LT8)
		if ((unsigned long) c->d.pre.reg & 1) {
			*(u32 *) & c->d.pre.reg &= ~3;
			c->flags |= CLK_GATE_SET_TO_DISABLE;
		}
	#endif
		init.ops = &sprd_clk_mux_ops;

		/* FIXME: Retrieve the phandle list property */
		of_get_property(node, "clocks", &num_parents);
		init.num_parents = (u8) num_parents / 4;
		mux =
		    kzalloc(sizeof(struct clk_mux) +
			    sizeof(const char *) * init.num_parents,
			    GFP_KERNEL);
		init.parent_names = (const char **)&mux[1];
		clk_debug("parents : ");
		for (i = 0; i < init.num_parents; i++) {
			init.parent_names[i] = of_clk_get_parent_name(node, i);
			pr_debug("[%d]%s ", i, init.parent_names[i]);
		}
		pr_debug("\n");

		/* struct clk_mux assignments */
		mux->reg = c->m.sel.reg;
		mux->shift = __ffs(c->m.sel.msk);
		mux->mask = c->m.sel.msk >> mux->shift;
		mux->flags = 0;
		mux->lock = 0;
		mux->table = 0;
		c->m.mux_hw = &mux->hw;	/* FIXME: should not use m.sel.reg at now */
		clk_debug("mux %u, %x\n", (u32) mux->shift, mux->mask);
	}

	if (divreg) {
		struct clk_divider *div;

		of_read_reg(&c->d.div, divreg);

		div = kzalloc(sizeof(struct clk_divider), GFP_KERNEL);
		init.ops = &sprd_clk_divider_ops;
		if (!init.num_parents) {
			parent_name = of_clk_get_parent_name(node, 0);;
			init.parent_names = (parent_name ? &parent_name : NULL);
			init.num_parents = (parent_name ? 1 : 0);
			clk_debug("parent %s\n", parent_name);
		}

		/* struct clk_divider assignments */
		div->reg = c->d.div.reg;
		div->shift = __ffs(c->d.div.msk);
		div->width = __ffs(~(c->d.div.msk >> div->shift));
		div->flags = 0;
		div->lock = 0;
		div->table = 0;
		c->d.div_hw = &div->hw;	/* FIXME: should not use d.div.reg at now */
		clk_debug("div %u, %u\n", (u32) div->shift, (u32) div->width);
	}

	if (divreg && selreg) {
		init.ops = &sprd_clk_composite_ops;
	}

	sprd_clk_register(NULL, node, c, &init);
	return c;

}

#define to_clk_mux(_hw) container_of(_hw, struct clk_mux, hw)

static void __init of_sprd_muxed_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	c = __of_sprd_composite_clk_setup(node, 1, 0);
	if (!c)
		return;

	{
		struct clk_mux *mux = to_clk_mux(c->m.mux_hw);
		if (!mux)
			return;
		clk_debug("[%p]%s select %p[%x] enable %p[%x] prepare %p[%x]\n",
			  c->hw.clk, __clk_get_name(c->hw.clk), mux->reg,
			  c->m.sel.msk, c->enb.reg, c->enb.msk, c->d.pre.reg,
			  c->d.pre.msk);
		clk_debug("%s RATE %lu\n", __clk_get_name(c->hw.clk),
			  clk_get_rate(c->hw.clk));
	}
}

#define to_clk_divider(_hw) container_of(_hw, struct clk_divider, hw)

static void __init of_sprd_divider_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	c = __of_sprd_composite_clk_setup(node, 0, 1);
	if (!c)
		return;
	{
		struct clk_divider *div = to_clk_divider(c->d.div_hw);
		if (!div)
			return;
		clk_debug("[%p]%s divider %p[%x] enable %p[%x]\n", c->hw.clk,
			  __clk_get_name(c->hw.clk), div->reg,
			  c->d.div.msk, c->enb.reg, c->enb.msk);
		clk_debug("%s RATE %lu\n", __clk_get_name(c->hw.clk),
			  clk_get_rate(c->hw.clk));
	}
}

static void __init of_sprd_composite_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	c = __of_sprd_composite_clk_setup(node, 1, 1);
	if (!c)
		return;
	{
		struct clk_mux *mux = to_clk_mux(c->m.mux_hw);
		struct clk_divider *div = to_clk_divider(c->d.div_hw);
		if (!mux || !div)
			return;
		clk_debug("[%p]%s select %p[%x] divider %p[%x] enable %p[%x]\n",
			  c->hw.clk, __clk_get_name(c->hw.clk), mux->reg,
			  c->m.sel.msk, div->reg, c->d.div.msk, c->enb.reg,
			  c->enb.msk);
		clk_debug("%s RATE %lu\n", __clk_get_name(c->hw.clk),
			  clk_get_rate(c->hw.clk));
	}
}

/* register the clock */
static struct clk_onecell_data clk_data;
static void __init init_clk_data(struct device_node *node)
{
	struct clk **clks;
	int num = of_get_child_count(node);
	clks = kzalloc(sizeof(struct clk *) * num, GFP_KERNEL);
	if (!clks)
		return;

	clk_data.clks = clks;
	clk_data.clk_num = num;
}

static void __init file_clk_data(struct clk *clk, const char *clk_name)
{
	static int clk_idx = 0;
	/* FIXME: Add oncell clock provider, be careful not to mistake the clock index */
	if (clk_data.clks) {
		clk = clk_get_sys(NULL, clk_name);
		if (!IS_ERR(clk)) {
			clk_info("[%d]%s\n", clk_idx, clk_name);
			clk_data.clks[clk_idx++] = clk;
		}
	}
}

static void __init sprd_clk_register(struct device *dev,
				     struct device_node *node,
				     struct clk_sprd *c,
				     struct clk_init_data *init)
{
	struct clk *clk;
	const char *clk_name = init->name;

	c->hw.init = init;
	clk = clk_register(dev, &c->hw);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, 0);
		file_clk_data(clk, clk_name);
	}
}

static void __init sprd_clocks_init(struct device_node *node)
{
	init_clk_data(node);
	of_clk_add_provider(node, of_clk_src_onecell_get, &clk_data);
}

/**
 * clk_force_disable - force disable clock output
 * @clk: clock source
 *
 * Forcibly disable the clock output.
 * NOTE: this *will* disable the clock output even if other consumer
 * devices have it enabled. This should be used for situations when device
 * suspend or damage will likely occur if the devices is not disabled.
 */
void clk_force_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_debug("clk %p, usage %d\n", clk, __clk_get_enable_count(clk));
	while (__clk_get_enable_count(clk) > 0)
		clk_disable(clk);
}

EXPORT_SYMBOL_GPL(clk_force_disable);

CLK_OF_DECLARE(scx35l_clock, "sprd,whale-clocks", sprd_clocks_init);
CLK_OF_DECLARE(fixed_clock, "sprd,fixed-clock", of_sprd_fixed_clk_setup);
CLK_OF_DECLARE(fixed_factor_clock, "sprd,fixed-factor-clock",
	       of_sprd_fixed_factor_clk_setup);
CLK_OF_DECLARE(fixed_pll_clock, "sprd,fixed-pll-clock",
	       of_sprd_fixed_pll_clk_setup);
CLK_OF_DECLARE(whale_adjustable_pll_clock, "sprd,whale-adjustable-pll-clock",
		of_sprd_whale_adjustable_pll_clk_setup);
CLK_OF_DECLARE(whale_adjustable_mpll1_clock, "sprd,whale-adjustable-mpll1-clock",
		of_sprd_whale_adjustable_mpll1_clk_setup);
CLK_OF_DECLARE(whale_adjustable_rpll_clock, "sprd,whale-adjustable-rpll-clock",
		of_sprd_whale_adjustable_rpll_clk_setup);
CLK_OF_DECLARE(gate_clock, "sprd,gate-clock", of_sprd_gate_clk_setup);
CLK_OF_DECLARE(muxed_clock, "sprd,muxed-clock", of_sprd_muxed_clk_setup);
CLK_OF_DECLARE(divider_clock, "sprd,divider-clock", of_sprd_divider_clk_setup);
CLK_OF_DECLARE(composite_clock, "sprd,composite-dev-clock",
	       of_sprd_composite_clk_setup);
#endif
