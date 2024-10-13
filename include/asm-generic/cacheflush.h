/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_CACHEFLUSH_H
#define __ASM_CACHEFLUSH_H

/* Keep includes the same across arches.  */
#include <linux/mm.h>

/*
 * The cache doesn't need to be flushed when TLB entries change when
 * the cache is mapped to physical memory, not virtual memory
 */
#ifdef CONFIG_ARCH_SPRD
static inline void __flush_dcache_all(void)
{
	__asm__ __volatile__("dmb sy\n\t"
			"mrs x0, clidr_el1\n\t"
			"and x3, x0, #0x7000000\n\t"
			"lsr x3, x3, #23\n\t"
			"cbz x3, 5f\n\t"
			"mov x10, #0\n\t"
		"1:\n\t"
			"add x2, x10, x10, lsr #1\n\t"
			"lsr x1, x0, x2\n\t"
			"and x1, x1, #7\n\t"
			"cmp x1, #2\n\t"
			"b.lt    4f\n\t"
			"mrs x9, daif\n\t"
			"msr daifset, #2\n\t"
			"msr csselr_el1, x10\n\t"
			"isb\n\t"
			"mrs x1, ccsidr_el1\n\t"
			"msr daif, x9\n\t"
			"and x2, x1, #7\n\t"
			"add x2, x2, #4\n\t"
			"mov x4, #0x3ff\n\t"
			"and x4, x4, x1, lsr #3\n\t"
			"clz w5, w4\n\t"
			"mov x7, #0x7fff\n\t"
			"and x7,x7, x1, lsr #13\n\t"
		"2:\n\t"
			"mov x9, x4\n\t"
		"3:\n\t"
			"lsl x6, x9, x5\n\t"
			"orr x11, x10, x6\n\t"
			"lsl x6, x7, x2\n\t"
			"orr x11, x11, x6\n\t"
			"dc  cisw, x11\n\t"
			"subs    x9, x9, #1\n\t"
			"b.ge    3b\n\t"
			"subs    x7, x7, #1\n\t"
			"b.ge    2b\n\t"
		"4:\n\t"
			"add x10, x10, #2\n\t"
			"cmp x3, x10\n\t"
			"b.gt    1b\n\t"
		"5:\n\t"
			"mov x10, #0\n\t"
			"msr csselr_el1, x10\n\t"
			"dsb sy\n\t"
			"isb\n\t");

}
#define flush_cache_all()			__flush_dcache_all()
#else
#define flush_cache_all()			do { } while (0)
#endif

#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_dup_mm(mm)			do { } while (0)
#define flush_cache_range(vma, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)	do { } while (0)
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0
#define flush_dcache_page(page)			do { } while (0)
#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)
#define flush_icache_range(start, end)		do { } while (0)
#define flush_icache_page(vma,pg)		do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)
#define flush_cache_vmap(start, end)		do { } while (0)
#define flush_cache_vunmap(start, end)		do { } while (0)

#define copy_to_user_page(vma, page, vaddr, dst, src, len) \
	do { \
		memcpy(dst, src, len); \
		flush_icache_user_range(vma, page, vaddr, len); \
	} while (0)
#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)

#endif /* __ASM_CACHEFLUSH_H */
