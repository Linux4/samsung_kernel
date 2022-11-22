/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS3472 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>

#include <mach/asv-exynos.h>
#include <mach/regs-clock.h>
#include <mach/cpufreq.h>

#define CPUFREQ_LEVEL_END	(L9 + 1)

int arm_lock;
//static int max_support_idx;

static struct clk *cpu_clk;
static struct clk *moutcore;
static struct clk *mout_mpll;
static struct clk *mout_apll;
static struct clk *fout_apll;

static unsigned int exynos3472_volt_table[CPUFREQ_LEVEL_END];
static unsigned int exynos3472_cpu_asv_abb[CPUFREQ_LEVEL_END];

static struct cpufreq_frequency_table exynos3472_freq_table[] = {
	{L0, 1000 * 1000},
	{L1,  900 * 1000},
	{L2,  800 * 1000},
	{L3,  700 * 1000},
	{L4,  600 * 1000},
	{L5,  500 * 1000},
	{L6,  400 * 1000},
	{L7,  300 * 1000},
	{L8,  200 * 1000},
	{L9,  100 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos3472_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0_3472[CPUFREQ_LEVEL_END][6] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */

	/* ARM L0: 1000Mhz */
	{ 0, 1, 4, 7, 1, 0 },

	/* ARM L1: 900MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L2: 800MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L3: 700MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L4: 600MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L5: 500MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L6: 400MHz */
	{ 0, 1, 3, 7, 1, 0 },

	/* ARM L7: 300MHz */
	{ 0, 1, 3, 5, 1, 0 },

	/* ARM L8: 200MHz */
	{ 0, 1, 3, 3, 1, 0 },

	/* ARM L9: 100MHz */
	{ 0, 1, 1, 1, 1, 0 },
};

static unsigned int clkdiv_cpu1_3472[CPUFREQ_LEVEL_END][2] = {
	/*
	 *Clock divider value for following
	 * { DIVCOPY, DIVHPM }
	 */
	/* ARM L0: 1000MHz */
	{ 7, 7 },

	/* ARM L1: 900MHz */
	{ 7, 7 },

	/* ARM L2: 800MHz */
	{ 7, 7 },

	/* ARM L3: 700MHz */
	{ 7, 7 },

	/* ARM L4: 600MHz */
	{ 7, 7 },

	/* ARM L5: 500MHz */
	{ 7, 7 },

	/* ARM L6: 400MHz */
	{ 7, 7 },

	/* ARM L7: 300MHz */
	{ 7, 7 },

	/* ARM L8: 200MHz */
	{ 7, 7 },

	/* ARM L9: 100MHz */
	{ 7, 7 },

};

static unsigned int exynos3472_apll_pms_table[CPUFREQ_LEVEL_END] = {

	/* APLL FOUT L0: 1000MHz */
	((250<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L1: 900MHz */
	((300<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L2: 800MHz */
	((200<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L3: 700MHz */
	((175<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L4: 600MHz */
	((400<<16)|(4<<8)|(0x2)),

	/* APLL FOUT L5: 500MHz */
	((250<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L6: 400MHz */
	((200<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L7: 300MHz */
	((400<<16)|(4<<8)|(0x3)),

	/* APLL FOUT L8: 200MHz */
	((200<<16)|(3<<8)|(0x3)),

	/* APLL FOUT L9: 100MHz */
	((200<<16)|(3<<8)|(0x4)),
};

static const unsigned int asv_voltage_3472[CPUFREQ_LEVEL_END] = {
	1050000,
	1000000,
	 950000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
	 900000,
};

static void exynos3472_set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - CPU0 */
	tmp = exynos3472_clkdiv_table[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU);

	while (__raw_readl(EXYNOS4_CLKDIV_STATCPU) & 0x11111111)
		cpu_relax();

	/* Change Divider - CPU1 */
	tmp = exynos3472_clkdiv_table[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU1);

	while (__raw_readl(EXYNOS4_CLKDIV_STATCPU1) & 0x111)
		cpu_relax();
}

static void exynos3472_set_apll(unsigned int index)
{
	unsigned int tmp, pdiv;

	/* 1. MUX_CORE_SEL = MPLL, ARMCLK uses MPLL for lock time */
	clk_set_parent(moutcore, mout_mpll);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS4_CLKMUX_STATCPU)
			>> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set APLL Lock time */
	pdiv = ((exynos3472_apll_pms_table[index] >> 8) & 0x3f);

	__raw_writel((pdiv * 270), EXYNOS4_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS4_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos3472_apll_pms_table[index];
	__raw_writel(tmp, EXYNOS4_APLL_CON0);

	/* 4. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS4_APLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS4_APLLCON0_LOCKED_SHIFT)));

	/* 5. MUX_CORE_SEL = APLL */
	clk_set_parent(moutcore, mout_apll);

	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS4_CLKMUX_STATCPU);
		tmp &= EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT));
}

bool exynos3472_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = exynos3472_apll_pms_table[old_index] >> 8;
	unsigned int new_pm = exynos3472_apll_pms_table[new_index] >> 8;

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos3472_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos3472_pms_change(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos3472_set_clkdiv(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos3472_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS4_APLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos3472_set_clkdiv(new_index);
			/* 2. Change the apll m,p,s value */
			exynos3472_set_apll(new_index);
		}
	} else if (old_index < new_index) {
		if (!exynos3472_pms_change(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos3472_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS4_APLL_CON0);
			/* 2. Change the system clock divider values */
			exynos3472_set_clkdiv(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apll m,p,s value */
			exynos3472_set_apll(new_index);
			/* 2. Change the system clock divider values */
			exynos3472_set_clkdiv(new_index);
		}
	}

	clk_set_rate(fout_apll, exynos3472_freq_table[new_index].frequency*1000);
}

static int __init set_volt_table(void)
{
	unsigned int i;

	for (i = 0; i < CPUFREQ_LEVEL_END; i++) {
		exynos3472_volt_table[i] = get_match_volt(ID_ARM, exynos3472_freq_table[i].frequency);

		if (arm_lock == 4) {
			if (i >= L7) exynos3472_volt_table[i] = get_match_volt(ID_ARM, exynos3472_freq_table[L7].frequency);
		} else if (arm_lock == 5) {
			if (i >= L6) exynos3472_volt_table[i] = get_match_volt(ID_ARM, exynos3472_freq_table[L6].frequency);
		} else if (arm_lock == 6) {
			if (i >= L5) exynos3472_volt_table[i] = get_match_volt(ID_ARM, exynos3472_freq_table[L5].frequency);
		} else if (arm_lock == 7) {
			if (i >= L4) exynos3472_volt_table[i] = get_match_volt(ID_ARM, exynos3472_freq_table[L4].frequency);
		}

		if (exynos3472_volt_table[i] == 0) {
			pr_err("%s: invalid value\n", __func__);
			return -EINVAL;
		}

		exynos3472_cpu_asv_abb[i] = get_match_abb(ID_ARM, exynos3472_freq_table[i].frequency);

		pr_info("CPUFREQ L%d : %d uV ABB : %d\n", i,
					exynos3472_volt_table[i],
					exynos3472_cpu_asv_abb[i]);
	}

	return 0;
}

int exynos3472_cpufreq_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table();

	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	moutcore = clk_get(NULL, "moutcore");
	if (IS_ERR(moutcore))
		goto err_moutcore;

	mout_mpll = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mout_mpll))
		goto err_mout_mpll;

	rate = clk_get_rate(mout_mpll) / 1000;

	mout_apll = clk_get(NULL, "mout_apll");
	fout_apll = clk_get(NULL, "fout_apll");

	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {
		exynos3472_clkdiv_table[i].index = i;

		tmp = __raw_readl(EXYNOS4_CLKDIV_CPU);

		tmp &= ~(EXYNOS4_CLKDIV_CPU0_CORE_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM0_MASK |
			EXYNOS4_CLKDIV_CPU0_ATB_MASK |
			EXYNOS4_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS4_CLKDIV_CPU0_APLL_MASK |
			EXYNOS4_CLKDIV_CPU0_CORE2_MASK);

		tmp |= ((clkdiv_cpu0_3472[i][0] << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT) |
			(clkdiv_cpu0_3472[i][1] << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT) |
			(clkdiv_cpu0_3472[i][2] << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT) |
			(clkdiv_cpu0_3472[i][3] << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT) |
			(clkdiv_cpu0_3472[i][4] << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT) |
			(clkdiv_cpu0_3472[i][5] << EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT));

		exynos3472_clkdiv_table[i].clkdiv = tmp;

		tmp = __raw_readl(EXYNOS4_CLKDIV_CPU1);

		tmp &= ~(EXYNOS4_CLKDIV_CPU1_COPY_MASK |
			EXYNOS4_CLKDIV_CPU1_HPM_MASK |
			EXYNOS4_CLKDIV_CPU1_CORES_MASK);
		tmp |= ((clkdiv_cpu1_3472[i][0] << EXYNOS4_CLKDIV_CPU1_COPY_SHIFT) |
			(clkdiv_cpu1_3472[i][1] << EXYNOS4_CLKDIV_CPU1_HPM_SHIFT));
		exynos3472_clkdiv_table[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pm_lock_idx = L6;
	info->pll_safe_idx = L6;
	info->max_support_idx = L0;
	info->min_support_idx = L9;
	info->cpu_clk = cpu_clk;
	info->volt_table = exynos3472_volt_table;
	info->abb_table = exynos3472_cpu_asv_abb;
	info->freq_table = exynos3472_freq_table;
	info->set_freq = exynos3472_set_frequency;
	info->need_apll_change = exynos3472_pms_change;
	return 0;

err_mout_apll:
	clk_put(mout_mpll);
err_mout_mpll:
	clk_put(moutcore);
err_moutcore:
	clk_put(cpu_clk);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos3472_cpufreq_init);
