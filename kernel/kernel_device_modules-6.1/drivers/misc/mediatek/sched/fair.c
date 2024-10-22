// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>
#include "eas/eas_plus.h"
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "common.h"
#include <sched/pelt.h>
#include <linux/stop_machine.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#include <mt-plat/mtk_irq_mon.h>
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "eas/vip.h"
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "eas/group.h"
#endif
#define CREATE_TRACE_POINTS
#include "sched_trace.h"

MODULE_LICENSE("GPL");

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/* Runqueue only has SCHED_IDLE tasks enqueued */
static int sched_idle_rq(struct rq *rq)
{
	return unlikely(rq->nr_running == rq->cfs.idle_h_nr_running &&
			rq->nr_running);
}

#ifdef CONFIG_SMP
bool mtk_cpus_share_cache(unsigned int this_cpu, unsigned int that_cpu)
{
	if (this_cpu == that_cpu)
		return true;

	return topology_cluster_id(this_cpu) == topology_cluster_id(that_cpu);
}

static int sched_idle_cpu(int cpu)
{
	return sched_idle_rq(cpu_rq(cpu));
}

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

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(rt_task(p) ? 0 : task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est(p);
}
#endif

int task_fits_capacity(struct task_struct *p, long capacity, unsigned int margin)
{
	return fits_capacity(uclamp_task_util(p), capacity, margin);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

unsigned long cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST) && is_util_est_enable())
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

#if IS_ENABLED(CONFIG_MTK_EAS)
/*
 * Predicts what cpu_util(@cpu) would return if @p was migrated (and enqueued)
 * to @dst_cpu.
 */
static unsigned long cpu_util_next(int cpu, struct task_struct *p, int dst_cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util_est, util = READ_ONCE(cfs_rq->avg.util_avg);

	/*
	 * If @p migrates from @cpu to another, remove its contribution. Or,
	 * if @p migrates from another CPU to @cpu, add its contribution. In
	 * the other cases, @cpu is not impacted by the migration, so the
	 * util_avg should already be correct.
	 */
	if (task_cpu(p) == cpu && dst_cpu != cpu)
		lsub_positive(&util, task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * During wake-up, the task isn't enqueued yet and doesn't
		 * appear in the cfs_rq->avg.util_est.enqueued of any rq,
		 * so just add it (if needed) to "simulate" what will be
		 * cpu_util() after the task has been enqueued.
		 */
		if (dst_cpu == cpu)
			util_est += _task_util_est(p);

		util = max(util, util_est);
	}

	return min(util, capacity_orig_of(cpu) + 1);
}

/*
 * Compute the task busy time for compute_energy(). This time cannot be
 * injected directly into effective_cpu_util() because of the IRQ scaling.
 * The latter only makes sense with the most recent CPUs where the task has
 * run.
 */
static inline void eenv_task_busy_time(struct energy_env *eenv,
				       struct task_struct *p, int prev_cpu)
{
	unsigned long busy_time, max_cap = arch_scale_cpu_capacity(prev_cpu);
	unsigned long irq = cpu_util_irq(cpu_rq(prev_cpu));

	if (unlikely(irq >= max_cap))
		busy_time = max_cap;
	else
		busy_time = scale_irq_capacity(task_util_est(p), irq, max_cap);

	eenv->task_busy_time = busy_time;
}

/*
 * Compute the perf_domain (PD) busy time for compute_energy(). Based on the
 * utilization for each @pd_cpus, it however doesn't take into account
 * clamping since the ratio (utilization / cpu_capacity) is already enough to
 * scale the EM reported power consumption at the (eventually clamped)
 * cpu_capacity.
 *
 * The contribution of the task @p for which we want to estimate the
 * energy cost is removed (by cpu_util_next()) and must be calculated
 * separately (see eenv_task_busy_time). This ensures:
 *
 *   - A stable PD utilization, no matter which CPU of that PD we want to place
 *     the task on.
 *
 *   - A fair comparison between CPUs as the task contribution (task_util())
 *     will always be the same no matter which CPU utilization we rely on
 *     (util_avg or util_est).
 *
 * Set @eenv busy time for the PD that spans @pd_cpus. This busy time can't
 * exceed @eenv->pd_cap.
 */
static inline void eenv_pd_busy_time(int gear_idx, struct energy_env *eenv,
				struct cpumask *pd_cpus,
				struct task_struct *p)
{
	unsigned long busy_time = 0;
	int cpu;

	if (eenv->pds_busy_time[gear_idx] != -1)
		return;

	for_each_cpu(cpu, pd_cpus) {
		unsigned long util = cpu_util_next(cpu, p, -1);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		busy_time += mtk_cpu_util(cpu, util, ENERGY_UTIL,
					NULL, 0, 1024);
#else
		busy_time += effective_cpu_util(cpu, util, ENERGY_UTIL, NULL);
#endif
	}

	eenv->pds_busy_time[gear_idx] = min(eenv->pds_cap[gear_idx], busy_time);
}

inline int reasonable_temp(int temp)
{
	if ((temp > 125) || (temp < -40))
		return 0;

	return 1;
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static inline void eenv_init(struct energy_env *eenv,
			struct task_struct *p, int prev_cpu, struct perf_domain *pd)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned int cpu, pd_idx, pd_cnt;
	struct perf_domain *pd_ptr = pd;
	unsigned int gear_idx;
	struct dsu_info *dsu;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;

	eenv_task_busy_time(eenv, p, prev_cpu);

	pd_cnt = get_nr_gears();
	for (pd_idx = 0; pd_idx < pd_cnt; pd_idx++) {
		eenv->pds_busy_time[pd_idx] =  -1;
		eenv->pds_max_util[pd_idx][0] =  -1;
		eenv->pds_max_util[pd_idx][1] =  -1;
		eenv->pds_cpu_cap[pd_idx] = -1;
		eenv->pds_cap[pd_idx] = -1;
	}

	eenv->wl_support = get_eas_dsu_ctrl();
	eenv->total_util = 0;
	for (; pd_ptr; pd_ptr = pd_ptr->next) {
		unsigned long cpu_thermal_cap;

		cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
		if (cpumask_empty(cpus))
			continue;

		/* Account thermal pressure for the energy estimation */
		cpu = cpumask_first(cpus);
		cpu_thermal_cap = arch_scale_cpu_capacity(cpu);
		/* copy arch_scale_thermal_pressure() code and add read_once to avoid data-racing */
		cpu_thermal_cap -= READ_ONCE(per_cpu(thermal_pressure, cpu));

		gear_idx = topology_cluster_id(cpu);
		eenv->pds_cpu_cap[gear_idx] = cpu_thermal_cap;
		eenv->pds_cap[gear_idx] = 0;
		for_each_cpu(cpu, cpus) {
			eenv->pds_cap[gear_idx] += cpu_thermal_cap;
		}

		if (trace_sched_energy_init_enabled()) {
			trace_sched_energy_init(cpus, gear_idx, eenv->pds_cpu_cap[gear_idx],
				eenv->pds_cap[gear_idx]);
		}

		if (eenv->wl_support) {
			eenv_pd_busy_time(gear_idx, eenv, cpus, p);
			eenv->total_util += eenv->pds_busy_time[gear_idx];
		}
	}

	for_each_cpu(cpu, cpu_possible_mask) {
		eenv->cpu_temp[cpu] = get_cpu_temp(cpu);
		eenv->cpu_temp[cpu] /= 1000;

		if (!reasonable_temp(eenv->cpu_temp[cpu])) {
			if (trace_sched_check_temp_enabled())
				trace_sched_check_temp("cpu", cpu, eenv->cpu_temp[cpu]);
		}
	}

	if (eenv->wl_support) {
		eenv->wl_type = get_em_wl();

		dsu = &(eenv->dsu);
		dsu->dsu_bw = get_pelt_dsu_bw();
		dsu->emi_bw = get_pelt_emi_bw();
		dsu->temp = get_dsu_temp()/1000;
		if (!reasonable_temp(dsu->temp)) {
			if (trace_sched_check_temp_enabled())
				trace_sched_check_temp("dsu", -1, dsu->temp);
		}

		eenv->dsu_freq_thermal = get_dsu_ceiling_freq();
		eenv->dsu_freq_base = mtk_get_dsu_freq();
		dsu_opp = dsu_get_freq_opp(eenv->dsu_freq_base);
		dsu_ps = dsu_get_opp_ps(eenv->wl_type, dsu_opp);
		eenv->dsu_volt_base = dsu_ps->volt;

		if (trace_sched_eenv_init_enabled())
			trace_sched_eenv_init(eenv->dsu_freq_base, eenv->dsu_volt_base,
					eenv->dsu_freq_thermal, share_buck.gear_idx);
	} else
		eenv->wl_type = 0;
}

