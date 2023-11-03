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

struct mlt;
struct rq;
struct tp_env;

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
		__array(	char,		event,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->arg1			= arg1;
		__entry->arg2			= arg2;
		strncpy(__entry->event, event ? event : "no event", 63);
	),

	TP_printk("comm=%s pid=%d cpu=%d arg1=%llu arg2=%llu event=%s",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->arg1, __entry->arg2, __entry->event)
);

/*
 * Tracepoint for finding busy cpu
 */
TRACE_EVENT(ems_find_busy_cpus,

	TP_PROTO(struct cpumask *busy_cpus, int busy_cpu_ratio),

	TP_ARGS(busy_cpus, busy_cpu_ratio),

	TP_STRUCT__entry(
		__field(	unsigned int,	busy_cpus		)
		__field(	int,		busy_cpu_ratio		)
	),

	TP_fast_assign(
		__entry->busy_cpus		= *(unsigned int *)cpumask_bits(busy_cpus);
		__entry->busy_cpu_ratio		= busy_cpu_ratio;
	),

	TP_printk("busy_cpus=%#x busy_cpu_ratio=%d",
		  __entry->busy_cpus, __entry->busy_cpu_ratio)
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
		__field(	unsigned long,	cpu)
		__field(	unsigned long,	task_util)
		__field(	unsigned long,	task_util_clamped)
		__field(	unsigned long,	task_load_avg)
		__field(	unsigned long,	util)
		__field(	unsigned long,	util_wo)
		__field(	unsigned long,	util_with)
		__field(	unsigned long,	runnable)
		__field(	unsigned long,	load_avg)
		__field(	unsigned long,	rt_util)
		__field(	unsigned long,	dl_util)
		__field(	unsigned int,	nr_running)
		__field(	int,		idle)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->cpu       		= cpu;
		__entry->task_util		= env->task_util;
		__entry->task_util_clamped	= env->task_util_clamped;
		__entry->task_load_avg		= env->task_load_avg;
		__entry->util			= env->cpu_stat[cpu].util;
		__entry->util_wo   		= env->cpu_stat[cpu].util_wo;
		__entry->util_with 		= env->cpu_stat[cpu].util_with;
		__entry->runnable 		= env->cpu_stat[cpu].runnable;
		__entry->load_avg 		= env->cpu_stat[cpu].load_avg;
		__entry->rt_util   		= env->cpu_stat[cpu].rt_util;
		__entry->dl_util   		= env->cpu_stat[cpu].dl_util;
		__entry->nr_running		= env->cpu_stat[cpu].nr_running;
		__entry->idle			= env->cpu_stat[cpu].idle;
	),

	TP_printk("comm=%s pid=%d cpu=%u "
		  "task_util=%lu task_util_clamped=%lu task_load_avg=%lu "
		  "util=%lu util_wo=%lu util_with=%lu runnable=%lu load_avg=%lu "
		  "rt_util=%lu dl_util=%lu nr_running=%u idle=%d",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->task_util, __entry->task_util_clamped,
		  __entry->task_load_avg,
		  __entry->util, __entry->util_wo, __entry->util_with,
		  __entry->runnable, __entry->load_avg,
		  __entry->rt_util, __entry->dl_util,
		  __entry->nr_running, __entry->idle)
);

/*
 * Tracepoint for setting fit_cpus by prio pinning
 */
TRACE_EVENT(tex_pinning_fit_cpus,

	TP_PROTO(struct task_struct *p, int tex, int suppress,
		struct cpumask *fit_cpus, struct cpumask *pinning_cpus,
		struct cpumask *busy_cpus),

	TP_ARGS(p, tex, suppress, fit_cpus, pinning_cpus, busy_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		tex			)
		__field(	int,		suppress		)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	pinning_cpus		)
		__field(	unsigned int,	busy_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->tex		= tex;
		__entry->suppress	= suppress;
		__entry->fit_cpus	= *(unsigned int *)cpumask_bits(fit_cpus);
		__entry->pinning_cpus	= *(unsigned int *)cpumask_bits(pinning_cpus);
		__entry->busy_cpus	= *(unsigned int *)cpumask_bits(busy_cpus);
	),

	TP_printk("comm=%s pid=%d tex=%d suppress=%d fit_cpus=%#x pinning_cpus=%#x busy_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->tex, __entry->suppress,
		  __entry->fit_cpus, __entry->pinning_cpus, __entry->busy_cpus)
);

/*
 * Tracepoint for prio pinning schedule
 */
TRACE_EVENT(tex_pinning_schedule,

	TP_PROTO(int target_cpu, struct cpumask *candidates),

	TP_ARGS(target_cpu, candidates),

	TP_STRUCT__entry(
		__field(	int,		target_cpu	)
		__field(	unsigned int,	candidates	)
	),

	TP_fast_assign(
		__entry->target_cpu	= target_cpu;
		__entry->candidates	= *(unsigned int *)cpumask_bits(candidates);
	),

	TP_printk("target_cpu=%d candidates=%#x",
		__entry->target_cpu, __entry->candidates)
);

