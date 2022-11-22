/*
 * Contiguous Memory Allocator for DMA mapping framework
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_CMA_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif

#include <asm/page.h>
#include <asm/dma-contiguous.h>

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/page-isolation.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/mm_types.h>
#include <linux/dma-contiguous.h>

#ifndef SZ_1M
#define SZ_1M (1 << 20)
#endif

struct cma {
	unsigned long	base_pfn;
	unsigned long	count;
	unsigned long	*bitmap;
};

struct cma *dma_contiguous_default_area;

int cma_available;

#ifdef CONFIG_CMA_SIZE_MBYTES
#define CMA_SIZE_MBYTES CONFIG_CMA_SIZE_MBYTES
#else
#define CMA_SIZE_MBYTES 0
#endif

/*
 * Default global CMA area size can be defined in kernel's .config.
 * This is usefull mainly for distro maintainers to create a kernel
 * that works correctly for most supported systems.
 * The size can be set in bytes or as a percentage of the total memory
 * in the system.
 *
 * Users, who want to set the size of global CMA area for their system
 * should use cma= kernel parameter.
 */
static const unsigned long size_bytes = CMA_SIZE_MBYTES * SZ_1M;
static long size_cmdline = -1;

static int __init early_cma(char *p)
{
	pr_debug("%s(%s)\n", __func__, p);
	size_cmdline = memparse(p, &p);
	return 0;
}
early_param("cma", early_cma);

#ifdef CONFIG_CMA_SIZE_PERCENTAGE

static unsigned long __init __maybe_unused cma_early_percent_memory(void)
{
	struct memblock_region *reg;
	unsigned long total_pages = 0;

	/*
	 * We cannot use memblock_phys_mem_size() here, because
	 * memblock_analyze() has not been called yet.
	 */
	for_each_memblock(memory, reg)
		total_pages += memblock_region_memory_end_pfn(reg) -
			       memblock_region_memory_base_pfn(reg);

	return (total_pages * CONFIG_CMA_SIZE_PERCENTAGE / 100) << PAGE_SHIFT;
}

#else

static inline __maybe_unused unsigned long cma_early_percent_memory(void)
{
	return 0;
}

#endif

/**
 * dma_contiguous_reserve() - reserve area for contiguous memory handling
 * @limit: End address of the reserved memory (optional, 0 for any).
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory.
 */
void __init dma_contiguous_reserve(phys_addr_t limit)
{
	unsigned long selected_size = 0;

	pr_debug("%s(limit %08lx)\n", __func__, (unsigned long)limit);

	if (size_cmdline != -1) {
		selected_size = size_cmdline;
	} else {
#ifdef CONFIG_CMA_SIZE_SEL_MBYTES
		selected_size = size_bytes;
#elif defined(CONFIG_CMA_SIZE_SEL_PERCENTAGE)
		selected_size = cma_early_percent_memory();
#elif defined(CONFIG_CMA_SIZE_SEL_MIN)
		selected_size = min(size_bytes, cma_early_percent_memory());
#elif defined(CONFIG_CMA_SIZE_SEL_MAX)
		selected_size = max(size_bytes, cma_early_percent_memory());
#endif
	}

	if (selected_size) {
		pr_debug("%s: reserving %ld MiB for global area\n", __func__,
			 selected_size / SZ_1M);

		dma_declare_contiguous(NULL, selected_size, 0, limit);
	}
};

static DEFINE_MUTEX(cma_mutex);

static __init int cma_activate_area(unsigned long base_pfn, unsigned long count)
{
	unsigned long pfn = base_pfn;
	unsigned i = count >> pageblock_order;
	struct zone *zone;

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;
		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			if (page_zone(pfn_to_page(pfn)) != zone)
				return -EINVAL;
		}
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);
	return 0;
}

static __init struct cma *cma_create_area(unsigned long base_pfn,
				     unsigned long count)
{
	int bitmap_size = BITS_TO_LONGS(count) * sizeof(long);
	struct cma *cma;
	int ret = -ENOMEM;

	pr_debug("%s(base %08lx, count %lx)\n", __func__, base_pfn, count);

	cma = kmalloc(sizeof *cma, GFP_KERNEL);
	if (!cma)
		return ERR_PTR(-ENOMEM);

	cma->base_pfn = base_pfn;
	cma->count = count;
	cma->bitmap = kzalloc(bitmap_size, GFP_KERNEL);

	if (!cma->bitmap)
		goto no_mem;

	ret = cma_activate_area(base_pfn, count);
	if (ret)
		goto error;

	pr_debug("%s: returned %p\n", __func__, (void *)cma);
	return cma;

error:
	kfree(cma->bitmap);
no_mem:
	kfree(cma);
	return ERR_PTR(ret);
}

