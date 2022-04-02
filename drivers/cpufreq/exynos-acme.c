/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <linux/tick.h>
#include <linux/debug-snapshot.h>
#include <linux/pm_opp.h>
#include <linux/cpu_cooling.h>
#include <linux/suspend.h>
#include <linux/ems.h>
#include <linux/sec_pm_cpufreq.h>

#include <trace/events/power.h>

#include <soc/samsung/cal-if.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/exynos-cpuhp.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-alt.h>

#include "exynos-acme.h"
#include "exynos-ufc.h"

/*
 * list head of cpufreq domain
 */
LIST_HEAD(domains);

/*
 * list head of units which have cpufreq policy dependancy
 */
LIST_HEAD(ready_list);

/*
 * flag to constrain frequency
 */
static unsigned int cpufreq_constraint_flag;
static unsigned int cpufreq_init_flag;

/* slack timer per cpu */
static DEFINE_PER_CPU(struct exynos_slack_timer, exynos_slack_timer);


#if defined(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
/*
 * disable cpu boot qos lock : flexable.cpuboot value is valid ( 0 , 1, 2 )
 * cpufreq_disable_boot_qos_lock does not apply max lock
 */
#define DISABLE_BOOT_QOS_LOCK_MAGIC 12349876

static int cpufreq_disable_boot_qos_lock_magic;
static int cpufreq_disable_boot_qos_lock_idx;

static int __init get_acme_boot_disable(char *str)
{
	int domain_idx;

	if (get_option(&str, &domain_idx)) {
		cpufreq_disable_boot_qos_lock_idx = domain_idx;
		cpufreq_disable_boot_qos_lock_magic = DISABLE_BOOT_QOS_LOCK_MAGIC;
	}

	return 0;
}
early_param("flexable.cpuboot", get_acme_boot_disable);
#endif

/*********************************************************************
 *                          HELPER FUNCTION                          *
 *********************************************************************/

struct list_head *get_domain_list(void)
{
	return &domains;
}

static struct exynos_cpufreq_domain *find_domain(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;

	pr_err("cannot find cpufreq domain by cpu\n");
	return NULL;
}

static
struct exynos_cpufreq_domain *find_domain_pm_qos_class(int pm_qos_class)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (domain->pm_qos_min_class == pm_qos_class ||
			domain->pm_qos_max_class == pm_qos_class)
			return domain;

	pr_err("cannot find cpufreq domain by PM QoS class\n");
	return NULL;
}

struct
exynos_cpufreq_domain *find_domain_cpumask(const struct cpumask *mask)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (cpumask_subset(mask, &domain->cpus))
			return domain;

	pr_err("cannot find cpufreq domain by cpumask\n");
	return NULL;
}

static void enable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = true;
	mutex_unlock(&domain->lock);
}

static void disable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = false;
	mutex_unlock(&domain->lock);
}

static bool static_governor(struct cpufreq_policy *policy)
{
	/*
	 * During cpu hotplug in sequence, governor can be empty for
	 * a while. In this case, we regard governor as default
	 * governor. Exynos never use static governor as default.
	 */
	if (!policy->governor)
		return false;

	if ((strcmp(policy->governor->name, "userspace") == 0)
		|| (strcmp(policy->governor->name, "performance") == 0)
		|| (strcmp(policy->governor->name, "powersave") == 0))
		return true;

	return false;
}

/*********************************************************************
 *                         FREQUENCY SCALING                         *
 *********************************************************************/
/*
 * Depending on cluster structure, it cannot be possible to get/set
 * cpu frequency while cluster is off. For this, disable cluster-wide
 * power mode while getting/setting frequency.
 */
static unsigned int get_freq(struct exynos_cpufreq_domain *domain)
{
	int wakeup_flag = 0;
	unsigned int freq;
	struct cpumask temp;

	cpumask_and(&temp, &domain->cpus, cpu_active_mask);

	if (cpumask_empty(&temp))
		return domain->old;

	if (domain->need_awake) {
		if (likely(domain->old))
			return domain->old;

		wakeup_flag = 1;
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);
	}

	freq = (unsigned int)cal_dfs_get_rate(domain->cal_id);
	if (!freq) {
		/* On changing state, CAL returns 0 */
		freq = domain->old;
	}

	if (unlikely(wakeup_flag))
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	return freq;
}

static int set_freq(struct exynos_cpufreq_domain *domain,
					unsigned int target_freq)
{
	int err;

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_IN);

	if (domain->need_awake)
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	err = cal_dfs_set_rate(domain->cal_id, target_freq);
	if (err < 0)
		pr_err("failed to scale frequency of domain%d (%d -> %d)\n",
			domain->id, domain->old, target_freq);

	if (domain->need_awake)
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_OUT);

	return err;
}

static unsigned int apply_pm_qos(struct exynos_cpufreq_domain *domain,
					struct cpufreq_policy *policy,
					unsigned int target_freq)
{
	unsigned int freq;
	int qos_min, qos_max;

	/*
	 * In case of static governor, it should garantee to scale to
	 * target, it does not apply PM QoS.
	 */
	if (static_governor(policy))
		return target_freq;

	qos_min = pm_qos_request(domain->pm_qos_min_class);
	qos_max = pm_qos_request(domain->pm_qos_max_class);

	if (qos_min < 0 || qos_max < 0)
		return target_freq;

	if (qos_min > policy->cpuinfo.max_freq) {
		qos_min = policy->cpuinfo.max_freq;
	}
	if (qos_max < policy->cpuinfo.min_freq) {
		qos_max = policy->cpuinfo.min_freq;
	}

	freq = max((unsigned int)qos_min, target_freq);
	freq = min((unsigned int)qos_max, freq);

	return freq;
}

static int pre_scale(void)
{
	return 0;
}

static int post_scale(void)
{
	return 0;
}