/*
 * Tracepoint for task picked by queue jump
 */
TRACE_EVENT(tex_qjump_pick_next_task,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prio			)
		__field(	int,		cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->cpu		= task_cpu(p);
	),

	TP_printk("comm=%s pid=%d prio=%d cpu=%d",
		  __entry->comm, __entry->pid, __entry->prio, __entry->cpu)
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
		__entry->pid		= env->p->pid;
		__entry->task_util	= env->task_util;
		__entry->best_cpu	= best_cpu;
		__entry->idle		= best_cpu < 0 ? -1 : env->cpu_stat[best_cpu].idle;
	),

	TP_printk("comm=%s pid=%d tsk_util=%d best_cpu=%d idle=%d",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->best_cpu, __entry->idle)
);

/*
 * Tracepoint for finding semi perf cpu
 */
TRACE_EVENT(ems_find_semi_perf_cpu,

	TP_PROTO(struct tp_env *env, int min_energy_cpu, int max_spare_cpu,
			struct cpumask *candidates),

	TP_ARGS(env, min_energy_cpu, max_spare_cpu, candidates),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		min_energy_cpu		)
		__field(	int,		max_spare_cpu		)
		__field(	unsigned int,	candidates		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid		= env->p->pid;
		__entry->min_energy_cpu	= min_energy_cpu;
		__entry->max_spare_cpu	= max_spare_cpu;
		__entry->candidates	= *(unsigned int *)cpumask_bits(candidates);
	),

	TP_printk("comm=%s pid=%d min_energy_cpu=%d max_spare_cpu=%d candidates=%#x",
		__entry->comm, __entry->pid, __entry->min_energy_cpu,
		__entry->max_spare_cpu, __entry->candidates)
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

	TP_printk("comm=%s pid=%d candidates=%#x energy_cpu=%d adv_energy_cpu=%d",
		  __entry->comm, __entry->pid, __entry->candidates,
		  __entry->energy_cpu, __entry->adv_energy_cpu)
);

/*
 * Tracepoint for computing energy
 */
TRACE_EVENT(ems_compute_energy,

	TP_PROTO(int cpu, unsigned long util, unsigned long cap, unsigned long v,
		unsigned long dp, unsigned long sp, unsigned long sl_sp, int apply_sp,
		unsigned long weight, unsigned long energy),

	TP_ARGS(cpu, util, cap, v, dp, sp, sl_sp, apply_sp, weight, energy),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	cap			)
		__field(	unsigned long,	v			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	sp			)
		__field(	unsigned long,	sl_sp			)
		__field(	int,		apply_sp		)
		__field(	unsigned long,	weight			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
		__entry->cap		= cap;
		__entry->v		= v;
		__entry->dp		= dp;
		__entry->sp		= sp;
		__entry->sl_sp		= sl_sp;
		__entry->apply_sp	= apply_sp;
		__entry->weight		= weight;
		__entry->energy		= energy;
	),

	TP_printk("cpu=%d util=%lu cap=%lu v=%lu dp=%lu sp=%lu sl_sp=%lu "
		  "apply_sp=%d weight=%lu energy=%lu",
		__entry->cpu, __entry->util, __entry->cap, __entry->v,
		__entry->dp, __entry->sp, __entry->sl_sp, __entry->apply_sp,
		__entry->weight, __entry->energy)
);

/*
 * Tracepoint for computing DSU energy
 */
TRACE_EVENT(ems_compute_dsu_energy,

	TP_PROTO(unsigned long max_cpu_freq, unsigned long dsu_freq,
		unsigned long v, unsigned long dp, unsigned long sp,
		unsigned long energy),

	TP_ARGS(max_cpu_freq, dsu_freq, v, dp, sp, energy),

	TP_STRUCT__entry(
		__field(	unsigned long,	max_cpu_freq		)
		__field(	unsigned long,	dsu_freq		)
		__field(	unsigned long,	v			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	sp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->max_cpu_freq	= max_cpu_freq;
		__entry->dsu_freq	= dsu_freq;
		__entry->v		= v;
		__entry->dp		= dp;
		__entry->sp		= sp;
		__entry->energy		= energy;
	),

	TP_printk("max_cpu_freq=%lu dsu_freq=%lu v=%lu dp=%lu sp=%lu energy=%lu",
		__entry->max_cpu_freq, __entry->dsu_freq, __entry->v, __entry->dp,
		__entry->sp, __entry->energy)
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
		__entry->pid		= p->pid;
		__entry->candidates	= *(unsigned int *)cpumask_bits(candidates);
		__entry->target_cpu	= target_cpu;
		__entry->energy		= energy;
	),

	TP_printk("comm=%s pid=%d candidates=%#x target_cpu=%d energy=%lu",
		__entry->comm, __entry->pid, __entry->candidates,
		__entry->target_cpu, __entry->energy)
);