static struct cma_reserved {
	phys_addr_t start;
	unsigned long size;
	struct device *dev;
} cma_reserved[MAX_CMA_AREAS] __initdata;
static unsigned cma_reserved_count __initdata;

#include <linux/seq_file.h>
#include <linux/proc_fs.h>
extern unsigned long migrate_page_copy_count;
static int cma_info_show(struct seq_file *s, void *unused)
{
	struct cma *cma = dev_get_cma_area(NULL);
	unsigned long start = 0, set = 0, end = 0, sum = 0;
	int nr_per_order[32];
	int i, total = 0, order, order_max = 0;
	struct page *pg;

	seq_printf(s, "CMA Region: pfn(%lx:%lx) phy(%08x:%08x)\n",
		cma->base_pfn, cma->base_pfn + cma->count - 1,
		__pfn_to_phys(cma->base_pfn),
		__pfn_to_phys(cma->base_pfn + cma->count) - 1);

	seq_printf(s, "\n( Un-Set    )           [ Set       ]\n");
	while (1) {
		set = find_next_bit(cma->bitmap, cma->count, start);
		if (set >= cma->count)
			break;
		end = find_next_zero_bit(cma->bitmap, cma->count, set);

		if (set > 0)
			seq_printf(s, "(%5lx:%5lx) %5lx ",
				cma->base_pfn + start, cma->base_pfn + set - 1,
				set - start);
		else
			seq_printf(s, "%16.s", "");

		seq_printf(s, "\t[%5lx:%5lx] %5lx\n", cma->base_pfn + set,
			cma->base_pfn + end - 1, end - set);

		start = end;
		sum += (end - set);
	}

	if (start < cma->count)
		seq_printf(s, "(%5lx:%5lx) %5lx\n",
			cma->base_pfn + start, cma->base_pfn + cma->count - 1,
			cma->count - start);

	seq_printf(s, "Total: %12lx%24lx%12lx\n",
		cma->count - sum, sum, cma->count);
	seq_printf(s, "\nMigrate copy count: %lx\n", migrate_page_copy_count);

	for (i = 0; i < 32; i++)
		nr_per_order[i] = 0;
	pg = pfn_to_page(cma->base_pfn);
	start = -1;
	for (i = 0; i < cma->count; i++, pg++) {
		if (!test_bit(i, cma->bitmap) && !page_count(pg)) {
			if (start == -1)
				start = i;
			end = i;

			if (i < (cma->count - 1))
				continue;
		}
		if (start != -1) {
			total += (end - start + 1);
			order = fls(end - start + 1) - 1;

			nr_per_order[order]++;
			start = -1;
			if (order_max < order)
				order_max = order;
		}
	}

	seq_printf(s, "\nIdle pages per order, total: %d\nOrder:", total);
	for (i = 0; i <= order_max; i++)
		seq_printf(s, "%6d ", i);

	seq_printf(s, "\nCount:");
	for (i = 0; i <= order_max; i++)
		seq_printf(s, "%6d ", nr_per_order[i]);
	seq_printf(s, "\n");

	return 0;
}

static int cma_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, cma_info_show, inode->i_private);
}

