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
#include <linux/pm_opp.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <uapi/linux/sched/types.h>

#include <linux/ems.h>

#include <soc/samsung/cpu_cooling.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-dm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/acme.h>

#include "exynos-acme.h"

/*
 * list head of cpufreq domain
 */
static LIST_HEAD(domains);

/******************************************************************************
 *                              HELPER FUNCTION                               *
 ******************************************************************************/
static struct exynos_cpufreq_domain *find_domain(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;

	pr_err("cannot find cpufreq domain by cpu\n");
	return NULL;
}

static unsigned int resolve_freq(struct cpufreq_policy *policy,
				unsigned int target_freq, int relation)
{
	unsigned int index = -1;

	if (relation == CPUFREQ_RELATION_L)
		index = cpufreq_table_find_index_al(policy, target_freq);
	else if (relation == CPUFREQ_RELATION_H)
		index = cpufreq_table_find_index_ah(policy, target_freq);

	if (index < 0) {
		pr_err("target frequency(%d) out of range\n", target_freq);
		return 0;
	}

	return policy->freq_table[index].frequency;
}

#define check_valid_freq_range(f, min, max)	(min <= f && f <= max)

/******************************************************************************
 *                            FREQUENCY SCALING                               *
 ******************************************************************************/
static void debug_pre_scale(struct exynos_cpufreq_domain *domain,
				unsigned int target_freq)
{
	dbg_snapshot_freq(domain->dss_type, domain->old, target_freq,
							DSS_FLAG_IN);
	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_IN);
}

static void debug_post_scale(struct exynos_cpufreq_domain *domain,
				unsigned int target_freq, int scale_result)
{
	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_OUT);
	dbg_snapshot_freq(domain->dss_type, domain->old, target_freq,
			scale_result < 0 ? scale_result : DSS_FLAG_OUT);
}

static int scale_slowpath(struct exynos_cpufreq_domain *domain,
				  unsigned int target_freq,
				  unsigned int relation,
				  struct cpufreq_policy *policy)
{
	int ret = 0;
	struct cpufreq_freqs freqs = {
		.policy         =       policy,
		.old            =       domain->old,
		.new            =       target_freq,
		.flags          =       0,
	};

	mutex_lock(&domain->lock);

	/* target is same as current, skip scaling */
	if (domain->old == target_freq) {
		ret = -EINVAL;
		goto out;
	}

	cpufreq_freq_transition_begin(policy, &freqs);
	debug_pre_scale(domain, target_freq);

	if (domain->need_awake)
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	ret = cal_dfs_set_rate(domain->cal_id, target_freq);

	if (domain->need_awake)
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	debug_post_scale(domain, target_freq, ret);
	cpufreq_freq_transition_end(policy, &freqs, ret);

	if (ret < 0) {
		pr_err("failed to scale frequency of domain%d (%d -> %d)\n",
			domain->id, domain->old, target_freq);
		goto out;
	}

	pr_debug("CPUFREQ domain%d frequency change %u kHz -> %u kHz\n",
			domain->id, domain->old, target_freq);

	domain->old = target_freq;

out:
	mutex_unlock(&domain->lock);

	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
static int scale_fastpath(struct exynos_cpufreq_domain *domain,
				  unsigned int target_freq,
				  unsigned int relation)
{
	int ret = 0;

	/* target is same as current, skip scaling */
	if (domain->old == target_freq)
		return -EINVAL;

	debug_pre_scale(domain, target_freq);

	ret = cal_dfs_set_rate(domain->cal_id, target_freq);

	/*
	 * In fastpath, frequench change done event is delivered by
	 * fast_switch_notifier
	 */
	if (ret < 0) {
		pr_err("failed to scale frequency of domain%d (%d -> %d)\n",
			domain->id, domain->old, target_freq);
		return ret;
	}

	pr_debug("CPUFREQ domain%d frequency change %u kHz -> %u kHz\n",
			domain->id, domain->old, target_freq);

	return 0;
}

static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = devdata;
	struct cpufreq_policy *policy;
	int ret;

	policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
	if (!policy) {
		pr_err("%s: failed get cpufreq policy\n", __func__);
		return -ENODEV;
	}

	if (domain->fast_switch)
		ret = scale_fastpath(domain, target_freq, relation);
	else
		ret = scale_slowpath(domain, target_freq, relation, policy);

	cpufreq_cpu_put(policy);

	return ret;
}

#elif IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int duration)
{
	struct exynos_cpufreq_domain *domain = devdata;

	/* for measuring DVFS latency */
	trace_acme_scale_freq(domain->id, domain->old,
		target_freq, "end", (unsigned long)ktime_to_us(duration));

	debug_post_scale(domain, target_freq, 0);
	domain->old = target_freq;

	return 0;
}
#endif

/******************************************************************************
 *                          CPUFREQ DRIVER INTERFACE                          *
 ******************************************************************************/
static int
init_fops(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy);

