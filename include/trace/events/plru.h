/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM plru

#if !defined(_TRACE_PLRU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PLRU_H

#include <linux/tracepoint.h>
#include <linux/mm.h>

#define	PAGEMAP_MAPPED		0x0001u
#define PAGEMAP_ANONYMOUS	0x0002u
#define PAGEMAP_FILE		0x0004u
#define PAGEMAP_SWAPCACHE	0x0008u
#define PAGEMAP_SWAPBACKED	0x0010u
#define PAGEMAP_MAPPEDDISK	0x0020u
#define PAGEMAP_BUFFERS		0x0040u

#define trace_pagemap_flags(page) ( \
	(PageAnon(page)		? PAGEMAP_ANONYMOUS  : PAGEMAP_FILE) | \
	(page_mapped(page)	? PAGEMAP_MAPPED     : 0) | \
	(PageSwapCache(page)	? PAGEMAP_SWAPCACHE  : 0) | \
	(PageSwapBacked(page)	? PAGEMAP_SWAPBACKED : 0) | \
	(PageMappedToDisk(page)	? PAGEMAP_MAPPEDDISK : 0) | \
	(page_has_private(page) ? PAGEMAP_BUFFERS    : 0) \
	)


TRACE_EVENT(plru_page_activate,

	TP_PROTO(unsigned long call_site, struct page *page, int protect_level),

	TP_ARGS(call_site, page, protect_level),

	TP_STRUCT__entry(
		__field(unsigned long,	call_site)
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(int,	protect_level)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->protect_level	= protect_level;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("call_site=%lx, page=%p pfn=%lu protect_level=%d",
		  __entry->call_site, __entry->page, __entry->pfn,
		  __entry->protect_level)

);

TRACE_EVENT(plru_page_deactivate,

	TP_PROTO(unsigned long call_site, struct page *page, int protect_level),

	TP_ARGS(call_site, page, protect_level),

	TP_STRUCT__entry(
		__field(unsigned long,	call_site)
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(int,	protect_level)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->protect_level	= protect_level;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("call_site=%lx, page=%p pfn=%lu protect_level=%d",
		  __entry->call_site, __entry->page, __entry->pfn,
		  __entry->protect_level)

);

TRACE_EVENT(plru_page_free,

	TP_PROTO(unsigned long call_site, struct page *page, unsigned long num,
		 int protect_level),

	TP_ARGS(call_site, page, num, protect_level),

	TP_STRUCT__entry(
		__field(unsigned long,	call_site)
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(unsigned long,	num)
		__field(int,	protect_level)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->num	= num;
		__entry->protect_level	= protect_level;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("call_site=%lx, page=%p pfn=%lu num=%lu protect_level=%d",
		  __entry->call_site, __entry->page, __entry->pfn, __entry->num,
		  __entry->protect_level)

);

TRACE_EVENT(plru_page_unprotect,

	TP_PROTO(struct page *page, unsigned long protect_level, int lru),

	TP_ARGS(page, protect_level, lru),

	TP_STRUCT__entry(
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(unsigned long,	protect_level)
		__field(int,	lru)
	),

	TP_fast_assign(
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->protect_level	= protect_level;
		__entry->lru	= lru;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("page=%p pfn=%lu protect_level=%lu lru=%d",
		  __entry->page, __entry->pfn, __entry->protect_level, __entry->lru)

);


TRACE_EVENT(plru_page_add,

	TP_PROTO(struct page *page, unsigned long num, int protect_level, int lru),

	TP_ARGS(page, num, protect_level, lru),

	TP_STRUCT__entry(
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(unsigned long,	num)
		__field(int,	protect_level)
		__field(int,	lru)
	),

	TP_fast_assign(
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->num	= num;
		__entry->protect_level	= protect_level;
		__entry->lru	= lru;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("page=%p pfn=%lu num=%lu protect_level=%d lru=%d",
		  __entry->page, __entry->pfn,
		  __entry->num, __entry->protect_level, __entry->lru)

);

TRACE_EVENT(plru_page_del,

	TP_PROTO(struct page *page, unsigned long num, int protect_level, int lru),

	TP_ARGS(page, num, protect_level, lru),

	TP_STRUCT__entry(
		__field(struct page *,	page)
		__field(unsigned long,	pfn)
		__field(unsigned long,	num)
		__field(int,	protect_level)
		__field(int,	lru)
	),

	TP_fast_assign(
		__entry->page	= page;
		__entry->pfn	= page_to_pfn(page);
		__entry->num	= num;
		__entry->protect_level	= protect_level;
		__entry->lru	= lru;
	),

	/* Flag format is based on page-types.c formatting for pagemap */
	TP_printk("page=%p pfn=%lu num=%lu protect_level=%d lru=%d",
		  __entry->page, __entry->pfn,
		  __entry->num, __entry->protect_level, __entry->lru)

);
#endif /* _TRACE_PROTECTLRU_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
