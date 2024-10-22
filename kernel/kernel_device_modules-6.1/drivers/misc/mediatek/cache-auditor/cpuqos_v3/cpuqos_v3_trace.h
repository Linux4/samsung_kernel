/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuqos_v3

#if !defined(_CPUQOS_V3_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CPUQOS_V3_TRACE_H
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for per-cpu pd
 */
TRACE_EVENT(cpuqos_cpu_pd,

	TP_PROTO(int cpu, int pid, int css_id,
		int old_pd, int new_pd,
		int rank, int cpuqos_perf_mode),

	TP_ARGS(cpu, pid, css_id,
		old_pd, new_pd,
		rank, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, css_id)
		__field(int, old_pd)
		__field(int, new_pd)
		__field(int, rank)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->pid		= pid;
		__entry->css_id		= css_id;
		__entry->old_pd		= old_pd;
		__entry->new_pd		= new_pd;
		__entry->rank		= rank;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("cpu=%d, p=%d, css_id=%d, old_part=%d, new_part=%d, rank=%d, mode=%d",
		__entry->cpu, __entry->pid, __entry->css_id,
		__entry->old_pd, __entry->new_pd,
		__entry->rank, __entry->cpuqos_perf_mode)
);

TRACE_EVENT(cpuqos_set_group_pd,

	TP_PROTO(int group_id, int css_id, int set,
		int old_pd, int new_pd,
		int cpuqos_perf_mode),

	TP_ARGS(group_id, css_id, set,
		old_pd, new_pd, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, group_id)
		__field(int, css_id)
		__field(int, set)
		__field(int, old_pd)
		__field(int, new_pd)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->group_id	= group_id;
		__entry->css_id		= css_id;
		__entry->set		= set;
		__entry->old_pd		= old_pd;
		__entry->new_pd		= new_pd;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("group_id=%d, css_id=%d, set=%d, old_pd=%d, new_pd=%d, mode=%d",
		__entry->group_id, __entry->css_id, __entry->set,
		__entry->old_pd, __entry->new_pd,
		__entry->cpuqos_perf_mode)

);

TRACE_EVENT(cpuqos_set_task_pd,

	TP_PROTO(int pid, int css_id, int set,
		int old_pd, int new_pd,
		int rank, int cpuqos_perf_mode),

	TP_ARGS(pid, css_id, set, old_pd, new_pd,
		rank, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, css_id)
		__field(int, set)
		__field(int, old_pd)
		__field(int, new_pd)
		__field(int, rank)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->pid		= pid;
		__entry->css_id		= css_id;
		__entry->set		= set;
		__entry->old_pd		= old_pd;
		__entry->new_pd		= new_pd;
		__entry->rank		= rank;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("p=%d, css_id=%d, set=%d, old_pd=%d, new_pd=%d, rank=%d, mode=%d",
		__entry->pid, __entry->css_id, __entry->set,
		__entry->old_pd, __entry->new_pd,
		__entry->rank, __entry->cpuqos_perf_mode)

);

TRACE_EVENT(cpuqos_set_cpuqos_mode,

	TP_PROTO(int cpuqos_perf_mode),

	TP_ARGS(cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("cpuqos_mode=%d", __entry->cpuqos_perf_mode)

);

TRACE_EVENT(cpuqos_debug_info,

	TP_PROTO(int cpuqos_perf_mode,
		u32 slice,
		u32 portion
	),

	TP_ARGS(cpuqos_perf_mode,
		slice,
		portion
	),

	TP_STRUCT__entry(
		__field(int, cpuqos_perf_mode)
		__field(u32, slice)
		__field(u32, portion)
	),

	TP_fast_assign(
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
		__entry->slice = slice;
		__entry->portion = portion;
	),

	TP_printk("cpuqos_mode=%d, slices=%u, cache way mode=%u",
		__entry->cpuqos_perf_mode,
		__entry->slice,
		__entry->portion
	)

);


#endif /* _CPUQOS_V3_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE cpuqos_v3_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
