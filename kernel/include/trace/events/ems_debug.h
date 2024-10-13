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

#include <linux/tracepoint.h>

struct mlt_cpu;
struct mlt_runnable;
struct mlt_uarch;
struct mlt_part;
struct mlt_task;
struct rq;
struct tp_env;
struct cpu_profile;

#define LABEL_LEN 32

/*
 * Tracepoint for multiple test
 */
TRACE_EVENT(ems_test,

	TP_PROTO(struct task_struct *p, int cpu, u64 arg1, u64 arg2, char *event),

	TP_ARGS(p, cpu, arg1, arg2, event),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	u64,		arg1			)
		__field(	u64,		arg2			)
		__array(	char,		event,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->arg1			= arg1;
		__entry->arg2			= arg2;
		strncpy(__entry->event, event ? event : "no event", LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, cpu=%1d, arg1=%20llu, arg2=%20llu, event=%s",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->arg1, __entry->arg2, __entry->event)
);

/*
 * Tracepoint for taking util snapshot
 */
TRACE_EVENT(ems_take_util_snapshot,

	TP_PROTO(int cpu, struct tp_env *env),

	TP_ARGS(cpu, env),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	task_load_avg		)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	util_wo			)
		__field(	unsigned long,	util_with		)
		__field(	int,		runnable		)
		__field(	unsigned long,	rt_util			)
		__field(	unsigned long,	dl_util			)
		__field(	unsigned int,	nr_running		)
		__field(	int,		idle			)
		__field(	int,		table_index			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->cpu       		= cpu;
		__entry->task_util		= env->task_util;
		__entry->task_load_avg		= env->task_load_avg;
		__entry->util			= env->cpu_stat[cpu].util;
		__entry->util_wo   		= env->cpu_stat[cpu].util_wo;
		__entry->util_with 		= env->cpu_stat[cpu].util_with;
		__entry->runnable 		= env->cpu_stat[cpu].runnable;
		__entry->rt_util   		= env->cpu_stat[cpu].rt_util;
		__entry->dl_util   		= env->cpu_stat[cpu].dl_util;
		__entry->nr_running		= env->cpu_stat[cpu].nr_running;
		__entry->idle			= env->cpu_stat[cpu].idle;
		__entry->table_index		= env->table_index;
	),

	TP_printk("comm=%15s, pid=%6d, cpu=%1d, "
		"task_util=%4lu, task_load_avg=%4lu, "
		"util=%4lu, util_wo=%4lu, util_with=%4lu, runnable=%5d, "
		"rt_util=%4lu, dl_util=%4lu, nr_running=%2u, idle=%1d, tbl_idx=%2d",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->task_util,
		__entry->task_load_avg,
		__entry->util, __entry->util_wo, __entry->util_with,
		__entry->runnable, __entry->rt_util, __entry->dl_util,
		__entry->nr_running, __entry->idle,
		__entry->table_index)
);

/*
 * Tracepoint for setting fit_cpus by prio pinning
 */
TRACE_EVENT(tex_boosted_fit_cpus,

	TP_PROTO(struct task_struct *p, int sync, struct cpumask *fit_cpus),

	TP_ARGS(p, sync, fit_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prev_cpu		)
		__field(	int,		sync			)
		__field(	unsigned int,	fit_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->prev_cpu		= task_cpu(p);
		__entry->sync			= sync;
		__entry->fit_cpus		= *(unsigned int *)cpumask_bits(fit_cpus);
	),

	TP_printk("comm=%15s, pid=%6d, prev_cpu=%1d, sync=%1d, fit_cpus=0x%03x",
		__entry->comm, __entry->pid, __entry->prev_cpu, __entry->sync,  __entry->fit_cpus)
);

/*
 * Tracepoint for prio pinning schedule
 */
TRACE_EVENT(tex_pinning_schedule,

	TP_PROTO(int target_cpu, struct cpumask *candidates),

	TP_ARGS(target_cpu, candidates),

	TP_STRUCT__entry(
		__field(	int,		target_cpu		)
		__field(	unsigned int,	candidates		)
	),

	TP_fast_assign(
		__entry->target_cpu		= target_cpu;
		__entry->candidates		= *(unsigned int *)cpumask_bits(candidates);
	),

	TP_printk("target_cpu=%1d, candidates=0x%03x",
		__entry->target_cpu, __entry->candidates)
);

/*
 * Tracepoint for tex update stats
 */
TRACE_EVENT(tex_update_stats,

	TP_PROTO(struct task_struct *p, char *label),

	TP_ARGS(p, label),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		tex_lv			)
		__field(	int,		prio			)
		__field(	u64,		tex_runtime		)
		__field(	u64,		tex_chances		)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->tex_lv			= get_tex_level(p);
		__entry->prio			= p->prio;
		__entry->tex_runtime		= ems_tex_runtime(p);
		__entry->tex_chances		= ems_tex_chances(p);
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, tex_lv=%1d, prio=%3d, tex_runtime=%llu, "
		"tex_chances=%llu, reason=%s",
		__entry->comm, __entry->pid, __entry->tex_lv, __entry->prio,  __entry->tex_runtime,
		__entry->tex_chances, __entry->label)
);

/*
 * Template for tex task
 */
DECLARE_EVENT_CLASS(tex_task_trace_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		tex_lv			)
		__field(	int,		prio			)
		__field(	u64,		tex_runtime		)
		__field(	u64,		tex_chances		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->tex_lv			= get_tex_level(p);
		__entry->prio			= p->prio;
		__entry->tex_runtime		= ems_tex_runtime(p);
		__entry->tex_chances		= ems_tex_chances(p);
	),

	TP_printk("comm=%15s, pid=%6d, tex_lv=%d, prio=%d, tex_runtime=%llu, tex_chances=%llu",
		__entry->comm, __entry->pid, __entry->tex_lv,
		__entry->prio,  __entry->tex_runtime, __entry->tex_chances)
);