/*
 * Compute the maximum utilization for compute_energy() when the task @p
 * is placed on the cpu @dst_cpu.
 *
 * Returns the maximum utilization among @eenv->cpus. This utilization can't
 * exceed @eenv->cpu_cap.
 */
static inline unsigned long
eenv_pd_max_util(int gear_idx, struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu)
{
	unsigned long max_util = 0, max_util_base = 0;
	int cpu, dst_idx = 0;

	if (dst_cpu != -1)
		dst_idx = 1;

	if (eenv->pds_max_util[gear_idx][dst_idx] != -1)
		return  eenv->pds_max_util[gear_idx][dst_idx];

	for_each_cpu(cpu, pd_cpus) {
		struct task_struct *tsk = (cpu == dst_cpu) ? p : NULL;
		unsigned long util = cpu_util_next(cpu, p, dst_cpu);
		unsigned long cpu_util;

		/*
		 * Performance domain frequency: utilization clamping
		 * must be considered since it affects the selection
		 * of the performance domain frequency.
		 * NOTE: in case RT tasks are running, by default the
		 * FREQUENCY_UTIL's utilization can be max OPP.
		 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		if (tsk)
			cpu_util = mtk_cpu_util(cpu, util, FREQUENCY_UTIL, tsk, eenv->min_cap,
					eenv->max_cap);
		else
			cpu_util = mtk_cpu_util(cpu, util, FREQUENCY_UTIL, tsk, 0, 1024);
#else
		cpu_util = effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif
		max_util = max(max_util, cpu_util);
		if (trace_sched_max_util_enabled())
			trace_sched_max_util(gear_idx, dst_cpu, max_util, cpu, util, cpu_util);

		if (dst_cpu != -1) {
			unsigned long util_base = cpu_util_next(cpu, p, -1);
			unsigned long cpu_util_base;

			cpu_util_base = mtk_cpu_util(cpu, util_base, FREQUENCY_UTIL, NULL,
						0, 1024);
			max_util_base = max(max_util_base, cpu_util_base);
			if (trace_sched_max_util_enabled())
				trace_sched_max_util(gear_idx, -1, max_util_base, cpu, util_base,
						cpu_util_base);
		}


	}

	eenv->pds_max_util[gear_idx][dst_idx] =  min(max_util, eenv->pds_cpu_cap[gear_idx]);
	if (dst_cpu != -1)
		eenv->pds_max_util[gear_idx][0] =  min(max_util_base, eenv->pds_cpu_cap[gear_idx]);


	if (trace_sched_max_util_enabled()) {
		trace_sched_max_util(gear_idx, -1, eenv->pds_max_util[gear_idx][0], -1, 0, 0);
		trace_sched_max_util(gear_idx, dst_cpu, eenv->pds_max_util[gear_idx][1], -1, 0, 0);
	}


	return eenv->pds_max_util[gear_idx][dst_idx];
}

static inline unsigned long
mtk_compute_energy_cpu(int gear_idx, struct energy_env *eenv, struct perf_domain *pd,
		       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long max_util = eenv_pd_max_util(gear_idx, eenv, pd_cpus, p, dst_cpu);
	unsigned long busy_time = eenv->pds_busy_time[gear_idx];
	unsigned long energy;

	if (dst_cpu >= 0)
		busy_time = min(eenv->pds_cap[gear_idx], busy_time + eenv->task_busy_time);

	energy =  mtk_em_cpu_energy(gear_idx, pd->em_pd, max_util, busy_time,
			eenv->pds_cpu_cap[gear_idx], eenv);

	if (trace_sched_compute_energy_enabled())
		trace_sched_compute_energy(dst_cpu, gear_idx, pd_cpus, energy, max_util, busy_time);

	return energy;
}

struct share_buck_info share_buck;
int get_share_buck(void)
{
	return share_buck.gear_idx;
}
EXPORT_SYMBOL_GPL(get_share_buck);

int init_share_buck(void)
{
	int ret;
	struct device_node *eas_node;

	/* Default share buck gear_idx=0 */
	share_buck.gear_idx = 0;
	eas_node = of_find_node_by_name(NULL, "eas-info");
	if (eas_node == NULL)
		pr_info("failed to find node @ %s\n", __func__);
	else {
		ret = of_property_read_u32(eas_node, "share-buck", &share_buck.gear_idx);
		if (ret < 0)
			pr_info("no share_buck err_code=%d %s\n", ret,  __func__);
	}

	share_buck.cpus = get_gear_cpumask(share_buck.gear_idx);


	return 0;
}

static inline int shared_gear(int gear_idx)
{
	return gear_idx == share_buck.gear_idx;
}

static inline unsigned long
mtk_compute_energy_cpu_dsu(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long cpu_pwr = 0, dsu_pwr = 0;
	unsigned long shared_pwr_base, shared_pwr_new, delta_share_pwr = 0;
	struct dsu_info *dsu = &eenv->dsu;

	dsu->dsu_freq = eenv->dsu_freq_base;
	dsu->dsu_volt = eenv->dsu_volt_base;
	cpu_pwr = mtk_compute_energy_cpu(eenv->gear_idx, eenv, pd, pd_cpus, p, dst_cpu);

	if ((eenv->dsu_freq_new  > eenv->dsu_freq_base) && !(shared_gear(eenv->gear_idx))
			&& share_buck.gear_idx != -1) {
		struct root_domain *rd = this_rq()->rd;
		struct perf_domain *pd_ptr, *share_buck_pd = 0;

		rcu_read_lock();
		pd_ptr = rcu_dereference(rd->pd);
		if (!pd_ptr)
			goto calc_sharebuck_done;

		for (; pd_ptr; pd_ptr = pd_ptr->next) {
			struct cpumask *pd_mask = perf_domain_span(pd_ptr);
			unsigned int cpu = cpumask_first(pd_mask);

			if (share_buck.gear_idx == topology_cluster_id(cpu)) {
				share_buck_pd = pd_ptr;
				break;
			}
		}

		if (!share_buck_pd)
			goto calc_sharebuck_done;

		/* calculate share_buck gear pwr with new DSU freq */
		dsu->dsu_freq  = eenv->dsu_freq_new;
		dsu->dsu_volt = eenv->dsu_volt_new;
		shared_pwr_new = mtk_compute_energy_cpu(share_buck.gear_idx, eenv, share_buck_pd,
							share_buck.cpus, p, -1);

		/* calculate share_buck gear pwr with new old freq */
		dsu->dsu_freq = eenv->dsu_freq_base;
		dsu->dsu_volt = eenv->dsu_volt_base;
		shared_pwr_base = mtk_compute_energy_cpu(share_buck.gear_idx, eenv, share_buck_pd,
							share_buck.cpus, p, -1);

		delta_share_pwr = max(shared_pwr_new, shared_pwr_base) - shared_pwr_base;
calc_sharebuck_done:
		rcu_read_unlock();
	}

	if (dst_cpu != -1) {
		if (trace_sched_compute_energy_dsu_enabled())
			trace_sched_compute_energy_dsu(dst_cpu, eenv->task_busy_time,
				eenv->pds_busy_time[eenv->gear_idx], dsu->dsu_bw, dsu->emi_bw,
					dsu->temp, dsu->dsu_freq, dsu->dsu_volt);

		dsu_pwr = get_dsu_pwr(eenv->wl_type, dst_cpu, eenv->task_busy_time,
						eenv->total_util, dsu);
	}

	if (trace_sched_compute_energy_cpu_dsu_enabled())
		trace_sched_compute_energy_cpu_dsu(dst_cpu, cpu_pwr, delta_share_pwr,
					dsu_pwr, cpu_pwr + delta_share_pwr + dsu_pwr);

	return cpu_pwr + delta_share_pwr + dsu_pwr;
}

/*
 * compute_energy(): Use the Energy Model to estimate the energy that @pd would
 * consume for a given utilization landscape @eenv. When @dst_cpu < 0, the task
 * contribution is ignored.
 */
static inline unsigned long
mtk_compute_energy(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{

	if (eenv->wl_support)
		return mtk_compute_energy_cpu_dsu(eenv, pd, pd_cpus, p, dst_cpu);
	else
		return mtk_compute_energy_cpu(eenv->gear_idx, eenv, pd, pd_cpus, p, dst_cpu);
}
#endif

static unsigned int uclamp_min_ls;
void set_uclamp_min_ls(unsigned int val)
{
	uclamp_min_ls = val;
}
EXPORT_SYMBOL_GPL(set_uclamp_min_ls);

unsigned int get_uclamp_min_ls(void)
{
	return uclamp_min_ls;
}
EXPORT_SYMBOL_GPL(get_uclamp_min_ls);

/*
 * attach_task() -- attach the task detached by detach_task() to its new rq.
 */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	attach_task(rq, p);
	rq_unlock(rq, &rf);
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static void __sched_fork_init(struct task_struct *p)
{
	struct soft_affinity_task *sa_task = &((struct mtk_task *)
		p->android_vendor_data1)->sa_task;

	sa_task->latency_sensitive = false;
	cpumask_copy(&sa_task->soft_cpumask, cpu_possible_mask);
}

static void android_rvh_sched_fork_init(void *unused, struct task_struct *p)
{
	__sched_fork_init(p);
}

void init_task_soft_affinity(void)
{
	struct task_struct *g, *p;
	int ret;

	/* init soft affinity related value to exist tasks */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		__sched_fork_init(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	/* init soft affinity related value to newly forked tasks */
	ret = register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	if (ret)
		pr_info("register sched_fork_init hooks failed, returned %d\n", ret);
}

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

void _init_tg_mask(struct cgroup_subsys_state *css)
{
	struct task_group *tg = css_tg(css);
	struct soft_affinity_tg *sa_tg = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg;

	cpumask_copy(&sa_tg->soft_cpumask, cpu_possible_mask);
}

void init_tg_soft_affinity(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	/* init soft affinity related value to exist cgroups */
	rcu_read_lock();
	_init_tg_mask(&root_task_group.css);
	css_for_each_child(css, top_css)
		_init_tg_mask(css);
	rcu_read_unlock();

	/* init soft affinity related value to newly created cgroups in vip.c*/
}

void soft_affinity_init(void)
{
	init_task_soft_affinity();
	init_tg_soft_affinity();
}

struct cpumask bit_to_cpumask(unsigned int cpumask_val)
{
	unsigned long cpumask_ulval = cpumask_val;
	struct cpumask cpumask_setting;
	int cpu;

	cpumask_clear(&cpumask_setting);
	for_each_possible_cpu(cpu) {
		if (test_bit(cpu, &cpumask_ulval))
			cpumask_set_cpu(cpu, &cpumask_setting);
	}
	return cpumask_setting;
}

void __set_group_prefer_cpus(struct task_group *tg, unsigned int cpumask_val)
{
	struct cpumask *tg_mask;
	struct cpumask soft_cpumask;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	soft_cpumask = bit_to_cpumask(cpumask_val);

	cpumask_copy(tg_mask, &soft_cpumask);
}

struct task_group *search_tg_by_name(char *group_name)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (!strcmp(css->cgroup->kn->name, group_name)) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

void set_group_prefer_cpus_by_name(unsigned int cpumask_val, char *group_name)
{
	struct task_group *tg = search_tg_by_name(group_name);

	if (tg == &root_task_group)
		return;

	__set_group_prefer_cpus(tg, cpumask_val);
}

void get_task_group_cpumask_by_name(char *group_name, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_name(group_name);
	struct cpumask *tg_mask;

	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;
	cpumask_copy(__tg_mask, tg_mask);
}

struct task_group *search_tg_by_cpuctl_id(unsigned int cpuctl_id)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (css->id == cpuctl_id) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

/* start of soft affinity interface */
int set_group_prefer_cpus(unsigned int cpuctl_id, unsigned int cpumask_val)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);

	if (tg == &root_task_group)
		return 0;

	__set_group_prefer_cpus(tg, cpumask_val);
	return 1;
}
EXPORT_SYMBOL_GPL(set_group_prefer_cpus);

void set_top_app_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "top-app");
}
EXPORT_SYMBOL_GPL(set_top_app_cpumask);

