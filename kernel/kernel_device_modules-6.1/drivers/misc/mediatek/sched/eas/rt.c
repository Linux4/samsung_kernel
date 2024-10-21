// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/cpuidle.h>
#include "../sugov/cpufreq.h"
#include "common.h"
#include "eas_plus.h"
#include "eas_trace.h"
#include "vip.h"
#include <mt-plat/mtk_irq_mon.h>

bool skip_hiIRQ_enable;
void init_skip_hiIRQ(void)
{
	skip_hiIRQ_enable = sched_skip_hiIRQ_enable_get();
}

bool rt_aggre_preempt_enable;
void init_rt_aggre_preempt(void)
{
	rt_aggre_preempt_enable = sched_rt_aggre_preempt_enable_get();
}

void set_rt_aggre_preempt(int val)
{
	rt_aggre_preempt_enable = (bool) val;
}
EXPORT_SYMBOL_GPL(set_rt_aggre_preempt);

static inline void rt_energy_aware_output_init(struct rt_energy_aware_output *rt_ea_output,
			struct task_struct *p)
{
	rt_ea_output->rt_cpus = 0;
	rt_ea_output->cfs_cpus = 0;
	rt_ea_output->idle_cpus = 0;
	rt_ea_output->cfs_lowest_cpu = -1;
	rt_ea_output->cfs_lowest_prio = 0;
	rt_ea_output->cfs_lowest_pid = -1;
	rt_ea_output->rt_lowest_cpu = -1;
	rt_ea_output->rt_lowest_prio = p->prio;
	rt_ea_output->rt_lowest_pid = -1;
	rt_ea_output->select_reason = 0;
	rt_ea_output->rt_aggre_preempt_enable = -1;
}

#if IS_ENABLED(CONFIG_RT_SOFTINT_OPTIMIZATION)
/*
 * Return whether the task on the given cpu is currently non-preemptible
 * while handling a potentially long softint, or if the task is likely
 * to block preemptions soon because it is a ksoftirq thread that is
 * handling slow softints.
 */
bool task_may_not_preempt(struct task_struct *task, int cpu)
{
	__u32 softirqs = per_cpu(active_softirqs, cpu) |
			local_softirq_pending();

	struct task_struct *cpu_ksoftirqd = per_cpu(ksoftirqd, cpu);

	return ((softirqs & LONG_SOFTIRQ_MASK) &&
		(task == cpu_ksoftirqd ||
		 task_thread_info(task)->preempt_count & SOFTIRQ_MASK));
}
#endif /* CONFIG_RT_SOFTINT_OPTIMIZATION */

#if IS_ENABLED(CONFIG_SMP)
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	if (sched_feat(UTIL_EST) && is_util_est_enable())
		return max(task_util(p), _task_util_est(p));
	return task_util(p);
}

static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	/*
	 * If the waker is CFS, then an RT sync wakeup would preempt the waker
	 * and force it to run for a likely small time after the RT wakee is
	 * done. So, only honor RT sync wakeups from RT wakers.
	 */
	return sync && task_has_rt_policy(rq->curr) &&
		p->prio <= rq->rt.highest_prio.next &&
		rq->rt.rt_nr_running <= 2;
}
#else
static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(rt_task(p) ? 0 : task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}

/*
 * Verify the fitness of task @p to run on @cpu taking into account the uclamp
 * settings.
 *
 * This check is only important for heterogeneous systems where uclamp_min value
 * is higher than the capacity of a @cpu. For non-heterogeneous system this
 * function will always return true.
 *
 * The function will return true if the capacity of the @cpu is >= the
 * uclamp_min and false otherwise.
 *
 * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
 * > uclamp_max.
 */
static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned int min_cap;
	unsigned int max_cap;
	unsigned int cpu_cap;

	/* Only heterogeneous systems can benefit from this check */
	if (!likely(mtk_sched_asym_cpucapacity))
		return true;

	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	cpu_cap = capacity_orig_of(cpu);

	return cpu_cap >= min(min_cap, max_cap);
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est(p);
}

