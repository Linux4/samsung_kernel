/*
 * Exynos Mobile Scheduler CPU selection
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * TEX (Task EXpress)                                                         *
 ******************************************************************************/
struct {
	struct cpumask pinning_cpus;
	struct cpumask busy_cpus;
	int qjump;
	int enabled[CGROUP_COUNT];
	int prio;
	int suppress;
} tex;

int tex_task(struct task_struct *p)
{
	int group_idx, sched_class_idx;

	group_idx = cpuctl_task_group_idx(p);
	if (!tex.enabled[group_idx])
		return 0;

	sched_class_idx = get_sched_class_idx(p->sched_class);
	if (sched_class_idx != EMS_SCHED_FAIR)
		return 0;

	return p->prio <= tex.prio;
}

void tex_enqueue_task(struct task_struct *p, int cpu)
{
	if (!tex_task(p))
		return;

	/*
	 * Although the task is a TEX task, it could be happen that qjump is
	 * enabled but pinning is not. In that case, DO NOT update busy CPUs
	 * because it affects on fit_cpus.
	 */
	if (cpumask_test_cpu(cpu, &tex.pinning_cpus))
		cpumask_set_cpu(cpu, &tex.busy_cpus);

	if (tex.qjump) {
		list_add_tail(ems_qjump_node(p), ems_qjump_list(cpu_rq(cpu)));
		ems_qjump_queued(p) = 1;
	}
}

void tex_dequeue_task(struct task_struct *p, int cpu)
{
	/*
	 * TEX qjump could be disabled while TEX task is being enqueued.
	 * Make sure to delete list node of TEX task from qjump list
	 * every time the task is dequeued.
	 */
	if (ems_qjump_queued(p)) {
		list_del(ems_qjump_node(p));
		ems_qjump_queued(p) = 0;
	}

	if (!tex_task(p))
		return;

	cpumask_clear_cpu(cpu, &tex.busy_cpus);
}

void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				struct sched_entity **se_ptr, bool *repick,
				bool simple, struct task_struct *prev)
{
	struct task_struct *p = NULL;

	if (!list_empty(ems_qjump_list(rq))) {
		p = ems_qjump_first_entry(ems_qjump_list(rq));
		*p_ptr = p;
		*se_ptr = &p->se;

		list_move_tail(ems_qjump_list(rq), ems_qjump_node(p));
		*repick = true;

		trace_tex_qjump_pick_next_task(p);
	}
}

static void
tex_pinning_schedule(struct tp_env *env, struct cpumask *candidates)
{
	int cpu, max_spare_cpu, max_spare_cpu_idle = -1, max_spare_cpu_active = -1;
	unsigned long spare, max_spare_idle = 0, max_spare_active = 0;

	for_each_cpu(cpu, candidates) {
		spare = capacity_cpu_orig(cpu) - env->cpu_stat[cpu].util_wo;
		if (env->cpu_stat[cpu].idle) {
			if (spare >= max_spare_idle) {
				max_spare_cpu_idle = cpu;
				max_spare_idle = spare;
			}
		} else {
			if (spare >= max_spare_active) {
				max_spare_cpu_active = cpu;
				max_spare_active = spare;
			}
		}
	}

	max_spare_cpu = (max_spare_cpu_idle >= 0) ? max_spare_cpu_idle : max_spare_cpu_active;
	trace_tex_pinning_schedule(max_spare_cpu, candidates);

	cpumask_clear(candidates);
	cpumask_set_cpu(max_spare_cpu, candidates);
}

int tex_suppress_task(struct task_struct *p)
{
	int sched_class_idx;

	if (!tex.suppress)
		return 0;

	sched_class_idx = get_sched_class_idx(p->sched_class);
	if (sched_class_idx == EMS_SCHED_FAIR && p->prio <= DEFAULT_PRIO)
		return 0;

	return 1;
}

static void
tex_pinning_fit_cpus(struct tp_env *env, struct cpumask *fit_cpus)
{
	int target = tex_task(env->p);
	int suppress = tex_suppress_task(env->p);

	if (target) {
		cpumask_andnot(fit_cpus, &tex.pinning_cpus, &tex.busy_cpus);
		cpumask_and(fit_cpus, fit_cpus, &env->cpus_allowed);

		if (cpumask_empty(fit_cpus)) {
			/*
			 * All priority pinning cpus are occupied by TEX tasks.
			 * Return cpu_possible_mask so that priority pinning
			 * has no effect.
			 */
			cpumask_copy(fit_cpus, cpu_possible_mask);
		} else {
			/* Pick best cpu among fit_cpus */
			tex_pinning_schedule(env, fit_cpus);
		}
	} else if (suppress) {
		/* Clear fastest & tex-busy cpus for the supressed task */
		cpumask_andnot(fit_cpus, &env->cpus_allowed, cpu_fastest_mask());
		cpumask_andnot(fit_cpus, fit_cpus, &tex.busy_cpus);

		if (cpumask_empty(fit_cpus))
			cpumask_copy(fit_cpus, &env->cpus_allowed);
	} else {
		/* Exclude cpus where priority pinning tasks run */
		cpumask_andnot(fit_cpus, cpu_possible_mask,
					&tex.busy_cpus);
	}

	trace_tex_pinning_fit_cpus(env->p, target, suppress, fit_cpus,
				   &tex.pinning_cpus, &tex.busy_cpus);
}