static int exynos_cpufreq_init(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	if (!domain)
		return -EINVAL;

	policy->fast_switch_possible = domain->fast_switch;
	policy->freq_table = domain->freq_table;
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->dvfs_possible_from_any_cpu = true;
	cpumask_copy(policy->cpus, &domain->cpus);

	init_fops(domain, policy);

	domain->enabled = true;

	pr_info("Initialize cpufreq policy%d\n", policy->cpu);

	return 0;
}

static int exynos_cpufreq_verify(struct cpufreq_policy_data *new_policy)
{
	int policy_cpu = new_policy->cpu;
	struct exynos_cpufreq_domain *domain;
	unsigned long max_capacity, capacity;
	struct cpufreq_policy *policy;
	unsigned int min = new_policy->min, max = new_policy->max;

	domain = find_domain(policy_cpu);
	if (!domain)
		return -EINVAL;

	policy = cpufreq_cpu_get(policy_cpu);
	if (!policy)
		return -ENODATA;

	/*
	 * if min/maxfrequency is updated, find valid frequency
	 * from the table
	 */
	if (min != policy->min)
		min = resolve_freq(policy, min, CPUFREQ_RELATION_L);
	if (max != policy->max)
		max = resolve_freq(policy, max, CPUFREQ_RELATION_H);

	/* We don't want that the minimum overs the maximum anytime */
	if (min > max)
		min = max;

	new_policy->min = min;
	new_policy->max = max;

	cpufreq_cpu_put(policy);

	/* update DM and thermal pressure with new min/max */
	policy_update_call_to_DM(domain->dm_type,
			new_policy->min, new_policy->max);

	max_capacity = arch_scale_cpu_capacity(policy_cpu);
	capacity = new_policy->max * max_capacity
			/ new_policy->cpuinfo.max_freq;
	arch_set_thermal_pressure(&domain->cpus, max_capacity - capacity);

	return 0;
}


static int exynos_cpufreq_target(struct cpufreq_policy *policy,
					unsigned int target_freq,
					unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned long freq;

	if (!domain || !domain->enabled)
		return -EINVAL;

	if (list_empty(&domain->dm_list))
		return scale_slowpath(domain, target_freq, relation, policy);

	freq = (unsigned long)target_freq;

	return DM_CALL(domain->dm_type, &freq);
}

static unsigned int exynos_cpufreq_fast_switch(struct cpufreq_policy *policy,
					       unsigned int target_freq)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned long freq, flags;

	if (!domain || !domain->enabled)
		return 0;

	freq = (unsigned long)target_freq;

	raw_spin_lock_irqsave(&domain->fast_switch_lock, flags);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	debug_pre_scale(domain, target_freq);
#endif

	if (DM_CALL(domain->dm_type, &freq)) {
		/* fail to change freq */
		freq = 0;
	}

	raw_spin_unlock_irqrestore(&domain->fast_switch_lock, flags);

	return freq;
}

static unsigned int get_freq(struct exynos_cpufreq_domain *domain)
{
	int wakeup_flag = 0;
	unsigned int freq;
	struct cpumask temp;

	cpumask_and(&temp, &domain->cpus, cpu_online_mask);

	if (cpumask_empty(&temp))
		return domain->old;

	if (domain->need_awake) {
		if (likely(domain->old))
			return domain->old;

		wakeup_flag = 1;
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);
	}

	freq = (unsigned int)cal_dfs_get_rate(domain->cal_id);
	if (!freq)
		return domain->old;

	if (unlikely(wakeup_flag))
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	return freq;
}

static unsigned int exynos_cpufreq_get(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain = find_domain(cpu);

	if (!domain)
		return 0;

	return get_freq(domain);
}

static int __exynos_cpufreq_suspend(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	unsigned int freq;
	struct work_struct *update_work = &policy->update;

	if (!domain)
		return 0;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq = domain->resume_freq;

	freq_qos_update_request(policy->min_freq_req, freq);
	freq_qos_update_request(policy->max_freq_req, freq);

	flush_work(update_work);

	return 0;
}

static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_suspend(policy, find_domain(policy->cpu));
}

static int __exynos_cpufreq_resume(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	if (!domain)
		return -EINVAL;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq_qos_update_request(policy->max_freq_req, domain->max_freq);
	freq_qos_update_request(policy->min_freq_req, domain->min_freq);

	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_resume(policy, find_domain(policy->cpu));
}

static int exynos_cpufreq_online(struct cpufreq_policy *policy)
{
	/* nothing to do */
	return 0;
}

static int exynos_cpufreq_offline(struct cpufreq_policy *policy)
{
	/* nothing to do */
	return 0;
}

static struct cpufreq_driver exynos_driver = {
	.name		= "exynos_cpufreq",
	.flags		= CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init		= exynos_cpufreq_init,
	.verify		= exynos_cpufreq_verify,
	.target		= exynos_cpufreq_target,
	.get		= exynos_cpufreq_get,
	.fast_switch	= exynos_cpufreq_fast_switch,
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
	.online		= exynos_cpufreq_online,
	.offline	= exynos_cpufreq_offline,
	.attr		= cpufreq_generic_attr,
};