static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif

inline unsigned long mtk_sched_cpu_util(int cpu)
{
	unsigned long util;

	irq_log_store();
	util = mtk_cpu_util(cpu, cpu_util_cfs(cpu), ENERGY_UTIL, NULL, 0, SCHED_CAPACITY_SCALE);
	irq_log_store();

	return util;
}

inline unsigned long mtk_sched_max_util(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap)
{
	unsigned long util;

	irq_log_store();
	util = mtk_cpu_util(cpu, cpu_util_cfs(cpu), FREQUENCY_UTIL, p, min_cap, max_cap);
	irq_log_store();

	return util;
}

unsigned int min_highirq_load[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS-1] = SCHED_CAPACITY_SCALE /* default 1024 */
};

unsigned int irq_ratio[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS-1] = 100 /* default irq=cpu */
};

inline void __set_cpu_irqUtil_threshold(int cpu, unsigned int min_util)
{
	min_highirq_load[cpu] = min_util;
}

inline void __set_cpu_irqRatio_threshold(int cpu, unsigned int min_ratio)
{
	irq_ratio[cpu] = min_ratio;
}

inline void __unset_cpu_irqUtil_threshold(int cpu)
{
	min_highirq_load[cpu] = SCHED_CAPACITY_SCALE;
}

inline void __unset_cpu_irqRatio_threshold(int cpu)
{
	irq_ratio[cpu] = 100;
}

int set_cpu_irqUtil_threshold(int cpu, int min_util)
{
	int ret = 0;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	/* check util validity */
	if (min_util < 0 || min_util > SCHED_CAPACITY_SCALE)
		goto done;

	__set_cpu_irqUtil_threshold(cpu, min_util);
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_cpu_irqUtil_threshold);

int unset_cpu_irqUtil_threshold(int cpu)
{
	int ret = 0;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	__unset_cpu_irqUtil_threshold(cpu);
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_cpu_irqUtil_threshold);

int get_cpu_irqUtil_threshold(int cpu)
{
	int val = -1;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	val = min_highirq_load[cpu];

done:
	return val;
}
EXPORT_SYMBOL_GPL(get_cpu_irqUtil_threshold);

int set_cpu_irqRatio_threshold(int cpu, int min_ratio)
{
	int ret = 0;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	/* check util validity */
	if (min_ratio < 0 || min_ratio > 100)
		goto done;

	__set_cpu_irqRatio_threshold(cpu, min_ratio);
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_cpu_irqRatio_threshold);

int unset_cpu_irqRatio_threshold(int cpu)
{
	int ret = 0;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	__unset_cpu_irqRatio_threshold(cpu);
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_cpu_irqRatio_threshold);

int get_cpu_irqRatio_threshold(int cpu)
{
	int val = -1;

	/* check feature is enabled */
	if (!skip_hiIRQ_enable)
		goto done;

	/* check cpu validity */
	if (cpu < 0 || cpu > MAX_NR_CPUS-1)
		goto done;

	val = irq_ratio[cpu];

done:
	return val;
}
EXPORT_SYMBOL_GPL(get_cpu_irqRatio_threshold);

inline int cpu_high_irqload(int cpu)
{
	unsigned long irq_util, cpu_util;

	if (!skip_hiIRQ_enable)
		return 0;

	irq_util = cpu_util_irq(cpu_rq(cpu));
	cpu_util = mtk_sched_cpu_util(cpu);
	if (irq_util < min_highirq_load[cpu])
		return 0;

	if (irq_util * 100 < cpu_util * irq_ratio[cpu])
		return 0;

	return 1;
}

inline unsigned int mtk_get_idle_exit_latency(int cpu,
			struct rt_energy_aware_output *rt_ea_output)
{
	struct cpuidle_state *idle;
	struct task_struct *curr;

	/* CPU is idle */
	if (available_idle_cpu(cpu)) {
		if (rt_ea_output)
			rt_ea_output->idle_cpus = (rt_ea_output->idle_cpus | (1 << cpu));

		idle = idle_get_state(cpu_rq(cpu));
		if (idle)
			return idle->exit_latency;

		/* CPU is in WFI */
		return 0;
	}

