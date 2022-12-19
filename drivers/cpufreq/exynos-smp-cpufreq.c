/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Jonghwan Choi <jhbird.choi@samsung.com>
 *
 * EXYNOS7580 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/exynos-ss.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/opp.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/types.h>

#include <plat/cpu.h>

#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos7580.h>
#include <mach/tmu.h>

/* Currently we support only two clusters */
#ifndef CONFIG_EXYNOS7580_QUAD
#define MAX_CLUSTERS		2
#else
#define MAX_CLUSTERS		1
#endif

#define DIV_MASK_ALL		0xffffffff
#define LIMIT_COLD_VOLTAGE	1350000
#define MIN_COLD_VOLTAGE	950000
#define COLD_VOLT_OFFSET	37500

#define APLL_FREQ(f, a0, a1, a2, a3, a4, a5, a6, b0, b1, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 16 | (a5) << 20 | (a6) << 24), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4), \
		.mps = ((m) << 12 | (p) << 4 | (s)), \
	}

/* This enum is also existed in mach/cpufreq.h */
typedef enum {
	CL_ZERO,
#ifndef CONFIG_EXYNOS7580_QUAD
	CL_ONE,
#endif
	CL_END,
} cluster_type;

static struct pm_qos_request pm_qos_mif;
static struct pm_qos_request cluster_qos_min[CL_END];
static struct pm_qos_request cluster_qos_max[CL_END];
static struct pm_qos_request boost_qos_min[CL_END];
#ifdef CONFIG_SW_SELF_DISCHARGING
static int self_discharging;
#endif

/* Include CPU mask of each cluster */
cluster_type exynos_boot_cluster;

static struct {
	unsigned long freq;
	u32 clk_div_cpu0;
	u32 clk_div_cpu1;
	u32 mps;
} apll_freq[] = {
	/*
	 * values:
	 * freq
	 * clock divider for CPU1, CPU2, ATCLK, PCLK_DBG_CP, ACLK_CPU, PCLK_CPU, SCLK_CNTCLK
	 * clock divider for SCLK_CPU_PLL, SCLK_HPM_CPU
	 * PLL M, P, S
	 */
	APLL_FREQ(1600000, 0, 0, 7, 7, 2, 7, 3, 7, 7, 246, 4, 0),
	APLL_FREQ(1500000, 0, 0, 7, 7, 2, 7, 3, 7, 7, 230, 4, 0),
	APLL_FREQ(1400000, 0, 0, 7, 7, 2, 7, 3, 7, 7, 216, 4, 0),
	APLL_FREQ(1300000, 0, 0, 7, 7, 2, 7, 3, 6, 7, 200, 4, 0),
	APLL_FREQ(1200000, 0, 0, 7, 7, 2, 7, 3, 6, 7, 368, 4, 1),
	APLL_FREQ(1100000, 0, 0, 7, 7, 2, 7, 3, 5, 7, 340, 4, 1),
	APLL_FREQ(1000000, 0, 0, 7, 7, 2, 7, 3, 5, 7, 308, 4, 1),
	APLL_FREQ(900000,  0, 0, 7, 7, 2, 7, 3, 4, 7, 276, 4, 1),
	APLL_FREQ(800000,  0, 0, 7, 7, 2, 7, 3, 4, 7, 248, 4, 1),
	APLL_FREQ(700000,  0, 0, 7, 7, 2, 7, 3, 3, 7, 216, 4, 1),
	APLL_FREQ(600000,  0, 0, 7, 7, 2, 7, 3, 3, 7, 368, 4, 2),
	APLL_FREQ(500000,  0, 0, 7, 7, 2, 7, 3, 2, 7, 312, 4, 2),
	APLL_FREQ(400000,  0, 0, 7, 7, 2, 7, 3, 2, 7, 248, 4, 2),
	APLL_FREQ(300000,  0, 0, 7, 7, 2, 7, 3, 1, 7, 368, 4, 3),
};

static unsigned int exynos_bus_table[] = {
	825000, /* 1.6GHz */
	825000, /* 1.5GHz */
	825000, /* 1.4GHz */
	825000, /* 1.3GHz */
	728000, /* 1.2GHz */
	728000, /* 1.1GHz */
	667000, /* 1.0GHz */
	559000, /* 900MHz */
	559000, /* 800MHz */
	416000, /* 700MHz */
	416000, /* 600MHz */
	416000, /* 500MHz */
	0,	/* 400MHz */
	0,	/* 300MHz */
};

static unsigned int voltage_tolerance;	/* in percentage */
static DEFINE_MUTEX(exynos_cpu_lock);
static bool is_suspended;
static unsigned int sync_frequency;
static unsigned int locking_frequency;
static unsigned int locking_volt;
static unsigned int cold_offset;

static struct clk *clk[MAX_CLUSTERS];
static struct clk *mux[MAX_CLUSTERS];
static struct clk *alt[MAX_CLUSTERS];
static struct regulator *reg[MAX_CLUSTERS];
static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS];
#ifndef CONFIG_EXYNOS7580_QUAD
static atomic_t cluster_usage[MAX_CLUSTERS] = {ATOMIC_INIT(0), ATOMIC_INIT(0)};
#else
static atomic_t cluster_usage[MAX_CLUSTERS] = {ATOMIC_INIT(0)};
#endif
static unsigned int alt_freq[MAX_CLUSTERS];
static const unsigned int boost_freq = 1300000; /* KHz */
#ifndef CONFIG_EXYNOS7580_QUAD
static unsigned int maxlock_freq;
#endif