/******************************************************************************
 *                               FREQ QOS HANDLER                             *
 ******************************************************************************/
static int exynos_cpufreq_update_limit(struct cpufreq_policy *policy)
{
	bool own;
	unsigned long owner = atomic_long_read(&policy->rwsem.owner);

	owner = owner & ~0x7;
	own = (struct task_struct *)owner == current;

	if (own)
		refresh_frequency_limits(policy);
	else {
		down_write(&policy->rwsem);
		refresh_frequency_limits(policy);
		up_write(&policy->rwsem);
	}

	return 0;
}

static int exynos_cpufreq_notifier_min(struct notifier_block *nb,
				       unsigned long freq,
				       void *data)
{
	struct cpufreq_policy *policy =
			container_of(nb, struct cpufreq_policy, nb_min);

	return exynos_cpufreq_update_limit(policy);
}

static int exynos_cpufreq_notifier_max(struct notifier_block *nb,
				       unsigned long freq,
				       void *data)
{
	struct cpufreq_policy *policy =
			container_of(nb, struct cpufreq_policy, nb_max);

	return exynos_cpufreq_update_limit(policy);
}

/******************************************************************************
 *                               CPUFREQ DEV FOPS                             *
 ******************************************************************************/
static ssize_t cpufreq_fops_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct freq_qos_request *req = filp->private_data;

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	freq_qos_update_request(req, value);

	return count;
}

static ssize_t cpufreq_fops_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value = 0;

	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static int cpufreq_fops_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct exynos_cpufreq_file_operations *fops = container_of(filp->f_op,
			struct exynos_cpufreq_file_operations, fops);
	struct freq_qos_request *req = kzalloc(sizeof(*req), GFP_KERNEL);

	if (!req)
		return -ENOMEM;

	filp->private_data = req;
	ret = freq_qos_tracer_add_request(fops->freq_constraints, req,
					fops->req_type, fops->default_value);
	if (ret)
		return ret;

	return 0;
}

static int cpufreq_fops_release(struct inode *inode, struct file *filp)
{
	struct freq_qos_request *req = filp->private_data;

	freq_qos_tracer_remove_request(req);
	kfree(req);

	return 0;
}

