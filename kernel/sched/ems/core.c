/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_qos.h>
#include <linux/platform_device.h>

#include "../sched.h"
#include "ems.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

extern int emstune_cur_level;
static void select_fit_cpus(struct tp_env *env)
{
	struct cpumask fit_cpus;
	struct cpumask cpus_allowed;
	struct cpumask ontime_fit_cpus, overcap_cpus, busy_cpus;
	struct task_struct *p = env->p;
	int cpu;
	struct cpumask mask_big_cpu;

	/* Clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&busy_cpus);
	cpumask_clear(&mask_big_cpu);

	/*
	 * Make cpus allowed task assignment.
	 * The fit cpus are determined among allowed cpus of below:
	 *   1. cpus allowed of task
	 *   2. cpus allowed of ems tune
	 *   3. cpus allowed of cpu sparing
	 */
	cpumask_and(&cpus_allowed, env->p->cpus_ptr, cpu_active_mask);
	cpumask_and(&cpus_allowed, &cpus_allowed, emstune_cpus_allowed(env->p));
	cpumask_and(&cpus_allowed, &cpus_allowed, ecs_cpus_allowed(env->p));
	cpumask_copy(&env->cpus_allowed, &cpus_allowed);

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
		unsigned long new_util;

		new_util = ml_cpu_util_without(cpu, p) + ml_task_util(p);
		if (new_util > capacity_cpu(cpu))
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
			int threshold = capacity_cpu(cpu) >> 3;

			if (ml_cpu_util(cpu) >= threshold)
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
	 * 3) Exclude cpu which prio_pinning task is assigned to
	 *
	 *    fit_cpus = cpus_allowed & ontime_fit_cpus & ~overcap_cpus
	 *			      & ~prio_pinning_assigned_cpus
	 */
	cpumask_and(&fit_cpus, &cpus_allowed, &ontime_fit_cpus);
	cpumask_andnot(&fit_cpus, &fit_cpus, &overcap_cpus);
	if (!need_prio_pinning(env->p))
		cpumask_andnot(&fit_cpus, &fit_cpus, get_prio_pinning_assigned_mask());

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
	if (emstune_cur_level == 1 && ml_task_util(p) < 100) {
		cpumask_set_cpu(7, &mask_big_cpu);
		cpumask_andnot(&fit_cpus, &fit_cpus, &mask_big_cpu);
	}
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
	int cpu;
	int task_util_est = (int)ml_task_util_est(env->p);

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = task_util_est ? task_util_est : 1;

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		env->cpu_util_wo[cpu] = ml_cpu_util_without(cpu, env->p);
		env->cpu_util_with[cpu] = ml_cpu_util_next(cpu, env->p, cpu);
		env->cpu_rt_util[cpu] = cpu_util_rt(cpu_rq(cpu));

		/*
		 * fill cpu util for get_next_cap,
		 * It improves expecting next_cap in governor
		 */
		env->cpu_util[cpu] = ml_cpu_util(cpu) + env->cpu_rt_util[cpu];
		env->nr_running[cpu] = cpu_rq(cpu)->nr_running;
		trace_ems_snapshot_cpu(cpu, env->task_util, env->cpu_util_wo[cpu],
			env->cpu_util_with[cpu], env->cpu_rt_util[cpu],
			env->cpu_util[cpu], env->nr_running[cpu]);
	}
}

static void filter_rt_cpu(struct tp_env *env)
{
	int cpu;
	struct cpumask mask;

	if (env->p->prio > IMPORT_PRIO)
		return;

	cpumask_copy(&mask, &env->fit_cpus);

	for_each_cpu(cpu, &env->fit_cpus)
		if (cpu_rq(cpu)->curr->sched_class == &rt_sched_class)
			cpumask_clear_cpu(cpu, &mask);

	if (!cpumask_empty(&mask))
		cpumask_copy(&env->fit_cpus, &mask);
}

