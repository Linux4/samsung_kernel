/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_qos.h>

#include "../sched.h"
#include "ems.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <soc/samsung/exynos-devfreq.h>
#include <dt-bindings/soc/samsung/exynos9830-devfreq.h>

static bool ems_core_initialized;

static void select_fit_cpus(struct tp_env *env)
{
	struct cpumask fit_cpus;
	struct cpumask cpus_allowed;
	struct cpumask ontime_fit_cpus, overcap_cpus, busy_cpus;
	struct task_struct *p = env->p;
	int cpu;

	/* Clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&busy_cpus);

	/*
	 * Make cpus allowed task assignment.
	 * The fit cpus are determined among allowed cpus of below:
	 *   1. cpus allowed of task
	 *   2. cpus allowed of ems tune
	 *   3. cpus allowed of cpu sparing
	 */
	cpumask_and(&cpus_allowed, &env->p->cpus_allowed, cpu_active_mask);
	cpumask_and(&cpus_allowed, &cpus_allowed, emstune_cpus_allowed(env->p));
	cpumask_and(&cpus_allowed, &cpus_allowed, ecs_cpus_allowed());
	if (cpumask_intersects(&cpus_allowed, &env->p->aug_cpus_allowed))
		cpumask_and(&cpus_allowed, &cpus_allowed, &env->p->aug_cpus_allowed);
	else
		cpumask_or(&cpus_allowed, &cpus_allowed, &env->p->aug_cpus_allowed);

	if (cpumask_empty(&cpus_allowed)) {
		/* no cpus allowed, give up on selecting cpus */
		return;
	}

	/* Get cpus where fits task from ontime migration */
	ontime_select_fit_cpus(p, &ontime_fit_cpus);
	cpumask_copy(&env->ontime_fit_cpus, &ontime_fit_cpus);

	/*
	 * Find cpus that becomes over capacity.
	 * If utilization of cpu with given task exceeds cpu capacity, it is
	 * over capacity.
	 *
	 * overcap_cpus = cpu util + task util > cpu capacity
	 */
	for_each_cpu(cpu, &cpus_allowed) {
		unsigned long capacity = capacity_cpu(cpu, p->sse);
		unsigned long new_util = _ml_cpu_util(cpu, p->sse);

		if (task_cpu(p) != cpu)
			new_util += ml_task_util(p);

		if (new_util > capacity)
			cpumask_set_cpu(cpu, &overcap_cpus);
	}

	/*
	 * Find busy cpus.
	 * If this function is called by ontime migration(env->wake == 0),
	 * it looks for busy cpu to exclude from selection. Utilization of cpu
	 * exceeds 12.5% of cpu capacity, it is defined as busy cpu.
	 * (12.5% : this percentage is heuristically obtained)
	 *
	 * However, the task wait time is too long (hungry state), don't consider
	 * the busy cpu to spread the task as much as possible.
	 *
	 * busy_cpus = cpu util >= 12.5% of cpu capacity
	 */
	if (!env->wake && !ml_task_hungry(p)) {
		for_each_cpu(cpu, &cpus_allowed) {
			int threshold = capacity_cpu(cpu, p->sse) >> 3;

			if (_ml_cpu_util(cpu, p->sse) >= threshold)
				cpumask_set_cpu(cpu, &busy_cpus);
		}

		goto combine_cpumask;
	}

combine_cpumask:
	/*
	 * To select cpuset where task fits, each cpumask is combined as
	 * below sequence:
	 *
	 * 1) Pick ontime_fit_cpus from cpus allowed.
	 * 2) Exclude overcap_cpu from fit cpus.
	 *    The utilization of cpu with given task become over capacity, the
	 *    cpu cannot process the task properly then performance drop.
	 *    therefore, overcap_cpu is excluded.
	 *
	 *    fit_cpus = cpus_allowed & ontime_fit_cpus & ~overcap_cpus
	 */
	cpumask_and(&fit_cpus, &cpus_allowed, &ontime_fit_cpus);
	cpumask_andnot(&fit_cpus, &fit_cpus, &overcap_cpus);

	/*
	 * Case: task migration
	 *
	 * 3) Exclude busy cpus if task migration.
	 *    To improve performance, do not select busy cpus in task
	 *    migration.
	 *
	 *    fit_cpus = fit_cpus & ~busy_cpus
	 */
	if (!env->wake) {
		cpumask_andnot(&fit_cpus, &fit_cpus, &busy_cpus);
		goto finish;
	}

finish:
	cpumask_copy(&env->fit_cpus, &fit_cpus);
	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&cpus_allowed),
		*(unsigned int *)cpumask_bits(&ontime_fit_cpus),
		*(unsigned int *)cpumask_bits(&overcap_cpus),
		*(unsigned int *)cpumask_bits(&busy_cpus));
}