static int
init_fops(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy)
{
	char *node_name_buffer;
	int ret, buffer_size;;

	buffer_size = sizeof(char [64]);
	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_min", domain->id);

	domain->min_qos_fops.fops.write		= cpufreq_fops_write;
	domain->min_qos_fops.fops.read		= cpufreq_fops_read;
	domain->min_qos_fops.fops.open		= cpufreq_fops_open;
	domain->min_qos_fops.fops.release	= cpufreq_fops_release;
	domain->min_qos_fops.fops.llseek	= noop_llseek;

	domain->min_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->min_qos_fops.miscdev.name	= node_name_buffer;
	domain->min_qos_fops.miscdev.fops	= &domain->min_qos_fops.fops;

	domain->min_qos_fops.freq_constraints	= &policy->constraints;
	domain->min_qos_fops.default_value	= FREQ_QOS_MIN_DEFAULT_VALUE;
	domain->min_qos_fops.req_type		= FREQ_QOS_MIN;

	ret = misc_register(&domain->min_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device min for domain %d",
								domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_max", domain->id);

	domain->max_qos_fops.fops.write		= cpufreq_fops_write;
	domain->max_qos_fops.fops.read		= cpufreq_fops_read;
	domain->max_qos_fops.fops.open		= cpufreq_fops_open;
	domain->max_qos_fops.fops.release	= cpufreq_fops_release;
	domain->max_qos_fops.fops.llseek	= noop_llseek;

	domain->max_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->max_qos_fops.miscdev.name	= node_name_buffer;
	domain->max_qos_fops.miscdev.fops	= &domain->max_qos_fops.fops;

	domain->max_qos_fops.freq_constraints	= &policy->constraints;
	domain->max_qos_fops.default_value	= FREQ_QOS_MAX_DEFAULT_VALUE;
	domain->max_qos_fops.req_type		= FREQ_QOS_MAX;

	ret = misc_register(&domain->max_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device max for domain %d",
								domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	return 0;
}

/******************************************************************************
 *                                 DVFS MANAGER                               *
 ******************************************************************************/
struct dm_work {
	struct work_struct work;
	int slave_type;
	int table_index;
} dm_work;

static void change_dm_table_work(struct work_struct *work)
{
	struct exynos_cpufreq_domain *domain;
	struct exynos_cpufreq_dm *dm;
	int slave_type = dm_work.slave_type;
	int new_index = dm_work.table_index;

	list_for_each_entry(domain, &domains, list) {
		list_for_each_entry(dm, &domain->dm_list, list) {
			if (dm->c.dm_slave != slave_type)
				continue;

			if (!dm->c.support_variable_freq_table)
				continue;

			if (dm->cur_table_index == new_index)
				continue;

			exynos_dm_change_freq_table(&dm->c, new_index);

			dm->cur_table_index = new_index;
		}
	}
}

static void change_dm_table(int type, int index)
{
	if (dm_work.table_index == index)
		return;

	dm_work.slave_type = type;
	dm_work.table_index = index;

	schedule_work(&dm_work.work);
}

static int
init_constraint_table_ect(struct exynos_dm_freq *dm_table, int table_length,
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

	for (index = 0; index < table_length; index++) {
		unsigned int freq = dm_table[index].master_freq;

		for (c_index = 0; c_index < ect_domain->num_of_level; c_index++) {
			/* find row same as frequency */
			if (freq == ect_domain->level[c_index].main_frequencies) {
				dm_table[index].slave_freq
					= ect_domain->level[c_index].sub_frequencies;
				valid_row = true;
				break;
			}
		}

		/*
		 * Due to higher levels of constraint_freq should not be NULL,
		 * they should be filled with highest value of sub_frequencies
		 * of ect until finding first(highest) domain frequency fit with
		 * main_frequeucy of ect.
		 */
		if (!valid_row)
			dm_table[index].slave_freq
				= ect_domain->level[0].sub_frequencies;
	}

	return 0;
}

static int
init_constraint_table_dt(struct exynos_dm_freq *dm_table, int table_length,
						struct device_node *dn)
{
	struct exynos_dm_freq *table;
	int size, table_size, index, c_index;

	/*
	 * A DM constraint table row consists of master and slave frequency
	 * value, the size of a row is 64bytes. Divide size in half when
	 * table is allocated.
	 */
	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	table_size = size / 2;
	table = kzalloc(sizeof(struct exynos_dm_freq) * table_size, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	of_property_read_u32_array(dn, "table", (unsigned int *)table, size);

	for (index = 0; index < table_length; index++) {
		unsigned int freq = dm_table[index].master_freq;

		for (c_index = 0; c_index < table_size; c_index++) {
			if (freq <= table[c_index].master_freq) {
				dm_table[index].slave_freq
					= table[c_index].slave_freq;
				break;
			}
		}
	}

	kfree(table);

	return 0;
}

static struct exynos_dm_freq *alloc_dm_table(struct exynos_cpufreq_domain *domain)
{
	struct exynos_dm_freq *dm_table;
	int index, r_index;

	/* allocate DM constraint table */
	dm_table = kcalloc(domain->table_size,
			sizeof(struct exynos_dm_freq), GFP_KERNEL);
	if (!dm_table)
		return NULL;

	/*
	 * fill master freq, domain frequency table is in ascending
	 * order, but DM constraint table must be in descending
	 * order.
	 */
	index = 0;
	r_index = domain->table_size - 1;
	while (r_index >= 0) {
		dm_table[index].master_freq =
			domain->freq_table[r_index].frequency;
		index++;
		r_index--;
	}

	return dm_table;
}

#define DM_DSU	0x7
static int init_dm_constraint(struct exynos_cpufreq_domain *domain,
			struct device_node *dn, struct exynos_dm_constraint *c)
{
	struct exynos_dm_freq *dm_table;
	struct device_node *child;
	int index = 0, num_freq_table = 0;

	/* support to disable DM constraint at runtime */
	if (of_property_read_bool(dn, "dynamic-disable"))
		c->support_dynamic_disable = true;

	/* guidance indicates H/W constraint */
	if (of_property_read_bool(dn, "guidance"))
		c->guidance = true;

	num_freq_table = of_get_child_count(dn);

	/*if (num_freq_table > 1) {
		c->support_variable_freq_table = true;
		c->num_table_index = num_freq_table;
		c->variable_freq_table = kcalloc(num_freq_table,
				sizeof(struct exynos_dm_freq *), GFP_KERNEL);
	}*/

	c->table_length = domain->table_size;

	for_each_child_of_node(dn, child) {
		dm_table = alloc_dm_table(domain);
		if (!dm_table)
			return -ENOMEM;

		if (c->guidance) {
			if (init_constraint_table_ect(dm_table,
					domain->table_size, child))
				return -ENOMEM;
		} else {
			if (init_constraint_table_dt(dm_table,
					domain->table_size, child))
				return -ENOMEM;
		}

		if (c->support_variable_freq_table) {
			c->variable_freq_table[index] = dm_table;
			index++;
		} else
			c->freq_table = dm_table;

		/*if (c->dm_slave == DM_DSU) {
			et_register_dsu_constraint(cpumask_any(&domain->cpus),
					dm_table, domain->table_size);
		}*/
	}

	return 0;
}

static int init_dm(struct exynos_cpufreq_domain *domain,
				struct device_node *dn)
{
	struct exynos_cpufreq_dm *dm;
	struct device_node *root;
	struct of_phandle_iterator iter;
	int cur_freq;
	int ret, err;

	ret = of_property_read_u32(dn, "dm-type", &domain->dm_type);
	if (ret)
		return ret;

	/*
	 * If cpus of domain are offline, initialize cur_freq of dm_data
	 * with min_freq to avoid freq/volt constraint with other
	 * freq/voltage domain.
	 */
	if (cpumask_intersects(&domain->cpus, cpu_online_mask))
		cur_freq = exynos_cpufreq_get(cpumask_any(&domain->cpus));
	else
		cur_freq = domain->min_freq;

	ret = exynos_dm_data_init(domain->dm_type, domain, domain->min_freq,
				domain->max_freq, cur_freq);
	if (ret)
		return ret;

	/* Initialize list head of DVFS Manager constraints */
	INIT_LIST_HEAD(&domain->dm_list);

	/*
	 * Initialize DVFS Manager constraints
	 * - constraint_type : minimum or maximum constraint
	 * - constraint_dm_type : cpu/mif/int/.. etc
	 * - guidance : constraint from chipset characteristic
	 * - freq_table : constraint table
	 */
	root = of_get_child_by_name(dn, "dm-constraints");
	of_for_each_phandle(&iter, err, root, "list", NULL, 0) {

		/* allocate DM constraint */
		dm = kzalloc(sizeof(struct exynos_cpufreq_dm), GFP_KERNEL);
		if (!dm)
			goto init_fail;

		list_add_tail(&dm->list, &domain->dm_list);

		of_property_read_u32(iter.node, "const-type",
					&dm->c.constraint_type);
		of_property_read_u32(iter.node, "dm-slave",
					&dm->c.dm_slave);
		of_property_read_u32(iter.node, "master-cal-id",
					&dm->master_cal_id);
		of_property_read_u32(iter.node, "slave-cal-id",
					&dm->slave_cal_id);

		/* initialize DM constraint table */
		if (init_dm_constraint(domain, iter.node, &dm->c))
			goto init_fail;

		/* register DM constraint */
		ret = register_exynos_dm_constraint_table(domain->dm_type,
								&dm->c);
		if (ret)
			goto init_fail;
	}

	return register_exynos_dm_freq_scaler(domain->dm_type, dm_scaler);

init_fail:
	while (!list_empty(&domain->dm_list)) {
		dm = list_last_entry(&domain->dm_list,
				struct exynos_cpufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SEC_BOOTSTAT)
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

		freq[cluster] = get_freq(domain);
	}
	put_online_cpus();
}
EXPORT_SYMBOL(sec_bootstat_get_cpuinfo);
#endif

/******************************************************************************
 *                             CPU HOTPLUG CALLBACK                           *
 ******************************************************************************/
static int exynos_cpufreq_cpu_up_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed before cpufreq_resume() is called.
	 * Therefore, if it is called by enable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(cpu);
	if (!domain)
		return 0;

	/*
	 * The first incoming cpu in domain enables frequency scaling
	 * and clears limit of frequency.
	 */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&mask) == 1) {
		domain->enabled = true;
		freq_qos_update_request(&domain->max_qos_req, domain->max_freq);
	}

	return 0;
}

