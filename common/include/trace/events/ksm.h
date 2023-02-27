#undef TRACE_SYSTEM
#define TRACE_SYSTEM ksm

#if !defined(_TRACE_KSM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_KSM_H
#include <linux/tracepoint.h>

TRACE_EVENT(ksm_s,
        TP_PROTO(int seq),

        TP_ARGS(seq),

	TP_STRUCT__entry(
		__field(	int,	seq)
        ),

	TP_fast_assign(
		__entry->seq = seq;
        ),

	TP_printk("start seq=%d", __entry->seq)
);

TRACE_EVENT(ksm_e,
        TP_PROTO(int seq, unsigned long shared, unsigned long sharing, unsigned long unshared ),

        TP_ARGS(seq, shared, sharing, unshared),

	TP_STRUCT__entry(
		__field(	int,	seq)
		__field(	unsigned long,	shared)
		__field(	unsigned long,	sharing)
		__field(	unsigned long,	unshared)
        ),

	TP_fast_assign(
		__entry->seq = seq;
		__entry->shared = shared;
		__entry->sharing = sharing;
		__entry->unshared = unshared;
        ),

	TP_printk("end seq=%d shared=%ld sharing=%ld unshared=%ld", __entry->seq, __entry->shared, __entry->sharing , __entry->unshared)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
