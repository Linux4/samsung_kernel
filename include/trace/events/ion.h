/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ion

#if !defined(_TRACE_ION_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ION_H

#include <linux/types.h>
#include <linux/tracepoint.h>

#define DEV_NAME_NONE "None"

DECLARE_EVENT_CLASS(ion_dma_map_cmo_class,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir),

	TP_STRUCT__entry(
		__string(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE)
		__string(name, name)
		__field(bool, cached)
		__field(bool, hlos_accessible)
		__field(unsigned long, map_attrs)
		__field(enum dma_data_direction, dir)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE);
		__assign_str(name, name);
		__entry->cached = cached;
		__entry->hlos_accessible = hlos_accessible;
		__entry->map_attrs = map_attrs;
		__entry->dir = dir;
	),

	TP_printk("dev=%s name=%s cached=%d access=%d map_attrs=0x%lx dir=%d",
		__get_str(dev_name),
		__get_str(name),
		__entry->cached,
		__entry->hlos_accessible,
		__entry->map_attrs,
		__entry->dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_map_cmo_apply,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_map_cmo_skip,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_unmap_cmo_apply,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_unmap_cmo_skip,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DECLARE_EVENT_CLASS(ion_access_cmo_class,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped),

	TP_STRUCT__entry(
		__string(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE)
		__string(name, name)
		__field(bool, cached)
		__field(bool, hlos_accessible)
		__field(enum dma_data_direction, dir)
		__field(bool, only_mapped)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE);
		__assign_str(name, name);
		__entry->cached = cached;
		__entry->hlos_accessible = hlos_accessible;
		__entry->dir = dir;
		__entry->only_mapped = only_mapped;
	),

	TP_printk("dev=%s name=%s cached=%d access=%d dir=%d, only_mapped=%d",
		  __get_str(dev_name),
		  __get_str(name),
		  __entry->cached,
		  __entry->hlos_accessible,
		  __entry->dir,
		  __entry->only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_cmo_apply,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_cmo_skip,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_notmapped,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_cmo_apply,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_cmo_skip,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_notmapped,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DECLARE_EVENT_CLASS(ion_alloc,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags),

	TP_ARGS(heap_name, len, mask, flags),

	TP_STRUCT__entry(
		__field(const char *,	heap_name)
		__field(size_t,		len)
		__field(unsigned int,	mask)
		__field(unsigned int,	flags)
	),

	TP_fast_assign(
		__entry->heap_name	= heap_name;
		__entry->len		= len;
		__entry->mask		= mask;
		__entry->flags		= flags;
	),

	TP_printk("heap_name=%s len=%zu mask=0x%x flags=0x%x",
		__entry->heap_name,
		__entry->len,
		__entry->mask,
		__entry->flags)
);

DEFINE_EVENT(ion_alloc, ion_alloc_buffer_start,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags),

	TP_ARGS(heap_name, len, mask, flags)
);

DEFINE_EVENT(ion_alloc, ion_alloc_buffer_end,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags),

	TP_ARGS(heap_name, len, mask, flags)
);

DECLARE_EVENT_CLASS(ion_alloc_error,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags,
		 long error),

	TP_ARGS(heap_name, len, mask, flags, error),

	TP_STRUCT__entry(
		__field(const char *,	heap_name)
		__field(size_t,		len)
		__field(unsigned int,	mask)
		__field(unsigned int,	flags)
		__field(long,		error)
	),

	TP_fast_assign(
		__entry->heap_name	= heap_name;
		__entry->len		= len;
		__entry->mask		= mask;
		__entry->flags		= flags;
		__entry->error		= error;
	),

	TP_printk(
	"heap_name=%s len=%zu mask=0x%x flags=0x%x error=%ld",
		__entry->heap_name,
		__entry->len,
		__entry->mask,
		__entry->flags,
		__entry->error)
);

DEFINE_EVENT(ion_alloc_error, ion_alloc_buffer_fallback,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags,
		 long error),

	TP_ARGS(heap_name, len, mask, flags, error)
);

DEFINE_EVENT(ion_alloc_error, ion_alloc_buffer_fail,

	TP_PROTO(const char *heap_name,
		 size_t len,
		 unsigned int mask,
		 unsigned int flags,
		 long error),

	TP_ARGS(heap_name, len, mask, flags, error)
);

DECLARE_EVENT_CLASS(ion_rbin,

	TP_PROTO(const char *heap_name,
		 void *buffer,
		 unsigned long size,
		 void *page),
	TP_ARGS(heap_name, buffer, size, page),

	TP_STRUCT__entry(
		__field(	const char *,	heap_name)
		__field(	void *,		buffer	)
		__field(	unsigned long,	size	)
		__field(	void *,		page	)
	),

	TP_fast_assign(
		__entry->heap_name	= heap_name;
		__entry->buffer		= buffer;
		__entry->size		= size;
		__entry->page		= page;
	),

	TP_printk("heap_name=%s buffer=%p size=%lu page=%p",
		__entry->heap_name,
		__entry->buffer,
		__entry->size,
		__entry->page
	)
);

DEFINE_EVENT(ion_rbin, ion_rbin_alloc_start,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_alloc_end,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_free_start,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_free_end,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_partial_alloc_start,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_partial_alloc_end,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_partial_free_start,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_partial_free_end,
	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);


DEFINE_EVENT(ion_rbin, ion_rbin_pool_alloc_start,

	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);

DEFINE_EVENT(ion_rbin, ion_rbin_pool_alloc_end,

	TP_PROTO(const char *heap_name, void *buffer, unsigned long size,
		 void *page),

	TP_ARGS(heap_name, buffer, size, page)
);
#endif /* _TRACE_ION_H */

#include <trace/define_trace.h>