static int exynos_cpufreq_cpu_down_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed after cpufreq_suspend() is called.
	 * Therefore, if it is called by disable_nonboot_cpus(),
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
		freq_qos_update_request(&domain->max_qos_req, domain->min_freq);
		domain->enabled = false;
	}

	return 0;
}

/******************************************************************************
 *                               NOTIFIER HANDLER                             *
 ******************************************************************************/
/*#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
static int exynos_cpufreq_fast_switch_notifier(struct notifier_block *notifier,
				       unsigned long domain_id, void *data)
{
	struct exynos_cpufreq_domain *domain = NULL;
	unsigned int freq = ((struct exynos_dm_fast_switch_notify_data *)data)->freq;

	list_for_each_entry(domain, &domains, list)
		if ((domain->cal_id & 0xffff) == domain_id)
			break;

	if (unlikely(!domain))
		return NOTIFY_DONE;

	if (!domain->fast_switch)
		return NOTIFY_BAD;

	debug_post_scale(domain, freq, 0);
	domain->old = freq;

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_fast_switch_nb = {
	.notifier_call = exynos_cpufreq_fast_switch_notifier,
};
#endif*/

static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct exynos_cpufreq_domain *domain;
	struct cpufreq_policy *policy;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		list_for_each_entry_reverse(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_suspend(policy, domain)) {
				cpufreq_cpu_put(policy);
				return NOTIFY_BAD;
			}
			cpufreq_cpu_put(policy);
		}
		break;
	case PM_POST_SUSPEND:
		list_for_each_entry(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_resume(policy, domain)) {
				cpufreq_cpu_put(policy);
				return NOTIFY_BAD;
			}
			cpufreq_cpu_put(policy);
		}
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_pm = {
	.notifier_call = exynos_cpufreq_pm_notifier,
	.priority = INT_MAX,
};

static int exynos_cpufreq_emstune_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	change_dm_table(DM_DSU, emstune_cpu_dsu_table_index(v));

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_emstune_nb = {
	.notifier_call = exynos_cpufreq_emstune_notifier,
};

static int init_freq_qos(struct exynos_cpufreq_domain *domain,
				struct cpufreq_policy *policy);