/* setup cpu and task util */
static void get_util_snapshot(struct tp_env *env)
{
	int cpu, prev_cpu = task_cpu(env->p);
	int task_util_est = (int)ml_task_util_est(env->p);

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = task_util_est ? task_util_est : 1;

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		env->cpu_util_wo[cpu][USS] = _ml_cpu_util_est(cpu, USS);
		env->cpu_util_wo[cpu][SSE] = _ml_cpu_util_est(cpu, SSE);
		/* remove task util from previous cpu util */
		if (cpu == prev_cpu) {
			env->cpu_util_wo[prev_cpu][env->p->sse] =
				max_t(int, (env->cpu_util_wo[prev_cpu][env->p->sse] - task_util_est), 0);
		}
		env->cpu_util_with[cpu] = ml_cpu_util_with(cpu, env->p);
		env->cpu_rt_util[cpu] = cpu_util_rt(cpu_rq(cpu));

		/*
		 * fill cpu util for get_next_cap,
		 * It improves expecting next_cap in governor
		 */
		env->cpu_util[cpu] = ml_cpu_util(cpu) + env->cpu_rt_util[cpu];

		env->nr_running[cpu] = cpu_rq(cpu)->nr_running;

		trace_ems_snapshot_cpu(cpu, env->task_util,
			env->cpu_util_wo[cpu][USS], env->cpu_util_wo[cpu][SSE],
			env->cpu_util_with[cpu], env->cpu_rt_util[cpu],
			env->cpu_util[cpu], env->nr_running[cpu]);
	}
}

static void get_ready_env(struct tp_env *env)
{
	int cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		/*
		 * The weight is pre-defined in EMSTune.
		 * We can get weight depending on current emst mode.
		 */
		env->eff_weight[cpu] = emstune_eff_weight(env->p, cpu, idle_cpu(cpu));
	}
	/* snapshot util to use same util during core selection */
	get_util_snapshot(env);

	/*
	 * Among fit_cpus, idle cpus are included in env->idle_candidates
	 * and running cpus are included in the env->candidate.
	 * Both candidates are exclusive.
	 */
	cpumask_clear(&env->idle_candidates);
	for_each_cpu(cpu, &env->fit_cpus)
		if (idle_cpu(cpu))
			cpumask_set_cpu(cpu, &env->idle_candidates);
	cpumask_andnot(&env->candidates, &env->fit_cpus, &env->idle_candidates);

	trace_ems_candidates(env->p, env->sched_policy,
		*(unsigned int *)cpumask_bits(&env->candidates),
		*(unsigned int *)cpumask_bits(&env->idle_candidates));
}

/*
 * if there is more pending task than number of the llc_size
 * set prefer_idle to spread task fastly
 */
static int wake_wide(int sch)
{
	if (!emstune_wake_wide)
		return 0;

	if (emstune_boosted())
		return 0;

	/* allow wake wide if sch is greater than NR_CPUS-2 */
	if (sch + 2 >= NR_CPUS)
		return 1;

	return 0;
}

extern void sync_entity_load_avg(struct sched_entity *se);

