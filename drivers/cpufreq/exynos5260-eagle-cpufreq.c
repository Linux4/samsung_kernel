/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5260 - EAGLE Core frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/clk-private.h>

#include <plat/clock.h>
#include <mach/map.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#define CPUFREQ_LEVEL_END_CA15	(L15 + 1)
#define L2_LOCAL_PWR_EN		0x7

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA15;
static int min_support_idx_CA15 = (CPUFREQ_LEVEL_END_CA15 - 1);

static struct clk *mout_egl_b;
static struct clk *mout_egl_a;
static struct clk *mout_egl_pll;
static struct clk *sclk_bus_pll;
static struct clk *fout_egl_pll;

static int egl_ema_con1;
static int egl_ema_con2;

static unsigned int exynos5260_volt_table_CA15[CPUFREQ_LEVEL_END_CA15];
static unsigned int exynos5260_abb_table_CA15[CPUFREQ_LEVEL_END_CA15];

static struct cpufreq_frequency_table exynos5260_freq_table_CA15[] = {
	{L0,  1700 * 1000},
	{L1,  1600 * 1000},
	{L2,  1500 * 1000},
	{L3,  1400 * 1000},
	{L4,  1300 * 1000},
	{L5,  1200 * 1000},
	{L6,  1100 * 1000},
	{L7,  1000 * 1000},
	{L8,   900 * 1000},
	{L9,   800 * 1000},
	{L10,  700 * 1000},
	{L11,  600 * 1000},
	{L12,  500 * 1000},
	{L13,  400 * 1000},
	{L14,  300 * 1000},
	{L15,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5260_clkdiv_table_CA15[CPUFREQ_LEVEL_END_CA15];

static unsigned int clkdiv_cpu0_5260_CA15_evt0[CPUFREQ_LEVEL_END_CA15][7] = {
	/*
	 * Clock divider value for following
	 * { EGL1, EGL2, EGL_PLL, ACLK_EGL,
	 *   ATCLK, PCLK_DBG, PCLK_EGL }
	 */

	/* ARM L0: 1.7GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L1: 1.6GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L2: 1.5GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L3: 1.4GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L4: 1.3GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L5: 1.2GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L6: 1.1GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L7: 1.0GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L8: 900MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L9: 800MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L10: 700MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L11: 600MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L12: 500MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L13: 400MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L14: 300MHz */
	{ 0, 0, 2, 1, 2, 0, 0 },

	/* ARM L15: 200MHz */
	{ 0, 0, 1, 1, 1, 0, 0 },
};

static unsigned int exynos5260_egl_pll_pms_table_CA15_evt0[CPUFREQ_LEVEL_END_CA15] = {
	/* MDIV | PDIV | SDIV */
	/* EGL_PLL FOUT L0: 1.7GHz */
	PLL2550X_PMS(425, 6, 0),

	/* EGL_PLL FOUT L1: 1.6GHz */
	PLL2550X_PMS(200, 3, 0),

	/* EGL_PLL FOUT L2: 1.5GHz */
	PLL2550X_PMS(250, 4, 0),

	/* EGL_PLL FOUT L3: 1.4GHz */
	PLL2550X_PMS(175, 3, 0),

	/* EGL_PLL FOUT L4: 1.3GHz */
	PLL2550X_PMS(325, 6, 0),

	/* EGL_PLL FOUT L5: 1.2GHz */
	PLL2550X_PMS(400, 4, 1),

	/* EGL_PLL FOUT L6: 1.1GHz */
	PLL2550X_PMS(275, 3, 1),

	/* EGL_PLL FOUT L7: 1.0GHz */
	PLL2550X_PMS(250, 3, 1),

	/* EGL_PLL FOUT L8: 900MHz */
	PLL2550X_PMS(300, 4, 1),

	/* EGL_PLL FOUT L9: 800MHz */
	PLL2550X_PMS(200, 3, 1),

	/* EGL_PLL FOUT L10: 700MHz */
	PLL2550X_PMS(175, 3, 1),

	/* EGL_PLL FOUT L11: 600MHz */
	PLL2550X_PMS(400, 4, 2),

	/* EGL_PLL FOUT L12: 500MHz */
	PLL2550X_PMS(250, 3, 2),

	/* EGL_PLL FOUT L13: 400MHz */
	PLL2550X_PMS(200, 3, 2),

	/* EGL_PLL FOUT L14: 300MHz */
	PLL2550X_PMS(400, 4, 3),

	/* EGL_PLL FOUT L15: 200MHz */
	PLL2550X_PMS(200, 3, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5260_CA15_evt0[CPUFREQ_LEVEL_END_CA15] = {
	1300000,	/* L0  1700 */
	1300000,	/* L1  1600 */
	1300000,	/* L2  1500 */
	1300000,	/* L3  1400 */
	1237500,	/* L4  1300 */
	1175000,	/* L5  1200 */
	1125000,	/* L6  1100 */
	1075000,	/* L7  1000 */
	1025000,	/* L8   900 */
	 975000,	/* L9   800 */
	 925000,	/* L10  700 */
	 900000,	/* L11  600 */
	 900000,	/* L12  500 */
	 900000,	/* L13  400 */
	 900000,	/* L14  300 */
	 900000,	/* L15  200 */
};

static unsigned int clkdiv_cpu0_5260_CA15[CPUFREQ_LEVEL_END_CA15][7] = {
	/*
	 * Clock divider value for following
	 * { EGL1, EGL2, EGL_PLL, ACLK_EGL,
	 *   ATCLK, PCLK_DBG, PCLK_EGL }
	 */

	/* ARM L0: 1.7GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L1: 1.6GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L2: 1.5GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L3: 1.4GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L4: 1.3GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L5: 1.2GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L6: 1.1GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L7: 1.0GHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L8: 900MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L9: 800MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L10: 700MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L11: 600MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L12: 500MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L13: 400MHz */
	{ 0, 0, 3, 1, 3, 0, 0 },

	/* ARM L14: 300MHz */
	{ 0, 0, 2, 1, 2, 0, 0 },

	/* ARM L15: 200MHz */
	{ 0, 0, 1, 1, 1, 0, 0 },
};

static unsigned int exynos5260_egl_pll_pms_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	/* MDIV | PDIV | SDIV */
	/* EGL_PLL FOUT L0: 1.7GHz */
	PLL2550X_PMS(425, 6, 0),

	/* EGL_PLL FOUT L1: 1.6GHz */
	PLL2550X_PMS(200, 3, 0),

	/* EGL_PLL FOUT L2: 1.5GHz */
	PLL2550X_PMS(250, 4, 0),

	/* EGL_PLL FOUT L3: 1.4GHz */
	PLL2550X_PMS(175, 3, 0),

	/* EGL_PLL FOUT L4: 1.3GHz */
	PLL2550X_PMS(325, 6, 0),

	/* EGL_PLL FOUT L5: 1.2GHz */
	PLL2550X_PMS(400, 4, 1),

	/* EGL_PLL FOUT L6: 1.1GHz */
	PLL2550X_PMS(275, 3, 1),

	/* EGL_PLL FOUT L7: 1.0GHz */
	PLL2550X_PMS(250, 3, 1),

	/* EGL_PLL FOUT L8: 900MHz */
	PLL2550X_PMS(300, 4, 1),

	/* EGL_PLL FOUT L9: 800MHz */
	PLL2550X_PMS(200, 3, 1),

	/* EGL_PLL FOUT L10: 700MHz */
	PLL2550X_PMS(175, 3, 1),

	/* EGL_PLL FOUT L11: 600MHz */
	PLL2550X_PMS(400, 4, 2),

	/* EGL_PLL FOUT L12: 500MHz */
	PLL2550X_PMS(250, 3, 2),

	/* EGL_PLL FOUT L13: 400MHz */
	PLL2550X_PMS(200, 3, 2),

	/* EGL_PLL FOUT L14: 300MHz */
	PLL2550X_PMS(400, 4, 3),

	/* EGL_PLL FOUT L15: 200MHz */
	PLL2550X_PMS(200, 3, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5260_CA15[CPUFREQ_LEVEL_END_CA15] = {
	1300000,	/* L0  1700 */
	1250000,	/* L1  1600 */
	1225000,	/* L2  1500 */
	1200000,	/* L3  1400 */
	1150000,	/* L4  1300 */
	1100000,	/* L5  1200 */
	1075000,	/* L6  1100 */
	1050000,	/* L7  1000 */
	1000000,	/* L8   900 */
	 975000,	/* L9   800 */
	 925000,	/* L10  700 */
	 900000,	/* L11  600 */
	 850000,	/* L12  500 */
	 850000,	/* L13  400 */
	 850000,	/* L14  300 */
	 850000,	/* L15  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5260_bus_table_CA15[CPUFREQ_LEVEL_END_CA15] = {
	 667000,	/* 1.7 MHz */
	 667000,	/* 1.6 GHz */
	 667000,	/* 1.5 GHz */
	 667000,	/* 1.4 GHz */
	 667000,	/* 1.3 GHz */
	 667000,	/* 1.2 GHz */
	 667000,	/* 1.1 GHz */
	 543000,	/* 1.0 GHz */
	 543000,	/* 900 MHz */
	 543000,	/* 800 MHz */
	 413000,	/* 700 MHz */
	 413000,	/* 600 MHz */
	 413000,	/* 500 MHz */
	 413000,	/* 400 MHz */
	 413000,	/* 300 MHz */
	 413000,	/* 200 MHz */
};

static int exynos5260_ema_valid_check_CA15(int ema_con_val)
{
	int ret = 0;

	switch (ema_con_val) {
	case EMA_VAL_0:
	case EMA_VAL_1:
	case EMA_VAL_3:
	case EMA_VAL_4:
		ret = ema_con_val;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void exynos5260_set_ema_CA15(unsigned int target_volt)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5260_EGL_EMA_CTRL);

	if (target_volt >= EMA_VOLT) {
		if (egl_ema_con1 > 0) {
			tmp &= ~((EMA_CON_MASK << EGL_L1_EMA_SHIFT) |
				(EMA_CON_MASK << EGL_L2_EMA_SHIFT));
			tmp |= ((egl_ema_con1 << EGL_L1_EMA_SHIFT) |
				(egl_ema_con1 << EGL_L2_EMA_SHIFT));
			__raw_writel(tmp, EXYNOS5260_EGL_EMA_CTRL);
		}
	} else {
		if (egl_ema_con2 > 0) {
			tmp &= ~((EMA_CON_MASK << EGL_L1_EMA_SHIFT) |
				(EMA_CON_MASK << EGL_L2_EMA_SHIFT));
			tmp |= ((egl_ema_con2 << EGL_L1_EMA_SHIFT) |
				(egl_ema_con2 << EGL_L2_EMA_SHIFT));
			__raw_writel(tmp, EXYNOS5260_EGL_EMA_CTRL);
		}
	}
}

static void exynos5260_set_clkdiv_CA15(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - EGL */
	tmp = exynos5260_clkdiv_table_CA15[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS5260_CLKDIV_EGL);

	while (__raw_readl(EXYNOS5260_CLKDIV_STAT_EGL) & 0x1111111)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5260_CLKDIV_EGL);

	pr_info("%s: DIV_EGL[0x%08x]\n", __func__, tmp);
#endif
}

static void exynos5260_set_egl_pll_CA15(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;
	unsigned int timeout = 0;

	/* 1. before change to BUS_PLL, set div for BUS_PLL output */
	if ((new_index < L9) && (old_index < L9))
		exynos5260_set_clkdiv_CA15(L9); /* pll_safe_idx of CA15 */

	/* 2. CLKMUX_SEL_EGL = SCLK_BUS_PLL, EGLCLK uses SCLK_BUS_PLL for lock time */
	if (clk_set_parent(mout_egl_b, sclk_bus_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_bus_pll->name, mout_egl_b->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_EGL);
		tmp &= EXYNOS5260_SRC_STAT_EGL_EGL_B_MASK;
		tmp >>= EXYNOS5260_SRC_STAT_EGL_EGL_B_SHIFT;
		timeout++;
		if (timeout >= 100)
			break;
	} while (tmp != 0x2);

	/* retry MUX change */
	if (timeout) {
		timeout = 0;
		if (clk_set_parent(mout_egl_b, sclk_bus_pll))
			pr_err("Unable to set parent %s of clock %s.\n",
					sclk_bus_pll->name, mout_egl_b->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_EGL);
			tmp &= EXYNOS5260_SRC_STAT_EGL_EGL_B_MASK;
			tmp >>= EXYNOS5260_SRC_STAT_EGL_EGL_B_SHIFT;
			timeout++;
			if (timeout >= 1000)
				panic("%s(%d): failed mux change\n",
						__func__, __LINE__);
		} while (tmp != 0x2);
	}

	/* 3. Set EGL_PLL Lock time */
	if (samsung_rev() == EXYNOS5260_REV_0)
		pdiv = ((exynos5260_egl_pll_pms_table_CA15_evt0[new_index] &
			EXYNOS5260_PLL_PDIV_MASK) >> EXYNOS5260_PLL_PDIV_SHIFT);
	else
		pdiv = ((exynos5260_egl_pll_pms_table_CA15[new_index] &
			EXYNOS5260_PLL_PDIV_MASK) >> EXYNOS5260_PLL_PDIV_SHIFT);

	tmp = __raw_readl(EXYNOS5260_APLL_LOCK);
	tmp &= ~EXYNOS5260_PLL_EGL_LOCKTIME_MASK;
	tmp |= ((pdiv * 200) << EXYNOS5260_PLL_EGL_LOCKTIME_SHIFT);
	__raw_writel(tmp, EXYNOS5260_APLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5260_APLL_CON0);
	tmp &= ~(EXYNOS5260_PLL_MDIV_MASK |
		EXYNOS5260_PLL_PDIV_MASK |
		EXYNOS5260_PLL_SDIV_MASK);
	if (samsung_rev() == EXYNOS5260_REV_0)
		tmp |= exynos5260_egl_pll_pms_table_CA15_evt0[new_index];
	else
		tmp |= exynos5260_egl_pll_pms_table_CA15[new_index];
	__raw_writel(tmp, EXYNOS5260_APLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_APLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5260_APLL_CON0_LOCKED_SHIFT)));

	timeout = 0;

	/* 6. CLKMUX_SEL_EGL2 = MOUT_EGL_PLL */
	if (clk_set_parent(mout_egl_b, mout_egl_a))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_egl_a->name, mout_egl_b->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_EGL);
		tmp &= EXYNOS5260_SRC_STAT_EGL_EGL_B_MASK;
		tmp >>= EXYNOS5260_SRC_STAT_EGL_EGL_B_SHIFT;
		timeout++;
		if (timeout >= 100)
			break;
	} while (tmp != 0x1);

	if (timeout) {
		if (clk_set_parent(mout_egl_b, mout_egl_a))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_egl_a->name, mout_egl_b->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_EGL);
			tmp &= EXYNOS5260_SRC_STAT_EGL_EGL_B_MASK;
			tmp >>= EXYNOS5260_SRC_STAT_EGL_EGL_B_SHIFT;
			timeout++;
			if (timeout >= 1000)
				panic("%s(%d): failed mux change\n",
						__func__, __LINE__);
		} while (tmp != 0x1);
	}

	/* 7. restore original div value */
	if ((new_index < L9) && (old_index < L9))
		exynos5260_set_clkdiv_CA15(new_index);
}

