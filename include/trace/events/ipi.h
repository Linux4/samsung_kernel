#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipi

#if !defined(_TRACE_IPI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IPI_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(silent,

	TP_PROTO(int unused),

	TP_ARGS(unused),

	TP_STRUCT__entry(
		__field( int, unused )
	),

	TP_fast_assign(
		__entry->unused = unused
	),

	TP_printk("%s", "")
);

#define DEFINE_SILENT_EVENT(name)		\
	DEFINE_EVENT(silent, name ,		\
		TP_PROTO(const int unused),	\
		     TP_ARGS(unused))

DEFINE_SILENT_EVENT(ipi_send_ping);
DEFINE_SILENT_EVENT(ipi_ping);
DEFINE_SILENT_EVENT(ipi_timer_exit);
DEFINE_SILENT_EVENT(ipi_reschedule_enter);
DEFINE_SILENT_EVENT(ipi_reschedule_exit);
DEFINE_SILENT_EVENT(ipi_cpu_stop_enter);
DEFINE_SILENT_EVENT(ipi_cpu_stop_exit);
DEFINE_SILENT_EVENT(ipi_call_func_exit);

TRACE_EVENT(ipi_send_call_func,

	TP_PROTO(const struct cpumask *mask),

	TP_ARGS(mask),

	TP_STRUCT__entry(
		__array( char, cpustring, 64 )
	),

	TP_fast_assign(
		cpumask_scnprintf(__entry->cpustring, 64, mask)
	),

	TP_printk("target_cpus=%s", __entry->cpustring)
);

TRACE_EVENT(ipi_send_call_func_single,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field( int, cpu )
	),

	TP_fast_assign(
		__entry->cpu = cpu
	),

	TP_printk("target_cpu=%d", __entry->cpu)
);

TRACE_EVENT(ipi_timer_send,

	TP_PROTO(const struct cpumask *mask),

	TP_ARGS(mask),

	TP_STRUCT__entry(
		__array( char, cpustring, 64 )
	),

	TP_fast_assign(
		cpumask_scnprintf(__entry->cpustring, 64, mask)
	),

	TP_printk("target_cpus=%s", __entry->cpustring)
);

TRACE_EVENT(ipi_send_reschedule,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field( int, cpu )
	),

	TP_fast_assign(
		__entry->cpu = cpu
	),

	TP_printk("target_cpu=%d", __entry->cpu)
);

TRACE_EVENT(ipi_send_cpu_stop,

	TP_PROTO(const struct cpumask *mask),

	TP_ARGS(mask),

	TP_STRUCT__entry(
		__array( char, cpustring, 64 )
	),

	TP_fast_assign(
		cpumask_scnprintf(__entry->cpustring, 64, mask)
	),

	TP_printk("target_cpus=%s", __entry->cpustring)
);

TRACE_EVENT(ipi_timer_enter,

	TP_PROTO(unsigned int evt, unsigned int handler),

	TP_ARGS(evt, handler),

	TP_STRUCT__entry(
		__field( unsigned int, evt)
		__field( unsigned int, handler)
	),

	TP_fast_assign(
		__entry->evt = evt;
		__entry->handler = handler
	),

	TP_printk("event 0x%08x calling handler 0x%08x",
		__entry->evt, __entry->handler)
);

TRACE_EVENT(ipi_cpu_backtrace,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field( int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu_id=%d",
		__entry->cpu)
);

TRACE_EVENT(ipi_call_func_enter,

	TP_PROTO(unsigned int func),

	TP_ARGS(func),

	TP_STRUCT__entry(
		__field( unsigned int, func)
	),

	TP_fast_assign(
		__entry->func = func;
	),

	TP_printk("calling function 0x%08x",
		__entry->func)
);

#endif /* _TRACE_IPI_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