static int tex_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	cpumask_copy(&tex.pinning_cpus, &cur_set->tex.mask);
	cpumask_clear(&tex.busy_cpus);

	tex.qjump = cur_set->tex.qjump;

	for (i = 0; i < CGROUP_COUNT; i++)
		tex.enabled[i] = cur_set->tex.enabled[i];

	tex.prio = cur_set->tex.prio;

	tex.suppress = cur_set->tex.suppress;

	return NOTIFY_OK;
}

static struct notifier_block tex_emstune_notifier = {
	.notifier_call = tex_emstune_notifier_call,
};

static void tex_init(void)
{
	int i;

	cpumask_clear(&tex.pinning_cpus);
	cpumask_clear(&tex.busy_cpus);
	tex.qjump = 0;
	for (i = 0; i < CGROUP_COUNT; i++)
		tex.enabled[i] = 0;
	tex.prio = 0;

	tex.suppress = 0;

	emstune_register_notifier(&tex_emstune_notifier);
}

/******************************************************************************
 * cpus_binding                                                               *
 ******************************************************************************/
struct {
	unsigned long target_sched_class;
	struct cpumask mask[CGROUP_COUNT];
} cpus_binding;

const struct cpumask *cpus_binding_mask(struct task_struct *p)
{
	int group_idx;
	int class_idx;

	class_idx = get_sched_class_idx(p->sched_class);
	if (!(cpus_binding.target_sched_class & class_idx))
		return cpu_active_mask;

	group_idx = cpuctl_task_group_idx(p);
	if (unlikely(cpumask_empty(&cpus_binding.mask[group_idx])))
		return cpu_active_mask;

	return &cpus_binding.mask[group_idx];
}

static int cpus_binding_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	cpus_binding.target_sched_class =
			cur_set->cpus_binding.target_sched_class;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_copy(&cpus_binding.mask[i],
			&cur_set->cpus_binding.mask[i]);

	return NOTIFY_OK;
}

static struct notifier_block cpus_binding_emstune_notifier = {
	.notifier_call = cpus_binding_emstune_notifier_call,
};

static void cpus_binding_init(void)
{
	int i;

	cpus_binding.target_sched_class = 0;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_setall(&cpus_binding.mask[i]);

	emstune_register_notifier(&cpus_binding_emstune_notifier);
}

/******************************************************************************
 * sched policy                                                               *
 ******************************************************************************/
static int sched_policy[CGROUP_COUNT];

static int sched_policy_get(struct task_struct *p)
{
	int policy = sched_policy[cpuctl_task_group_idx(p)];

	/*
	 * Change target tasks's policy to SCHED_POLICY_ENERGY
	 * for power optimization, if
	 * 1) target task's sched_policy is SCHED_POLICY_EFF_TINY and
	 *    its utilization is under 1.56% of SCHED_CAPACITY_SCALE.
	 * 2) tasks is worker thread.
	 */
	if (p->flags & PF_WQ_WORKER)
		return SCHED_POLICY_ENERGY;

	if (policy == SCHED_POLICY_EFF_TINY)
		if (ml_task_util_est(p) <= SCHED_CAPACITY_SCALE >> 6)
			return SCHED_POLICY_ENERGY;

	return policy;
}

static int sched_policy_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		sched_policy[i] = cur_set->sched_policy.policy[i];

	return NOTIFY_OK;
}

static struct notifier_block sched_policy_emstune_notifier = {
	.notifier_call = sched_policy_emstune_notifier_call,
};

static void sched_policy_init(void)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		sched_policy[i] = SCHED_POLICY_EFF;

	emstune_register_notifier(&sched_policy_emstune_notifier);
}

/******************************************************************************
 * cpu weight / idle weight                                                   *
 ******************************************************************************/
static int active_weight[CGROUP_COUNT][VENDOR_NR_CPUS];
static int idle_weight[CGROUP_COUNT][VENDOR_NR_CPUS];

static int cpu_weight_get(struct task_struct *p, int cpu, int idle)
{
	int group_idx, weight;

	group_idx = cpuctl_task_group_idx(p);

	if (idle)
		weight = idle_weight[group_idx][cpu];
	else
		weight = active_weight[group_idx][cpu];

	return weight;
}

