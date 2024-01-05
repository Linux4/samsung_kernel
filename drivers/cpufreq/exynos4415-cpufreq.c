/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4415 - CPU frequency scaling support
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

#include <mach/regs-clock-exynos4415.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <mach/devfreq.h>

#include <mach/sec_debug.h>

#define CPUFREQ_LEVEL_END	(L15 + 1)

static int max_support_idx;
static int min_support_idx = (CPUFREQ_LEVEL_END - 1);

static struct clk *cpu_clk;
static struct clk *moutcore;
static struct clk *mout_mpll;
static struct clk *mout_apll;

static unsigned int exynos4415_volt_table[CPUFREQ_LEVEL_END];
static unsigned int exynos4415_arm_abb_table[CPUFREQ_LEVEL_END];

static struct cpufreq_frequency_table exynos4415_freq_table[] = {
	{L0, 1600 * 1000},
	{L1, 1500 * 1000},
	{L2, 1400 * 1000},
	{L3, 1300 * 1000},
	{L4, 1200 * 1000},
	{L5, 1100 * 1000},
	{L6, 1000 * 1000},
	{L7,  900 * 1000},
	{L8,  800 * 1000},
	{L9,  700 * 1000},
	{L10, 600 * 1000},
	{L11, 500 * 1000},
	{L12, 400 * 1000},
	{L13, 300 * 1000},
	{L14, 200 * 1000},
	{L15, 100 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos4415_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0_4415[CPUFREQ_LEVEL_END][8] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM0, DIVCOREM1, DIVPERIPH
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */
	/* ARM L0: 1600Mhz */
	{ 0, 3, 7, 7, 3, 1, 3, 0 },

	/* ARM L1: 1500Mhz */
	{ 0, 3, 7, 7, 3, 1, 3, 0 },

	/* ARM L2: 1400Mhz */
	{ 0, 3, 6, 7, 3, 1, 2, 0 },

	/* ARM L3: 1300Mhz */
	{ 0, 3, 6, 7, 3, 1, 2, 0 },

	/* ARM L4: 1200Mhz */
	{ 0, 3, 5, 7, 3, 1, 2, 0 },

	/* ARM L5: 1100MHz */
	{ 0, 3, 5, 7, 3, 1, 2, 0 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 4, 7, 3, 1, 2, 0 },

	/* ARM L7: 900MHz */
	{ 0, 2, 4, 7, 3, 1, 1, 0 },

	/* ARM L8: 800MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L9: 700MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L10: 600MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L11: 500MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L12: 400MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L13: 300MHz */
	{ 0, 2, 3, 7, 3, 1, 1, 0 },

	/* ARM L14: 200MHz */
	{ 0, 1, 2, 7, 3, 1, 1, 0 },

	/* ARM L15: 100MHz */
	{ 0, 1, 2, 7, 3, 1, 1, 0 },
};

static unsigned int clkdiv_cpu1_4415[CPUFREQ_LEVEL_END][3] = {
	/* Clock divider value for following
	 * { DIVCOPY, DIVHPM }
	 */
	/* ARM L0: 1600MHz */
	{ 7, 7 },

	/* ARM L1: 1500MHz */
	{ 7, 7 },

	/* ARM L2: 1400MHz */
	{ 7, 7 },

	/* ARM L3: 1300MHz */
	{ 7, 7 },

	/* ARM L4: 1200MHz */
	{ 7, 7 },

	/* ARM L5: 1100MHz */
	{ 7, 7 },

	/* ARM L6: 1000MHz */
	{ 7, 7 },

	/* ARM L7: 900MHz */
	{ 7, 7 },

	/* ARM L8: 800MHz */
	{ 7, 7 },

	/* ARM L9: 700MHz */
	{ 7, 7 },

	/* ARM L10: 600MHz */
	{ 7, 7 },

	/* ARM L11: 500MHz */
	{ 7, 7 },

	/* ARM L12: 400MHz */
	{ 7, 7 },

	/* ARM L13: 300MHz */
	{ 7, 7 },

	/* ARM L14: 200MHz */
	{ 7, 7 },

	/* ARM L15: 100MHz */
	{ 7, 7 },
};

static unsigned int exynos4415_apll_pms_table[CPUFREQ_LEVEL_END] = {
	/* APLL FOUT L0: 1600MHz */
	((200<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L1: 1500MHz */
	((250<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L2: 1400MHz */
	((175<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L3: 1300MHz */
	((325<<16)|(6<<8)|(0x0)),

	/* APLL FOUT L4: 1200MHz */
	((400<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L5: 1100MHz */
	((275<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L6: 1000MHz */
	((250<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L7: 900MHz */
	((300<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L8: 800MHz */
	((200<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L9: 700MHz */
	((175<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L10: 600MHz */
	((400<<16)|(4<<8)|(0x2)),

	/* APLL FOUT L11: 500MHz */
	((250<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L12 400MHz */
	((200<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L13: 300MHz */
	((400<<16)|(4<<8)|(0x3)),

	/* APLL FOUT L14: 200MHz */
	((200<<16)|(3<<8)|(0x3)),

	/* APLL FOUT L15: 100MHz */
	((200<<16)|(3<<8)|(0x4)),

};

static int exynos4415_bus_table[CPUFREQ_LEVEL_END] = {
	543000,         /* 1.6GHz */
	543000,         /* 1.5GHz */
	543000,         /* 1.4GHz */
	413000,         /* 1.3GHz */
	413000,         /* 1.2GHz */
	413000,         /* 1.1GHz */
	413000,         /* 1.0GHz */
	413000,         /* 900MHz */
	413000,         /* 800MHz */
	275000,         /* 700MHz */
	206000,         /* 600MHz */
	138000,         /* 500MHz */
	138000,         /* 400MHz */
	103000,         /* 300MHz */
	103000,         /* 200MHz */
	103000,         /* 100MHz */
};

/* INT freq locking based on ARM Frequency */
static int exynos4415_int_bus_table[CPUFREQ_LEVEL_END] = {
	269000,	/* 1.6 GHz */
	268000,	/* 1.5 GHz */
	268000,	/* 1.4 GHz */
	268000,	/* 1.3 GHz */
	0,		/* 1.2 GHz */
	0,		/* 1.1 GHz */
	0,		/* 1.0 GHz */
	0,		/* 900 MHz */
	0,		/* 800 MHz */
	0,		/* 700 MHz */
	0,		/* 600 MHz */
	0,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
	0,		/* 200 MHz */
	0,		/* 100 MHz */
};

static int exynos4415_int_freq_lock_table[CPUFREQ_LEVEL_END] = {
	269000,	/* 1.6 GHz */
	0,		/* 1.5 GHz */
	0,		/* 1.4 GHz */
	0,		/* 1.3 GHz */
	0,		/* 1.2 GHz */
	0,		/* 1.1 GHz */
	0,		/* 1.0 GHz */
	0,		/* 900 MHz */
	0,		/* 800 MHz */
	0,		/* 700 MHz */
	0,		/* 600 MHz */
	0,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
	0,		/* 200 MHz */
	0,		/* 100 MHz */
};

static void exynos4415_set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;
	unsigned int stat_cpu1 = 0x11;

	/* Change Divider - CPU0 */

	tmp = exynos4415_clkdiv_table[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS4415_CLKDIV_CPU0);

	while (__raw_readl(EXYNOS4415_CLKDIV_STAT_CPU0) & 0x11111111)
		cpu_relax();

	/* Change Divider - CPU1 */
	tmp = exynos4415_clkdiv_table[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS4415_CLKDIV_CPU1);

	while (__raw_readl(EXYNOS4415_CLKDIV_STAT_CPU1) & stat_cpu1)
		cpu_relax();
}

static void exynos4415_set_apll(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

        /* 0. before change to MPLL, set div for MPLL output */
        if ((new_index > L8) && (old_index > L8))
                exynos4415_set_clkdiv(L8); /* pll_safe_idx */

	/* 1. MUX_CORE_SEL = MPLL, ARMCLK uses MPLL for lock time */
	clk_set_parent(moutcore, mout_mpll);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS4415_CLKMUX_STATCPU)
			>> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set APLL Lock time */
	pdiv = ((exynos4415_apll_pms_table[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 270), EXYNOS4415_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS4415_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos4415_apll_pms_table[new_index];
	__raw_writel(tmp, EXYNOS4415_APLL_CON0);

	/* 4. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS4415_APLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS4_APLLCON0_LOCKED_SHIFT)));

	/* 5. MUX_CORE_SEL = APLL */
	clk_set_parent(moutcore, mout_apll);

	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS4415_CLKMUX_STATCPU);
		tmp &= EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT));

       /* 6. restore original div value */
        if ((new_index > L8) && (old_index > L8))
                exynos4415_set_clkdiv(new_index);
}

bool exynos4415_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = exynos4415_apll_pms_table[old_index] >> 8;
	unsigned int new_pm = exynos4415_apll_pms_table[new_index] >> 8;

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos4415_set_ema(unsigned int target_volt)
{
	if (target_volt <= EMA_UP_VOLT)
		__raw_writel(0x404, EXYNOS4415_ARM_EMA_CTRL);
	else if (target_volt >= EMA_DOWN_VOLT)
		__raw_writel(0x101, EXYNOS4415_ARM_EMA_CTRL);
	else
		__raw_writel(0x303, EXYNOS4415_ARM_EMA_CTRL);
}

static void exynos4415_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_CLOCK_SWITCH_CHANGE,
		"[CPU] %7d <= %7d",
		exynos4415_freq_table[new_index].frequency,
		exynos4415_freq_table[old_index].frequency);

	if (old_index > new_index) {
		exynos4415_int_busfreq_voltage_constraint(exynos4415_int_bus_table[new_index]);

		/* Clock Configuration Procedure */
		/* 1. Change the system clock divider values */
		exynos4415_set_clkdiv(new_index);
		/* 2. Change the apll m,p,s value */
		exynos4415_set_apll(new_index, old_index);
	} else if (old_index < new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the apll m,p,s value */
		exynos4415_set_apll(new_index, old_index);
		/* 2. Change the system clock divider values */
		exynos4415_set_clkdiv(new_index);

		exynos4415_int_busfreq_voltage_constraint(exynos4415_int_bus_table[new_index]);
	}
}

extern int system_rev;

static void __init set_volt_table(void)
{
	unsigned int i;

	for (i = 0; i < CPUFREQ_LEVEL_END; i++) {
		exynos4415_volt_table[i] = get_match_volt(ID_ARM, exynos4415_freq_table[i].frequency);
		exynos4415_arm_abb_table[i] = get_match_abb(ID_ARM, exynos4415_freq_table[i].frequency);

		pr_info("CPUFREQ L%d : %d uV ABB : %d\n", i, exynos4415_volt_table[i], exynos4415_arm_abb_table[i]);
	}

	if (system_rev >= 4)
		max_support_idx = L1;	/* 1.5GHz Limit*/
	else
		max_support_idx = L4;	/* 1.2GHz Limit*/
	min_support_idx = L14;

	for (i = L0; i < max_support_idx; i++)
		exynos4415_freq_table[i].frequency = CPUFREQ_ENTRY_INVALID;

	for (i = min_support_idx + 1; i < CPUFREQ_LEVEL_END; i++)
		exynos4415_freq_table[i].frequency = CPUFREQ_ENTRY_INVALID;

}

int __init exynos4415_cpufreq_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table();

	cpu_clk = clk_get(NULL, "fout_apll");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	moutcore = clk_get(NULL, "moutcore");
	if (IS_ERR(moutcore))
		goto err_moutcore;

	mout_mpll = clk_get(NULL, "mout_mpll_user_cpu");
	if (IS_ERR(mout_mpll))
		goto err_mout_mpll;

	rate = clk_get_rate(mout_mpll) / 1000;

	mout_apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {

		exynos4415_clkdiv_table[i].index = i;

		tmp = __raw_readl(EXYNOS4415_CLKDIV_CPU0);

		tmp &= ~(EXYNOS4_CLKDIV_CPU0_CORE_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM0_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM1_MASK |
			EXYNOS4_CLKDIV_CPU0_PERIPH_MASK |
			EXYNOS4_CLKDIV_CPU0_ATB_MASK |
			EXYNOS4_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS4_CLKDIV_CPU0_APLL_MASK |
			EXYNOS4_CLKDIV_CPU0_CORE2_MASK);

		tmp |= ((clkdiv_cpu0_4415[i][0] << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT) |
			(clkdiv_cpu0_4415[i][1] << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT) |
			(clkdiv_cpu0_4415[i][2] << EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT) |
			(clkdiv_cpu0_4415[i][3] << EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT) |
			(clkdiv_cpu0_4415[i][4] << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT) |
			(clkdiv_cpu0_4415[i][5] << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT) |
			(clkdiv_cpu0_4415[i][6] << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT) |
			(clkdiv_cpu0_4415[i][7] << EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT));

		exynos4415_clkdiv_table[i].clkdiv = tmp;

		tmp = __raw_readl(EXYNOS4415_CLKDIV_CPU1);

		tmp &= ~(EXYNOS4_CLKDIV_CPU1_COPY_MASK |
			EXYNOS4_CLKDIV_CPU1_HPM_MASK);
		tmp |= ((clkdiv_cpu1_4415[i][0] << EXYNOS4_CLKDIV_CPU1_COPY_SHIFT) |
			(clkdiv_cpu1_4415[i][1] << EXYNOS4_CLKDIV_CPU1_HPM_SHIFT));

		exynos4415_clkdiv_table[i].clkdiv1 = tmp;
	}

	clk_set_rate(cpu_clk, 0);
	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L7;
	info->max_support_idx = max_support_idx;
	info->min_support_idx = min_support_idx;
	info->cpu_clk = cpu_clk;
	info->volt_table = exynos4415_volt_table;
	info->abb_table = exynos4415_arm_abb_table;
	info->freq_table = exynos4415_freq_table;
	info->bus_table = exynos4415_bus_table;
	info->int_bus_table = exynos4415_int_freq_lock_table;
	info->set_freq = exynos4415_set_frequency;
	info->need_apll_change = exynos4415_pms_change;
	info->set_ema = exynos4415_set_ema;

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
EXPORT_SYMBOL(exynos4415_cpufreq_init);
