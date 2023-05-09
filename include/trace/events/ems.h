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

#include <linux/sched.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_task_rq,

	TP_PROTO(struct task_struct *p, int target_cpu, int wakeup, char *state),

	TP_ARGS(p, target_cpu, wakeup, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		target_cpu		)
		__field(	int,		wakeup			)
		__array(	char,		state,		30	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->target_cpu	= target_cpu;
		__entry->wakeup		= wakeup;
		memcpy(__entry->state, state, 30);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d wakeup=%d state=%s",
		  __entry->comm, __entry->pid, __entry->target_cpu,
		  __entry->wakeup, __entry->state)
);

TRACE_EVENT(ems_find_best_idle,

	TP_PROTO(struct task_struct *p, int task_util, unsigned int idle_candidates,
			unsigned int active_candidates, int bind, int best_cpu),

	TP_ARGS(p, task_util, idle_candidates, active_candidates, bind, best_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		task_util		)
		__field(	unsigned int,	idle_candidates		)
		__field(	unsigned int,	active_candidates	)
		__field(	int,		bind			)
		__field(	int,		best_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->task_util	= task_util;
		__entry->idle_candidates = idle_candidates;
		__entry->active_candidates = active_candidates;
		__entry->bind = bind;
		__entry->best_cpu = best_cpu;
	),

	TP_printk("comm=%s pid=%d tsk_util=%d idle=%#x active=%#x bind=%d best_cpu=%d",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->idle_candidates, __entry->active_candidates,
		__entry->bind, __entry->best_cpu)
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

	TP_PROTO(int cpu, int util, int freq),

	TP_ARGS(cpu, util, freq),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		util				)
		__field( int,		freq				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->freq			= freq;
	),

	TP_printk("cpu=%d util=%d, freq=%d",
		__entry->cpu, __entry->util, __entry->freq)
);

/*
 * Tracepoint for rq selection in FRT
 */
TRACE_EVENT(frt_select_task_rq,

	TP_PROTO(struct task_struct *tsk, struct sched_avg *avg, int best, char* str),

	TP_ARGS(tsk, avg, best, str),

	TP_STRUCT__entry(
		__array( char,	selectby,	TASK_COMM_LEN	)
		__array( char,	targettsk,	TASK_COMM_LEN	)
		__field( pid_t,	pid				)
		__field( int,	bestcpu				)
		__field( int,	prevcpu				)
		__field( unsigned long,	load_avg		)
		__field( unsigned long,	util_avg		)
	),

	TP_fast_assign(
		memcpy(__entry->selectby, str, TASK_COMM_LEN);
		memcpy(__entry->targettsk, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->bestcpu		= best;
		__entry->prevcpu		= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->util_avg		= avg->util_avg;
	),
	TP_printk("frt: comm=%s pid=%d assigned to #%d from #%d load_avg=%lu util_avg=%lu "
			"by %s.",
		  __entry->targettsk,
		  __entry->pid,
		  __entry->bestcpu,
		  __entry->prevcpu,
		  __entry->load_avg,
		  __entry->util_avg,
		  __entry->selectby)
);

/*
 * Tracepoint for accounting load averages for sched_entity of rt
 */
TRACE_EVENT(frt_load_rt_se,

	TP_PROTO(struct sched_rt_entity *se),

	TP_ARGS(se),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		cpu			)
		__field( unsigned long,	load_avg		)
		__field( unsigned long,	util_avg		)
		__field( u64,		load_sum		)
		__field( u32,		util_sum		)
		__field( u32,		period_contrib		)
	),

	TP_fast_assign(
		struct task_struct *tsk = container_of(se, struct task_struct, rt);
		struct sched_avg *avg = &se->avg;

		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->util_avg		= avg->util_avg;
		__entry->load_sum		= avg->load_sum;
		__entry->util_sum		= avg->util_sum;
		__entry->period_contrib		= avg->period_contrib;
	),
	TP_printk("rt: comm=%s pid=%d cpu=%d load_avg=%lu util_avg=%lu "
			"load_sum=%llu util_sum=%u period_contrib=%u",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->load_avg,
		  __entry->util_avg,
		  (u64)__entry->load_sum,
		  (u32)__entry->util_sum,
		  (u32)__entry->period_contrib)
);