void set_foreground_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "foreground");
}
EXPORT_SYMBOL_GPL(set_foreground_cpumask);

void set_background_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "background");
}
EXPORT_SYMBOL_GPL(set_background_cpumask);

void get_group_prefer_cpus(unsigned int cpuctl_id, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);
	struct cpumask *tg_mask;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	cpumask_copy(__tg_mask, tg_mask);
}
EXPORT_SYMBOL_GPL(get_group_prefer_cpus);

struct cpumask top_app_cpumask;
struct cpumask foreground_cpumask;
struct cpumask background_cpumask;
struct cpumask *get_top_app_cpumask(void)
{
	get_task_group_cpumask_by_name("top-app", &top_app_cpumask);
	return &top_app_cpumask;
}
EXPORT_SYMBOL_GPL(get_top_app_cpumask);

struct cpumask *get_foreground_cpumask(void)
{
	get_task_group_cpumask_by_name("foreground", &foreground_cpumask);
	return &foreground_cpumask;
}
EXPORT_SYMBOL_GPL(get_foreground_cpumask);

struct cpumask *get_background_cpumask(void)
{
	get_task_group_cpumask_by_name("background", &background_cpumask);
	return &background_cpumask;
}
EXPORT_SYMBOL_GPL(get_background_cpumask);

void set_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = true;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls);

void unset_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = false;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls);

void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	struct cpumask soft_cpumask = bit_to_cpumask(cpumask_val);

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = true;
		cpumask_copy(&sa_task->soft_cpumask, &soft_cpumask);
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls_prefer_cpus);

void unset_task_ls_prefer_cpus(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	struct cpumask soft_cpumask = bit_to_cpumask(0xff);

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
		sa_task->latency_sensitive = false;
		cpumask_copy(&sa_task->soft_cpumask, &soft_cpumask);
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls_prefer_cpus);
/* end of soft affinity interface */

#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg;

	if (!css)
		return false;
	tg = container_of(css, struct task_group, css);

	return tg->latency_sensitive;
}
#else
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	return false;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

bool is_task_ls_uclamp(struct task_struct *p)
{
	bool latency_sensitive = false;

	if (!uclamp_min_ls)
		latency_sensitive = cloned_uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					cloned_uclamp_latency_sensitive(p);
	}
	return latency_sensitive;
}

inline bool is_task_latency_sensitive(struct task_struct *p)
{
	struct soft_affinity_task *sa_task;
	bool latency_sensitive = false;

	sa_task = &((struct mtk_task *) p->android_vendor_data1)->sa_task;
	rcu_read_lock();
	latency_sensitive = sa_task->latency_sensitive || is_task_ls_uclamp(p);
	rcu_read_unlock();

	return latency_sensitive;
}
EXPORT_SYMBOL_GPL(is_task_latency_sensitive);

inline void compute_effective_softmask(struct task_struct *p,
		bool *latency_sensitive, struct cpumask *dst_mask)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct cpumask *task_mask;
	struct cpumask *tg_mask;

	*latency_sensitive = is_task_latency_sensitive(p);
	css = task_css(p, cpu_cgrp_id);
	if (!*latency_sensitive || !css) {
		cpumask_copy(dst_mask, cpu_possible_mask);
		return;
	}

	tg = container_of(css, struct task_group, css);
	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;

	task_mask = &((struct mtk_task *) p->android_vendor_data1)->sa_task.soft_cpumask;

	if (!cpumask_and(dst_mask, task_mask, tg_mask)) {
		cpumask_copy(dst_mask, tg_mask);
		return;
	}
}

void mtk_can_migrate_task(void *data, struct task_struct *p,
	int dst_cpu, int *can_migrate)
{
	bool latency_sensitive;
	struct cpumask eff_mask;

	if (READ_ONCE(cpu_rq(task_cpu(p))->rd->overutilized)) {
		*can_migrate = 1;
		return;
	}

	compute_effective_softmask(p, &latency_sensitive, &eff_mask);
	if (latency_sensitive && !(cpumask_test_cpu(dst_cpu, &eff_mask)))
		*can_migrate = 0;
}

__read_mostly int num_sched_clusters;
cpumask_t __read_mostly ***cpu_array;

void init_cpu_array(void)
{
	int i, j;
	num_sched_clusters = get_nr_gears();

	cpu_array = kcalloc(num_sched_clusters, sizeof(cpumask_t **),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		free_cpu_array();

	for (i = 0; i < num_sched_clusters; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t *),
				GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			free_cpu_array();

		for (j = 0; j < num_sched_clusters; j++) {
			cpu_array[i][j] = kcalloc(2, sizeof(cpumask_t),
					GFP_ATOMIC | __GFP_NOFAIL);
			if (!cpu_array[i][j])
				free_cpu_array();
		}
	}
}

void build_cpu_array(void)
{
	int i;

	if (!cpu_array)
		free_cpu_array();

	/* Construct cpu_array row by row */
	for (i = 0; i < num_sched_clusters; i++) {
		int j, k = 1;

		/* Fill out first column with appropriate cpu arrays */
		cpumask_copy(&cpu_array[i][0][0], get_gear_cpumask(i));
		cpumask_copy(&cpu_array[i][0][1], get_gear_cpumask(i));
		/*
		 * k starts from column 1 because 0 is filled
		 * Fill clusters for the rest of the row,
		 * above i in ascending order
		 */
		for (j = i + 1; j < num_sched_clusters; j++) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][j][1], get_gear_cpumask(j));
			k++;
		}

		/*
		 * k starts from where we left off above.
		 * Fill cluster below i in descending order.
		 */
		for (j = i - 1; j >= 0; j--) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][i-j][1], get_gear_cpumask(j));
			k++;
		}
	}
}