static int scale(struct exynos_cpufreq_domain *domain,
				struct cpufreq_policy *policy,
				unsigned int target_freq)
{
	int ret;
	struct cpufreq_freqs freqs = {
		.cpu		= policy->cpu,
		.old		= domain->old,
		.new		= target_freq,
		.flags		= 0,
	};

	cpufreq_freq_transition_begin(policy, &freqs);
	dbg_snapshot_freq(domain->id, domain->old, target_freq, DSS_FLAG_IN);

	ret = pre_scale();
	if (ret)
		goto fail_scale;

	/* Scale frequency by hooked function, set_freq() */
	ret = set_freq(domain, target_freq);
	if (ret)
		goto fail_scale;

	ret = post_scale();
	if (ret)
		goto fail_scale;

fail_scale:
	/* In scaling failure case, logs -1 to exynos snapshot */
	dbg_snapshot_freq(domain->id, domain->old, target_freq,
					ret < 0 ? ret : DSS_FLAG_OUT);
	cpufreq_freq_transition_end(policy, &freqs, ret);

	return ret;
}

static int update_freq(struct exynos_cpufreq_domain *domain,
					 unsigned int freq)
{
	struct cpufreq_policy *policy;
	int ret;
	struct cpumask mask;

	pr_debug("update frequency of domain%d to %d kHz\n",
						domain->id, freq);

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return -ENODEV;
	policy = cpufreq_cpu_get(cpumask_first(&mask));
	if (!policy)
		return -EINVAL;

	if (static_governor(policy)) {
		cpufreq_cpu_put(policy);
		return 0;
	}

	ret = cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	return ret;
}

/*********************************************************************
 *                   EXYNOS CPUFREQ DRIVER INTERFACE                 *
 *********************************************************************/
static int exynos_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	if (!domain)
		return -EINVAL;

	policy->freq_table = domain->freq_table;
	policy->cur = get_freq(domain);
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->dvfs_possible_from_any_cpu = true;
	cpumask_copy(policy->cpus, &domain->cpus);

	pr_info("CPUFREQ domain%d registered\n", domain->id);

	return 0;
}

static unsigned int exynos_cpufreq_resolve(struct cpufreq_policy *policy,
						unsigned int target_freq)
{
	unsigned int index;

	index = cpufreq_frequency_table_target(policy, target_freq, CPUFREQ_RELATION_L);
	if (index < 0) {
		pr_err("target frequency(%d) out of range\n", target_freq);
		return 0;
	}

	return policy->freq_table[index].frequency;
}


static int exynos_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	if (!domain)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, domain->freq_table);
}

static int __exynos_cpufreq_target(struct cpufreq_policy *policy,
				  unsigned int target_freq,
				  unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned int resolve_freq;
	int ret = 0;
	struct dev_pm_opp *opp;
	struct device *dev = get_cpu_device(policy->cpu);
	unsigned long freq_khz = target_freq * 1000;

	if (!domain)
		return -EINVAL;

	mutex_lock(&domain->lock);

	if (!domain->enabled) {
		ret = -EINVAL;
		goto out;
	}

	if (domain->old != get_freq(domain)) {
		pr_err("oops, inconsistency between domain->old:%d, real clk:%d\n",
			domain->old, get_freq(domain));
		BUG_ON(1);
	}

	resolve_freq = exynos_cpufreq_resolve(policy, target_freq);
	if (target_freq != resolve_freq)
		pr_debug("%s:%d target_freq(%u) is differ with resolve_freq(%u)\n",
				__func__, __LINE__, target_freq, resolve_freq);

	if (relation == CPUFREQ_RELATION_H) {
		opp = dev_pm_opp_find_freq_floor(dev, &freq_khz);
		if (opp == ERR_PTR(-ERANGE))
			opp = dev_pm_opp_find_freq_ceil(dev, &freq_khz);
	} else {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq_khz);
		if (opp == ERR_PTR(-ERANGE))
			opp = dev_pm_opp_find_freq_floor(dev, &freq_khz);
	}

	target_freq = (freq_khz / 1000);


	/* Target is same as current, skip scaling */
	if (domain->old == target_freq)
		goto out;

	ret = scale(domain, policy, target_freq);
	if (ret)
		goto out;

	pr_debug("CPUFREQ domain%d frequency change %u kHz -> %u kHz\n",
			domain->id, domain->old, target_freq);

	domain->old = target_freq;
	arch_set_freq_scale(&domain->cpus, target_freq,
				min(policy->max, domain->qos_max_freq));

out:
	mutex_unlock(&domain->lock);

	return ret;
}

static int exynos_cpufreq_target(struct cpufreq_policy *policy,
					unsigned int target_freq,
					unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned long freq;

	if (!domain)
		return -EINVAL;

	if (!domain->enabled)
		return -EINVAL;

	target_freq = apply_pm_qos(domain, policy, target_freq);

	if (list_empty(&domain->dm_list))
		return __exynos_cpufreq_target(policy, target_freq, relation);

	mutex_lock(&domain->lock);

	freq = exynos_cpufreq_resolve(policy, target_freq);
	if (!freq || domain->old == freq) {
		mutex_unlock(&domain->lock);
		return 0;
	}
	mutex_unlock(&domain->lock);

	freq = (unsigned long)target_freq;

	exynos_alt_call_chain();

	return DM_CALL(domain->dm_type, &freq);
}

static unsigned int exynos_cpufreq_get(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain = find_domain(cpu);

	if (!domain)
		return 0;

	return get_freq(domain);
}

static int __exynos_cpufreq_suspend(struct exynos_cpufreq_domain *domain)
{
	struct cpumask mask;

	if (!domain)
		return -EINVAL;

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return 0;

	/* To handle reboot faster, it does not thrrotle frequency of domain0 */
	if (system_state == SYSTEM_RESTART && domain->id != 0)
		cpufreq_constraint_flag |= CPUFREQ_RESTART;

	cpufreq_update_policy(cpumask_any(&mask));

	disable_domain(domain);

	return 0;
}

