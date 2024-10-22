/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_timer

#if !defined(_TRACE_EVENT_MTK_TIMER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_MTK_TIMER_H

#include <linux/tracepoint.h>

TRACE_EVENT(timer_set_next_event,
	TP_PROTO(unsigned long ticks),

	TP_ARGS(ticks),

	TP_STRUCT__entry(
		__field(unsigned long, ticks)
	),

	TP_fast_assign(
		__entry->ticks = ticks;
	),

	TP_printk("mtk_timer: next_event_ticks=%lu", __entry->ticks)
);

#endif /* _TRACE_EVENT_MTK_TIMER_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE timer-mediatek-trace
#include <trace/define_trace.h>
