/*
 *  Copyright (C) 2019 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems_debug

#if !defined(_TRACE_EMS_DEBUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_DEBUG_H

#include <linux/sched.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for multiple debugging
 */
TRACE_EVENT(ems_debug,

	TP_PROTO(struct task_struct *p, int cpu, int state, char *event),

	TP_ARGS(p, cpu, state, event),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	int,		state			)
		__array(	char,		event,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->state			= state;
		strncpy(__entry->event, event ? event : "no event", 63);
	),

	TP_printk("comm=%s pid=%d cpu=%d state=%d event=%s",
		  __entry->comm, __entry->pid, __entry->cpu, __entry->state, __entry->event)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_fit_cpus,

	TP_PROTO(struct task_struct *p, int wake,
		unsigned int fit_cpus, unsigned int cpus_allowed, unsigned int ontime_fit_cpus,
		unsigned int overcap_cpus, unsigned int busy_cpus),

	TP_ARGS(p, wake, fit_cpus, cpus_allowed, ontime_fit_cpus,
				overcap_cpus, busy_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	unsigned long,	task_util		)
		__field(	int,		hungry			)
		__field(	int,		wake			)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	ontime_fit_cpus		)
		__field(	unsigned int,	overcap_cpus		)
		__field(	unsigned int,	busy_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->src_cpu		= task_cpu(p);
		__entry->task_util		= p->se.ml.util_avg;
		__entry->hungry			= p->se.ml.hungry;
		__entry->wake			= wake;
		__entry->fit_cpus		= fit_cpus;
		__entry->cpus_allowed		= cpus_allowed;
		__entry->ontime_fit_cpus	= ontime_fit_cpus;
		__entry->overcap_cpus		= overcap_cpus;
		__entry->busy_cpus		= busy_cpus;
	),

	TP_printk("comm=%s pid=%d src_cpu=%d task_util=%lu hungry=%d wake=%d fit_cpus=%#x cpus_allowed=%#x ontime_fit_cpus=%#x overcap_cpus=%#x busy_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->src_cpu, __entry->task_util, __entry->hungry,
		  __entry->wake, __entry->fit_cpus, __entry->cpus_allowed, __entry->ontime_fit_cpus,
		  __entry->overcap_cpus, __entry->busy_cpus)
);

/*
 * Tracepoint for candidate cpu
 */
TRACE_EVENT(ems_candidates,

	TP_PROTO(struct task_struct *p, int sched_policy,
		unsigned int candidates, unsigned int idle_candidates),

	TP_ARGS(p, sched_policy, candidates, idle_candidates),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sched_policy		)
		__field(	unsigned int,	candidates		)
		__field(	unsigned int,	idle_candidates		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->sched_policy		= sched_policy;
		__entry->candidates		= candidates;
		__entry->idle_candidates	= idle_candidates;
	),

	TP_printk("comm=%s pid=%d sched_policy=%d candidates=%#x idle_candidates=%#x",
		  __entry->comm, __entry->pid, __entry->sched_policy,
		  __entry->candidates, __entry->idle_candidates)
);

TRACE_EVENT(ems_snapshot_cpu,

	TP_PROTO(int cpu, unsigned int task_util,
		unsigned int util_wo, unsigned int util_s_wo,
		unsigned int util_with, unsigned int rt_util,
		unsigned int util, unsigned int nr_running),

	TP_ARGS(cpu, task_util, util_wo, util_s_wo, util_with, rt_util,
		util, nr_running),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpu)
		__field(	unsigned int,	task_util)
		__field(	unsigned int,	util_wo)
		__field(	unsigned int,	util_s_wo)
		__field(	unsigned int,	util_with)
		__field(	unsigned int,	rt_util)
		__field(	unsigned int,	util)
		__field(	unsigned int,	nr_running)
	),

	TP_fast_assign(
		__entry->cpu       		= cpu;
		__entry->task_util		= task_util;
		__entry->util_wo   		= util_wo;
		__entry->util_s_wo 		= util_s_wo;
		__entry->util_with 		= util_with;
		__entry->rt_util   		= rt_util;
		__entry->util			= util;
		__entry->nr_running		= nr_running;
	),

	TP_printk("cpu%u tsk=%u, u_wo=%u u_s_wo=%u, u_with=%u, u_rt=%u util=%u nr=%u\n",
		  __entry->cpu, __entry->task_util,
		  __entry->util_wo, __entry->util_s_wo,
		  __entry->util_with, __entry->rt_util,
		  __entry->util, __entry->nr_running)
);