static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	return __exynos_cpufreq_suspend(domain);
}

static int __exynos_cpufreq_resume(struct exynos_cpufreq_domain *domain)
{
	struct cpumask mask;

	if (!domain)
		return -EINVAL;

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return 0;

	enable_domain(domain);

	cpufreq_update_policy(cpumask_any(&mask));

	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	return __exynos_cpufreq_resume(domain);
}

static void exynos_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_ready_block *ready_block;

	list_for_each_entry(ready_block, &ready_list, list) {
		if (ready_block->update)
			ready_block->update(policy);
		if (ready_block->get_target)
			ready_block->get_target(policy, exynos_cpufreq_target);
	}
}

static int exynos_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct exynos_cpufreq_domain *domain;

	if (!cpufreq_init_flag) {
		pr_warn("ACME is not initialized\n");
		return NOTIFY_BAD;
	}

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cpufreq_constraint_flag |= CPUFREQ_SUSPEND;
		list_for_each_entry_reverse(domain, &domains, list)
			if (__exynos_cpufreq_suspend(domain))
				return NOTIFY_BAD;
		break;
	case PM_POST_SUSPEND:
		cpufreq_constraint_flag &= ~CPUFREQ_SUSPEND;
		list_for_each_entry_reverse(domain, &domains, list)
			if (__exynos_cpufreq_resume(domain))
				return NOTIFY_BAD;
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_pm = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

static struct cpufreq_driver exynos_driver = {
	.name		= "exynos_cpufreq",
	.flags		= CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init		= exynos_cpufreq_driver_init,
	.verify		= exynos_cpufreq_verify,
	.target		= exynos_cpufreq_target,
	.get		= exynos_cpufreq_get,
	.resolve_freq	= exynos_cpufreq_resolve,
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
	.ready		= exynos_cpufreq_ready,
	.exit		= exynos_cpufreq_exit,
	.attr		= cpufreq_generic_attr,
};

/*********************************************************************
 *                      SUPPORT for DVFS MANAGER                     *
 *********************************************************************/

static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = devdata;
	struct cpufreq_policy *policy;
	struct cpumask mask;
	int ret;

	/* Skip scaling if all cpus of domain are hotplugged out */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return -ENODEV;

	if (relation == EXYNOS_DM_RELATION_L)
		relation = CPUFREQ_RELATION_L;
	else
		relation = CPUFREQ_RELATION_H;

	policy = cpufreq_cpu_get(cpumask_first(&mask));
	if (!policy) {
		pr_err("%s: failed get cpufreq policy\n", __func__);
		return -ENODEV;
	}

	ret = __exynos_cpufreq_target(policy, target_freq, relation);

	cpufreq_cpu_put(policy);

	return ret;
}

/*********************************************************************
 * 			CPUFREQ SLACK TIMER			     *
 *********************************************************************/

static void slack_update_min(struct cpufreq_policy *policy)
{
	unsigned int cpu;
	unsigned long max_cap, min_cap;
	struct exynos_slack_timer *slack_timer;

	max_cap = arch_scale_cpu_capacity(NULL, policy->cpu);

	/* min_cap is minimum value making higher frequency than policy->min */
	min_cap = (max_cap * policy->min) / policy->max;
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ENERGYSTEP
	min_cap -= 1;
#else
	min_cap = (min_cap * 4 / 5) + 1;
#endif
	for_each_cpu(cpu, policy->cpus) {
		slack_timer = &per_cpu(exynos_slack_timer, cpu);
		slack_timer->min = min_cap;
	}
}

static s64 get_next_event_time_ms(unsigned int cpu)
{
	return ktime_to_us(ktime_sub(*(get_next_event_cpu(cpu)), ktime_get()));
}

static int need_slack_timer(unsigned int cpu)
{
	struct exynos_slack_timer *slack_timer = &per_cpu(exynos_slack_timer, cpu);
	unsigned long util = cpufreq_governor_get_util(cpu);

	if ((util > slack_timer->min) &&
		(get_next_event_time_ms(cpu) > slack_timer->expired_time))
		return 1;

	return 0;
}

static void slack_nop_timer(struct timer_list *timer)
{
	/*
	 * The purpose of slack-timer is to wake up the CPU from IDLE, in order
	 * to decrease its frequency if it is not set to minimum already.
	 *
	 * This is important for platforms where CPU with higher frequencies
	 * consume higher power even at IDLE.
	 */
	trace_exynos_slack_func(smp_processor_id());
}

/*********************************************************************
 *                       CPUFREQ SYSFS			             *
 *********************************************************************/

static ssize_t show_cpufreq_qos_min(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		count += snprintf(buf + count, 30, "cpu%d: qos_min: %d\n",
				cpumask_first(&domain->cpus), domain->user_qos_min_req.node.prio);
	return count;
}

static ssize_t store_cpufreq_qos_min(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int freq, cpu;
	struct exynos_cpufreq_domain *domain;

	if (!sscanf(buf, "%d %8d", &cpu, &freq))
		return -EINVAL;
	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)					\
		return -EINVAL;								\

	domain = find_domain(cpu);
	if (!domain)
		return -EINVAL;

	pm_qos_update_request(&domain->user_qos_min_req, freq);

	return count;
}


static ssize_t show_cpufreq_qos_max(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		count += snprintf(buf + count, 30, "cpu%d: qos_max: %d\n",
				cpumask_first(&domain->cpus), domain->user_qos_max_req.node.prio);
	return count;
}

static ssize_t store_cpufreq_qos_max(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int freq, cpu;
	struct exynos_cpufreq_domain *domain;

	if (!sscanf(buf, "%d %8d", &cpu, &freq))
		return -EINVAL;
	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)					\
		return -EINVAL;								\

	domain = find_domain(cpu);
	if (!domain)
		return -EINVAL;

	pm_qos_update_request(&domain->user_qos_max_req, freq);

	return count;
}

