/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM lpm_gov_trace_event

#if !defined(_LPM_GOV_TRACE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _LPM_GOV_TRACE_EVENT_H

#include <linux/tracepoint.h>

TRACE_EVENT(lpm_gov,
	TP_PROTO(unsigned int state, int cpu, bool ipi_abort,
		 uint32_t predict_info, uint64_t predict,
		 uint64_t next_timer_ns, bool htmr_wkup,
		 uint64_t slp_std, uint64_t ipi_std,
		 int64_t latency_req),

	TP_ARGS(state, cpu, ipi_abort, predict_info, predict,
		next_timer_ns, htmr_wkup, slp_std, ipi_std,
		latency_req),

	TP_STRUCT__entry(
		__field(unsigned int, state)
		__field(int, cpu)
		__field(bool, ipi_abort)
		__field(uint32_t, predict_info)
		__field(uint64_t, predict)
		__field(uint64_t, next_timer_ns)
		__field(bool, htmr_wkup)
		__field(uint64_t, slp_std)
		__field(uint64_t, ipi_std)
		__field(int64_t, latency_req)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->cpu = cpu;
		__entry->ipi_abort = ipi_abort;
		__entry->predict_info = predict_info;
		__entry->predict = predict;
		__entry->next_timer_ns = next_timer_ns;
		__entry->htmr_wkup = htmr_wkup;
		__entry->slp_std = slp_std;
		__entry->ipi_std = ipi_std;
		__entry->latency_req = latency_req;
	),

	TP_printk("state=%u cpu_id=%d ipi_abort=%d predict=0x%x time=%llu next=%llu shallow=%d std1=%llu std2=%llu latency_req=%lld",
		  __entry->state, __entry->cpu, __entry->ipi_abort,
		  __entry->predict_info, __entry->predict,
		  __entry->next_timer_ns, __entry->htmr_wkup,
		  __entry->slp_std, __entry->ipi_std,
		  __entry->latency_req)
);

#endif /* _TRACE_MTK_POLICY_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE lpm_gov_trace_event

/* This part must be outside protection */
#include <trace/define_trace.h>
