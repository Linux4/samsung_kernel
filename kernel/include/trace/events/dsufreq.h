#undef TRACE_SYSTEM
#define TRACE_SYSTEM dsufreq

#if !defined(_TRACE_DSUFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DSUFREQ_H

#include <linux/tracepoint.h>

TRACE_EVENT(dsufreq_dm_scaler,

	TP_PROTO(unsigned int target_freq, unsigned int resolve_freq, unsigned int cur_freq),

	TP_ARGS(target_freq, resolve_freq, cur_freq),

	TP_STRUCT__entry(
		__field( unsigned int,		target_freq			)
		__field( unsigned int,		resolve_freq		)
		__field( unsigned int,		cur_freq			)
	),

	TP_fast_assign(
		__entry->target_freq		= target_freq;
		__entry->resolve_freq		= resolve_freq;
		__entry->cur_freq			= cur_freq;
	),

	TP_printk("target_freq=%u resolve_freq=%u cur_freq=%u",
		__entry->target_freq, __entry->resolve_freq, __entry->cur_freq)
);

TRACE_EVENT(dsufreq_target,

	TP_PROTO(unsigned int target_freq, unsigned int prev_freq, unsigned int curr_freq),

	TP_ARGS(target_freq, prev_freq, curr_freq),

	TP_STRUCT__entry(
		__field( unsigned int,		target_freq		)
		__field( unsigned int,		prev_freq		)
		__field( unsigned int,		curr_freq		)
	),

	TP_fast_assign(
		__entry->target_freq		= target_freq;
		__entry->prev_freq		= prev_freq;
		__entry->curr_freq		= curr_freq;
	),

	TP_printk("target_freq=%u prev_freq=%u curr_freq=%u",
		__entry->target_freq, __entry->prev_freq, __entry->curr_freq)
);


#endif /* _TRACE_DSUFREQ_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/events

/* This part must be outside protection */
#include <trace/define_trace.h>
