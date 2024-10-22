// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <linux/sched/topology.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "eas_trace.h"
#include "common.h"
#include <mt-plat/mtk_irq_mon.h>
#include "sugov/cpufreq.h"

DEFINE_PER_CPU(struct task_rotate_work, task_rotate_works);
bool big_task_rotation_enable = true;
int sched_min_cap_orig_cpu = -1;
#define TASK_ROTATION_THRESHOLD_NS 6000000
#define HEAVY_TASK_NUM 4

unsigned int capacity_margin = 1280;

struct task_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

int is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	int reserved = 0;
	struct rq_flags rf;

	irq_log_store();
	rq_lock(rq, &rf);
	irq_log_store();
	reserved = rq->active_balance;
	irq_log_store();
	rq_unlock(rq, &rf);
	irq_log_store();

	return reserved;
}

bool is_min_capacity_cpu(int cpu)
{
	if (arch_scale_cpu_capacity(cpu) ==
		arch_scale_cpu_capacity(sched_min_cap_orig_cpu))
		return true;

	return false;
}

bool is_max_capacity_cpu(int cpu)
{
	return arch_scale_cpu_capacity(cpu) == SCHED_CAPACITY_SCALE;
}

static void task_rotate_work_func(struct work_struct *work)
{
	struct task_rotate_work *wr = container_of(work,
				struct task_rotate_work, w);

	int ret = -1;
	struct rq *src_rq, *dst_rq;

	irq_log_store();
	ret = migrate_swap(wr->src_task, wr->dst_task,
			task_cpu(wr->dst_task), task_cpu(wr->src_task));
	irq_log_store();

	if (ret == 0) {
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
						wr->src_task->pid,
						wr->dst_task->pid,
						true);
	}

	irq_log_store();
	put_task_struct(wr->src_task);
	irq_log_store();
	put_task_struct(wr->dst_task);
	irq_log_store();

	src_rq = cpu_rq(wr->src_cpu);
	dst_rq = cpu_rq(wr->dst_cpu);

	irq_log_store();
	local_irq_disable();
	double_rq_lock(src_rq, dst_rq);
	irq_log_store();
	src_rq->active_balance = 0;
	dst_rq->active_balance = 0;
	irq_log_store();
	double_rq_unlock(src_rq, dst_rq);
	local_irq_enable();
	irq_log_store();
}

void task_rotate_work_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct task_rotate_work *wr = &per_cpu(task_rotate_works, i);

		INIT_WORK(&wr->w, task_rotate_work_func);
	}
}

void task_rotate_init(void)
{
	int i, min_cap_orig_cpu = -1;
	unsigned long min_orig_cap = ULONG_MAX;

	/* find min_cap cpu */
	for_each_possible_cpu(i) {
		if (capacity_orig_of(i) >= min_orig_cap)
			continue;
		min_orig_cap = capacity_orig_of(i);
		min_cap_orig_cpu = i;
	}

	if (min_cap_orig_cpu >= 0) {
		sched_min_cap_orig_cpu = min_cap_orig_cpu;
		pr_info("scheduler: min_cap_orig_cpu = %d\n",
			sched_min_cap_orig_cpu);
	} else
		pr_info("scheduler: can not find min_cap_orig_cpu\n");

	/* init rotate work */
	task_rotate_work_init();
}

bool system_has_many_heavy_task(void)
{
	int i, heavy_task = 0;

	irq_log_store();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = READ_ONCE(rq->curr);

		if (curr_task &&
			!task_fits_capacity(curr_task, READ_ONCE(rq->cpu_capacity),
						get_adaptive_margin(i)))
			heavy_task += 1;

		if (heavy_task >= HEAVY_TASK_NUM)
			break;
	}
	irq_log_store();

	if (heavy_task < HEAVY_TASK_NUM)
		return 0;

	return 1;
}

