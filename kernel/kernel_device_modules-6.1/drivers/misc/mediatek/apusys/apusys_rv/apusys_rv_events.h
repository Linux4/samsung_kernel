/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apusys_rv_events
#if !defined(__APUSYS_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __APUSYS_RV_EVENTS_H__
#include <linux/tracepoint.h>

#define APUSYS_RV_TAG_IPI_SEND_PRINT \
	"ipi_send:id=%d,len=%d,serial_no=%d,csum=0x%x,user_id=0x%x,usage_cnt=%d,elapse=%llu"
#define APUSYS_RV_TAG_IPI_HANDLE_PRINT \
	"ipi_handle:id=%d,len=%d,serial_no=%d,csum=0x%x,user_id=0x%x,usage_cnt=%d," \
	"top_start_time=%llu,bottom_start_time=%llu,latency=%llu,elapse=%llu,t_handler=%llu"
#define APUSYS_RV_TAG_PWR_CTRL_PRINT \
	"pwr_ctrl:id=%d,on=%d,off=%d,latency=%llu," \
	"sub_lat0=%llu,sub_lat1=%llu,sub_lat2=%llu," \
	"sub_lat3=%llu,sub_lat4=%llu,sub_lat5=%llu," \
	"sub_lat6=%llu,sub_lat7=%llu"

TRACE_EVENT(apusys_rv_ipi_send,
	TP_PROTO(unsigned int id,
			unsigned int len,
			unsigned int serial_no,
			unsigned int csum,
			unsigned int user_id,
			int usage_cnt,
			uint64_t elapse
		),
	TP_ARGS(id, len, serial_no, csum, user_id, usage_cnt, elapse
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, len)
		__field(unsigned int, serial_no)
		__field(unsigned int, csum)
		__field(unsigned int, user_id)
		__field(int, usage_cnt)
		__field(uint64_t, elapse)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->len = len;
		__entry->serial_no = serial_no;
		__entry->csum = csum;
		__entry->user_id = user_id;
		__entry->usage_cnt = usage_cnt;
		__entry->elapse = elapse;
	),
	TP_printk(
		APUSYS_RV_TAG_IPI_SEND_PRINT,
		__entry->id,
		__entry->len,
		__entry->serial_no,
		__entry->csum,
		__entry->user_id,
		__entry->usage_cnt,
		__entry->elapse
	)
);

TRACE_EVENT(apusys_rv_ipi_handle,
	TP_PROTO(unsigned int id,
			unsigned int len,
			unsigned int serial_no,
			unsigned int csum,
			unsigned int user_id,
			int usage_cnt,
			uint64_t top_start_time,
			uint64_t bottom_start_time,
			uint64_t latency,
			uint64_t elapse,
			uint64_t t_handler
		),
	TP_ARGS(id, len, serial_no, csum, user_id, usage_cnt, top_start_time,
		bottom_start_time, latency, elapse, t_handler
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, len)
		__field(unsigned int, serial_no)
		__field(unsigned int, csum)
		__field(unsigned int, user_id)
		__field(int, usage_cnt)
		__field(uint64_t, top_start_time)
		__field(uint64_t, bottom_start_time)
		__field(uint64_t, latency)
		__field(uint64_t, elapse)
		__field(uint64_t, t_handler)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->len = len;
		__entry->serial_no = serial_no;
		__entry->csum = csum;
		__entry->user_id = user_id;
		__entry->usage_cnt = usage_cnt;
		__entry->top_start_time = top_start_time;
		__entry->bottom_start_time = bottom_start_time;
		__entry->latency = latency;
		__entry->elapse = elapse;
		__entry->t_handler = t_handler;
	),
	TP_printk(
		APUSYS_RV_TAG_IPI_HANDLE_PRINT,
		__entry->id,
		__entry->len,
		__entry->serial_no,
		__entry->csum,
		__entry->user_id,
		__entry->usage_cnt,
		__entry->top_start_time,
		__entry->bottom_start_time,
		__entry->latency,
		__entry->elapse,
		__entry->t_handler
	)
);

TRACE_EVENT(apusys_rv_pwr_ctrl,
	TP_PROTO(unsigned int id,
			unsigned int on,
			unsigned int off,
			uint64_t latency,
			uint64_t sub_latency_0,
			uint64_t sub_latency_1,
			uint64_t sub_latency_2,
			uint64_t sub_latency_3,
			uint64_t sub_latency_4,
			uint64_t sub_latency_5,
			uint64_t sub_latency_6,
			uint64_t sub_latency_7
		),
	TP_ARGS(id, on, off, latency,
		sub_latency_0, sub_latency_1, sub_latency_2,
		sub_latency_3, sub_latency_4, sub_latency_5,
		sub_latency_6, sub_latency_7
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, on)
		__field(unsigned int, off)
		__field(uint64_t, latency)
		__field(uint64_t, sub_latency_0)
		__field(uint64_t, sub_latency_1)
		__field(uint64_t, sub_latency_2)
		__field(uint64_t, sub_latency_3)
		__field(uint64_t, sub_latency_4)
		__field(uint64_t, sub_latency_5)
		__field(uint64_t, sub_latency_6)
		__field(uint64_t, sub_latency_7)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->on = on;
		__entry->off = off;
		__entry->latency = latency;
		__entry->sub_latency_1 = sub_latency_0;
		__entry->sub_latency_1 = sub_latency_1;
		__entry->sub_latency_2 = sub_latency_2;
		__entry->sub_latency_3 = sub_latency_3;
		__entry->sub_latency_4 = sub_latency_4;
		__entry->sub_latency_5 = sub_latency_5;
		__entry->sub_latency_6 = sub_latency_6;
		__entry->sub_latency_7 = sub_latency_7;
	),
	TP_printk(
		APUSYS_RV_TAG_PWR_CTRL_PRINT,
		__entry->id,
		__entry->on,
		__entry->off,
		__entry->latency,
		__entry->sub_latency_0,
		__entry->sub_latency_1,
		__entry->sub_latency_2,
		__entry->sub_latency_3,
		__entry->sub_latency_4,
		__entry->sub_latency_5,
		__entry->sub_latency_6,
		__entry->sub_latency_7
	)
);


#endif /* #if !defined(__APUSYS_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE apusys_rv_events
#include <trace/define_trace.h>
