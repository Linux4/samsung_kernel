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
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_lock.h>

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
#define to_range(b, first, base) ( (b) - (first) + (base) )

static inline u32 cfg_reg_p2v(const u32 regp)
{
	u32 regv = 0;

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

	return regv;
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

	clk_debug("%s %s %p[%x]\n", __clk_get_name(hw->clk),
		  (is_set) ? "SET" : "CLR", reg, (u32) msk);

	__arch_default_lock(HWLOCK_GLB, &flags);

	if (is_set)
		__raw_writel(msk, (void *)((u32) (reg) + 0x1000));
	else
		__raw_writel(msk, (void *)((u32) (reg) + 0x2000));

	__arch_default_unlock(HWLOCK_GLB, &flags);
}

#define __glbreg_set(hw, reg, msk)	__glbreg_setclr(hw, reg, msk, 1)
#define __glbreg_clr(hw, reg, msk)	__glbreg_setclr(hw, reg, msk, 0)

static int sprd_clk_prepare(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int set = ! !(c->flags & CLK_GATE_SET_TO_DISABLE);

	__glbreg_setclr(hw, c->d.pre.reg, (u32) c->d.pre.msk, set ^ 1);
	return 0;
}

static void sprd_clk_unprepare(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int set = ! !(c->flags & CLK_GATE_SET_TO_DISABLE);
	__glbreg_setclr(hw, c->d.pre.reg, (u32) c->d.pre.msk, set ^ 0);
}

static int sprd_clk_is_prepared(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int ret, set = ! !(c->flags & CLK_GATE_SET_TO_DISABLE);

	if (!c->d.pre.reg)
		return 0;

	/* if a set bit prepare this gate, flip it before masking */
	ret = ! !(__raw_readl(c->d.pre.reg) & BIT(c->d.pre.msk));
	return set ^ ret;
}

static int sprd_clk_enable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	__glbreg_set(hw, c->enb.reg, (u32) c->enb.msk);
	return 0;
}

static void sprd_clk_disable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	__glbreg_clr(hw, c->enb.reg, (u32) c->enb.msk);
}

static int sprd_clk_is_enable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	int ret = ! !(__raw_readl(c->enb.reg) & BIT(c->enb.msk));
	return ret;
}

static unsigned long sprd_clk_fixed_pll_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	return to_clk_sprd(hw)->m.fixed_rate;
}

#define BITS_MPLL_REFIN(_X_)                              ( (_X_) << 24 & (BIT(24)|BIT(25)) )

/* bits definitions for register REG PLL CFG1 */
#define BITS_PLL_KINT(_X_)                               ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_PLL_DIV_S                                    ( BIT(10) )
#define BITS_PLL_RSV(_X_)                                ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_PLL_MOD_EN                                   ( BIT(7) )
#define BIT_PLL_SDM_EN                                   ( BIT(6) )
#define BITS_PLL_NINT(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

#define SHFT_PLL_KINT                                     ( 12 )
#define SHFT_PLL_NINT                                     ( 0 )

/*
 How To look PLL Setting
 reg name	 val	 bit
 MPLL_CFG	 Fin	 [25:24]
 MPLL_CFG	 N	 [5:0]
 MPLL_CFG1	 div_s	 [10]
 MPLL_CFG1	 sdm_en  [6]
 MPLL_CFG1	 Nint	 [5:0]
 MPLL_CFG1	 Kint	 [31:12]
 div_s = 1	 sdm_en = 1  Fout = Fin * ( Nint + Kint/1048576)
 div_s = 1	 sdm_en = 0  Fout = Fin * Nint
 div_s = 0	 sdm_en = x  Fout = Fin * N
 */

static inline unsigned int __pll_get_refin_rate(void *reg)
{
	const unsigned long refin[4] = { 2000000, 4000000, 13000000, 26000000 };
	u32 i, msk = BITS_MPLL_REFIN(-1);
	i = (__raw_readl(reg) & msk) >> __ffs(msk);
	return refin[i];
}

static unsigned long sprd_clk_adjustable_pll_recalc_rate(struct clk_hw *hw,
							 unsigned long
							 parent_rate)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	unsigned int rate;