/* Tracepoint for task picked by queue jump */
DEFINE_EVENT(tex_task_trace_template, tex_qjump_pick_next_task,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/* Tracepoint for insert tex task to qjump list */
DEFINE_EVENT(tex_task_trace_template, tex_insert_to_qjump_list,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/* Tracepoint for remove tex task from qjump list*/
DEFINE_EVENT(tex_task_trace_template, tex_remove_from_qjump_list,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/*
 * Tracepoint for check preempt
 */
TRACE_EVENT(tex_check_preempt_wakeup,

	TP_PROTO(struct task_struct *curr, struct task_struct *p, bool preempt, bool ignore),

	TP_ARGS(curr, p, preempt, ignore),

	TP_STRUCT__entry(
		__array(	char,		curr_comm,	TASK_COMM_LEN	)
		__field(	int,		curr_tex			)
		__array(	char,		p_comm,		TASK_COMM_LEN	)
		__field(	int,		p_tex				)
		__field(	bool,		preempt				)
		__field(	bool,		ignore				)
	),

	TP_fast_assign(
		memcpy(__entry->curr_comm, curr->comm, TASK_COMM_LEN);
		__entry->curr_tex		= get_tex_level(curr);
		memcpy(__entry->p_comm, p->comm, TASK_COMM_LEN);
		__entry->p_tex			= get_tex_level(p);
		__entry->preempt		= preempt;
		__entry->ignore			= ignore;
	),

	TP_printk("curr_comm=%15s, curr_tex=%d, p_comm=%15s, p_tex=%d, preempt=%d, ignore=%d",
		__entry->curr_comm, __entry->curr_tex, __entry->p_comm, __entry->p_tex,
		__entry->preempt, __entry->ignore)
);

/*
 * Tracepoint for finding best perf cpu
 */
TRACE_EVENT(ems_find_best_perf_cpu,

	TP_PROTO(struct tp_env *env, int best_cpu),

	TP_ARGS(env, best_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		task_util		)
		__field(	int,		best_cpu		)
		__field(	int,		idle			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->task_util		= env->task_util;
		__entry->best_cpu		= best_cpu;
		__entry->idle			= best_cpu < 0 ? -1 : env->cpu_stat[best_cpu].idle;
	),

	TP_printk("comm=%15s, pid=%6d, tsk_util=%4d, best_cpu=%2d, idle=%1d",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->best_cpu, __entry->idle)
);

TRACE_EVENT(ems_find_best_perf_cpu_info,

	TP_PROTO(int cpu, unsigned long cap, int idle, unsigned long util, unsigned long spare,
		int nr_prio_tex_tasks, int nr_launch, int p_tex_lvl, int cur_tex_lvl),

	TP_ARGS(cpu, cap, idle, util, spare, nr_prio_tex_tasks, nr_launch, p_tex_lvl, cur_tex_lvl),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__field(	unsigned long,	cap		)
		__field(	int,		idle		)
		__field(	unsigned long,	util		)
		__field(	unsigned long,	spare		)
		__field(	int,		nr_prio_tex_tasks	)
		__field(	int,		nr_launch	)
		__field(	int,		p_tex_lvl	)
		__field(	int,		cur_tex_lvl	)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cap			= cap;
		__entry->idle			= idle;
		__entry->util			= util;
		__entry->spare			= spare;
		__entry->nr_prio_tex_tasks	= nr_prio_tex_tasks;
		__entry->nr_launch		= nr_launch;
		__entry->p_tex_lvl		= p_tex_lvl;
		__entry->cur_tex_lvl		= cur_tex_lvl;
	),

	TP_printk("cpu=%1d, cap=%4lu, idle=%1d, util=%4lu, spare=%4lu, "
		"nr_prio_tex_tasks=%2d, nr_launch=%2d, p_tex_lvl=%1d, cur_tex_lvl=%1d",
		__entry->cpu, __entry->cap, __entry->idle, __entry->util,
		__entry->spare, __entry->nr_prio_tex_tasks, __entry->nr_launch,
		__entry->p_tex_lvl, __entry->cur_tex_lvl)
);

/*
 * Tracepoint for finding lowest energy cpu
 */
TRACE_EVENT(ems_find_energy_cpu,

	TP_PROTO(struct task_struct *p, struct cpumask *candidates,
		int energy_cpu, int adv_energy_cpu),

	TP_ARGS(p, candidates, energy_cpu, adv_energy_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
		__field(	int,		energy_cpu		)
		__field(	int,		adv_energy_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->candidates		= *(unsigned int *)cpumask_bits(candidates);
		__entry->energy_cpu		= energy_cpu;
		__entry->adv_energy_cpu		= adv_energy_cpu;
	),

	TP_printk("comm=%15s, pid=%6d, candidates=0x%03x, energy_cpu=%2d, adv_energy_cpu=%2d",
		__entry->comm, __entry->pid, __entry->candidates,
		__entry->energy_cpu, __entry->adv_energy_cpu)
);

/*
 * Tracepoint for computing energy
 */
TRACE_EVENT(ems_compute_energy,

	TP_PROTO(int cpu, unsigned long util, unsigned long cap,
		unsigned long f, unsigned long v, unsigned long dp,
		unsigned long energy),

	TP_ARGS(cpu, util, cap, f, v, dp, energy),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	cap			)
		__field(	unsigned long,	f			)
		__field(	unsigned long,	v			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->cap			= cap;
		__entry->f			= f;
		__entry->v			= v;
		__entry->dp			= dp;
		__entry->energy			= energy;
	),

	TP_printk("cpu=%1d, util=%4lu, cap=%4lu, f=%7lu, v=%4lu, "
		"dp=%4lu, energy=%7lu",
		__entry->cpu, __entry->util, __entry->cap, __entry->f, __entry->v,
		__entry->dp,  __entry->energy)
);

/*
 * Tracepoint for computing DSU energy
 */
TRACE_EVENT(ems_compute_dsu_energy,

	TP_PROTO(unsigned long freq, unsigned long volt,
		unsigned long dp, unsigned long energy),

	TP_ARGS(freq, volt, dp, energy),

	TP_STRUCT__entry(
		__field(	unsigned long,	freq			)
		__field(	unsigned long,	volt			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->freq			= freq;
		__entry->volt			= volt;
		__entry->dp			= dp;
		__entry->energy			= energy;
	),

	TP_printk("freq=%7lu, volt=%4lu, dp=%4lu, energy=%7lu",
		__entry->freq, __entry->volt, __entry->dp, __entry->energy)
);

/*
 * Tracepoint for computing system energy
 */
TRACE_EVENT(ems_compute_system_energy,

	TP_PROTO(struct task_struct *p, const struct cpumask *candidates,
			int target_cpu, unsigned long energy),

	TP_ARGS(p, candidates, target_cpu, energy),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
		__field(	int,		target_cpu		)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->candidates		= *(unsigned int *)cpumask_bits(candidates);
		__entry->target_cpu		= target_cpu;
		__entry->energy			= energy;
	),

	TP_printk("comm=%15s, pid=%6d, candidates=0x%03x, target_cpu=%1d, energy=%7lu",
		__entry->comm, __entry->pid, __entry->candidates,
		__entry->target_cpu, __entry->energy)
);

/*
 * Tracepoint for core selection by sysbusy schedule
 */
TRACE_EVENT(sysbusy_schedule,

	TP_PROTO(struct task_struct *p, int state, int target_cpu),

	TP_ARGS(p, state, target_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	util			)
		__field(	int,		state			)
		__field(	int,		target_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->util			= p->se.avg.util_avg;
		__entry->state			= state;
		__entry->target_cpu		= target_cpu;
	),

	TP_printk("comm=%15s, pid=%6d, util=%4ld, state=%1d, target_cpu=%1d",
		__entry->comm, __entry->pid, __entry->util,
		__entry->state, __entry->target_cpu)
);

/*
 * Tracepoint for monitoring sysbusy state
 */
TRACE_EVENT(ems_cpu_profile,

	TP_PROTO(int cpu, struct cpu_profile *cs, int nr_running),

	TP_ARGS(cpu, cs, nr_running),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		cpu_util		)
		__field(	int,		cpu_util_busy		)
		__field(	u64,		hratio			)
		__field(	int,		pid			)
		__field(	int,		nr_running		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cpu_util		= cs[CPU_UTIL].value;
		__entry->cpu_util_busy		= cs[CPU_UTIL].data;
		__entry->hratio			= cs[CPU_HTSK].value;
		__entry->pid			= cs[CPU_HTSK].data;
		__entry->nr_running		= nr_running;
	),

	TP_printk("cpu=%1d, cpu_util=%4llu(busy=%1d), "
		"hratio=%4llu(pid=%6d), nr_running=%2d",
		__entry->cpu, __entry->cpu_util, __entry->cpu_util_busy,
		__entry->hratio, __entry->pid, __entry->nr_running)
);


/*
 * Tracepoint for tracking tasks in system
 */
TRACE_EVENT(ems_profile_tasks,

	TP_PROTO(int busy_cpu_count, unsigned long cpu_util_sum,
		int heavy_task_count, unsigned long heavy_task_util_sum,
		int avg_nr_run_sum),

	TP_ARGS(busy_cpu_count, cpu_util_sum, heavy_task_count, heavy_task_util_sum,
		avg_nr_run_sum),

	TP_STRUCT__entry(
		__field(	int,		busy_cpu_count		)
		__field(	unsigned long,	cpu_util_sum		)
		__field(	int,		heavy_task_count	)
		__field(	unsigned long,	heavy_task_util_sum	)
		__field(	int,	avg_nr_run_sum)
	),

	TP_fast_assign(
		__entry->busy_cpu_count		= busy_cpu_count;
		__entry->cpu_util_sum		= cpu_util_sum;
		__entry->heavy_task_count	= heavy_task_count;
		__entry->heavy_task_util_sum	= heavy_task_util_sum;
		__entry->avg_nr_run_sum		= avg_nr_run_sum;
	),

	TP_printk("busy_cpu_count=%1d, cpu_util_sum=%4lu, "
		"heavy_task_count=%1d, heavy_task_util_sum=%4lu, "
		"avg_nr_run_su=%d",
		__entry->busy_cpu_count, __entry->cpu_util_sum,
		__entry->heavy_task_count, __entry->heavy_task_util_sum,
		__entry->avg_nr_run_sum)
);

/*
 * Tracepoint for somac scheduling
 */
TRACE_EVENT(sysbusy_somac,

	TP_PROTO(struct task_struct *p, unsigned long task_util,
		int src_cpu, int dst_cpu, unsigned long time),

	TP_ARGS(p, task_util, src_cpu, dst_cpu, time),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	task_util		)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__field(	unsigned long,	time			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->task_util		= task_util;
		__entry->src_cpu		= src_cpu;
		__entry->dst_cpu		= dst_cpu;
		__entry->time			= time;
	),

	TP_printk("comm=%15s, pid=%6d, task_util=%4lu, src_cpu=%1d, dst_cpu=%1d, time=%10lu",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->src_cpu, __entry->dst_cpu, __entry->time)
);

