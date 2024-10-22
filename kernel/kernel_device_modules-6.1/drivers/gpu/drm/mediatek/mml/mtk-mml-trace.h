/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */
#if IS_ENABLED(CONFIG_FTRACE)

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mml

#if !defined(_MTK_MML_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_MML_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_mark_write,

	TP_PROTO(char *str),

	TP_ARGS(str),

	TP_STRUCT__entry(
		__array(char, s, MML_TRACE_MSG_LEN)
	),

	TP_fast_assign(
		memcpy(__entry->s, str, MML_TRACE_MSG_LEN);
	),

	TP_printk("%s", __entry->s)
);

#endif /* _MTK_MML_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/drm/mediatek/mml
#define TRACE_INCLUDE_FILE mtk-mml-trace
#include <trace/define_trace.h>

#endif /* CONFIG_FTRACE */