static int exynos_cpufreq_policy_notifier(struct notifier_block *nb,
					       unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct exynos_cpufreq_domain *domain;
	int ret;

	if (val != CPUFREQ_CREATE_POLICY)
		return NOTIFY_OK;

	domain = find_domain(policy->cpu);
	if (!domain)
		return NOTIFY_BAD;

#if IS_ENABLED(CONFIG_EXYNOS_CPU_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_CPU_THERMAL_MODULE)
	exynos_cpufreq_cooling_register(domain->dn, policy);
#endif

	ret = init_freq_qos(domain, policy);
	if (ret) {
		pr_info("failed to init pm_qos with err %d\n", ret);
		return ret;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_policy_nb = {
	.notifier_call = exynos_cpufreq_policy_notifier,
	.priority = INT_MIN,
};

/******************************************************************************
 *                               SYSFS HANDLER                                *
 ******************************************************************************/
#define show_store_freq_qos(type)					\
static ssize_t show_freq_qos_##type(struct device *dev,			\
		struct device_attribute *attr, char *buf)		\
{									\
	ssize_t count = 0;						\
	struct exynos_cpufreq_domain *domain;				\
									\
	list_for_each_entry(domain, &domains, list)			\
		count += snprintf(buf + count, 30,			\
			"policy%d: qos_%s: %d\n",			\
			cpumask_first(&domain->cpus), #type,		\
			domain->user_##type##_qos_req.pnode.prio);	\
									\
	return count;							\
}									\
									\
static ssize_t store_freq_qos_##type(struct device *dev,		\
				struct device_attribute *attr,		\
				const char *buf, size_t count)		\
{									\
	int freq, cpu;							\
	struct exynos_cpufreq_domain *domain;				\
									\
	if (!sscanf(buf, "%d %8d", &cpu, &freq))			\
		return -EINVAL;						\
									\
	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)			\
		return -EINVAL;						\
									\
	domain = find_domain(cpu);					\
	if (!domain)							\
		return -EINVAL;						\
									\
	freq_qos_update_request(&domain->user_##type##_qos_req, freq);	\
									\
	return count;							\
}

show_store_freq_qos(min);
show_store_freq_qos(max);

static DEVICE_ATTR(freq_qos_max, S_IRUGO | S_IWUSR,
		show_freq_qos_max, store_freq_qos_max);
static DEVICE_ATTR(freq_qos_min, S_IRUGO | S_IWUSR,
		show_freq_qos_min, store_freq_qos_min);

static void init_sysfs(struct kobject *kobj)
{
	if (sysfs_create_file(kobj, &dev_attr_freq_qos_max.attr))
		pr_err("failed to create user_max node\n");

	if (sysfs_create_file(kobj, &dev_attr_freq_qos_min.attr))
		pr_err("failed to create user_min node\n");
}

/******************************************************************************
 *                            DRIVER INITIALIZATION                           *
 ******************************************************************************/
static void print_domain_info(struct exynos_cpufreq_domain *domain)
{
	int i;
	char buf[10];

	pr_info("CPUFREQ of domain%d cal-id : %#x\n",
			domain->id, domain->cal_id);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&domain->cpus));
	pr_info("CPUFREQ of domain%d sibling cpus : %s\n",
			domain->id, buf);

	pr_info("CPUFREQ of domain%d boot freq = %d kHz resume freq = %d kHz\n",
			domain->id, domain->boot_freq, domain->resume_freq);

	pr_info("CPUFREQ of domain%d max freq : %d kHz, min freq : %d kHz\n",
			domain->id,
			domain->max_freq, domain->min_freq);

	pr_info("CPUFREQ of domain%d table size = %d\n",
			domain->id, domain->table_size);

	for (i = 0; i < domain->table_size; i++) {
		if (domain->freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		pr_info("CPUFREQ of domain%d : L%-2d  %7d kHz\n",
			domain->id,
			domain->freq_table[i].driver_data,
			domain->freq_table[i].frequency);
	}
}

static void freq_qos_release(struct work_struct *work)
{
	struct exynos_cpufreq_domain *domain =
				container_of(to_delayed_work(work),
				struct exynos_cpufreq_domain,
				work);

	freq_qos_update_request(&domain->min_qos_req, domain->min_freq);
	freq_qos_update_request(&domain->max_qos_req, domain->max_freq);
}

static int init_freq_qos(struct exynos_cpufreq_domain *domain,
				struct cpufreq_policy *policy)
{
	unsigned int boot_qos, val;
	struct device_node *dn = domain->dn;
	int ret;
	unsigned long boost_duration;

	/* replace freq qos notifier with ACME's one */
	policy->nb_min.notifier_call = exynos_cpufreq_notifier_min;
	policy->nb_max.notifier_call = exynos_cpufreq_notifier_max;