#ifndef CONFIG_EXYNOS7580_QUAD
static const char *cpu_mux[MAX_CLUSTERS] = {"mout_cpu", "mout_apl"};
static const char *alt_pat[MAX_CLUSTERS] = {"mout_bus_pll_cpu_user", "mout_bus_pll_apl_user"};
#else
static const char *cpu_mux[MAX_CLUSTERS] = {"mout_cpu"};
static const char *alt_pat[MAX_CLUSTERS] = {"mout_bus_pll_cpu_user"};
#endif

static inline int cpu_to_cluster(int cpu)
{
	return topology_physical_package_id(cpu);
}

static bool support_full_frequency(void)
{
	unsigned int data;

	data = __raw_readl(S5P_VA_CHIPID + 0x14);

	/* Check the magic number */
	if ((data & 0xffffff) == 0x16e493)
		return false;

	return true;
}

static unsigned int exynos_cpufreq_get_cluster(unsigned int cluster)
{
	unsigned int freq = clk_get_rate(clk[cluster]) / 1000;

	freq += 50000;
	freq = (freq / 100000) * 100000;

	return freq;
}

static unsigned int exynos_cpufreq_get(unsigned int cpu)
{
	u32 cur_cluster = cpu_to_cluster(cpu);

	return exynos_cpufreq_get_cluster(cur_cluster);
}

/* Validate policy frequency range */
static int exynos_cpufreq_verify_policy(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);

	return cpufreq_frequency_table_verify(policy, freq_table[cur_cluster]);
}

static void exynos_cpufreq_boost_frequency(int cpu, unsigned int timeout_ms)
{
	unsigned int booting_freq;
	cluster_type target_cluster;

#ifndef CONFIG_EXYNOS7580_QUAD
	target_cluster = (cpu < 4) ? (CL_ZERO) : (CL_ONE);
#else
	target_cluster = (CL_ZERO);
#endif

	if (!support_full_frequency())
		booting_freq = boost_freq;
	else
		booting_freq = boost_freq + 200000;

	if (timeout_ms)
		pm_qos_update_request_timeout(&boost_qos_min[target_cluster], booting_freq, timeout_ms * 1000);
	else
		pm_qos_update_request(&boost_qos_min[target_cluster], booting_freq);
}

static unsigned int exynos_get_safe_armvolt(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;
	struct opp *opp;
	u32 cluster;

	cpu_dev = get_cpu_device(policy->cpu);
	cluster = cpu_to_cluster(policy->cpu);
	opp = opp_find_freq_exact(cpu_dev, alt_freq[cluster] * 1000, true);

	return opp_get_voltage(opp);
}

static void wait_until_divider_stable(void __iomem *div_reg, unsigned long mask)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

	do {
		if (!(__raw_readl(div_reg) & mask))
			return;
	} while (time_before(jiffies, timeout));

	pr_err("%s: timeout in divider stablization\n", __func__);
}