static int cpu_weight_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i, cpu;

	for (i = 0; i < CGROUP_COUNT; i++) {
		for_each_possible_cpu(cpu) {
			active_weight[i][cpu] =
				cur_set->active_weight.ratio[i][cpu];
			idle_weight[i][cpu] =
				cur_set->idle_weight.ratio[i][cpu];
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block cpu_weight_emstune_notifier = {
	.notifier_call = cpu_weight_emstune_notifier_call,
};

#define DEFAULT_WEIGHT	(100)
static void cpu_weight_init(void)
{
	int i, cpu;

	for (i = 0; i < CGROUP_COUNT; i++) {
		for_each_possible_cpu(cpu) {
			active_weight[i][cpu] = DEFAULT_WEIGHT;
			idle_weight[i][cpu] = DEFAULT_WEIGHT;
		}
	}

	emstune_register_notifier(&cpu_weight_emstune_notifier);
}

/******************************************************************************
 * prefer cpu                                                                 *
 ******************************************************************************/
static struct {
	struct cpumask mask[CGROUP_COUNT];
	struct {
		unsigned long threshold;
		struct cpumask mask;
	} small_task;
} prefer_cpu;

static void prefer_cpu_get(struct tp_env *env, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_active_mask);

	if (env->task_util < prefer_cpu.small_task.threshold) {
		cpumask_and(mask, mask, &prefer_cpu.small_task.mask);
		return;
	}

	if (uclamp_boosted(env->p) || emstune_sched_boost())
		cpumask_and(mask, mask,
			&prefer_cpu.mask[cpuctl_task_group_idx(env->p)]);
}

static int prefer_cpu_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_copy(&prefer_cpu.mask[i], &cur_set->prefer_cpu.mask[i]);

	prefer_cpu.small_task.threshold = cur_set->prefer_cpu.small_task_threshold;
	cpumask_copy(&prefer_cpu.small_task.mask,
			&cur_set->prefer_cpu.small_task_mask);

	return NOTIFY_OK;
}

static struct notifier_block prefer_cpu_emstune_notifier = {
	.notifier_call = prefer_cpu_emstune_notifier_call,
};

static void prefer_cpu_init(void)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_copy(&prefer_cpu.mask[i], cpu_possible_mask);

	prefer_cpu.small_task.threshold = 0;
	cpumask_copy(&prefer_cpu.small_task.mask, cpu_possible_mask);

	emstune_register_notifier(&prefer_cpu_emstune_notifier);
}

/******************************************************************************
 * best energy cpu selection                                                  *
 ******************************************************************************/
static LIST_HEAD(csd_head);

struct cs_domain __percpu **__pcpu_csd;
#define pcpu_csd(cpu)	(*per_cpu_ptr(__pcpu_csd, cpu))

#define INVALID_CPU	-1

static inline void
prev_cpu_advantage(unsigned long *cpu_util, unsigned long task_util)
{
	long util = *cpu_util;

	/*
	 * subtract cpu util by 12.5% of task util to give advantage to
	 * prev cpu when computing energy.
	 */
	util -= (task_util >> 3);
	*cpu_util = max(util, (long)0);
}

static unsigned int
compute_system_energy(struct tp_env *env,
	const struct cpumask *candidates, int dst_cpu)
{
	struct cs_domain *csd;
	struct list_head __csd_head;
	struct cs_domain csd_pool[VENDOR_NR_CPUS];
	int csd_count = 0;
	unsigned long util[VENDOR_NR_CPUS] = {0, };

	INIT_LIST_HEAD(&__csd_head);

	list_for_each_entry(csd, &csd_head, list) {
		struct cs_domain *__csd = &csd_pool[csd_count];
		unsigned long next_cap, grp_util_wo = 0;
		int cpu, num_cpu, grp_idle_cnt = 0, apply_sp = 0;
		int grp_cpu = cpumask_any(&csd->cpus);

		/* get next_cap and cap_idx */
		next_cap = cpufreq_get_next_cap(env, &csd->cpus, dst_cpu, false);

		/* calculate group util */
		for_each_cpu(cpu, &csd->cpus) {
			grp_util_wo += env->cpu_stat[cpu].util_wo;

			if (cpu == dst_cpu)
				util[cpu] = env->cpu_stat[cpu].util_with;
			else
				util[cpu] = env->cpu_stat[cpu].util_wo;

			if (env->cpu_stat[cpu].idle)
				grp_idle_cnt++;
		}

		if (task_cpu(env->p) == dst_cpu &&
				cpumask_test_cpu(dst_cpu, &csd->cpus))
			prev_cpu_advantage(&util[dst_cpu], env->task_util);

		/*
		 * HACK: static power is applied when
		 *  1. target cpu does not belongs to slowest cpumask(LIT)
		 *  2. average util of target coregroup is greater than 3% of capacity
		 *  3. all cpus in target coregroup are idle
		 */
		num_cpu = cpumask_weight(&csd->cpus);
		if (cpumask_test_cpu(dst_cpu, &csd->cpus) &&
		    !cpumask_subset(&csd->cpus, cpu_slowest_mask()) &&
		    (grp_util_wo / num_cpu) < (capacity_cpu(grp_cpu) >> 5) &&
		    num_cpu == grp_idle_cnt)
			apply_sp = 1;

		/* HACK: Selective application of static power is being evaluated */
		apply_sp = 1;

		/* fill csd and add to list */
		cpumask_copy(&__csd->cpus, &csd->cpus);
		__csd->next_freq = et_cap_to_freq(grp_cpu, next_cap);
		__csd->apply_sp = apply_sp;
		list_add_tail(&__csd->list, &__csd_head);

		csd_count++;
	}

	return et_compute_system_energy(&__csd_head, util, env->weight, dst_cpu);
}