void free_cpu_array(void)
{
	int i, j;

	if (!cpu_array)
		return;

	for (i = 0; i < num_sched_clusters; i++) {
		for (j = 0; j < num_sched_clusters; j++) {
			kfree(cpu_array[i][j]);
			cpu_array[i][j] = NULL;
		}
		kfree(cpu_array[i]);
		cpu_array[i] = NULL;
	}
	kfree(cpu_array);
	cpu_array = NULL;
}

static int mtk_wake_affine_idle(int this_cpu, int prev_cpu, int sync)
{
	if (available_idle_cpu(this_cpu) && mtk_cpus_share_cache(this_cpu, prev_cpu))
		return available_idle_cpu(prev_cpu) ? prev_cpu : this_cpu;

	if (sync && cpu_rq(this_cpu)->nr_running == 1)
		return this_cpu;

	if (available_idle_cpu(prev_cpu))
		return prev_cpu;

	return nr_cpumask_bits;
}

static inline unsigned long cfs_rq_load_avg(struct cfs_rq *cfs_rq)
{
	return cfs_rq->avg.load_avg;
}

static unsigned long cpu_load(struct rq *rq)
{
	return cfs_rq_load_avg(&rq->cfs);
}

#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
static void update_cfs_rq_h_load(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct sched_entity *se = cfs_rq->tg->se[cpu_of(rq)];
	unsigned long now = jiffies;
	unsigned long load;

	if (cfs_rq->last_h_load_update == now)
		return;

	WRITE_ONCE(cfs_rq->h_load_next, NULL);
	for (; se; se = se->parent) {
		cfs_rq = cfs_rq_of(se);
		WRITE_ONCE(cfs_rq->h_load_next, se);
		if (cfs_rq->last_h_load_update == now)
			break;
	}

	if (!se) {
		cfs_rq->h_load = cfs_rq_load_avg(cfs_rq);
		cfs_rq->last_h_load_update = now;
	}

	while ((se = READ_ONCE(cfs_rq->h_load_next)) != NULL) {
		load = cfs_rq->h_load;
		load = div64_ul(load * se->avg.load_avg,
				cfs_rq_load_avg(cfs_rq) + 1);
		cfs_rq = group_cfs_rq(se);
		cfs_rq->h_load = load;
		cfs_rq->last_h_load_update = now;
	}
}

static unsigned long task_h_load(struct task_struct *p)
{
	struct cfs_rq *cfs_rq = task_cfs_rq(p);

	update_cfs_rq_h_load(cfs_rq);
	return div64_ul(p->se.avg.load_avg * cfs_rq->h_load,
			cfs_rq_load_avg(cfs_rq) + 1);
}
#else
static unsigned long task_h_load(struct task_struct *p)
{
	return p->se.avg.load_avg;
}
#endif

static int mtk_wake_affine_weight(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	s64 this_eff_load, prev_eff_load;
	unsigned long task_load;
	//struct sched_domain *sd = NULL;

	/* since "sched_domain_mutex undefined" build error, comment search sched_domain */
	/* find least shared sched_domain for this_cpu and prev_cpu */
	//for_each_domain(this_cpu, sd) {
	//	if ((sd->flags & SD_WAKE_AFFINE) &&
	//		cpumask_test_cpu(prev_cpu, sched_domain_span(sd)))
	//		break;
	//}
	//if (unlikely(!sd))
	//	return nr_cpumask_bits;

	this_eff_load = cpu_load(cpu_rq(this_cpu));

	if (sync) {
		unsigned long current_load = task_h_load(current);

		if (current_load > this_eff_load)
			return this_cpu;

		this_eff_load -= current_load;
	}

	task_load = task_h_load(p);

	this_eff_load += task_load;
	if (sched_feat(WA_BIAS))
		this_eff_load *= 100;
	this_eff_load *= capacity_of(prev_cpu);

	prev_eff_load = cpu_load(cpu_rq(prev_cpu));
	prev_eff_load -= task_load;

	/* since "sched_domain_mutex undefined" build error, replace imbalance_pct with its val 117
	 * prev_eff_load *= 100 + (sd->imbalance_pct - 100) / 2;
	 */
	if (sched_feat(WA_BIAS))
		prev_eff_load *= 100 + (117 - 100) / 2;
	prev_eff_load *= capacity_of(this_cpu);

	/*
	 * If sync, adjust the weight of prev_eff_load such that if
	 * prev_eff == this_eff that select_idle_sibling() will consider
	 * stacking the wakee on top of the waker if no other CPU is
	 * idle.
	 */
	if (sync)
		prev_eff_load += 1;

	return this_eff_load < prev_eff_load ? this_cpu : nr_cpumask_bits;
}

static int mtk_wake_affine(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	int target = nr_cpumask_bits;

	if (sched_feat(WA_IDLE))
		target = mtk_wake_affine_idle(this_cpu, prev_cpu, sync);

	if (sched_feat(WA_WEIGHT) && target == nr_cpumask_bits)
		target = mtk_wake_affine_weight(p, this_cpu, prev_cpu, sync);

	if (target == nr_cpumask_bits)
		return prev_cpu;

	return target;
}

/*
 * Scan the asym_capacity domain for idle CPUs; pick the first idle one on whi
 * the task fits. If no CPU is big enough, but there are idle ones, try to
 * maximize capacity.
 */
static int
mtk_select_idle_capacity(struct task_struct *p, struct cpumask *allowed_cpumask, int target)
{
	unsigned long task_util, best_cap = 0;
	int cpu, best_cpu = -1;

	task_util = uclamp_task_util(p);

	for_each_cpu_wrap(cpu, allowed_cpumask, target) {
		unsigned long cpu_cap = capacity_of(cpu);

		if (!available_idle_cpu(cpu) && !sched_idle_cpu(cpu))
			continue;
		if (fits_capacity(task_util, cpu_cap, get_adaptive_margin(cpu)))
			return cpu;

		if (cpu_cap > best_cap) {
			best_cap = cpu_cap;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

static struct cpumask bcpus;
static unsigned long util_Th;

void get_most_powerful_pd_and_util_Th(void)
{
	unsigned int nr_gear = get_nr_gears();

	/* no mutliple pd */
	if (nr_gear <= 1) {
		util_Th = 0;
		cpumask_clear(&bcpus);
		return;
	}

	/* pd_capacity_tbl is sorted by ascending order,
	 * so nr_gear-1 is most powerful gear and
	 * nr_gear is the second powerful gear.
	 */
	cpumask_copy(&bcpus, get_gear_cpumask(nr_gear-1));
	/* threshold is set to large capacity in mcpus */
	util_Th = pd_get_opp_capacity(
		cpumask_first(get_gear_cpumask(nr_gear-2)), 0);

}

bool gear_hints_enable;
static inline bool task_can_skip_this_cpu(struct task_struct *p, unsigned long p_uclamp_min,
		bool latency_sensitive, int cpu, struct cpumask *bcpus)
{
	bool cpu_in_bcpus;
	unsigned long task_util;
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	if (latency_sensitive)
		return 0;

	if (gear_hints_enable &&
		ghts->gear_start >= 0 &&
		ghts->gear_start <= num_sched_clusters-1)
		return 0;

	if (p_uclamp_min > 0)
		return 0;

	if (cpumask_empty(bcpus))
		return 0;

	cpu_in_bcpus = cpumask_test_cpu(cpu, bcpus);
	task_util = task_util_est(p);
	if (!cpu_in_bcpus || !fits_capacity(task_util, util_Th, get_adaptive_margin(cpu)))
		return 0;

	if (cpu_in_bcpus && task_is_vip(p, VVIP))
		return 0;

	return 1;
}

static inline bool is_target_max_spare_cpu(bool is_vip, unsigned int num_vip, unsigned int min_num_vip,
			long spare_cap, long target_max_spare_cap,
			int best_cpu, int new_cpu, const char *type)
{
	bool replace = true;

	if (is_vip) {
		if (num_vip > min_num_vip) {
			replace = false;
			goto out;
		}

		if (num_vip == min_num_vip &&
				spare_cap <= target_max_spare_cap) {
			replace = false;
			goto out;
		}
	} else {
		if (spare_cap <= target_max_spare_cap) {
			replace = false;
			goto out;
		}
	}

out:
	if (trace_sched_target_max_spare_cpu_enabled())
		trace_sched_target_max_spare_cpu(type, best_cpu, new_cpu, replace,
			is_vip, num_vip, min_num_vip, spare_cap, target_max_spare_cap);

	return replace;
}

void init_gear_hints(void)
{
	gear_hints_enable = sched_gear_hints_enable_get();
}

void __set_gear_indices(struct task_struct *p, int gear_start, int num_gear, int reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	ghts->gear_start = gear_start;
	ghts->num_gear   = num_gear;
	ghts->reverse    = reverse;
}

int set_gear_indices(int pid, int gear_start, int num_gear, int reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	/* check gear_start validity */
	if (gear_start < -1 || gear_start > num_sched_clusters-1)
		goto done;

	/* check num_gear validity */
	if ((num_gear != -1 && num_gear < 1) || num_gear > num_sched_clusters)
		goto done;

	/* check num_gear validity */
	if (reverse < 0 || reverse > 1)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}

	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_gear_indices);

int unset_gear_indices(int pid)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, GEAR_HINT_UNSET, GEAR_HINT_UNSET, 0);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_gear_indices);