static int exynos_cpufreq_get_index(int cluster, unsigned long freq)
{
	int index;

	freq = freq / 1000;

	for (index = 0;
			freq_table[cluster][index].frequency != CPUFREQ_TABLE_END; index++) {
		if (freq_table[cluster][index].frequency == freq)
			break;
	}

	if (freq_table[cluster][index].frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return index;
}

static void exynos_apll_set_clkdiv(int cluster, int div_index)
{
	unsigned int div;

	/* Change Divider - CPU0 */
	div = apll_freq[div_index].clk_div_cpu0;

	__raw_writel(div, cluster ? EXYNOS7580_DIV_APL_0 : EXYNOS7580_DIV_CPU_0);

	wait_until_divider_stable(cluster ? EXYNOS7580_DIV_STAT_APL_0 : EXYNOS7580_DIV_STAT_CPU_0, DIV_MASK_ALL);

	/* Change Divider - CPU1 */
	div = apll_freq[div_index].clk_div_cpu1;

	__raw_writel(div, cluster ? EXYNOS7580_DIV_APL_1 : EXYNOS7580_DIV_CPU_1);

	wait_until_divider_stable(cluster ? EXYNOS7580_DIV_STAT_APL_1 : EXYNOS7580_DIV_STAT_CPU_1, DIV_MASK_ALL);
}

static int exynos_apll_set_pms(int cluster, unsigned long freq)
{
	int ret;

	clk_set_parent(mux[cluster], alt[cluster]);

	ret = clk_set_rate(clk[cluster], freq);
	if (ret)
		pr_err("clk_set_rate failed: %d\n", ret);

	clk_set_parent(mux[cluster], clk[cluster]);

	return ret;
}

static void exynos_request_mif(void)
{
	unsigned long freq;
	int index;

	/* Get the max freq from cluter0/1 */
#ifndef CONFIG_EXYNOS7580_QUAD
	freq = max(cpufreq_quick_get(0), cpufreq_quick_get(4));
#else
	freq = cpufreq_quick_get(0);
#endif
	/* Cluster 0/1 have a same freq table */
	index = exynos_cpufreq_get_index(0, freq * 1000);
	if (index >= 0)
		pm_qos_update_request(&pm_qos_mif, exynos_bus_table[index]);
}

static int exynos_set_rate(int cluster, unsigned long freq, bool up)
{
	int ret;
	int index;

	index = exynos_cpufreq_get_index(cluster, freq);
	if (index < 0)
		return -EINVAL;

	if (up) {
		exynos_apll_set_clkdiv(cluster, index);
		ret = exynos_apll_set_pms(cluster, freq);
	} else {
		ret = exynos_apll_set_pms(cluster, freq);
		exynos_apll_set_clkdiv(cluster, index);
	}

	return ret;
}

static int exynos_regulator_set_voltage(int cluster, unsigned long volt)
{
	unsigned long target;

	if (volt > LIMIT_COLD_VOLTAGE)
		target = volt;
	else if (volt + cold_offset > LIMIT_COLD_VOLTAGE)
		target = LIMIT_COLD_VOLTAGE;
	else if (cold_offset && ((volt + cold_offset) < MIN_COLD_VOLTAGE))
		target = MIN_COLD_VOLTAGE;
	else
		target = volt + cold_offset;

	/* 6250(BUCK STEP valye) value depends on pmic */
	return regulator_set_voltage(reg[cluster], target, target + 6250);
}

/* Set clock frequency */
static int exynos_cpufreq_scale(struct cpufreq_policy *policy,
		unsigned int target_freq)
{
	struct cpufreq_freqs freqs;
	struct device *cpu_dev;
	struct opp *opp;
	u32 cur_cluster;
	unsigned long volt, safe_volt = 0;
	int ret = 0;

	freqs.old = exynos_cpufreq_get(policy->cpu);
	freqs.new = target_freq;

	if (freqs.new == freqs.old)
		return 0;

	cur_cluster = cpu_to_cluster(policy->cpu);

	if (freqs.old < alt_freq[cur_cluster] && freqs.new < alt_freq[cur_cluster])
		safe_volt = exynos_get_safe_armvolt(policy);

	cpu_dev = get_cpu_device(policy->cpu);
	opp = opp_find_freq_exact(cpu_dev, freqs.new * 1000, true);
	if (IS_ERR(opp)) {
		pr_err("failed to find OPP for %u\n", freqs.new * 1000);
		return PTR_ERR(opp);
	}

	volt = opp_get_voltage(opp);
	if ((freqs.new > freqs.old) && !safe_volt) {
		ret = exynos_regulator_set_voltage(cur_cluster, volt);
		if (ret) {
			pr_err("failed to scale voltage up : %d\n", ret);
			goto out;
		}
		set_match_abb(cur_cluster, get_match_abb(cur_cluster, freqs.new * 1000));
	} else if (safe_volt) {
		ret = exynos_regulator_set_voltage(cur_cluster, safe_volt);
		if (ret) {
			pr_err("failed to scale voltage up : %d\n", ret);
			goto out;
		}
		set_match_abb(cur_cluster, get_match_abb(cur_cluster, freqs.new * 1000));
	}

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	exynos_ss_freq(cur_cluster, freqs.old, ESS_FLAG_IN);

	ret = exynos_set_rate(cur_cluster, freqs.new * 1000, freqs.new > freqs.old);
	if (ret) {
		pr_err("exynos_set_rate failed: %d\n", ret);
		freqs.new = freqs.old;
	}

	exynos_ss_freq(cur_cluster, freqs.new, ESS_FLAG_OUT);

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	if ((freqs.new < freqs.old) ||
	    ((freqs.new > freqs.old) && safe_volt)) {
		set_match_abb(cur_cluster, get_match_abb(cur_cluster, freqs.new * 1000));
		ret = exynos_regulator_set_voltage(cur_cluster, volt);
		if (ret)
			pr_err("failed to scale voltage down : %d\n", ret);
	}

	exynos_request_mif();
out:
	return ret;
}

static unsigned int exynos_verify_pm_qos_limit(int cluster, unsigned int freq)
{
	unsigned int target_freq;
	int pm_qos_class_min = PM_QOS_CLUSTER0_FREQ_MIN;
	int pm_qos_class_max = PM_QOS_CLUSTER0_FREQ_MAX;

#ifndef CONFIG_EXYNOS7580_QUAD
	if (cluster == CL_ONE) {
		pm_qos_class_min = PM_QOS_CLUSTER1_FREQ_MIN;
		pm_qos_class_max = PM_QOS_CLUSTER1_FREQ_MAX;
	}
#endif

	target_freq = max((unsigned int)pm_qos_request(pm_qos_class_min), freq);
	target_freq = min((unsigned int)pm_qos_request(pm_qos_class_max), target_freq);

#ifndef CONFIG_EXYNOS7580_QUAD
	/* If cluster1 is turned on, first freq should be higher than cluster 0 */
	if (sync_frequency && (cluster == CL_ONE)) {
		target_freq = max(target_freq, sync_frequency);
		sync_frequency = 0;
	}
#endif

	return target_freq;
}

/* Set clock frequency */
static int exynos_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int target_freq, unsigned int relation)
{
	u32 cpu = policy->cpu, freq_tab_idx, cur_cluster;
	unsigned int freq;
	int ret = 0;

	mutex_lock(&exynos_cpu_lock);

	if (is_suspended)
		goto out;

	if (target_freq == 0)
		target_freq = policy->min;

	/* if PLL bypass, frequency scale is skip */
	if (exynos_cpufreq_get(cpu) <= 26000)
		goto out;

	cur_cluster = cpu_to_cluster(policy->cpu);

	target_freq = exynos_verify_pm_qos_limit(cur_cluster, target_freq);

	/* Determine valid target frequency using freq_table */
	ret = cpufreq_frequency_table_target(policy, freq_table[cur_cluster],
			target_freq, relation, &freq_tab_idx);
	if (ret) {
		pr_err("failed to match target freqency %d: %d\n",
			target_freq, ret);
		goto out;
	}

	freq = freq_table[cur_cluster][freq_tab_idx].frequency;

	pr_debug("%s: cpu: %d, cluster: %d, target freq: %d, new freq: %d\n",
			__func__, cpu, cur_cluster, target_freq, freq);

	ret = exynos_cpufreq_scale(policy, freq);

out:
	mutex_unlock(&exynos_cpu_lock);
	return ret;
}

