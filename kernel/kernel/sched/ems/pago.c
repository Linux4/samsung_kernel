/*
 * PAGO(PMU based Advanced memory GOvernor) memory latency governor.
 * Copyright (C) 2023, Samsung Electronic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>

#include "ems.h"
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/exynos-dsufreq.h>

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#define DEFAULT_UPDATE_MS (8)

enum {
	CYCLE,
	INSTR,
	L3_REFILL,
	MAX_PMU_EVENTS,
};

static int pmu_event_id[MAX_PMU_EVENTS] = {
	ARMV8_PMUV3_PERFCTR_CPU_CYCLES,
	ARMV8_PMUV3_PERFCTR_INST_RETIRED,
	ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL,
};

struct boost_request {
	bool			activated;
	int			level;

	ktime_t			activated_ts;
};

struct cpu_mem_map {
	unsigned int cpufreq;
	unsigned int memfreq;
};

struct pago_cpu {
	struct pago_domain 	**domains;
	int			nr_domains;

	int			cpu;
	bool			cpu_up;
	bool			enabled;
	bool			idle_sample;
	unsigned int		cycle_freq;
	unsigned int		*target_freq;
	unsigned int		*down_step;

	ktime_t			last_sampled;
	ktime_t			last_updated;
	raw_spinlock_t		lock;

	/* pmu event data */
	struct perf_event	*pe[MAX_PMU_EVENTS];
	u64			curr[MAX_PMU_EVENTS];
	u64			prev[MAX_PMU_EVENTS];
	s64			delta[MAX_PMU_EVENTS];
};

struct pago_domain {
	struct pago_group	*grp;
	struct cpumask		cpus;
	struct kobject		kobj;

	struct cpu_mem_map	*freq_table;
	unsigned int		table_size;

	bool			perf;
	unsigned int		mpki_threshold;
};

struct pago_group {
	struct kobject			kobj;
	const char			*name;

	int				miss_idx;

	struct pago_domain		**domains;
	int				nr_domains;

	struct exynos_pm_qos_request	pm_qos;
	unsigned int			pm_qos_class;

	unsigned int			boost_min;
	unsigned int			boost_threshold;
	unsigned int			force_boost_ms;

	unsigned int			max_mem_freq;
	unsigned int			max_boost_freq;
	unsigned int			max_target_freq;
	unsigned int			max_perf_cycle_freq;

	unsigned int			resolved_freq;

	unsigned int			*ftable;
	int				ftable_size;
};

struct pago {
	struct kobject			*kobj;
	struct cpumask			thread_allowed_cpus;
	struct task_struct		*thread;
	struct kthread_worker		worker;
	struct kthread_work		work;
	struct kthread_work		boost_work;
	struct irq_work			irq_work;
	struct irq_work			irq_boost_work;

	struct pago_group		**groups;
	int				nr_groups;

	struct boost_request		emstune;

	ktime_t				update_period;
	ktime_t				last_updated;
	ktime_t				last_jiffies;

	raw_spinlock_t			lock;

	bool				enable;
};


static struct pago *pago;
static DEFINE_PER_CPU(struct pago_cpu, pago_cpu);
static DEFINE_PER_CPU(unsigned int, cpu_old_freq);

static int cpuhp_state;

static u64 read_pmu_events(struct pago_cpu *data, int idx)
{
	u64 ret = 0;

	perf_event_read_local(data->pe[idx], &ret, NULL, NULL);

	return ret;
}

static void deactivate_pmu_events(struct pago_cpu *data)
{
	int i;

	for (i = 0; i < MAX_PMU_EVENTS; i++) {
		if (data->pe[i]) {
			exynos_perf_event_release_kernel(data->pe[i]);
			data->pe[i] = NULL;
		}
	}
}

static int activate_pmu_events(struct pago_cpu *data)
{
	struct perf_event *pe;
	struct perf_event_attr *pe_attr;
	int i;

	pe_attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!pe_attr) {
		pr_err("failed to allocate perf event attr\n");
		return -ENOMEM;
	}

	pe_attr->size = (u32)sizeof(struct perf_event_attr);
	pe_attr->type = PERF_TYPE_RAW;
	pe_attr->pinned = 1;

	/* enable 64-bit counter */
	pe_attr->config1 = 1;

	for (i = 0; i < MAX_PMU_EVENTS; i++) {
		pe_attr->config = pmu_event_id[i];
#if IS_ENABLED(CONFIG_SOC_S5E9945) || IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
		/* Root hunter cpu errata: L3D_CACHE_REFILL is incorrect
		 * use LL_CACHE_RD
		 */
		if (i == L3_REFILL && data->cpu >= 4 && data->cpu <= 8) {
			pe_attr->config = ARMV8_PMUV3_PERFCTR_LL_CACHE_RD;
		}
#endif
		pe = exynos_perf_create_kernel_counter(pe_attr, data->cpu, NULL, NULL, NULL);
		if (!pe) {
			pr_err("failed to create kernel perf event. CPU=%d\n", data->cpu);
			kfree(pe_attr);
			return -ENOMEM;
		}

		data->pe[i] = pe;
	}

	kfree(pe_attr);

	return 0;
}