/*
 * Tracepoint for accounting load averages for rq of rt
 */
TRACE_EVENT(frt_load_rt_rq,

	TP_PROTO(struct rt_rq *rt_rq),

	TP_ARGS(rt_rq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	load			)
		__field(	unsigned long,	rbl_load		)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
		__entry->cpu		= rt_rq->rq->cpu;
		__entry->load		= rt_rq->avg.load_avg;
		__entry->util		= rt_rq->avg.util_avg;
	),

	TP_printk("cpu=%d load=%lu util=%lu",
		  __entry->cpu, __entry->load, __entry->util)
);

/*
 * Tracepoint for accounting multi load for tasks
 */
DECLARE_EVENT_CLASS(multi_load_task,

	TP_PROTO(struct multi_load *ml),

	TP_ARGS(ml),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid				)
		__field( int,		cpu				)
		__field( int,		sse				)
		__field( unsigned long,	runnable			)
		__field( unsigned long,	util				)
		__field( unsigned long,	hungry				)
	),

	TP_fast_assign(
		struct sched_entity *se = container_of(ml, struct sched_entity, ml);
		struct task_struct *tsk = se->my_q ? NULL
					: container_of(se, struct task_struct, se);

		memcpy(__entry->comm, tsk ? tsk->comm : "(null)", TASK_COMM_LEN);
		__entry->pid			= tsk ? tsk->pid : -1;
		__entry->cpu			= tsk ? task_cpu(tsk) : cpu_of(se->my_q->rq);
		__entry->sse			= tsk ? tsk->sse : -1;
		__entry->runnable		= ml->runnable_avg;
		__entry->util			= ml->util_avg;
		__entry->hungry			= ml->hungry;
	),
	TP_printk("comm=%s pid=%d cpu=%d sse=%d runnable=%lu util=%lu hungry=%d",
		  __entry->comm, __entry->pid, __entry->cpu, __entry->sse,
					__entry->runnable, __entry->util, __entry->hungry)
);

DEFINE_EVENT(multi_load_task, multi_load_task,

	TP_PROTO(struct multi_load *ml),

	TP_ARGS(ml)
);

DEFINE_EVENT(multi_load_task, multi_load_new_task,

	TP_PROTO(struct multi_load *ml),

	TP_ARGS(ml)
);

/*
 * Tracepoint for accounting multi load for cpu.
 */
TRACE_EVENT(multi_load_cpu,

	TP_PROTO(struct multi_load *ml),

	TP_ARGS(ml),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	runnable			)
		__field( unsigned long,	runnable_s			)
		__field( unsigned long,	util				)
		__field( unsigned long,	util_s				)
	),

	TP_fast_assign(
		struct cfs_rq *cfs_rq = container_of(ml, struct cfs_rq, ml);

		__entry->cpu			= cpu_of(cfs_rq->rq);
		__entry->runnable		= ml->runnable_avg;
		__entry->runnable_s		= ml->runnable_avg_s;
		__entry->util			= ml->util_avg;
		__entry->util_s			= ml->util_avg_s;
	),
	TP_printk("cpu=%d runnable=%lu runnable_s=%lu util=%lu util_s=%lu",
		  __entry->cpu, __entry->runnable, __entry->runnable_s,
					__entry->util, __entry->util_s)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
				int src_cpu, int dst_cpu),

	TP_ARGS(p, load, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sse			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->sse		= p->sse;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
	),

	TP_printk("comm=%s pid=%d sse=%d ontime_load_avg=%lu src_cpu=%d dst_cpu=%d",
		__entry->comm, __entry->pid, __entry->sse, __entry->load,
		__entry->src_cpu, __entry->dst_cpu)
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
 * Tracepoint for global boost
 */
TRACE_EVENT(ems_global_boost,

	TP_PROTO(char *name, int boost),

	TP_ARGS(name, boost),

	TP_STRUCT__entry(
		__array(	char,	name,	64	)
		__field(	int,	boost		)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 64);
		__entry->boost		= boost;
	),

	TP_printk("name=%s global_boost=%d", __entry->name, __entry->boost)
);

/*
 * Trace for overutilization flag of LBT
 */