TRACE_EVENT(ems_energy_candidates,

	TP_PROTO(struct task_struct *p, unsigned int candidates),

	TP_ARGS(p, candidates),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->candidates		= candidates;
	),

	TP_printk("comm=%s pid=%d candidates=%#x",
		  __entry->comm, __entry->pid, __entry->candidates)
);

/*
 * Tracepoint for computing energy
 */
TRACE_EVENT(ems_compute_energy,

	TP_PROTO(struct task_struct *p, int tsk_util, int weight, int dst, int cpu,
		unsigned int util, unsigned int util_s, unsigned int cap, unsigned int cap_s,
		unsigned int power, unsigned int power_s, unsigned int energy),

	TP_ARGS(p, tsk_util, weight, dst, cpu, util, util_s, cap, cap_s, power, power_s, energy),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sse			)
		__field(	unsigned long,	tsk_util		)
		__field(	int,		weight			)
		__field(	int,		src			)
		__field(	int,		dst			)
		__field(	int,		cpu			)
		__field(	unsigned int,	util			)
		__field(	unsigned int,	util_s			)
		__field(	unsigned int,	cap			)
		__field(	unsigned int,	cap_s			)
		__field(	unsigned int,	power			)
		__field(	unsigned int,	power_s			)
		__field(	unsigned int,	energy			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->sse		= p->sse;
		__entry->tsk_util	= tsk_util;
		__entry->weight		= weight;
		__entry->src		= p->cpu;
		__entry->dst		= dst;
		__entry->cpu		= cpu;
		__entry->util		= util;
		__entry->util_s		= util_s;
		__entry->cap		= cap;
		__entry->cap_s		= cap_s;
		__entry->power		= power;
		__entry->power_s	= power_s;
		__entry->energy		= energy;
	),

	TP_printk("comm=%s pid=%d sse=%d tsk_ut=%lu(w %d) cpu%d->%d cpu%d ut=%u/%u cap=%u/%u pwr=%u/%u e=%u",
		__entry->comm, __entry->pid, __entry->sse, __entry->tsk_util, __entry->weight,
		__entry->src, __entry->dst,
		__entry->cpu,  __entry->util, __entry->util_s,  __entry->cap, __entry->cap_s,
		__entry->power, __entry->power_s, __entry->energy)
);


/*
 * Tracepoint for computing energy/capacity efficiency
 */
TRACE_EVENT(ems_compute_eff,

	TP_PROTO(struct task_struct *p, int cpu,
		unsigned long cpu_util_with, unsigned int eff_weight,
		unsigned long capacity, unsigned long energy, unsigned int eff),

	TP_ARGS(p, cpu, cpu_util_with, eff_weight, capacity, energy, eff),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sse			)
		__field(	int,		cpu			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	cpu_util_with		)
		__field(	unsigned int,	eff_weight		)
		__field(	unsigned long,	capacity		)
		__field(	unsigned long,	energy			)
		__field(	unsigned int,	eff			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->sse		= p->sse;
		__entry->cpu		= cpu;
		__entry->task_util	= p->se.ml.util_avg;
		__entry->cpu_util_with	= cpu_util_with;
		__entry->eff_weight	= eff_weight;
		__entry->capacity	= capacity;
		__entry->energy		= energy;
		__entry->eff		= eff;
	),

	TP_printk("comm=%s pid=%d sse=%d cpu=%d task_util=%lu cpu_util_with=%lu eff_weight=%u capacity=%lu energy=%lu eff=%u",
		__entry->comm, __entry->pid, __entry->sse, __entry->cpu,
		__entry->task_util, __entry->cpu_util_with,
		__entry->eff_weight, __entry->capacity,
		__entry->energy, __entry->eff)
);

/*
 * Tracepoint for computing performance
 */
TRACE_EVENT(ems_compute_perf,

	TP_PROTO(struct task_struct *p, int cpu,
		unsigned int capacity, int idle_state),

	TP_ARGS(p, cpu, capacity, idle_state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sse			)
		__field(	int,		cpu			)
		__field(	unsigned int,	capacity		)
		__field(	int,		idle_state		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->sse		= p->sse;
		__entry->cpu		= cpu;
		__entry->capacity	= capacity;
		__entry->idle_state	= idle_state;
	),

	TP_printk("comm=%s pid=%d sse=%d cpu=%d capacity=%u idle_state=%d",
		__entry->comm, __entry->pid, __entry->sse, __entry->cpu,
		__entry->capacity, __entry->idle_state)
);