static int
find_min_util_cpu(struct tp_env *env, const struct cpumask *mask, bool among_idle)
{
	int cpu, min_cpu = INVALID_CPU;
	unsigned long min_util = ULONG_MAX;

	for_each_cpu_and(cpu, &env->fit_cpus, mask) {
		unsigned long cpu_util;

		/*
		 * If among_idle is true, find min util cpu among idle cpu.
		 * Skip non-idle cpu.
		 */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		cpu_util = env->cpu_stat[cpu].util_with;
		if (cpu == task_cpu(env->p))
			prev_cpu_advantage(&cpu_util, env->task_util);

		if (cpu_util < min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	return min_cpu;
}

static int
__find_energy_cpu(struct tp_env *env, const struct cpumask *candidates)
{
	int cpu, energy_cpu = -1, min_util = INT_MAX;
	unsigned int min_energy = UINT_MAX;

	/* find energy cpu */
	for_each_cpu(cpu, candidates) {
		int cpu_util = env->cpu_stat[cpu].util_with;
		unsigned int energy;

		/* calculate system energy */
		energy = compute_system_energy(env, candidates, cpu);

		trace_ems_compute_system_energy(env->p, candidates, cpu, energy);

		/* find min_energy cpu */
		if (energy < min_energy)
			goto energy_cpu_found;
		if (energy > min_energy)
			continue;

		/* find min_util cpu if energy is same */
		if (cpu_util >= min_util)
			continue;

energy_cpu_found:
		min_energy = energy;
		energy_cpu = cpu;
		min_util = cpu_util;
	}

	return energy_cpu;
}

static int find_energy_cpu(struct tp_env *env)
{
	struct cs_domain *csd;
	struct cpumask candidates;
	int min_cpu, energy_cpu, adv_energy_cpu = INVALID_CPU;

	/* set candidates cpu to find energy cpu */
	cpumask_clear(&candidates);

	/* Pick minimum utilization cpu from each domain */
	list_for_each_entry(csd, &csd_head, list) {
		min_cpu = find_min_util_cpu(env, &csd->cpus, false);
		if (min_cpu >= 0)
			cpumask_set_cpu(min_cpu, &candidates);
	}

	if (cpumask_weight(&candidates) == 1) {
		energy_cpu = cpumask_any(&candidates);
		goto out;
	}

	/* find min energy cpu */
	energy_cpu = __find_energy_cpu(env, &candidates);

	/*
	 * Slowest cpumask is usually the coregroup that includes the boot
	 * processor(cpu0), has low power consumption but also low performance
	 * efficiency. If selected cpu belongs to slowest cpumask and task is
	 * tiny enough not to increase system energy, reselect min energy cpu
	 * among idle cpu within slowest cpumask for faster task processing.
	 * (tiny task criteria = task util < 12.5% of slowest cpu capacity)
	 */
	if (cpumask_test_cpu(energy_cpu, cpu_slowest_mask()) &&
	    env->task_util < (capacity_cpu(0) >> 3))
		adv_energy_cpu = find_min_util_cpu(env,
					cpu_slowest_mask(), true);

out:
	trace_ems_find_energy_cpu(env->p, &candidates,
				energy_cpu, adv_energy_cpu);

	if (adv_energy_cpu >= 0)
		return adv_energy_cpu;

	return energy_cpu;
}

/******************************************************************************
 * best efficiency cpu selection                                              *
 ******************************************************************************/
static unsigned int
compute_efficiency(struct tp_env *env, int cpu)
{
	unsigned long next_cap = 0;
	unsigned long energy;
	unsigned long long eff;

	/* Get next capacity of cpu in coregroup with task */
	next_cap = cpufreq_get_next_cap(env, &pcpu_csd(cpu)->cpus, cpu, true);

	energy = et_compute_cpu_energy(cpu, et_cap_to_freq(cpu, next_cap),
			env->cpu_stat[cpu].util_with, env->weight[cpu]);

	/* to prevent divide by zero */
	energy = energy ? energy : 1;

	/*
	 * Compute performance efficiency
	 *  efficiency = capacity / energy
	 *
	 * Weight is not applied when computing performance because weight
	 * is already applied when computing energy.
	 *             = capacity / (energy * 100 / weight)
	 *             = capacity / energy * weight / 100
	 *             = efficiency * weight / 100
	 */
	eff = (next_cap << SCHED_CAPACITY_SHIFT * 2) / energy;

	trace_ems_compute_eff(env, cpu, env->weight[cpu],
				next_cap, energy, (unsigned int)eff);

	return (unsigned int)eff;
}

static int find_best_eff_cpu(struct tp_env *env)
{
	int cpu, best_cpu = INVALID_CPU;
	unsigned int eff, best_eff = 0;

	/* find best efficiency cpu */
	for_each_cpu(cpu, &env->fit_cpus) {
		eff = compute_efficiency(env, cpu);

		/*
		 * Give 6.25% margin to prev cpu efficiency.
		 * This margin means migration cost.
		 */
		if (cpu == task_cpu(env->p))
			eff = eff + (eff >> 4);

		if (eff > best_eff) {
			best_eff = eff;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

/******************************************************************************
 * best performance cpu selection                                             *
 ******************************************************************************/
static int find_max_spare_cpu(struct tp_env *env, bool among_idle,
					unsigned long *raw_spare_cap)
{
	int cpu, best_cpu = -1;
	unsigned long max_spare_cap = 0;

	/* find maximum spare cpu */
	for_each_cpu(cpu, &env->fit_cpus) {
		unsigned long curr_cap, spare_cap;

		/* among_idle is true, find max spare cpu among idle cpu */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		curr_cap = (capacity_cpu(cpu) * arch_scale_freq_capacity(cpu))
						>> SCHED_CAPACITY_SHIFT;
		spare_cap = curr_cap - env->cpu_stat[cpu].util_wo;

		if (max_spare_cap < spare_cap) {
			max_spare_cap = spare_cap;
			best_cpu = cpu;
		}

		if (raw_spare_cap)
			raw_spare_cap[cpu] = spare_cap;
	}

	return best_cpu;
}

static int find_best_perf_cpu(struct tp_env *env)
{
	int best_cpu;

	if (env->idle_cpu_count)
		best_cpu = find_max_spare_cpu(env, true, NULL);
	else
		best_cpu = find_max_spare_cpu(env, false, NULL);

	trace_ems_find_best_perf_cpu(env, best_cpu);

	return best_cpu;
}

static int find_semi_perf_cpu(struct tp_env *env)
{
	int best_idle_cpu = -1;
	int min_exit_lat = UINT_MAX;
	int cpu;
	int max_spare_cap_cpu_ls = task_cpu(env->p);
	unsigned long max_spare_cap_ls = 0;
	unsigned long spare_cap, util, cpu_cap, target_cap;
	struct cpuidle_state *idle;

	for_each_cpu(cpu, &env->fit_cpus) {
		util = env->cpu_stat[cpu].util_wo;
		cpu_cap = capacity_cpu(cpu);
		spare_cap = cpu_cap - util;
		if (available_idle_cpu(cpu)) {
			cpu_cap = capacity_cpu_orig(cpu);
			if (cpu_cap < target_cap)
				continue;
			idle = idle_get_state(cpu_rq(cpu));
			if (idle && idle->exit_latency > min_exit_lat &&
				cpu_cap == target_cap)
				continue;

			if (idle)
				min_exit_lat = idle->exit_latency;
			target_cap = cpu_cap;
			best_idle_cpu = cpu;
		} else if (spare_cap > max_spare_cap_ls) {
			max_spare_cap_ls = spare_cap;
			max_spare_cap_cpu_ls = cpu;
		}
	}


	trace_ems_find_semi_perf_cpu(env, best_idle_cpu,
				max_spare_cap_cpu_ls, &env->fit_cpus);

	return best_idle_cpu >= 0 ? best_idle_cpu : max_spare_cap_cpu_ls;
}

/******************************************************************************
 * cpu with low energy in best efficienct domain selection                    *
 ******************************************************************************/
static int find_best_eff_energy_cpu(struct tp_env *env)
{
	int eff_cpu, energy_cpu;
	struct cpumask candidates;

	eff_cpu = find_best_eff_cpu(env);
	if (eff_cpu < 0)
		return eff_cpu;

	cpumask_copy(&candidates, &pcpu_csd(eff_cpu)->cpus);
	energy_cpu = __find_energy_cpu(env, &candidates);

	return energy_cpu;
}

/******************************************************************************
 * best cpu selection                                                         *
 ******************************************************************************/
int find_best_cpu(struct tp_env *env)
{
	int best_cpu;

	/* When sysbusy is detected, do scheduling under other policies */
	if (sysbusy_activated()) {
		best_cpu = sysbusy_schedule(env);
		if (best_cpu >= 0)
			return best_cpu;
	}

	switch (env->sched_policy) {
	case SCHED_POLICY_EFF:
	case SCHED_POLICY_EFF_TINY:
		/* Find best efficiency cpu */
		best_cpu = find_best_eff_cpu(env);
		break;
	case SCHED_POLICY_ENERGY:
		/* Find lowest energy cpu */
		best_cpu = find_energy_cpu(env);
		break;
	case SCHED_POLICY_PERF:
		/* Find best performance cpu */
		best_cpu = find_best_perf_cpu(env);
		break;
	case SCHED_POLICY_SEMI_PERF:
		/* Find semi performance cpu */
		best_cpu = find_semi_perf_cpu(env);
		break;
	case SCHED_POLICY_EFF_ENERGY:
		/* Find lowest energy cpu in best efficient domain */
		best_cpu = find_best_eff_energy_cpu(env);
		break;
	default:
		best_cpu = INVALID_CPU;
	}

	return best_cpu;
}

static int find_min_load_avg_cpu(struct tp_env *env)
{
	int cpu, min_load_avg_cpu = -1;
	unsigned long load_avg, min_load_avg = ULONG_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		load_avg = env->cpu_stat[cpu].load_avg;
		if (load_avg < min_load_avg) {
			min_load_avg = load_avg;
			min_load_avg_cpu = cpu;
		}
	}

	return min_load_avg_cpu;
}

static void find_overcap_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;
	unsigned long util;

	if (is_misfit_task_util(env->task_util_clamped))
		goto misfit;

	/*
	 * Find cpus that becomes over capacity with a given task.
	 *
	 * overcap_cpus = cpu capacity < cpu util + task util
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		util = env->cpu_stat[cpu].util_wo + env->task_util_clamped;
		if (util > capacity_cpu_orig(cpu))
			cpumask_set_cpu(cpu, mask);
	}

	return;

misfit:
	/*
	 * A misfit task is likely to cause all cpus to become over capacity,
	 * so it is likely to fail proper scheduling. In case of misfit task,
	 * only cpu that meet the following conditions:
	 *
	 *  - the cpu with big enough capacity that can handle misfit task
	 *  - the cpu not busy (util is under 80% of capacity)
	 *  - the cpu with empty runqueue.
	 *
	 * Other cpus are considered overcapacity.
	 */
	cpumask_copy(mask, &env->cpus_allowed);
	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long capacity = capacity_cpu_orig(cpu);

		if (env->task_util_clamped <= capacity &&
		    !check_busy(env->cpu_stat[cpu].util_wo, capacity) &&
		    !cpu_rq(cpu)->nr_running)
			cpumask_clear_cpu(cpu, mask);
	}
}

static void find_busy_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	cpumask_clear(mask);

	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long util, runnable, nr_running;

		util = env->cpu_stat[cpu].util;
		runnable = env->cpu_stat[cpu].runnable;
		nr_running = env->cpu_stat[cpu].nr_running;

		if (!__is_busy_cpu(util, runnable, capacity_orig_of(cpu), nr_running))
			continue;

		cpumask_set_cpu(cpu, mask);
	}

	trace_ems_find_busy_cpus(mask, busy_cpu_ratio);
}

