/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 */

/**
 * This header file is created to implement below specification in :
 * platform/frameworks/native/services/gpuservice/gpuwork/bpfprogs/gpu_work.c
 * Defines the structure of the kernel tracepoint:
 *     /sys/kernel/tracing/events/power/gpu_work_period/
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM power

#if !defined(_SGPU_WORKTIME_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _SGPU_WORKTIME_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(gpu_work_period,
	TP_PROTO(u32 gpu_id, u32 uid, u64 start_time_ns,
		u64 end_time_ns, u64 total_active_duration_ns),
	TP_ARGS(gpu_id, uid, start_time_ns, end_time_ns, total_active_duration_ns),
	TP_STRUCT__entry(
		__field(u32, gpu_id)
		__field(u32, uid)
		__field(u64, start_time_ns)
		__field(u64, end_time_ns)
		__field(u64, total_active_duration_ns)
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

#endif  /* _AMDGPU_WORKTIME_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../../drivers/gpu/drm/samsung/gpu/sgpu
#define TRACE_INCLUDE_FILE sgpu_worktime_trace
#include <trace/define_trace.h>