#ifdef CONFIG_ARCH_SCX30G
	unsigned int k = 0, mn, cfg1;
	cfg1 = __raw_readl(pll->m.mul.reg);
	mn = (cfg1 & BITS_PLL_NINT(~0)) >> SHFT_PLL_NINT;

	/* FIXME: Kint only valid while sdm_en = 1 */
	if ((cfg1 & BIT_PLL_SDM_EN))
		k = (cfg1 & BITS_PLL_KINT(~0)) >> SHFT_PLL_KINT;

	rate =
	    26 * (mn) * 1000000 + DIV_ROUND_CLOSEST(26 * k * 100,
						    1048576) * 10000;
	clk_debug("rate %u, k %u, mn %u\n", rate, k, mn);
#else
	unsigned int refin, mn;
	refin = __pll_get_refin_rate(pll->m.mul.reg);
	mn = (__raw_readl(pll->m.mul.reg)
	      & pll->m.mul.msk) >> __ffs(pll->m.mul.msk);

	rate = refin * mn;
	clk_debug("rate %u, refin %u, mn %u\n", rate, refin, mn);
#endif
	return (unsigned long)rate;
}

static long sprd_clk_adjustable_pll_round_rate(struct clk_hw *hw,
					       unsigned long rate,
					       unsigned long *prate)
{
	//struct clk_sprd *pll = to_clk_sprd(hw);
	clk_debug("rate %lu, %lu\n", rate, *prate);
	return rate;
}

static void __pllreg_write(void *reg, u32 val, u32 msk)
{
	__raw_writel((__raw_readl(reg) & ~msk) | val, reg);
}

static int __pll_enable_time(struct clk_hw *hw, unsigned long old_rate)
{
	/* FIXME: for mpll, each step (100MHz) takes 50us */
	u32 rate = sprd_clk_adjustable_pll_recalc_rate(hw, 0) / 1000000;
	int dly = abs(rate - old_rate) * 50 / 100;
	WARN_ON(dly > 1000);
	udelay(dly);
	return 0;
}

static int sprd_clk_adjustable_pll_set_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long parent_rate)
{
	struct clk_sprd *pll = to_clk_sprd(hw);
	u32 old_rate = sprd_clk_adjustable_pll_recalc_rate(hw, 0) / 1000000;
#ifdef CONFIG_ARCH_SCX30G
	u32 k, mn, cfg1;

	mn = (rate / 1000000) / 26;
	k = DIV_ROUND_CLOSEST(((rate / 10000) - 26 * mn * 100) * 1048576,
			      26 * 100);

	cfg1 = BITS_PLL_NINT(mn);
	if (k)
		cfg1 |= BITS_PLL_KINT(k) | BIT_PLL_SDM_EN;

	clk_debug("%s rate %u, k %u, mn %u\n", __clk_get_name(hw->clk),
		  (u32) rate, k, mn);
	__pllreg_write(pll->m.mul.reg, cfg1,
		       BITS_PLL_KINT(~0) | BITS_PLL_NINT(~0) | BIT_PLL_SDM_EN);
#else
	u32 refin, mn;
	refin = __pll_get_refin_rate(pll->m.mul.reg);
	mn = rate / refin;
	clk_debug("rate %u, refin %u, mn %u\n", (u32) rate, refin, mn);
	if (mn <= pll->m.mul.msk >> __ffs(pll->m.mul.msk)) {
		__pllreg_write(pll->m.mul.reg, mn << __ffs(pll->m.mul.msk),
			       pll->m.mul.msk);
	}
#endif
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
	.prepare = sprd_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.is_prepared = sprd_clk_is_prepared,
	.recalc_rate = sprd_clk_fixed_pll_recalc_rate,
};

