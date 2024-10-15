/*
 *  Copyright (C) 2017 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems

#if !defined(_TRACE_EMS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_H

#include <linux/tracepoint.h>

struct tp_env;

#define LABEL_LEN 32

/*
 * Tracepoint for finding cpus allowed
 */
TRACE_EVENT(ems_find_cpus_allowed,

	TP_PROTO(struct tp_env *env, struct cpumask *mask),

	TP_ARGS(env, mask),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	bool,		per_cpu_kthread		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	active_mask		)
		__field(	unsigned int,	ecs_cpus_allowed	)
		__field(	unsigned int,	cpus_ptr		)
		__field(	unsigned int,	cpus_binding_mask	)
		__field(	unsigned int,	el0_mask		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->per_cpu_kthread	= env->per_cpu_kthread;
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->active_mask		= *(unsigned int *)cpumask_bits(&mask[1]);
		__entry->ecs_cpus_allowed	= *(unsigned int *)cpumask_bits(&mask[2]);
		__entry->cpus_ptr		= *(unsigned int *)cpumask_bits(&mask[0]);
		__entry->cpus_binding_mask	= *(unsigned int *)cpumask_bits(&mask[3]);
		__entry->el0_mask		= *(unsigned int *)cpumask_bits(&mask[4]);
	),

	TP_printk("comm=%15s, pid=%6d, per_cpu_kthread=%1d, "
		"cpus_allowed=0x%03x, active_mask=0x%03x, ecs_cpus_allowed=0x%03x, "
		"cpus_ptr=0x%03x, cpus_binding_mask=0x%03x, el0_mask=0x%03x",
		__entry->comm, __entry->pid, __entry->per_cpu_kthread,
		__entry->cpus_allowed, __entry->active_mask, __entry->ecs_cpus_allowed,
		__entry->cpus_ptr, __entry->cpus_binding_mask, __entry->el0_mask)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_find_fit_cpus,

	TP_PROTO(struct tp_env *env),

	TP_ARGS(env),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prev_cpu		)
		__field(	int,		cl_sync			)
		__field(	int,		sync			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	task_util_clamped	)
		__field(	int,		task_ipc		)
		__field(	int,		task_class		)
		__field(	int,		sched_policy		)
		__field(	int,		pl_init_index		)
		__field(	int,		pl_depth		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	overcap_cpus		)
		__field(	unsigned int,	migrating_cpus		)

	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->prev_cpu		= task_cpu(env->p);
		__entry->cl_sync		= env->cl_sync;
		__entry->sync			= env->sync;
		__entry->task_util		= env->task_util;
		__entry->task_ipc		= env->ipc;
		__entry->task_class		= env->task_class;
		__entry->sched_policy		= env->sched_policy;
		__entry->pl_init_index		= env->pl_init_index;
		__entry->pl_depth		= env->pl_depth;
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->fit_cpus		= *(unsigned int *)cpumask_bits(&env->fit_cpus);
		__entry->overcap_cpus		= *(unsigned int *)cpumask_bits(&env->overcap_cpus);
		__entry->migrating_cpus		= *(unsigned int *)cpumask_bits(&env->migrating_cpus);
	),

	TP_printk("comm=%15s, pid=%6d, prev_cpu=%1d, cl_sync=%1d, sync=%1d, "
		"task_util=%4lu, ipc=%4d "
		"task_class=%1d, sched_policy=%1d, pl_init_index=%1d, pl_depth=%1d "
		"fit_cpus=0x%03x, cpus_allowed=0x%03x, overcap_cpus=0x%03x, "
		"migrating_cpus=0x%03x",
		__entry->comm, __entry->pid, __entry->prev_cpu, __entry->cl_sync, __entry->sync,
		__entry->task_util, __entry->task_ipc, __entry->task_class,
		__entry->sched_policy, __entry->pl_init_index, __entry->pl_depth,
		__entry->fit_cpus, __entry->cpus_allowed, __entry->overcap_cpus,
		__entry->migrating_cpus)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_task_rq,

	TP_PROTO(struct tp_env *env, int target_cpu, char *state),

	TP_ARGS(env, target_cpu, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cgroup_idx		)
		__field(	int,		target_cpu		)
		__field(	int,		wakeup			)
		__field(	u64,		task_util		)
		__field(	u32,		nr_running		)
		__array(	char,		state,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->cgroup_idx		= env->cgroup_idx;
		__entry->wakeup			= !env->p->on_rq;
		__entry->task_util		= env->task_util;
		__entry->target_cpu		= target_cpu;
		__entry->nr_running		= cpu_rq(target_cpu)->nr_running;
		memcpy(__entry->state, state, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, cgroup_idx=%1d, wakeup=%1d, "
		"task_util=%4lld, target_cpu=%2d, nr_running=%2u, state=%s",
		__entry->comm, __entry->pid, __entry->cgroup_idx, __entry->wakeup,
		__entry->task_util, __entry->target_cpu, __entry->nr_running, __entry->state)
);

/*
 * Tracepoint for the EGO
 */