static struct kobj_attribute user_qos_max =
	__ATTR(cpufreq_qos_max, 0644,
		show_cpufreq_qos_max, store_cpufreq_qos_max);

static struct kobj_attribute user_qos_min =
	__ATTR(cpufreq_qos_min, 0644,
		show_cpufreq_qos_min, store_cpufreq_qos_min);

/*********************************************************************
 *                       CPUFREQ PM QOS HANDLER                      *
 *********************************************************************/
static void update_dm_constraint(struct exynos_cpufreq_domain *domain,
								struct cpufreq_policy *new_policy)
{
	struct cpufreq_policy *policy;
	unsigned int policy_min, policy_max;
	unsigned int pm_qos_min, pm_qos_max;
	struct cpumask mask;

	if (new_policy) {
		policy_min = new_policy->min;
		policy_max = new_policy->max;
	} else {
		cpumask_and(&mask, &domain->cpus, cpu_online_mask);
		if (cpumask_empty(&mask))
			return;
		policy = cpufreq_cpu_get(cpumask_first(&mask));
		if(!policy)
			return;

		policy_min = policy->min;
		policy_max = policy->max;
		cpufreq_cpu_put(policy);
	}

	pm_qos_min = pm_qos_request(domain->pm_qos_min_class);
	pm_qos_max = pm_qos_request(domain->pm_qos_max_class);

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return;
	policy = cpufreq_cpu_get_raw(cpumask_first(&mask));
	if(!policy)
		return;

	if (pm_qos_min > policy->cpuinfo.max_freq) {
		pm_qos_min = policy->cpuinfo.max_freq;
	}
	if (pm_qos_max < policy->cpuinfo.min_freq) {
		pm_qos_max = policy->cpuinfo.min_freq;
	}

	policy_update_call_to_DM(domain->dm_type, max(policy_min, pm_qos_min),
											min(policy_max, pm_qos_max));
}

static int need_update_freq(struct exynos_cpufreq_domain *domain,
				int pm_qos_class, unsigned int freq)
{
	unsigned int cur = get_freq(domain);

	if (cur == freq)
		return 0;

	if ((pm_qos_class != domain->pm_qos_min_class) &&
			(pm_qos_class != domain->pm_qos_max_class)) {
		/* invalid PM QoS class */
		return -EINVAL;
	}

	return 1;
}

static int exynos_cpufreq_pm_qos_callback(struct notifier_block *nb,
					unsigned long val, void *v)
{
	int pm_qos_class = *((int *)v);
	struct exynos_cpufreq_domain *domain;
	struct cpufreq_policy *policy;
	struct cpumask mask;
	int ret;
	unsigned int next_freq;
	int qos_max;

	pr_debug("update PM QoS class %d to %ld kHz\n", pm_qos_class, val);

	if (!cpufreq_init_flag) {
		pr_warn("ACME is not initialized\n");
		return NOTIFY_BAD;
	}

	domain = find_domain_pm_qos_class(pm_qos_class);
	if (!domain)
		return NOTIFY_BAD;

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return NOTIFY_BAD;

	policy = cpufreq_cpu_get(cpumask_first(&mask));
	if (!policy)
		return NOTIFY_BAD;

	down_read(&policy->rwsem);
	update_dm_constraint(domain, NULL);
	up_read(&policy->rwsem);

	if (pm_qos_class == domain->pm_qos_max_class) {
		rebuild_sched_energy_table(&domain->cpus, val,
					policy->cpuinfo.max_freq, STATES_PMQOS);
		qos_max = pm_qos_request(pm_qos_class);
		if (qos_max < policy->cpuinfo.min_freq) {
			domain->qos_max_freq = policy->cpuinfo.min_freq;
		} else
			domain->qos_max_freq = qos_max;
	}

	ret = need_update_freq(domain, pm_qos_class, val);
	if (ret < 0)
		return NOTIFY_BAD;
	if (!ret)
		return NOTIFY_OK;

	/*
	 * In 'need_update_freq()', pm qos class is checked whether min or max.
	 * When pm qos lock is released, we update the frequency with next freq.
	 * In normal case, lock is set with pm qos value or governor value.
	 * The reason is to apply the next freq immediately for fast reactivity.
	 */
	next_freq = cpufreq_governor_get_freq(policy->cpu);

	/* If 'sugov_get_freq()' fail, we just update frequency with pm qos val */
	if (next_freq) {
		if (pm_qos_class == domain->pm_qos_min_class)
			val = max_t(unsigned int, val, next_freq);
		else
			val = min_t(unsigned int, val, next_freq);
	}

	if (update_freq(domain, val))
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

/*********************************************************************
 *                       EXTERNAL EVENT HANDLER                      *
 *********************************************************************/

static int exynos_cpufreq_cpu_pm_callback(struct notifier_block *nb,
						unsigned long event, void *v)
{
	unsigned int cpu = raw_smp_processor_id();
	unsigned long util = cpufreq_governor_get_util(cpu);
	struct exynos_slack_timer *slack_timer = &per_cpu(exynos_slack_timer, cpu);
	struct timer_list *timer = &slack_timer->timer;

	if (!slack_timer->enabled)
		return NOTIFY_OK;

	switch (event) {
	case CPU_PM_ENTER_PREPARE:
		if (timer_pending(timer))
			del_timer_sync(timer);

		if (need_slack_timer(cpu)) {
			timer->expires = jiffies + msecs_to_jiffies(slack_timer->expired_time);
			add_timer_on(timer, cpu);

			trace_exynos_slack(cpu, util, slack_timer->min, event, 1);
		}
		break;

	case CPU_PM_ENTER:
		if (timer_pending(timer) && !need_slack_timer(cpu)) {
			del_timer_sync(timer);

			trace_exynos_slack(cpu, util, slack_timer->min, event, -1);
		}
		break;

	case CPU_PM_EXIT_POST:
		if (timer_pending(timer)) {
			del_timer_sync(timer);

			trace_exynos_slack(cpu, util, slack_timer->min, event, -1);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_cpu_pm_notifier = {
	.notifier_call = exynos_cpufreq_cpu_pm_callback,
};

static int exynos_cpufreq_policy_callback(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned int freq;

	if (!domain)
		return NOTIFY_OK;

	switch (event) {
	case CPUFREQ_ADJUST:
		if (cpufreq_constraint_flag) {
			if (cpufreq_constraint_flag & CPUFREQ_RESTART)
				freq = domain->min_freq;
			else if (cpufreq_constraint_flag & CPUFREQ_SUSPEND)
				freq = domain->resume_freq;
			else
				return NOTIFY_DONE;

			cpufreq_verify_within_limits(policy, freq, freq);
		}
		else
			return NOTIFY_DONE;
		break;

	case CPUFREQ_NOTIFY:
		arch_set_freq_scale(&domain->cpus, domain->old,
					min(policy->max, domain->qos_max_freq));
		update_dm_constraint(domain, policy);

		/* update min capacity for slack timer */
		slack_update_min(policy);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_policy_notifier = {
	.notifier_call = exynos_cpufreq_policy_callback,
};

extern bool cpuhp_tasks_frozen;
static int exynos_cpufreq_cpu_up_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed before cpufreq_resume() is called.
	 * Therefore, if this callback is called by enable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(cpu);
	if (!domain)
		return 0;

	/*
	 * The first incomming cpu in domain enables frequency scaling
	 * and clears limit of frequency.
	 */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&mask) == 1) {
		enable_domain(domain);
		pm_qos_update_request(&domain->max_qos_req, domain->max_freq);
	}

	return 0;
}

static int exynos_cpufreq_cpu_down_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed after cpufreq_suspend() is called.
	 * Therefore, if this callback is called by disable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(cpu);
	if (!domain)
		return 0;

	/*
	 * The last outgoing cpu in domain limits frequency to minimum
	 * and disables frequency scaling.
	 */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&mask) == 1) {
		pm_qos_update_request(&domain->max_qos_req, domain->min_freq);
		disable_domain(domain);
	}

	return 0;
}