	if (rt_ea_output) {
		curr = cpu_rq(cpu)->curr;
		if (curr && (curr->policy == SCHED_NORMAL)) {
			rt_ea_output->cfs_cpus = (rt_ea_output->cfs_cpus | (1 << cpu));

			if ((curr->prio > rt_ea_output->cfs_lowest_prio)
				&& (!task_may_not_preempt(curr, cpu))) {
				rt_ea_output->cfs_lowest_prio = curr->prio;
				rt_ea_output->cfs_lowest_cpu = cpu;
				rt_ea_output->cfs_lowest_pid = curr->pid;
			}
		}

		if (curr && rt_task(curr)) {
			rt_ea_output->rt_cpus = (rt_ea_output->rt_cpus | (1 << cpu));

			if (curr->prio > rt_ea_output->rt_lowest_prio) {
				rt_ea_output->rt_lowest_prio = curr->prio;
				rt_ea_output->rt_lowest_cpu = cpu;
				rt_ea_output->rt_lowest_pid = curr->pid;
			}
		}
	}

	/* CPU is not idle */
	return UINT_MAX;
}

void track_sched_cpu_util(struct task_struct *p, int cpu,
			unsigned long min_cap, unsigned long max_cap)
{
	if (trace_sched_cpu_util_enabled())
		trace_sched_cpu_util(p, cpu, skip_hiIRQ_enable, min_cap, max_cap);
}

static void mtk_rt_energy_aware_wake_cpu(struct task_struct *p,
			struct cpumask *lowest_mask, int ret, int *best_cpu, bool energy_eval,
			struct rt_energy_aware_output *rt_ea_output)
{
	int cpu, best_idle_cpu_cluster, best_non_idle_cpu_cluster, non_idle_cpu_prio;
	unsigned long util_cum[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = ULONG_MAX};
	unsigned long cpu_util_cum, non_idle_cpu_util_cum, best_cpu_util_cum = ULONG_MAX;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned long best_idle_exit_latency = UINT_MAX;
	unsigned long cpu_idle_exit_latency = UINT_MAX;
	int cluster, weight;
	int order_index, end_index, reverse;
	cpumask_t candidates;
	bool best_cpu_has_lt, cpu_has_lt;
	unsigned long pwr_eff, this_pwr_eff;
	struct perf_domain *target_pd, *pd;
	bool _rt_aggre_preempt_enable = rt_aggre_preempt_enable;

	irq_log_store();
	mtk_get_gear_indicies(p, &order_index, &end_index, &reverse);
	irq_log_store();
	end_index = energy_eval ? end_index : 0;

	cpumask_copy(&candidates, lowest_mask);
	cpumask_clear(&candidates);

	irq_log_store();
	/* No targets found */
	if (!ret)
		return;

	rcu_read_lock();

	pd = rcu_dereference((cpu_rq(smp_processor_id())->rd)->pd);
	/* pd not existed */
	if (!pd)
		goto unlock;

	rt_ea_output->rt_aggre_preempt_enable = _rt_aggre_preempt_enable;
	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		best_idle_exit_latency = UINT_MAX;
		best_idle_cpu_cluster = -1;
		non_idle_cpu_util_cum = ULONG_MAX;
		non_idle_cpu_prio = -1;
		best_non_idle_cpu_cluster = -1;
		best_cpu_has_lt = true;

		target_pd = rcu_dereference(pd);
		target_pd = find_pd(target_pd,
				cpumask_first(&cpu_array[order_index][cluster][reverse]));
		if (!target_pd)
			continue;