TRACE_EVENT(ego_req_freq,

	TP_PROTO(int cpu, unsigned int target_freq, unsigned int min_freq,
		unsigned int max_freq, unsigned int org_freq, unsigned int eng_freq,
		unsigned long util, unsigned long max, char *reason),

	TP_ARGS(cpu, target_freq, min_freq, max_freq, org_freq, eng_freq, util, max, reason),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned int,	target_freq		)
		__field(	unsigned int,	min_freq		)
		__field(	unsigned int,	max_freq		)
		__field(	unsigned int,	org_freq		)
		__field(	unsigned int,	eng_freq		)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	max			)
		__array(	char,		reason, 32		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->target_freq		= target_freq;
		__entry->min_freq		= min_freq;
		__entry->max_freq		= max_freq;
		__entry->org_freq		= org_freq;
		__entry->eng_freq		= eng_freq;
		__entry->util			= util;
		__entry->max			= max;
		memcpy(__entry->reason, reason, 32);
	),

	TP_printk("cpu=%1d, target_freq=%7d, min_freq=%7d, max_freq=%7d, "
		"org_freq=%7d, eng_freq=%7d, util=%4ld, max=%4ld, reason=%s",
		__entry->cpu, __entry->target_freq, __entry->min_freq, __entry->max_freq,
		__entry->org_freq, __entry->eng_freq, __entry->util, __entry->max, __entry->reason)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long task_util, int src_cpu, int dst_cpu,
		char *reason),

	TP_ARGS(p, task_util, src_cpu, dst_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	task_util		)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__array(	char,		reason,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->task_util		= task_util;
		__entry->src_cpu		= src_cpu;
		__entry->dst_cpu		= dst_cpu;
		memcpy(__entry->reason, reason, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, task_util=%4lu, src_cpu=%1d, dst_cpu=%1d, reason=%s",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->src_cpu, __entry->dst_cpu, __entry->reason)
);

/*
 * Tracepoint for emstune mode
 */
TRACE_EVENT(emstune_mode,

	TP_PROTO(int mode, int level),

	TP_ARGS(mode, level),

	TP_STRUCT__entry(
		__field(	int,		mode			)
		__field(	int,		level			)
	),

	TP_fast_assign(
		__entry->mode			= mode;
		__entry->level			= level;
	),

	TP_printk("mode=%1d, level=%1d", __entry->mode, __entry->level)
);

/*
 * Tracepoint for logging FRT schedule activity
 */
TRACE_EVENT(frt_select_task_rq,

	TP_PROTO(struct task_struct *p, int best_cpu, char* reason),

	TP_ARGS(p, best_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		best_cpu		)
		__field(	int,		prev_cpu		)
		__field(	int,		prev_prio		)
		__field(	int,		best_prio		)
		__array(	char,		reason,	TASK_COMM_LEN	)
	),	

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->prev_cpu		= task_cpu(p);
		__entry->best_cpu		= best_cpu;
		__entry->prev_prio		= p->prio;
		__entry->best_prio		= cpu_rq(best_cpu)->rt.highest_prio.curr;
		memcpy(__entry->reason, reason, TASK_COMM_LEN);
	),

	TP_printk("comm=%15s, pid=%6d, prev_cpu=%1d, best_cpu=%1d, prev_prio=%3d, best_prio=%3d, reason=%s.",
		__entry->comm, __entry->pid, __entry->prev_cpu, __entry->best_cpu,
		__entry->prev_prio, __entry->best_prio, __entry->reason)
);

/*
 * Tracepoint for system busy state change
 */
TRACE_EVENT(sysbusy_state,

	TP_PROTO(int old_state, int next_state),

	TP_ARGS(old_state, next_state),

	TP_STRUCT__entry(
		__field(	int,		old_state		)
		__field(	int,		next_state		)
	),

	TP_fast_assign(
		__entry->old_state		= old_state;
		__entry->next_state		= next_state;
	),

	TP_printk("old_state=%1d, next_state=%1d",
		__entry->old_state, __entry->next_state)
);

/* CPUIDLE Governor */
TRACE_EVENT(halo_select,

	TP_PROTO(u32 waker, u32 selector, s64 resi, s64 lat, s64 tick, s64 timer, s64 vsync,
		s64 det, s64 pred, s64 expired, u32 idx),

	TP_ARGS(waker, selector, resi, lat, tick, timer, vsync, det, pred, expired, idx),

	TP_STRUCT__entry(
		__field(	u32,		waker			)
		__field(	u32,		selector		)
		__field(	s64,		resi			)
		__field(	s64,		lat			)
		__field(	s64,		tick			)
		__field(	s64,		timer			)
		__field(	s64,		vsync			)
		__field(	s64,		det			)
		__field(	s64,		pred			)
		__field(	s64,		expired			)
		__field(	u32,		idx			)
	),

	TP_fast_assign(
		__entry->waker			= waker;
		__entry->selector		= selector;
		__entry->resi			= resi;
		__entry->lat			= lat;
		__entry->tick			= tick;
		__entry->timer			= timer;
		__entry->vsync			= vsync;
		__entry->det			= det;
		__entry->pred			= pred;
		__entry->expired		= expired;
		__entry->idx			= idx;
	),

	TP_printk("waker=%1d, selector=%1d, resi=%13lld, lat=%13lld, tick=%7lld, "
		"timer=%10lld, vsync=%19lld, det=%10lld, pred=%19lld, expired=%8lld, idx=%1u",
		__entry->waker,  __entry->selector, __entry->resi, __entry->lat, __entry->tick,
		__entry->timer, __entry->vsync, __entry->det, __entry->pred, __entry->expired,
		__entry->idx)
);