static void lock_all_cpus(void)
{
	struct pago_cpu *data;
	int cpu, sub_class = 0;

	for_each_possible_cpu(cpu) {
		data = &per_cpu(pago_cpu, cpu);
		if (sub_class == 0)
			raw_spin_lock(&data->lock);
		else
			raw_spin_lock_nested(&data->lock, sub_class++);
	}
}

static void unlock_all_cpus(void)
{
	struct pago_cpu *data;
	int cpu;

	for_each_possible_cpu(cpu) {
		data = &per_cpu(pago_cpu, cpu);
		raw_spin_unlock(&data->lock);
	}
}

static unsigned int cpufreq_to_memfreq(unsigned int scale_freq, struct pago_domain *dom)
{
	struct cpu_mem_map *table = dom->freq_table;
	unsigned int mem_freq = 0;
	int i;

	if (!table)
		goto out;

	for (i = 0; i < dom->table_size; i++) {
		if (scale_freq < table[i].cpufreq)
                        break;
		else if (dom->grp->pm_qos_class && scale_freq == table[i].cpufreq)
                        break;

                mem_freq = table[i].memfreq;
	}

out:
	return mem_freq;
}

int get_freq_idx(struct pago_group *grp, unsigned int mem_freq)
{
	int i = 0;

	if (grp->ftable == NULL)
		return -1;

	for (i = 0; i < grp->ftable_size; i++) {
		if (grp->ftable[i] == mem_freq)
			return i;
	}

	return -1;
}

static void pago_calc_target_freq(ktime_t now)
{
	struct boost_request *emstune_boost = &pago->emstune;
	struct pago_cpu *data;
	int i, cpu;
	unsigned long flags;
	s64 delta, update_delta;

	local_irq_save(flags);
	lock_all_cpus();

	update_delta = ktime_us_delta(now, pago->last_updated);

	/* update pmu counter */
	for_each_possible_cpu(cpu) {
		unsigned int cur_freq;
		unsigned int *old_freq;

		data = &per_cpu(pago_cpu, cpu);
		old_freq = &per_cpu(cpu_old_freq, cpu);

		if (!data->cpu_up || !data->enabled)
			continue;

		/* calculate counter delta */
		for (i = 0; i < MAX_PMU_EVENTS; i++) {
			data->delta[i] = data->curr[i] - data->prev[i];
			if (data->delta[i] < 0)
				data->delta[i] = 0;
			data->prev[i] = data->curr[i];
		}

		if (data->idle_sample) {
			delta = update_delta;
			data->last_updated = now;
		}
		else {
			delta = ktime_us_delta(data->last_sampled, data->last_updated);
			if (delta < 0)
				delta = update_delta;

			data->last_updated = data->last_sampled;
		}

		data->cycle_freq = (unsigned int)DIV_ROUND_CLOSEST_ULL(data->delta[CYCLE], delta) * 1000;

		trace_pago_delta_snapshot(cpu, data->idle_sample, delta, data->delta);

		cur_freq = cpufreq_quick_get(cpu);

		/* update domain & group */
		for (i = 0; i < data->nr_domains; i++) {
			struct pago_domain *dom = data->domains[i];
			struct pago_group *grp = dom->grp;
			unsigned int scale_freq = data->cycle_freq;
			unsigned int mpki = 0;

			if (!emstune_boost->activated && cur_freq > *old_freq) {
				/* cpufreq lamp up */
				scale_freq = cur_freq;
				data->down_step[i] = 0;
			} else if (cur_freq && ((data->cycle_freq * 100 / cur_freq) > 90)) {
				/* If MPKI is low, freq down one step */
				if (grp->miss_idx != -1)
					mpki = (data->delta[grp->miss_idx] * 1000) /
						(data->delta[INSTR] / 1000);

				if (mpki < dom->mpki_threshold) {
					data->down_step[i]++;
					if (data->down_step[i] > (grp->ftable_size - 1))
						data->down_step[i] = grp->ftable_size - 1;
				} else {
					data->down_step[i] = 0;
				}
			} else {
				data->down_step[i] = 0;
			}

			if (dom->perf)
				grp->max_perf_cycle_freq = max(grp->max_perf_cycle_freq, data->cycle_freq);

			data->target_freq[i] = cpufreq_to_memfreq(scale_freq, dom);

			if (data->down_step[i] > 0) {
				int state = get_freq_idx(grp, data->target_freq[i]);
				if (state != -1) {
					state -= data->down_step[i];
					if (state < 0)
						state = 0;

					data->target_freq[i] = grp->ftable[state];
				}
			}

			grp->max_target_freq = max(grp->max_target_freq, data->target_freq[i]);

			trace_pago_target_snapshot(cpu, data->target_freq[i], data->cycle_freq,
					scale_freq, cur_freq, mpki, data->down_step[i], grp->name);
		}

		*old_freq = cur_freq;
	}

	pago->last_updated = now;

	unlock_all_cpus();
	local_irq_restore(flags);
}

