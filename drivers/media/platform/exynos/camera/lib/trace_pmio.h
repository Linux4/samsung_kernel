/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM pmio

#if !defined(_TRACE_PMIO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PMIO_H

#include <linux/tracepoint.h>

/*
 * Pablo MMIO events
 */
DECLARE_EVENT_CLASS(pmio_reg,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg,
		 unsigned int val),
	TP_ARGS(pmio, reg, val),
	TP_STRUCT__entry(
		__string(	name,		pmio_name(pmio)	)
		__field(	unsigned int,	reg		)
		__field(	unsigned int,	val		)
	),
	TP_fast_assign(
		__assign_str(name, pmio_name(pmio));
		__entry->reg = reg;
		__entry->val = val;
	),
	TP_printk("%s reg=%x val=%x", __get_str(name),
		  (unsigned int)__entry->reg,
		  (unsigned int)__entry->val)
);

DEFINE_EVENT(pmio_reg, pmio_reg_write,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg,
		 unsigned int val),
	TP_ARGS(pmio, reg, val)
);

DEFINE_EVENT(pmio_reg, pmio_reg_read,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg,
		 unsigned int val),
	TP_ARGS(pmio, reg, val)
);

DEFINE_EVENT(pmio_reg, pmio_reg_read_cache,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg,
		 unsigned int val),
	TP_ARGS(pmio, reg, val)
);

DECLARE_EVENT_CLASS(pmio_block,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg, int count),
	TP_ARGS(pmio, reg, count),
	TP_STRUCT__entry(
		__string(	name,		pmio_name(pmio)	)
		__field(	unsigned int,	reg		)
		__field(	int,		count		)
	),
	TP_fast_assign(
		__assign_str(name, pmio_name(pmio));
		__entry->reg = reg;
		__entry->count = count;
	),
	TP_printk("%s reg=%x count=%d", __get_str(name),
		  (unsigned int)__entry->reg,
		  (int)__entry->count)
);

DEFINE_EVENT(pmio_block, pmio_hw_read_start,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg, int count),
	TP_ARGS(pmio, reg, count)
);

DEFINE_EVENT(pmio_block, pmio_hw_read_done,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg, int count),
	TP_ARGS(pmio, reg, count)
);

DEFINE_EVENT(pmio_block, pmio_hw_write_start,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg, int count),
	TP_ARGS(pmio, reg, count)
);

DEFINE_EVENT(pmio_block, pmio_hw_write_done,
	TP_PROTO(struct pablo_mmio *pmio, unsigned int reg, int count),
	TP_ARGS(pmio, reg, count)
);

TRACE_EVENT(pmio_cache_sync_region,
	TP_PROTO(struct pablo_mmio *pmio, const char *type,
		 unsigned int from, unsigned int to,
		 const char *status),
	TP_ARGS(pmio, type, from, to, status),
	TP_STRUCT__entry(
		__string(	name,		pmio_name(pmio)	)
		__string(	type,		type		)
		__field(	int,		type		)
		__field(	unsigned int,	from		)
		__field(	unsigned int,	to		)
		__string(	status,		status		)
	),
	TP_fast_assign(
		__assign_str(name, pmio_name(pmio));
		__assign_str(type, type);
		__entry->from = from;
		__entry->to = to;
		__assign_str(status, status);
	),
	TP_printk("%s %s %u-%u %s", __get_str(name), __get_str(type),
		(unsigned int)__entry->from, (unsigned int)__entry->to,
		__get_str(status))
);

TRACE_EVENT(pmio_cache_fsync_region,
	TP_PROTO(struct pablo_mmio *pmio, const char *type,
		 enum pmio_formatter_type fmt,
		 unsigned int from, unsigned int to,
		 const char *status),
	TP_ARGS(pmio, type, fmt, from, to, status),
	TP_STRUCT__entry(
		__string(	name,		pmio_name(pmio)	)
		__string(	type,		type		)
		__field(	int,		type		)
		__field(	unsigned int,	fmt		)
		__field(	unsigned int,	from		)
		__field(	unsigned int,	to		)
		__string(	status,		status		)
	),
	TP_fast_assign(
		__assign_str(name, pmio_name(pmio));
		__assign_str(type, type);
		__entry->fmt = fmt;
		__entry->from = from;
		__entry->to = to;
		__assign_str(status, status);
	),
	TP_printk("%s %s %u-%u %u %s", __get_str(name), __get_str(type),
		(unsigned int)__entry->fmt, (unsigned int)__entry->from,
		(unsigned int)__entry->to, __get_str(status))
);

TRACE_EVENT(pmio_cache_drop_region,
	TP_PROTO(struct pablo_mmio *pmio, const char *type,
		 unsigned int from, unsigned int to),
	TP_ARGS(pmio, type, from, to),
	TP_STRUCT__entry(
		__string(	name,		pmio_name(pmio)	)
		__string(	type,		type		)
		__field(	int,		type		)
		__field(	unsigned int,	from		)
		__field(	unsigned int,	to		)
	),
	TP_fast_assign(
		__assign_str(name, pmio_name(pmio));
		__entry->from = from;
		__entry->to = to;
	),
	TP_printk("%s %s %u-%u", __get_str(name), __get_str(type),
		 (unsigned int)__entry->from, (unsigned int)__entry->to)
);

#endif /* _TRACE_PMIO_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/media/platform/exynos/camera/lib/

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace_pmio

/* This part must be outside protection */
#include <trace/define_trace.h>