	ret = freq_qos_tracer_add_request(&policy->constraints,
				&domain->min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints,
				&domain->max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints,
				&domain->user_min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints,
				&domain->user_max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	/* boost_duration = 40s - now */
	boost_duration = ktime_to_ms(ktime_sub(ktime_set(40, 0), ktime_get()));
	if (boost_duration < 0)
		return 0;

	/*
	 * Booting boost is set to smaller of max frequency of domain
	 * and pm_qos-booting value defined in device tree.
	 */
	boot_qos = domain->max_freq;
	if (!of_property_read_u32(dn, "pm_qos-booting", &val))
		boot_qos = min(boot_qos, val);

	freq_qos_update_request(&domain->min_qos_req, boot_qos);
	freq_qos_update_request(&domain->max_qos_req, boot_qos);
	pr_info("domain%d operates at %dKHz for %lumsecs\n",
				domain->id, boot_qos, boost_duration);

	INIT_DELAYED_WORK(&domain->work, freq_qos_release);
	schedule_delayed_work(&domain->work, msecs_to_jiffies(boost_duration));

	return 0;
}

static int init_fast_switch(struct exynos_cpufreq_domain *domain,
			    struct device_node *dn)
{
	domain->fast_switch = of_property_read_bool(dn, "fast-switch");
	if (!domain->fast_switch)
		return 0;

	raw_spin_lock_init(&domain->fast_switch_lock);

	return 0;
}

static void adjust_freq_table_with_min_max_freq(unsigned int *freq_table,
			int table_size, struct exynos_cpufreq_domain *domain)
{
	int index;

	for (index = 0; index < table_size; index++) {
		if (freq_table[index] == domain->min_freq)
			break;

		if (freq_table[index] > domain->min_freq) {
			freq_table[index - 1] = domain->min_freq;
			break;
		}
	}

	for (index = table_size - 1; index >= 0; index--) {
		if (freq_table[index] == domain->max_freq)
			break;

		if (freq_table[index] < domain->max_freq) {
			if (domain->max_freq - freq_table[index] < FREQ_MERGE_THRESHOLD) {
				freq_table[index] = domain->max_freq;
				break;
			}

			freq_table[index + 1] = domain->max_freq;
			break;
		}
	}
}

struct freq_volt {
	unsigned int freq;
	unsigned int volt;
};

static int init_domain(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	unsigned int val, raw_table_size;
	int index, i;
	unsigned int freq_table[100];
	struct freq_volt *fv_table;
	const char *buf;
	int cpu;
	int ret;

	/*
	 * Get cpumask which belongs to domain.
	 */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return ret;

	cpulist_parse(buf, &domain->cpus);
	cpumask_and(&domain->cpus, &domain->cpus, cpu_possible_mask);
	if (cpumask_weight(&domain->cpus) == 0)
		return -ENODEV;

	/* Get CAL ID */
	ret = of_property_read_u32(dn, "cal-id", &domain->cal_id);
	if (ret)
		return ret;

	/* Get DSS type */
	if (of_property_read_u32(dn, "dss-type", &domain->dss_type)) {
		pr_info("%s:dss_type is not initialized. domain_id replaces it.",
									__func__);
		domain->dss_type = domain->id;
	}

	if (of_property_read_bool(dn, "need-awake"))
		domain->need_awake = true;

	/*
	 * Set min/max frequency.
	 * If max-freq property exists in device tree, max frequency is
	 * selected to smaller one between the value defined in device
	 * tree and CAL. In case of min-freq, min frequency is selected
	 * to bigger one.
	 */
	domain->max_freq = cal_dfs_get_max_freq(domain->cal_id);
	domain->min_freq = cal_dfs_get_min_freq(domain->cal_id);

	if (!of_property_read_u32(dn, "max-freq", &val))
		domain->max_freq = min(domain->max_freq, val);
	if (!of_property_read_u32(dn, "min-freq", &val))
		domain->min_freq = max(domain->min_freq, val);

	/* Get freq-table from device tree and cut the out of range */
	raw_table_size = of_property_count_u32_elems(dn, "freq-table");
	if (of_property_read_u32_array(dn, "freq-table",
				freq_table, raw_table_size)) {
		pr_err("%s: freq-table does not exist\n", __func__);
		return -ENODATA;
	}

	/*
	 * Min/max freq must exist in freq table. If it does not exist,
	 * the freq close to min/max freq is replaced with min/max freq.
	 * Replaced freq is as below:
	 *  - min freq : lowest freq higher than given min freq
	 *  - max freq : highest freq lower than given max freq
	 */
	adjust_freq_table_with_min_max_freq(freq_table, raw_table_size, domain);

	domain->table_size = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] > domain->max_freq ||
		    freq_table[index] < domain->min_freq) {
			freq_table[index] = CPUFREQ_ENTRY_INVALID;
			continue;
		}

		domain->table_size++;
	}

	/*
	 * Get volt table from CAL with given freq-table
	 * cal_dfs_get_freq_volt_table() is called by filling the desired
	 * frequency in fv_table, the corresponding volt is filled.
	 */
	fv_table = kzalloc(sizeof(struct freq_volt)
				* (domain->table_size), GFP_KERNEL);
	if (!fv_table) {
		pr_err("%s: failed to alloc fv_table\n", __func__);
		return -ENOMEM;
	}

	i = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] == CPUFREQ_ENTRY_INVALID)
			continue;
		fv_table[i].freq = freq_table[index];
		i++;
	}

	if (cal_dfs_get_freq_volt_table(domain->cal_id,
				fv_table, domain->table_size)) {
		pr_err("%s: failed to get fv table from CAL\n", __func__);
		kfree(fv_table);
		return -EINVAL;
	}

	/*
	 * Allocate and initialize frequency table.
	 * Last row of frequency table must be set to CPUFREQ_TABLE_END.
	 * Table size should be one larger than real table size.
	 */
	domain->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
				* (domain->table_size + 1), GFP_KERNEL);
	if (!domain->freq_table) {
		kfree(fv_table);
		return -ENOMEM;
	}

	for (index = 0; index < domain->table_size; index++) {
		domain->freq_table[index].driver_data = index;
		domain->freq_table[index].frequency = fv_table[index].freq;
	}
	domain->freq_table[index].driver_data = index;
	domain->freq_table[index].frequency = CPUFREQ_TABLE_END;

	/*
	 * Add OPP table for thermal.
	 * Thermal CPU cooling is based on the OPP table.
	 */
	for (index = domain->table_size - 1; index >= 0; index--) {
		for_each_cpu_and(cpu, &domain->cpus, cpu_possible_mask)
			dev_pm_opp_add(get_cpu_device(cpu),
				fv_table[index].freq * 1000,
				fv_table[index].volt);
	}

	kfree(fv_table);

	/*
	 * Initialize boot and resume freq.
	 * If cal returns invalid freq, set as min freq for workaround.
	 */
	domain->boot_freq = cal_dfs_get_boot_freq(domain->cal_id);
	if (!check_valid_freq_range(domain->boot_freq,
			domain->min_freq, domain->max_freq))
		domain->boot_freq = domain->min_freq;

	domain->resume_freq = cal_dfs_get_resume_freq(domain->cal_id);
	if (!check_valid_freq_range(domain->resume_freq,
			domain->min_freq, domain->max_freq))
		domain->resume_freq = domain->min_freq;

	domain->old = get_freq(domain);

	mutex_init(&domain->lock);

	/*
	 * Initialize CPUFreq DVFS Manager
	 * DVFS Manager is the optional function, it does not check return value
	 */
	init_dm(domain, dn);

	/* Register EM to device of CPU in this domain */
	cpu = cpumask_first(&domain->cpus);
	dev_pm_opp_of_register_em(get_cpu_device(cpu), &domain->cpus);

	/* Initialize fields to test fast switch */
	init_fast_switch(domain, dn);

	pr_info("Complete to initialize cpufreq-domain%d\n", domain->id);

	return 0;
}