static void pago_calc_boost_freq(ktime_t now)
{
	struct boost_request *emstune_boost = &pago->emstune;
	int sysbusy_boost_activated = sysbusy_activated();
	int i;

	for (i = 0; i < pago->nr_groups; i++) {
		struct pago_group *grp = pago->groups[i];
		unsigned int max_boost_freq = 0;

		/* check sysbusy boost */
		if (sysbusy_boost_activated)
			max_boost_freq = grp->max_mem_freq;
		/* check emstune boost */
		else if (emstune_boost->activated) {
			s64 boosted_time = now - emstune_boost->activated_ts;

			if (boosted_time < ms_to_ktime(grp->force_boost_ms)) {
				max_boost_freq = grp->max_mem_freq;
			} else if (grp->boost_threshold <= grp->max_perf_cycle_freq) {
				max_boost_freq = grp->max_mem_freq;
			} else if (grp->boost_min) {
				if (grp->max_target_freq > grp->boost_min)
					max_boost_freq = grp->max_mem_freq;
				else
					max_boost_freq = grp->boost_min;
			}

		}

		grp->max_boost_freq = max_boost_freq;
	}
}

static void pago_request_group_target(struct pago_group *grp, unsigned int target_freq)
{
	if (grp->pm_qos_class)
		exynos_pm_qos_update_request(&grp->pm_qos, target_freq);

#ifdef CONFIG_SCHED_EMS_DSU_FREQ_SELECT
	if (!grp->pm_qos_class)
		exynos_dsufreq_target(target_freq);
#endif
}

static void pago_boost_work(struct kthread_work *work)
{
	int i;

	for (i = 0; i < pago->nr_groups; i++) {
		struct pago_group *grp = pago->groups[i];

		/* request new boost freq */
		pago_request_group_target(grp, grp->max_mem_freq);

		trace_pago_boost_work(grp->max_mem_freq, grp->name);
	}
}

static void pago_work(struct kthread_work *work)
{
	int i;
	ktime_t now = ktime_get();

	/* calc target_freq using PMU events */
	pago_calc_target_freq(now);

	/* calc boost_freq */
	pago_calc_boost_freq(now);

	/* request new mem freq */
	for (i = 0; i < pago->nr_groups; i++) {
		struct pago_group *grp = pago->groups[i];

		grp->resolved_freq = max(grp->max_target_freq, grp->max_boost_freq);

		pago_request_group_target(grp, grp->resolved_freq);

		trace_pago_work(grp->max_target_freq, grp->max_boost_freq, grp->resolved_freq, grp->name);

		/* clear calculated freq */
		grp->max_boost_freq = 0;
		grp->max_target_freq = 0;
		grp->max_perf_cycle_freq = 0;
	}
}

static void pago_irq_boost_work(struct irq_work *irq_work)
{
	kthread_queue_work(&pago->worker, &pago->boost_work);
}

static void pago_irq_work(struct irq_work *irq_work)
{
	kthread_queue_work(&pago->worker, &pago->work);
}

