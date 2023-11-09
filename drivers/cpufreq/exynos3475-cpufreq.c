/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS3475 - KFC Core frequency scaling support
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

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-clock-exynos3475.h>
#include <mach/regs-pmu.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <mach/asv-exynos_cal.h>

#define CPUFREQ_LEVEL_END	(L12 + 1)
#define DIV_NUM	9

static int pll_safe_idx;
static int max_support_idx;
static int min_support_idx = (CPUFREQ_LEVEL_END - 1);

static struct clk *cpu_pll;
static struct clk *mout_cpu;
static struct clk *mout_mpll;

//static unsigned int exynos3475_volt_table[CPUFREQ_LEVEL_END];

static unsigned int exynos3475_volt_table[CPUFREQ_LEVEL_END] = {
	1200000, 1175000, 1150000, 1125000, 1100000, 1075000, 1050000,
	1025000, 1000000, 975000, 950000, 925000, 900000,
};

static struct cpufreq_frequency_table exynos3475_freq_table[] = {
	{L0,  1495 * 1000},
	{L1,  1404 * 1000},
	{L2,  1300 * 1000},
	{L3,  1196 * 1000},
	{L4,  1105 * 1000},
	{L5,  1001 * 1000},
	{L6,   897 * 1000},
	{L7,   806 * 1000},
	{L8,   702 * 1000},
	{L9,   598 * 1000},
	{L10,  507 * 1000},
	{L11,  403 * 1000},
	{L12,  299 * 1000},
	{0, CPUFREQ_TABLE_END},
};
#if 0
static unsigned int exynos3475_clkdiv_table[9][CPUFREQ_LEVEL_END] = {
	/* Divider , L0, L1, L2, L3, L4, L5, L6, L7, L8, L9, L10, L11, L12 */
	{EXYNOS3475_CLK_CON_DIV_CPU_1,		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{EXYNOS3475_CLK_CON_DIV_CPU_2,		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{EXYNOS3475_CLK_CON_DIV_ACLK_CPU,	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{EXYNOS3475_CLK_CON_DIV_ATCLK_CPU,	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
	{EXYNOS3475_CLK_CON_DIV_PCLK_CPU,	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
	{EXYNOS3475_CLK_CON_DIV_PCLK_DBG,	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
	{EXYNOS3475_CLK_CON_DIV_CPU_RUN_MONITOR,3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
	{EXYNOS3475_CLK_CON_DIV_CPU_PLL,	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{EXYNOS3475_CLK_CON_DIV_SCLK_HPM_CPU,	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
};
#endif
static unsigned int exynos3475_apll_pms_table[CPUFREQ_LEVEL_END] = {
	/* MDIV | PDIV | SDIV */
	/* APLL FOUT L0: 1500MHz */
	PLL2555X_PMS(230, 4, 0),
	/* APLL FOUT L1: 1400MHz */
	PLL2555X_PMS(216, 4, 0),
	/* APLL FOUT L2: 1300MHz */
	PLL2555X_PMS(200, 4, 0),
	/* APLL FOUT L3: 1200MHz */
	PLL2555X_PMS(368, 4, 1),
	/* APLL FOUT L4: 1100MHz */
	PLL2555X_PMS(340, 4, 1),
	/* APLL FOUT L5: 1000MHz */
	PLL2555X_PMS(308, 4, 1),
	/* APLL FOUT L6:  900MHz */
	PLL2555X_PMS(276, 4, 1),
	/* APLL FOUT L7:  800MHz */
	PLL2555X_PMS(248, 4, 1),
	/* APLL FOUT L8:  700MHz */
	PLL2555X_PMS(216, 4, 1),
	/* APLL FOUT L9:  600MHz */
	PLL2555X_PMS(368, 4, 2),
	/* APLL FOUT L10:  500MHz */
	PLL2555X_PMS(312, 4, 2),
	/* APLL FOUT L11:  400MHz */
	PLL2555X_PMS(248, 4, 2),
	/* APLL FOUT L12:  300MHz */
	PLL2555X_PMS(368, 4, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_3475[CPUFREQ_LEVEL_END] = {
	1200000,	/* L0  1500 */
	1175000,	/* L1  1400 */
	1150000,	/* L2  1300 */
	1125000,	/* L3  1200 */
	1100000,	/* L4  1100 */
	1075000,	/* L5  1000 */
	1050000,	/* L6   900 */
	1025000,	/* L7   800 */
	1000000,	/* L8   700 */
	 975000,	/* L9   600 */
	 950000,	/* L10  500 */
	 925000,	/* L11  400 */
	 900000,	/* L12  300 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos3475_bus_table[CPUFREQ_LEVEL_END] = {
	666000,		/* 1.5 GHz */
	666000,		/* 1.4 GHz */
	666000,		/* 1.3 GHz */
	666000,		/* 1.2 GHz */
	666000,		/* 1.1 GHz */
	559000,		/* 1.0 GHz */
	413000,		/* 900 MHz */
	413000,		/* 800 MHz */
	413000,		/* 700 MHz */
	413000,		/* 600 MHz */
	413000,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
};
static void exynos3475_set_clkdiv(unsigned int div_index)
{
	/*
	 * Change Divider
	 * CPU1/2, ACLK_CPU, ATCLK, PCLK_CPU, PCLK_DBG_CP, SCLK_CNTCLK
	 * SCLK_CPU_PLL, SCLK_HPM_CPU
	 */
#if 0
	for (i=0; i < DIV_NUM; i++) {
		__raw_writel(exynos3475_clkdiv_table[i][div_index],
				exyno3475_clkdiv_table[i][0]);
		while(__raw_readl(exyno3475_clkdiv_table[i][0]) & (0x1 << 25))
				cpu_relax();
	};
#endif
}

static void exynos3475_set_ema(unsigned int volt)
{
	cal_set_ema(SYSC_DVFS_CL0, volt);
}

bool exynos3475_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = exynos3475_apll_pms_table[old_index] >> 4;
	unsigned int new_pm = exynos3475_apll_pms_table[new_index] >> 4;

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos3475_set_apll(unsigned int new_index, unsigned int old_index)
{
	if (!exynos3475_pms_change(old_index, new_index)) {
		clk_set_rate(cpu_pll, exynos3475_freq_table[new_index].frequency * 1000);
	} else {
		/* 1. before change to BUS_PLL, set div for BUS_PLL output */
		if ((new_index > pll_safe_idx) && (old_index > pll_safe_idx))
			exynos3475_set_clkdiv(pll_safe_idx); /* pll_safe_idx */

		/* 2. CLKMUX_SEL_CPU = MOUT_BUS_PLL_USER, CPUCLK uses BUS_PLL_USER for lock time */
		if (clk_set_parent(mout_cpu, mout_mpll))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_mpll->name, mout_cpu->name);

		/* 3. Change CPU_PLL frequency */
		clk_set_rate(cpu_pll, exynos3475_freq_table[new_index].frequency * 1000);

		/* 4. CLKMUX_SEL_CPU = MOUT_CPU_PLL */
		if (clk_set_parent(mout_cpu, cpu_pll))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_cpu->name, cpu_pll->name);

		/* 5. restore original div value */
		if ((new_index > pll_safe_idx) && (old_index > pll_safe_idx))
			exynos3475_set_clkdiv(new_index);
	}
}

static void exynos3475_set_frequency(unsigned int old_index,
					 unsigned int new_index)
{
	if (old_index > new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the system clock divider values */
		exynos3475_set_clkdiv(new_index);
		/* 2. Change the apll m,p,s value */
		exynos3475_set_apll(new_index, old_index);
	} else {
		/* Clock Configuration Procedure */
		/* 1. Change the apll m,p,s value */
		exynos3475_set_apll(new_index, old_index);
		/* 2. Change the system clock divider values */
		exynos3475_set_clkdiv(new_index);
	}

	pr_debug("[%s] NewFreq: %d OldFreq: %d\n", __func__,
			exynos3475_freq_table[new_index].frequency,
			exynos3475_freq_table[old_index].frequency);
}

static void __init set_volt_table(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END; i++) {
		asv_volt = get_match_volt(ID_CL0, exynos3475_freq_table[i].frequency);
		if (!asv_volt)
			exynos3475_volt_table[i] = asv_voltage_3475[i];
		else
			exynos3475_volt_table[i] = asv_volt;

		pr_info("CPUFREQ: L%d : %d uV\n", i,
				exynos3475_volt_table[i]);
	}

	max_support_idx = L2;	/* 1.3GHz */
	min_support_idx = L11;	/* 400MHz */
	pr_info("CPUFREQ : max_freq : L%d %u khz\n", max_support_idx,
		exynos3475_freq_table[max_support_idx].frequency);
	pr_info("CPUFREQ : min_freq : L%d %u khz\n", min_support_idx,
		exynos3475_freq_table[min_support_idx].frequency);
}

int __init exynos3475_cpufreq_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;

	set_volt_table();

	cpu_pll = clk_get(NULL, "cpu_pll");
	if (IS_ERR(cpu_pll))
		return PTR_ERR(cpu_pll);

	mout_cpu = clk_get(NULL, "mout_cpu");
	if (IS_ERR(mout_cpu))
		goto err_moutcpu;

	mout_mpll = clk_get(NULL, "mout_sclk_bus_pll_user");
	if (IS_ERR(mout_mpll))
		goto err_moutmpll;
	rate = clk_get_rate(mout_mpll) / 1000;

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = pll_safe_idx = L7;

	info->max_support_idx = max_support_idx;
	info->min_support_idx = min_support_idx;
	info->suspend_freq = exynos3475_freq_table[L9].frequency;
	info->cpu_clk = cpu_pll;
	/* booting frequency is 1.2GHz ~ 1.3GHz */
	info->boot_cpu_min_qos = exynos3475_freq_table[CONFIG_BOOTING_FREQ].frequency;
	info->boot_cpu_max_qos = exynos3475_freq_table[CONFIG_BOOTING_FREQ].frequency;
	info->bus_table = exynos3475_bus_table;
	info->set_ema = exynos3475_set_ema;

	info->volt_table = exynos3475_volt_table;
	info->freq_table = exynos3475_freq_table;
	info->set_freq = exynos3475_set_frequency;
	info->need_apll_change = exynos3475_pms_change;

	return 0;

err_moutmpll:
	clk_put(mout_cpu);
err_moutcpu:
	clk_put(cpu_pll);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos3475_cpufreq_init);