/*
 * Tracepoint for computing energy/capacity efficiency
 */
TRACE_EVENT(ems_compute_eff,

	TP_PROTO(struct tp_env *env, int cpu,
		unsigned int weight, unsigned long capacity,
		unsigned long energy, unsigned int eff),

	TP_ARGS(env, cpu, weight, capacity, energy, eff),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	cpu_util_with		)
		__field(	unsigned int,	weight			)
		__field(	unsigned long,	capacity		)
		__field(	unsigned long,	energy			)
		__field(	unsigned int,	eff			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid		= env->p->pid;
		__entry->cpu		= cpu;
		__entry->task_util	= env->p->se.avg.util_avg;
		__entry->cpu_util_with	= env->cpu_stat[cpu].util_with;
		__entry->weight		= weight;
		__entry->capacity	= capacity;
		__entry->energy		= energy;
		__entry->eff		= eff;
	),

	TP_printk("comm=%s pid=%d cpu=%d task_util=%lu cpu_util_with=%lu "
		  "weight=%u capacity=%lu energy=%lu eff=%u",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->task_util, __entry->cpu_util_with,
		__entry->weight, __entry->capacity,
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
		__field(	int,		cpu			)
		__field(	unsigned int,	capacity		)
		__field(	int,		idle_state		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
		__entry->capacity	= capacity;
		__entry->idle_state	= idle_state;
	),

	TP_printk("comm=%s pid=%d cpu=%d capacity=%u idle_state=%d",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->capacity, __entry->idle_state)
);

/*
 * Tracepoint for core selection by sysbusy schedule
 */
TRACE_EVENT(sysbusy_schedule,

	TP_PROTO(struct task_struct *p, int state, int target_cpu),

	TP_ARGS(p, state, target_cpu),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field(unsigned long,	util			)
		__field(int,		state			)
		__field(int,		target_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= p->se.avg.util_avg;
		__entry->state		= state;
		__entry->target_cpu	= target_cpu;
	),

	TP_printk("comm=%s pid=%d util=%ld state=%d target_cpu=%d",
		__entry->comm, __entry->pid, __entry->util,
		__entry->state, __entry->target_cpu)
);

/*
 * Tracepoint for monitoring sysbusy state
 */
TRACE_EVENT(ems_profile,

	TP_PROTO(int cpu, unsigned long cpu_util),

	TP_ARGS(cpu, cpu_util),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	cpu_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cpu_util		= cpu_util;
	),

	TP_printk("cpu=%d cpu_util=%ld", __entry->cpu, __entry->cpu_util)
);

/*
 * Tracepoint for tracking tasks in system
 */
TRACE_EVENT(ems_profile_tasks,

	TP_PROTO(int busy_cpu_count, unsigned long cpu_util_sum,
		int heavy_task_count, unsigned long heavy_task_util_sum,
		int misfit_task_count, unsigned long misfit_task_util_sum),

	TP_ARGS(busy_cpu_count, cpu_util_sum,
		heavy_task_count, heavy_task_util_sum,
		misfit_task_count, misfit_task_util_sum),

	TP_STRUCT__entry(
		__field(	int,		busy_cpu_count		)
		__field(	unsigned long,	cpu_util_sum		)
		__field(	int,		heavy_task_count	)
		__field(	unsigned long,	heavy_task_util_sum	)
		__field(	int,		misfit_task_count	)
		__field(	unsigned long,	misfit_task_util_sum	)
	),

	TP_fast_assign(
		__entry->busy_cpu_count		= busy_cpu_count;
		__entry->cpu_util_sum		= cpu_util_sum;
		__entry->heavy_task_count	= heavy_task_count;
		__entry->heavy_task_util_sum	= heavy_task_util_sum;
		__entry->misfit_task_count	= misfit_task_count;
		__entry->misfit_task_util_sum	= misfit_task_util_sum;
	),

	TP_printk("busy_cpu_count=%d cpu_util_sum=%lu "
		  "heavy_task_count=%d heavy_task_util_sum=%lu "
		  "misfit_task_count=%d misfit_task_util_sum=%lu",
		  __entry->busy_cpu_count, __entry->cpu_util_sum,
		  __entry->heavy_task_count, __entry->heavy_task_util_sum,
		  __entry->misfit_task_count, __entry->misfit_task_util_sum)
);

/*
 * Tracepoint for somac scheduling
 */
TRACE_EVENT(sysbusy_somac,

	TP_PROTO(struct task_struct *p, unsigned long util,
				int src_cpu, int dst_cpu),

	TP_ARGS(p, util, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	util			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= util;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
	),

	TP_printk("comm=%s pid=%d util_avg=%lu src_cpu=%d dst_cpu=%d",
		__entry->comm, __entry->pid, __entry->util,
		__entry->src_cpu, __entry->dst_cpu)
);