static void put_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	dev_dbg(cpu_dev, "%s: cluster: %d\n", __func__, cluster);
}

/* get cpu node with valid operating-points */
static struct device_node *get_cpu_node_with_valid_op(int cpu)
{
	struct device_node *np = NULL, *parent;
	int count = 0;

	parent = of_find_node_by_path("/cpus");
	if (!parent) {
		pr_err("failed to find OF /cpus\n");
		return NULL;
	}

	for_each_child_of_node(parent, np) {
		if (count++ != cpu)
			continue;
		if (!of_get_property(np, "operating-points", NULL)) {
			of_node_put(np);
			np = NULL;
		}

		break;
	}

	of_node_put(parent);
	return np;
}

/**
 * exynos_of_init_opp_table() - Initialize opp table from device tree
 * @dev:	device pointer used to lookup device OPPs.
 *
 * Register the initial OPP table with the OPP library for given device.
 */
static int exynos_of_init_opp_table(struct device *dev)
{
	u32 cluster = cpu_to_cluster(dev->id);
	const struct property *prop;
	const __be32 *val;
	enum asv_type_id asv_id;
	int nr;

	prop = of_find_property(dev->of_node, "operating-points", NULL);
	if (!prop)
		return -ENODEV;
	if (!prop->value)
		return -ENODATA;

	/*
	 * Each OPP is a set of tuples consisting of frequency and
	 * voltage like <freq-kHz vol-uV>.
	 */
	nr = prop->length / sizeof(u32);
	if (nr % 2) {
		dev_err(dev, "%s: Invalid OPP list\n", __func__);
		return -EINVAL;
	}

	val = prop->value;
	while (nr) {
		unsigned long freq = be32_to_cpup(val++) * 1000;
		unsigned long volt = be32_to_cpup(val++);
		unsigned long temp;

		asv_id = cluster;
		temp = get_match_volt(asv_id, freq);
		if (temp)
			volt = temp;

		if (opp_add_dec(dev, freq, volt))
			dev_warn(dev, "%s: Failed to add OPP %ld\n",
				 __func__, freq);
		nr -= 2;
	}

	return 0;
}

static int exynos_init_opp_table(struct device *cpu_dev)
{
	struct device_node *np;
	int ret;

	np = get_cpu_node_with_valid_op(cpu_dev->id);
	if (!np)
		return -ENODATA;

	cpu_dev->of_node = np;
	ret = exynos_of_init_opp_table(cpu_dev);
	of_node_put(np);

	return ret;
}