void __get_gear_indices(struct task_struct *p, int *gear_start, int *num_gear, int *reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	*gear_start = ghts->gear_start;
	*num_gear = ghts->num_gear;
	*reverse = ghts->reverse;
}

int get_gear_indices(int pid, int *gear_start, int *num_gear, int *reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__get_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_gear_indices);

#if IS_ENABLED(CONFIG_MTK_SCHED_UPDOWN_MIGRATE)

bool updown_migration_enable;
void init_updown_migration(void)
{
	updown_migration_enable = sched_updown_migration_enable_get();
}

/* Default Migration margin */
bool adaptive_margin_enabled[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = true /* default on */
};
unsigned int sched_capacity_up_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};
unsigned int sched_capacity_down_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};

int set_updown_migration_pct(int gear_idx, int dn_pct, int up_pct)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	/* check pct validity */
	if (dn_pct < 1 || dn_pct > 100)
		goto done;

	if (up_pct < 1 || up_pct > 100)
		goto done;

	if (dn_pct > up_pct)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / up_pct;
		sched_capacity_down_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / dn_pct;
		adaptive_margin_enabled[cpu] = false;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_updown_migration_pct);

int unset_updown_migration_pct(int gear_idx)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] = 1024;
		sched_capacity_down_margin[cpu] = 1024;
		adaptive_margin_enabled[cpu] = true;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_updown_migration_pct);

int get_updown_migration_pct(int gear_idx, int *dn_pct, int *up_pct)
{
	int ret = 0, cpu;

	*dn_pct = *up_pct = -1;
	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpu = cpumask_first(get_gear_cpumask(gear_idx));

	*dn_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_down_margin[cpu];
	*up_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_up_margin[cpu];

	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_updown_migration_pct);

static inline bool task_demand_fits(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	unsigned int margin;
	bool AM_enabled;
	unsigned int sugov_margin;
	unsigned long dst_capacity = capacity_orig_of(dst_cpu);
	unsigned long src_capacity = capacity_orig_of(src_cpu);

	if (dst_capacity == SCHED_CAPACITY_SCALE)
		return true;

	/* if updown_migration is not enabled */
	if (!updown_migration_enable)
		return task_fits_capacity(p, dst_capacity, get_adaptive_margin(dst_cpu));

	/*
	 * Derive upmigration/downmigration margin wrt the src/dst CPU.
	 */
	if (src_capacity > dst_capacity) {
		margin = sched_capacity_down_margin[dst_cpu];
		AM_enabled = adaptive_margin_enabled[dst_cpu];
	} else {
		margin = sched_capacity_up_margin[src_cpu];
		AM_enabled = adaptive_margin_enabled[src_cpu];
	}

	/* bypass adaptive margin if capacity_updown_margin is enabled */
	sugov_margin = AM_enabled ? get_adaptive_margin(dst_cpu) : SCHED_CAPACITY_SCALE;

	return (dst_capacity * SCHED_CAPACITY_SCALE * SCHED_CAPACITY_SCALE
				> uclamp_task_util(p) * margin * sugov_margin);
}

static inline bool util_fits_capacity(unsigned long cpu_util, unsigned long cpu_cap, int cpu)
{
	unsigned long ceiling;
	bool AM_enabled = adaptive_margin_enabled[cpu], fit;
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;

	/* if updown_migration is not enabled */
	if (!updown_migration_enable)
		return fits_capacity(cpu_util, cpu_cap, get_adaptive_margin(cpu));

	ceiling = SCHED_CAPACITY_SCALE * capacity_orig_of(cpu) / sched_capacity_up_margin[cpu];

	fit = (min(ceiling, cpu_cap) * SCHED_CAPACITY_SCALE >= cpu_util * sugov_margin);

	if (trace_sched_fits_cap_ceiling_enabled())
		trace_sched_fits_cap_ceiling(fit, cpu, cpu_util, cpu_cap, ceiling, sugov_margin,
			sched_capacity_down_margin[cpu], sched_capacity_up_margin[cpu], AM_enabled);

	return fit;
}

#else
static inline bool task_demand_fits(struct task_struct *p, int cpu)
{
	unsigned long capacity = capacity_orig_of(cpu);

	if (capacity == SCHED_CAPACITY_SCALE)
		return true;

	return task_fits_capacity(p, capacity, get_adaptive_margin(cpu));
}

static inline bool util_fits_capacity(unsigned long cpu_util, unsigned long cpu_cap, int cpu)
{
	return fits_capacity(cpu_util, cpu_cap, get_adaptive_margin(cpu));
}
#endif

/*default value: gear_start = -1, num_gear = -1, reverse = 0 */
static inline bool gear_hints_unset(struct task_gear_hints *ghts)
{
	if (ghts->gear_start >= 0)
		return false;

	if (ghts->num_gear > 0 && ghts->num_gear <= num_sched_clusters)
		return false;

	if (ghts->reverse)
		return false;

	return true;
}

void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index,
		int *reverse)
{
	int i = 0;
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;
	int max_num_gear = -1;

	*order_index = 0;
	*end_index = 0;
	*reverse = 0;
	if (num_sched_clusters <= 1)
		goto out;

	/* gear_start's range -1~num_sched_clusters */
	if (ghts->gear_start > num_sched_clusters || ghts->gear_start < -1)
		goto out;
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	/* group based prefer MCPU */
	if (gear_hints_enable && gear_hints_unset(ghts) && group_get_gear_hint(p)) {
		*order_index = 1;
		*end_index = 0;
		*reverse = 1;
		goto out;
	}
#endif
	/* task has customized gear prefer */
	if (gear_hints_enable && ghts->gear_start >= 0) {
		*order_index = ghts->gear_start;
	} else {
		for (i = *order_index; i < num_sched_clusters - 1; i++) {
			if (task_demand_fits(p, cpumask_first(&cpu_array[i][0][0])))
				break;
		}

		*order_index = i;
	}

	if (gear_hints_enable && ghts->reverse)
		max_num_gear = *order_index + 1;
	else
		max_num_gear = num_sched_clusters - *order_index;

	if (gear_hints_enable && ghts->num_gear > 0 && ghts->num_gear <= max_num_gear)
		*end_index = ghts->num_gear - 1;
	else
		*end_index = max_num_gear - 1;

	if (gear_hints_enable)
		*reverse     = ghts->reverse;

out:
	if (trace_sched_get_gear_indices_enabled())
		trace_sched_get_gear_indices(p, uclamp_task_util(p), gear_hints_enable,
				ghts->gear_start, ghts->num_gear, ghts->reverse,
				num_sched_clusters, max_num_gear, *order_index, *end_index, *reverse);
}

struct find_best_candidates_parameters {
	bool in_irq;
	bool latency_sensitive;
	int prev_cpu;
	int order_index;
	int end_index;
	int reverse;
	int fbc_reason;
};

DEFINE_PER_CPU(cpumask_var_t, mtk_fbc_mask);

static void mtk_find_best_candidates(struct cpumask *candidates, struct task_struct *p,
		struct cpumask *effective_softmask, struct cpumask *allowed_cpu_mask,
		struct energy_env *eenv, struct find_best_candidates_parameters *fbc_params,
		int *sys_max_spare_cap_cpu, int *idle_max_spare_cap_cpu, unsigned long *cpu_utils)
{
	int cluster, cpu;
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_fbc_mask);
	unsigned long target_cap = 0;
	unsigned long cpu_cap, cpu_util, cpu_util_without_p;
	bool not_in_softmask;
	struct cpuidle_state *idle;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	unsigned long min_cap = eenv->min_cap;
	unsigned long max_cap = eenv->max_cap;
	bool is_vip = false;
	bool is_vvip = false;
	unsigned int num_vip, prev_min_num_vip, min_num_vip;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	struct vip_task_struct *vts;
	unsigned int (*num_vip_in_cpu_fn)(int cpu) = num_vip_in_cpu;
	int target_balance_cluster;