/*
 * Tracepoint for the STT
 */
TRACE_EVENT(stt_update_cpu,

	TP_PROTO(int cpu, int cur_ar, int htsk_ar, int pid, int ht_en, u64 next_update,
		int hcnt, u64 last_update, int htask_boost, int state),

	TP_ARGS(cpu, cur_ar, htsk_ar, pid, ht_en, next_update, hcnt, last_update, htask_boost, state),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_ar			)
		__field( int,	htsk_ar			)
		__field( int,	pid			)
		__field( int,	ht_en			)
		__field( u64,	next_update		)
		__field( int,	hcnt			)
		__field( u64,	last_update		)
		__field( int,	htask_boost		)
		__field( int,	state			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cur_ar			= cur_ar;
		__entry->htsk_ar		= htsk_ar;
		__entry->pid			= pid;
		__entry->ht_en			= ht_en;
		__entry->next_update		= next_update;
		__entry->hcnt			= hcnt;
		__entry->last_update		= last_update;
		__entry->htask_boost		= htask_boost;
		__entry->state			= state;
	),

	TP_printk("cpu=%d cur_ar=%d htask_ar=%d pid=%d(hten=%d) nextup=%lu hcnt=%d lastup=%lu hboost=%d state=%d",
		__entry->cpu, __entry->cur_ar, __entry->htsk_ar,
		__entry->pid, __entry->ht_en, __entry->next_update, __entry->hcnt,
		__entry->last_update, __entry->htask_boost, __entry->state)
);

TRACE_EVENT(stt_update_htsk_heaviness,

	TP_PROTO(int cpu, int pid, int idx, int ratio, int cnt),

	TP_ARGS(cpu, pid, idx, ratio, cnt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	pid			)
		__field( int,	idx			)
		__field( int,	ratio			)
		__field( int,	cnt			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pid			= pid;
		__entry->idx			= idx;
		__entry->ratio			= ratio;
		__entry->cnt			= cnt;
	),

	TP_printk("cpu=%d pid=%d idx=%d ratio=%d cnt=%d",
		__entry->cpu, __entry->pid, __entry->idx, __entry->ratio, __entry->cnt)
);

/*
 * Tracepoint for EGO
 */
TRACE_EVENT(ego_sched_util,

	TP_PROTO(int cpu, unsigned long util, unsigned long cfs, unsigned long rt,
		unsigned long dl, unsigned long bwdl, unsigned long irq),

	TP_ARGS(cpu, util, cfs, rt, dl, bwdl, irq),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	util				)
		__field( unsigned long,	cfs				)
		__field( unsigned long,	rt				)
		__field( unsigned long,	dl				)
		__field( unsigned long,	bwdl				)
		__field( unsigned long,	irq				)
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

	TP_printk("cpu=%d util=%ld cfs=%ld rt=%ld dl=%ld dlbw=%ld irq=%ld",
		__entry->cpu, __entry->util, __entry->cfs, __entry->rt,
		__entry->dl, __entry->bwdl, __entry->irq)
);

TRACE_EVENT(ego_cpu_util,

	TP_PROTO(int cpu, unsigned long pelt_boost, unsigned long util, unsigned long io_util, unsigned long bst_util),

	TP_ARGS(cpu, pelt_boost, util, io_util, bst_util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	pelt_boost			)
		__field( unsigned long,	util				)
		__field( unsigned long,	io_util				)
		__field( unsigned long,	bst_util			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pelt_boost		= pelt_boost;
		__entry->util			= util;
		__entry->io_util		= io_util;
		__entry->bst_util		= bst_util;
	),

	TP_printk("cpu=%d pelt_boost=%ld, util=%ld io_util=%ld bst_util=%ld",
		__entry->cpu, __entry->pelt_boost, __entry->util, __entry->io_util,
		__entry->bst_util)
);

TRACE_EVENT(ego_cpu_idle_ratio,

	TP_PROTO(int cpu, int flag, int idx, u64 s1, u64 s2, u64 s3, u64 s4, int last_idx),

	TP_ARGS(cpu, flag, idx, s1, s2, s3, s4, last_idx),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		flag				)
		__field( int,		idx				)
		__field( u64,		s1				)
		__field( u64,		s2				)
		__field( u64,		s3				)
		__field( u64,		s4				)
		__field( int,		last_idx			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->flag			= flag;
		__entry->idx			= idx;
		__entry->s1			= s1;
		__entry->s2			= s2;
		__entry->s3			= s3;
		__entry->s4			= s4;
		__entry->last_idx		= last_idx;
	),

	TP_printk("cpu=%d f=%2d(lidx=%d, %ld->%ld) idx=%d clkoff=%ld pwroff=%ld",
		__entry->cpu, __entry->flag, __entry->last_idx, __entry->s3, __entry->s4,
		__entry->idx, __entry->s1, __entry->s2)
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