static int get_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	char name[14] = "cpu-cluster.";
	int ret;

	if (atomic_inc_return(&cluster_usage[cluster]) != 1)
		return 0;

	ret = exynos_init_opp_table(cpu_dev);
	if (ret) {
		dev_err(cpu_dev, "%s: init_opp_table failed, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		goto atomic_dec;
	}

	ret = opp_init_cpufreq_table(cpu_dev, &freq_table[cluster]);
	if (ret) {
		dev_err(cpu_dev, "%s: failed to init cpufreq table, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		goto atomic_dec;
	}
	name[12] = cluster + '0';
	reg[cluster] = devm_regulator_get(cpu_dev, name);
	if (IS_ERR(reg[cluster])) {
		dev_err(cpu_dev, "%s: failed to get regulator, cluster: %d\n",
				__func__, cluster);
		goto opp_free;
	}

	mux[cluster] = devm_clk_get(cpu_dev, cpu_mux[cluster]);
	if (IS_ERR(mux[cluster])) {
		dev_err(cpu_dev, "%s: failed to get clk for cpu mux, cluster: %d\n",
				__func__, cluster);
		goto opp_free;
	}

	alt[cluster] = devm_clk_get(cpu_dev, alt_pat[cluster]);
	if (IS_ERR(alt[cluster])) {
		dev_err(cpu_dev, "%s: failed to get clk for alt parent, cluster: %d\n",
				__func__, cluster);
		goto opp_free;
	}

	alt_freq[cluster] = clk_get_rate(alt[cluster]) / 1000;

	clk[cluster] = devm_clk_get(cpu_dev, name);
	if (!IS_ERR(clk[cluster])) {
		dev_dbg(cpu_dev, "%s: clk: %p & freq table: %p, cluster: %d\n",
				__func__, clk[cluster], freq_table[cluster],
				cluster);
		return 0;
	}

	dev_err(cpu_dev, "%s: Failed to get clk for cpu: %d, cluster: %d\n",
			__func__, cpu_dev->id, cluster);
	ret = PTR_ERR(clk[cluster]);

opp_free:
	opp_free_cpufreq_table(cpu_dev, &freq_table[cluster]);

atomic_dec:
	atomic_dec(&cluster_usage[cluster]);
	dev_err(cpu_dev, "%s: Failed to get data for cluster: %d\n", __func__,
			cluster);
	return ret;
}

static int exynos_get_transition_latency(struct device *cpu_dev)
{
	struct device_node *np;
	u32 transition_latency = CPUFREQ_ETERNAL;

	np = get_cpu_node_with_valid_op(cpu_dev->id);
	if (!np)
		return CPUFREQ_ETERNAL;

	of_property_read_u32(np, "clock-latency", &transition_latency);
	of_node_put(np);

	pr_debug("%s: clock-latency: %d\n", __func__, transition_latency);
	return transition_latency;
}

static int exynos_get_voltage_tolerance(struct device *cpu_dev)
{
	struct device_node *np;
	u32 voltage_tolerance = 0;

	np = get_cpu_node_with_valid_op(cpu_dev->id);
	if (!np)
		return voltage_tolerance;

	of_property_read_u32(np, "voltage-torelance", &voltage_tolerance);
	of_node_put(np);

	pr_debug("%s: voltage-torelance: %d\n", __func__, voltage_tolerance);
	return voltage_tolerance;
}

static int exynos_pm_notify(struct notifier_block *nb, unsigned long event,
		void *dummy)
{
	int i, ret;

	if (event == PM_SUSPEND_PREPARE) {
		mutex_lock(&exynos_cpu_lock);
		is_suspended = true;
		mutex_unlock(&exynos_cpu_lock);

		for (i = 0; i < MAX_CLUSTERS; i++) {
			if (locking_frequency > exynos_cpufreq_get_cluster(i)) {
				ret = exynos_regulator_set_voltage(i, locking_volt);
				if (ret < 0) {
					pr_err("%s: Exynos cpufreq suspend: setting voltage to %d\n",
							__func__, locking_volt);
					mutex_lock(&exynos_cpu_lock);
					is_suspended = false;
					mutex_unlock(&exynos_cpu_lock);

					return NOTIFY_BAD;
				}
			}

		}

	} else if (event == PM_POST_SUSPEND) {
		mutex_lock(&exynos_cpu_lock);
		is_suspended = false;
		mutex_unlock(&exynos_cpu_lock);
	}

	return NOTIFY_OK;
}


#ifndef CONFIG_EXYNOS7580_QUAD
static int __cpuinit exynos_cpufreq_cpu_up_notifier(struct notifier_block *notifier,
                                        unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct cpumask mask;
	int cluster;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_ONLINE:
			cluster = cpu_to_cluster(cpu);
			if (cluster == CL_ONE) {
				cpumask_and(&mask, cpu_coregroup_mask(cpu), cpu_online_mask);
				if (cpumask_weight(&mask) == 1)
					pm_qos_update_request(&cluster_qos_max[CL_ONE], maxlock_freq);
			}
			break;
		}
	}

	return NOTIFY_OK;
}