#endif
	bool latency_sensitive = fbc_params->latency_sensitive;
	bool in_irq = fbc_params->in_irq;
	int prev_cpu = fbc_params->prev_cpu;
	int order_index = fbc_params->order_index;
	int end_index = fbc_params->end_index;
	int reverse = fbc_params->reverse;

	num_vip = prev_min_num_vip = min_num_vip = UINT_MAX;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	vts->vip_prio = get_vip_task_prio(p);
	is_vip = task_is_vip(p, NOT_VIP);
	is_vvip = task_is_vip(p, VVIP);

	if (is_vvip) {
		target_balance_cluster = find_imbalanced_vvip_gear();
		if (target_balance_cluster != -1) {
			order_index = target_balance_cluster;
			end_index = 0;
			num_vip_in_cpu_fn = num_vvip_in_cpu;
		}
	}
#endif

	/* find best candidate */
	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		unsigned int uint_cpu;
		long spare_cap, spare_cap_without_p, pd_max_spare_cap = LONG_MIN;
		long pd_max_spare_cap_ls_idle = LONG_MIN;
		unsigned int pd_min_exit_lat = UINT_MAX;
		int pd_max_spare_cap_cpu = -1;
		int pd_max_spare_cap_cpu_ls_idle = -1;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[MAX_NR_CPUS]  ____cacheline_aligned, cnt, i;

#endif

		cpumask_and(cpus, &cpu_array[order_index][cluster][reverse], cpu_active_mask);

		if (cpumask_empty(cpus))
			continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(cpus, cpu_order, in_irq);

		for (i = 0; i < cnt; i++) {
			cpu = in_irq ? cpu_order[0] + i : cpu_order[i];
#else
		for_each_cpu(cpu, cpus) {
#endif
			track_sched_cpu_util(p, cpu, min_cap, max_cap);

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (cpu_high_irqload(cpu))
				continue;

			cpumask_set_cpu(cpu, allowed_cpu_mask);

			if (in_irq &&
				task_can_skip_this_cpu(p, min_cap, latency_sensitive, cpu, &bcpus))
				continue;

			if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
						!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
				continue;

			cpu_util = cpu_util_next(cpu, p, cpu);
			cpu_util_without_p = cpu_util_next(cpu, p, -1);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			spare_cap_without_p = cpu_cap;
			lsub_positive(&spare_cap, cpu_util);
			lsub_positive(&spare_cap_without_p, cpu_util_without_p);
			not_in_softmask = (latency_sensitive &&
						!cpumask_test_cpu(cpu, effective_softmask));

			if (not_in_softmask)
				continue;

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
			if (is_vip) {
				num_vip = num_vip_in_cpu_fn(cpu);
				if (!is_vvip && num_vvip_in_cpu(cpu))
					continue;

				if (num_vip > min_num_vip)
					continue;

				prev_min_num_vip = min_num_vip;
				min_num_vip = num_vip;
				/*don't choice CPU only because it can calc energy, choice min_num_vip CPU */
				if ((prev_min_num_vip != UINT_MAX) && (prev_min_num_vip != min_num_vip))
					cpumask_clear(candidates);
			}
#endif

			if (cpu == prev_cpu)
				spare_cap += spare_cap >> 6;

			if (is_target_max_spare_cpu(is_vip, num_vip, prev_min_num_vip,
					spare_cap_without_p, sys_max_spare_cap,
					*sys_max_spare_cap_cpu, cpu, "sys_max_spare")) {
				sys_max_spare_cap = spare_cap_without_p;
				*sys_max_spare_cap_cpu = cpu;
			}

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && available_idle_cpu(cpu)) {
				if (is_target_max_spare_cpu(is_vip, num_vip, prev_min_num_vip,
					spare_cap_without_p, idle_max_spare_cap,
					*idle_max_spare_cap_cpu, cpu, "idle_max_spare")) {
					idle_max_spare_cap = spare_cap_without_p;
					*idle_max_spare_cap_cpu = cpu;
				}
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with effective_cpu_util().
			 */
			uint_cpu = cpu;
			/* record pre-clamping cpu_util */
			cpu_utils[uint_cpu] = cpu_util;
			cpu_util = mtk_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p,
							min_cap, max_cap);

			if (trace_sched_util_fits_cpu_enabled())
				trace_sched_util_fits_cpu(cpu, cpu_utils[uint_cpu], cpu_util, cpu_cap,
						min_cap, max_cap, cpu_rq(cpu));

			/* replace with post-clamping cpu_util */
			cpu_utils[uint_cpu] = cpu_util;

			if (!util_fits_capacity(cpu_util, cpu_cap, cpu))
				continue;

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			if (!latency_sensitive && is_target_max_spare_cpu(is_vip, num_vip,
					prev_min_num_vip, spare_cap, pd_max_spare_cap,
					pd_max_spare_cap_cpu, cpu, "pd_max_spare")) {
				pd_max_spare_cap = spare_cap;
				pd_max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (available_idle_cpu(cpu)) {
				cpu_cap = capacity_orig_of(cpu);
				idle = idle_get_state(cpu_rq(cpu));
				if (idle && idle->exit_latency > pd_min_exit_lat &&
						cpu_cap == target_cap)
					continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
				if (!in_irq && idle && idle->exit_latency == pd_min_exit_lat
						&& cpu_cap == target_cap)
					continue;
#endif

				if (!is_target_max_spare_cpu(is_vip, num_vip, prev_min_num_vip,
					spare_cap, pd_max_spare_cap_ls_idle,
					pd_max_spare_cap_cpu_ls_idle, cpu, "pd_max_spare_is_idle"))
					continue;

				pd_min_exit_lat = idle ? idle->exit_latency : 0;

				pd_max_spare_cap_ls_idle = spare_cap;
				target_cap = cpu_cap;
				pd_max_spare_cap_cpu_ls_idle = cpu;
			}
		}

		if (pd_max_spare_cap_cpu_ls_idle != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu_ls_idle, candidates);
		else if (pd_max_spare_cap_cpu != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu, candidates);

		if ((cluster >= end_index) && (!cpumask_empty(candidates)))
			break;
		else if (is_vvip &&
			(*idle_max_spare_cap_cpu>=0 || *sys_max_spare_cap_cpu>=0)) {
			/*
			 * Don't calc energy if we found max spare cpu
			 * since we have search a target balance gear for VVIP
			 */
			break;
		}
	}

	if ((cluster > end_index) && !cpumask_empty(cpus))
		fbc_params->fbc_reason = LB_FAIL;

	if (trace_sched_find_best_candidates_enabled())
		trace_sched_find_best_candidates(p, is_vip, candidates, order_index, end_index,
				allowed_cpu_mask);
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static DEFINE_PER_CPU(cpumask_t, energy_cpus);

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync,
					int *new_cpu)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned long best_delta = ULONG_MAX;
	int this_cpu = smp_processor_id();
	struct root_domain *rd = cpu_rq(this_cpu)->rd;
	int sys_max_spare_cap_cpu = -1;
	int idle_max_spare_cap_cpu = -1;
	bool latency_sensitive = false;
	int best_energy_cpu = -1;
	unsigned int cpu;
	struct perf_domain *pd;
	int select_reason = -1, backup_reason = 0;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	struct energy_env eenv;
	struct cpumask effective_softmask;
	bool in_irq = in_interrupt();
	struct cpumask allowed_cpu_mask;
	int order_index, end_index, weight, reverse;
	cpumask_t *candidates;
	struct find_best_candidates_parameters fbc_params;
	unsigned long cpu_utils[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = ULONG_MAX};
	int recent_used_cpu, target;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	int min_num_vvip_cpu = -1;
	unsigned int num_vvip = 0, min_num_vvip_in_cpu = UINT_MAX;