/*
 * Tracepoint for util in ESG
 */
TRACE_EVENT(esg_cpu_util,

	TP_PROTO(int cpu, int nr_diff, int org_io_util, int io_util,
		int org_step_util, int step_util, int org_pelt_util, int pelt_util,
		int pelt_margin, int pelt_boost, int max, int util),

	TP_ARGS(cpu, nr_diff, org_io_util, io_util,
		org_step_util, step_util, org_pelt_util, pelt_util, pelt_margin, pelt_boost, max, util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		nr_diff				)
		__field( int,		org_io_util			)
		__field( int,		io_util				)
		__field( int,		org_step_util			)
		__field( int,		step_util			)
		__field( int,		org_pelt_util			)
		__field( int,		pelt_util			)
		__field( int,		pelt_margin			)
		__field( int,		pelt_boost			)
		__field( int,		max				)
		__field( int,		util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->nr_diff		= nr_diff;
		__entry->org_io_util		= org_io_util;
		__entry->io_util		= io_util;
		__entry->org_step_util		= org_step_util;
		__entry->step_util		= step_util;
		__entry->org_pelt_util		= org_pelt_util;
		__entry->pelt_util		= pelt_util;
		__entry->pelt_margin		= pelt_margin;
		__entry->pelt_boost		= pelt_boost;
		__entry->max			= max;
		__entry->util			= util;
	),

	TP_printk("cpu=%d nr_dif=%d, io_ut=%d->%d, step_ut=%d->%d pelt_ut=%d->%d(mg=%d boost=%d) max=%d, ut=%d",
			__entry->cpu, __entry->nr_diff,
			__entry->org_io_util, __entry->io_util,
			__entry->org_step_util, __entry->step_util,
			__entry->org_pelt_util, __entry->pelt_util,
			__entry->pelt_margin, __entry->pelt_boost,
			__entry->max, __entry->util)
);

/*
 * Tracepoint for slack timer func called
 */
TRACE_EVENT(cpufreq_gov_slack_func,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d SLACK EXPIRED", __entry->cpu)
);

/*
 * Tracepoint for detalis of slack timer func called
 */
TRACE_EVENT(cpufreq_gov_slack,

	TP_PROTO(int cpu, unsigned long util,
		unsigned long min, unsigned long action),

	TP_ARGS(cpu, util, min, action),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned long, min)
		__field(unsigned long, action)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->action = action;
	),

	TP_printk("cpu=%d util=%ld min=%ld action=%ld",
		__entry->cpu, __entry->util, __entry->min, __entry->action)
);

/*
 * Tracepoint for available cpus in FRT
 */
TRACE_EVENT(frt_available_cpus,

	TP_PROTO(unsigned int domain, int util_sum, int busy_thr, unsigned int available_mask),

	TP_ARGS(domain, util_sum, busy_thr, available_mask),

	TP_STRUCT__entry(
		__field(	unsigned int,	domain	)
		__field(	int,		util_sum	)
		__field(	int,		busy_thr	)
		__field(	unsigned int,	available_mask	)
	),

	TP_fast_assign(
		__entry->domain		= domain;
		__entry->util_sum	= util_sum;
		__entry->busy_thr	= busy_thr;
		__entry->available_mask	= available_mask;
	),

	TP_printk("domain=%x util_sum=%d busy_thr=%d available_mask=%x",
		__entry->domain, __entry->util_sum,
		__entry->busy_thr, __entry->available_mask)
);

/*
 * Tracepoint for multi load tracking
 */
