/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5260 - KFC Core frequency scaling support
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

#define CPUFREQ_LEVEL_END_CA7	(L13 + 1)
#define L2_LOCAL_PWR_EN		0x7

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA7;
static int min_support_idx_CA7 = (CPUFREQ_LEVEL_END_CA7 - 1);

static struct clk *mout_kfc;
static struct clk *mout_kfc_pll;
static struct clk *sclk_media_pll;
static struct clk *fout_kfc_pll;

static int kfc_ema_con1;
static int kfc_ema_con2;

static unsigned int exynos5260_volt_table_CA7[CPUFREQ_LEVEL_END_CA7];
static unsigned int exynos5260_abb_table_CA7[CPUFREQ_LEVEL_END_CA7];

static struct cpufreq_frequency_table exynos5260_freq_table_CA7[] = {
	{L0,  1500 * 1000},
	{L1,  1400 * 1000},
	{L2,  1300 * 1000},
	{L3,  1200 * 1000},
	{L4,  1100 * 1000},
	{L5,  1000 * 1000},
	{L6,   900 * 1000},
	{L7,   800 * 1000},
	{L8,   700 * 1000},
	{L9,   600 * 1000},
	{L10,  500 * 1000},
	{L11,  400 * 1000},
	{L12,  300 * 1000},
	{L13,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5260_clkdiv_table_CA7[CPUFREQ_LEVEL_END_CA7];

static unsigned int clkdiv_cpu0_5260_CA7_evt0[CPUFREQ_LEVEL_END_CA7][7] = {
	/*
	 * Clock divider value for following
	 * { KFC1, KFC2, KFC_PLL, ACLK_KFC,
	 *   ATCLK, PCLK_DBG, PCLK_KFC }
	 */

	/* ARM L0: 1.5GMHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L1: 1.4GMHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L2: 1.3GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L3: 1.2GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L4: 1.1GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L5: 1000MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L6: 900MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L7: 800MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L8: 700MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L9: 600MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L10: 500MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L11: 400MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L12: 300MHz */
	{ 0, 0, 2, 1, 2, 2, 2 },

	/* ARM L13: 200MHz */
	{ 0, 0, 1, 1, 1, 1, 1 },
};

static unsigned int exynos5260_kfc_pll_pms_table_CA7_evt0[CPUFREQ_LEVEL_END_CA7] = {
	/* MDIV | PDIV | SDIV */
	/* KPLL FOUT L0: 1.5GHz */
	PLL2550X_PMS(250, 4, 0),

	/* KPLL FOUT L1: 1.4GHz */
	PLL2550X_PMS(175, 3, 0),

	/* KPLL FOUT L2: 1.3GHz */
	PLL2550X_PMS(325, 6, 0),

	/* KPLL FOUT L3: 1.2GHz */
	PLL2550X_PMS(400, 4, 1),

	/* KPLL FOUT L4: 1.1GHz */
	PLL2550X_PMS(275, 3, 1),

	/* KPLL FOUT L5: 1000MHz */
	PLL2550X_PMS(250, 3, 1),

	/* KPLL FOUT L6: 900MHz */
	PLL2550X_PMS(300, 4, 1),

	/* KPLL FOUT L7: 800MHz */
	PLL2550X_PMS(200, 3, 1),

	/* KPLL FOUT L8: 700MHz */
	PLL2550X_PMS(175, 3, 1),

	/* KPLL FOUT L9: 600MHz */
	PLL2550X_PMS(400, 4, 2),

	/* KPLL FOUT L10: 500MHz */
	PLL2550X_PMS(250, 3, 2),

	/* KPLL FOUT L11: 400MHz */
	PLL2550X_PMS(200, 3, 2),

	/* KPLL FOUT L12: 300MHz */
	PLL2550X_PMS(400, 4, 3),

	/* KPLL FOUT L13: 200MHz */
	PLL2550X_PMS(200, 3, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5260_CA7_evt0[CPUFREQ_LEVEL_END_CA7] = {
	1325000,	/* L0  1500 */
	1325000,	/* L1  1400 */
	1325000,	/* L2  1300 */
	1325000,	/* L3  1200 */
	1325000,	/* L4  1100 */
	1237500,	/* L5  1000 */
	1150000,	/* L6   900 */
	1075000,	/* L7   800 */
	1000000,	/* L8   700 */
	 950000,	/* L9   600 */
	 900000,	/* L10  500 */
	 900000,	/* L11  400 */
	 900000,	/* L12  300 */
	 900000,	/* L13  200 */
};

static unsigned int clkdiv_cpu0_5260_CA7[CPUFREQ_LEVEL_END_CA7][7] = {
	/*
	 * Clock divider value for following
	 * { KFC1, KFC2, KFC_PLL, ACLK_KFC,
	 *   ATCLK, PCLK_DBG, PCLK_KFC }
	 */

	/* ARM L0: 1.5GMHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L1: 1.4GMHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L2: 1.3GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L3: 1.2GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L4: 1.1GHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L5: 1000MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L6: 900MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L7: 800MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L8: 700MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L9: 600MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L10: 500MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L11: 400MHz */
	{ 0, 0, 3, 1, 3, 3, 3 },

	/* ARM L12: 300MHz */
	{ 0, 0, 2, 1, 2, 2, 2 },

	/* ARM L13: 200MHz */
	{ 0, 0, 1, 1, 1, 1, 1 },
};

static unsigned int exynos5260_kfc_pll_pms_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	/* MDIV | PDIV | SDIV */
	/* KPLL FOUT L0: 1.5GHz */
	PLL2550X_PMS(250, 4, 0),

	/* KPLL FOUT L1: 1.4GHz */
	PLL2550X_PMS(175, 3, 0),

	/* KPLL FOUT L2: 1.3GHz */
	PLL2550X_PMS(325, 6, 0),

	/* KPLL FOUT L3: 1.2GHz */
	PLL2550X_PMS(400, 4, 1),

	/* KPLL FOUT L4: 1.1GHz */
	PLL2550X_PMS(275, 3, 1),

	/* KPLL FOUT L5: 1000MHz */
	PLL2550X_PMS(250, 3, 1),

	/* KPLL FOUT L6: 900MHz */
	PLL2550X_PMS(300, 4, 1),

	/* KPLL FOUT L7: 800MHz */
	PLL2550X_PMS(200, 3, 1),

	/* KPLL FOUT L8: 700MHz */
	PLL2550X_PMS(175, 3, 1),

	/* KPLL FOUT L9: 600MHz */
	PLL2550X_PMS(400, 4, 2),

	/* KPLL FOUT L10: 500MHz */
	PLL2550X_PMS(250, 3, 2),

	/* KPLL FOUT L11: 400MHz */
	PLL2550X_PMS(200, 3, 2),

	/* KPLL FOUT L12: 300MHz */
	PLL2550X_PMS(400, 4, 3),

	/* KPLL FOUT L13: 200MHz */
	PLL2550X_PMS(200, 3, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5260_CA7[CPUFREQ_LEVEL_END_CA7] = {
	1250000,	/* L0  1500 */
	1250000,	/* L1  1400 */
	1250000,	/* L2  1300 */
	1200000,	/* L3  1200 */
	1150000,	/* L4  1100 */
	1100000,	/* L5  1000 */
	1050000,	/* L6   900 */
	1025000,	/* L7   800 */
	 975000,	/* L8   700 */
	 925000,	/* L9   600 */
	 875000,	/* L10  500 */
	 850000,	/* L11  400 */
	 850000,	/* L12  300 */
	 850000,	/* L13  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5260_bus_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	543000,	/* 1.5 GHz */
	543000,	/* 1.4 GHz */
	543000,	/* 1.3 GHz */
	543000,	/* 1.2 GHz */
	413000,	/* 1.1 GHz */
	413000,	/* 1.0 GHz */
	413000,	/* 900 MHz */
	275000,	/* 800 MHz */
	275000,	/* 700 MHz */
	0,	/* 600 MHz */
	0,	/* 500 MHz */
	0,	/* 400 MHz */
	0,	/* 300 MHz */
	0,	/* 200 MHz */
};

static int exynos5260_ema_valid_check_CA7(int ema_con_val)
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

static void exynos5260_set_ema_CA7(unsigned int target_volt)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5260_KFC_EMA_CTRL);

	if (target_volt >= EMA_VOLT) {
		if (kfc_ema_con1 > 0) {
			tmp &= ~((EMA_CON_MASK << KFC_EMA_SHIFT) |
				(EMA_CON_MASK << KFC_EMA_HD_SHIFT));
			tmp |= ((kfc_ema_con1 << KFC_EMA_SHIFT) |
				(kfc_ema_con1 << KFC_EMA_HD_SHIFT));
			__raw_writel(tmp, EXYNOS5260_KFC_EMA_CTRL);
		}
	} else {
		if (kfc_ema_con2 > 0) {
			tmp &= ~((EMA_CON_MASK << KFC_EMA_SHIFT) |
				(EMA_CON_MASK << KFC_EMA_HD_SHIFT));
			tmp |= ((kfc_ema_con2 << KFC_EMA_SHIFT) |
				(kfc_ema_con2 << KFC_EMA_HD_SHIFT));
			__raw_writel(tmp, EXYNOS5260_KFC_EMA_CTRL);
		}
	}
}

static void exynos5260_set_clkdiv_CA7(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - KFC */
	tmp = exynos5260_clkdiv_table_CA7[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS5260_CLKDIV_KFC);

	while (__raw_readl(EXYNOS5260_CLKDIV_STAT_KFC) & 0x1111111)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5260_CLKDIV_KFC);
	pr_info("%s: DIV_KFC[0x%08x]\n", __func__, tmp);
#endif
}

static void exynos5260_set_kfc_pll_CA7(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. before change to MEDIA_PLL, set div for MEDIA_PLL output */
	if ((new_index < L8) && (old_index < L8))
		exynos5260_set_clkdiv_CA7(L8); /* pll_safe_idx of CA7 */

	/* 2. CLKMUX_SEL_KFC2 = MEDIA_PLL, KFCCLK uses MEDIA_PLL for lock time */
	if (clk_set_parent(mout_kfc, sclk_media_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_media_pll->name, mout_kfc->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_KFC2);
		tmp &= EXYNOS5260_SRC_STAT_KFC2_MASK;
	} while (tmp != 0x2);

	/* 3. Set KFC_PLL Lock time */
	if (samsung_rev() == EXYNOS5260_REV_0)
		pdiv = ((exynos5260_kfc_pll_pms_table_CA7_evt0[new_index] &
			EXYNOS5260_PLL_PDIV_MASK) >> EXYNOS5260_PLL_PDIV_SHIFT);
	else
		pdiv = ((exynos5260_kfc_pll_pms_table_CA7[new_index] &
			EXYNOS5260_PLL_PDIV_MASK) >> EXYNOS5260_PLL_PDIV_SHIFT);

	tmp = __raw_readl(EXYNOS5260_KPLL_LOCK);
	tmp &= ~EXYNOS5260_PLL_KFC_LOCKTIME_MASK;
	tmp |= ((pdiv * 200) << EXYNOS5260_PLL_KFC_LOCKTIME_SHIFT);
	__raw_writel(tmp, EXYNOS5260_KPLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5260_KPLL_CON0);
	tmp &= ~(EXYNOS5260_PLL_MDIV_MASK |
		EXYNOS5260_PLL_PDIV_MASK |
		EXYNOS5260_PLL_SDIV_MASK);
	if (samsung_rev() == EXYNOS5260_REV_0)
		tmp |= exynos5260_kfc_pll_pms_table_CA7_evt0[new_index];
	else
		tmp |= exynos5260_kfc_pll_pms_table_CA7[new_index];
	__raw_writel(tmp, EXYNOS5260_KPLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_KPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5260_KPLL_CON0_LOCKED_SHIFT)));