static int find_fit_cpus(struct tp_env *env)
{
	struct cpumask mask[6];
	int cpu = smp_processor_id();

	if (EMS_PF_GET(env->p) & EMS_PF_RUNNABLE_BUSY) {
		cpu = find_min_load_avg_cpu(env);
		if (cpu >= 0) {
			cpumask_set_cpu(cpu, &env->fit_cpus);
			goto out;
		}
	}

	/*
	 * take a snapshot of cpumask to get fit cpus
	 * - mask0 : overcap cpus
	 * - mask1 : prio pinning cpus
	 * - mask2 : ontime fit cpus
	 * - mask3 : prefer cpu
	 * - mask4 : busy cpu
	 * - mask5 : gsc cpus
	 */
	find_overcap_cpus(env, &mask[0]);
	tex_pinning_fit_cpus(env, &mask[1]);
	ontime_select_fit_cpus(env->p, &mask[2]);
	prefer_cpu_get(env, &mask[3]);
	find_busy_cpus(env, &mask[4]);
	gsc_fit_cpus(env->p, &mask[5]);

	/*
	 * Exclude overcap cpu from cpus_allowed. If there is only one or no
	 * fit cpus, it does not need to find fit cpus anymore.
	 */
	cpumask_andnot(&env->fit_cpus, &env->cpus_allowed, &mask[0]);
	if (cpumask_weight(&env->fit_cpus) <= 1)
		goto out;

	/* Exclude busy cpus from fit_cpus */
	cpumask_andnot(&env->fit_cpus, &env->fit_cpus, &mask[4]);
	if (cpumask_weight(&env->fit_cpus) <= 1)
		goto out;

	if (env->p->pid && emstune_task_boost() == env->p->pid) {
		if (cpumask_intersects(&env->fit_cpus, cpu_fastest_mask())) {
			cpumask_and(&env->fit_cpus, &env->fit_cpus, cpu_fastest_mask());
			goto out;
		}
	}

	/*
	 * If cl_sync is true, pick fit cpus among coregroup mask of this cpu
	 * if it is applicable.
	 */
	if (env->cl_sync) {
		struct cpumask temp;

		cpumask_copy(&temp, &pcpu_csd(cpu)->cpus);
		if (cpumask_intersects(&env->fit_cpus, &temp)) {
			cpumask_and(&env->fit_cpus, &env->fit_cpus, &temp);
			if (cpumask_weight(&env->fit_cpus) == 1)
				goto out;
		}
	}

	/* Handle sync flag */
	if (env->sync) {
		if (cpu_rq(cpu)->nr_running == 1 &&
			cpumask_test_cpu(cpu, &env->fit_cpus)) {
			struct cpumask mask;

			cpumask_andnot(&mask, cpu_active_mask,
						cpu_slowest_mask());

			/*
			 * Select this cpu if boost is activated and this
			 * cpu does not belong to slowest cpumask.
			 */
			if (cpumask_test_cpu(cpu, &mask)) {
				cpumask_clear(&env->fit_cpus);
				cpumask_set_cpu(cpu, &env->fit_cpus);
				goto out;
			}
		}
	}

	/*
	 * Apply priority pinning to fit_cpus. Since priority pinning has a
	 * higher priority than ontime, it does not need to execute sequence
	 * afterwards if there is only one fit cpus.
	 */
	if (cpumask_intersects(&env->fit_cpus, &mask[1])) {
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[1]);
		if (cpumask_weight(&env->fit_cpus) == 1)
			goto out;
	}

	/* Pick ontime fit cpus if ontime is applicable */
	if (cpumask_intersects(&env->fit_cpus, &mask[2]))
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[2]);

	/* Apply prefer cpu if it is applicable */
	if (cpumask_intersects(&env->fit_cpus, &mask[3]))
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[3]);

	/* Apply gsc fit cpus if it is applicable */
	if (cpumask_intersects(&env->fit_cpus, &mask[5]))
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[5]);