/*
 * Tracepoint for system busy boost
 */
TRACE_EVENT(ems_sysbusy_boost,

       TP_PROTO(int boost),

       TP_ARGS(boost),

       TP_STRUCT__entry(
               __field(int,    boost)
       ),

       TP_fast_assign(
               __entry->boost  = boost;
       ),

       TP_printk("boost=%d", __entry->boost)
);

/*
 * Tracepoint for step_util in ESG
 */
TRACE_EVENT(esg_cpu_step_util,

	TP_PROTO(int cpu, int allowed_cap, int active_ratio, int max, int util),

	TP_ARGS(cpu, allowed_cap, active_ratio, max, util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		allowed_cap			)
		__field( int,		active_ratio			)
		__field( int,		max				)
		__field( int,		util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->allowed_cap		= allowed_cap;
		__entry->active_ratio		= active_ratio;
		__entry->max			= max;
		__entry->util			= util;
	),

	TP_printk("cpu=%d allowed_cap=%d active_ratio=%d, max=%d, util=%d",
			__entry->cpu, __entry->allowed_cap, __entry->active_ratio,
			__entry->max, __entry->util)
);

TRACE_EVENT(esg_migov_boost,

	TP_PROTO(int cpu, int boost, int util, int boosted_util),

	TP_ARGS(cpu, boost, util, boosted_util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		boost				)
		__field( int,		util				)
		__field( int,		boosted_util			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->boost			= boost;
		__entry->util			= util;
		__entry->boosted_util		= boosted_util;
	),

	TP_printk("cpu=%d boostp=%d util=%d, boosted_util=%d",
			__entry->cpu, __entry->boost, __entry->util,
			__entry->boosted_util)
);

/*
 * Tracepoint for util in ESG
 */
TRACE_EVENT(esg_cpu_util,

	TP_PROTO(int cpu, int nr_diff, int pelt_util_diff, int org_io_util, int io_util,
		int org_step_util, int step_util, int org_pelt_util, int pelt_util, int max, int util),

	TP_ARGS(cpu, nr_diff, pelt_util_diff, org_io_util, io_util,
		org_step_util, step_util, org_pelt_util, pelt_util, max, util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		nr_diff				)
		__field( int,		pelt_util_diff			)
		__field( int,		org_io_util			)
		__field( int,		io_util				)
		__field( int,		org_step_util			)
		__field( int,		step_util			)
		__field( int,		org_pelt_util			)
		__field( int,		pelt_util			)
		__field( int,		max				)
		__field( int,		util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->nr_diff		= nr_diff;
		__entry->pelt_util_diff		= pelt_util_diff;
		__entry->org_io_util		= org_io_util;
		__entry->io_util		= io_util;
		__entry->org_step_util		= org_step_util;
		__entry->step_util		= step_util;
		__entry->org_pelt_util		= org_pelt_util;
		__entry->pelt_util		= pelt_util;
		__entry->max			= max;
		__entry->util			= util;
	),

	TP_printk("cpu=%d nr_dif=%d, pelt_ut_dif=%d io_ut=%d->%d, step_ut=%d->%d pelt_ut=%d->%d, max=%d, ut=%d",
			__entry->cpu, __entry->nr_diff, __entry->pelt_util_diff,
			__entry->org_io_util, __entry->io_util,
			__entry->org_step_util, __entry->step_util,
			__entry->org_pelt_util, __entry->pelt_util,
			__entry->max, __entry->util)
);

/*
 * Tracepoint for available cpus in FRT
 */
TRACE_EVENT(frt_available_cpus,

	TP_PROTO(int cpu, int util_sum, int busy_thr, unsigned int available_mask),

	TP_ARGS(cpu, util_sum, busy_thr, available_mask),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__field(	int,		util_sum	)
		__field(	int,		busy_thr	)
		__field(	unsigned int,	available_mask	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util_sum	= util_sum;
		__entry->busy_thr	= busy_thr;
		__entry->available_mask	= available_mask;
	),

	TP_printk("cpu=%d util_sum=%d busy_thr=%d available_mask=%x",
		__entry->cpu,__entry->util_sum,
		__entry->busy_thr, __entry->available_mask)
);

/*
 * Tracepoint for tasks' estimated utilization.
 */
