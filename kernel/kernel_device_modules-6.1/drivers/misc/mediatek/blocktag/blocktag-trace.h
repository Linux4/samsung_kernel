/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM blocktag

#if !defined(_TRACE_BLOCKTAG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BLOCKTAG_H

#include <linux/tracepoint.h>
#include "blocktag-internal.h"

TRACE_EVENT(blocktag_ufs_send,
	TP_PROTO(__u64 delay, __u32 tid, __u16 qid, __u16 cmd, __u32 len),

	TP_ARGS(delay, tid, qid, cmd, len),

	TP_STRUCT__entry(
		__field(__u64, delay)
		__field(__u32, tid)
		__field(__u16, qid)
		__field(__u16, cmd)
		__field(__u32, len)
	),

	TP_fast_assign(
		__entry->delay  = delay;
		__entry->tid    = tid;
		__entry->qid    = qid;
		__entry->cmd    = cmd;
		__entry->len    = len;
	),

	TP_printk("tid=%u qid=%u cmd=0x%02x len=%u delay=%llu",
		__entry->tid, __entry->qid,
		__entry->cmd, __entry->len,
		__entry->delay)
);

TRACE_EVENT(blocktag_ufs_complete,
	TP_PROTO(__u64 delay, __u32 tid, __u16 qid),

	TP_ARGS(delay, tid, qid),

	TP_STRUCT__entry(
		__field(__u64, delay)
		__field(__u32, tid)
		__field(__u16, qid)
	),

	TP_fast_assign(
		__entry->delay  = delay;
		__entry->tid    = tid;
		__entry->qid    = qid;
	),

	TP_printk("tid=%u qid=%u delay=%llu",
		__entry->tid, __entry->qid, __entry->delay)
);

TRACE_EVENT(blocktag_mictx_get_data,
	TP_PROTO(const char *name, struct mtk_btag_mictx_iostat_struct *iostat),

	TP_ARGS(name, iostat),

	TP_STRUCT__entry(
		__string(name, name)
		__field(__u64, duration)
		__field(__u32, tp_req_r)
		__field(__u32, tp_req_w)
		__field(__u32, tp_all_r)
		__field(__u32, tp_all_w)
		__field(__u32, reqsize_r)
		__field(__u32, reqsize_w)
		__field(__u32, reqcnt_r)
		__field(__u32, reqcnt_w)
		__field(__u16, wl)
		__field(__u16, top)
		__field(__u16, q_depth)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->duration  = iostat->duration;
		__entry->tp_req_r  = iostat->tp_req_r;
		__entry->tp_req_w  = iostat->tp_req_w;
		__entry->tp_all_r  = iostat->tp_all_r;
		__entry->tp_all_w  = iostat->tp_all_w;
		__entry->reqsize_r = iostat->reqsize_r;
		__entry->reqsize_w = iostat->reqsize_w;
		__entry->reqcnt_r  = iostat->reqcnt_r;
		__entry->reqcnt_w  = iostat->reqcnt_w;
		__entry->wl        = iostat->wl;
		__entry->top       = iostat->top;
		__entry->q_depth   = iostat->q_depth;
	),

	TP_printk("[%s] dur=%llu wl=%u top=%u qd=%u req_size=%u,%u req_cnt=%u,%u tp_req=%u,%u tp_all=%u,%u",
		__get_str(name), __entry->duration,
		__entry->wl, __entry->top, __entry->q_depth,
		__entry->reqsize_r, __entry->reqsize_w,
		__entry->reqcnt_r, __entry->reqcnt_w,
		__entry->tp_req_r, __entry->tp_req_w,
		__entry->tp_all_r, __entry->tp_all_w)
);

#endif /*_TRACE_BLOCKTAG_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE blocktag-trace

/* This part must be outside protection */
#include <trace/define_trace.h>