/*
 * Tracepoint for move somac task
 */
TRACE_EVENT(sysbusy_mv_somac_task,

	TP_PROTO(int src_cpu, char *label),

	TP_ARGS(src_cpu, label),

	TP_STRUCT__entry(
		__field(	int,		src_cpu			)
		__field(	int,		cfs_nr			)
		__field(	int,		cfs_h_nr		)
		__field(	int,		active_balance		)
		__field(	u64,		somac_deq		)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		__entry->src_cpu		= src_cpu;
		__entry->cfs_nr			= cpu_rq(src_cpu)->cfs.nr_running;
		__entry->cfs_h_nr		= cpu_rq(src_cpu)->cfs.h_nr_running;
		__entry->active_balance		= cpu_rq(src_cpu)->active_balance;
		__entry->somac_deq		= ems_somac_dequeued(cpu_rq(src_cpu));
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("src_cpu=%1d, cfs_nr=%2d, cfs_h_nr=%2d, active_balance=%d, "
		"somac_deq=%10llu, reason=%30s",
		__entry->src_cpu, __entry->cfs_nr, __entry->cfs_h_nr, __entry->active_balance,
		__entry->somac_deq, __entry->label)
);

/*
 * Tracepoint for somac label
 */
TRACE_EVENT(sysbusy_somac_reason,

	TP_PROTO(int src_cpu, int dst_cpu, char *label),

	TP_ARGS(src_cpu, dst_cpu, label),

	TP_STRUCT__entry(
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		__entry->src_cpu		= src_cpu;
		__entry->dst_cpu		= dst_cpu;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("src_cpu=%1d, dst_cpu=%1d, reason=%14s",
		__entry->src_cpu, __entry->dst_cpu, __entry->label)
);

/*
 * Tracepoint for determine_sysbusy_state
 */
TRACE_EVENT(sysbusy_determine_state,

	TP_PROTO(int old_state, int next_state, int busy_cpu_cnt,
		unsigned long heavy_task_util_sum, unsigned long cpu_util_sum,
		int avg_nr_run_sum, int heavy_task_cnt),

	TP_ARGS(old_state, next_state, busy_cpu_cnt, heavy_task_util_sum,
		cpu_util_sum, avg_nr_run_sum, heavy_task_cnt),

	TP_STRUCT__entry(
		__field(	int,		old_state		)
		__field(	int,		next_state		)
		__field(	int,		busy_cpu_cnt		)
		__field(	unsigned long,	heavy_task_util_sum	)
		__field(	unsigned long,	cpu_util_sum		)
		__field(	int,		avg_nr_run_sum		)
		__field(	int,		heavy_task_cnt		)
	),

	TP_fast_assign(
		__entry->old_state		= old_state;
		__entry->next_state		= next_state;
		__entry->busy_cpu_cnt		= busy_cpu_cnt;
		__entry->heavy_task_util_sum	= heavy_task_util_sum;
		__entry->cpu_util_sum		= cpu_util_sum;
		__entry->avg_nr_run_sum		= avg_nr_run_sum;
		__entry->heavy_task_cnt		= heavy_task_cnt;
	),

	TP_printk("old_state=%d, next_state=%d busy_cpu_cnt=%d heavy_task_util_sum=%lu "
			"cpu_util_sum=%lu avg_nr_run_sum=%d heavy_task_cnt=%d",
		__entry->old_state, __entry->next_state, __entry->busy_cpu_cnt,
		__entry->heavy_task_util_sum, __entry->cpu_util_sum, __entry->avg_nr_run_sum,
		__entry->heavy_task_cnt)
);

/*
 * Tracepoint for EGO
 */
TRACE_EVENT(ego_sched_util,

	TP_PROTO(int cpu, unsigned long util, unsigned long cfs, unsigned long rt,
		unsigned long dl, unsigned long bwdl, unsigned long irq),

	TP_ARGS(cpu, util, cfs, rt, dl, bwdl, irq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	cfs			)
		__field(	unsigned long,	rt			)
		__field(	unsigned long,	dl			)
		__field(	unsigned long,	bwdl			)
		__field(	unsigned long,	irq			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->cfs			= cfs;
		__entry->rt			= rt;
		__entry->dl			= dl;
		__entry->bwdl			= bwdl;
		__entry->irq			= irq;
	),

	TP_printk("cpu=%1d, util=%4ld, cfs=%4ld, rt=%4ld, dl=%4ld, dlbw=%4ld, irq=%4ld",
		__entry->cpu, __entry->util, __entry->cfs, __entry->rt,
		__entry->dl, __entry->bwdl, __entry->irq)
);

TRACE_EVENT(ego_cpu_util,

	TP_PROTO(int cpu, unsigned long pelt_boost, unsigned long util,
		unsigned long io_util, unsigned long bst_util),

	TP_ARGS(cpu, pelt_boost, util, io_util, bst_util),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	pelt_boost		)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	io_util			)
		__field(	unsigned long,	bst_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pelt_boost		= pelt_boost;
		__entry->util			= util;
		__entry->io_util		= io_util;
		__entry->bst_util		= bst_util;
	),

	TP_printk("cpu=%1d, pelt_boost=%3ld, util=%4ld, io_util=%4ld, bst_util=%4ld",
		__entry->cpu, __entry->pelt_boost, __entry->util, __entry->io_util,
		__entry->bst_util)
);