TRACE_EVENT(multi_load_util_est_task,

	TP_PROTO(struct task_struct *tsk, struct multi_load *ml),

	TP_ARGS(tsk, ml),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid			)
		__field( int,		cpu			)
		__field( int,		sse			)
		__field( unsigned int,	util_avg		)
		__field( unsigned int,	est_enqueued		)
		__field( unsigned int,	est_ewma		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->sse			= tsk->sse;
		__entry->util_avg		= ml->util_avg;
		__entry->est_enqueued		= ml->util_est.enqueued;
		__entry->est_ewma		= ml->util_est.ewma;
	),

	TP_printk("comm=%s pid=%d cpu=%d sse=%d util_avg=%u util_est_ewma=%u util_est_enqueued=%u",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->sse,
		  __entry->util_avg,
		  __entry->est_ewma,
		  __entry->est_enqueued)
);

/*
 * Tracepoint for root cfs_rq's estimated utilization.
 */
TRACE_EVENT(multi_load_util_est_cpu,

	TP_PROTO(int cpu, struct cfs_rq *cfs_rq),

	TP_ARGS(cpu, cfs_rq),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned int,	util_avg		)
		__field( unsigned int,	util_est_enqueued	)
		__field( unsigned int,	util_est_enqueued_s	)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util_avg		= cfs_rq->ml.util_avg;
		__entry->util_est_enqueued	= cfs_rq->ml.util_est.enqueued;
		__entry->util_est_enqueued_s	= cfs_rq->ml.util_est_s.enqueued;
	),

	TP_printk("cpu=%d util_avg=%u util_est_enqueued=%u util_est_enqueued_s=%u",
		  __entry->cpu,
		  __entry->util_avg,
		  __entry->util_est_enqueued,
		  __entry->util_est_enqueued_s)
);

/*
 * Tracepoint for active ratio tracking
 */
TRACE_EVENT(multi_load_update_cpu_active_ratio,

	TP_PROTO(int cpu, struct part *pa, char *event),

	TP_ARGS(cpu, pa, event),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( u64,	start				)
		__field( int,	recent				)
		__field( int,	last				)
		__field( int,	avg				)
		__field( int,	max				)
		__field( int,	est				)
		__array( char,	event,		64		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->start			= pa->period_start;
		__entry->recent			= pa->active_ratio_recent;
		__entry->last			= pa->hist[pa->hist_idx];
		__entry->avg			= pa->active_ratio_avg;
		__entry->max			= pa->active_ratio_max;
		__entry->est			= pa->active_ratio_est;
		strncpy(__entry->event, event, 63);
	),

	TP_printk("cpu=%d start=%llu recent=%4d last=%4d avg=%4d max=%4d est=%4d event=%s",
		__entry->cpu, __entry->start, __entry->recent, __entry->last,
		__entry->avg, __entry->max, __entry->est, __entry->event)
);

/*
 * Tracepoint for active ratio
 */
TRACE_EVENT(multi_load_cpu_active_ratio,

	TP_PROTO(int cpu, unsigned long part_util, unsigned long pelt_util),

	TP_ARGS(cpu, part_util, pelt_util),

	TP_STRUCT__entry(
		__field( int,		cpu					)
		__field( unsigned long,	part_util				)
		__field( unsigned long,	pelt_util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->part_util		= part_util;
		__entry->pelt_util		= pelt_util;
	),

	TP_printk("cpu=%d part_util=%lu pelt_util=%lu",
			__entry->cpu, __entry->part_util, __entry->pelt_util)
);

/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ontime_can_migrate_task,

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
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update,

	TP_PROTO(unsigned int heavy_cpus, unsigned int busy_cpus, unsigned int cpus),

	TP_ARGS(heavy_cpus, busy_cpus, cpus),

	TP_STRUCT__entry(
		__field(	unsigned int,	heavy_cpus		)
		__field(	unsigned int,	busy_cpus		)
		__field(	unsigned int,	cpus		)
	),

	TP_fast_assign(
		__entry->heavy_cpus		= heavy_cpus;
		__entry->busy_cpus		= busy_cpus;
		__entry->cpus			= cpus;
	),

	TP_printk("heavy_cpus=%#x busy_cpus=%#x cpus=%#x",
		__entry->heavy_cpus, __entry->busy_cpus, __entry->cpus)
);

/*
 * Tracepoint for frequency boost
 */