		for_each_cpu_and(cpu, lowest_mask, &cpu_array[order_index][cluster][reverse]) {

			track_sched_cpu_util(p, cpu, min_cap, max_cap);

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (cpu_high_irqload(cpu))
				continue;

			/* RT task skips cpu that runs latency_sensitive or vip tasks */
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
			cpu_has_lt = is_task_latency_sensitive(cpu_rq(cpu)->curr)
				|| task_is_vip(cpu_rq(cpu)->curr, NOT_VIP);
#else
			cpu_has_lt = is_task_latency_sensitive(cpu_rq(cpu)->curr);
#endif

			/*
			 * When the best cpu is suitable and the current is not,
			 * skip it
			 */
			if (cpu_has_lt && !best_cpu_has_lt)
				continue;

			/* record best non-idle cpu in gear if gear don't have idle cpus */
			irq_log_store();
			cpu_util_cum = mtk_sched_max_util(p, cpu, min_cap, max_cap);
			irq_log_store();

			util_cum[cpu] = cpu_util_cum;
			if (_rt_aggre_preempt_enable &&
				!cpu_has_lt && ((cpu_rq(cpu)->curr->policy) == SCHED_NORMAL)) {
				if (((cpu_rq(cpu)->curr->prio) > non_idle_cpu_prio) ||
					(((cpu_rq(cpu)->curr->prio) == non_idle_cpu_prio)
						&& (non_idle_cpu_util_cum > cpu_util_cum))) {
					best_non_idle_cpu_cluster = cpu;
					non_idle_cpu_prio = (cpu_rq(cpu)->curr->prio);
					non_idle_cpu_util_cum = cpu_util_cum;
				}
			}

			/*
			 * If candidate CPU is the previous CPU, select it.
			 * Otherwise, if its load is same with best_cpu and in
			 * a shallow C-State, select it. If all above
			 * conditions are same, select the least cumulative
			 * window demand CPU.
			 */
			irq_log_store();
			cpu_idle_exit_latency = mtk_get_idle_exit_latency(cpu, rt_ea_output);
			irq_log_store();

			if (cpu_idle_exit_latency == UINT_MAX)
				continue;

			if (best_idle_exit_latency < cpu_idle_exit_latency)
				continue;

			if (best_idle_exit_latency == cpu_idle_exit_latency &&
					best_cpu_util_cum < cpu_util_cum)
				continue;

			best_idle_exit_latency = cpu_idle_exit_latency;
			best_cpu_util_cum = cpu_util_cum;
			best_idle_cpu_cluster = cpu;
			best_cpu_has_lt = cpu_has_lt;
		}

		irq_log_store();

		if (best_idle_cpu_cluster != -1)
			cpumask_set_cpu(best_idle_cpu_cluster, &candidates);
		else if (best_non_idle_cpu_cluster != -1)
			cpumask_set_cpu(best_non_idle_cpu_cluster, &candidates);

		if ((cluster >= end_index) && (!cpumask_empty(&candidates)))
			break;
	}

	if ((cluster > end_index) && target_pd)
		rt_ea_output->select_reason = LB_FAIL;

	weight = cpumask_weight(&candidates);
	irq_log_store();

	if (!weight)
		goto unlock;

	if (weight == 1) {
		/* fast path */
		*best_cpu = cpumask_first(&candidates);
		goto unlock;
	} else {
		int wl_type;

		wl_type = get_em_wl();
		pwr_eff = ULONG_MAX;
		/* compare pwr_eff among clusters */
		for_each_cpu(cpu, &candidates) {
			cpu_util_cum = util_cum[cpu];
			irq_log_store();
			this_pwr_eff = calc_pwr_eff(wl_type, cpu, cpu_util_cum);
			irq_log_store();

			if (trace_sched_aware_energy_rt_enabled()) {
				trace_sched_aware_energy_rt(wl_type, cpu, this_pwr_eff,
						pwr_eff, cpu_util_cum);
			}

			if (this_pwr_eff < pwr_eff) {
				pwr_eff = this_pwr_eff;
				*best_cpu = cpu;
			}
		}
	}

unlock:
	rcu_read_unlock();
	irq_log_store();
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_rt_mask);

