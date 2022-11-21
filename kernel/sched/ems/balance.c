/*
 * Load balance for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/***************************************************************/
/*			idle balance				*/
/***************************************************************/
static int ems_find_target_cpu(int dst_cpu)
{
	struct cpumask candidates;
	int cpu, target_cpu = -1;
	unsigned long util, target_util = 0;

	cpumask_copy(&candidates, cpu_coregroup_mask(dst_cpu));

	for_each_cpu(cpu, &candidates) {
		if (cpu == dst_cpu)
			continue;

		/* sanity checks */
		if (!cpu_active(cpu) || !ecs_cpu_available(cpu, NULL))
			continue;

		/* no runnable task in this cpu */
		if (cpu_rq(cpu)->nr_running <= 1 || !cpu_rq(cpu)->cfs.h_nr_running)
			continue;

		util = ml_cpu_util(cpu);
		if (util > target_util) {
			target_cpu = cpu;
			target_util = util;
		}

		trace_ems_find_target_cpu(cpu, util);
	}

	return target_cpu;
}

static void ems_detach_task(struct task_struct *p, struct rq *src_rq,
		struct rq *dst_rq)
{
	deactivate_task(src_rq, p, 0);

	double_lock_balance(src_rq, dst_rq);
	if (!(src_rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(src_rq);
	set_task_cpu(p, dst_rq->cpu);
	double_unlock_balance(src_rq, dst_rq);
}

static void ems_attach_task(struct task_struct *p, struct rq *dst_rq)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&dst_rq->lock, flags);
	activate_task(dst_rq, p, 0);
	check_preempt_curr(dst_rq, p, 0);
	raw_spin_unlock_irqrestore(&dst_rq->lock, flags);
}

static int ems_migrate_task(int src_cpu, int dst_cpu)
{
	struct task_struct *p, *target_task = NULL;
	struct rq *src_rq = cpu_rq(src_cpu);
	struct rq *dst_rq = cpu_rq(dst_cpu);
	unsigned long flags;

	if (dst_rq->nr_running)
		return 0;

	raw_spin_lock_irqsave(&src_rq->lock, flags);
	list_for_each_entry(p, &src_rq->cfs_tasks, se.group_node) {
		/* Give chance to runnable task */
		if (task_running(src_rq, p))
			continue;

		/* Do not pick task if it is under migration */
		if (p->on_rq == TASK_ON_RQ_MIGRATING)
			continue;

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		target_task = p;
		break;
	}

	/* No task to migarte */
	if (!target_task) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return 0;
	}

	/* Detach target task from src_cpu */
	ems_detach_task(target_task, src_rq, dst_rq);

	raw_spin_unlock_irqrestore(&src_rq->lock, flags);

	/* Attach target task to dst_cpu */
	ems_attach_task(target_task, dst_rq);

	/* a task is pulled */
	return 1;
}

static bool ems_need_idle_balance(struct rq *this_rq)
{
	if (!READ_ONCE(this_rq->rd->overload))
		return false;

	if ((this_rq->avg_idle < SCHED_MIGRATION_COST) &&
			atomic_read(&this_rq->nr_iowait))
		return false;

	return true;
}

void ems_newidle_balance(void *data, struct rq *this_rq,
		struct rq_flags *rf, int *pulled_task, int *done)
{
	unsigned long next_balance = jiffies + HZ;
	int this_cpu = this_rq->cpu;
	int target_cpu = -1;

	*done = 1;

	this_rq->misfit_task_load = 0;
	this_rq->idle_stamp = rq_clock(this_rq);

	/* Do not pull any task */
	if (!cpu_active(this_cpu) || sysbusy_on_somac() ||
			!ecs_cpu_available(this_cpu, NULL))
		return;

	rq_unpin_lock(this_rq, rf);

	if (this_rq->nr_running)
		goto out;

	if (!ems_need_idle_balance(this_rq))
		goto out_repin;

	raw_spin_unlock(&this_rq->lock);

	target_cpu = ems_find_target_cpu(this_cpu);
	/* we do not need to pull any task */
	if (target_cpu == -1) {
		raw_spin_lock(&this_rq->lock);
		goto out;
	}

	/* Migrate a task from target_cpu to this_cpu */
	*pulled_task = ems_migrate_task(target_cpu, this_cpu);

	raw_spin_lock(&this_rq->lock);

out:
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

	/* If we pulled a task, clear the idle stamp and move the next balance */
	if (*pulled_task) {
		this_rq->idle_stamp = 0;

		/* Move the next balance forward */
		if (time_after(this_rq->next_balance, next_balance))
			this_rq->next_balance = next_balance;
	}
	else
		*done = 0;

out_repin:
	rq_repin_lock(this_rq, rf);

	trace_ems_newidle_balance(this_cpu, target_cpu, *pulled_task);
}
