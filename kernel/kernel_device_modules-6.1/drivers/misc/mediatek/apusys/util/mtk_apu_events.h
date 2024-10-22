/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_FTRACE)

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_apu_events

#if !defined(__TRACE_MTK_APU_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_MTK_APU_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_mark_write,
	TP_PROTO(char *s),
	TP_ARGS(s),
	TP_STRUCT__entry(
	__array(char, s, TRACE_LEN)
	),
	TP_fast_assign(
	if (snprintf(__entry->s, TRACE_LEN, "%s", s) < 0)
		__entry->s[0] = '\0';
	),
	TP_printk("%s", __entry->s)
);

#endif //__TRACE_APUSYS_EVENTS_H

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_apu_events
#include <trace/define_trace.h>

#endif