TRACE_EVENT(emstune_freq_boost,

	TP_PROTO(int cpu, int ratio, unsigned long util, unsigned long boosted_util),

	TP_ARGS(cpu, ratio, util, boosted_util),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( unsigned long,	boosted_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
		__entry->util			= util;
		__entry->boosted_util		= boosted_util;
	),

	TP_printk("cpu=%d ratio=%d util=%lu boosted_util=%lu",
		  __entry->cpu, __entry->ratio, __entry->util, __entry->boosted_util)
);

/*
 * Tracepint for PMU Contention AVG
 */
TRACE_EVENT(cont_crossed_thr,

	TP_PROTO(int cpu, u64 event_thr, u64 event_ratio, u64 active_thr, u64 active_ratio),

	TP_ARGS(cpu, event_thr, event_ratio, active_thr, active_ratio),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		event_thr		)
		__field(	u64,		event_ratio		)
		__field(	u64,		active_thr		)
		__field(	u64,		active_ratio		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->event_thr	= event_thr;
		__entry->event_ratio	= event_ratio;
		__entry->active_thr	= active_thr;
		__entry->active_ratio	= active_ratio;
	),

	TP_printk("cpu=%d  event_thr=%lu, event_ratio=%lu active_thr=%lu active_ratio=%lu",
		__entry->cpu, __entry->event_thr, __entry->event_ratio,
		__entry->active_thr, __entry->active_ratio)
);

TRACE_EVENT(cont_distribute_pmu_count,

	TP_PROTO(int cpu, u64 elapsed, u64 period_count,
			u64 curr0, u64 curr1,
			u64 prev0, u64 prev1,
			u64 diff0, u64 diff1, int event),

	TP_ARGS(cpu, elapsed, period_count,
		curr0, curr1, prev0, prev1, diff0, diff1, event),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		elapsed			)
		__field(	u64,		period_count		)
		__field(	u64,		curr0			)
		__field(	u64,		curr1			)
		__field(	u64,		prev0			)
		__field(	u64,		prev1			)
		__field(	u64,		diff0			)
		__field(	u64,		diff1			)
		__field(	int,		event			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->elapsed	= elapsed;
		__entry->period_count	= period_count;
		__entry->curr0		= curr0;
		__entry->curr1		= curr1;
		__entry->prev0		= prev0;
		__entry->prev1		= prev1;
		__entry->diff0		= diff0;
		__entry->diff1		= diff1;
		__entry->event		= event;
	),

	TP_printk("cpu=%d elapsed=%lu, pcnt=%lu, c0=%lu c1=%lu p0=%lu p1=%lu d0=%lu d1=%lu event=%d",
		__entry->cpu, __entry->elapsed, __entry->period_count,
		__entry->curr0, __entry->curr1,
		__entry->prev0, __entry->prev1,
		__entry->diff0, __entry->diff1, __entry->event)
);

/*
 * Tracepoint for updating load balance control
 */
TRACE_EVENT(group_smaller_max_cpu_capacity,

	TP_PROTO(int level, int src_cpu, int dst_cpu, int src_cap, int dst_cap, int extra_margin),

	TP_ARGS(level, src_cpu, dst_cpu, src_cap, dst_cap, extra_margin),

	TP_STRUCT__entry(
		__field(	int,	level		)
		__field(	int,	src_cpu		)
		__field(	int,	dst_cpu		)
		__field(	int,	src_cap		)
		__field(	int,	dst_cap		)
		__field(	int,	extra_margin)
	),

	TP_fast_assign(
		__entry->level			= level;
		__entry->src_cpu		= src_cpu;
		__entry->dst_cpu		= dst_cpu;
		__entry->src_cap		= src_cap;
		__entry->dst_cap		= dst_cap;
		__entry->extra_margin		= extra_margin;
	),

	TP_printk("lv=%d cpu%d(cap=%d+%d) -> cpu%d(cap=%d) ",
		__entry->level, __entry->src_cpu, __entry->src_cap, __entry->extra_margin,
		__entry->dst_cpu, __entry->dst_cap)
);

TRACE_EVENT(enabled_iss_margin,

	TP_PROTO(int flag),

	TP_ARGS(flag),

	TP_STRUCT__entry(
		__field(	int,	flag		)
	),

	TP_fast_assign(
		__entry->flag		= flag;
	),

	TP_printk("flag=%d", __entry->flag)
);

#endif /* _TRACE_EMS_DEBUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