/*********************************************************************
 *                       EXTERNAL REFERENCE APIs                     *
 *********************************************************************/
unsigned int exynos_cpufreq_get_max_freq(struct cpumask *mask)
{
	struct exynos_cpufreq_domain *domain = find_domain_cpumask(mask);

	return domain->max_freq;
}
EXPORT_SYMBOL(exynos_cpufreq_get_max_freq);

bool exynos_cpufreq_allow_change_max(unsigned int cpu, unsigned long max)
{
	struct exynos_cpufreq_domain *domain = find_domain(cpu);
	bool allow;

	mutex_lock(&domain->lock);
	allow = domain->old <= max;
	mutex_unlock(&domain->lock);

	return allow;
}

void exynos_cpufreq_ready_list_add(struct exynos_cpufreq_ready_block *rb)
{
	if (!rb)
		return;

	list_add(&rb->list, &ready_list);
}
EXPORT_SYMBOL(exynos_cpufreq_ready_list_add);

unsigned int __weak exynos_pstate_get_boost_freq(int cpu)
{
	return 0;
}
EXPORT_SYMBOL(exynos_pstate_get_boost_freq);

#ifdef CONFIG_SEC_BOOTSTAT
void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;
	int cluster;
	struct exynos_cpufreq_domain *domain;

	get_online_cpus();
	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu) {
		domain = find_domain(cpu);
		if (!domain)
			continue;
		pr_err("%s, dm type = %d\n", __func__, domain->dm_type);
		cluster = 0;
		if (domain->dm_type == DM_CPU_CL1)
			cluster = 1;
		else if (domain->dm_type == DM_CPU_CL2)
			cluster = 2;

		freq[cluster] = get_freq(domain);
	}
	put_online_cpus();
}
#endif

/*********************************************************************
 *                  INITIALIZE EXYNOS CPUFREQ DRIVER                 *
 *********************************************************************/
static void print_domain_info(struct exynos_cpufreq_domain *domain)
{
	int i;
	char buf[10];

	pr_info("CPUFREQ of domain%d cal-id : %#x\n",
			domain->id, domain->cal_id);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&domain->cpus));
	pr_info("CPUFREQ of domain%d sibling cpus : %s\n",
			domain->id, buf);

	pr_info("CPUFREQ of domain%d boot freq = %d kHz, resume freq = %d kHz\n",
			domain->id, domain->boot_freq, domain->resume_freq);

	pr_info("CPUFREQ of domain%d max freq : %d kHz, min freq : %d kHz\n",
			domain->id,
			domain->max_freq, domain->min_freq);

	pr_info("CPUFREQ of domain%d PM QoS max-class-id : %d, min-class-id : %d\n",
			domain->id,
			domain->pm_qos_max_class, domain->pm_qos_min_class);

	pr_info("CPUFREQ of domain%d table size = %d\n",
			domain->id, domain->table_size);

	for (i = 0; i < domain->table_size; i++) {
		if (domain->freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		pr_info("CPUFREQ of domain%d : L%2d  %7d kHz\n",
			domain->id,
			domain->freq_table[i].driver_data,
			domain->freq_table[i].frequency);
	}
}