out:
	trace_ems_find_fit_cpus(env, mask);

	return cpumask_weight(&env->fit_cpus);
}

static void take_util_snapshot(struct tp_env *env)
{
	int cpu;

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = max(ml_task_util_est(env->p), (unsigned long)1);
	env->task_util_clamped = max(ml_uclamp_task_util(env->p), (unsigned long)1);

	env->task_load_avg = ml_task_load_avg(env->p);

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long extra_util;
		struct cfs_rq *cfs_rq;

		env->cpu_stat[cpu].rt_util = cpu_util_rt(cpu_rq(cpu));
		env->cpu_stat[cpu].dl_util = cpu_util_dl(cpu_rq(cpu));
		extra_util = env->cpu_stat[cpu].rt_util +
				env->cpu_stat[cpu].dl_util;

		env->cpu_stat[cpu].util_wo =
			min(ml_cpu_util_without(cpu, env->p) + extra_util,
			    capacity_cpu_orig(cpu));
		env->cpu_stat[cpu].util_with =
			min(ml_cpu_util_with(env->p, cpu) + extra_util,
			    capacity_cpu_orig(cpu));

		cfs_rq = &cpu_rq(cpu)->cfs;
		env->cpu_stat[cpu].runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
		env->cpu_stat[cpu].load_avg = ml_cpu_load_avg(cpu);

		if (cpu == task_cpu(env->p))
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_with;
		else
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_wo;

		env->cpu_stat[cpu].nr_running = cpu_rq(cpu)->nr_running;
		env->cpu_stat[cpu].idle = available_idle_cpu(cpu);

		trace_ems_take_util_snapshot(cpu, env);
	}
}