/*
 * Tracepoint for slack timer
 */
TRACE_EVENT(slack_timer,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d slack timer expired", __entry->cpu)
);

/*
 * Tracepoint for checking need for slack timer
 */
TRACE_EVENT(ego_need_slack_timer,

	TP_PROTO(int cpu, unsigned long boosted_util, unsigned long min_cap,
		int heaviest_cpu, unsigned int cur_freq, unsigned int min_freq,
		unsigned int energy_freq, unsigned int orig_freq, int need),

	TP_ARGS(cpu, boosted_util, min_cap, heaviest_cpu, cur_freq,
		min_freq, energy_freq, orig_freq, need),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	boosted_util		)
		__field(	unsigned long,	min_cap			)
		__field(	int,		heaviest_cpu		)
		__field(	unsigned int,	cur_freq		)
		__field(	unsigned int,	min_freq		)
		__field(	unsigned int,	energy_freq		)
		__field(	unsigned int,	orig_freq		)
		__field(	int,		need			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->boosted_util		= boosted_util;
		__entry->min_cap		= min_cap;
		__entry->heaviest_cpu		= heaviest_cpu;
		__entry->cur_freq		= cur_freq;
		__entry->min_freq		= min_freq;
		__entry->energy_freq		= energy_freq;
		__entry->orig_freq		= orig_freq;
		__entry->need			= need;
	),

	TP_printk("cpu=%1d, boosted_util=%4lu, min_cap=%4lu, heaviest_cpu=%1d, "
		"cur_freq=%7u, min_freq=%7u, energy_freq=%7u, orig_freq=%7u, need=%1d",
		__entry->cpu, __entry->boosted_util, __entry->min_cap,
		__entry->heaviest_cpu, __entry->cur_freq, __entry->min_freq,
		__entry->energy_freq, __entry->orig_freq, __entry->need)
);

/*
 * Tracepoint for available cpus in FRT
 */

TRACE_EVENT(ems_frt_lowest_rq_stat,

	TP_PROTO(struct task_struct *p, int cpu, unsigned long irq_load,
		 int tex_lvl, int idle, unsigned int exit_lat),

	TP_ARGS(p, cpu, irq_load, tex_lvl, idle, exit_lat),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	unsigned long,	irq_load		)
		__field(	int,		tex_lvl			)
		__field(	unsigned long,	cpu_util		)
		__field(	unsigned int,	exit_lat		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->irq_load		= irq_load;
		__entry->tex_lvl		= tex_lvl;
		__entry->cpu_util		= idle ? 0 : ml_cpu_util(cpu);
		__entry->exit_lat		= exit_lat;
	),

	TP_printk("comm=%15s, pid=%6d, cpu=%1d, irq_load=%4lu, "
		  "tex_lvl=%1d, cpu_util=%4lu, exit_lat=%4d",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->irq_load, __entry->tex_lvl, __entry->cpu_util,
		  __entry->exit_lat)
);

TRACE_EVENT(ems_frt_lowest_rq,

	TP_PROTO(struct task_struct *p, int index, int best_cpu, unsigned int best_lat,
		 unsigned long best_cpu_util, unsigned int best_tex_stat),

	TP_ARGS(p, index, best_cpu, best_lat, best_cpu_util, best_tex_stat),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		index			)
		__field(	int,		best_cpu		)
		__field(	unsigned int,	best_lat		)
		__field(	unsigned long,	best_cpu_util		)
		__field(	unsigned int,	best_tex_stat		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->index			= index;
		__entry->best_cpu		= best_cpu;
		__entry->best_lat		= best_lat;
		__entry->best_cpu_util		= best_cpu_util;
		__entry->best_tex_stat		= best_tex_stat;
	),

	TP_printk("comm=%15s, pid=%6d, init_index=%1d, best_cpu=%1d "
		  "best_lat=%4d, best_cpu_util=%4lu, best_tex_stat=%1d",
		  __entry->comm, __entry->pid, __entry->index, __entry->best_cpu,
		  __entry->best_lat, __entry->best_cpu_util, __entry->best_tex_stat)
);

/*
 * Tracepoint for multi load tracking for cstate
 */
TRACE_EVENT(mlt_update_cpu_part,

	TP_PROTO(int cpu, struct mlt_part *part),

	TP_ARGS(cpu, part),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		cur_period		)
		__field(	int,		state			)
		__field(	u64,		contrib			)
		__field(	int,		recent			)
		__field(	int,		last			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cur_period		= part->cur_period;
		__entry->state			= part->state;
		__entry->contrib		= part->contrib;
		__entry->recent			= part->recent;
		__entry->last			= part->periods[part->cur_period];
	),

	TP_printk("cpu=%1d cur_period=%1d state=%2d contrib=%8llu recent=%4d last=%4d",
		__entry->cpu, __entry->cur_period, __entry->state,
		__entry->contrib, __entry->recent, __entry->last)
);

TRACE_EVENT(mlt_update_cpu_uarch,

	TP_PROTO(int cpu, struct mlt_part *part, struct mlt_uarch *uarch),

	TP_ARGS(cpu, part, uarch),

	TP_STRUCT__entry(
		__field(	int,	cpu				)
		__array(	u32,	recent,	MLT_UARCH_CPU_NUM	)
		__array(	u32,	last,	MLT_UARCH_CPU_NUM	)
	),

	TP_fast_assign(
		int i;
		__entry->cpu			= cpu;
		for (i = 0; i < MLT_UARCH_CPU_NUM; i++) {
			__entry->last[i]	= uarch[i].periods[part->cur_period];
			__entry->recent[i]	= uarch[i].recent;
		}
	),

	TP_printk("cpu=%1d "
		"ipc_lst=%6u ipc_rct=%6u mpki_lst=%6u mpki_rct=%6u",
		__entry->cpu,
		__entry->last[0], __entry->recent[0],
		__entry->last[1], __entry->recent[1])
);

TRACE_EVENT(mlt_update_tsk_uarch,

	TP_PROTO(struct task_struct *p, struct mlt_part *part, struct mlt_uarch *uarch),

	TP_ARGS(p, part, uarch),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN		)
		__field(	pid_t,		pid			)
		__field(	int,		cpu				)
		__array(	u32,		recent,	MLT_UARCH_TSK_NUM	)
		__array(	u32,		last,	MLT_UARCH_TSK_NUM	)
	),

	TP_fast_assign(
		int i;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->cpu			= task_cpu(p);
		for (i = 0; i < MLT_UARCH_TSK_NUM; i++) {
			__entry->last[i]	= uarch[i].periods[part->active_period];
			__entry->recent[i]	= uarch[i].recent;
		}
	),

	TP_printk("comm=%15s pid=%6d cpu=%1d "
		"ipc_lst=%6u ipc_rct=%6u",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->last[0], __entry->recent[0]
		)
);