/*
 * Tracepoint for active load balance
 */
TRACE_EVENT(lb_active_migration,

	TP_PROTO(struct task_struct *p, int prev_cpu, int new_cpu, char *reason),

	TP_ARGS(p, prev_cpu, new_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		misfit			)
		__field(	u64,		util			)
		__field(	int,		prev_cpu		)
		__field(	int,		new_cpu			)
		__array(	char,		reason,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->misfit			= ems_task_misfited(p);
		__entry->util			= ml_uclamp_task_util(p);
		__entry->prev_cpu		= prev_cpu;
		__entry->new_cpu		= new_cpu;
		strncpy(__entry->reason, reason, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, misfit=%1d, util=%4llu, prev_cpu=%1d, new_cpu=%1d, reason=%s",
		__entry->comm, __entry->pid, __entry->misfit, __entry->util,
		__entry->prev_cpu, __entry->new_cpu, __entry->reason)
);

/*
 * Tracepoint for find busiest queue
 */
TRACE_EVENT(lb_find_busiest_queue,

	TP_PROTO(int dst_cpu, int busiest_cpu, struct cpumask *src_cpus),

	TP_ARGS(dst_cpu, busiest_cpu, src_cpus),

	TP_STRUCT__entry(
		__field(	int,		dst_cpu			)
		__field(	int,		busiest_cpu		)
		__field(	unsigned int,	src_cpus		)
	),

	TP_fast_assign(
		__entry->dst_cpu		= dst_cpu;
		__entry->busiest_cpu		= busiest_cpu;
		__entry->src_cpus		= *(unsigned int *)cpumask_bits(src_cpus);
	),

	TP_printk("dst_cpu=%1d, busiest_cpu=%2d, src_cpus=0x%03x",
		__entry->dst_cpu, __entry->busiest_cpu, __entry->src_cpus)
);

/*
 * Tracepoint for ems newidle balance
 */
TRACE_EVENT(lb_newidle_balance,

	TP_PROTO(int this_cpu, int busy_cpu, int pulled, bool short_idle),

	TP_ARGS(this_cpu, busy_cpu, pulled, short_idle),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		busy_cpu		)
		__field(	int,		pulled			)
		__field(	unsigned int,	nr_running		)
		__field(	unsigned int,	rt_nr_running		)
		__field(	int,		nr_iowait		)
		__field(	u64,		avg_idle		)
		__field(	bool,		short_idle		)
		__field(	int,		overload		)
	),

	TP_fast_assign(
		__entry->cpu			= this_cpu;
		__entry->busy_cpu		= busy_cpu;
		__entry->pulled			= pulled;
		__entry->nr_running		= cpu_rq(this_cpu)->nr_running;
		__entry->rt_nr_running		= cpu_rq(this_cpu)->rt.rt_nr_running;
		__entry->nr_iowait		= atomic_read(&(cpu_rq(this_cpu)->nr_iowait));
		__entry->avg_idle		= cpu_rq(this_cpu)->avg_idle;
		__entry->short_idle		= short_idle;
		__entry->overload		= cpu_rq(this_cpu)->rd->overload;
	),

	TP_printk("cpu=%1d, busy_cpu=%2d, pulled=%1d, nr_running=%2u, rt_nr_running=%2u, "
		"nr_iowait=%1d, avg_idle=%7llu, short_idle=%1d, overload=%1d",
		__entry->cpu, __entry->busy_cpu, __entry->pulled,
		__entry->nr_running, __entry->rt_nr_running, __entry->nr_iowait,
		__entry->avg_idle, __entry->short_idle, __entry->overload)
);

TRACE_EVENT(ems_update_cpu_capacity,

	TP_PROTO(int cpu, unsigned long prev, unsigned long new_cap,
		unsigned long freq_cap, unsigned long arch_cap),

	TP_ARGS(cpu, prev, new_cap, freq_cap, arch_cap),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	prev			)
		__field(	unsigned long,	new_cap			)
		__field(	unsigned long,	freq_cap		)
		__field(	unsigned long,	arch_cap		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->prev			= prev;
		__entry->new_cap		= new_cap;
		__entry->freq_cap		= freq_cap;
		__entry->arch_cap		= arch_cap;
	),

	TP_printk("cpu=%d prev=%lu new_cap=%lu freq_cap=%lu arch_cap=%lu",
		__entry->cpu, __entry->prev, __entry->new_cap, __entry->freq_cap,
		__entry->arch_cap)
);
#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
