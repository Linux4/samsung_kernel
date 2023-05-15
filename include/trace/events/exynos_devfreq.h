/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos_devfreq

#if !defined(_TRACE_EXYNOS_DEVFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_DEVFREQ_H

#include <linux/pm_qos.h>
#include <linux/tracepoint.h>
#include <linux/trace_events.h>

#define DEVFREQ_DEV_NAME_LEN 30

TRACE_EVENT(exynos_devfreq,

	TP_PROTO(unsigned int old_freq, unsigned int new_freq, const char *domain_name),

	TP_ARGS(old_freq, new_freq, domain_name),

	TP_STRUCT__entry(
		__field(	unsigned int,	old_freq			)
		__field(	unsigned int,	new_freq			)
		__array(	char,	name	,	DEVFREQ_DEV_NAME_LEN	)
	),

	TP_fast_assign(
		__entry->old_freq	= old_freq;
		__entry->new_freq	= new_freq;
		strcpy(__entry->name, domain_name);
	),

	TP_printk("domain_name = %s, old_freq = %u -> new_freq = %u", __entry->name, __entry->old_freq, __entry->new_freq)
);

#ifdef CONFIG_EXYNOS_ALT_DVFS_DEBUG
TRACE_EVENT(alt_dvfs_load_tracking,

	TP_PROTO(unsigned long long delta, unsigned int load, unsigned int freq),

	TP_ARGS(delta, load, freq),

	TP_STRUCT__entry(
		__field(	unsigned long long,	delta			)
		__field(	unsigned int,	load			)
		__field(	unsigned int,	freq			)
	),

	TP_fast_assign(
		__entry->delta	= delta;
		__entry->load	= load;
		__entry->freq	= freq;
	),

	TP_printk("delta = %llu, load = %u -> freq = %u", __entry->delta, __entry->load, __entry->freq)
);
#endif
#endif /* _TRACE_EXYNOS_DEVFREQ_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