#define UPDATE_MARGIN          ((NSEC_PER_SEC / HZ) >> 1)
#define UPDATE_DELAY           (100 * NSEC_PER_USEC)
#define TARGET_PERIOD          (4)
#define ACTIVE_THRESHOLD       (20)
static bool pago_need_update(ktime_t now)
{
	ktime_t update_period = pago->update_period;
	int cpu, i, active_ratio;

	if (now - pago->last_jiffies + UPDATE_MARGIN < ms_to_ktime(update_period))
		return false;

	for_each_possible_cpu(cpu) {
		active_ratio = mlt_cpu_value(cpu, TARGET_PERIOD, AVERAGE, ACTIVE)
				* 100 / SCHED_CAPACITY_SCALE;

		if (active_ratio > ACTIVE_THRESHOLD)
			return true;
	}

	for (i = 0; i < pago->nr_groups; i++) {
		struct pago_group *grp = pago->groups[i];

		/* if pm_qos requested, force update for release */
		if (grp->pm_qos_class && grp->pm_qos.node.prio > 0)
			return true;
		else if (grp->resolved_freq > 0)
			return true;
	}

	return false;
}

void pago_update(void)
{
	ktime_t now = ktime_get();

	if (unlikely(!pago->enable))
		return;

	if (!pago_need_update(now))
		return;

	irq_work_queue(&pago->irq_work);

	pago->last_jiffies = now;
}

void pago_tick(struct rq *rq)
{
	struct pago_cpu *data = &per_cpu(pago_cpu, rq->cpu);
	int i;
	unsigned long flags;
	ktime_t now = ktime_get(), update_period = pago->update_period;

	raw_spin_lock_irqsave(&data->lock, flags);

	if (!data->enabled || !data->cpu_up)
		goto out;

	if (now - data->last_updated + UPDATE_MARGIN < ms_to_ktime(update_period))
		goto out;

	/* Read pmu counter here */
	for (i = 0; i < MAX_PMU_EVENTS; i++)
		data->curr[i] = read_pmu_events(data, i);

	data->last_sampled = now;
	data->idle_sample = false;

	trace_pago_data_snapshot(rq->cpu, data->idle_sample, data->last_sampled,
			data->last_updated, data->curr, "tick");

out:
	raw_spin_unlock_irqrestore(&data->lock, flags);
}

void pago_idle_enter(int cpu)
{
	struct pago_cpu *data = &per_cpu(pago_cpu, cpu);
	int i;
	unsigned long flags;

	raw_spin_lock_irqsave(&data->lock, flags);

	if (!data->enabled || !data->cpu_up)
		goto out;

	for (i = 0; i < MAX_PMU_EVENTS; i++)
		data->curr[i] = read_pmu_events(data, i);

	data->idle_sample = true;

	trace_pago_data_snapshot(cpu, data->idle_sample, data->last_sampled,
			data->last_updated, data->curr, "idle-enter");

out:
	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static int pago_cpu_up(unsigned int cpu)
{
	struct pago_cpu *data = &per_cpu(pago_cpu, cpu);
	unsigned long flags;

	activate_pmu_events(data);

	raw_spin_lock_irqsave(&data->lock, flags);
	data->cpu_up = true;
	raw_spin_unlock_irqrestore(&data->lock, flags);

	trace_pago_data_snapshot(cpu, data->cpu_up, data->last_sampled,
			data->last_updated, data->curr, "cpu-up");

	return 0;
}

static int pago_cpu_down(unsigned int cpu)
{
	struct pago_cpu *data = &per_cpu(pago_cpu, cpu);
	unsigned long flags;

	raw_spin_lock_irqsave(&data->lock, flags);
	data->cpu_up = false;
	raw_spin_unlock_irqrestore(&data->lock, flags);

	deactivate_pmu_events(data);

	trace_pago_data_snapshot(cpu, data->cpu_up, data->last_sampled,
			data->last_updated, data->curr, "cpu-down");

	return 0;
}

static int pago_emstune_notifier_call(struct notifier_block *nb,
			unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct boost_request *emstune = &pago->emstune;
	bool boost;

	if (unlikely(!pago->enable))
		return NOTIFY_OK;

	if (!emstune->activated && cur_set->level < EMSTUNE_BOOST_LEVEL)
		return NOTIFY_OK;

	boost = cur_set->level == EMSTUNE_BOOST_LEVEL;

	raw_spin_lock(&pago->lock);
	emstune->level = cur_set->level;
	emstune->activated = boost;
	emstune->activated_ts = boost ? ktime_get() : 0;
	raw_spin_unlock(&pago->lock);

	if (boost)
		irq_work_queue(&pago->irq_boost_work);

	trace_pago_notifier(cur_set->level, "emstune");

	return NOTIFY_OK;
}

static struct notifier_block pago_emstune_notifier = {
	.notifier_call = pago_emstune_notifier_call,
};

static void pago_disable(void)
{
	struct pago_cpu *data;
	int cpu;
	unsigned long flags;

	if (!pago->enable)
		return;

	if (cpuhp_state > 0)
		cpuhp_remove_state_nocalls(cpuhp_state);

	for_each_possible_cpu(cpu) {
		data = &per_cpu(pago_cpu, cpu);

		raw_spin_lock_irqsave(&data->lock, flags);
		data->enabled = false;
		data->cpu_up = false;
		data->idle_sample = true;
		raw_spin_unlock_irqrestore(&data->lock, flags);

		deactivate_pmu_events(data);
	}

	pago->enable = false;
}

static void pago_enable(void)
{
	struct pago_cpu *data;
	int cpu;
	unsigned long flags;

	if (pago->enable)
		return;

	cpus_read_lock();

	for_each_possible_cpu(cpu) {
		data = &per_cpu(pago_cpu, cpu);

		activate_pmu_events(data);

		raw_spin_lock_irqsave(&data->lock, flags);
		data->enabled = true;
		data->cpu_up = cpumask_test_cpu(cpu, cpu_online_mask);
		data->idle_sample = false;
		raw_spin_unlock_irqrestore(&data->lock, flags);
	}

	cpuhp_state = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "pago", pago_cpu_up, pago_cpu_down);

	pago->enable = true;

	cpus_read_unlock();
}
/**********************************************************************
 *			    SYSFS interface			      *
 **********************************************************************/