static bool exynos5260_pms_change_CA15(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm;
	unsigned int new_pm;

	if (samsung_rev() == EXYNOS5260_REV_0) {
		old_pm = (exynos5260_egl_pll_pms_table_CA15_evt0[old_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
		new_pm = (exynos5260_egl_pll_pms_table_CA15_evt0[new_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
	} else {
		old_pm = (exynos5260_egl_pll_pms_table_CA15[old_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
		new_pm = (exynos5260_egl_pll_pms_table_CA15[new_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
	}

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5260_set_frequency_CA15(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5260_pms_change_CA15(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5260_set_clkdiv_CA15(new_index);
			/* 2. Change just s value in egl_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5260_APLL_CON0);
			tmp &= ~EXYNOS5260_PLL_SDIV_MASK;
			if (samsung_rev() == EXYNOS5260_REV_0)
				tmp |= (exynos5260_egl_pll_pms_table_CA15_evt0[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			else
				tmp |= (exynos5260_egl_pll_pms_table_CA15[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5260_APLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5260_set_clkdiv_CA15(new_index);
			/* 2. Change the egl_pll m,p,s value */
			exynos5260_set_egl_pll_CA15(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5260_pms_change_CA15(old_index, new_index)) {
			/* 1. Change just s value in egl_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5260_APLL_CON0);
			tmp &= ~EXYNOS5260_PLL_SDIV_MASK;
			if (samsung_rev() == EXYNOS5260_REV_0)
				tmp |= (exynos5260_egl_pll_pms_table_CA15_evt0[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			else
				tmp |= (exynos5260_egl_pll_pms_table_CA15[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5260_APLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5260_set_clkdiv_CA15(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the egl_pll m,p,s value */
			exynos5260_set_egl_pll_CA15(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5260_set_clkdiv_CA15(new_index);
		}
	}

	clk_set_rate(fout_egl_pll, exynos5260_freq_table_CA15[new_index].frequency * 1000);
}

static void __init set_volt_table_CA15(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;
	unsigned int asv_abb = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		asv_volt = get_match_volt(ID_ARM, exynos5260_freq_table_CA15[i].frequency);
		if (!asv_volt) {
			if (samsung_rev() == EXYNOS5260_REV_0)
				exynos5260_volt_table_CA15[i] = asv_voltage_5260_CA15_evt0[i];
			else
				exynos5260_volt_table_CA15[i] = asv_voltage_5260_CA15[i];
		} else {
			exynos5260_volt_table_CA15[i] = asv_volt;
		}

		pr_info("CPUFREQ of CA15  L%d : %d uV\n", i,
				exynos5260_volt_table_CA15[i]);

		asv_abb = get_match_abb(ID_ARM, exynos5260_freq_table_CA15[i].frequency);
		exynos5260_abb_table_CA15[i] = asv_abb;

		pr_info("CPUFREQ of CA15  L%d : ABB %d\n", i,
				exynos5260_abb_table_CA15[i]);
	}

	if (samsung_rev() == EXYNOS5260_REV_0) {
		max_support_idx_CA15 = L3;
		min_support_idx_CA15 = L10;
	} else {
		max_support_idx_CA15 = L0;
		min_support_idx_CA15 = L9;
	}
}

static bool exynos5260_is_alive_CA15(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_EGL);
	tmp &= EXYNOS5260_CLKSRC_STAT_EGL_PLL_MASK;
	tmp >>= EXYNOS5260_CLKSRC_STAT_EGL_PLL_SHIFT;

	return (tmp == 0x1) ? false : true;
}

int __init exynos5_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table_CA15();

	mout_egl_b = clk_get(NULL, "mout_egl_b");
	if (IS_ERR(mout_egl_b)) {
		pr_err("failed get mout_egl_b clk\n");
		return PTR_ERR(mout_egl_b);
	}

	mout_egl_a = clk_get(NULL, "mout_egl_a");
	if (IS_ERR(mout_egl_a)) {
		pr_err("failed get mout_egl_a clk\n");
		goto err_mout_egl_a;
	}

	mout_egl_pll = clk_get(NULL, "mout_egl_pll");
	if (IS_ERR(mout_egl_pll)) {
		pr_err("failed get mout_egl_pll clk\n");
		goto err_mout_egl_pll;
	}

	if (clk_set_parent(mout_egl_a, mout_egl_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_egl_pll->name, mout_egl_a->name);
		goto err_clk_set_parent_egl;
	}

	sclk_bus_pll = clk_get(NULL, "sclk_mpll");
	if (IS_ERR(sclk_bus_pll)) {
		pr_err("failed get sclk_bus_pll clk\n");
		goto err_sclk_bus_pll;
	}

	rate = clk_get_rate(sclk_bus_pll) / 1000;

	fout_egl_pll = clk_get(NULL, "fout_apll");
	if (IS_ERR(fout_egl_pll)) {
		pr_err("failed get fout_egl_pll clk\n");
		goto err_fout_egl_pll;
	}

	clk_put(mout_egl_pll);

	for (i = L0; i < CPUFREQ_LEVEL_END_CA15; i++) {
		exynos5260_clkdiv_table_CA15[i].index = i;

		/* CLK_DIV_EGL */
		tmp = __raw_readl(EXYNOS5260_CLKDIV_EGL);

		tmp &= ~(EXYNOS5260_DIV_EGL_EGL1_MASK |
			EXYNOS5260_DIV_EGL_EGL2_MASK |
			EXYNOS5260_DIV_EGL_ACLK_EGL_MASK |
			EXYNOS5260_DIV_EGL_PCLK_EGL_MASK |
			EXYNOS5260_DIV_EGL_ATCLK_MASK |
			EXYNOS5260_DIV_EGL_PCLK_DBG_MASK |
			EXYNOS5260_DIV_EGL_EGL_PLL_MASK);

		if (samsung_rev() == EXYNOS5260_REV_0) {
			tmp |= ((clkdiv_cpu0_5260_CA15_evt0[i][0] << EXYNOS5260_DIV_EGL_EGL1_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][1] << EXYNOS5260_DIV_EGL_EGL2_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][2] << EXYNOS5260_DIV_EGL_EGL_PLL_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][3] << EXYNOS5260_DIV_EGL_ACLK_EGL_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][4] << EXYNOS5260_DIV_EGL_ATCLK_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][5] << EXYNOS5260_DIV_EGL_PCLK_DBG_SHIFT) |
				(clkdiv_cpu0_5260_CA15_evt0[i][6] << EXYNOS5260_DIV_EGL_PCLK_EGL_SHIFT));
		} else {
			tmp |= ((clkdiv_cpu0_5260_CA15[i][0] << EXYNOS5260_DIV_EGL_EGL1_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][1] << EXYNOS5260_DIV_EGL_EGL2_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][2] << EXYNOS5260_DIV_EGL_EGL_PLL_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][3] << EXYNOS5260_DIV_EGL_ACLK_EGL_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][4] << EXYNOS5260_DIV_EGL_ATCLK_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][5] << EXYNOS5260_DIV_EGL_PCLK_DBG_SHIFT) |
				(clkdiv_cpu0_5260_CA15[i][6] << EXYNOS5260_DIV_EGL_PCLK_EGL_SHIFT));
		}

		exynos5260_clkdiv_table_CA15[i].clkdiv = tmp;
	}

	/* Save EMA control values */
	tmp = __raw_readl(EMA_VAL_REG);
	egl_ema_con1 = (tmp >> EMA_CON1_SHIFT) & EMA_CON_MASK;
	egl_ema_con2 = (tmp >> EMA_CON2_SHIFT) & EMA_CON_MASK;

	egl_ema_con1 = exynos5260_ema_valid_check_CA15(egl_ema_con1);
	egl_ema_con2 = exynos5260_ema_valid_check_CA15(egl_ema_con2);

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L0;
	info->pll_safe_idx = L9;
	info->max_support_idx = max_support_idx_CA15;
	info->min_support_idx = min_support_idx_CA15;
	info->boot_cpu_min_qos = exynos5260_freq_table_CA15[L5].frequency;
	info->boot_cpu_max_qos = exynos5260_freq_table_CA15[L5].frequency;
	info->bus_table = exynos5260_bus_table_CA15;
	info->cpu_clk = fout_egl_pll;

	info->volt_table = exynos5260_volt_table_CA15;
	if (samsung_rev() != EXYNOS5260_REV_0)
		info->abb_table = exynos5260_abb_table_CA15;
	info->freq_table = exynos5260_freq_table_CA15;
	info->set_freq = exynos5260_set_frequency_CA15;
	info->set_ema = exynos5260_set_ema_CA15;
	info->need_apll_change = exynos5260_pms_change_CA15;
	info->is_alive = exynos5260_is_alive_CA15;

#ifdef ENABLE_CLKOUT
	/* dividing EGL_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5260_CLKOUT_CMU_EGL);
	tmp &= ~0x3fff;
	tmp |= 0x10f00;
	__raw_writel(tmp, EXYNOS5260_CLKOUT_CMU_EGL);
#endif

	return 0;

err_fout_egl_pll:
	clk_put(sclk_bus_pll);
err_sclk_bus_pll:
err_clk_set_parent_egl:
	clk_put(mout_egl_pll);
err_mout_egl_pll:
	clk_put(mout_egl_a);
err_mout_egl_a:
	clk_put(mout_egl_b);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