static int tiny_threshold = 16;

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

	/* fill up cpumask for scheduling */
	select_fit_cpus(env);

	/* filter rt running cpu */
	filter_rt_cpu(env);

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

	/*
	 * Change target tasks's policy to SCHED_POLICY_ENERGY
	 * for power optimization, if
	 * 1) target task's sched_policy is SCHED_POLICY_EFF_TINY and
	 * 2) its utilization is under 6.25% of SCHED_CAPACITY_SCALE.
	 * or
	 * 3) tasks is worker thread.
	 */
	if ((env->sched_policy == SCHED_POLICY_EFF_TINY &&
	     ml_task_util_est(env->p) <= tiny_threshold) ||
	    (env->p->flags & PF_WQ_WORKER))
		env->sched_policy = SCHED_POLICY_ENERGY;

	trace_ems_candidates(env->p, env->sched_policy,
		*(unsigned int *)cpumask_bits(&env->candidates),
		*(unsigned int *)cpumask_bits(&env->idle_candidates));
}

static int
exynos_select_task_rq_dl(struct task_struct *p, int prev_cpu,
			 int sd_flag, int wake_flag)
{
	/* TODO */
	return -1;
}

static int
exynos_select_task_rq_rt(struct task_struct *p, int prev_cpu,
			 int sd_flag, int wake_flag)
{
	/* TODO */
	return -1;
}

extern void sync_entity_load_avg(struct sched_entity *se);

static int
exynos_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag)
{
	int target_cpu = -1;
	int sync = (wake_flag & WF_SYNC) && !(current->flags & PF_EXITING);
	int cl_sync = (wake_flag & WF_SYNC_CL) && !(current->flags & PF_EXITING);
	int wake = !p->on_rq;
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

	/*
	 * Get ready to find best cpu.
	 * Depending on the state of the task, the candidate cpus and C/E
	 * weight are decided.
	 */
	get_ready_env(&env);

	/* When sysbusy is detected, do scheduling under other policies */
	if (is_sysbusy()) {
		target_cpu = sysbusy_schedule(&env);
		if (target_cpu >= 0) {
			trace_ems_select_task_rq(p, target_cpu, wake, "sysbusy");
			return target_cpu;
		}
	}

	if (need_prio_pinning(p)) {
		target_cpu = prio_pinning_schedule(&env, prev_cpu);
		if (target_cpu >= 0) {
			trace_ems_select_task_rq(p, target_cpu, wake, "prio pinning");
			return target_cpu;
		}
	}

	if (cl_sync) {
		int cl_sync_cpu;
		struct cpumask cl_sync_mask;

		cl_sync_cpu = smp_processor_id();
		cpumask_and(&cl_sync_mask, &env.cpus_allowed,
					   cpu_coregroup_mask(cl_sync_cpu));
		if (!cpumask_empty(&cl_sync_mask)) {
			target_cpu = cpumask_any(&cl_sync_mask);
			trace_ems_select_task_rq(p, target_cpu, wake, "cl sync");
			return target_cpu;
		}
	}

	if (cpumask_empty(&env.fit_cpus)) {
		if (!cpumask_empty(&env.cpus_allowed) &&
			!cpumask_test_cpu(prev_cpu, &env.cpus_allowed)) {
			target_cpu = cpumask_any(&env.cpus_allowed);
			trace_ems_select_task_rq(p, target_cpu, wake, "cpus allowed");
			return target_cpu;
		}

		/* Handle sync flag */
		if (sync) {
			target_cpu = smp_processor_id();
			trace_ems_select_task_rq(p, target_cpu, wake, "sync");
			return target_cpu;
		}

		/*
		 * If there are no fit cpus, give up on choosing rq and keep
		 * the task on the prev cpu
		 */
		trace_ems_select_task_rq(p, prev_cpu, wake, "no fit cpu");
		return prev_cpu;
	}

	target_cpu = find_best_cpu(&env);
	if (target_cpu >= 0) {
		trace_ems_select_task_rq(p, target_cpu, wake, "best_cpu");
		return target_cpu;
	}

	/*
	 * Keep task on allowed cpus if no efficient cpu is found
	 * and prev cpu is not on allowed cpu
	 */
	if (!cpumask_empty(&env.cpus_allowed) &&
		!cpumask_test_cpu(prev_cpu, &env.cpus_allowed)) {
		target_cpu = cpumask_any(&env.cpus_allowed);
		trace_ems_select_task_rq(p, target_cpu, wake, "cpus allowed");
		return target_cpu;
	}

	/* Keep task on prev cpu if no efficient cpu is found */
	target_cpu = prev_cpu;
	trace_ems_select_task_rq(p, target_cpu, wake, "no benefit");

	if (!cpu_active(target_cpu))
		return cpumask_any(p->cpus_ptr);

	return target_cpu;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	if (!cpu_overutilized(capacity_cpu(task_cpu(p)), ml_cpu_util(task_cpu(p)))) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "underutilized");
		return 0;
	}

	if (!ontime_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "ontime");
		return 0;
	}

	if (!emstune_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "emstune");
		return 0;
	}

	if (need_prio_pinning(p)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "prio pinning");
		return 0;
	}

	trace_ems_can_migrate_task(p, dst_cpu, true, "n/a");

	return 1;
}