static ssize_t show_update_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%llu\n", pago->update_period);
}

static ssize_t store_update_period(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u64 input;

	if (sscanf(buf, "%llu", &input) != 1)
		return -EINVAL;

	pago->update_period = input;

	return count;
}

static struct kobj_attribute update_period_attr =
__ATTR(update_period, 0644, show_update_period, store_update_period);

static ssize_t show_pago_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", pago->enable);
}

static ssize_t store_pago_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	input = !!input;

	if (input)
		pago_enable();
	else
		pago_disable();

	return count;
}

static struct kobj_attribute pago_enable_attr =
__ATTR(enable, 0644, show_pago_enable, store_pago_enable);

struct pago_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

static ssize_t show_freq_table(struct kobject *k, char *buf)
{
	struct pago_domain *dom = container_of(k, struct pago_domain, kobj);
	unsigned int count = 0;
	int i;

	count += scnprintf(buf, PAGE_SIZE, "CPU freq\tMem freq\n");

	for (i = 0; i < dom->table_size; i++) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "%8u\t%8u\n",
				dom->freq_table[i].cpufreq, dom->freq_table[i].memfreq);
	}

	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static struct pago_attr freq_table_attr = __ATTR(freq_table, 0444, show_freq_table, NULL);

#define pago_attr_rw(scale, name)			\
static struct pago_attr name##_attr =		\
__ATTR(name, 0644, show_##name, store_##name)

#define pago_show(scale, name)								\
static ssize_t show_##name(struct kobject *k, char *buf)				\
{											\
	struct pago_##scale *scale = container_of(k, struct pago_##scale, kobj);	\
											\
	return sprintf(buf, "%u\n", scale->name);						\
}											\

#define pago_store(scale, name)								\
static ssize_t store_##name(struct kobject *k, const char *buf, size_t count)		\
{											\
	struct pago_##scale *scale = container_of(k, struct pago_##scale, kobj);	\
	unsigned int data;								\
											\
	if (!sscanf(buf, "%u", &data))							\
		return -EINVAL;								\
											\
	scale->name = data;								\
	return count;									\
}

pago_show(domain, mpki_threshold);
pago_store(domain, mpki_threshold);
pago_attr_rw(domain, mpki_threshold);

pago_show(domain, perf);
pago_store(domain, perf);
pago_attr_rw(domain, perf);

pago_show(group, boost_min);
pago_store(group, boost_min);
pago_attr_rw(group, boost_min);

pago_show(group, boost_threshold);
pago_store(group, boost_threshold);
pago_attr_rw(group, boost_threshold);

pago_show(group, force_boost_ms);
pago_store(group, force_boost_ms);
pago_attr_rw(group, force_boost_ms);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct pago_attr *fvattr = container_of(at, struct pago_attr, attr);

	return fvattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
		const char *buf, size_t count)
{
	struct pago_attr *fvattr = container_of(at, struct pago_attr, attr);

	return fvattr->store(kobj, buf, count);
}