static int __cpuinit exynos_cpufreq_cpu_down_notifier(struct notifier_block *notifier,
		unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct cpumask mask;
	int cluster;

	if (is_suspended)
		return NOTIFY_OK;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_DOWN_PREPARE:
			cluster = cpu_to_cluster(cpu);
			if (cluster == CL_ONE) {
				cpumask_and(&mask, cpu_coregroup_mask(cpu), cpu_online_mask);
				if (cpumask_weight(&mask) == 1)
					pm_qos_update_request(&cluster_qos_max[CL_ONE], apll_freq[ARRAY_SIZE(apll_freq) - 2].freq / 1000);
			}
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata exynos_cpufreq_cpu_up_nb = {
        .notifier_call = exynos_cpufreq_cpu_up_notifier,
        .priority = INT_MIN,
};

static struct notifier_block __refdata exynos_cpufreq_cpu_down_nb = {
        .notifier_call = exynos_cpufreq_cpu_down_notifier,
        .priority = INT_MAX,
};
#endif

static struct notifier_block exynos_cpu_pm_notifier = {
	.notifier_call = exynos_pm_notify,
	.priority = -1,
};

/* reboot notifier */
static int exynos_reboot_notify(struct notifier_block *nb, unsigned long event,
		void *dummy)
{
	int i, ret;

	mutex_lock(&exynos_cpu_lock);
	is_suspended = true;
	mutex_unlock(&exynos_cpu_lock);

	for (i = 0; i < MAX_CLUSTERS; i++) {
		if (locking_frequency > exynos_cpufreq_get_cluster(i)) {
			ret = exynos_regulator_set_voltage(i, locking_volt);
			if (ret < 0) {
				pr_err("%s: Exynos cpufreq reboot: setting voltage to %d\n",
						__func__, locking_volt);
				mutex_lock(&exynos_cpu_lock);
				is_suspended = false;
				mutex_unlock(&exynos_cpu_lock);

				return NOTIFY_BAD;
			}
		}

	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_reboot_notifier = {
	.notifier_call = exynos_reboot_notify,
};

static int exynos_cpufreq_tmu_notifier(struct notifier_block *notifier,
				unsigned long event, void *v)
{
	struct device *cpu_dev;
	struct opp *opp;
	unsigned long freq, volt;
	int i, ret = NOTIFY_OK;
	unsigned int *on = v;

	if (event != TMU_COLD)
		return NOTIFY_OK;

	mutex_lock(&exynos_cpu_lock);

	if (is_suspended)
		goto out;

	if (*on)
		cold_offset = COLD_VOLT_OFFSET;
	else
		cold_offset = 0;

	for (i = 0; i < MAX_CLUSTERS; i++) {
		freq = exynos_cpufreq_get_cluster(i);
		cpu_dev = get_cpu_device(0);
		opp = opp_find_freq_exact(cpu_dev, freq * 1000, true);
		volt = opp_get_voltage(opp);
		ret = exynos_regulator_set_voltage(i, volt);
		if (ret) {
			mutex_unlock(&exynos_cpu_lock);
			return NOTIFY_BAD;
		}
	}

out:
	mutex_unlock(&exynos_cpu_lock);

	return NOTIFY_OK;
}

static struct notifier_block exynos_tmu_nb = {
	.notifier_call = exynos_cpufreq_tmu_notifier,
};

/* Per-CPU initialization */
static int exynos_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	ret = get_cluster_clk_and_freq_table(cpu_dev);
	if (ret)
		return ret;

	ret = cpufreq_frequency_table_cpuinfo(policy, freq_table[cur_cluster]);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
				policy->cpu, cur_cluster);
		put_cluster_clk_and_freq_table(cpu_dev);
		return ret;
	}

	cpufreq_frequency_table_get_attr(freq_table[cur_cluster], policy->cpu);

	policy->cpuinfo.transition_latency = exynos_get_transition_latency(cpu_dev);
	voltage_tolerance = exynos_get_voltage_tolerance(cpu_dev);
	policy->cur = exynos_cpufreq_get(policy->cpu);
	/* Later this code will be removed. This is for first lot */
	policy->cpuinfo.min_freq = 400000;
	freq_table[cur_cluster][13].frequency = CPUFREQ_ENTRY_INVALID;

	if (samsung_rev() == EXYNOS7580_REV_0) {
		if (!support_full_frequency())
			policy->cpuinfo.max_freq = 800000;
		else
			policy->cpuinfo.max_freq = 1400000;
	} else if (soc_is_exynos7580_v1()) {
		policy->cpuinfo.max_freq = 1500000;
		freq_table[cur_cluster][0].frequency = CPUFREQ_ENTRY_INVALID;
	}

	if (soc_is_exynos7580_v1())
		policy->cpuinfo.max_freq = 1500000;

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));

	if (policy->cpu == 0) {
		exynos_boot_cluster = cpu_to_cluster(0);
		locking_frequency = exynos_cpufreq_get(0);
		register_pm_notifier(&exynos_cpu_pm_notifier);
		register_reboot_notifier(&exynos_cpu_reboot_notifier);
	} else {
		sync_frequency = exynos_cpufreq_get(0);
	}

	dev_info(cpu_dev, "%s: CPU %d initialized\n", __func__, policy->cpu);
	return 0;
}

static int exynos_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	put_cluster_clk_and_freq_table(cpu_dev);
	dev_dbg(cpu_dev, "%s: Exited, cpu: %d\n", __func__, policy->cpu);

	return 0;
}

/* Export freq_table to sysfs */
static struct freq_attr *exynos_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static void exynos_qos_nop(void *info)
{
}

static ssize_t show_cpufreq_table(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	unsigned int i;
	ssize_t count = 0;
	struct cpufreq_frequency_table *table = freq_table[0];

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		count += sprintf(&buf[count], "%d ", table[i].frequency);
	}
	count += sprintf(&buf[count], "\n");

	return count;
}

static ssize_t show_cpufreq_min_limit(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	int len;

	len = sprintf(buf, "%u\n", pm_qos_request(PM_QOS_CLUSTER0_FREQ_MIN));
#ifndef CONFIG_EXYNOS7580_QUAD
	len += sprintf(buf + len, "%u\n", pm_qos_request(PM_QOS_CLUSTER1_FREQ_MIN));
#endif

	return len;
}

static ssize_t store_cpufreq_min_limit(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t n)
{
	int i;
	int ret, freq;

	ret = sscanf(buf, "%d", &freq);
	if (ret != 1)
		return -EINVAL;

	if (freq < 0)
		freq = 0;

#ifdef CONFIG_SW_SELF_DISCHARGING
	if (freq < self_discharging) {
		freq = self_discharging;
	}
#endif

	for (i = 0; i < CL_END; i++)
		pm_qos_update_request(&cluster_qos_min[i], freq);

	return n;
}