/*
 * Return number of CPUs allowed.
 */
int find_cpus_allowed(struct tp_env *env)
{
	struct cpumask mask[4];

	if (is_somac_ready(env))
		goto out;

	/*
	 * take a snapshot of cpumask to get CPUs allowed
	 * - mask0 : cpu_active_mask
	 * - mask1 : ecs_cpus_allowed
	 * - mask2 : p->cpus_ptr
	 * - mask3 : cpus_binding_mask
	 */
	cpumask_copy(&mask[0], cpu_active_mask);
	cpumask_copy(&mask[1], ecs_cpus_allowed(env->p));
	cpumask_copy(&mask[2], env->p->cpus_ptr);
	cpumask_copy(&mask[3], cpus_binding_mask(env->p));

	/*
	 * Putting per-cpu kthread on other cpu is not allowed.
	 * It does not have to find cpus allowed in this case.
	 */
	if (env->per_cpu_kthread) {
		cpumask_copy(&env->cpus_allowed, &mask[2]);
		goto out;
	}

	/*
	 * Given task must run on the CPU combined as follows:
	 *	cpu_active_mask & ecs_cpus_allowed
	 */
	cpumask_and(&env->cpus_allowed, &mask[0], &mask[1]);
	if (cpumask_empty(&env->cpus_allowed))
		goto out;

	/*
	 * Unless task is per-cpu kthread, p->cpus_ptr does not cause a problem
	 * even if it is ignored. Consider p->cpus_ptr as possible, but if it
	 * does not overlap with CPUs allowed made above, ignore it.
	 */
	if (cpumask_intersects(&env->cpus_allowed, &mask[2]))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask[2]);

	/*
	 * cpus_binding is for performance/power optimization and can be
	 * ignored. As above, ignore it if it does not overlap with CPUs
	 * allowed made above.
	 */
	if (cpumask_intersects(&env->cpus_allowed, &mask[3]))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask[3]);

	if (is_somac_ready(env))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, somac_ready_cpus());

out:
	if (test_ti_thread_flag(&env->p->thread_info, TIF_32BIT)) {
		/*
		 * cpus_ptr of 32bits task contains only the cpu that supports
		 * 32bits. If cpus_allowed does not intersect p->cpus_ptr, the
		 * cpu allowed by cpu sparing among p->cpus_ptr is selected. If
		 * all cpus of p->cpus_ptr are not allowed by cpu sparing,
		 * select first cpu of 32bit el0 supported cpus.
		 */
		if (!cpumask_intersects(&env->cpus_allowed, &mask[2])) {
			struct cpumask temp;

			cpumask_and(&temp, &mask[1], &mask[2]);
			if (cpumask_empty(&temp)) {
				/* HACK: Must be fixed */
				cpumask_set_cpu(4, &temp);
			}

			cpumask_copy(&env->cpus_allowed, &temp);
		}
	}

	trace_ems_find_cpus_allowed(env, mask);

	return cpumask_weight(&env->cpus_allowed);
}

