/* linux/arch/arm/mach-exynos/include/mach/cpufreq.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/notifier.h>

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20, L21, L22, L23, L24,
};

struct exynos_dvfs_info {
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	unsigned int	pm_lock_idx;
	unsigned int	max_support_idx;
	unsigned int	min_support_idx;
	unsigned int	cluster_num;
	unsigned int	boot_freq;
	unsigned int	boot_cpu_min_qos;
	unsigned int	boot_cpu_max_qos;
	int		boot_freq_idx;
	int		*bus_table;
	int             *int_bus_table;
	bool		blocked;
	struct clk	*cpu_clk;
	unsigned int	*volt_table;
	unsigned int	*abb_table;
	const unsigned int	*max_op_freqs;
	struct cpufreq_frequency_table	*freq_table;
	struct regulator *regulator;
	void (*set_freq)(unsigned int, unsigned int);
	void (*set_ema)(unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
	bool (*is_alive)(void);
};

struct cpufreq_clkdiv {
	unsigned int    index;
	unsigned int    clkdiv;
	unsigned int    clkdiv1;
};

#if defined(CONFIG_ARM_EXYNOS3470_CPUFREQ)
#define EMA_VOLT		900000
/* EMA_UP_VALUE is target volt upper than 0.9V */
#define EMA_UP_VALUE		(0x3)
/* EMA_DOWN_VALUE is target volt lower than 0.9V */
#define EMA_DOWN_VALUE		(0x4)
#define EMA_HD_SHIFT		(3)
#define EMA_SHIFT		(0)
#define EMA_MASK		(0x7)
#define COLD_VOLT_OFFSET	50000
#endif

#if defined(CONFIG_ARM_EXYNOS4415_CPUFREQ)
#define EMA_VOLT		900000
#define COLD_VOLT_OFFSET	50000
#endif

#if defined(CONFIG_ARCH_EXYNOS3)
#if defined(CONFIG_ARM_EXYNOS3250_CPUFREQ)
#define COLD_VOLT_OFFSET        50000
#endif
extern int exynos3250_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos3472_cpufreq_init(struct exynos_dvfs_info *);
static inline int exynos3470_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}
static inline int exynos4210_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}
static inline int exynos4x12_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}
static inline int exynos4415_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}
static inline int exynos5250_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	return 0;
}

#elif defined(CONFIG_ARCH_EXYNOS4)
extern int exynos3470_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4210_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4x12_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4415_cpufreq_init(struct exynos_dvfs_info *);

static inline int exynos3472_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}
static inline int exynos3250_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5250_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	return 0;
}

#ifdef CONFIG_EXYNOS_DM_CPU_HOTPLUG
extern void dm_cpu_hotplug_init(void);
#endif

#elif defined(CONFIG_ARCH_EXYNOS5)
static inline int exynos4210_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos4x12_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

extern int exynos5250_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *);
extern int exynos5_cpufreq_CA15_init(struct exynos_dvfs_info *);
#else
	#warning "Should define CONFIG_ARCH_EXYNOS4(5)\n"
#endif
extern void exynos_thermal_throttle(void);
extern void exynos_thermal_unthrottle(void);

/* CPUFREQ init events */
#define CPUFREQ_INIT_COMPLETE	0x0001

#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern int exynos_cpufreq_init_register_notifier(struct notifier_block *nb);
extern int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb);
#else
static inline int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif

#if defined(CONFIG_ARCH_EXYNOS5)
/*
 * CPU usage threshold value to determine changing from b to L
 * Assumption: A15(500MHz min), A7(1GHz max) has almost same performance
 * in DMIPS. If a A15 is working at minimum DVFS level and current cpu usage
 * is less than b_to_L_threshold, try to change this A15 to A7.
 */
#define DMIPS_A15	35	/* 3.5 * 10 (factor) */
#define DMIPS_A7	19	/* 1.9 * 10 (factor) */

/*
 * Performance factor A15 vs A7 at same frequency
 * DMIPS_A15/DMIPS_A7 * 10 = 35 / 19 * 10
 */
#define PERF_FACTOR		18

#define b_to_L_threshold	80

typedef enum {
	CA7,
	CA15,
	CA_END,
} cluster_type;

#if defined(CONFIG_ARM_EXYNOS5260_CPUFREQ)
#define EMA_VAL_REG		(S5P_VA_CHIPID2 + 0x04)
#define EMA_CON1_SHIFT		24
#define EMA_CON2_SHIFT		27
#define EMA_CON_MASK		0x7
#define EMA_VAL_0		0x0
#define EMA_VAL_1		0x1
#define EMA_VAL_3		0x3
#define EMA_VAL_4		0x4
#define EMA_VOLT		900000
#define KFC_EMA_SHIFT		0
#define KFC_EMA_HD_SHIFT	4
#define EGL_L1_EMA_SHIFT	0
#define EGL_L2_EMA_SHIFT	12
#define COLD_VOLT_OFFSET        50000
#define ENABLE_MIN_COLD         1
#define LIMIT_COLD_VOLTAGE      1300000
#define MIN_COLD_VOLTAGE        950000
#define NR_CA7		4
#define NR_CA15		2
#elif defined(CONFIG_ARM_EXYNOS5420_CPUFREQ)
#define COLD_VOLT_OFFSET        37500
#define ENABLE_MIN_COLD         1
#define LIMIT_COLD_VOLTAGE      1250000
#define MIN_COLD_VOLTAGE        950000
#define NR_CA7		4
#define NR_CA15		4
#endif

enum op_state {
	NORMAL,		/* Operation : Normal */
	SUSPEND,	/* Direct API will be blocked in this state */
	RESUME,		/* Re-enabling DVFS using direct API after resume */
};

/*
 * Keep frequency value for counterpart cluster DVFS
 * cur, min, max : Frequency (KHz),
 * c_id : Counter cluster with booting cluster, if booting cluster is
 * A15, c_id will be A7.
 */
struct cpu_info_alter {
	unsigned int cur;
	unsigned int min;
	unsigned int max;
	cluster_type boot_cluster;
	cluster_type c_id;
};

extern cluster_type exynos_boot_cluster;
extern unsigned int exynos_cpufreq_direct_scale(unsigned int target_freq,
						unsigned int curr_freq,
						enum op_state state);
extern int exynos_init_bL_info(struct cpu_info_alter *info);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
extern void (*disable_c3_idle)(bool disable);
#endif

#if defined(CONFIG_ARM_EXYNOS_IKS_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern void exynos_lowpower_for_cluster(cluster_type cluster, bool on);
extern void reset_lpj_for_cluster(cluster_type cluster);
extern struct pm_qos_request max_cpu_qos_blank;
extern struct mutex cpufreq_lock;
#else
static inline void reset_lpj_for_cluster(cluster_type cluster) {}
static inline void exynos_lowpower_for_cluster(cluster_type cluster, bool on) {}
#endif
#if defined(CONFIG_SCHED_HMP) && defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG)
int big_cores_hotplug(bool out_flag);
void event_hotplug_in(void);
bool is_big_hotpluged(void);
#else
static inline int big_cores_hotplug(bool out_flag)
{
	return 0;
}

static inline void event_hotplug_in(void)
{
	return;
}

static inline bool is_big_hotpluged(void)
{
	return 0;
}
#endif

#endif