void mtk_select_task_rq_rt(void *data, struct task_struct *p, int source_cpu,
				int sd_flag, int flags, int *target_cpu)
{
	struct task_struct *curr;
	struct rq *rq, *this_cpu_rq;
	bool may_not_preempt;
	bool sync = !!(flags & WF_SYNC);
	int ret, target = -1, this_cpu, select_reason = -1;
	struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(mtk_select_rq_rt_mask);
	struct rt_energy_aware_output rt_ea_output;

	rt_energy_aware_output_init(&rt_ea_output, p);

	irq_log_store();

	/* For anything but wake ups, just return the task_cpu */
	if (!(flags & (WF_TTWU | WF_FORK))) {
		if (!cpu_paused(source_cpu)) {
			select_reason = LB_RT_FAIL;
			*target_cpu = source_cpu;
			goto out;
		}
	}

	irq_log_store();

	this_cpu = smp_processor_id();
	this_cpu_rq = cpu_rq(this_cpu);

	/*
	 * Respect the sync flag as long as the task can run on this CPU.
	 */
	if (should_honor_rt_sync(this_cpu_rq, p, sync) &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
		*target_cpu = this_cpu;
		select_reason = LB_RT_SYNC;
		goto out;
	}

	irq_log_store();

	rq = cpu_rq(source_cpu);

	rcu_read_lock();
	/* unlocked access */
	curr = READ_ONCE(rq->curr);

	/* check source_cpu status */
	may_not_preempt = task_may_not_preempt(curr, source_cpu);

	ret = cpupri_find_fitness(&task_rq(p)->rd->cpupri, p,
				lowest_mask, rt_task_fits_capacity);

	cpumask_andnot(lowest_mask, lowest_mask, cpu_pause_mask);
	cpumask_and(lowest_mask, lowest_mask, cpu_active_mask);

	irq_log_store();
	mtk_rt_energy_aware_wake_cpu(p, lowest_mask, ret, &target, true, &rt_ea_output);

	if (target != -1 &&
		(may_not_preempt || p->prio < cpu_rq(target)->rt.highest_prio.curr)) {
		*target_cpu = target;
		select_reason = LB_RT_IDLE | rt_ea_output.select_reason;
		if (!(rt_ea_output.idle_cpus & (1 << target)))
			select_reason = select_reason | LB_FAIL_IN_REGULAR;
	} else if (rt_ea_output.cfs_lowest_cpu != -1) {
		*target_cpu = rt_ea_output.cfs_lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_NORMAL;
	} else if (rt_ea_output.rt_lowest_cpu != -1) {
		*target_cpu = rt_ea_output.rt_lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_RT;
	} else {
		/* previous CPU as backup */
		*target_cpu = source_cpu;
		select_reason = LB_RT_SOURCE_CPU;
	}

	rcu_read_unlock();
	irq_log_store();
out:
	if (trace_sched_select_task_rq_rt_enabled())
		trace_sched_select_task_rq_rt(p, select_reason, *target_cpu, &rt_ea_output,
				lowest_mask, sd_flag, sync, task_util_est(p), uclamp_task_util(p));
	irq_log_store();
}

