/*
 *  Copyright (C) 2023 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mhdvfs

#if !defined(_TRACE_MHDVFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MHDVFS_H

#include <linux/tracepoint.h>

/*
 * Tracepoint for uarch information profile
 */
TRACE_EVENT(mhdvfs_profile,

	TP_PROTO(int ipc_sum, int mspc_sum, int mpi, int mpi_crit,
		int max_actual_ipc, int max_ipc, int max_ipc_ar),

	TP_ARGS(ipc_sum, mspc_sum, mpi, mpi_crit, max_actual_ipc, max_ipc, max_ipc_ar),

	TP_STRUCT__entry(
		__field(	int,		ipc_sum			)
		__field(	int,		mspc_sum		)
		__field(	int,		mpi			)
		__field(	int,		mpi_crit		)
		__field(	int,		max_actual_ipc		)
		__field(	int,		max_ipc			)
		__field(	int,		max_ipc_ar		)
	),

	TP_fast_assign(
		__entry->ipc_sum		= ipc_sum;
		__entry->mspc_sum		= mspc_sum;
		__entry->mpi			= mpi;
		__entry->mpi_crit		= mpi_crit;
		__entry->max_actual_ipc		= max_actual_ipc;
		__entry->max_ipc		= max_ipc;
		__entry->max_ipc_ar		= max_ipc_ar;
	),

	TP_printk("ipc_sum=%d mspc_sum=%d mpi=%d mpi_crit=%d max_actual_ipc=%d max_ipc=%d max_ipc_ar=%d",
		  __entry->ipc_sum, __entry->mspc_sum, __entry->mpi, __entry->mpi_crit,
		  __entry->max_actual_ipc, __entry->max_ipc, __entry->max_ipc_ar)
);

/*
 * Tracepoint for kicking dvfs driver
 */
TRACE_EVENT(mhdvfs_kick,

	TP_PROTO(int dsu_level, int mif_level, int cpu_level),

	TP_ARGS(dsu_level, mif_level, cpu_level),

	TP_STRUCT__entry(
		__field(	int,		dsu_level		)
		__field(	int,		mif_level		)
		__field(	int,		cpu_level		)
	),

	TP_fast_assign(
		__entry->dsu_level		= dsu_level;
		__entry->mif_level		= mif_level;
		__entry->cpu_level		= cpu_level;
	),

	TP_printk("dsu_level=%d mif_level=%d cpu_level=%d",
		__entry->dsu_level, __entry->mif_level, __entry->cpu_level)
);

/*
 * Tracepoint for next level selection
 */
TRACE_EVENT(mhdvfs_next_level,

	TP_PROTO(int id, int cur_level, int next_level, int mpi, int mpi_crit,
			int ipc, int ipc_crit, int sw_ipc, int sw_ipc_crit),

	TP_ARGS(id, cur_level, next_level, mpi, mpi_crit,
			ipc, ipc_crit, sw_ipc, sw_ipc_crit),

	TP_STRUCT__entry(
		__field(	int,		id			)
		__field(	int,		cur_level		)
		__field(	int,		next_level		)
		__field(	int,		mpi			)
		__field(	int,		mpi_crit		)
		__field(	int,		ipc			)
		__field(	int,		ipc_crit		)
		__field(	int,		sw_ipc			)
		__field(	int,		sw_ipc_crit		)
	),

	TP_fast_assign(
		__entry->id			= id;
		__entry->cur_level		= cur_level;
		__entry->next_level		= next_level;
		__entry->mpi			= mpi;
		__entry->mpi_crit		= mpi_crit;
		__entry->ipc			= ipc;
		__entry->ipc_crit		= ipc_crit;
		__entry->sw_ipc			= sw_ipc;
		__entry->sw_ipc_crit		= sw_ipc_crit;
	),

	TP_printk("id=%d cur_level=%d next_level=%d mpi=%d mpi_crit=%d "
		  "ipc=%d ipc_crit=%d sw_ipc=%d sw_ipc_crit=%d",
		__entry->id, __entry->cur_level, __entry->next_level, __entry->mpi,
		__entry->mpi_crit, __entry->ipc, __entry->ipc_crit, __entry->sw_ipc,
		__entry->sw_ipc_crit)
);

/*
 * Tracepoint for change freq
 */
TRACE_EVENT(mhdvfs_change_freq,

	TP_PROTO(int id, int base_freq, int req_freq, int adjust_level),

	TP_ARGS(id, base_freq, req_freq, adjust_level),

	TP_STRUCT__entry(
		__field(	int,		id		)
		__field(	int,		base_freq	)
		__field(	int,		req_freq	)
		__field(	int,		adjust_level	)
	),

	TP_fast_assign(
		__entry->id		= id;
		__entry->base_freq	= base_freq;
		__entry->req_freq	= req_freq;
		__entry->adjust_level	= adjust_level;
	),

	TP_printk("id=%d base_freq=%d req_freq=%d adjust_level=%d",
		__entry->id, __entry->base_freq,
		__entry->req_freq, __entry->adjust_level)
);

/*
 * Tracepoint for update frequency info
 */
TRACE_EVENT(mhdvfs_update_freq_info,

	TP_PROTO(int id, unsigned int avg_freq, unsigned int max_freq),

	TP_ARGS(id, avg_freq, max_freq),

	TP_STRUCT__entry(
		__field(	int,		id		)
		__field(	unsigned int,	avg_freq	)
		__field(	unsigned int,	max_freq	)
	),

	TP_fast_assign(
		__entry->id		= id;
		__entry->avg_freq	= avg_freq;
		__entry->max_freq	= max_freq;
	),

	TP_printk("id=%d avg_freq=%u max_freq=%u",
		__entry->id, __entry->avg_freq, __entry->max_freq)
);

/*
 * Tracepoint for frequency transition notifier
 */
TRACE_EVENT(mhdvfs_transition_notifier,

	TP_PROTO(int id, unsigned int freq),

	TP_ARGS(id, freq),

	TP_STRUCT__entry(
		__field(	int,		id	)
		__field(	unsigned int,	freq	)
	),

	TP_fast_assign(
		__entry->id		= id;
		__entry->freq		= freq;
	),

	TP_printk("id=%d freq=%u", __entry->id, __entry->freq)
);

TRACE_EVENT(tlb_check_tg_busy,

	TP_PROTO(struct task_struct *gl, unsigned long util_sum,
			unsigned long util_avg, int task_count),

	TP_ARGS(gl, util_sum, util_avg, task_count),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid		)
		__field(	unsigned long,	util_sum	)
		__field(	unsigned long,	util_avg	)
		__field(	int,		task_count	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, gl->comm, TASK_COMM_LEN);
		__entry->pid		= gl->pid;
		__entry->util_sum	= util_sum;
		__entry->util_avg	= util_avg;
		__entry->task_count	= task_count;
	),

	TP_printk("comm=%s pid=%d util=%lu util_avg=%lu task_count=%d",
		__entry->comm, __entry->pid, __entry->util_sum,
		__entry->util_avg, __entry->task_count)
);
#endif /* _TRACE_MHDVFS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
