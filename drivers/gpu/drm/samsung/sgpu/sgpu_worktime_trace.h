/*
 * @file sgpu_worktime_trace.h
 * @copyright 2022 Samsung Electronics
 */

/*
 * this header file is created to implement below specification in :
 * platform/frameworks/native/services/gpuservice/gpuwork/bpfprogs/gpu_work.c
 * Defines the structure of the kernel tracepoint:
 * /sys/kernel/tracing/events/power/gpu_work_period/
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM power

#if !defined(_SGPU_WORKTIME_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _SGPU_WORKTIME_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(gpu_work_period,
	TP_PROTO(uint32_t gpu_id, uint32_t uid, uint64_t start_time_ns,
		uint64_t end_time_ns, uint64_t total_active_duration_ns),
	TP_ARGS(gpu_id, uid, start_time_ns, end_time_ns, total_active_duration_ns),
	TP_STRUCT__entry(
		__field(uint32_t, gpu_id)
		__field(uint32_t, uid)
		__field(uint64_t, start_time_ns)
		__field(uint64_t, end_time_ns)
		__field(uint64_t, total_active_duration_ns)
	),

	TP_fast_assign(
		__entry->gpu_id = gpu_id;
		__entry->uid = uid;
		__entry->start_time_ns = start_time_ns;
		__entry->end_time_ns = end_time_ns;
		__entry->total_active_duration_ns = total_active_duration_ns;
	),

	TP_printk("%u %u %llu %llu %llu",
		__entry->gpu_id, __entry->uid, __entry->start_time_ns,
		__entry->end_time_ns, __entry->total_active_duration_ns)
);

TRACE_EVENT(gpu_work_period_debug,
	TP_PROTO(struct sgpu_worktime *wt,
		uint32_t uid, uint64_t start_time_ns,
		uint64_t end_time_ns, uint64_t total_active_duration_ns),
	TP_ARGS(wt, uid, start_time_ns, end_time_ns, total_active_duration_ns),
	TP_STRUCT__entry(
		__field(struct sgpu_worktime*, wt)
		__field(uint32_t, uid)
		__field(uint64_t, start_time_ns)
		__field(uint64_t, end_time_ns)
		__field(uint64_t, total_active_duration_ns)
	),

	TP_fast_assign(
		__entry->wt = wt;
		__entry->uid = uid;
		__entry->start_time_ns = start_time_ns;
		__entry->end_time_ns = end_time_ns;
		__entry->total_active_duration_ns = total_active_duration_ns;
	),

	TP_printk("%p %u %llu %llu %llu",
		__entry->wt, __entry->uid, __entry->start_time_ns,
		__entry->end_time_ns, __entry->total_active_duration_ns)
);

TRACE_EVENT(gpu_work_period_open,
	TP_PROTO(struct sgpu_worktime *wt, uint32_t uid),
	TP_ARGS(wt, uid),
	TP_STRUCT__entry(
		__field(struct sgpu_worktime*, wt)
		__field(uint32_t, uid)
	),

	TP_fast_assign(
		__entry->wt = wt;
		__entry->uid = uid;
	),

	TP_printk(" %p %u", __entry->wt, __entry->uid)
);

TRACE_EVENT(gpu_work_period_close,
	TP_PROTO(struct sgpu_worktime *wt, uint32_t uid),
	TP_ARGS(wt, uid),
	TP_STRUCT__entry(
		__field(struct sgpu_worktime*, wt)
		__field(uint32_t, uid)
	),

	TP_fast_assign(
		__entry->wt = wt;
		__entry->uid = uid;
	),

	TP_printk("%p %u", __entry->wt, __entry->uid)
);

TRACE_EVENT(gpu_work_period_suspend,
	TP_PROTO(uint64_t time),
	TP_ARGS(time),
	TP_STRUCT__entry(
		__field(uint64_t, time)
	),

	TP_fast_assign(
		__entry->time = time;
	),

	TP_printk("%llu", __entry->time)
);

TRACE_EVENT(gpu_work_period_resume,
	TP_PROTO(uint64_t time),
	TP_ARGS(time),
	TP_STRUCT__entry(
		__field(uint64_t, time)
	),

	TP_fast_assign(
		__entry->time = time;
	),

	TP_printk("%llu", __entry->time)
);
#endif  /* _AMDGPU_WORKTIME_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../../drivers/gpu/drm/samsung/sgpu
#define TRACE_INCLUDE_FILE sgpu_worktime_trace
#include <trace/define_trace.h>