	/* 6. CLKMUX_SEL_KFC2 = MOUT_KFC_PLL */
	if (clk_set_parent(mout_kfc, mout_kfc_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_kfc_pll->name, mout_kfc->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_KFC2);
		tmp &= EXYNOS5260_SRC_STAT_KFC2_MASK;
	} while (tmp != 0x1);

	/* 7. restore original div value */
	if ((new_index < L8) && (old_index < L8))
		exynos5260_set_clkdiv_CA7(new_index);
}

static bool exynos5260_pms_change_CA7(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm;
	unsigned int new_pm;

	if (samsung_rev() == EXYNOS5260_REV_0) {
		old_pm = (exynos5260_kfc_pll_pms_table_CA7_evt0[old_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
		new_pm = (exynos5260_kfc_pll_pms_table_CA7_evt0[new_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
	} else {
		old_pm = (exynos5260_kfc_pll_pms_table_CA7[old_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
		new_pm = (exynos5260_kfc_pll_pms_table_CA7[new_index] >>
				EXYNOS5260_PLL_PDIV_SHIFT);
	}

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5260_set_frequency_CA7(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5260_pms_change_CA7(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5260_set_clkdiv_CA7(new_index);
			/* 2. Change just s value in kfc_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5260_KPLL_CON0);
			tmp &= ~EXYNOS5260_PLL_SDIV_MASK;
			if (samsung_rev() == EXYNOS5260_REV_0)
				tmp |= (exynos5260_kfc_pll_pms_table_CA7_evt0[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			else
				tmp |= (exynos5260_kfc_pll_pms_table_CA7[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5260_KPLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5260_set_clkdiv_CA7(new_index);
			/* 2. Change the kfc_pll m,p,s value */
			exynos5260_set_kfc_pll_CA7(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5260_pms_change_CA7(old_index, new_index)) {
			/* 1. Change just s value in kfc_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5260_KPLL_CON0);
			tmp &= ~EXYNOS5260_PLL_SDIV_MASK;
			if (samsung_rev() == EXYNOS5260_REV_0)
				tmp |= (exynos5260_kfc_pll_pms_table_CA7_evt0[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			else
				tmp |= (exynos5260_kfc_pll_pms_table_CA7[new_index] &
						EXYNOS5260_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5260_KPLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5260_set_clkdiv_CA7(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the kfc_pll m,p,s value */
			exynos5260_set_kfc_pll_CA7(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5260_set_clkdiv_CA7(new_index);
		}
	}

	clk_set_rate(fout_kfc_pll, exynos5260_freq_table_CA7[new_index].frequency * 1000);
}

static void __init set_volt_table_CA7(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;
	unsigned int asv_abb = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		asv_volt = get_match_volt(ID_KFC, exynos5260_freq_table_CA7[i].frequency);
		if (!asv_volt) {
			if (samsung_rev() == EXYNOS5260_REV_0)
				exynos5260_volt_table_CA7[i] = asv_voltage_5260_CA7_evt0[i];
			else
				exynos5260_volt_table_CA7[i] = asv_voltage_5260_CA7[i];
		} else {
			exynos5260_volt_table_CA7[i] = asv_volt;
		}

		pr_info("CPUFREQ of CA7  L%d : %d uV\n", i,
				exynos5260_volt_table_CA7[i]);

		asv_abb = get_match_abb(ID_KFC, exynos5260_freq_table_CA7[i].frequency);
		if (!asv_abb)
			exynos5260_abb_table_CA7[i] = ABB_BYPASS;
		else
			exynos5260_abb_table_CA7[i] = asv_abb;

		pr_info("CPUFREQ of CA7  L%d : ABB %d\n", i,
				exynos5260_abb_table_CA7[i]);
	}

	if (samsung_rev() == EXYNOS5260_REV_0) {
		max_support_idx_CA7 = L4;
		min_support_idx_CA7 = L13;
	} else {
		max_support_idx_CA7 = L2;
		min_support_idx_CA7 = L10;
	}
}

static bool exynos5260_is_alive_CA7(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5260_CLKSRC_STAT_KFC0);
	tmp &= EXYNOS5260_CLKSRC_STAT_KFC_PLL_MASK;

	return (tmp == 0x1) ? false : true;
}

int __init exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table_CA7();

	mout_kfc = clk_get(NULL, "mout_kfc");
	if (IS_ERR(mout_kfc)) {
		pr_err("failed get mout_kfc clk\n");
		return PTR_ERR(mout_kfc);
	}

	mout_kfc_pll = clk_get(NULL, "mout_kfc_pll");
	if (IS_ERR(mout_kfc_pll)) {
		pr_err("failed get mout_kfc_pll clk\n");
		goto err_mout_kfc_pll;
	}

	sclk_media_pll = clk_get(NULL, "sclk_cpll");
	if (IS_ERR(sclk_media_pll)) {
		pr_err("failed get sclk_media_pll clk\n");
		goto err_sclk_media_pll;
	}

	rate = clk_get_rate(sclk_media_pll) / 1000;

	fout_kfc_pll = clk_get(NULL, "fout_kpll");
	if (IS_ERR(fout_kfc_pll)) {
		pr_err("failed get fout_kfc_pll clk\n");
		goto err_fout_kfc_pll;
	}

	for (i = L0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		exynos5260_clkdiv_table_CA7[i].index = i;

		/* CLK_DIV_KFC */
		tmp = __raw_readl(EXYNOS5260_CLKDIV_KFC);

		tmp &= ~(EXYNOS5260_DIV_KFC_KFC1_MASK |
			EXYNOS5260_DIV_KFC_KFC2_MASK |
			EXYNOS5260_DIV_KFC_ATCLK_MASK |
			EXYNOS5260_DIV_KFC_PCLK_DBG_MASK |
			EXYNOS5260_DIV_KFC_ACLK_KFC_MASK |
			EXYNOS5260_DIV_KFC_PCLK_KFC_MASK |
			EXYNOS5260_DIV_KFC_KFC_PLL_MASK);

		if (samsung_rev() == EXYNOS5260_REV_0) {
			tmp |= ((clkdiv_cpu0_5260_CA7_evt0[i][0] << EXYNOS5260_DIV_KFC_KFC1_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][1] << EXYNOS5260_DIV_KFC_KFC2_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][2] << EXYNOS5260_DIV_KFC_KFC_PLL_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][3] << EXYNOS5260_DIV_KFC_ACLK_KFC_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][4] << EXYNOS5260_DIV_KFC_ATCLK_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][5] << EXYNOS5260_DIV_KFC_PCLK_DBG_SHIFT) |
				(clkdiv_cpu0_5260_CA7_evt0[i][6] << EXYNOS5260_DIV_KFC_PCLK_KFC_SHIFT));
		} else {
			tmp |= ((clkdiv_cpu0_5260_CA7[i][0] << EXYNOS5260_DIV_KFC_KFC1_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][1] << EXYNOS5260_DIV_KFC_KFC2_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][2] << EXYNOS5260_DIV_KFC_KFC_PLL_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][3] << EXYNOS5260_DIV_KFC_ACLK_KFC_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][4] << EXYNOS5260_DIV_KFC_ATCLK_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][5] << EXYNOS5260_DIV_KFC_PCLK_DBG_SHIFT) |
				(clkdiv_cpu0_5260_CA7[i][6] << EXYNOS5260_DIV_KFC_PCLK_KFC_SHIFT));
		}

		exynos5260_clkdiv_table_CA7[i].clkdiv = tmp;
	}

	/* Save EMA control values */
	tmp = __raw_readl(EMA_VAL_REG);
	kfc_ema_con1 = (tmp >> EMA_CON1_SHIFT) & EMA_CON_MASK;
	kfc_ema_con2 = (tmp >> EMA_CON2_SHIFT) & EMA_CON_MASK;

	kfc_ema_con1 = exynos5260_ema_valid_check_CA7(kfc_ema_con1);
	kfc_ema_con2 = exynos5260_ema_valid_check_CA7(kfc_ema_con2);

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L0;
	info->pll_safe_idx = L8;
	info->max_support_idx = max_support_idx_CA7;
	info->min_support_idx = min_support_idx_CA7;
	if (samsung_rev() == EXYNOS5260_REV_0) {
		info->boot_cpu_min_qos = exynos5260_freq_table_CA7[L4].frequency;
		info->boot_cpu_max_qos = exynos5260_freq_table_CA7[L4].frequency;
	} else {
		info->boot_cpu_min_qos = exynos5260_freq_table_CA7[L2].frequency;
		info->boot_cpu_max_qos = exynos5260_freq_table_CA7[L2].frequency;
	}
	info->bus_table = exynos5260_bus_table_CA7;
	info->cpu_clk = fout_kfc_pll;

	info->volt_table = exynos5260_volt_table_CA7;
	if (samsung_rev() != EXYNOS5260_REV_0)
		info->abb_table = exynos5260_abb_table_CA7;
	info->freq_table = exynos5260_freq_table_CA7;
	info->set_freq = exynos5260_set_frequency_CA7;
	info->set_ema = exynos5260_set_ema_CA7;
	info->need_apll_change = exynos5260_pms_change_CA7;
	info->is_alive = exynos5260_is_alive_CA7;

#ifdef ENABLE_CLKOUT
	/* dividing KFC_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5260_CLKOUT_CMU_KFC);
	tmp &= ~0x3fff;
	tmp |= 0x10f00;
	__raw_writel(tmp, EXYNOS5260_CLKOUT_CMU_KFC);
#endif

	return 0;

err_fout_kfc_pll:
	clk_put(sclk_media_pll);
err_sclk_media_pll:
	clk_put(mout_kfc_pll);
err_mout_kfc_pll:
	clk_put(mout_kfc);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