TRACE_EVENT(mlt_update_tsk_uarch_periods,

	TP_PROTO(struct task_struct *p, struct mlt_part *part, struct mlt_uarch *uarch, int idx),

	TP_ARGS(p, part, uarch, idx),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN		)
		__field(	pid_t,		pid				)
		__field(	int,		cpu				)
		__field(	int,		prd				)
		__array(	u32,		periods,	MLT_PERIOD_COUNT)
		__field(	u32,		recent				)
	),

	TP_fast_assign(
		int i;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->cpu			= task_cpu(p);
		__entry->prd			= part->active_period;
		__entry->recent			= uarch[idx].recent;
		for (i = 0; i < MLT_PERIOD_COUNT; i++) {
		//	c = ((part->active_period + i + MLT_PERIOD_COUNT) % MLT_PERIOD_COUNT);
			__entry->periods[i]	= uarch[idx].periods[i];
		}
	),

	TP_printk("comm=%15s pid=%6d cpu=%1d prd=%2d type=ipc"
		"p=%6u,%6u,%6u,%6u,%6u,%6u,%6u,%6u rct=%6u",
		__entry->comm, __entry->pid, __entry->cpu, __entry->prd,
		__entry->periods[7], __entry->periods[6], __entry->periods[5], __entry->periods[4],
		__entry->periods[3], __entry->periods[2], __entry->periods[1], __entry->periods[0],
		__entry->recent
		)
);

TRACE_EVENT(mlt_runnable_update,

	TP_PROTO(int cpu, struct mlt_runnable *mlt),

	TP_ARGS(cpu, mlt),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		contrib			)
		__field(	u64,		runnable_cntr		)
		__field(	int,		nr_run			)
		__field(	int,		runnable)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->contrib		= mlt->contrib;
		__entry->runnable_cntr		= mlt->runnable_contrib;
		__entry->nr_run			= mlt->nr_run;
		__entry->runnable		= mlt->runnable;
	),

	TP_printk("cpu=%2d, nr_run=%2d, ctrb=%10llu, runnable_cntr=%11llu, runnable=%5d",
		__entry->cpu, __entry->nr_run, __entry->contrib,
		__entry->runnable_cntr, __entry->runnable)
);


/*
 * Tracepoint for multi load tracking for task
 */
TRACE_EVENT(mlt_update_task_part,

	TP_PROTO(struct task_struct *p, struct mlt_part *part),

	TP_ARGS(p, part),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	int,		cur_period		)
		__field(	int,		state			)
		__field(	u64,		contrib			)
		__field(	int,		recent			)
		__field(	int,		last			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->cpu			= p->wake_cpu;
		__entry->cur_period		= part->cur_period;
		__entry->state			= part->state;
		__entry->contrib		= part->contrib;
		__entry->recent			= part->recent;
		__entry->last			= part->periods[part->cur_period];
	),

	TP_printk("comm=%15s pid=%6d cpu=%d cur_period=%1d state=%2d cntrb=%8llu recent=%4d last=%4d",
		__entry->comm, __entry->pid, __entry->cpu, __entry->cur_period,
		__entry->state, __entry->contrib, __entry->recent, __entry->last)
);

/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ontime_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__field(	int,		migrate			)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, src_cpu=%1d, dst_cpu=%1d, migrate=%1d, reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

/*
* Tracepoint for ecs
*/
struct dynamic_dom;
TRACE_EVENT(dynamic_domain_info,

	TP_PROTO(int cpu, struct dynamic_dom *domain),

	TP_ARGS(cpu, domain),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned int,	gov_cpus		)
		__field(	int,		nr_busy_cpu		)
		__field(	int,		avg_nr_run		)
		__field(	int,		slower_misfit		)
		__field(	int,		active_ratio		)
		__field(	u64,		util			)
		__field(	u64,		cap			)
		__field(	int,		throttle_cnt		)
		__field(	int,		flag			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->gov_cpus		= *(unsigned int *)cpumask_bits(&domain->governor_cpus);
		__entry->avg_nr_run		= domain->avg_nr_run;
		__entry->nr_busy_cpu		= domain->nr_busy_cpu;
		__entry->slower_misfit		= domain->slower_misfit;
		__entry->active_ratio		= domain->active_ratio;
		__entry->util			= domain->util;
		__entry->cap			= domain->cap;
		__entry->throttle_cnt		= domain->throttle_cnt;
		__entry->flag			= domain->flag;
	),

	TP_printk("cpu=%1d, gov_cpus=0x%03x, avg_nr_run=%1d, nr_busy_cpu=%1d, slower_misfit=%1d, "
		"ar=%3d, util=%4llu, cap=%4llu, tcnt=%1d, flag=0x%02x",
		__entry->cpu, __entry->gov_cpus, __entry->avg_nr_run, __entry->nr_busy_cpu,
		__entry->slower_misfit, __entry->active_ratio, __entry->util,
		__entry->cap, __entry->throttle_cnt, __entry->flag)
);

/*
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update_governor_cpus,

	TP_PROTO(char *label, unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(label, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("cpus=0x%03x->0x%03x, reason=%s",
		__entry->prev_cpus, __entry->next_cpus, __entry->label)
);

/*
 * Tracepoint for frequency boost
 */
TRACE_EVENT(freqboost_boosted_util,

	TP_PROTO(int cpu, int ratio, unsigned long util, long margin),

	TP_ARGS(cpu, ratio, util, margin),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		ratio			)
		__field(	unsigned long,	util			)
		__field(	long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio == INT_MIN ? -9999 : ratio;
		__entry->util			= util;
		__entry->margin			= margin;
	),

	TP_printk("cpu=%1d, ratio=%5d, util=%4lu, margin=%4ld",
		__entry->cpu, __entry->ratio, __entry->util, __entry->margin)
);

TRACE_EVENT(freqboost_htsk_boosted_util,

	TP_PROTO(int cpu, int hratio, int ratio, int boost, unsigned long util, long margin),

	TP_ARGS(cpu, hratio, ratio, boost, util, margin),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		hratio			)
		__field(	int,		ratio			)
		__field(	int,		boost			)
		__field(	unsigned long,	util			)
		__field(	long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->hratio			= hratio;
		__entry->ratio			= ratio;
		__entry->boost			= boost;
		__entry->util			= util;
		__entry->margin			= margin;
	),

	TP_printk("cpu=%1d, hratio=%3d, ratio=%3d, boost=%3d, util=%4lu, margin=%3ld",
		__entry->cpu, __entry->hratio, __entry->ratio,
		__entry->boost, __entry->util,  __entry->margin)
);

struct fclamp_data;

/*
 * Tracepoint for fclamp applying result
 */
TRACE_EVENT(fclamp_apply,

	TP_PROTO(int cpu, unsigned int orig_freq, unsigned int new_freq, int boost,
		struct fclamp_data *fcd),

	TP_ARGS(cpu, orig_freq, new_freq, boost, fcd),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned int,	orig_freq		)
		__field(	unsigned int,	new_freq		)
		__field(	int,		boost			)
		__field(	int,		clamp_freq		)
		__field(	int,		target_period		)
		__field(	int,		target_ratio		)
		__field(	int,		type			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->orig_freq		= orig_freq;
		__entry->new_freq		= new_freq;
		__entry->boost			= boost;
		__entry->clamp_freq		= fcd->freq;
		__entry->target_period		= fcd->target_period;
		__entry->target_ratio		= fcd->target_ratio;
		__entry->type			= fcd->type;
	),

	TP_printk("cpu=%1d, orig_freq=%7u, new_freq=%7u, boost=%1d, clamp_freq=%7d, "
		"target_period=%3d, target_ratio=%4d, type=%s",
		__entry->cpu, __entry->orig_freq, __entry->new_freq, __entry->boost,
		__entry->clamp_freq, __entry->target_period, __entry->target_ratio,
		__entry->type ? "MAX" : "MIN")
);