static int exynos_cpufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn;
	struct exynos_cpufreq_domain *domain;
	unsigned int domain_id = 0;
	int ret = 0;

	/*
	 * Pre-initialization.
	 * allocate and initialize cpufreq domain
	 */
	for_each_child_of_node(pdev->dev.of_node, dn) {
		domain = kzalloc(sizeof(struct exynos_cpufreq_domain),
							GFP_KERNEL);
		if (!domain) {
			pr_err("failed to allocate domain%d\n", domain_id);
			return -ENOMEM;
		}

		domain->id = domain_id++;
		if (init_domain(domain, dn)) {
			pr_err("failed to initialize cpufreq domain%d\n",
							domain->id);
			kfree(domain->freq_table);
			kfree(domain);
			continue;
		}

		domain->dn = dn;
		list_add_tail(&domain->list, &domains);

		print_domain_info(domain);
	}

	if (!domain_id) {
		pr_err("Failed to initialize cpufreq driver\n");
		return -ENOMEM;
	}

	cpufreq_register_notifier(&exynos_cpufreq_policy_nb,
					CPUFREQ_POLICY_NOTIFIER);

	/* Register cpufreq driver */
	ret = cpufreq_register_driver(&exynos_driver);
	if (ret) {
		pr_err("failed to register cpufreq driver\n");
		return ret;
	}

	/*
	 * Post-initialization
	 */
	init_sysfs(&pdev->dev.kobj);

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "exynos:acme",
				exynos_cpufreq_cpu_up_callback,
				exynos_cpufreq_cpu_down_callback);

/*#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	exynos_dm_fast_switch_notifier_register(&exynos_cpufreq_fast_switch_nb);
#endif*/

	register_pm_notifier(&exynos_cpufreq_pm);

	emstune_register_notifier(&exynos_cpufreq_emstune_nb);
	INIT_WORK(&dm_work.work, change_dm_table_work);

	pr_info("Initialized Exynos cpufreq driver\n");

	return ret;
}

static const struct of_device_id of_exynos_cpufreq_match[] = {
	{ .compatible = "samsung,exynos-acme", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_cpufreq_match);

static struct platform_driver exynos_cpufreq_driver = {
	.driver = {
		.name	= "exynos-acme",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_cpufreq_match,
	},
	.probe		= exynos_cpufreq_probe,
};

module_platform_driver(exynos_cpufreq_driver);

MODULE_DESCRIPTION("Exynos ACME");
MODULE_LICENSE("GPL");
