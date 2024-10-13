/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM governor_lealt_trace

#if !defined(_TRACE_GOVERNOR_LEALT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GOVERNOR_LEALT_H

#include <linux/tracepoint.h>

TRACE_EVENT(lealt_sample_backtrace,
	TP_PROTO(unsigned int seq_no, long long duration, unsigned long next_freq,
		 int next_llc),

	TP_ARGS(seq_no, duration, next_freq, next_llc),

	TP_STRUCT__entry(
		__field(unsigned int, seq_no)
		__field(long long, duration)
		__field(unsigned long, next_freq)
		__field(int, next_llc)
	),

	TP_fast_assign(
		__entry->seq_no = seq_no;
		__entry->duration = duration;
		__entry->next_freq = next_freq;
		__entry->next_llc = next_llc;
	),

	TP_printk("seq_no=%u duration=%lld next_freq=%lu next_llc=%d",
		__entry->seq_no, __entry->duration,
		__entry->next_freq, __entry->next_llc)
);

TRACE_EVENT(lealt_sample_cur,
	TP_PROTO(unsigned int seq_no, long long duration, unsigned long next_freq,
		 int next_llc, unsigned long mif_freq,
		 unsigned long active_load, unsigned long targetload,
		 unsigned long mif_active_freq, unsigned long base_minlock,
		 unsigned long base_maxlock),

	TP_ARGS(seq_no, duration, next_freq, next_llc, mif_freq,
		 active_load, targetload, mif_active_freq, base_minlock,
		 base_maxlock),

	TP_STRUCT__entry(
		__field(unsigned int, seq_no)
		__field(long long, duration)
		__field(unsigned long, next_freq)
		__field(int, next_llc)
		__field(unsigned long, mif_freq)
		__field(unsigned long, active_load)
		__field(unsigned long, targetload)
		__field(unsigned long, mif_active_freq)
		__field(unsigned long, base_minlock)
		__field(unsigned long, base_maxlock)
	),

	TP_fast_assign(
		__entry->seq_no = seq_no;
		__entry->duration = duration;
		__entry->next_freq = next_freq;
		__entry->next_llc = next_llc;
		__entry->mif_freq = mif_freq;
		__entry->active_load = active_load;
		__entry->targetload = targetload;
		__entry->mif_active_freq = mif_active_freq;
		__entry->base_minlock = base_minlock;
		__entry->base_maxlock = base_maxlock;
	),

	TP_printk("seq_no=%u duration=%lld next_freq=%lu next_llc=%d\
		mif_freq=%lu active_load=%lu targetload=%lu mif_active_load=%lu\
		base_minlock=%lu base_maxlock=%lu",
		__entry->seq_no, __entry->duration, __entry->next_freq,
		__entry->next_llc, __entry->mif_freq, __entry->active_load,
		__entry->targetload, __entry->mif_active_freq,
		__entry->base_minlock, __entry->base_maxlock)
);

TRACE_EVENT(lealt_queue_work,
	TP_PROTO(unsigned int new_ms),

	TP_ARGS(new_ms),

	TP_STRUCT__entry(
		__field(unsigned int, new_ms)
	),

	TP_fast_assign(
		__entry->new_ms = new_ms;
	),

	TP_printk("new_ms=%u",
		__entry->new_ms)
);

TRACE_EVENT(lealt_alert_prepare,
	TP_PROTO(unsigned int prepare_alert,
		 unsigned int global_prepare_freq,
		 int cpucl, unsigned int freq_cur),

	TP_ARGS(prepare_alert, global_prepare_freq,
		cpucl, freq_cur),

	TP_STRUCT__entry(
		__field(unsigned int, prepare_alert)
		__field(unsigned int, global_prepare_freq)
		__field(unsigned int, cpucl)
		__field(unsigned int, freq_cur)
	),

	TP_fast_assign(
		__entry->prepare_alert = prepare_alert;
		__entry->global_prepare_freq = global_prepare_freq;
		__entry->cpucl = cpucl;
		__entry->freq_cur = freq_cur;
	),

	TP_printk("prepare_alert=%u global_prepare_freq=%u cpucl=%u freq_cur=%u",
		  __entry->prepare_alert, __entry->global_prepare_freq,
		  __entry->cpucl, __entry->freq_cur)
);

TRACE_EVENT(lealt_alert_stability,
	TP_PROTO(unsigned int stability_alert,
		 unsigned int stability,
		 int cpucl, unsigned int freq_cur,
		 unsigned int freq_high, unsigned int freq_low),

	TP_ARGS(stability_alert, stability,
		cpucl, freq_cur, freq_high, freq_low),

	TP_STRUCT__entry(
		__field(unsigned int, stability_alert)
		__field(unsigned int, stability)
		__field(unsigned int, cpucl)
		__field(unsigned int, freq_cur)
		__field(unsigned int, freq_high)
		__field(unsigned int, freq_low)
	),

	TP_fast_assign(
		__entry->stability_alert = stability_alert;
		__entry->stability = stability;
		__entry->cpucl = cpucl;
		__entry->freq_cur = freq_cur;
		__entry->freq_high = freq_high;
		__entry->freq_low = freq_low;
	),

	TP_printk("stability_alert=%u stability=%u\
		  cpucl=%u freq_cur=%u freq_high=%u freq_low=%u",
		  __entry->stability_alert,
		  __entry->stability, __entry->cpucl, __entry->freq_cur,
		  __entry->freq_high, __entry->freq_low)
);

TRACE_EVENT(lealt_governor_pmu_each,
	TP_PROTO(int id, unsigned long freq,
		 unsigned int ratio, unsigned long stall_pct),

	TP_ARGS(id, freq, ratio, stall_pct),

	TP_STRUCT__entry(
		__field(int, id)
		__field(unsigned long, freq)
		__field(unsigned int, ratio)
		__field(unsigned long, stall_pct)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->freq = freq;
		__entry->ratio = ratio;
		__entry->stall_pct = stall_pct;
	),

	TP_printk("id=%d freq=%lu ratio=%u stall_pct=%lu",
		__entry->id, __entry->freq, __entry->ratio,
		__entry->stall_pct)
);

TRACE_EVENT(lealt_governor_latency_dev,
	TP_PROTO(int id, unsigned long freq,
		 unsigned long targetload),

	TP_ARGS(id, freq, targetload),

	TP_STRUCT__entry(
		__field(int, id)
		__field(unsigned long, freq)
		__field(unsigned long, targetload)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->freq = freq;
		__entry->targetload = targetload;
	),

	TP_printk("id=%d freq=%lu targetload=%lu",
		__entry->id, __entry->freq, __entry->targetload)
);
#endif /* _TRACE_GOVERNOR_LEALT_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