/*
 * Tracepoint for checking fclamp release
 */
TRACE_EVENT(fclamp_can_release,

	TP_PROTO(int cpu, int type, int period, int target_period,
			int target_ratio, int active_ratio),

	TP_ARGS(cpu, type, period, target_period, target_ratio, active_ratio),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		type			)
		__field(	int,		period			)
		__field(	int,		target_period		)
		__field(	int,		target_ratio		)
		__field(	int,		active_ratio		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->type			= type;
		__entry->period			= period;
		__entry->target_period		= target_period;
		__entry->target_ratio		= target_ratio;
		__entry->active_ratio		= active_ratio;
	),

	TP_printk("cpu=%1d, type=%3s, period=%3d, target_period=%3d, "
		"target_ratio=%4d, active_ratio=%4d",
		__entry->cpu, __entry->type ? "MAX" : "MIN",
		__entry->period, __entry->target_period,
		__entry->target_ratio, __entry->active_ratio)
);

/*
 * Tracepoint for cfs_rq load tracking:
 */
TRACE_EVENT(sched_load_cfs_rq,

	TP_PROTO(struct cfs_rq *cfs_rq),

	TP_ARGS(cfs_rq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	load			)
		__field(	unsigned long,	rbl_load		)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
		struct rq *rq = __trace_get_rq(cfs_rq);

		__entry->cpu			= rq->cpu;
		__entry->load			= cfs_rq->avg.load_avg;
		__entry->rbl_load		= cfs_rq->avg.runnable_avg;
		__entry->util			= cfs_rq->avg.util_avg;
	),

	TP_printk("cpu=%1d, load=%4lu, rbl_load=%4lu, util=%4lu",
		__entry->cpu, __entry->load, __entry->rbl_load,__entry->util)
);

/*
 * Tracepoint for rt_rq load tracking:
 */
TRACE_EVENT(sched_load_rt_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
		__entry->cpu			= rq->cpu;
		__entry->util			= rq->avg_rt.util_avg;
	),

	TP_printk("cpu=%1d, util=%4lu", __entry->cpu, __entry->util)
);

/*
 * Tracepoint for dl_rq load tracking:
 */
TRACE_EVENT(sched_load_dl_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(	int,		cpu 			)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
		__entry->cpu			= rq->cpu;
		__entry->util			= rq->avg_dl.util_avg;
	),

	TP_printk("cpu=%1d util=%4lu", __entry->cpu, __entry->util)
);

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
/*
 * Tracepoint for irq load tracking:
 */
TRACE_EVENT(sched_load_irq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
		__entry->cpu			= rq->cpu;
		__entry->util			= rq->avg_irq.util_avg;
	),

	TP_printk("cpu=%1d util=%4lu", __entry->cpu, __entry->util)
);
#endif

#ifdef CREATE_TRACE_POINTS
static inline
struct rq * __trace_get_rq(struct cfs_rq *cfs_rq)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return cfs_rq->rq;
#else
	return container_of(cfs_rq, struct rq, cfs);
#endif
}
#endif /* CREATE_TRACE_POINTS */
	
/*
   ijiiiiiiiiiiil
 * Tracepoint for sched_	entity load tracking:
 */
TRACE_EVENT(sched_load_se,

	TP_PROTO(struct sched_entity *se),

	TP_ARGS(se),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	unsigned long,	rbl_load		)
		__field(	unsigned long,	util			)
	),

	TP_fast_assign(
#ifdef CONFIG_FAIR_GROUP_SCHED
		struct cfs_rq *gcfs_rq = se->my_q;
#else
		struct cfs_rq *gcfs_rq = NULL;
#endif
		struct task_struct *p = gcfs_rq ? NULL : container_of(se, struct task_struct, se);
		struct rq *rq = gcfs_rq ? __trace_get_rq(gcfs_rq) : NULL;

		__entry->cpu			= rq ? rq->cpu
						: task_cpu((container_of(se, struct task_struct, se)));
		memcpy(__entry->comm, p ? p->comm : "(null)", p ? TASK_COMM_LEN : sizeof("(null)"));
		__entry->pid			= p ? p->pid : -1;
		__entry->load			= se->avg.load_avg;
		__entry->rbl_load		= se->avg.runnable_avg;
		__entry->util			= se->avg.util_avg;
	),

	TP_printk("cpu=%1d, comm=%15s, pid=%6d, load=%4lu, rbl_load=%4lu, util=%4lu",
		__entry->cpu, __entry->comm, __entry->pid,
		__entry->load, __entry->rbl_load, __entry->util)
);

/*
 * Tracepoint for system overutilized flag
 */
TRACE_EVENT(sched_overutilized,

	TP_PROTO(int overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field(	int,		overutilized		)
	),

	TP_fast_assign(
		__entry->overutilized	= overutilized;
	),

	TP_printk("overutilized=%d",
		__entry->overutilized)
);

TRACE_EVENT(gsc_decision_activate,

	TP_PROTO(unsigned long group_load, u64 now, char *label),

	TP_ARGS(group_load, now, label),

	TP_STRUCT__entry(
		__field(	unsigned long,	group_load		)
		__field(	u64,		now			)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		__entry->group_load		= group_load;
		__entry->now			= now;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("group_load=%lu, now=%llu, reason=%s",
		__entry->group_load, __entry->now, __entry->label)
);

TRACE_EVENT(gsc_freq_boost,

	TP_PROTO(int cpu, int avg_runnable, int ratio, int boosted),

	TP_ARGS(cpu, avg_runnable, ratio, boosted),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__field(	int,		avg_runnable	)
		__field(	int,		ratio		)
		__field(	int,		boosted)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->avg_runnable	= avg_runnable;
		__entry->ratio		= ratio;
		__entry->boosted	= boosted;
	),

	TP_printk("cpu=%d, avg_runnable=%6d boost_ratio=%3d boosted=%2d",
		__entry->cpu, __entry->avg_runnable,
		__entry->ratio, __entry->boosted)
);

/* CPUIDLE Governor */
TRACE_EVENT(halo_reflect,

	TP_PROTO(unsigned int cpu, unsigned int waker, u64 sleep_length),

	TP_ARGS(cpu, waker, sleep_length),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpu			)
		__field(	unsigned int,	waker			)
		__field(	u64,		sleep_length		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->waker			= waker;
		__entry->sleep_length		= sleep_length;
	),

	TP_printk("cpu=%1d, waker=%1d, sleep_length=%10llu",
		__entry->cpu, __entry->waker, __entry->sleep_length)
);

TRACE_EVENT(halo_timer_fn,

	TP_PROTO(unsigned int cpu, unsigned int worked),

	TP_ARGS(cpu, worked),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpu			)
		__field(	unsigned int,	worked			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->worked			= worked;
	),

	TP_printk("cpu=%1d, worked=%1d",
		__entry->cpu, __entry->worked)
);