static __init int init_table(struct exynos_cpufreq_domain *domain)
{
	unsigned int index, cpu;
	unsigned long *table;
	unsigned int *volt_table;
	struct exynos_cpufreq_dm *dm;
	int ret = 0;
	struct cpumask mask;

	/*
	 * Initialize frequency and voltage table of domain.
	 * Allocate temporary table to get DVFS table from CAL.
	 * Deliver this table to CAL API, then CAL fills the information.
	 */
	table = kzalloc(sizeof(unsigned long) * domain->table_size, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	volt_table = kzalloc(sizeof(unsigned int) * domain->table_size, GFP_KERNEL);
	if (!volt_table) {
		ret = -ENOMEM;
		goto free_table;
	}

	cal_dfs_get_rate_table(domain->cal_id, table);
	cal_dfs_get_asv_table(domain->cal_id, volt_table);

	cpumask_and(&mask, &domain->cpus, cpu_online_mask);

	for (index = 0; index < domain->table_size; index++) {
		domain->freq_table[index].driver_data = index;

		if (table[index] > domain->max_freq)
			domain->freq_table[index].frequency = CPUFREQ_ENTRY_INVALID;
		else if (table[index] < domain->min_freq)
			domain->freq_table[index].frequency = CPUFREQ_ENTRY_INVALID;
		else {
			domain->freq_table[index].frequency = table[index];
			/* Add OPP table to first cpu of domain */
			for_each_cpu(cpu, &mask) {
				dev_pm_opp_add(get_cpu_device(cpu),
						table[index] * 1000, volt_table[index]);
			}
		}

		/* Initialize table of DVFS manager constraint */
		list_for_each_entry(dm, &domain->dm_list, list)
			dm->c.freq_table[index].master_freq = table[index];
	}
	domain->freq_table[index].driver_data = index;
	domain->freq_table[index].frequency = CPUFREQ_TABLE_END;

	init_sched_energy_table(&domain->cpus, domain->table_size, table, volt_table,
				domain->max_freq, domain->min_freq);

	kfree(volt_table);

free_table:
	kfree(table);

	return ret;
}

static __init void set_boot_qos(struct exynos_cpufreq_domain *domain)
{
	unsigned int boot_qos, val;
	struct device_node *dn = domain->dn;

	/*
	 * Basically booting pm_qos is set to max frequency of domain.
	 * But if pm_qos-booting exists in device tree,
	 * booting pm_qos is selected to smaller one
	 * between max frequency of domain and the value defined in device tree.
	 */
	boot_qos = domain->max_freq;
	if (!of_property_read_u32(dn, "pm_qos-booting", &val))
		boot_qos = min(boot_qos, val);

#if defined(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
	if (cpufreq_disable_boot_qos_lock_magic == DISABLE_BOOT_QOS_LOCK_MAGIC) {
		pr_info("skip boot cpu[%d] min qos lock\n", domain->id);

		// skip max freq
		if (domain->id == cpufreq_disable_boot_qos_lock_idx) {
			pr_info("skip boot cpu[%d] max qos lock\n", domain->id);
		} else {
			pm_qos_update_request_timeout(&domain->max_qos_req,
				boot_qos, 40 * USEC_PER_SEC);
		}
	} else {
		pm_qos_update_request_timeout(&domain->min_qos_req,
			boot_qos, 40 * USEC_PER_SEC);
		pm_qos_update_request_timeout(&domain->max_qos_req,
			boot_qos, 40 * USEC_PER_SEC);
	}
#else
	pm_qos_update_request_timeout(&domain->min_qos_req,
			boot_qos, 40 * USEC_PER_SEC);
	pm_qos_update_request_timeout(&domain->max_qos_req,
			boot_qos, 40 * USEC_PER_SEC);
#endif
}

static __init int init_pm_qos(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	int ret;

	ret = of_property_read_u32(dn, "pm_qos-min-class",
					&domain->pm_qos_min_class);
	if (ret)
		return ret;

	ret = of_property_read_u32(dn, "pm_qos-max-class",
					&domain->pm_qos_max_class);
	if (ret)
		return ret;

	domain->pm_qos_min_notifier.notifier_call = exynos_cpufreq_pm_qos_callback;
	domain->pm_qos_min_notifier.priority = INT_MAX;
	domain->pm_qos_max_notifier.notifier_call = exynos_cpufreq_pm_qos_callback;
	domain->pm_qos_max_notifier.priority = INT_MAX;

	pm_qos_add_notifier(domain->pm_qos_min_class,
				&domain->pm_qos_min_notifier);
	pm_qos_add_notifier(domain->pm_qos_max_class,
				&domain->pm_qos_max_notifier);

	pm_qos_add_request(&domain->min_qos_req,
			domain->pm_qos_min_class, domain->min_freq);
	pm_qos_add_request(&domain->max_qos_req,
			domain->pm_qos_max_class, domain->max_freq);

	pm_qos_add_request(&domain->user_qos_min_req,
			domain->pm_qos_min_class, domain->min_freq);
	pm_qos_add_request(&domain->user_qos_max_req,
			domain->pm_qos_max_class, domain->max_freq);

	return 0;
}

static int init_constraint_table_ect(struct exynos_cpufreq_domain *domain,
					struct exynos_cpufreq_dm *dm,
					struct device_node *dn)
{
	void *block;
	struct ect_minlock_domain *ect_domain;
	const char *ect_name;
	unsigned int index, c_index;
	bool valid_row = false;
	int ret;

	ret = of_property_read_string(dn, "ect-name", &ect_name);
	if (ret)
		return ret;

	block = ect_get_block(BLOCK_MINLOCK);
	if (!block)
		return -ENODEV;

	ect_domain = ect_minlock_get_domain(block, (char *)ect_name);
	if (!ect_domain)
		return -ENODEV;

	for (index = 0; index < domain->table_size; index++) {
		unsigned int freq = domain->freq_table[index].frequency;

		for (c_index = 0; c_index < ect_domain->num_of_level; c_index++) {
			/* find row same as frequency */
			if (freq == ect_domain->level[c_index].main_frequencies) {
				dm->c.freq_table[index].slave_freq
					= ect_domain->level[c_index].sub_frequencies;
				valid_row = true;
				break;
			}
		}

		/*
		 * Due to higher levels of constraint_freq should not be NULL,
		 * they should be filled with highest value of sub_frequencies of ect
		 * until finding first(highest) domain frequency fit with main_frequeucy of ect.
		 */
		if (!valid_row)
			dm->c.freq_table[index].slave_freq
				= ect_domain->level[0].sub_frequencies;
	}

	return 0;
}

static int init_constraint_table_dt(struct exynos_cpufreq_domain *domain,
					struct exynos_cpufreq_dm *dm,
					struct device_node *dn)
{
	struct exynos_dm_freq *table;
	int size, index, c_index;

	/*
	 * A DVFS Manager table row consists of CPU and MIF frequency
	 * value, the size of a row is 64bytes. Divide size in half when
	 * table is allocated.
	 */
	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	table = kzalloc(sizeof(struct exynos_dm_freq) * size / 2, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	of_property_read_u32_array(dn, "table", (unsigned int *)table, size);
	for (index = 0; index < domain->table_size; index++) {
		unsigned int freq = domain->freq_table[index].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		for (c_index = 0; c_index < size / 2; c_index++) {
			/* find row same or nearby frequency */
			if (freq <= table[c_index].master_freq)
				dm->c.freq_table[index].slave_freq
					= table[c_index].slave_freq;

			if (freq >= table[c_index].master_freq)
				break;

		}
	}

	kfree(table);
	return 0;
}

static int init_dm(struct exynos_cpufreq_domain *domain,
				struct device_node *dn)
{
	struct device_node *root, *child;
	struct exynos_cpufreq_dm *dm;
	int ret;

	if (list_empty(&domain->dm_list))
		return 0;

	ret = of_property_read_u32(dn, "dm-type", &domain->dm_type);
	if (ret)
		return ret;

	ret = exynos_dm_data_init(domain->dm_type, domain, domain->min_freq,
				domain->max_freq, domain->old);
	if (ret)
		return ret;

	dm = list_entry(&domain->dm_list, struct exynos_cpufreq_dm, list);
	root = of_find_node_by_name(dn, "dm-constraints");
	for_each_child_of_node(root, child) {
		/*
		 * Initialize DVFS Manaver constraints
		 * - constraint_type : minimum or maximum constraint
		 * - constraint_dm_type : cpu/mif/int/.. etc
		 * - guidance : constraint from chipset characteristic
		 * - freq_table : constraint table
		 */
		dm = list_next_entry(dm, list);

		of_property_read_u32(child, "const-type", &dm->c.constraint_type);
		of_property_read_u32(child, "dm-type", &dm->c.dm_slave);

		if (of_property_read_bool(child, "guidance")) {
			dm->c.guidance = true;
			if (init_constraint_table_ect(domain, dm, child))
				continue;
		} else {
			if (init_constraint_table_dt(domain, dm, child))
				continue;
		}

		dm->c.table_length = domain->table_size;

		/* dynamic disable for migov control */
		if (of_property_read_bool(child, "dynamic-disable"))
			dm->c.support_dynamic_disable = true;

		ret = register_exynos_dm_constraint_table(domain->dm_type, &dm->c);
		if (ret)
			return ret;
	}

	return register_exynos_dm_freq_scaler(domain->dm_type, dm_scaler);
}

static __init void init_slack_timer(struct exynos_cpufreq_domain *domain,
		struct device_node *dn)
{
	int cpu;
	struct device_node *timer_node = NULL;

	timer_node = of_find_node_by_type(dn, "slack-timer-domain");
	if (!timer_node)
		return;

	for_each_cpu(cpu, &domain->cpus) {
		struct exynos_slack_timer *slack_timer =
			&per_cpu(exynos_slack_timer, cpu);

		/* parsing slack info */
		if (of_property_read_u32(timer_node, "expired_time", &slack_timer->expired_time)) {
			slack_timer->enabled = 0;
			break;
		}

		slack_timer->min = ULONG_MAX;

		/* Initialize slack-timer */
		timer_setup(&slack_timer->timer, slack_nop_timer, TIMER_PINNED);
	}
}

static __init int init_domain(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	unsigned int val;
	int ret;

	mutex_init(&domain->lock);

	/* Initialize frequency scaling */
	domain->max_freq = cal_dfs_get_max_freq(domain->cal_id);
	domain->min_freq = cal_dfs_get_min_freq(domain->cal_id);

	/*
	 * If max-freq property exists in device tree, max frequency is
	 * selected to smaller one between the value defined in device
	 * tree and CAL. In case of min-freq, min frequency is selected
	 * to bigger one.
	 */
	if (!of_property_read_u32(dn, "max-freq", &val))
		domain->max_freq = min(domain->max_freq, val);
	if (!of_property_read_u32(dn, "min-freq", &val))
		domain->min_freq = max(domain->min_freq, val);

	/* If this domain has boost freq, change max */
	val = exynos_pstate_get_boost_freq(cpumask_first(&domain->cpus));
	if (val > domain->max_freq)
		domain->max_freq = val;

	domain->qos_max_freq = domain->max_freq;

	if (of_property_read_bool(dn, "need-awake"))
		domain->need_awake = true;

	domain->boot_freq = cal_dfs_get_boot_freq(domain->cal_id);
	domain->resume_freq = cal_dfs_get_resume_freq(domain->cal_id);

	/* Initialize slack timer */
	init_slack_timer(domain, dn);

	ret = init_table(domain);
	if (ret)
		return ret;

	domain->old = get_freq(domain);

	/* Initialize PM QoS */
	ret = init_pm_qos(domain, dn);
	if (ret)
		return ret;

	/*
	 * Initialize CPUFreq DVFS Manager
	 * DVFS Manager is the optional function, it does not check return value
	 */
	init_dm(domain, dn);

	pr_info("Complete to initialize cpufreq-domain%d\n", domain->id);

	return ret;
}

static __init int early_init_domain(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	const char *buf;
	int ret;

	/* Initialize list head of DVFS Manager constraints */
	INIT_LIST_HEAD(&domain->dm_list);

	ret = of_property_read_u32(dn, "cal-id", &domain->cal_id);
	if (ret)
		return ret;

	/* Get size of frequency table from CAL */
	domain->table_size = cal_dfs_get_lv_num(domain->cal_id);

	/* Get cpumask which belongs to domain */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return ret;
	cpulist_parse(buf, &domain->cpus);
	cpumask_and(&domain->cpus, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&domain->cpus) == 0)
		return -ENODEV;

	return 0;
}

static __init void __free_domain(struct exynos_cpufreq_domain *domain)
{
	struct exynos_cpufreq_dm *dm;

	while (!list_empty(&domain->dm_list)) {
		dm = list_last_entry(&domain->dm_list,
				struct exynos_cpufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	kfree(domain->freq_table);
	kfree(domain);
}

static __init void free_domain(struct exynos_cpufreq_domain *domain)
{
	list_del(&domain->list);
	unregister_exynos_dm_freq_scaler(domain->dm_type);

	__free_domain(domain);
}

static __init struct exynos_cpufreq_domain *alloc_domain(struct device_node *dn)
{
	struct exynos_cpufreq_domain *domain;
	struct device_node *root, *child;

	domain = kzalloc(sizeof(struct exynos_cpufreq_domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	/*
	 * early_init_domain() initailize the domain information requisite
	 * to allocate domain and table.
	 */
	if (early_init_domain(domain, dn))
		goto free;

	/*
	 * Allocate frequency table.
	 * Last row of frequency table must be set to CPUFREQ_TABLE_END.
	 * Table size should be one larger than real table size.
	 */
	domain->freq_table =
		kzalloc(sizeof(struct cpufreq_frequency_table)
				* (domain->table_size + 1), GFP_KERNEL);
	if (!domain->freq_table)
		goto free;

	/*
	 * Allocate DVFS Manager constraints.
	 * Constraints are needed only by DVFS Manager, these are not
	 * created when DVFS Manager is disabled. If constraints does
	 * not exist, driver does scaling without DVFS Manager.
	 */
#ifndef CONFIG_EXYNOS_DVFS_MANAGER
	return domain;
#endif

	root = of_find_node_by_name(dn, "dm-constraints");
	for_each_child_of_node(root, child) {
		struct exynos_cpufreq_dm *dm;

		dm = kzalloc(sizeof(struct exynos_cpufreq_dm), GFP_KERNEL);
		if (!dm)
			goto free;

		dm->c.freq_table = kzalloc(sizeof(struct exynos_dm_freq)
					* domain->table_size, GFP_KERNEL);
		if (!dm->c.freq_table)
			goto free;

		list_add_tail(&dm->list, &domain->dm_list);
	}

	return domain;

free:
	__free_domain(domain);

	return NULL;
}

static int __init exynos_sysfs_init(void)
{
	int ret;

	ret = sysfs_create_file(power_kobj, &user_qos_max.attr);
	if (ret) {
		pr_err("failed to create usr_qos_max node\n");
		return ret;
	}

	ret = sysfs_create_file(power_kobj, &user_qos_min.attr);
	if (ret) {
		pr_err("failed to create user_qos_min\n");
		return ret;
	}

	return 0;
}

static int __init exynos_cpufreq_init(void)
{
	struct device_node *dn = NULL;
	struct exynos_cpufreq_domain *domain;
	int ret = 0;
	unsigned int domain_id = 0;

	while ((dn = of_find_node_by_type(dn, "cpufreq-domain"))) {
		domain = alloc_domain(dn);
		if (!domain) {
			pr_err("failed to allocate domain%d\n", domain_id);
			continue;
		}

		list_add_tail(&domain->list, &domains);

		domain->dn = dn;
		domain->id = domain_id++;
		ret = init_domain(domain, dn);
		if (ret) {
			pr_err("failed to initialize cpufreq domain%d\n",
							domain_id);
			free_domain(domain);
			continue;
		}

		print_domain_info(domain);
	}

	if (!domain_id) {
		pr_err("Can't find device type cpufreq-domain\n");
		return -ENODATA;
	}

	ret = cpufreq_register_driver(&exynos_driver);
	if (ret) {
		pr_err("failed to register cpufreq driver\n");
		return ret;
	}

	exynos_cpuhp_register("ACME", *cpu_online_mask, 0);

	cpufreq_register_notifier(&exynos_cpufreq_policy_notifier,
					CPUFREQ_POLICY_NOTIFIER);

	cpu_pm_register_notifier(&exynos_cpufreq_cpu_pm_notifier);

	cpuhp_setup_state_nocalls(CPUHP_AP_EXYNOS_ACME,
					"exynos:acme",
					exynos_cpufreq_cpu_up_callback,
					exynos_cpufreq_cpu_down_callback);

	register_pm_notifier(&exynos_cpufreq_pm);

	/*
	 * Enable scale of domain.
	 * Update frequency as soon as domain is enabled.
	 */
	list_for_each_entry(domain, &domains, list) {
		struct cpufreq_policy *policy;
		enable_domain(domain);
		policy = cpufreq_cpu_get_raw(cpumask_first(&domain->cpus));
		if (policy) {
#ifdef CONFIG_CPU_THERMAL
			exynos_cpufreq_cooling_register(domain->dn, policy);
#endif
			sec_pm_cpufreq_register(policy);
			slack_update_min(policy);
		}
		set_boot_qos(domain);
	}

	cpufreq_init_flag = true;

	exynos_ufc_init();
	exynos_sysfs_init();

	pr_info("Initialized Exynos cpufreq driver\n");

	return ret;
}
device_initcall(exynos_cpufreq_init);