void task_check_for_rotation(struct rq *src_rq)
{
	u64 wc, wait, max_wait = 0, run, max_run = 0;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	struct task_rotate_work *wr = NULL;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	int force = 0;
	struct cpumask src_eff;
	struct cpumask dst_eff;
	bool src_ls;
	bool dst_ls;

	irq_log_store();

	if (!big_task_rotation_enable) {
		irq_log_store();
		return;
	}

	if (is_max_capacity_cpu(src_cpu)) {
		irq_log_store();
		return;
	}

	if (cpu_paused(src_cpu)) {
		irq_log_store();
		return;
	}

	if (!(rd->android_vendor_data1[0])) {
		irq_log_store();
		return;
	}

	irq_log_store();
	wc = ktime_get_raw_ns();
	irq_log_store();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct rot_task_struct *rts;

		if (cpu_paused(i))
			continue;

		if (!is_min_capacity_cpu(i))
			continue;

		if (!READ_ONCE(rq->misfit_task_load) ||
			(READ_ONCE(rq->curr->policy) != SCHED_NORMAL))
			continue;

		if (is_reserved(i))
			continue;

		compute_effective_softmask(rq->curr, &dst_ls, &dst_eff);
		if (dst_ls && !cpumask_test_cpu(src_cpu, &dst_eff))
			continue;

		rts = &((struct mtk_task *) rq->curr->android_vendor_data1)->rot_task;
		wait = wc - READ_ONCE(rts->ktime_ns);

		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}
	irq_log_store();

	if (deserved_cpu != src_cpu) {
		irq_log_store();
		return;
	}

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct rot_task_struct *rts;

		if (cpu_paused(i))
			continue;

		if (capacity_orig_of(i) <= capacity_orig_of(src_cpu))
			continue;

		if (READ_ONCE(rq->curr->policy) != SCHED_NORMAL)
			continue;

		if (READ_ONCE(rq->nr_running) > 1)
			continue;

		if (is_reserved(i))
			continue;

		compute_effective_softmask(rq->curr, &dst_ls, &dst_eff);
		if (dst_ls && !cpumask_test_cpu(src_cpu, &dst_eff))
			continue;

		rts = &((struct mtk_task *) rq->curr->android_vendor_data1)->rot_task;
		run = wc - READ_ONCE(rts->ktime_ns);

		if (run < TASK_ROTATION_THRESHOLD_NS)
			continue;

		if (run > max_run) {
			max_run = run;
			dst_cpu = i;
		}
	}
	irq_log_store();

	if (dst_cpu == nr_cpu_ids) {
		irq_log_store();
		return;
	}

	dst_rq = cpu_rq(dst_cpu);
	irq_log_store();
	double_rq_lock(src_rq, dst_rq);
	irq_log_store();

	if (dst_rq->curr->policy == SCHED_NORMAL) {
		if (!cpumask_test_cpu(dst_cpu,
					src_rq->curr->cpus_ptr) ||
			!cpumask_test_cpu(src_cpu,
					dst_rq->curr->cpus_ptr)) {
			double_rq_unlock(src_rq, dst_rq);
			irq_log_store();
			return;
		}

		if (cpu_paused(src_cpu) || cpu_paused(dst_cpu)) {
			double_rq_unlock(src_rq, dst_rq);
			irq_log_store();
			return;
		}

		compute_effective_softmask(dst_rq->curr, &dst_ls, &dst_eff);
		if (dst_ls && !cpumask_test_cpu(src_cpu, &dst_eff)) {
			double_rq_unlock(src_rq, dst_rq);
			return;
		}

		compute_effective_softmask(src_rq->curr, &src_ls, &src_eff);
		if (src_ls && !cpumask_test_cpu(dst_cpu, &src_eff)) {
			double_rq_unlock(src_rq, dst_rq);
			return;
		}

		if (!src_rq->active_balance && !dst_rq->active_balance) {
			src_rq->active_balance = 1;
			dst_rq->active_balance = 1;

			irq_log_store();
			get_task_struct(src_rq->curr);
			irq_log_store();
			get_task_struct(dst_rq->curr);
			irq_log_store();

			wr = &per_cpu(task_rotate_works, src_cpu);

			wr->src_task = src_rq->curr;
			wr->dst_task = dst_rq->curr;

			wr->src_cpu = src_rq->cpu;
			wr->dst_cpu = dst_rq->cpu;
			force = 1;
		}
	}
	irq_log_store();
	double_rq_unlock(src_rq, dst_rq);
	irq_log_store();

	if (force) {
		queue_work_on(src_cpu, system_highpri_wq, &wr->w);
		irq_log_store();
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
					wr->src_task->pid, wr->dst_task->pid,
					false);
	}
	irq_log_store();
}

void set_big_task_rotation(bool enable)
{
	big_task_rotation_enable = enable;
}
EXPORT_SYMBOL_GPL(set_big_task_rotation);

void rotat_after_enqueue_task(void __always_unused *data, struct rq *rq,
				struct task_struct *p)
{
	struct rot_task_struct *rts = &((struct mtk_task *)p->android_vendor_data1)->rot_task;

	WRITE_ONCE(rts->ktime_ns, ktime_get_raw_ns());
}

void rotat_task_stats(void __always_unused *data,
				struct task_struct *p)
{
	struct rot_task_struct *rts = &((struct mtk_task *)p->android_vendor_data1)->rot_task;

	WRITE_ONCE(rts->ktime_ns, ktime_get_raw_ns());
}

void rotat_task_newtask(void __always_unused *data,
				struct task_struct *p, unsigned long clone_flags)
{
	struct rot_task_struct *rts = &((struct mtk_task *)p->android_vendor_data1)->rot_task;

	WRITE_ONCE(rts->ktime_ns, 0);
}
