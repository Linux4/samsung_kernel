/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ged

#if !defined(_TRACE_GED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GED_H

#include <linux/tracepoint.h>

/* common tracepoints */
TRACE_EVENT(tracing_mark_write,

	TP_PROTO(int pid, const char *name, long long value),

	TP_ARGS(pid, name, value),

	TP_STRUCT__entry(
		__field(int, pid)
		__string(name, name)
		__field(long long, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),

	TP_printk("C|%d|%s|%lld", __entry->pid, __get_str(name), __entry->value)
);

#endif /* _TRACE_GED_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/mediatek/ged/include
#define TRACE_INCLUDE_FILE ged_tracepoint
#include <trace/define_trace.h>