static void get_ready_env(struct tp_env *env)
{
	int cpu, nr_cpus;

	/*
	 * If there is only one or no CPU allowed for a given task,
	 * it does not need to initialize env.
	 */
	if (find_cpus_allowed(env) <= 1)
		return;

	/* snapshot util to use same util during core selection */
	take_util_snapshot(env);

	/* Find fit cpus */
	nr_cpus = find_fit_cpus(env);
	if (nr_cpus == 0) {
		int prev_cpu = task_cpu(env->p);

		/* There is no fit cpus, keep the task on prev cpu */
		if (cpumask_test_cpu(prev_cpu, &env->cpus_allowed)) {
			cpumask_clear(&env->cpus_allowed);
			cpumask_set_cpu(prev_cpu, &env->cpus_allowed);
			return;
		}

		/*
		 * But prev cpu is not allowed, copy cpus_allowed to
		 * fit_cpus to find appropriate cpu among cpus_allowed.
		 */
		cpumask_copy(&env->fit_cpus, &env->cpus_allowed);
	} else if (nr_cpus == 1) {
		/* Only one cpu is fit. Select this cpu. */
		cpumask_copy(&env->cpus_allowed, &env->fit_cpus);
		return;
	}

	/*
	 * Get cpu weight.
	 * Weights are applied to energy or efficiency calculation
	 */
	for_each_cpu(cpu, cpu_active_mask)
		env->weight[cpu] = cpu_weight_get(env->p, cpu,
					available_idle_cpu(cpu));

	/* Get available idle cpu count */
	env->idle_cpu_count = 0;
	for_each_cpu(cpu, &env->fit_cpus)
		if (env->cpu_stat[cpu].idle)
			env->idle_cpu_count++;
}

int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag)
{
	int target_cpu = -1;
	struct tp_env env = {
		.p = p,
		.cgroup_idx = cpuctl_task_group_idx(p),
		.per_cpu_kthread = is_per_cpu_kthread(p),
		.sched_policy = sched_policy_get(p),
		.cl_sync = (wake_flag & WF_ANDROID_VENDOR) &&
			   !(current->flags & PF_EXITING),
		.sync = (wake_flag & WF_SYNC) &&
			!(current->flags & PF_EXITING),
	};

	/* Get ready environment variable */
	get_ready_env(&env);

	/* there is no CPU allowed, give up find new cpu */
	if (cpumask_empty(&env.cpus_allowed)) {
		trace_ems_select_task_rq(&env, target_cpu, "no CPU allowed");
		goto out;
	}

	/* there is only one CPU allowed, no need to find cpu */
	if (cpumask_weight(&env.cpus_allowed) == 1) {
		target_cpu = cpumask_any(&env.cpus_allowed);
		trace_ems_select_task_rq(&env, target_cpu, "only one CPU allowed");
		goto out;
	}

	target_cpu = find_best_cpu(&env);
	trace_ems_select_task_rq(&env, target_cpu, "best_cpu");

	if (target_cpu < 0 || !cpumask_test_cpu(target_cpu, &env.cpus_allowed)) {
		/*
		 * If target_cpu is positive but not allowed, it indicates that
		 * cpu selection logic has problem.
		 */
		if (target_cpu >= 0) {
			pr_err("Disallowed cpu%d is selected (cpus_allowed=%*pbl)\n",
				target_cpu, cpumask_pr_args(&env.cpus_allowed));
			WARN_ON(1);
		}

		if (cpumask_test_cpu(prev_cpu, &env.cpus_allowed))
			target_cpu = prev_cpu;
		else
			target_cpu = cpumask_any(&env.cpus_allowed);
	}

out:
	return target_cpu;
}

int core_init(struct device_node *ems_dn)
{
	struct device_node *dn, *child;

	dn = of_find_node_by_name(ems_dn, "cpu-selection-domain");
	if (unlikely(!dn))
		return -ENODATA;

	__pcpu_csd = alloc_percpu(struct cs_domain *);
	for_each_child_of_node(dn, child) {
		struct cs_domain *csd;
		const char *buf;
		int cpu;

		if (of_property_read_string(child, "cpus", &buf))
			return -ENODATA;

		csd = kzalloc(sizeof(struct cs_domain), GFP_KERNEL);
		if (unlikely(!csd))
			return -ENOMEM;

		cpulist_parse(buf, &csd->cpus);
		list_add_tail(&csd->list, &csd_head);

		for_each_cpu(cpu, &csd->cpus)
			*per_cpu_ptr(__pcpu_csd, cpu) = csd;
	}

	tex_init();
	cpus_binding_init();
	sched_policy_init();
	cpu_weight_init();
	prefer_cpu_init();

	return 0;
}