#endif

	cpumask_clear(&allowed_cpu_mask);

	irq_log_store();

	rcu_read_lock();
	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

	pd = rcu_dereference(rd->pd);
	if (!pd || READ_ONCE(rd->overutilized)) {
		select_reason = LB_FAIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	if (sync && cpu_rq(this_cpu)->nr_running == 1 &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
	    task_fits_capacity(p, capacity_of(this_cpu), get_adaptive_margin(this_cpu)) &&
	    !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
		rcu_read_unlock();
		*new_cpu = this_cpu;
		select_reason = LB_SYNC;
		goto done;
	}

	irq_log_store();

	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	mtk_get_gear_indicies(p, &order_index, &end_index, &reverse);

	irq_log_store();

	eenv.min_cap = min_cap;
	eenv.max_cap = max_cap;
	if (!in_irq) {
		eenv_task_busy_time(&eenv, p, prev_cpu);

		eenv_init(&eenv, p, prev_cpu, pd);

		if (!eenv.task_busy_time) {
			select_reason = LB_ZERO_EENV_UTIL;
			rcu_read_unlock();
			goto fail;
		}
	}

	irq_log_store();

	fbc_params.in_irq = in_irq;
	fbc_params.latency_sensitive = latency_sensitive;
	fbc_params.prev_cpu = prev_cpu;
	fbc_params.order_index = order_index;
	fbc_params.end_index = end_index;
	fbc_params.reverse = reverse;
	fbc_params.fbc_reason = 0;

	/* Pre-select a set of candidate CPUs. */
	candidates = this_cpu_ptr(&energy_cpus);
	cpumask_clear(candidates);

	mtk_find_best_candidates(candidates, p, &effective_softmask, &allowed_cpu_mask,
		&eenv, &fbc_params, &sys_max_spare_cap_cpu, &idle_max_spare_cap_cpu, cpu_utils);

	irq_log_store();

	/* select best energy cpu in candidates */
	weight = cpumask_weight(candidates);
	if (!weight)
		goto unlock;
	if (weight == 1) {
		best_energy_cpu = cpumask_first(candidates);
		goto unlock;
	}

	for_each_cpu(cpu, candidates) {
		unsigned long cur_delta, base_energy;
		int gear_idx;
		unsigned int uint_cpu = cpu;
		struct perf_domain *target_pd = rcu_dereference(pd);

		/* Evaluate the energy impact of using this CPU. */
		if (unlikely(in_irq)) {
			int wl_type = get_em_wl();

			cur_delta = calc_pwr_eff(wl_type, cpu, cpu_utils[uint_cpu]);
			base_energy = 0;
		} else {
			target_pd = find_pd(target_pd, cpu);
			if (!target_pd)
				continue;

			gear_idx = eenv.gear_idx = topology_cluster_id(cpu);
			cpus = get_gear_cpumask(gear_idx);

			eenv_pd_busy_time(gear_idx, &eenv, cpus, p);
			cur_delta = mtk_compute_energy(&eenv, target_pd, cpus, p,
								cpu);
			base_energy = mtk_compute_energy(&eenv, target_pd, cpus, p, -1);
		}
		cur_delta = max(cur_delta, base_energy) - base_energy;
		if (cur_delta < best_delta) {
			best_delta = cur_delta;
			best_energy_cpu = cpu;
		}
	}

unlock:
	rcu_read_unlock();

	irq_log_store();

	if (latency_sensitive) {
		if (best_energy_cpu >= 0) {
			*new_cpu = best_energy_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU | fbc_params.fbc_reason;
			goto done;
		}
		if (idle_max_spare_cap_cpu >= 0) {
			*new_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
			goto done;
		}
		if (sys_max_spare_cap_cpu >= 0) {
			*new_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
			goto done;
		}
	}

	irq_log_store();

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (best_energy_cpu >= 0) {
		*new_cpu = best_energy_cpu;
		select_reason = LB_BEST_ENERGY_CPU | fbc_params.fbc_reason;
		goto done;
	}

	irq_log_store();

	if (sys_max_spare_cap_cpu >= 0) {
		*new_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto done;
	}

	irq_log_store();

fail:
	/* All cpu failed, even sys_max_spare_cap_cpu is not captured*/

	/* from overutilized & zero_util */
	if (cpumask_empty(&allowed_cpu_mask)) {
		cpumask_andnot(&allowed_cpu_mask, p->cpus_ptr, cpu_pause_mask);
		cpumask_and(&allowed_cpu_mask, &allowed_cpu_mask, cpu_active_mask);
	} else {
		select_reason = LB_FAIL_IN_REGULAR;
	}

	rcu_read_lock();

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	/* iterrate from biggest cpu, find CPU with minimum num VVIP.
	 * if all CPU have the same num of VVIP, min_num_vvip_cpu = biggest_cpu.
	 */
	if (task_is_vip(p, VVIP) && balance_vvip_overutilied && pd) {
		for (; pd; pd = pd->next) {
			cpumask_and(cpus, perf_domain_span(pd), &allowed_cpu_mask);
			for_each_cpu(cpu, cpus) {
				num_vvip = num_vvip_in_cpu(cpu);
				if (min_num_vvip_in_cpu > num_vvip) {
					min_num_vvip_cpu = cpu;
					min_num_vvip_in_cpu = num_vvip;
				}
			}
		}

		if (min_num_vvip_cpu != -1) {
			*new_cpu = min_num_vvip_cpu;
			backup_reason = LB_BACKUP_VVIP;
			goto backup_unlock;
		}
	}
#endif

	if (cpumask_test_cpu(this_cpu, p->cpus_ptr) && (this_cpu != prev_cpu))
		target = mtk_wake_affine(p, this_cpu, prev_cpu, sync);
	else
		target = prev_cpu;

	if (cpumask_test_cpu(target, &allowed_cpu_mask) &&
	    (available_idle_cpu(target) || sched_idle_cpu(target)) &&
	    task_fits_capacity(p, capacity_of(target), get_adaptive_margin(target))) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_IDLE_FIT;
		goto backup_unlock;
	}

	/*
	 * If the previous CPU fit_capacity and idle, don't be stupid:
	 */
	if (prev_cpu != target && mtk_cpus_share_cache(prev_cpu, target) &&
		cpumask_test_cpu(prev_cpu, &allowed_cpu_mask) &&
	    (available_idle_cpu(prev_cpu) || sched_idle_cpu(prev_cpu)) &&
	    task_fits_capacity(p, capacity_of(prev_cpu), get_adaptive_margin(prev_cpu))) {
		*new_cpu = prev_cpu;
		backup_reason = LB_BACKUP_PREV;
		goto backup_unlock;
	}

	irq_log_store();

	/*
	 * Allow a per-cpu kthread to stack with the wakee if the
	 * kworker thread and the tasks previous CPUs are the same.
	 * The assumption is that the wakee queued work for the
	 * per-cpu kthread that is now complete and the wakeup is
	 * essentially a sync wakeup. An obvious example of this
	 * pattern is IO completions.
	 */
	if (cpumask_test_cpu(this_cpu, &allowed_cpu_mask) &&
		is_per_cpu_kthread(current) && in_task() &&
	    prev_cpu == this_cpu &&
	    this_rq()->nr_running <= 1 &&
	    task_fits_capacity(p, capacity_of(this_cpu), get_adaptive_margin(this_cpu))) {
		*new_cpu = this_cpu;
		backup_reason = LB_BACKUP_CURR;
		goto backup_unlock;
	}

	irq_log_store();

	/* Check a recently used CPU as a potential idle candidate: */
	recent_used_cpu = p->recent_used_cpu;
	p->recent_used_cpu = prev_cpu;
	if (recent_used_cpu != prev_cpu && recent_used_cpu != target &&
		mtk_cpus_share_cache(recent_used_cpu, target) &&
		cpumask_test_cpu(recent_used_cpu, &allowed_cpu_mask) &&
	    (available_idle_cpu(recent_used_cpu) || sched_idle_cpu(recent_used_cpu)) &&
	    task_fits_capacity(p, capacity_of(recent_used_cpu), get_adaptive_margin(recent_used_cpu))) {
		*new_cpu = recent_used_cpu;
		/*
		 * Replace recent_used_cpu
		 * candidate for the next
		 */
		p->recent_used_cpu = prev_cpu;
		backup_reason = LB_BACKUP_RECENT_USED_CPU;
		goto backup_unlock;
	}

	irq_log_store();

	*new_cpu = mtk_select_idle_capacity(p, &allowed_cpu_mask, target);
	if (*new_cpu < nr_cpumask_bits) {
		backup_reason = LB_BACKUP_IDLE_CAP;
		goto backup_unlock;
	}

	if (*new_cpu == -1 && cpumask_test_cpu(target, p->cpus_ptr)) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_WITHOUT_IDLE_CAP;
		goto backup_unlock;
	}

	*new_cpu = -1;
backup_unlock:
	rcu_read_unlock();

done:
	irq_log_store();

	if (trace_sched_find_energy_efficient_cpu_enabled())
		trace_sched_find_energy_efficient_cpu(in_irq, best_delta, best_energy_cpu,
				best_energy_cpu, idle_max_spare_cap_cpu, sys_max_spare_cap_cpu);
	if (trace_sched_select_task_rq_enabled()) {
		trace_sched_select_task_rq(p, in_irq, select_reason, backup_reason, prev_cpu,
				*new_cpu, task_util(p), task_util_est(p), uclamp_task_util(p),
				latency_sensitive, sync, &effective_softmask);
	}
	if (trace_sched_effective_mask_enabled()) {
		struct soft_affinity_task *sa_task = &((struct mtk_task *)
			p->android_vendor_data1)->sa_task;
		struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
		struct cpumask softmask;

		if (css) {
			struct task_group *tg = container_of(css, struct task_group, css);
			struct soft_affinity_tg *sa_tg = &((struct mtk_tg *)
				tg->android_vendor_data1)->sa_tg;
			softmask = sa_tg->soft_cpumask;
		} else {
			cpumask_clear(&softmask);
			cpumask_copy(&softmask, cpu_possible_mask);
		}
		trace_sched_effective_mask(p, *new_cpu, latency_sensitive,
			&effective_softmask, &sa_task->soft_cpumask, &softmask);
	}

	irq_log_store();
}
#endif

