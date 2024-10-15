// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_slab_info.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/swab.h>
#include <linux/kasan.h>
#include <linux/notifier.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <asm/memory.h>
#include <linux/panic_notifier.h>

#include <trace/hooks/traps.h>
#include <trace/hooks/fault.h>

#include <linux/slab.h>
#include "../../../../mm/slab.h"

#define OBJ_MAX 4
#define BUF_MAX	256
#define INDEX_NOT_FOUND		(-1)

static unsigned long slab_cache_start_addr = ULONG_MAX;
static unsigned long slab_cache_end_addr;
static unsigned int secdbg_esr;
static struct pt_regs secdbg_regs;
static struct list_head *slab_caches_head;

static void android_rvh_die_kernel_fault(void *data,
                const char *msg, unsigned long addr, unsigned int esr, struct pt_regs *regs)
{
	secdbg_esr = esr;
	secdbg_regs = *regs;
}

static int secdbg_set_range_of_slab_cache(void)
{
	struct kmem_cache *s;
	struct list_head *p;
	struct list_head *head;

	int i = 0;

	for (i = 0; i < KMALLOC_SHIFT_HIGH + 1; i++) {
		s = kmalloc_caches[KMALLOC_NORMAL][i];
		if (s)
			break;
	}

	if (!s)
		return -ENXIO;

	head = (struct list_head *)&s->list;

	list_for_each(p, head) {
		if (!__is_lm_address(p)) {
			slab_caches_head = p;
			break;
		}
	}

	if (!slab_caches_head)
		return -ENXIO;

	list_for_each_entry(s, slab_caches_head, list) {
		if (slab_cache_start_addr > (unsigned long)s)
			slab_cache_start_addr = (unsigned long)s;
		if (slab_cache_end_addr < (unsigned long)s)
			slab_cache_end_addr = (unsigned long)s;
	}
	pr_info("%s %lx %lx\n", __func__, slab_cache_start_addr, slab_cache_end_addr);

	return 0;

}

static inline bool is_slab_cache(unsigned long reg)
{
	struct kmem_cache *s;

	if (reg < slab_cache_start_addr || reg > slab_cache_end_addr)
		return false;

	list_for_each_entry(s, slab_caches_head, list) {
		if (reg == (unsigned long)s)
			return true;
	}
	return false;
}

static inline void *restore_red_left(struct kmem_cache *s, void *p)
{
	if (s->flags & SLAB_RED_ZONE)
		p -= s->red_left_pad;

	return p;
}

static inline int check_valid_pointer(struct kmem_cache *s,
				struct page *page, void *object)
{
	void *base;

	if (!object)
		return 1;

	base = page_address(page);
	object = kasan_reset_tag(object);
	object = restore_red_left(s, object);
	if (object < base || object >= base + page->objects * s->size ||
		(object - base) % s->size) {
		return 0;
	}

	return 1;
}

static inline unsigned long freelist_ptr(const struct kmem_cache *s, void *ptr,
				 unsigned long ptr_addr)
{
#ifdef CONFIG_SLAB_FREELIST_HARDENED
	return ((unsigned long)ptr ^ s->random ^
			swab((unsigned long)kasan_reset_tag((void *)ptr_addr)));
#else
	return (unsigned long)ptr;
#endif
}

static void *secdbg_fixup_red_left(struct kmem_cache *s, void *p)
{
	if (s->flags & SLAB_RED_ZONE)
		p += s->red_left_pad;

	return p;
}

/* Loop over all objects in a slab */
#define for_each_object(__p, __s, __addr, __objects) \
	for (__p = secdbg_fixup_red_left(__s, __addr); \
		__p < (__addr) + (__objects) * (__s)->size; \
		__p += (__s)->size)


static int secdbg_slab_info_get_candidate(struct kmem_cache *s, struct kmem_cache_cpu *c, void *freelist, char *buf)
{
	struct page *page = c->page;
	int count = page->objects < OBJ_MAX ? page->objects : OBJ_MAX;
	void *p;
	unsigned long freepointer_addr;
	unsigned long candidate;
	ssize_t offset = 0;

	for_each_object(p, s, page_address(page), count) {
		p = kasan_reset_tag(p);
		freepointer_addr = (unsigned long)p + s->offset;
		candidate = freelist_ptr(s, freelist, freepointer_addr);
		offset += scnprintf(buf + offset, BUF_MAX - offset, " %lx", candidate);
	}

	return count;
}

static void secdbg_display_slab_cache_info(void)
{
	int i = 0;
	int slab_cache_reg = INDEX_NOT_FOUND;
	struct kmem_cache *s;
	struct kmem_cache_cpu *c;
	void *freelist;
	int cpu = raw_smp_processor_id();
	char buf[BUF_MAX];
	int obj_count;

	for (i = 20; i < 29; i++) {
		if (is_slab_cache(secdbg_regs.regs[i])) {
				slab_cache_reg = i;
				break;
		}
	}

	if (slab_cache_reg == INDEX_NOT_FOUND)
		return;

	s = (struct kmem_cache *)secdbg_regs.regs[slab_cache_reg];
	c = per_cpu_ptr(s->cpu_slab, cpu);
	freelist = c->freelist;

	if (!check_valid_pointer(s, c->page, freelist)) {
		pr_auto(ASL1, "x%d is %s slab cache\n", slab_cache_reg, s->name);
		pr_auto(ASL1, "Freelist was corrupted to %px\n", freelist);
		obj_count = secdbg_slab_info_get_candidate(s, c, freelist, buf);
		pr_auto(ASL1, "Candidate(%d of %d) :%s", obj_count, c->page->objects, buf);

	} else {
		pr_info("x%d is %s slab cache\n", slab_cache_reg, s->name);
	}
}

static int secdbg_slab_info_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (ESR_ELx_EC(secdbg_esr) != ESR_ELx_EC_DABT_CUR)
		return NOTIFY_DONE;

	if (secdbg_set_range_of_slab_cache())
		return NOTIFY_DONE;

	secdbg_display_slab_cache_info();

	return NOTIFY_DONE;
}


static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_slab_info_handler,
};

static int __init secdbg_slab_info_init(void)
{
	pr_info("%s: init\n", __func__);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
	register_trace_android_rvh_die_kernel_fault(android_rvh_die_kernel_fault, NULL);
#endif

	return 0;
}
module_init(secdbg_slab_info_init);

static void __exit secdbg_slab_info_exit(void)
{
}
module_exit(secdbg_slab_info_exit);

MODULE_DESCRIPTION("Samsung Debug slab info driver");
MODULE_LICENSE("GPL v2");