const struct clk_ops sprd_clk_adjustable_pll_ops = {
	.prepare = sprd_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.round_rate = sprd_clk_adjustable_pll_round_rate,
	.set_rate = sprd_clk_adjustable_pll_set_rate,
	.recalc_rate = sprd_clk_adjustable_pll_recalc_rate,
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

static inline void __mmreg_setclr(struct clk_hw *hw, void *reg, u32 msk,
				  int is_set)
{
	if (!reg)
		return;

	clk_debug("%s %s %p[%x]\n", __clk_get_name(hw->clk),
		  (is_set) ? "SET" : "CLR", reg, (u32) msk);

	if (is_set)
		__raw_writel(__raw_readl((void *)reg) | msk, (void *)reg);
	else
		__raw_writel(__raw_readl((void *)reg) & ~msk, (void *)reg);
}

#define __mmreg_set(hw, reg, msk)	__mmreg_setclr(hw, reg, msk, 1)
#define __mmreg_clr(hw, reg, msk)	__mmreg_setclr(hw, reg, msk, 0)
#define __SPRD_MM_TIMEOUT		(3 * 1000)
static int sprd_mm_domain_state(struct clk_hw *hw)
{
	/* FIXME: rtc domain */
	u32 power_state1, power_state2, power_state3;
	unsigned long timeout = jiffies + msecs_to_jiffies(__SPRD_MM_TIMEOUT);

	do {
		cpu_relax();
		power_state1 =
		    __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG) &
		    BITS_PD_MM_TOP_STATE(-1);
		power_state2 =
		    __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG) &
		    BITS_PD_MM_TOP_STATE(-1);
		power_state3 =
		    __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG) &
		    BITS_PD_MM_TOP_STATE(-1);
		if (time_after(jiffies, timeout)) {
			pr_emerg("[%d]mm domain not ready, state %08x %08x\n",
				 __LINE__, __raw_readl((void *)
						       REG_PMU_APB_PWR_STATUS0_DBG), __raw_readl((void *)REG_MM_AHB_AHB_EB));
			pr_emerg("mm prepare count %u, enable count %u\n",
				 __clk_get_prepare_count(hw->clk),
				 __clk_get_enable_count(hw->clk));
		}
	} while (power_state1 != power_state2 || power_state2 != power_state3);

	return (power_state1);
}

static int sprd_mm_domain_is_ready(struct clk_hw *hw)
{
#ifdef CONFIG_ARCH_SCX35
	return sprd_mm_domain_state(hw) == BITS_PD_MM_TOP_STATE(0);
#else
	return 1;
#endif
}

static int sprd_mm_domain_is_shutdown(struct clk_hw *hw)
{
#ifdef CONFIG_ARCH_SCX35
	return sprd_mm_domain_state(hw) == BITS_PD_MM_TOP_STATE(7);
#else
	return 0;
#endif
}

/* FIXME: mm domain soft-retention */
static u32 saved_mm_ckg[10];
static void sprd_mm_domain_save(struct clk_hw *hw)
{
#ifdef CONFIG_ARCH_SCX35
	u32 *ckg = (u32 *) REG_MM_CLK_MM_AHB_CFG;
	int i;
	BUG_ON(!(sprd_mm_domain_is_ready(hw)));
	BUG_ON(!(__raw_readl((void *)REG_AON_APB_APB_EB0) & BIT_MM_EB));
	for (i = 0; i < ARRAY_SIZE(saved_mm_ckg); i++, ckg++) {
		saved_mm_ckg[i] = __raw_readl(ckg);
	}
	clk_debug("ahb %08x sensor %08x vsp %08x\n", saved_mm_ckg[0],
		  saved_mm_ckg[1], saved_mm_ckg[4]);
#endif
}

static void sprd_mm_domain_restore(struct clk_hw *hw)
{
#ifdef CONFIG_ARCH_SCX35
	u32 *ckg = (u32 *) REG_MM_CLK_MM_AHB_CFG;
	int i;
	clk_debug("ahb %08x sensor %08x vsp %08x\n", saved_mm_ckg[0],
		  saved_mm_ckg[1], saved_mm_ckg[4]);
	BUG_ON(!(sprd_mm_domain_is_ready(hw)));
	BUG_ON(!(__raw_readl((void *)REG_AON_APB_APB_EB0) & BIT_MM_EB));
	for (i = 0; i < ARRAY_SIZE(saved_mm_ckg); i++, ckg++) {
		__raw_writel(saved_mm_ckg[i], ckg);
	}
#endif
}