#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
/* must hold runqueue lock for queue se is currently on */
static struct task_struct *detach_a_hint_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p, *best_task = NULL, *backup = NULL;
	int dst_capacity, src_capacity;
	unsigned int task_util;
	bool latency_sensitive = false, in_many_heavy_tasks;
	struct cpumask effective_softmask;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;

	lockdep_assert_rq_held(src_rq);

	rcu_read_lock();
	in_many_heavy_tasks = rd->android_vendor_data1[0];
	src_capacity = capacity_orig_of(src_rq->cpu);
	dst_capacity = cpu_cap_ceiling(dst_cpu);
	list_for_each_entry_reverse(p,
			&src_rq->cfs_tasks, se.group_node) {

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		if (task_on_cpu(src_rq, p))
			continue;

		task_util = uclamp_task_util(p);

		compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

		if (latency_sensitive && !cpumask_test_cpu(dst_cpu, &effective_softmask))
			continue;

		if (in_many_heavy_tasks &&
			!fits_capacity(task_util, src_capacity, get_adaptive_margin(src_rq->cpu))) {
			/* when too many big task, pull misfit runnable task */
			best_task = p;
			break;
		} else if (latency_sensitive &&
			task_util <= dst_capacity) {
			best_task = p;
			break;
		} else if (latency_sensitive && !backup) {
			backup = p;
		}
	}
	p = best_task ? best_task : backup;
	if (p) {
		/* detach_task */
		deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
		set_task_cpu(p, dst_cpu);
	}
	rcu_read_unlock();
	return p;
}
#endif

static int mtk_active_load_balance_cpu_stop(void *data)
{
	struct task_struct *target_task = data;
	int busiest_cpu = smp_processor_id();
	struct rq *busiest_rq = cpu_rq(busiest_cpu);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct rq_flags rf;
	int deactivated = 0;

	local_irq_disable();
	raw_spin_lock(&target_task->pi_lock);
	rq_lock(busiest_rq, &rf);

	if (task_cpu(target_task) != busiest_cpu ||
		(!cpumask_test_cpu(target_cpu, target_task->cpus_ptr)) ||
		task_on_cpu(busiest_rq, target_task) ||
		target_rq == busiest_rq)
		goto out_unlock;

	if (!task_on_rq_queued(target_task))
		goto out_unlock;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu))
		goto out_unlock;

	if (cpu_paused(busiest_cpu) || cpu_paused(target_cpu))
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(!busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	update_rq_clock(busiest_rq);
	deactivate_task(busiest_rq, target_task, DEQUEUE_NOCLOCK);
	set_task_cpu(target_task, target_cpu);
	deactivated = 1;
out_unlock:
	busiest_rq->active_balance = 0;
	rq_unlock(busiest_rq, &rf);

	if (deactivated)
		attach_one_task(target_rq, target_task);

	raw_spin_unlock(&target_task->pi_lock);
	put_task_struct(target_task);

	local_irq_enable();
	return 0;
}

int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target, int reason)
{
	int active_balance = false;
	unsigned long flags;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;

	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);
	raw_spin_rq_lock_irqsave(target, flags);
	if (!target->active_balance &&
		(task_rq(p) == target) && READ_ONCE((p)->__state) != TASK_DEAD &&
		 !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
		target->active_balance = 1;
		target->push_cpu = this_cpu;
		active_balance = true;
		get_task_struct(p);
	}
	raw_spin_rq_unlock_irqrestore(target, flags);
	if (active_balance) {
		trace_sched_force_migrate(p, this_cpu, reason);
		stop_one_cpu_nowait(cpu_of(target),
				mtk_active_load_balance_cpu_stop,
				p, &target->active_balance_work);
	}

	return active_balance;
}

void try_to_pull_VVIP(int this_cpu, bool *had_pull_vvip, struct rq_flags *src_rf)
{
	struct root_domain *rd;
	struct perf_domain *pd;
	struct rq *src_rq, *this_rq;
	struct task_struct *p;
	int cpu;

	if (!cpumask_test_cpu(this_cpu, &bcpus))
		return;

	if (cpu_paused(this_cpu))
		return;

	if (!cpu_active(this_cpu))
		return;

	this_rq = cpu_rq(this_cpu);
	rd = this_rq->rd;
	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	for (; pd; pd = pd->next) {
		for_each_cpu(cpu, perf_domain_span(pd)) {

			if (cpu_paused(cpu))
				continue;

			if (!cpu_active(cpu))
				continue;

			if (cpu == this_cpu)
				continue;

			src_rq = cpu_rq(cpu);

			if (num_vvip_in_cpu(cpu) < 1)
				continue;

			else if (num_vvip_in_cpu(cpu) == 1) {
				/* the only one VVIP in cpu is running */
				if (src_rq->curr && task_is_vip(src_rq->curr, VVIP))
					continue;
			}

			/* There are runnables in cpu */
			rq_lock_irqsave(src_rq, src_rf);
			if (src_rq->curr)
				update_rq_clock(src_rq);
			p = next_vip_runnable_in_cpu(src_rq, VVIP);
			if (p && cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
				deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
				set_task_cpu(p, this_cpu);
				rq_unlock_irqrestore(src_rq, src_rf);

				trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_PULL_VIP_RUNNABLE);
				attach_one_task(this_rq, p);
				*had_pull_vvip = true;
				goto unlock;
			}
			rq_unlock_irqrestore(src_rq, src_rf);
		}
	}
unlock:
	rcu_read_unlock();
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static DEFINE_PER_CPU(u64, next_update_new_balance_time_ns);
void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf,
		int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq, *misfit_task_rq = NULL;
	struct task_struct *p = NULL, *best_running_task = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;
	unsigned long misfit_load = 0;
	u64 now_ns;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;
	bool had_pull_vvip = false;

	if (cpu_paused(this_cpu)) {
		*done = 1;
		return;
	}

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (this_rq->ttwu_pending)
		return;

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(this_cpu))
		return;

	now_ns = ktime_get_real_ns();

	if (now_ns < per_cpu(next_update_new_balance_time_ns, this_cpu))
		return;

	per_cpu(next_update_new_balance_time_ns, this_cpu) =
		now_ns + new_idle_balance_interval_ns;

	trace_sched_next_new_balance(now_ns, per_cpu(next_update_new_balance_time_ns, this_cpu));

	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_rq_unlock(this_rq);

	this_cpu = this_rq->cpu;

	/* try to pull runnable VVIP if this_cpu is in big gear */
	try_to_pull_VVIP(this_cpu, &had_pull_vvip, &src_rf);
	if (had_pull_vvip)
		goto out;

	for_each_cpu(cpu, cpu_active_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		rq_lock_irqsave(src_rq, &src_rf);
		update_rq_clock(src_rq);
		if (src_rq->active_balance) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		if ((src_rq->misfit_task_load > misfit_load) &&
			(cpu_cap_ceiling(this_cpu) > cpu_cap_ceiling(cpu))) {
			p = src_rq->curr;
			if (p) {
				compute_effective_softmask(p, &latency_sensitive,
							&effective_softmask);
				if (p->policy == SCHED_NORMAL &&
					cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
					!(latency_sensitive &&
					!cpumask_test_cpu(this_cpu, &effective_softmask))) {

					misfit_task_rq = src_rq;
					misfit_load = src_rq->misfit_task_load;
					if (best_running_task)
						put_task_struct(best_running_task);
					best_running_task = p;
					get_task_struct(best_running_task);
				}
			}
			p = NULL;
		}

		if (src_rq->nr_running <= 1) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}

		p = detach_a_hint_task(src_rq, this_cpu);

		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

	/*
	 * If p is null meaning that we have not pull a runnable task, we try to
	 * pull a latency sensitive running task.
	 */
	if (!p && misfit_task_rq)
		*done = migrate_running_task(this_cpu, best_running_task,
					misfit_task_rq, MIGR_IDLE_PULL_MISFIT_RUNNING);
	if (best_running_task)
		put_task_struct(best_running_task);
out:
	raw_spin_rq_lock(this_rq);
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if (*pulled_task != 0)
		*done = 1;

	rq_repin_lock(this_rq, rf);

}
#endif
