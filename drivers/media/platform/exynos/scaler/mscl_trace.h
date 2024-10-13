/* SPDX-License-Identifier: GPL-2.0 */
/*
 * M2M Scaler trace support
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mscl

#if !defined(_MSCL_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_mark_write,
	TP_PROTO(char type, int pid, const char *name, int value),
	TP_ARGS(type, pid, name, value),
	TP_STRUCT__entry(
		__field(char, type)
		__field(int, pid)
		__string(name, name)
		__field(int, value)
	),
	TP_fast_assign(
		__entry->type = type;
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),
	TP_printk("%c|%d|%s|%d",
		__entry->type, __entry->pid, __get_str(name), __entry->value)
);

#endif /* _MSCL_TRACE_H_ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mscl_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
