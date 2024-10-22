/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmqos_events

#if !defined(_TRACE_MMQOS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMQOS_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmqos__larb_port_avg_bw,
	TP_PROTO(const char *r_w_type, const char *dev_name, int larb, int port, int avg_bw,
			int comm, int chn),
	TP_ARGS(r_w_type, dev_name, larb, port, avg_bw, comm, chn),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__string(dev_name, dev_name)
		__field(int, larb)
		__field(int, port)
		__field(int, avg_bw)
		__field(int, comm)
		__field(int, chn)
	),
	TP_fast_assign(
		__assign_str(r_w_type, r_w_type);
		__assign_str(dev_name, dev_name);
		__entry->larb = larb;
		__entry->port = port;
		__entry->avg_bw = avg_bw;
		__entry->comm = comm;
		__entry->chn = chn;
	),
	TP_printk("%s_%d_%s_comm%d_chn%d=%d",
		__get_str(dev_name),
		(int)__entry->port,
		__get_str(r_w_type),
		(int)__entry->comm,
		(int)__entry->chn,
		(int)__entry->avg_bw)
);
TRACE_EVENT(mmqos__larb_port_peak_bw,
	TP_PROTO(const char *r_w_type, const char *dev_name, int larb, int port, int peak_bw,
			int comm, int chn),
	TP_ARGS(r_w_type, dev_name, larb, port, peak_bw, comm, chn),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__string(dev_name, dev_name)
		__field(int, larb)
		__field(int, port)
		__field(int, peak_bw)
		__field(int, comm)
		__field(int, chn)
	),
	TP_fast_assign(
		__assign_str(r_w_type, r_w_type);
		__assign_str(dev_name, dev_name);
		__entry->larb = larb;
		__entry->port = port;
		__entry->peak_bw = peak_bw;
		__entry->comm = comm;
		__entry->chn = chn;
	),
	TP_printk("%s_%d_%s_comm%d_chn%d=%d",
		__get_str(dev_name),
		(int)__entry->port,
		__get_str(r_w_type),
		(int)__entry->comm,
		(int)__entry->chn,
		(int)__entry->peak_bw)
);
TRACE_EVENT(mmqos__larb_avg_bw,
	TP_PROTO(const char *r_w_type, const char *dev_name, int bw,
			int comm, int chn),
	TP_ARGS(r_w_type, dev_name, bw, comm, chn),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__string(dev_name, dev_name)
		__field(int, bw)
		__field(int, comm)
		__field(int, chn)
	),
	TP_fast_assign(
		__assign_str(r_w_type, r_w_type);
		__assign_str(dev_name, dev_name);
		__entry->bw = bw;
		__entry->comm = comm;
		__entry->chn = chn;
	),
	TP_printk("%s_%s_comm%d_chn%d=%d",
		__get_str(dev_name),
		__get_str(r_w_type),
		(int)__entry->comm,
		(int)__entry->chn,
		(int)__entry->bw)
);
TRACE_EVENT(mmqos__larb_peak_bw,
	TP_PROTO(const char *r_w_type, const char *dev_name, int bw,
			int comm, int chn),
	TP_ARGS(r_w_type, dev_name, bw, comm, chn),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__string(dev_name, dev_name)
		__field(int, bw)
		__field(int, comm)
		__field(int, chn)
	),
	TP_fast_assign(
		__assign_str(r_w_type, r_w_type);
		__assign_str(dev_name, dev_name);
		__entry->bw = bw;
		__entry->comm = comm;
		__entry->chn = chn;
	),
	TP_printk("%s_%s_comm%d_chn%d=%d",
		__get_str(dev_name),
		__get_str(r_w_type),
		(int)__entry->comm,
		(int)__entry->chn,
		(int)__entry->bw)
);

#define TYPE_IS_ON		0
#define TYPE_IS_OFF		1
#define TYPE_IS_VCP		2
#define TYPE_IS_URATE	3
#define TYPE_IS_MAX		4
#define TYPE_IS_MAX_URATE		5

TRACE_EVENT(mmqos__chn_bw,
	TP_PROTO(int comm_id, int chn_id, int s_r, int s_w, int h_r, int h_w, int max, int type, int type_max),
	TP_ARGS(comm_id, chn_id, s_r, s_w, h_r, h_w, max, type, type_max),
	TP_STRUCT__entry(
		__field(int, comm_id)
		__field(int, chn_id)
		__field(int, s_r)
		__field(int, s_w)
		__field(int, h_r)
		__field(int, h_w)
		__field(int, max)
		__field(int, type)
		__field(int, type_max)
	),
	TP_fast_assign(
		__entry->comm_id = comm_id;
		__entry->chn_id = chn_id;
		__entry->s_r = s_r;
		__entry->s_w = s_w;
		__entry->h_r = h_r;
		__entry->h_w = h_w;
		__entry->max = max;
		__entry->type = type;
		__entry->type_max = type_max;
	),

	TP_printk("%s_comm%d_%d_s_r=%d, %s_comm%d_%d_s_w=%d, %s_comm%d_%d_h_r=%d, %s_comm%d_%d_h_w=%d, %s_comm%d_%d=%d",
		(int)__entry->type == 0 ? "on" : ((int)__entry->type == 1 ? "off" :
			((int)__entry->type == 2 ? "vcp" : "urate")),
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->s_r,
		(int)__entry->type == 0 ? "on" : ((int)__entry->type == 1 ? "off" :
			((int)__entry->type == 2 ? "vcp" : "urate")),
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->s_w,
		(int)__entry->type == 0 ? "on" : ((int)__entry->type == 1 ? "off" :
			((int)__entry->type == 2 ? "vcp" : "urate")),
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->h_r,
		(int)__entry->type == 0 ? "on" : ((int)__entry->type == 1 ? "off" :
			((int)__entry->type == 2 ? "vcp" : "urate")),
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->h_w,
		((int)__entry->type_max == 4 ? "max" : "max_urate"),
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->max)
);
TRACE_EVENT(mmqos__bw_to_emi,
	TP_PROTO(int comm_id, int avg_bw, int peak_bw),
	TP_ARGS(comm_id, avg_bw, peak_bw),
	TP_STRUCT__entry(
		__field(int, comm_id)
		__field(int, avg_bw)
		__field(int, peak_bw)
	),
	TP_fast_assign(
		__entry->comm_id = comm_id;
		__entry->avg_bw = avg_bw;
		__entry->peak_bw = peak_bw;
	),
	TP_printk("%s_comm%d_avg=%d, %s_comm%d_peak=%d",
		((int)__entry->comm_id > 1 ? "vcp" : "ap"),
		(int)__entry->comm_id,
		(int)__entry->avg_bw,
		((int)__entry->comm_id > 1 ? "vcp" : "ap"),
		(int)__entry->comm_id,
		(int)__entry->peak_bw)
);
#endif /* _TRACE_MMQOS_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mmqos_events

/* This part must be outside protection */
#include <trace/define_trace.h>