static int __sprd_clk_mm_prepare(struct clk_hw *hw)
{
#ifdef CONFIG_ARCH_SCX35
	if (!sprd_mm_domain_is_ready(hw)) {
		unsigned long timeout =
		    jiffies + msecs_to_jiffies(__SPRD_MM_TIMEOUT);

		__glbreg_clr(hw, (void *)REG_PMU_APB_PD_MM_TOP_CFG,
			     BIT_PD_MM_TOP_FORCE_SHUTDOWN);
		/* FIXME: wait a moment for mm domain stable
		 */
		while (!sprd_mm_domain_is_ready(hw)
		       && !time_after(jiffies, timeout)) {
			udelay(50);
			cpu_relax();
		}
		WARN(!sprd_mm_domain_is_ready(hw),
		     "MM TOP CFG 0x%08x, STATE 0x%08x\n",
		     __raw_readl((void *)REG_PMU_APB_PD_MM_TOP_CFG),
		     __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG));
		__glbreg_set(hw, (void *)REG_AON_APB_APB_EB0, BIT_MM_EB);
		__glbreg_set(hw, (void *)REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
		__mmreg_set(hw, (void *)REG_MM_AHB_GEN_CKG_CFG,
			    BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN);
		__mmreg_set(hw, (void *)REG_MM_CLK_MM_AHB_CFG, 0x3);	/* set mm ahb 153.6MHz */
		sprd_mm_domain_restore(hw);
	}

	if (!(__raw_readl((void *)REG_AON_APB_APB_EB0) & BIT_MM_EB)) {
		__glbreg_set(hw, (void *)REG_AON_APB_APB_EB0, BIT_MM_EB);
		if (!(__raw_readl((void *)REG_MM_AHB_AHB_EB) & BIT_MM_CKG_EB)) {
			__glbreg_set(hw, (void *)REG_MM_AHB_AHB_EB,
				     BIT_MM_CKG_EB);
			__mmreg_set(hw, (void *)REG_MM_AHB_GEN_CKG_CFG,
				    BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN);
			__mmreg_set(hw, (void *)REG_MM_CLK_MM_AHB_CFG, 0x3);	/* set mm ahb 153.6MHz */
		}
	}
#endif
	return 0;
}

static void __sprd_clk_mm_unprepare(struct clk_hw *hw)
{
	if (sprd_mm_domain_is_ready(hw)) {
		unsigned long timeout =
		    jiffies + msecs_to_jiffies(__SPRD_MM_TIMEOUT);

		if (!(__raw_readl((void *)REG_AON_APB_APB_EB0) & BIT_MM_EB))
			__glbreg_set(hw, (void *)REG_AON_APB_APB_EB0, BIT_MM_EB);
		if (!(__raw_readl((void *)REG_MM_AHB_AHB_EB) & BIT_MM_CKG_EB))
			__glbreg_set(hw, (void *)REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
		/* FIXME: save all mm ckg regs before disable mm */
		sprd_mm_domain_save(hw);
		__glbreg_clr(hw, (void *)REG_AON_APB_APB_EB0, BIT_MM_EB);

		__glbreg_set(hw, (void *)REG_PMU_APB_PD_MM_TOP_CFG,
			     BIT_PD_MM_TOP_FORCE_SHUTDOWN);
		/* FIXME: wait a moment for mm domain stable
		 */
		while (!sprd_mm_domain_is_shutdown(hw)
		       && !time_after(jiffies, timeout)) {
			udelay(50);
			cpu_relax();
		}
		WARN(!sprd_mm_domain_is_shutdown(hw),
		     "MM TOP CFG 0x%08x, STATE 0x%08x\n",
		     __raw_readl((void *)REG_PMU_APB_PD_MM_TOP_CFG),
		     __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG));
	} else {
		__glbreg_clr(hw, (void *)REG_AON_APB_APB_EB0, BIT_MM_EB);
	}
}

