/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPU frequency scaling support for EXYNOS series
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
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#include <plat/cpu.h>

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long   ref;
	unsigned int    freq;
};

static struct lpj_info global_lpj_ref;
#endif

#define ARM_COLD_OFFSET	50000

/* Use boot_freq when entering sleep mode */
static unsigned int boot_freq;
static unsigned int freq_max;
static unsigned int freq_min;

static struct exynos_dvfs_info *exynos_info;

static struct regulator *arm_regulator;
static struct cpufreq_freqs freqs;

DEFINE_MUTEX(cpufreq_lock);

static bool exynos_cpufreq_disable;
static bool exynos_cpufreq_init_done;

static struct pm_qos_request min_cpu_qos;
static struct pm_qos_request max_cpu_qos;

int exynos_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy,
					      exynos_info->freq_table);
}

unsigned int exynos_getspeed(unsigned int cpu)
{
	return clk_get_rate(exynos_info->cpu_clk) / 1000;
}

static unsigned int exynos_get_safe_armvolt(unsigned int old_index, unsigned int new_index)
{
	unsigned int safe_arm_volt = 0;
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int *volt_table = exynos_info->volt_table;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * reguired voltage level
	 */

	if (exynos_info->need_apll_change != NULL) {
		if (exynos_info->need_apll_change(old_index, new_index) &&
			(freq_table[new_index].frequency < exynos_info->mpll_freq_khz) &&
			(freq_table[old_index].frequency < exynos_info->mpll_freq_khz)) {
				safe_arm_volt = volt_table[exynos_info->pll_safe_idx];
			}

	}

	return safe_arm_volt;
}

int exynos4_frequency_table_target(struct cpufreq_policy *policy,
		struct cpufreq_frequency_table *table,
		unsigned int target_freq,
		unsigned int relation,
		unsigned int *index)
{
	unsigned int i;

	if (!cpu_online(policy->cpu))
		return -EINVAL;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			*index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return 0;
}

static int exynos_cpufreq_scale(unsigned int target_freq,
				unsigned int curr_freq, struct cpufreq_policy *policy)
{
	unsigned int *volt_table = exynos_info->volt_table;
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	int new_index, old_index;
	unsigned int arm_volt, safe_arm_volt = 0;
	unsigned int max_volt;
	int ret = 0;

	freqs.new = target_freq;

	if (curr_freq == freqs.new)
		goto out;

	if (exynos4_frequency_table_target(policy, freq_table,
					   curr_freq, CPUFREQ_RELATION_H, &old_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (exynos4_frequency_table_target(policy, freq_table,
					   target_freq, CPUFREQ_RELATION_H, &new_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (old_index == new_index)
		goto out;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	safe_arm_volt = exynos_get_safe_armvolt(old_index, new_index);

	arm_volt = volt_table[new_index];
	max_volt = volt_table[0];

	/* When the new frequency is higher than current frequency */
	if ((freqs.new > freqs.old) && !safe_arm_volt) {
		/* Firstly, voltage up to increase frequency */
		ret = regulator_set_voltage(arm_regulator, arm_volt, max_volt);
		if (ret) {
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, arm_volt);
			return ret;
		}
#ifdef CONFIG_SOC_EXYNOS4415
		exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
		exynos_set_ema(arm_volt);
#else
		if (soc_is_exynos3470())
			exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
#endif
	}

	if (safe_arm_volt) {
		ret = regulator_set_voltage(arm_regulator, safe_arm_volt,
				max_volt);
		if (ret) {
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, safe_arm_volt);
			return ret;
		}
		if (soc_is_exynos3470())
			exynos_set_abb(ID_ARM, exynos_info->abb_table[exynos_info->pll_safe_idx]);
	}

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	exynos_info->set_freq(old_index, new_index);

#ifdef CONFIG_SMP
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs.old;
	}
	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref, global_lpj_ref.freq,
			freqs.new);
#endif

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	/* When the new frequency is lower than current frequency */
	if ((freqs.new < freqs.old) ||
	   ((freqs.new > freqs.old) && safe_arm_volt)) {
		/* down the voltage after frequency change */
		ret = regulator_set_voltage(arm_regulator, arm_volt, max_volt);
		if (ret)
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, arm_volt);
#ifdef CONFIG_SOC_EXYNOS4415
		exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
		exynos_set_ema(arm_volt);
#else
		if (soc_is_exynos3470())
			exynos_set_abb(ID_ARM, exynos_info->abb_table[new_index]);
#endif
	}

out:
	return ret;
}