TRACE_EVENT(halo_tick,

	TP_PROTO(unsigned int cpu, bool triggerd, s64 last_tick_ns),

	TP_ARGS(cpu, triggerd, last_tick_ns),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpu			)
		__field(	bool,		triggerd		)
		__field(	s64,		last_tick_ns		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->triggerd		= triggerd;
		__entry->last_tick_ns		= last_tick_ns;
	),

	TP_printk("cpu=%d, triggerd=%d, last_tick_ns=%lld",
		__entry->cpu, __entry->triggerd, __entry->last_tick_ns)
);

TRACE_EVENT(halo_ipi,

	TP_PROTO(int cpu, int heavy, s64 min, s64 max, s64 cur, s64 avg, s64 correct, s64 pred),

	TP_ARGS(cpu, heavy, min, max, cur, avg, correct, pred),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		heavy			)
		__field(	s64,		min			)
		__field(	s64,		max			)
		__field(	s64,		cur			)
		__field(	s64,		avg			)
		__field(	s64,		correct			)
		__field(	s64,		pred			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->heavy			= heavy;
		__entry->min			= min;
		__entry->max			= max;
		__entry->cur			= cur;
		__entry->avg			= avg;
		__entry->correct		= correct;
		__entry->pred			= pred;
	),

	TP_printk("cpu=%1d, heavy=%4d, min=%10lld, max=%10lld, cur=%10lld, "
		"avg=%10lld, corrrect=%10lld, pred=%10lld",
		__entry->cpu, __entry->heavy, __entry->min, __entry->max, __entry->cur,
		__entry->avg, __entry->correct, __entry->pred)
);

TRACE_EVENT(halo_hstate,

	TP_PROTO(int cpu, int idx, u64 hit, u64 miss),

	TP_ARGS(cpu, idx, hit, miss),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		idx			)
		__field(	u64,		hit			)
		__field(	u64,		miss			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->idx			= idx;
		__entry->hit			= hit;
		__entry->miss			= miss;
	),

	TP_printk("cpu=%1d, idx=%1d, hit=%4llu, miss=%4llu",
		  __entry->cpu, __entry->idx, __entry->hit, __entry->miss)
);

/*
 * Tracepoint for lb cpu util
 */
TRACE_EVENT(lb_cpu_util,

	TP_PROTO(int cpu, char *label),

	TP_ARGS(cpu, label),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		active_balance		)
		__field(	int,		idle			)
		__field(	int,		nr_running		)
		__field(	int,		cfs_nr_running		)
		__field(	int,		nr_misfits		)
		__field(	unsigned long,	cpu_util		)
		__field(	unsigned long,	capacity_orig		)
		__array(	char,		label,	LABEL_LEN	)
		),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->active_balance		= cpu_rq(cpu)->active_balance;
		__entry->idle			= available_idle_cpu(cpu);
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->cfs_nr_running		= cpu_rq(cpu)->cfs.h_nr_running;
		__entry->nr_misfits		= ems_rq_nr_misfited(cpu_rq(cpu));
		__entry->cpu_util		= ml_cpu_util(cpu);
		__entry->capacity_orig		= capacity_orig_of(cpu);
		strncpy(__entry->label, label, LABEL_LEN - 1);
		),

	TP_printk("cpu=%d, active_balance=%1d, idle=%1d, nr_running=%2d, cfs_nr_running=%2d, "
		"nr_misfits=%1d, cpu_util=%4lu, capacity=%4lu, reason=%s",
		__entry->cpu, __entry->active_balance, __entry->idle,
		__entry->nr_running, __entry->cfs_nr_running, __entry->nr_misfits,
		__entry->cpu_util, __entry->capacity_orig, __entry->label)
);


/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(lb_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__field(	int,		migrate			)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, src_cpu=%1d, dst_cpu=%1d, migrate=%1d, reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

 /*
 * Tracepoint for misfit
 */
TRACE_EVENT(lb_update_misfit,
	TP_PROTO(struct task_struct *p, bool old, bool cur, int cpu, u64 rq_nr_misfit, char *label),

	TP_ARGS(p, old, cur, cpu, rq_nr_misfit, label),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	util			)
		__field(	bool,		old			)
		__field(	bool,		cur			)
		__field(	int,		cpu			)
		__field(	u64,		rq_nr_misfit		)
		__array(	char,		label,	LABEL_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->util			= ml_uclamp_task_util(p);
		__entry->old			= old;
		__entry->cur			= cur;
		__entry->cpu			= cpu;
		__entry->rq_nr_misfit		= rq_nr_misfit;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("comm=%15s, pid=%6d, util=%4ld, old=%1d, cur=%1d, cpu=%1d, "
		"rq_nr_misfit=%3llu, reason=%s",
		__entry->comm, __entry->pid, __entry->util, __entry->old,
		__entry->cur, __entry->cpu, __entry->rq_nr_misfit, __entry->label)
);

/*
 * Tracepoint for nohz balancer kick
 */
TRACE_EVENT(lb_nohz_balancer_kick,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned int,	nr_running		)
		__field(	unsigned int,	cfs_nr_running		)
	),

	TP_fast_assign(
		__entry->cpu			= rq->cpu;
		__entry->nr_running		= rq->nr_running;
		__entry->cfs_nr_running		= rq->cfs.h_nr_running;
	),

	TP_printk("cpu=%1d, nr_running=%2u, cfs_nr_running=%2u",
		__entry->cpu, __entry->nr_running, __entry->cfs_nr_running)
);

/*
 * Tracepoint for mhdvfs profile
 */
TRACE_EVENT(mhdvfs_profile,

	TP_PROTO(u64 ipc_sum, u64 mspc_sum),

	TP_ARGS(ipc_sum, mspc_sum),

	TP_STRUCT__entry(
		__field(	u64,		ipc_sum			)
		__field(	u64,		mspc_sum		)
	),

	TP_fast_assign(
		__entry->ipc_sum		= ipc_sum;
		__entry->mspc_sum		= mspc_sum;
	),

	TP_printk("ipc_sum=%10llu, mspc_sum=%10llu",
		  __entry->ipc_sum, __entry->mspc_sum)
);

/*
 * Tracepoint for kicking dsufreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_dsufreq,

	TP_PROTO(u64 mspc, int ratio),

	TP_ARGS(mspc, ratio),

	TP_STRUCT__entry(
		__field(	u64,		mspc			)
		__field(	int,		ratio			)
	),

	TP_fast_assign(
		__entry->mspc			= mspc;
		__entry->ratio			= ratio;
	),

	TP_printk("mspc=%10llu, ratio=%4d", __entry->mspc, __entry->ratio)
);

/*
 * Tracepoint for kicking miffreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_miffreq,

	TP_PROTO(u64 mspc, unsigned int ratio, u64 duration_ms),

	TP_ARGS(mspc, ratio, duration_ms),

	TP_STRUCT__entry(
		__field(	u64,		mspc			)
		__field(	unsigned int,	ratio			)
		__field(	u64,		duration_ms		)
	),

	TP_fast_assign(
		__entry->mspc			= mspc;
		__entry->ratio			= ratio;
		__entry->duration_ms		= duration_ms;
	),

	TP_printk("mspc=%10llu, ratio=%4u, duration=%10llu",
		__entry->mspc, __entry->ratio, __entry->duration_ms)
);

/*
 * Tracepoint for kicking cpufreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_cpufreq,

	TP_PROTO(int cpu, u64 ipc, int ratio),

	TP_ARGS(cpu, ipc, ratio),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		ipc			)
		__field(	int,		ratio			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ipc			= ipc;
		__entry->ratio			= ratio;
	),

	TP_printk("cpu=%1d, ipc=%10llu, ratio=%4d",
		__entry->cpu, __entry->ipc, __entry->ratio)
);

/*
 * Tracepoint for emstune task boost
 */