static ssize_t show_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	int len;

	len = sprintf(buf, "%u\n", pm_qos_request(PM_QOS_CLUSTER0_FREQ_MAX));
#ifndef CONFIG_EXYNOS7580_QUAD
	len += sprintf(buf + len, "%u\n", pm_qos_request(PM_QOS_CLUSTER1_FREQ_MAX));
#endif

	return len;
}

static ssize_t store_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t n)
{
	int i;
	int ret, freq;
	int index = 0;

	ret = sscanf(buf, "%d", &freq);
	if (ret != 1)
		return -EINVAL;

	if (soc_is_exynos7580_v1())
		index = 1;

	if (freq < 0)
		freq = apll_freq[index].freq / 1000;

	for (i = 0; i < CL_END; i++)
		pm_qos_update_request(&cluster_qos_max[i], freq);


#ifndef CONFIG_EXYNOS7580_QUAD
	maxlock_freq = freq;
#endif
	return n;
}

#ifdef CONFIG_SW_SELF_DISCHARGING
static ssize_t show_cpufreq_self_discharging(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", self_discharging);
}

static ssize_t store_cpufreq_self_discharging(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	int input;
	int i;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0) {
		self_discharging = input;
		cpu_idle_poll_ctrl(true);
	}
	else {
		self_discharging = 0;
		cpu_idle_poll_ctrl(false);
	}

	/* Isla Quad(A53 quad) need cpufreq min limit */
	for (i = 0; i < CL_END; i++) {
		pm_qos_update_request(&cluster_qos_min[i], self_discharging);
	}

	return count;
}
#endif

define_one_global_ro(cpufreq_table);
define_one_global_rw(cpufreq_min_limit);
define_one_global_rw(cpufreq_max_limit);
#ifdef CONFIG_SW_SELF_DISCHARGING
define_one_global_rw(cpufreq_self_discharging);
#endif

static struct attribute * g[] = {
        &cpufreq_table.attr,
        &cpufreq_min_limit.attr,
        &cpufreq_max_limit.attr,
#ifdef CONFIG_SW_SELF_DISCHARGING
        &cpufreq_self_discharging.attr,
#endif
        NULL,
};

static struct attribute_group attr_group = {
        .attrs = g,
};