static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int index;
	unsigned int new_freq;
	int ret = 0;

	mutex_lock(&cpufreq_lock);

	if (exynos_cpufreq_disable)
		goto out;

	freqs.old = policy->cur;

	target_freq = max((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MIN), target_freq);
	target_freq = min((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MAX), target_freq);

	if (cpufreq_frequency_table_target(policy, freq_table,
					   target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	new_freq = freq_table[index].frequency;

	exynos_cpufreq_scale(new_freq, freqs.old, policy);

out:
	mutex_unlock(&cpufreq_lock);

	return ret;
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}
#endif

/**
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *			context
 * @notifier
 * @pm_event
 * @v
 *
 * While cpufreq_disable == true, target() ignores every frequency but
 * boot_freq. The boot_freq value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	int ret;
	unsigned int tmp_volt;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	if (!policy) {
		pr_err("%s: Cannot get policy struct\n", __func__);
		return NOTIFY_BAD;
	}

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&cpufreq_lock);
		exynos_cpufreq_disable = true;
		mutex_unlock(&cpufreq_lock);

		freqs.old = exynos_getspeed(0);
		ret = exynos_cpufreq_scale(boot_freq, freqs.old, policy);

		if (ret < 0)
			return NOTIFY_BAD;

		tmp_volt = regulator_get_voltage(arm_regulator);
		tmp_volt += ARM_COLD_OFFSET;
		regulator_set_voltage(arm_regulator, tmp_volt, tmp_volt);

#ifdef CONFIG_SOC_EXYNOS4415
		exynos_set_abb(ID_ARM, ABB_BYPASS);
		exynos_set_ema(tmp_volt);
#else
		if (samsung_rev() >= EXYNOS3470_REV_2_0)
			exynos_set_abb(ID_ARM, ABB_BYPASS);
#endif

		pr_debug("PM_SUSPEND_PREPARE for CPUFREQ\n");
		break;
	case PM_POST_SUSPEND:
		pr_debug("PM_POST_SUSPEND for CPUFREQ\n");
		/* L6 level frequency 800MHz */
#ifdef CONFIG_SOC_EXYNOS4415
		exynos_set_abb(ID_ARM, exynos_info->abb_table[L6]);
#else
		if (samsung_rev() >= EXYNOS3470_REV_2_0)
			exynos_set_abb(ID_ARM, exynos_info->abb_table[L6]);
#endif

		mutex_lock(&cpufreq_lock);
		exynos_cpufreq_disable = false;
		mutex_unlock(&cpufreq_lock);

		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

	cpufreq_frequency_table_get_attr(exynos_info->freq_table, policy->cpu);

	/* set the transition latency value */
	policy->cpuinfo.transition_latency = 100000;

	/*
	 * EXYNOS4 multi-core processors has 2 cores
	 * that the frequency cannot be set independently.
	 * Each cpu is bound to the same speed.
	 * So the affected cpu is all of the cpus.
	 */
	if (num_online_cpus() == 1) {
		cpumask_copy(policy->related_cpus, cpu_possible_mask);
		cpumask_copy(policy->cpus, cpu_online_mask);
	} else {
		cpumask_setall(policy->cpus);
	}

	return cpufreq_frequency_table_cpuinfo(policy, exynos_info->freq_table);
}

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= exynos_verify_speed,
	.target		= exynos_target,
	.get		= exynos_getspeed,
	.init		= exynos_cpufreq_cpu_init,
	.name		= "exynos_cpufreq",
#ifdef CONFIG_PM
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
#endif
};

/************************** sysfs interface ************************/

static ssize_t show_freq_table(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (!tbl_sz)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
					freq_table[i].frequency);
	}

	count += snprintf(&buf[count], 2, "\n");
	return count;
}

static ssize_t show_min_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MIN));
}

static ssize_t show_max_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MAX));
}

static ssize_t store_min_freq(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = min(input, (int)freq_max);

	if (input <= (int)freq_min) {
		if (pm_qos_request_active(&min_cpu_qos))
			pm_qos_update_request(&min_cpu_qos, freq_min);
	} else {
		if (pm_qos_request_active(&min_cpu_qos))
			pm_qos_update_request(&min_cpu_qos, input);
		else
			pm_qos_add_request(&min_cpu_qos, PM_QOS_CPU_FREQ_MIN, input);
	}

	return count;
}

static ssize_t store_max_freq(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = max(input, (int)freq_min);

	if (input >= (int)freq_max || input <= 0) {
		if (pm_qos_request_active(&max_cpu_qos))
			pm_qos_update_request(&max_cpu_qos, freq_max);
	} else {
		if (pm_qos_request_active(&max_cpu_qos))
			pm_qos_update_request(&max_cpu_qos, input);
		else
			pm_qos_add_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX, input);
	}

	return count;
}

define_one_global_ro(freq_table);
define_one_global_rw(min_freq);
define_one_global_rw(max_freq);

static struct global_attr cpufreq_table =
		__ATTR(cpufreq_table, S_IRUGO, show_freq_table, NULL);
