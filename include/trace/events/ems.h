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

/*
 * Tracepoint for finding cpus allowed
 */
TRACE_EVENT(ems_find_cpus_allowed,

	TP_PROTO(struct tp_env *env, struct cpumask *mask),

	TP_ARGS(env, mask),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	bool,		isa_32			)
		__field(	bool,		per_cpu_kthread		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	active_mask		)
		__field(	unsigned int,	ecs_cpus_allowed	)
		__field(	unsigned int,	cpus_ptr		)
		__field(	unsigned int,	cpus_binding_mask	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->isa_32			= test_ti_thread_flag(&env->p->thread_info, TIF_32BIT);
		__entry->per_cpu_kthread	= env->per_cpu_kthread;
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->active_mask		= *(unsigned int *)cpumask_bits(&mask[0]);
		__entry->ecs_cpus_allowed	= *(unsigned int *)cpumask_bits(&mask[1]);
		__entry->cpus_ptr		= *(unsigned int *)cpumask_bits(&mask[2]);
		__entry->cpus_binding_mask	= *(unsigned int *)cpumask_bits(&mask[3]);
	),

	TP_printk("comm=%s pid=%d isa_32=%d per_cpu_kthread=%d cpus_allowed=%#x active_mask=%#x "
		  "ecs_cpus_allowed=%#x cpus_ptr=%#x cpus_binding_mask=%#x",
		  __entry->comm, __entry->pid, __entry->isa_32, __entry->per_cpu_kthread,
		  __entry->cpus_allowed, __entry->active_mask, __entry->ecs_cpus_allowed,
		  __entry->cpus_ptr, __entry->cpus_binding_mask)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_find_fit_cpus,

	TP_PROTO(struct tp_env *env, struct cpumask *mask),

	TP_ARGS(env, mask),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prev_cpu		)
		__field(	int,		cl_sync			)
		__field(	int,		sync			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	task_util_clamped	)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	overcap_cpus		)
		__field(	unsigned int,	tex_pinning_cpus	)
		__field(	unsigned int,	ontime_fit_cpus		)
		__field(	unsigned int,	prefer_cpu		)
		__field(	unsigned int,	busy_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->prev_cpu		= task_cpu(env->p);
		__entry->cl_sync		= env->cl_sync;
		__entry->sync			= env->sync;
		__entry->task_util		= env->task_util;
		__entry->task_util_clamped	= env->task_util_clamped;
		__entry->fit_cpus		= *(unsigned int *)cpumask_bits(&env->fit_cpus);
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->overcap_cpus		= *(unsigned int *)cpumask_bits(&mask[0]);
		__entry->tex_pinning_cpus	= *(unsigned int *)cpumask_bits(&mask[1]);
		__entry->ontime_fit_cpus	= *(unsigned int *)cpumask_bits(&mask[2]);
		__entry->prefer_cpu		= *(unsigned int *)cpumask_bits(&mask[3]);
		__entry->busy_cpu		= *(unsigned int *)cpumask_bits(&mask[4]);
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d cl_sync=%d sync=%d task_util=%lu task_util_clamped=%lu "
		  "fit_cpus=%#x cpus_allowed=%#x overcap_cpus=%#x tex_pinning_cpus=%#x ontime_fit_cpus=%#x "
		  "prefer_cpu=%#x busy_cpu=%#x",
		  __entry->comm, __entry->pid, __entry->prev_cpu, __entry->cl_sync, __entry->sync,
		  __entry->task_util, __entry->task_util_clamped, __entry->fit_cpus, __entry->cpus_allowed,
		  __entry->overcap_cpus, __entry->tex_pinning_cpus, __entry->ontime_fit_cpus,
		  __entry->prefer_cpu, __entry->busy_cpu)
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
		__array(	char,		state,		30	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid		= env->p->pid;
		__entry->cgroup_idx	= env->cgroup_idx;
		__entry->wakeup		= !env->p->on_rq;
		__entry->target_cpu	= target_cpu;
		memcpy(__entry->state, state, 30);
	),

	TP_printk("comm=%s pid=%d cgroup_idx=%d wakeup=%d target_cpu=%d state=%s",
		  __entry->comm, __entry->pid, __entry->cgroup_idx,
		  __entry->wakeup, __entry->target_cpu, __entry->state)
);

/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ems_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		src_cpu			)
		__field( int,		dst_cpu			)
		__field( int,		migrate			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d src_cpu=%d dst_cpu=%d migrate=%d reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

/*
 * Tracepoint for policy/PM QoS update in ESG
 */
TRACE_EVENT(esg_update_limit,

	TP_PROTO(int cpu, int min, int max),

	TP_ARGS(cpu, min, max),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		min				)
		__field( int,		max				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->min			= min;
		__entry->max			= max;
	),

	TP_printk("cpu=%d min_cap=%d, max_cap=%d",
		__entry->cpu, __entry->min, __entry->max)
);

/*
 * Tracepoint for frequency request in ESG
 */
TRACE_EVENT(esg_req_freq,

	TP_PROTO(int cpu, int util, int freq, int rapid_scale),

	TP_ARGS(cpu, util, freq, rapid_scale),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		util				)
		__field( int,		freq				)
		__field( int,		rapid_scale			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->freq			= freq;
		__entry->rapid_scale		= rapid_scale;
	),

	TP_printk("cpu=%d util=%d freq=%d rapid_scale=%d",
		__entry->cpu, __entry->util, __entry->freq, __entry->rapid_scale)
);