extern void (*disable_c3_idle)(bool disable);
static int exynos_min_cluster0_notifier(struct notifier_block *notifier,
				       unsigned long val, void *v)
{
	struct cpufreq_policy *policy;
	int ret;
	unsigned int freq;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return NOTIFY_BAD;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	freq = exynos_cpufreq_get(0);

	if (freq >= val)
		goto out;

	freq = val;

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(0, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, freq, 0);

	if (disable_c3_idle)
		disable_c3_idle(false);

	if (ret < 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

out:
	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}

#ifndef CONFIG_EXYNOS7580_QUAD
static int exynos_min_cluster1_notifier(struct notifier_block *notifier,
				       unsigned long val, void *v)
{
	struct cpufreq_policy *policy;
	int ret;
	unsigned int freq;

	policy = cpufreq_cpu_get(4);
	if (!policy)
		return NOTIFY_BAD;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	freq = exynos_cpufreq_get(4);

	if (freq >= val)
		goto out;

	freq = val;

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(4, exynos_qos_nop, NULL, 0);

	if (disable_c3_idle)
		disable_c3_idle(false);

	ret = __cpufreq_driver_target(policy, freq, 0);
	if (ret < 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

out:
	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}
#endif

static struct notifier_block exynos_min_cluster0_nb = {
	.notifier_call = exynos_min_cluster0_notifier,
};

#ifndef CONFIG_EXYNOS7580_QUAD
static struct notifier_block exynos_min_cluster1_nb = {
	.notifier_call = exynos_min_cluster1_notifier,
};
#endif

static int exynos_max_cluster0_notifier(struct notifier_block *notifier,
				       unsigned long val, void *v)
{
	struct cpufreq_policy *policy;
	int ret;
	unsigned int freq;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return NOTIFY_BAD;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	freq = exynos_cpufreq_get(0);

	if (freq <= val)
		goto out;

	freq = val;

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(0, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, freq, 0);

	if (disable_c3_idle)
		disable_c3_idle(false);

	if (ret < 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

out:
	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}

#ifndef CONFIG_EXYNOS7580_QUAD
static int exynos_max_cluster1_notifier(struct notifier_block *notifier,
				       unsigned long val, void *v)
{
	struct cpufreq_policy *policy;
	int ret;
	unsigned int freq;

	policy = cpufreq_cpu_get(4);
	if (!policy)
		return NOTIFY_BAD;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	freq = exynos_cpufreq_get(4);

	if (freq <= val)
		goto out;

	freq = val;

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(4, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, freq, 0);

	if (disable_c3_idle)
		disable_c3_idle(false);

	if (ret < 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

out:
	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}
#endif

static struct notifier_block exynos_max_cluster0_nb = {
	.notifier_call = exynos_max_cluster0_notifier,
};

#ifndef CONFIG_EXYNOS7580_QUAD
static struct notifier_block exynos_max_cluster1_nb = {
	.notifier_call = exynos_max_cluster1_notifier,
};
#endif

static struct cpufreq_driver exynos_cpufreq_driver = {
	.name			= "smp-cpufreq",
	.flags			= CPUFREQ_STICKY,
	.verify			= exynos_cpufreq_verify_policy,
	.target			= exynos_cpufreq_set_target,
	.get			= exynos_cpufreq_get,
	.init			= exynos_cpufreq_init,
	.exit			= exynos_cpufreq_exit,
	.have_governor_per_policy = true,
	.attr			= exynos_cpufreq_attr,
};

static struct platform_device_info devinfo = { .name = "exynos-smp-cpufreq", };

static int exynos_cpufreq_device_init(void)
{
	return IS_ERR(platform_device_register_full(&devinfo));
}
device_initcall(exynos_cpufreq_device_init);

static int exynos_smp_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct device *cpu_dev;
	struct opp *opp;
	int ret;

	np = get_cpu_node_with_valid_op(pdev->dev.id);
	if (!np)
		return -ENODEV;

	of_node_put(np);

	if (soc_is_exynos7580_v1()) {
		pm_qos_add_request(&pm_qos_mif, PM_QOS_BUS_THROUGHPUT, exynos_bus_table[ARRAY_SIZE(apll_freq) - 2]);
		pm_qos_add_request(&cluster_qos_max[CL_ZERO], PM_QOS_CLUSTER0_FREQ_MAX, apll_freq[1].freq / 1000);
#ifndef CONFIG_EXYNOS7580_QUAD
		pm_qos_add_request(&cluster_qos_max[CL_ONE], PM_QOS_CLUSTER1_FREQ_MAX, apll_freq[1].freq / 1000);
		maxlock_freq = apll_freq[1].freq / 1000;
#endif
	} else {
		pm_qos_add_request(&pm_qos_mif, PM_QOS_BUS_THROUGHPUT, exynos_bus_table[ARRAY_SIZE(apll_freq) - 1]);
		pm_qos_add_request(&cluster_qos_max[CL_ZERO], PM_QOS_CLUSTER0_FREQ_MAX, apll_freq[0].freq / 1000);
#ifndef CONFIG_EXYNOS7580_QUAD
		pm_qos_add_request(&cluster_qos_max[CL_ONE], PM_QOS_CLUSTER1_FREQ_MAX, apll_freq[0].freq / 1000);
		maxlock_freq = apll_freq[0].freq / 1000;
#endif
	}

	pm_qos_add_request(&cluster_qos_min[CL_ZERO], PM_QOS_CLUSTER0_FREQ_MIN, 0);
#ifndef CONFIG_EXYNOS7580_QUAD
	pm_qos_add_request(&cluster_qos_min[CL_ONE], PM_QOS_CLUSTER1_FREQ_MIN, 0);
#endif

	ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret)
		pr_err("%s: Failed creating sysfs group, err: %d\n",
				__func__, ret);

	exynos_tmu_add_notifier(&exynos_tmu_nb);

	pm_qos_add_notifier(PM_QOS_CLUSTER0_FREQ_MIN, &exynos_min_cluster0_nb);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_FREQ_MAX, &exynos_max_cluster0_nb);
#ifndef CONFIG_EXYNOS7580_QUAD
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MIN, &exynos_min_cluster1_nb);
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MAX, &exynos_max_cluster1_nb);
#endif

	ret = cpufreq_register_driver(&exynos_cpufreq_driver);
	if (ret)
		pr_info("%s: Failed registering platform driver, err: %d\n",
				__func__, ret);

	pm_qos_add_request(&boost_qos_min[CL_ZERO] , PM_QOS_CLUSTER0_FREQ_MIN, 0);
	exynos_cpufreq_boost_frequency(0, 30000);
#ifndef CONFIG_EXYNOS7580_QUAD
	pm_qos_add_request(&boost_qos_min[CL_ONE] , PM_QOS_CLUSTER1_FREQ_MIN, 0);
	exynos_cpufreq_boost_frequency(4, 30000);
#endif

	cpu_dev = get_cpu_device(0);
	opp = opp_find_freq_exact(cpu_dev, locking_frequency * 1000, true);
	locking_volt = opp_get_voltage(opp);

#ifndef CONFIG_EXYNOS7580_QUAD
	register_hotcpu_notifier(&exynos_cpufreq_cpu_up_nb);
	register_hotcpu_notifier(&exynos_cpufreq_cpu_down_nb);
#endif

	return ret;
}

static int exynos_smp_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&exynos_cpufreq_driver);

	pm_qos_remove_request(&pm_qos_mif);

	pr_info("%s: Un-registered platform driver\n", __func__);

	return 0;
}

static struct platform_driver exynos_smp_platdrv = {
	.driver = {
		.name   = "exynos-smp-cpufreq",
		.owner  = THIS_MODULE,
	},
	.probe          = exynos_smp_probe,
	.remove         = exynos_smp_remove,
};
module_platform_driver(exynos_smp_platdrv);

MODULE_AUTHOR("Jonghwan Choi <jhbird.choi@samsung.com>");
MODULE_DESCRIPTION("Exynos SMP cpufreq driver via DT");
MODULE_LICENSE("GPL");