static struct global_attr cpufreq_min_limit =
		__ATTR(cpufreq_min_limit, S_IRUGO | S_IWUSR, show_min_freq, store_min_freq);
static struct global_attr cpufreq_max_limit =
		__ATTR(cpufreq_max_limit, S_IRUGO | S_IWUSR, show_max_freq, store_max_freq);

static struct attribute *cpufreq_attributes[] = {
	&freq_table.attr,
	&min_freq.attr,
	&max_freq.attr,
	NULL
};

static struct attribute_group cpufreq_attr_group = {
	.attrs = cpufreq_attributes,
	.name = "exynos-cpufreq",
};

/************************** sysfs end ************************/
static int exynos_cpufreq_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	int ret;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	if (!policy) {
		pr_err("%s: Cannot get policy struct\n", __func__);
		return NOTIFY_BAD;
	}

	mutex_lock(&cpufreq_lock);
	exynos_cpufreq_disable = true;
	mutex_unlock(&cpufreq_lock);

	freqs.old = exynos_getspeed(0);
	ret = exynos_cpufreq_scale(boot_freq, freqs.old, policy);

	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_DONE;
}


static int exynos_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	struct cpufreq_policy *policy;

	if (freqs.old >= val)
		return NOTIFY_OK;

	policy = cpufreq_cpu_get(0);

	if (!policy)
		return  NOTIFY_BAD;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_OK;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static int exynos_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	struct cpufreq_policy *policy;

	if (freqs.old <= val)
		return NOTIFY_OK;

	policy = cpufreq_cpu_get(0);

	if (!policy)
		return  NOTIFY_BAD;

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		return NOTIFY_OK;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static struct notifier_block exynos_min_qos_notifier = {
	.notifier_call = exynos_min_qos_handler,
};

static struct notifier_block exynos_max_qos_notifier = {
	.notifier_call = exynos_max_qos_handler,
};

static struct notifier_block exynos_cpufreq_reboot_notifier = {
	.notifier_call = exynos_cpufreq_reboot_notifier_call,
};

static int __init exynos_cpufreq_init(void)
{
	int ret = -EINVAL;

	exynos_info = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info)
		return -ENOMEM;

	if (soc_is_exynos3470())
		ret = exynos3470_cpufreq_init(exynos_info);
	else if (soc_is_exynos4210())
		ret = exynos4210_cpufreq_init(exynos_info);
	else if (soc_is_exynos4212() || soc_is_exynos4412())
		ret = exynos4x12_cpufreq_init(exynos_info);
	else if (soc_is_exynos4415())
		ret = exynos4415_cpufreq_init(exynos_info);
	else if (soc_is_exynos5250())
		ret = exynos5250_cpufreq_init(exynos_info);
	else
		pr_err("%s: CPU type not found\n", __func__);

	if (ret)
		goto err_vdd_arm;

	if (exynos_info->set_freq == NULL) {
		pr_err("%s: No set_freq function (ERR)\n", __func__);
		goto err_vdd_arm;
	}

	arm_regulator = regulator_get(NULL, "vdd_arm");
	if (IS_ERR(arm_regulator)) {
		pr_err("%s: failed to get resource vdd_arm\n", __func__);
		goto err_vdd_arm;
	}

	freq_max = exynos_info->freq_table[exynos_info->max_support_idx].frequency;
	freq_min = exynos_info->freq_table[exynos_info->min_support_idx].frequency;

	exynos_cpufreq_disable = false;

	register_pm_notifier(&exynos_cpufreq_nb);
	register_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MIN, &exynos_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MAX, &exynos_max_qos_notifier);

	pm_qos_add_request(&min_cpu_qos, PM_QOS_CPU_FREQ_MIN, freq_min);
	pm_qos_add_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX, freq_max);

	if (cpufreq_register_driver(&exynos_driver)) {
		pr_err("%s: failed to register cpufreq driver\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_group(cpufreq_global_kobject, &cpufreq_attr_group);
	if (ret) {
		pr_err("%s: failed to create iks-cpufreq sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(cpufreq_global_kobject, &cpufreq_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_table sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(cpufreq_global_kobject, &cpufreq_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_min_limit sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(cpufreq_global_kobject, &cpufreq_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_max_limit sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_max_limit sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_min_limit sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_table sysfs interface\n", __func__);
		goto err_cpufreq;
	}

	boot_freq = exynos_getspeed(0);

	exynos_cpufreq_init_done = true;
#ifdef CONFIG_EXYNOS_DM_CPU_HOTPLUG
	dm_cpu_hotplug_init();
#endif

	return 0;
err_cpufreq:
	unregister_pm_notifier(&exynos_cpufreq_nb);

	if (!IS_ERR(arm_regulator))
		regulator_put(arm_regulator);
err_vdd_arm:
	kfree(exynos_info);
	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
late_initcall(exynos_cpufreq_init);