TRACE_EVENT(emstune_task_boost,

	TP_PROTO(struct task_struct *p, int val, char *label),

	TP_ARGS(p, val, label),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__array(	char,		val,	10		)
		__array( 	char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid = p->pid;
		strncpy(__entry->val, val ? "on" : "off", 9);
		strncpy(__entry->label, label, 63);
	),

	TP_printk("%s: comm=%s pid=%d val=%s",
		__entry->label, __entry->comm, __entry->pid, __entry->val)
);

TRACE_EVENT(pago_data_snapshot,

	TP_PROTO(int cpu, int idle_sample, u64 last_sampled, u64 last_updated, u64 *data, char *label),

	TP_ARGS(cpu, idle_sample, last_sampled, last_updated, data, label),

	TP_STRUCT__entry(
		__field( int,		cpu		)
		__field( int,		idle_sample	)
		__field( u64,		last_sampled	)
		__field( u64,		last_updated	)
		__field( u64,		cycle		)
		__field( u64,		inst		)
		__field( u64,		l3_refill	)
		__array( char,		label,	64	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->idle_sample	= idle_sample;
		__entry->last_sampled	= last_sampled;
		__entry->last_updated	= last_updated;
		__entry->cycle		= data[0];
		__entry->inst		= data[1];
		__entry->l3_refill	= data[2];
		strncpy(__entry->label, label, 63);
	),

	TP_printk("%s: cpu=%d idle_sample=%d last_sampled=%llu last_updated=%llu "
			"cycle=%llu inst=%llu l3_refill=%llu",
		__entry->label, __entry->cpu, __entry->idle_sample,
		__entry->last_sampled, __entry->last_updated, __entry->cycle,
		__entry->inst, __entry->l3_refill)
);

TRACE_EVENT(pago_delta_snapshot,

	TP_PROTO(int cpu, int idle_sample, s64 delta_us, u64 *data),

	TP_ARGS(cpu, idle_sample, delta_us, data),

	TP_STRUCT__entry(
		__field( int,		cpu		)
		__field( int,		idle_sample	)
		__field( s64,		delta_us	)
		__field( u64,		cycle		)
		__field( u64,		inst		)
		__field( u64,		l3_refill	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->idle_sample	= idle_sample;
		__entry->delta_us	= delta_us;
		__entry->cycle		= data[0];
		__entry->inst		= data[1];
		__entry->l3_refill	= data[2];
	),

	TP_printk("cpu=%d idle_sample=%d delta_us=%lld cycle=%llu inst=%llu l3_refill=%llu",
		__entry->cpu, __entry->idle_sample, __entry->delta_us, __entry->cycle,
		__entry->inst, __entry->l3_refill)
);

TRACE_EVENT(pago_target_snapshot,

	TP_PROTO(int cpu, unsigned int target_freq, unsigned int cycle_freq, unsigned int scale_freq,
		unsigned int cur_freq, unsigned int mpki, unsigned int down_step, const char *label),

	TP_ARGS(cpu, target_freq, cycle_freq, scale_freq, cur_freq, mpki, down_step, label),

	TP_STRUCT__entry(
		__field( int,		cpu		)
		__field( unsigned int,	target_freq	)
		__field( unsigned int,	cycle_freq	)
		__field( unsigned int,	scale_freq	)
		__field( unsigned int,	cur_freq	)
		__field( unsigned int, 	mpki		)
		__field( unsigned int,	down_step	)
		__array( char,		label,	64	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->target_freq	= target_freq;
		__entry->cycle_freq	= cycle_freq;
		__entry->scale_freq	= scale_freq;
		__entry->cur_freq	= cur_freq;
		__entry->mpki		= mpki;
		__entry->down_step	= down_step;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("%s: cpu=%d target_freq=%u cycle_freq=%u scale_freq=%u cur_freq=%u mpki=%u down_step=%u",
		__entry->label, __entry->cpu, __entry->target_freq, __entry->cycle_freq,
		__entry->scale_freq, __entry->cur_freq, __entry->mpki, __entry->down_step)
);

TRACE_EVENT(pago_notifier,

	TP_PROTO(int level, char *label),

	TP_ARGS(level, label),

	TP_STRUCT__entry(
		__field( int,		level		)
		__array( char,		label,	64	)
	),

	TP_fast_assign(
		__entry->level		= level;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("%s level=%d",
		__entry->label, __entry->level)
);

TRACE_EVENT(pago_boost_work,

	TP_PROTO(unsigned int boost_freq, const char *label),

	TP_ARGS(boost_freq, label),

	TP_STRUCT__entry(
		__field( unsigned int, boost_freq )
		__array( char, label, LABEL_LEN )
	),

	TP_fast_assign(
		__entry->boost_freq = boost_freq;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("%s: boost_freq=%u", __entry->label, __entry->boost_freq)
);

TRACE_EVENT(pago_work,

	TP_PROTO(unsigned int target_freq, unsigned int boost_freq, unsigned int resolved_freq,
		const char *label),

	TP_ARGS(target_freq, boost_freq, resolved_freq, label),

	TP_STRUCT__entry(
		__field( unsigned int, target_freq )
		__field( unsigned int, boost_freq )
		__field( unsigned int, resolved_freq )
		__array( char, label, LABEL_LEN )
	),

	TP_fast_assign(
		__entry->target_freq = target_freq;
		__entry->boost_freq = boost_freq;
		__entry->resolved_freq = resolved_freq;
		strncpy(__entry->label, label, LABEL_LEN - 1);
	),

	TP_printk("%s: target_freq=%u boost_freq=%u resolved_freq=%u",
		__entry->label, __entry->target_freq, __entry->boost_freq, __entry->resolved_freq)
);

/*
 * Tracepoint for ems taskgroup
 */
TRACE_EVENT(ems_update_taskgroup,

	TP_PROTO(struct task_struct *p, int group),

	TP_ARGS(p, group),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		group			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid = p->pid;
		__entry->group = group
	),

	TP_printk("comm=%s pid=%d taskgroup=%d",
		__entry->comm, __entry->pid, __entry->group)
);

TRACE_EVENT(ems_update_uclamp_min,

	TP_PROTO(int group, unsigned int uclamp_min),

	TP_ARGS(group, uclamp_min),

	TP_STRUCT__entry(
		__field(	int,		group			)
		__field(	unsigned int,	uclamp_min		)
	),

	TP_fast_assign(
		__entry->group = group;
		__entry->uclamp_min = uclamp_min
	),

	TP_printk("taskgroup=%d uclamp_min=%u",
		__entry->group, __entry->uclamp_min)
);

TRACE_EVENT(ems_update_uclamp_max,

	TP_PROTO(int group, unsigned int uclamp_max),

	TP_ARGS(group, uclamp_max),

	TP_STRUCT__entry(
		__field(	int,		group			)
		__field(	unsigned int,	uclamp_max		)
	),

	TP_fast_assign(
		__entry->group = group;
		__entry->uclamp_max = uclamp_max
	),

	TP_printk("taskgroup=%d uclamp_max=%u",
		__entry->group, __entry->uclamp_max)
);
#endif /* _TRACE_EMS_DEBUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