TRACE_EVENT(mlt_update,

	TP_PROTO(struct mlt *mlt),

	TP_ARGS(mlt),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( int,	cur_period			)
		__field( int,	art_state			)
		__field( int,	cst_state			)
		__field( int,	art_g0_recent			)
		__field( int,	art_g1_recent			)
		__field( int,	art_g2_recent			)
		__field( int,	art_g3_recent			)
		__field( int,	art_g4_recent			)
		__field( int,	art_g5_recent			)
		__field( int,	art_g6_recent			)
		__field( int,	art_g7_recent			)
		__field( int,	art_g8_recent			)
		__field( int,	cst_c0_recent			)
		__field( int,	cst_c1_recent			)
		__field( int,	art_g0_last			)
		__field( int,	art_g1_last			)
		__field( int,	art_g2_last			)
		__field( int,	art_g3_last			)
		__field( int,	art_g4_last			)
		__field( int,	art_g5_last			)
		__field( int,	art_g6_last			)
		__field( int,	art_g7_last			)
		__field( int,	art_g8_last			)
		__field( int,	art_sum				)
		__field( int,	cst_c0_last			)
		__field( int,	cst_c1_last			)
		__field( int,	cst_sum				)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		__entry->cpu			= mlt->cpu;
		__entry->cur_period		= mlt->cur_period;
		__entry->art_state		= mlt->art.state;
		__entry->cst_state		= mlt->cst.state;
		__entry->art_g0_recent		= mlt->art.recent[0];
		__entry->art_g1_recent		= mlt->art.recent[1];
		__entry->art_g2_recent		= mlt->art.recent[2];
		__entry->art_g3_recent  	= mlt->art.recent[3];
		__entry->art_g4_recent  	= mlt->art.recent[4];
		__entry->art_g5_recent  	= mlt->art.recent[5];
		__entry->art_g6_recent		= mlt->art.recent[6];
		__entry->art_g7_recent		= mlt->art.recent[7];
		__entry->art_g8_recent		= mlt->art.recent[8];
		__entry->cst_c0_recent  	= mlt->cst.recent[0];
		__entry->cst_c1_recent  	= mlt->cst.recent[1];
		__entry->art_g0_last    	= mlt->art.periods[0][i];
		__entry->art_g1_last  		= mlt->art.periods[1][i];
		__entry->art_g2_last  		= mlt->art.periods[2][i];
		__entry->art_g3_last  		= mlt->art.periods[3][i];
		__entry->art_g4_last    	= mlt->art.periods[4][i];
		__entry->art_g5_last    	= mlt->art.periods[5][i];
		__entry->art_g6_last    	= mlt->art.periods[6][i];
		__entry->art_g7_last  		= mlt->art.periods[7][i];
		__entry->art_g8_last  		= mlt->art.periods[8][i];
		__entry->art_sum  		= mlt->art.periods[0][i] + mlt->art.periods[1][i] +
						  mlt->art.periods[2][i] + mlt->art.periods[3][i] +
						  mlt->art.periods[4][i] + mlt->art.periods[5][i] +
						  mlt->art.periods[6][i] + mlt->art.periods[7][i] +
						  mlt->art.periods[8][i];
		__entry->cst_c0_last  		= mlt->cst.periods[0][i];
		__entry->cst_c1_last  		= mlt->cst.periods[1][i];
		__entry->cst_sum		= mlt->cst.periods[0][i] + mlt->cst.periods[1][i];
	),

	TP_printk("cpu=%d cur_period=%d art_state=%d cst_state=%d "
			"art_recent=%d,%d,%d,%d,%d,%d,%d,%d,%d cst_recent=%d,%d "
			"art_last=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d) cst_last=%d,%d(%d)",
		__entry->cpu, __entry->cur_period, __entry->art_state, __entry->cst_state,
		__entry->art_g0_recent, __entry->art_g1_recent, __entry->art_g2_recent,
		__entry->art_g3_recent, __entry->art_g4_recent, __entry->art_g5_recent,
		__entry->art_g6_recent, __entry->art_g7_recent, __entry->art_g8_recent,
		__entry->cst_c0_recent, __entry->cst_c1_recent,
		__entry->art_g0_last, __entry->art_g1_last, __entry->art_g2_last,
		__entry->art_g3_last, __entry->art_g4_last, __entry->art_g5_last,
		__entry->art_g6_last, __entry->art_g7_last, __entry->art_g8_last, __entry->art_sum,
		__entry->cst_c0_last, __entry->cst_c1_last, __entry->cst_sum)
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
TRACE_EVENT(ecs_update_domain,

	TP_PROTO(char *move, unsigned int id,
		unsigned int util_sum, unsigned int threshold,
		unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(move, id, util_sum, threshold, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__array(	char,		move,	64		)
		__field(	unsigned int,	id			)
		__field(	unsigned int,	util_sum		)
		__field(	unsigned int,	threshold		)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		strncpy(__entry->move, move, 63);
		__entry->id			= id;
		__entry->util_sum		= util_sum;
		__entry->threshold		= threshold;
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("move=%s id=%d util_sum=%u threshold=%u cpus=%#x->%#x",
		__entry->move, __entry->id,
		__entry->util_sum, __entry->threshold,
		__entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for updating spared cpus with fastest cpus
 */
TRACE_EVENT(ecs_reflect_fastest_cpus,

	TP_PROTO(int heavy_task_count,
		 unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(heavy_task_count, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__field(		 int,	heavy_task_count	)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		__entry->heavy_task_count	= heavy_task_count;
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("heavy_task_count=%d cpus=%#x->%#x",
		  __entry->heavy_task_count,
		  __entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update_governor_cpus,

	TP_PROTO(char *reason, unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(reason, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__array(	char,		reason,		64	)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		strncpy(__entry->reason, reason, 63);
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("reason=%s cpus=%#x->%#x",
		  __entry->reason, __entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for frequency boost
 */
TRACE_EVENT(freqboost_boosted_util,

	TP_PROTO(int cpu, int ratio, unsigned long util, long margin, int type),

	TP_ARGS(cpu, ratio, util, margin, type),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( long,		margin			)
		__field( int,		type			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->ratio		= ratio;
		__entry->util		= util;
		__entry->margin		= margin;
		__entry->type		= type;
	),

	TP_printk("cpu=%d ratio=%d util=%lu margin=%ld type=%d",
		  __entry->cpu, __entry->ratio, __entry->util,
		  __entry->margin, __entry->type)
);

TRACE_EVENT(freqboost_htsk_boosted_util,

	TP_PROTO(int cpu, int ratio, unsigned long util, long margin),

	TP_ARGS(cpu, ratio, util, margin),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->ratio		= ratio;
		__entry->util		= util;
		__entry->margin		= margin;
	),

	TP_printk("cpu=%d ratio=%d util=%lu margin=%ld",
		  __entry->cpu, __entry->ratio, __entry->util,
		  __entry->margin)
);

/*
 * Tracepoint for wakeup boost
 */
TRACE_EVENT(wakeboost_boosted_util,

	TP_PROTO(int cpu, int ratio, unsigned long util, long margin),

	TP_ARGS(cpu, ratio, util, margin),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
		__entry->util			= util;
		__entry->margin			= margin;
	),

	TP_printk("cpu=%d ratio=%d util=%lu margin=%ld",
		  __entry->cpu, __entry->ratio, __entry->util, __entry->margin)
);

struct fclamp_data;

/*
 * Tracepoint for fclamp applying result
 */
TRACE_EVENT(fclamp_apply,

	TP_PROTO(int cpu, unsigned int orig_freq, unsigned int new_freq,
				int boost, struct fclamp_data *fcd),

	TP_ARGS(cpu, orig_freq, new_freq, boost, fcd),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned int,	orig_freq		)
		__field( unsigned int,	new_freq		)
		__field( int,		boost			)
		__field( int,		clamp_freq		)
		__field( int,		target_period		)
		__field( int,		target_ratio		)
		__field( int,		type			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->orig_freq	= orig_freq;
		__entry->new_freq	= new_freq;
		__entry->boost		= boost;
		__entry->clamp_freq	= fcd->freq;
		__entry->target_period	= fcd->target_period;
		__entry->target_ratio	= fcd->target_ratio;
		__entry->type		= fcd->type;
	),

	TP_printk("cpu=%d orig_freq=%u new_freq=%u boost=%d clamp_freq=%d "
		   "target_period=%d target_ratio=%d type=%s",
		  __entry->cpu, __entry->orig_freq, __entry->new_freq, __entry->boost,
		  __entry->clamp_freq, __entry->target_period, __entry->target_ratio,
		  __entry->type ? "MAX" : "MIN")
);

/*
 * Tracepoint for checking fclamp release
 */
TRACE_EVENT(fclamp_can_release,

	TP_PROTO(int cpu, int type, int period, int period_count,
			int target_ratio, int active_ratio),

	TP_ARGS(cpu, type, period, period_count, target_ratio, active_ratio),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		type			)
		__field( int,		period			)
		__field( int,		period_count		)
		__field( int,		target_ratio		)
		__field( int,		active_ratio		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->type		= type;
		__entry->period		= period;
		__entry->period_count	= period_count;
		__entry->target_ratio	= target_ratio;
		__entry->active_ratio	= active_ratio;
	),

	TP_printk("cpu=%d type=%s period=%d period_count=%d target_ratio=%d active_ratio=%d",
		  __entry->cpu, __entry->type ? "MAX" : "MIN",
		  __entry->period, __entry->period_count,
		  __entry->target_ratio, __entry->active_ratio)
);

/*
 * Tracepoint for cfs_rq load tracking:
 */
TRACE_EVENT(sched_load_cfs_rq,

	TP_PROTO(struct cfs_rq *cfs_rq),

	TP_ARGS(cfs_rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  load                    )
		__field(        unsigned long,  rbl_load                )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		struct rq *rq = __trace_get_rq(cfs_rq);

		__entry->cpu            = rq->cpu;
		__entry->load           = cfs_rq->avg.load_avg;
		__entry->rbl_load       = cfs_rq->avg.runnable_avg;
		__entry->util           = cfs_rq->avg.util_avg;
	),

	TP_printk("cpu=%d load=%lu rbl_load=%lu util=%lu",
			__entry->cpu, __entry->load,
			__entry->rbl_load,__entry->util)
);

/*
 * Tracepoint for rt_rq load tracking:
 */
TRACE_EVENT(sched_load_rt_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_rt.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

/*
 * Tracepoint for dl_rq load tracking:
 */
TRACE_EVENT(sched_load_dl_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_dl.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
/*
 * Tracepoint for irq load tracking:
 */
TRACE_EVENT(sched_load_irq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_irq.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
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
 * Tracepoint for sched_entity load tracking:
 */
TRACE_EVENT(sched_load_se,

	TP_PROTO(struct sched_entity *se),

	TP_ARGS(se),

	TP_STRUCT__entry(
		__field(        int,            cpu                           )
		__array(        char,           comm,   TASK_COMM_LEN         )
		__field(        pid_t,          pid                           )
		__field(        unsigned long,  load                          )
		__field(        unsigned long,  rbl_load                      )
		__field(        unsigned long,  util                          )
	),

	TP_fast_assign(
#ifdef CONFIG_FAIR_GROUP_SCHED
		struct cfs_rq *gcfs_rq = se->my_q;
#else
		struct cfs_rq *gcfs_rq = NULL;
#endif
		struct task_struct *p = gcfs_rq ? NULL
				: container_of(se, struct task_struct, se);
		struct rq *rq = gcfs_rq ? __trace_get_rq(gcfs_rq) : NULL;

		__entry->cpu            = rq ? rq->cpu
					: task_cpu((container_of(se, struct task_struct, se)));
		memcpy(__entry->comm, p ? p->comm : "(null)",
			p ? TASK_COMM_LEN : sizeof("(null)"));
		__entry->pid = p ? p->pid : -1;
		__entry->load = se->avg.load_avg;
		__entry->rbl_load = se->avg.runnable_avg;
		__entry->util = se->avg.util_avg;
	),

	TP_printk("cpu=%d comm=%s pid=%d load=%lu rbl_load=%lu util=%lu",
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
		__field( int,  overutilized    )
	),

	TP_fast_assign(
		__entry->overutilized   = overutilized;
	),

	TP_printk("overutilized=%d",
		__entry->overutilized)
);

TRACE_EVENT(gsc_decision_activate_result,

	TP_PROTO(int gid, unsigned long group_util, u64 now, u64 last_update_time, char *label),

	TP_ARGS(gid, group_util, now, last_update_time, label),

	TP_STRUCT__entry(
		__field( int,			gid			)
		__field( unsigned long,		group_util		)
		__field( u64,			now			)
		__field( u64,			last_update_time	)
		__array( char,			label,	64		)
	),

	TP_fast_assign(
		__entry->gid			= gid;
		__entry->group_util		= group_util;
		__entry->now			= now;
		__entry->last_update_time	= last_update_time;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("group_id = %d group_util=%lu now=%llu last_uptime=%llu reason=%s",
		__entry->gid,  __entry->group_util, __entry->now,
		__entry->last_update_time, __entry->label)
);

TRACE_EVENT(gsc_task_cgroup_attach,

	TP_PROTO(struct task_struct *p, unsigned int gid, int ret),

	TP_ARGS(p, gid, ret),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field( unsigned int,		gid			)
		__field( int,			ret			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->gid		= gid;
		__entry->ret		= ret;
	),

	TP_printk("comm=%s pid=%d gid=%d ret=%d",
		__entry->comm, __entry->pid, __entry->gid, __entry->ret)
);

TRACE_EVENT(gsc_flush_task,

	TP_PROTO(struct task_struct *p, int ret),

	TP_ARGS(p, ret),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field( int,			ret			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->ret		= ret;
	),

	TP_printk("comm=%s pid=%d ret=%d",
		__entry->comm, __entry->pid, __entry->ret)
);

/*
 * Tracepoint for find target cpu
 */
TRACE_EVENT(ems_find_target_cpu,

	TP_PROTO(int cpu, unsigned long util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned long,		util			)
		__field( int,		nr_running			)
		__field( int,		cfs_nr_running		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->cfs_nr_running	= cpu_rq(cpu)->cfs.h_nr_running;
	),

	TP_printk("cpu=%d util=%lu nr_running=%d cfs_nr_running=%d",
		__entry->cpu, __entry->util, __entry->nr_running, __entry->cfs_nr_running)
);

/*
 * Tracepoint for newidle balance
 */
TRACE_EVENT(ems_newidle_balance,

	TP_PROTO(int cpu, int target_cpu, int pulled),

	TP_ARGS(cpu, target_cpu, pulled),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		target_cpu			)
		__field( int,		pulled		)
		__field( int,		nr_running		)
		__field( int,		rt_nr_running		)
		__field( int,		cfs_nr_running		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->pulled		= pulled;
		__entry->target_cpu		= target_cpu;
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->rt_nr_running		= cpu_rq(cpu)->rt.rt_nr_running;
		__entry->cfs_nr_running	= cpu_rq(cpu)->cfs.h_nr_running;
	),

	TP_printk("cpu=%d pulled=%d target_cpu=%d nr_running=%d rt_nr_running=%d cfs_nr_running=%d",
		__entry->cpu, __entry->pulled, __entry->target_cpu, __entry->nr_running,
		__entry->rt_nr_running, __entry->cfs_nr_running)
);
#endif /* _TRACE_EMS_DEBUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
