/*
 *  Copyright (C) 2021 Park Choonghoon <choong.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos_cpufreq

#if !defined(_TRACE_EXYNOS_CPU_FREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_CPU_FREQ_H

#include <linux/tracepoint.h>

/*
 * Tracepoint for frequency scaling start/end points
 */
TRACE_EVENT(cpufreq_scale_freq,

	TP_PROTO(unsigned int cluster, unsigned int old_freq,
		 unsigned int new_freq, char *point, unsigned long time),

	TP_ARGS(cluster, old_freq, new_freq, point, time),

	TP_STRUCT__entry(
		__field(	unsigned int,		cluster		)
		__field(	unsigned int,		old_freq	)
		__field(	unsigned int,		new_freq	)
		__array(	char,			point,	8	)
		__field(	unsigned long,		time		)
	),

	TP_fast_assign(
		__entry->cluster		= cluster;
		__entry->old_freq		= old_freq;
		__entry->new_freq		= new_freq;
		__entry->time			= time;
		memcpy(__entry->point, point, 8);
		__entry->time			= time;
	),

	TP_printk("cluster=%u old_freq=%u new_freq=%u point=%s time=%lu",
		  __entry->cluster, __entry->old_freq,
		  __entry->new_freq, __entry->point, __entry->time)
);
#endif /* _TRACE_EXYNOS_CPU_FREQ_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