static int __sprd_clk_mm_enable(struct clk_hw *hw)
{
	if (!sprd_mm_domain_is_ready(hw)) {
		pr_emerg("[%d]mm domain not ready, state %08x %08x\n", __LINE__,
			 __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG),
			 __raw_readl((void *)REG_MM_AHB_AHB_EB));
		pr_emerg("mm prepare count %u, enable count %u\n",
			 __clk_get_prepare_count(hw->clk),
			 __clk_get_enable_count(hw->clk));
	}
	return 0;
}

static void __sprd_clk_mm_disable(struct clk_hw *hw)
{
	if (!sprd_mm_domain_is_ready(hw)) {
		pr_emerg("[%d]mm domain not ready, state %08x %08x\n", __LINE__,
			 __raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG),
			 __raw_readl((void *)REG_MM_AHB_AHB_EB));
		pr_emerg("mm prepare count %u, enable count %u\n",
			 __clk_get_prepare_count(hw->clk),
			 __clk_get_enable_count(hw->clk));
	}
}

static int sprd_mm_clk_prepare(struct clk_hw *hw)
{
	__sprd_clk_mm_prepare(hw);
	return sprd_clk_prepare(hw);
}

static int sprd_mm_clk_enable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (sprd_mm_domain_is_ready(hw)) {
		__mmreg_set(hw, c->enb.reg, (u32) c->enb.msk);
	}
	return 0;
}

static void sprd_mm_clk_disable(struct clk_hw *hw)
{
	struct clk_sprd *c = to_clk_sprd(hw);
	if (sprd_mm_domain_is_ready(hw)) {
		__mmreg_clr(hw, c->enb.reg, (u32) c->enb.msk);
	}
}

static int sprd_mm_clk_is_enable(struct clk_hw *hw)
{
	if (sprd_mm_domain_is_ready(hw)) {
		return sprd_clk_is_enable(hw);
	}
	return 0;
}

static u8 sprd_mm_clk_mux_get_parent(struct clk_hw *hw)
{
	if (sprd_mm_domain_is_ready(hw)) {
		return sprd_clk_mux_get_parent(hw);
	}
	return 0;
}

static int sprd_mm_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	if (sprd_mm_domain_is_ready(hw)) {
		return sprd_clk_mux_set_parent(hw, index);
	}
	return 0;
}

static unsigned long sprd_mm_clk_divider_recalc_rate(struct clk_hw *hw,
						     unsigned long parent_rate)
{
	if (sprd_mm_domain_is_ready(hw)) {
		return sprd_clk_divider_recalc_rate(hw, parent_rate);
	}
	return parent_rate;
}

static int sprd_mm_clk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	if (sprd_mm_domain_is_ready(hw)) {
		return sprd_clk_divider_set_rate(hw, rate, parent_rate);
	}
	return 0;
}

const struct clk_ops sprd_clk_mm_gate_ops = {
	.prepare = __sprd_clk_mm_prepare,
	.unprepare = __sprd_clk_mm_unprepare,
	.enable = __sprd_clk_mm_enable,
	.disable = __sprd_clk_mm_disable,
	.is_enabled = sprd_clk_is_enable,
};

const struct clk_ops sprd_mm_clk_gate_ops = {
	.prepare = sprd_mm_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.enable = sprd_mm_clk_enable,
	.disable = sprd_mm_clk_disable,
	.is_enabled = sprd_mm_clk_is_enable,
};

const struct clk_ops sprd_mm_clk_mux_ops = {
	.prepare = sprd_mm_clk_prepare,
	.unprepare = sprd_clk_unprepare,
	.enable = sprd_mm_clk_enable,
	.disable = sprd_mm_clk_disable,
	.get_parent = sprd_mm_clk_mux_get_parent,
	.set_parent = sprd_mm_clk_mux_set_parent,
};

const struct clk_ops sprd_mm_clk_composite_ops = {
	.enable = sprd_mm_clk_enable,
	.disable = sprd_mm_clk_disable,
	.get_parent = sprd_mm_clk_mux_get_parent,
	.set_parent = sprd_mm_clk_mux_set_parent,
	.recalc_rate = sprd_mm_clk_divider_recalc_rate,
	.round_rate = sprd_clk_divider_round_rate,
	.set_rate = sprd_mm_clk_divider_set_rate,
};