int exynos_select_task_rq(struct task_struct *p, int prev_cpu,
			  int sd_flag, int wake_flag)
{
	int cpu = -1;

	if (p->sched_class == &dl_sched_class)
		cpu = exynos_select_task_rq_dl(p, prev_cpu, sd_flag, wake_flag);
	else if (p->sched_class == &rt_sched_class)
		cpu = exynos_select_task_rq_rt(p, prev_cpu, sd_flag, wake_flag);
	else if (p->sched_class == &fair_sched_class)
		cpu = exynos_select_task_rq_fair(p, prev_cpu, sd_flag, wake_flag);

	if (cpu >= 0 && !is_dst_allowed(p, cpu))
		cpu = -1;

	return cpu;
}

void ems_tick(struct rq *rq)
{
	set_part_period_start(rq);
	update_cpu_active_ratio(rq, NULL, EMS_PART_UPDATE);

	if (rq->cpu == 0)
		frt_update_available_cpus(rq);

	monitor_sysbusy();
	somac_tasks();

	ontime_migration();
	ecs_update();
}

struct kobject *ems_kobj;

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

		for (; sd; sd = sd->child) {
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

static ssize_t show_tiny_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tiny_threshold);
}

static ssize_t store_tiny_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long threshold;

	if (!sscanf(buf, "%d", &threshold))
		return -EINVAL;

	tiny_threshold = threshold;

	return count;
}

static struct kobj_attribute tiny_threshold_attr =
__ATTR(tiny_threshold, 0644, show_tiny_threshold, store_tiny_threshold);

struct kobject *ems_drv_kobj;
static int ems_probe(struct platform_device *pdev)
{
	ems_drv_kobj = &pdev->dev.kobj;
	return 0;
}

static const struct of_device_id of_ems_match[] = {
	{ .compatible = "samsung,ems", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_ems_match);

static struct platform_driver ems_driver = {
	.driver = {
		.name = "ems",
		.owner = THIS_MODULE,
		.of_match_table = of_ems_match,
	},
	.probe		= ems_probe,
};

struct delayed_work ems_init_dwork;
static void ems_delayed_init(struct work_struct *work)
{
	sysbusy_init();
}

static int ems_init(void)
{
	int ret;

	ems_kobj = kobject_create_and_add("ems", kernel_kobj);

	ret = sysfs_create_file(ems_kobj, &sched_topology_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	ret = sysfs_create_file(ems_kobj, &tiny_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	hook_init();
	energy_table_init();
	part_init();
	ontime_init();
	esgov_pre_init();
	freqboost_init();
	frt_init();
	ecs_init();
	emstune_init();

	/* booting lock for sysbusy, it is expired after 40s */
	INIT_DELAYED_WORK(&ems_init_dwork, ems_delayed_init);
	schedule_delayed_work(&ems_init_dwork,
			msecs_to_jiffies(40 * MSEC_PER_SEC));

	return platform_driver_register(&ems_driver);
}
core_initcall(ems_init);