/*
 * Tracepoint for the EGO
 */
TRACE_EVENT(ego_req_freq,

	TP_PROTO(int cpu, unsigned int target_freq, unsigned int min_freq,
		unsigned int max_freq, unsigned int org_freq, unsigned int eng_freq,
		unsigned long util, unsigned long max),

	TP_ARGS(cpu, target_freq, min_freq, max_freq,
		org_freq, eng_freq, util, max),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned int,	target_freq			)
		__field( unsigned int,	min_freq			)
		__field( unsigned int,	max_freq			)
		__field( unsigned int,	org_freq			)
		__field( unsigned int,	eng_freq			)
		__field( unsigned long,	util				)
		__field( unsigned long,	max				)
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
	),

	TP_printk("cpu=%d target_f=%d min_f=%d max_f=%d org_f=%d eng_f=%d util=%ld max=%ld",
		__entry->cpu, __entry->target_freq, __entry->min_freq,
		__entry->max_freq, __entry->org_freq, __entry->eng_freq,
		__entry->util, __entry->max)
);

TRACE_EVENT(ego_cpu_eng,

	TP_PROTO(int cpu, unsigned long cap, unsigned int dyn,
		unsigned long sta, unsigned long idle_sum,
		unsigned long active_eng, unsigned long idle_eng),

	TP_ARGS(cpu, cap, dyn, sta, idle_sum, active_eng, idle_eng),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	cap				)
		__field( unsigned long,	dyn				)
		__field( unsigned long,	sta				)
		__field( unsigned long,	idle_sum			)
		__field( unsigned long,	active_eng			)
		__field( unsigned long,	idle_eng			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cap			= cap;
		__entry->dyn			= dyn;
		__entry->sta			= sta;
		__entry->idle_sum		= idle_sum;
		__entry->active_eng		= active_eng;
		__entry->idle_eng		= idle_eng;
	),

	TP_printk("cpu=%d cap=%lu dyn=%lu sta=%lu isum=%lu aeng=%lu ieng=%lu eng=%lu",
		__entry->cpu, __entry->cap, __entry->dyn,
		__entry->sta, __entry->idle_sum,
		__entry->active_eng, __entry->idle_eng,
		(__entry->active_eng + __entry->idle_eng))
);

/*
 * Tracepoint for the STT
 */
TRACE_EVENT(stt_tracking_htsk,

	TP_PROTO(int cpu, int pid, int hcnt, u64 expired, char *tag),

	TP_ARGS(cpu, pid, hcnt, expired, tag),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	pid			)
		__field( int,	hcnt			)
		__field( u64,	expired			)
		__array( char,	tag,		30	)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pid			= pid;
		__entry->hcnt			= hcnt;
		__entry->expired		= expired;
		memcpy(__entry->tag, tag, 30);
	),

	TP_printk("cpu=%d pid=%d hcnt=%d expired=%lu, tag=%s",
		__entry->cpu, __entry->pid, __entry->hcnt,
		__entry->expired, __entry->tag)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
				int src_cpu, int dst_cpu, char *reason),

	TP_ARGS(p, load, src_cpu, dst_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__array(	char,		reason,		16	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		memcpy(__entry->reason, reason, 16);
	),

	TP_printk("comm=%s pid=%d ontime_load_avg=%lu src_cpu=%d dst_cpu=%d reason=%s",
		__entry->comm, __entry->pid, __entry->load,
		__entry->src_cpu, __entry->dst_cpu, __entry->reason)
);

/*
 * Tracepoint for emstune mode
 */
TRACE_EVENT(emstune_mode,

	TP_PROTO(int next_mode, int next_level),

	TP_ARGS(next_mode, next_level),

	TP_STRUCT__entry(
		__field( int,		next_mode		)
		__field( int,		next_level		)
	),

	TP_fast_assign(
		__entry->next_mode		= next_mode;
		__entry->next_level		= next_level;
	),

	TP_printk("mode=%d level=%d", __entry->next_mode, __entry->next_level)
);

/*
 * Tracepoint for logging FRT schedule activity
 */
TRACE_EVENT(frt_select_task_rq,

	TP_PROTO(struct task_struct *tsk, int best, char* str),

	TP_ARGS(tsk, best, str),

	TP_STRUCT__entry(
		__array( char,	selectby,	TASK_COMM_LEN	)
		__array( char,	targettsk,	TASK_COMM_LEN	)
		__field( pid_t,	pid				)
		__field( int,	bestcpu				)
		__field( int,	prevcpu				)
	),

	TP_fast_assign(
		memcpy(__entry->selectby, str, TASK_COMM_LEN);
		memcpy(__entry->targettsk, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->bestcpu		= best;
		__entry->prevcpu		= task_cpu(tsk);
	),

	TP_printk("frt: comm=%s pid=%d assigned to #%d from #%d by %s.",
		  __entry->targettsk,
		  __entry->pid,
		  __entry->bestcpu,
		  __entry->prevcpu,
		  __entry->selectby)
);

/*
 * Tracepoint for system busy state change
 */
TRACE_EVENT(sysbusy_state,

	TP_PROTO(int old_state, int next_state),

	TP_ARGS(old_state, next_state),

	TP_STRUCT__entry(
		__field(int,	old_state)
		__field(int,	next_state)
	),

	TP_fast_assign(
		__entry->old_state	= old_state;
		__entry->next_state	= next_state;
	),

	TP_printk("old_state=%d next_state=%d",
		__entry->old_state, __entry->next_state)
);
#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