static const struct sysfs_ops pago_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *pago_domain_attrs[] = {
	&mpki_threshold_attr.attr,
	&perf_attr.attr,
	&freq_table_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(pago_domain);

static struct kobj_type ktype_pago_domain = {
	.sysfs_ops	= &pago_sysfs_ops,
	.default_groups	= pago_domain_groups,
};

static struct attribute *pago_group_attrs[] = {
	&boost_min_attr.attr,
	&boost_threshold_attr.attr,
	&force_boost_ms_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(pago_group);

static struct kobj_type ktype_pago_group = {
	.sysfs_ops	= &pago_sysfs_ops,
	.default_groups	= pago_group_groups,
};

static void pago_free(void)
{
	int i, j, cpu;

	if (!pago)
		return;

	if (pago->groups) {
		for (i = 0; i < pago->nr_groups; i++) {
			struct pago_group *grp = pago->groups[i];

			if (grp) {
				if (grp->domains) {
					for (j = 0; j < grp->nr_domains; j++) {
						struct pago_domain *dom = grp->domains[j];

						if (dom) {
							if (dom->freq_table)
								kfree(dom->freq_table);
							kfree(dom);
						}
					}

					kfree(grp->domains);
				}

				kfree(grp);
			}
		}

		for_each_possible_cpu(cpu) {
			struct pago_cpu *data = &per_cpu(pago_cpu, cpu);

			if (data->domains)
				kfree(data->domains);
			if (data->target_freq)
				kfree(data->target_freq);
			if (data->down_step)
				kfree(data->down_step);

			deactivate_pmu_events(data);
		}

		kfree(pago->groups);
	}

	if (pago->thread)
		kthread_stop(pago->thread);
	if (pago->kobj)
		kfree(pago->kobj);
	kfree(pago);
}

static int pago_sysfs_create(struct kobject *ems_kobj)
{
	pago->kobj = kobject_create_and_add("pago", ems_kobj);
	if (!pago->kobj)
		return -ENOMEM;

	if (sysfs_create_file(pago->kobj, &update_period_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(pago->kobj, &pago_enable_attr.attr))
		return -ENOMEM;

	return 0;
}

static int pago_kthread_create(struct device_node *dn)
{
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };
	const char *buf;

	init_irq_work(&pago->irq_work, pago_irq_work);
	init_irq_work(&pago->irq_boost_work, pago_irq_boost_work);
	kthread_init_work(&pago->work, pago_work);
	kthread_init_work(&pago->boost_work, pago_boost_work);
	kthread_init_worker(&pago->worker);

	thread = kthread_create(kthread_worker_fn, &pago->worker, "pagov");
	if (IS_ERR(thread)) {
		pr_err("failed to create pago thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	if (sched_setscheduler_nocheck(thread, SCHED_FIFO, &param)) {
		kthread_stop(thread);
		pr_err("failed to set SCHED_FIFO\n");
		return -1;
	}

	if (!of_property_read_string(dn, "thread-run-on", &buf))
		cpulist_parse(buf, &pago->thread_allowed_cpus);
	else
		cpumask_copy(&pago->thread_allowed_cpus, cpu_possible_mask);

	set_cpus_allowed_ptr(thread, &pago->thread_allowed_cpus);
	thread->flags |= PF_NO_SETAFFINITY;
	pago->thread = thread;

	wake_up_process(pago->thread);

	return 0;
}

unsigned int *pago_init_freq_table(struct device_node *dn, int *table_size)
{
	int raw_table_size, ret = 0;
	unsigned int *raw_table;

	raw_table_size = of_property_count_u32_elems(dn, "mif-table");
	if (raw_table_size < 0) {
		*table_size = 0;
		return NULL;
	}

	raw_table = kcalloc(raw_table_size, sizeof(u32), GFP_KERNEL);
	if (!raw_table)
		return NULL;

	ret = of_property_read_u32_array(dn, "mif-table", raw_table, raw_table_size);
	if (ret) {
		kfree(raw_table);
		return NULL;
	}

	*table_size = raw_table_size;
	return raw_table;
}

static int pago_register(struct kobject *ems_kobj, struct device_node *dn)
{
	pago = kzalloc(sizeof(struct pago), GFP_KERNEL);
	if (!pago) {
		pr_err("failed to allocate PAGO\n");
		return -ENOMEM;
	}

	if (pago_sysfs_create(ems_kobj)) {
		pr_err("failed to create sysfs\n");
		return -ENOMEM;
	}

	if (pago_kthread_create(dn)) {
		pr_err("failed to create kthread\n");
		return -ENOMEM;
	}

	pago->nr_groups = of_get_child_count(dn);
	if (pago->nr_groups <= 0) {
		pr_err("%s: failed to get pago group\n", __func__);
		return -EINVAL;
	}

	pago->groups = kmalloc_array(pago->nr_groups, sizeof(struct pago_group *), GFP_KERNEL);
	if (!pago->groups) {
		pr_err("%s: failed to alloc pago group\n", __func__);
		return -ENOMEM;
	}

	pago->update_period = DEFAULT_UPDATE_MS;
	raw_spin_lock_init(&pago->lock);

	pago->emstune.level = 0;
	pago->emstune.activated = false;
	pago->emstune.activated_ts = 0;

	emstune_register_notifier(&pago_emstune_notifier);

	return 0;
}

static int pago_group_init(struct device_node *dn, struct pago_group *grp)
{
	if (of_property_read_string(dn, "desc", &grp->name)) {
		pr_err("%s: failed to get group name\n", __func__);
		return -EINVAL;
	}

	if (kobject_init_and_add(&grp->kobj, &ktype_pago_group, pago->kobj,
				"%s", grp->name)) {
		pr_err("%s: failed to init kobject\n", __func__);
		return -ENOMEM;
	}

	grp->nr_domains = of_get_child_count(dn);
	if (grp->nr_domains <= 0) {
		pr_err("%s: failed to get domains for pago group\n", __func__);
		return -EINVAL;
	}

	grp->domains = kmalloc_array(grp->nr_domains, sizeof(struct pago_domain *), GFP_KERNEL);
	if (!grp->domains) {
		pr_err("%s: failed to alloc domains for pago group\n", __func__);
		return -ENOMEM;
	}

	if (!of_property_read_u32(dn, "pm-qos-class", &grp->pm_qos_class))
		exynos_pm_qos_add_request(&grp->pm_qos, grp->pm_qos_class, 0);
	else
		grp->pm_qos_class = 0;

	if (of_property_read_u32(dn, "force-boost-ms", &grp->force_boost_ms))
		grp->force_boost_ms = DEFAULT_UPDATE_MS * 10;

	if (of_property_read_u32(dn, "boost-threshold", &grp->boost_threshold))
		grp->boost_threshold = UINT_MAX;

	if (of_property_read_u32(dn, "boost-min", &grp->boost_min))
		grp->boost_min = 0;

	if (of_property_read_s32(dn, "miss-idx", &grp->miss_idx))
		grp->miss_idx = -1;

	grp->ftable = pago_init_freq_table(dn, &grp->ftable_size);

	grp->max_mem_freq = 0;
	grp->max_boost_freq = 0;
	grp->max_target_freq = 0;
	grp->max_perf_cycle_freq = 0;

	grp->resolved_freq = 0;

	return 0;
}

static int pago_init_domain(struct device_node *dn, struct pago_group *grp,
		struct pago_domain *dom)
{
	const char *buf;
	unsigned int *raw_table;
	int raw_table_size, i, ti, ret;
	struct cpu_mem_map *table;

	if (!of_property_read_string(dn, "cpus", &buf))
		cpulist_parse(buf, &dom->cpus);
	else {
		pr_err("%s: failed to parse cpus\n", __func__);
		return -EINVAL;
	}

	ret = kobject_init_and_add(&dom->kobj, &ktype_pago_domain, &grp->kobj,
				"domain%d", cpumask_first(&dom->cpus));
	if (ret) {
		pr_err("%s: failed to init kobject\n", __func__);
		return ret;
	}

	if (of_property_read_u32(dn, "mpki-threshold", &dom->mpki_threshold))
		dom->mpki_threshold = 0;

	dom->perf = of_property_read_bool(dn, "perf");

	raw_table_size = of_property_count_u32_elems(dn, "cpu-mem-table");
	if (raw_table_size <= 0) {
		pr_err("%s: failed to count raw table size\n", __func__);
		return -EINVAL;
	}

	raw_table = kmalloc_array(raw_table_size, sizeof(unsigned int), GFP_KERNEL);
	if (!raw_table) {
		pr_err("%s: failed to alloc raw table\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32_array(dn, "cpu-mem-table", raw_table, raw_table_size);
	if (ret) {
		pr_err("%s: failed to parse raw table\n", __func__);
		goto out;
	}

	table = kcalloc(raw_table_size / 2, sizeof(struct cpu_mem_map), GFP_KERNEL);
	if (!table) {
		pr_err("%s: failed to alloc cpu mem table\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0, ti = 0; i < raw_table_size; i += 2, ti++) {
		table[ti].cpufreq = raw_table[i];
		table[ti].memfreq = raw_table[i + 1];
	}

	dom->table_size = raw_table_size / 2;
	dom->freq_table = table;

out:
	kfree(raw_table);

	return ret;
}

static int pago_init_cpus(void)
{
	int cpu, i, j, ret = 0;

	cpus_read_lock();

	for_each_possible_cpu(cpu) {
		struct pago_cpu *data = &per_cpu(pago_cpu, cpu);

		data->nr_domains = pago->nr_groups;
		data->domains = kmalloc_array(data->nr_domains, sizeof(struct pago_domain *), GFP_KERNEL);
		if (!data->domains) {
			pr_err("%s: failed to alloc domains. CPU=%d\n", __func__, cpu);
			ret = -ENOMEM;
			goto unlock;
		}

		data->target_freq = kmalloc_array(data->nr_domains, sizeof(unsigned int), GFP_KERNEL);
		if (!data->target_freq) {
			pr_err("%s: failed to alloc target_freq. CPU=%d\n", __func__, cpu);
			ret = -ENOMEM;
			goto unlock;
		}

		data->down_step = kcalloc(data->nr_domains, sizeof(unsigned int), GFP_KERNEL);
		if (!data->down_step) {
			pr_err("%s: failed to alloc down_step. CPU=%d\n", __func__, cpu);
			ret = -ENOMEM;
			goto unlock;
		}

		for (i = 0; i < pago->nr_groups; i++) {
			struct pago_group *grp = pago->groups[i];

			for (j = 0; j < grp->nr_domains; j++) {
				struct pago_domain *dom = grp->domains[j];

				if (cpumask_test_cpu(cpu, &dom->cpus))
					data->domains[i] = dom;
			}
		}

		data->cpu = cpu;
		data->cpu_up = cpumask_test_cpu(cpu, cpu_online_mask);
		data->enabled = true;
		data->idle_sample = false;
		data->last_sampled = 0;
		data->last_updated = 0;
		raw_spin_lock_init(&data->lock);

		if (activate_pmu_events(data)) {
			pr_err("%s: failed to enable pmu events. CPU=%d\n", __func__, cpu);
		}

		pr_info("cpu%d: pmu event init complete.\n", cpu);
	}

	cpuhp_state = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "pago", pago_cpu_up, pago_cpu_down);
	if (cpuhp_state < 0) {
		pr_err("%s: failed to init pago hotplug\n", __func__);
		ret = cpuhp_state;
	}

unlock:
	cpus_read_unlock();

	return ret;
}

static int pago_domain_register(struct device_node *dn, struct pago_group *grp)
{
	struct device_node *child;
	int idx = 0, ret = 0;

	for_each_child_of_node(dn, child) {
		struct pago_domain *dom;

		dom = kzalloc(sizeof(struct pago_domain), GFP_KERNEL);
		if (!dom) {
			pr_err("%s: failed to alloc pago domain\n", __func__);
			return -ENOMEM;
		}

		ret = pago_init_domain(child, grp, dom);
		if (ret)
			return ret;

		dom->grp = grp;
		grp->domains[idx] = dom;
		grp->max_mem_freq = max(grp->max_mem_freq,
					dom->freq_table[dom->table_size - 1].memfreq);

		idx++;
	}

	return 0;
}

int pago_init(struct kobject *ems_kobj)
{
	struct device_node *dn, *child;
	int idx = 0;

	dn = of_find_node_by_path("/ems/pago");
	if (!dn)
		return -1;

	if (pago_register(ems_kobj, dn))
		goto fail;

	/* Initialize pago group and domain */
	for_each_child_of_node(dn, child) {
		struct pago_group *grp;

		grp = kzalloc(sizeof(struct pago_group), GFP_KERNEL);
		if (!grp) {
			pr_err("%s: failed to alloc pago group\n", __func__);
			return -ENOMEM;
		}

		if (pago_group_init(child, grp)) {
			pr_err("%s: failed to init pago group\n", __func__);
			goto fail;
		}

		if (pago_domain_register(child, grp)) {
			pr_err("%s: failed to regist pago domain\n", __func__);
			goto fail;
		}

		pago->groups[idx] = grp;
		idx++;
	}

	if (pago_init_cpus())
		goto fail;

	pago->enable = true;

	pr_info("PAGO init complete.\n");

	return 0;

fail:
	pago_free();

	return -1;
}