void mtk_find_lowest_rq(void *data, struct task_struct *p, struct cpumask *lowest_mask,
			int ret, int *lowest_cpu)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd;
	int cpu, source_cpu, best_cpu;
	int this_cpu = smp_processor_id();
	cpumask_t avail_lowest_mask;
	int target = -1, select_reason = -1;
	struct rt_energy_aware_output rt_ea_output;

	rt_energy_aware_output_init(&rt_ea_output, p);

	irq_log_store();

	if (!ret) {
		select_reason = LB_RT_NO_LOWEST_RQ;
		goto out; /* No targets found */
	}

	cpumask_andnot(&avail_lowest_mask, lowest_mask, cpu_pause_mask);
	cpumask_and(&avail_lowest_mask, &avail_lowest_mask, cpu_active_mask);

	irq_log_store();

	mtk_rt_energy_aware_wake_cpu(p, &avail_lowest_mask, ret, &target, false, &rt_ea_output);

	irq_log_store();

	/* best energy cpu found */
	if (target != -1) {
		*lowest_cpu = target;
		select_reason = LB_RT_IDLE | rt_ea_output.select_reason;
		if (!(rt_ea_output.idle_cpus & (1 << target)))
			select_reason = select_reason | LB_FAIL_IN_REGULAR;
		goto out;
	}

	if (rt_ea_output.cfs_lowest_cpu != -1) {
		*lowest_cpu = rt_ea_output.cfs_lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_NORMAL;
		goto out;
	}

	if (rt_ea_output.rt_lowest_cpu != -1) {
		*lowest_cpu = rt_ea_output.rt_lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_RT;
		goto out;
	}

	irq_log_store();

	source_cpu = task_cpu(p);

	/* use prev cpu as target */
	if (cpumask_test_cpu(source_cpu, &avail_lowest_mask)) {
		*lowest_cpu = source_cpu;
		select_reason = LB_RT_SOURCE_CPU;
		goto out;
	}

	irq_log_store();

	/* Skip this_cpu if it is not among lowest */
	if (!cpumask_test_cpu(this_cpu, &avail_lowest_mask))
		this_cpu = -1;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	/* Find best_cpu on same cluster with task_cpu(p) */
	for (; pd; pd = pd->next) {
		if (!cpumask_test_cpu(source_cpu, perf_domain_span(pd)))
			continue;

		/*
		 * "this_cpu" is cheaper to preempt than a
		 * remote processor.
		 */
		if (this_cpu != -1 && cpumask_test_cpu(this_cpu, perf_domain_span(pd))) {
			rcu_read_unlock();
			*lowest_cpu = this_cpu;
			select_reason = LB_RT_SAME_SYNC;
			goto out;
		}

		best_cpu = cpumask_any_and_distribute(&avail_lowest_mask,
						perf_domain_span(pd));
		if (best_cpu < nr_cpu_ids) {
			rcu_read_unlock();
			*lowest_cpu = best_cpu;
			select_reason = LB_RT_SAME_FIRST;
			goto out;
		}
	}

unlock:
	rcu_read_unlock();

	/*
	 * And finally, if there were no matches within the domains
	 * just give the caller *something* to work with from the compatible
	 * locations.
	 */
	if (this_cpu != -1) {
		*lowest_cpu = this_cpu;
		select_reason = LB_RT_FAIL_SYNC;
		goto out;
	}

	cpu = cpumask_any_and_distribute(&avail_lowest_mask, cpu_possible_mask);
	if (cpu < nr_cpu_ids) {
		*lowest_cpu = cpu;
		select_reason = LB_RT_FAIL_FIRST;
		goto out;
	}

	/* Let find_lowest_rq not to choose dst_cpu */
	*lowest_cpu = -1;
	select_reason = LB_RT_FAIL;
out:
	irq_log_store();

	if (trace_sched_find_lowest_rq_enabled())
		trace_sched_find_lowest_rq(p, select_reason, *lowest_cpu,
				&avail_lowest_mask, lowest_mask, cpu_active_mask);
	/*
	 * move here so that we can see lowest_mask in trace event,
	 * work only if lowest_cpu == -1
	 */
	cpumask_clear(lowest_mask);

	irq_log_store();
}

void throttled_rt_tasks_debug(void *unused, int cpu, u64 clock,
		ktime_t rt_period, u64 rt_runtime, s64 rt_period_timer_expires)
{
	printk_deferred("sched: RT throttling activated for cpu %d\n", cpu);
	printk_deferred("sched: cpu=%d, expires=%lld now=%llu rt_time=%llu runtime=%llu period=%llu\n",
			cpu, rt_period_timer_expires, ktime_get_raw_ns(),
			task_rq(current)->rt.rt_time, rt_runtime, rt_period);
#ifdef CONFIG_SCHED_INFO
	if (sched_info_on())
		printk_deferred("cpu=%d, current %s (%d) is running for %llu nsec\n",
				cpu, current->comm, current->pid,
				clock - current->sched_info.last_arrival);
#endif
}