static const struct file_operations cma_info_fops = {
	.open = cma_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init cma_init_reserved_areas(void)
{
	struct cma_reserved *r = cma_reserved;
	unsigned i = cma_reserved_count;

	if (!cma_available)
		return 0;

	pr_debug("%s()\n", __func__);

	for (; i; --i, ++r) {
		struct cma *cma;
		cma = cma_create_area(PFN_DOWN(r->start),
				      r->size >> PAGE_SHIFT);
		if (!IS_ERR(cma))
			dev_set_cma_area(r->dev, cma);
	}

	proc_create("cmainfo", S_IRUGO, NULL, &cma_info_fops);
	return 0;
}
core_initcall(cma_init_reserved_areas);

/**
 * dma_declare_contiguous() - reserve area for contiguous memory handling
 *			      for particular device
 * @dev:   Pointer to device structure.
 * @size:  Size of the reserved memory.
 * @base:  Start address of the reserved memory (optional, 0 for any).
 * @limit: End address of the reserved memory (optional, 0 for any).
 *
 * This function reserves memory for specified device. It should be
 * called by board specific code when early allocator (memblock or bootmem)
 * is still activate.
 */
int __init dma_declare_contiguous(struct device *dev, unsigned long size,
				  phys_addr_t base, phys_addr_t limit)
{
	struct cma_reserved *r = &cma_reserved[cma_reserved_count];
	unsigned long alignment;

	pr_debug("%s(size %lx, base %08lx, limit %08lx)\n", __func__,
		 (unsigned long)size, (unsigned long)base,
		 (unsigned long)limit);

	/* Sanity checks */
	if (cma_reserved_count == ARRAY_SIZE(cma_reserved)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	/* Sanitise input arguments */
	alignment = PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	/* Reserve memory */
	if (base) {
		if (memblock_is_region_reserved(base, size) ||
		    memblock_reserve(base, size) < 0) {
			base = -EBUSY;
			goto err;
		}
	} else {
		/*
		 * Use __memblock_alloc_base() since
		 * memblock_alloc_base() panic()s.
		 */
		phys_addr_t addr = __memblock_alloc_base(size, alignment, limit);
		if (!addr) {
			base = -ENOMEM;
			goto err;
		} else if (addr + size > ~(unsigned long)0) {
			memblock_free(addr, size);
			base = -EINVAL;
			goto err;
		} else {
			base = addr;
		}
	}

	/*
	 * Each reserved area must be initialised later, when more kernel
	 * subsystems (like slab allocator) are available.
	 */
	r->start = base;
	r->size = size;
	r->dev = dev;
	cma_reserved_count++;
	pr_info("CMA: reserved %ld MiB at %08lx\n", size / SZ_1M,
		(unsigned long)base);

	/* Architecture specific contiguous memory fixup. */
	dma_contiguous_early_fixup(base, size);

	cma_available = 1;
	return 0;
err:
	pr_err("CMA: failed to reserve %ld MiB\n", size / SZ_1M);
	return base;
}

static int cma_bitmap_show(struct device *dev)
{
	struct cma *cma = dev_get_cma_area(dev);
	unsigned long start = 0, set = 0, end = 0, sum = 0;

	pr_info("cma free list pfn[%lx %lx]: dev(%s)\n", cma->base_pfn,
		cma->base_pfn + cma->count - 1, dev ? dev_name(dev) : "");

	while (1) {
		set = find_next_bit(cma->bitmap, cma->count, start);
		if (set >= cma->count)
			break;
		end = find_next_zero_bit(cma->bitmap, cma->count, set);

		if (set > 0)
			pr_info("[%6lx:%6lx] %6lx %6lx",
				cma->base_pfn + start, cma->base_pfn + set - 1,
				set - start, end - set);
		start = end;
		sum += (end - set);
	}

	if (start < cma->count)
		pr_info("[%6lx:%6lx] %6lx ",
			cma->base_pfn + start, cma->base_pfn + cma->count - 1,
			cma->count - start);

	pr_info("Total: free(%lx) set(%lx) all(%lx)\n",
		cma->count - sum, sum, cma->count);
	return 0;
}

/**
 * dma_alloc_from_contiguous() - allocate pages from contiguous area
 * @dev:   Pointer to device for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 *
 * This function allocates memory buffer for specified device. It uses
 * device specific contiguous memory area if available or the default
 * global one. Requires architecture specific get_dev_cma_area() helper
 * function.
 */
struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int align)
{
	unsigned long mask, pfn = -1, pageno, start = 0;
	struct cma *cma = dev_get_cma_area(dev);
	struct page *page = NULL;
	int ret;

	if (!cma || !cma->count)
		return NULL;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	pr_debug("%s(cma %p, count %d, align %d)\n", __func__, (void *)cma,
		 count, align);

	if (!count)
		return NULL;

	mask = (1 << align) - 1;

	mutex_lock(&cma_mutex);

	for (;;) {
		pageno = bitmap_find_next_zero_area(cma->bitmap, cma->count,
						    start, count, mask);
		if (pageno >= cma->count)
			break;

		pfn = cma->base_pfn + pageno;
		ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA);
		if (ret == 0) {
			bitmap_set(cma->bitmap, pageno, count);
			page = pfn_to_page(pfn);
			break;
		} else if (ret != -EBUSY) {
			break;
		}
		pr_debug("%s(): memory range at %p (%lx, %lx) is busy,"\
			" retrying\n", __func__, pfn_to_page(pfn),
			pfn, pfn + count);
		/* try again with a bit different memory target */
		start = pageno + mask + 1;
	}

	if (!page)
		cma_bitmap_show(dev);

	mutex_unlock(&cma_mutex);
	pr_debug("%s(): returned %p pfn(%lx)\n", __func__, page, pfn);
	return page;
}

/**
 * dma_release_from_contiguous() - release allocated pages
 * @dev:   Pointer to device for which the pages were allocated.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by dma_alloc_from_contiguous().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count)
{
	struct cma *cma = dev_get_cma_area(dev);
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	mutex_lock(&cma_mutex);
	bitmap_clear(cma->bitmap, pfn - cma->base_pfn, count);
	free_contig_range(pfn, count);
	mutex_unlock(&cma_mutex);

	return true;
}