int exynos_select_task_rq(struct task_struct *p, int prev_cpu,
			int sd_flag, int sync, int wake, int sch)
{
	int target_cpu = -1;
	struct tp_env env = {
		.p = p,
		.sched_policy = emstune_sched_policy(p),
		.wake = wake,
	};

	/*
	 * Update utilization of waking task to apply "sleep" period
	 * before selecting cpu.
	 */
	if (!(sd_flag & SD_BALANCE_FORK))
		sync_entity_load_avg(&p->se);

	if (wake_wide(sch)) {
		target_cpu = find_wide_cpu(&env, sch);
		if (target_cpu >= 0) {
			trace_ems_select_task_rq(p, target_cpu, sch, "wide_cpu");
			return target_cpu;
		}
	}

	select_fit_cpus(&env);
	if (cpumask_empty(&env.fit_cpus)) {
		/*
		 * If there are no fit cpus, give up on choosing rq and keep
		 * the task on the prev cpu
		 */
		trace_ems_select_task_rq(p, prev_cpu, wake, "no fit cpu");
		return prev_cpu;
	}

	if (sysctl_sched_sync_hint_enable && sync) {
		target_cpu = smp_processor_id();
		if (cpumask_test_cpu(target_cpu, &env.fit_cpus)) {
			trace_ems_select_task_rq(p, target_cpu, wake, "sync");
			return target_cpu;
		}
	}

	/*
	 * Get ready to find best cpu.
	 * Depending on the state of the task, the candidate cpus and C/E
	 * weight are decided.
	 */
	get_ready_env(&env);
	target_cpu = find_best_cpu(&env);
	if (target_cpu >= 0) {
		trace_ems_select_task_rq(p, target_cpu, wake, "best_cpu");
		return target_cpu;
	}

	/* Keep task on prev cpu if no efficient cpu is found */
	target_cpu = prev_cpu;
	trace_ems_select_task_rq(p, target_cpu, wake, "no benefit");

	return target_cpu;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	if (!ontime_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "ontime");
		return 0;
	}

	if (!emstune_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "emstune");
		return 0;
	}

	trace_ems_can_migrate_task(p, dst_cpu, true, "n/a");

	return 1;
}

struct sysbusy {
	raw_spinlock_t lock;
	u64 last_update_time;
	u64 boost_duration;
	struct work_struct work;
} sysbusy;

static void sysbusy_boost_fn(struct work_struct *work)
{
	if (sysbusy.boost_duration)
		exynos_devfreq_alt_mode_change(DEVFREQ_MIF, 1);
	else
		exynos_devfreq_alt_mode_change(DEVFREQ_MIF, 0);
}

void sysbusy_boost(void)
{
	int cpu, busy_count = 0;
	unsigned long now = jiffies;
	u64 old_boost_duration;

	if (unlikely(!ems_core_initialized))
		return;

	if (!raw_spin_trylock(&sysbusy.lock))
		return;

	if (now <= sysbusy.last_update_time + sysbusy.boost_duration)
		goto out;

	sysbusy.last_update_time = now;

	for_each_online_cpu(cpu) {
		/* count busy cpu */
		if (ml_cpu_util(cpu) * 100 >= capacity_cpu(cpu, USS) * 95)
			busy_count++;
	}

	old_boost_duration = sysbusy.boost_duration;
	if (busy_count >= (cpumask_weight(cpu_possible_mask) >> 1)) {
		sysbusy.boost_duration = 250;	/* 250HZ == 1s*/
		trace_ems_sysbusy_boost(1);
	} else {
		sysbusy.boost_duration = 0;
		trace_ems_sysbusy_boost(0);
	}

	if (old_boost_duration != sysbusy.boost_duration)
		schedule_work(&sysbusy.work);

out:
	raw_spin_unlock(&sysbusy.lock);
}

static ssize_t show_sched_topology(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cpu;
	struct sched_domain *sd;
	int ret = 0;

	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		int sched_domain_level = 0;

		sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
		while (sd->parent) {
			sched_domain_level++;
			sd = sd->parent;
		}

		for_each_lower_domain(sd) {
			ret += snprintf(buf + ret, 70,
					"[lv%d] cpu%d: flags=%#x sd->span=%#x sg->span=%#x\n",
					sched_domain_level, cpu, sd->flags,
					*(unsigned int *)cpumask_bits(sched_domain_span(sd)),
					*(unsigned int *)cpumask_bits(sched_group_span(sd->groups)));
			sched_domain_level--;
		}
		ret += snprintf(buf + ret,
				50, "----------------------------------------\n");
	}
	rcu_read_unlock();

	return ret;
}

static struct kobj_attribute sched_topology_attr =
__ATTR(sched_topology, 0444, show_sched_topology, NULL);

struct kobject *ems_kobj;

static int __init init_ems_core(void)
{
	int ret;

	ems_kobj = kobject_create_and_add("ems", kernel_kobj);

	ret = sysfs_create_file(ems_kobj, &sched_topology_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	raw_spin_lock_init(&sysbusy.lock);
	INIT_WORK(&sysbusy.work, sysbusy_boost_fn);

	ems_core_initialized = true;

	return 0;
}
core_initcall(init_ems_core);

void __init init_ems(void)
{
	init_part();
}