TRACE_EVENT(ems_lbt_overutilized,

	TP_PROTO(int cpu, int level, unsigned long util, unsigned long capacity, bool overutilized),

	TP_ARGS(cpu, level, util, capacity, overutilized),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		level			)
		__field( unsigned long,	util			)
		__field( unsigned long,	capacity		)
		__field( bool,		overutilized		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->level			= level;
		__entry->util			= util;
		__entry->capacity		= capacity;
		__entry->overutilized		= overutilized;
	),

	TP_printk("cpu=%d level=%d util=%lu capacity=%lu overutilized=%d",
		__entry->cpu, __entry->level, __entry->util,
		__entry->capacity, __entry->overutilized)
);

/*
 * Tracepoint for overutilzed status for lb dst_cpu's sibling
 */
TRACE_EVENT(ems_lb_sibling_overutilized,

	TP_PROTO(int dst_cpu, int level, unsigned long ou),

	TP_ARGS(dst_cpu, level, ou),

	TP_STRUCT__entry(
		__field( int,		dst_cpu			)
		__field( int,		level			)
		__field( unsigned long,	ou			)
	),

	TP_fast_assign(
		__entry->dst_cpu		= dst_cpu;
		__entry->level			= level;
		__entry->ou			= ou;
	),

	TP_printk("dst_cpu=%d level=%d ou=%lu",
				__entry->dst_cpu, __entry->level, __entry->ou)
);

/*
 * Tracepoint for active ratio pattern
 */
TRACE_EVENT(ems_cpu_active_ratio_patten,

	TP_PROTO(int cpu, int p_count, int p_avg, int p_stdev),

	TP_ARGS(cpu, p_count, p_avg, p_stdev),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( int,	p_count				)
		__field( int,	p_avg				)
		__field( int,	p_stdev				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->p_count		= p_count;
		__entry->p_avg			= p_avg;
		__entry->p_stdev		= p_stdev;
	),

	TP_printk("cpu=%d p_count=%2d p_avg=%4d p_stdev=%4d",
		__entry->cpu, __entry->p_count, __entry->p_avg, __entry->p_stdev)
);

/*
 * Tracepint for PMU Contention AVG
 */
TRACE_EVENT(cont_found_attacker,

	TP_PROTO(int cpu, struct task_struct *p),

	TP_ARGS(cpu, p),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		),

	TP_fast_assign(
		__entry->cpu = cpu;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		),

	TP_printk("cp%d comm=%s pid=%d",
		__entry->cpu, __entry->comm, __entry->pid)
);

TRACE_EVENT(cont_found_victim,

	TP_PROTO(int victim_found, u64 victim_found_time),

	TP_ARGS(victim_found, victim_found_time),

	TP_STRUCT__entry(
		__field(	int,		victim_found		)
		__field(	u64,		victim_found_time	)
		),

	TP_fast_assign(
		__entry->victim_found = victim_found;
		__entry->victim_found_time = victim_found_time;
		),

	TP_printk("victim_found=%d time=%lu",
		__entry->victim_found, __entry->victim_found_time)
   );

TRACE_EVENT(cont_set_period_start,

	TP_PROTO(int cpu, u64 last_updated, u64 period_start, u32 hist_idx, u64 prev0, u64 prev1),

	TP_ARGS(cpu, last_updated, period_start, hist_idx, prev0, prev1),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		last_updated		)
		__field(	u64,		period_start		)
		__field(	u32,		hist_idx		)
		__field(	u64,		prev0			)
		__field(	u64,		prev1			)
		),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->last_updated	= last_updated;
		__entry->period_start	= period_start;
		__entry->hist_idx	= hist_idx;
		__entry->prev0		= prev0;
		__entry->prev1		= prev1;
		),

	TP_printk("cpu=%d last_updated=%lu, period_start=%lu, hidx=%u prev0=%lu prev1=%lu",
			__entry->cpu, __entry->last_updated, __entry->period_start,
			__entry->hist_idx, __entry->prev0, __entry->prev1)
);

TRACE_EVENT(ww_init,

	TP_PROTO(int sch, unsigned int mask),

	TP_ARGS(sch, mask),

	TP_STRUCT__entry(
		__field( int,		sch				)
		__field( int,		mask				)
	),

	TP_fast_assign(
		__entry->sch			= sch;
		__entry->mask			= mask;
	),

	TP_printk("sch=%d, mask=%x",
		__entry->sch, __entry->mask)
);

#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