static void __init file_clk_data(struct clk *clk, const char *clk_name);
static void __init sprd_clk_register(struct device *dev,
				     struct device_node *node,
				     struct clk_sprd *c,
				     struct clk_init_data *init);

/**
 * of_sprd_fixed_clk_setup() - Setup function for simple sprd fixed rate clock
 */
void __init of_sprd_fixed_clk_setup(struct device_node *node)
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

/**
 * of_sprd_adjustable_pll_clk_setup() - Setup function for simple sprd adjustable-pll clock
 */
static void __init of_sprd_adjustable_pll_clk_setup(struct device_node *node)
{
	struct clk_sprd *c;
	struct clk *clk = NULL;
	const char *clk_name = node->name;
	struct clk_init_data init = {
		.name = clk_name,
		.ops = &sprd_clk_adjustable_pll_ops,
		.flags = CLK_IS_ROOT,
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
		    ("%s adjustable pll clock <%s> must have a prepare reg property\n",
		     __func__, node->name);
		return;
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
	of_read_reg(&c->d.pre, prereg);

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
		//prepare reg is optional
	}

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	/* allocate the gate */
	c = kzalloc(sizeof(struct clk_sprd), GFP_KERNEL);
	if (!c) {
		pr_err("%s: could not allocate gated clk\n", __func__);
		return;
	}

	if (of_get_property(node, "mm-domain", NULL)) {
		init.ops = &sprd_mm_clk_gate_ops;
	} else if (0 == strcmp(clk_name, "clk_mm")) {
#ifdef CONFIG_ARCH_SCX35
		init.ops = &sprd_clk_mm_gate_ops;
		__sprd_clk_mm_prepare(&c->hw);
#endif
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
	if ((u32) c->d.pre.reg & 1) {
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

		init.ops = &sprd_clk_mux_ops;
		if (of_get_property(node, "mm-domain", NULL)) {
			init.ops = &sprd_mm_clk_mux_ops;
		}

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
		const char *parent_name;
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
		if (of_get_property(node, "mm-domain", NULL)) {
			init.ops = &sprd_mm_clk_composite_ops;
		}
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
	while (__clk_get_enable_count(clk) > 0) {
		clk_disable(clk);
	}
}

EXPORT_SYMBOL_GPL(clk_force_disable);

CLK_OF_DECLARE(scx15_clock, "sprd,scx15-clocks", sprd_clocks_init);
CLK_OF_DECLARE(scx35_clock, "sprd,scx35-clocks", sprd_clocks_init);
CLK_OF_DECLARE(scx30g_clock, "sprd,scx30g-clocks", sprd_clocks_init);
CLK_OF_DECLARE(scx35l_clock, "sprd,scx35l-clocks", sprd_clocks_init);
CLK_OF_DECLARE(fixed_clock, "sprd,fixed-clock", of_sprd_fixed_clk_setup);
CLK_OF_DECLARE(fixed_factor_clock, "sprd,fixed-factor-clock",
	       of_sprd_fixed_factor_clk_setup);
CLK_OF_DECLARE(fixed_pll_clock, "sprd,fixed-pll-clock",
	       of_sprd_fixed_pll_clk_setup);
CLK_OF_DECLARE(adjustable_pll_clock, "sprd,adjustable-pll-clock",
	       of_sprd_adjustable_pll_clk_setup);
CLK_OF_DECLARE(gate_clock, "sprd,gate-clock", of_sprd_gate_clk_setup);
CLK_OF_DECLARE(muxed_clock, "sprd,muxed-clock", of_sprd_muxed_clk_setup);
CLK_OF_DECLARE(divider_clock, "sprd,divider-clock", of_sprd_divider_clk_setup);
CLK_OF_DECLARE(composite_clock, "sprd,composite-dev-clock",
	       of_sprd_composite_clk_setup);
#endif
