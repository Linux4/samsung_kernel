/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 * Filename     : osa_mem.c
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the source code of memory-related functions in osa.
 *
 */

/*
 ******************************
 *          HEADERS
 ******************************
 */

/*
 * typically OSA supports the test of checking memory leak.
 * all the memory alloc/free and new/delete will be overloaded
 * by the memory-leak-checking tool.
 * since osa_mem.c implements the memory management behaviors,
 * the interfaces in this file should not be overloaded.
 * the macro, "OSA_DISABLE_ML_OSA_MEM", makes this file invisible to
 * the overloading behaviors of the memory-leak checking tool.
 */
#define OSA_DISABLE_ML_OSA_MEM

#include <osa.h>

/*
 ******************************
 *          MACROS
 ******************************
 */

/*
 * *normally* alloc_pages can support less than
 * or equal to OSA_4M memory block.
 * the capability of alloc_pages can be configured
 * in .config. (CONFIG_FORCE_MAX_ZONEORDER)
 */
#define KMEM_ALLOC_MAX_SIZE     (OSA_4M)

#define L1_CACHE_LINE_SIZE      (32)
#define L2_CACHE_LINE_SIZE      (32)

#define L1_CACHE_LINE_MASK      (0xFFFFFFE0)

#define MAKE_PAGE_VALID(va)                                         \
do {                                                                \
	volatile uint32_t tmp;                                          \
	uint32_t ret;                                                   \
	uint32_t *p = (uint32_t *)&tmp;                                 \
	ret = copy_from_user((void *)p, (void __user *)va, 1);          \
	ret = copy_to_user((void __user *)va, (__force void *)p, 1);    \
} while (0)

/*
 ******************************
 *          TYPES
 ******************************
 */

struct _pages_record {
	struct osa_list node;

	void __kernel *vir_addr;
	uint32_t size;
};

#if 0

struct _phys_mem_info_header {
	void *alloc_virt_addr;
	void *map_virt_addr;
	uint32_t map_size;
	bool is_cached;
};

#else

struct _phys_mem_info_header {
	void *alloc_virt_addr;
	void *alloc_phys_addr;
	uint32_t alloc_size;
	bool is_cached;
};

#endif

/*
 ******************************
 *          VARIABLES
 ******************************
 */
static struct osa_list _g_alloc_pages_list = OSA_LIST_HEAD(_g_alloc_pages_list);
static DEFINE_SPINLOCK(_g_pg_walk_by_cp15_lock);

static DEFINE_SEMAPHORE(_g_list_sema);

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

/*
 * Name:        osa_kmem_alloc
 *
 * Description: allocate physical/virtual-sequential memory block in kernel
 *
 * Params:      size - the size of the memory block
 *
 * Returns:     void __kernel * - the pointer to the memory block allocated
 *
 * Notes:       no call in isr
 */
void __kernel *osa_kmem_alloc(uint32_t size)
{
	OSA_ASSERT(size);

	if (size < OSA_128K)
		return (void __kernel *)kmalloc(size, GFP_KERNEL);
	else if (size <= KMEM_ALLOC_MAX_SIZE) {
		struct page *page;
		struct _pages_record *record;
		void __kernel *ret;

		record =
		    (struct _pages_record *)
		    kmalloc(sizeof(struct _pages_record), GFP_KERNEL);
		if (!record) {
			osa_dbg_print(DBG_ERR,
				      "ERROR - failed to alloc a page record in osa_kmem_alloc\n");
			return NULL;
		}

		page = alloc_pages(GFP_KERNEL, get_order(size));
		if (!page) {
			osa_dbg_print(DBG_ERR,
				      "ERROR - failed to alloc pages in osa_kmem_alloc\n");
			kfree(record);
			return NULL;
		}

		ret = (void __kernel *)page_address(page);

		record->vir_addr = ret;
		record->size = size;

		down(&_g_list_sema);
		osa_list_add(&record->node, &_g_alloc_pages_list);
		up(&_g_list_sema);

		return ret;
	} else {
		osa_dbg_print(DBG_ERR,
			      "ERROR - too large memory block required\n");
		return NULL;
	}
}
OSA_EXPORT_SYMBOL(osa_kmem_alloc);

/*
 * Name:        osa_kmem_free
 *
 * Description: free the memory block allocated by osa_kmem_alloc
 *
 * Params:      addr - the pointer to the memory block to free
 *
 * Returns:     none
 *
 * Notes:       no call in isr
 */
void osa_kmem_free(void __kernel *addr)
{
	struct _pages_record *record = NULL;
	struct osa_list *ptr = NULL;

	OSA_ASSERT(addr);

	down(&_g_list_sema);
	osa_list_iterate(&_g_alloc_pages_list, ptr) {
		OSA_LIST_ENTRY(ptr, struct _pages_record, node, record);
		OSA_ASSERT(record);
		if (record->vir_addr == addr)
			break;
	}

	if (ptr != &_g_alloc_pages_list) {
		/* find record in the list */
		free_pages((ulong_t) addr, get_order(record->size));
		osa_list_del(&record->node);
		up(&_g_list_sema);
		kfree(record);
	} else {
		up(&_g_list_sema);
		kfree(addr);
	}
}
OSA_EXPORT_SYMBOL(osa_kmem_free);

/*
 * Name:        osa_vmem_alloc
 *
 * Description: allocate the virtual-sequential memory block in kernel
 *
 * Params:      size - the size of the memory block
 *              attr - attribute of the memory block, see osa_mem_attr_t
 *
 * Returns:     void __kernel * - the pointer to the memory block allocated
 *
 * Notes:       no call in isr
 *              OSA_MEM_WRITE_ONLY is not supported in linux
 */
void __kernel *osa_vmem_alloc(uint32_t size, osa_mem_attr_t attr)
{
	void __kernel *ret = NULL;

	OSA_ASSERT(size);

	switch (attr) {
	case OSA_MEM_NO_ACCESS:
		ret = __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_NONE);
		break;
	case OSA_MEM_READ_ONLY:
		ret =
		    __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_READONLY);
		break;
	case OSA_MEM_WRITE_ONLY:
		ret = __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL);
		break;
	case OSA_MEM_READ_WRITE:
		ret = __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL);
		break;
	default:
		OSA_ASSERT(0);
	}

	return ret;
}
OSA_EXPORT_SYMBOL(osa_vmem_alloc);

/*
 * Name:        osa_vmem_free
 *
 * Description: free the memory block allocated by osa_vmem_alloc
 *
 * Params:      addr - the pointer to the memory block to free
 *
 * Returns:     none
 *
 * Notes:       no call in isr
 */
void osa_vmem_free(void __kernel *addr)
{
	OSA_ASSERT(addr);

	vfree(addr);
}
OSA_EXPORT_SYMBOL(osa_vmem_free);

/*
 * Name:        osa_pages_alloc
 *
 * Description: allocate pages in kernel
 *
 * Params:      nr - the number of pages
 *
 * Returns:     void __kernel * - the pointer to the memory block allocated
 *
 * Notes:       no call in isr
 */
void __kernel *osa_pages_alloc(uint32_t nr)
{
	uint32_t size = PAGE_SIZE * nr;

	OSA_ASSERT(nr);

	if (size <= KMEM_ALLOC_MAX_SIZE) {
		struct page *page;
		struct _pages_record *record;
		void __kernel *ret;
		void *phys;
		uint32_t i;

		record =
		    (struct _pages_record *)
		    kmalloc(sizeof(struct _pages_record), GFP_KERNEL);
		if (!record) {
			osa_dbg_print(DBG_ERR,
				      "ERROR - failed to alloc a page record in osa_pages_alloc\n");
			return NULL;
		}

		page = alloc_pages(GFP_KERNEL, get_order(size));
		if (!page) {
			osa_dbg_print(DBG_ERR,
				      "ERROR - failed to alloc pages in osa_pages_alloc\n");
			kfree(record);
			return NULL;
		}

		ret = (void __kernel *)page_address(page);

		/*
		 * in the current kernel,
		 * remap_pfn_range works in the condition of
		 * PageReserved is set.
		 */
		phys = __virt_to_phys(ret);
		for (i = 0; i < nr; i++) {
			SetPageReserved(pfn_to_page
					(((uint32_t) phys >> PAGE_SHIFT) + i));
		}

		record->vir_addr = ret;
		record->size = size;

		down(&_g_list_sema);
		osa_list_add(&record->node, &_g_alloc_pages_list);
		up(&_g_list_sema);

		return ret;
	} else {
		osa_dbg_print(DBG_ERR,
			      "ERROR - too large memory block required\n");
		return NULL;
	}
}
OSA_EXPORT_SYMBOL(osa_pages_alloc);

/*
 * Name:        osa_pages_free
 *
 * Description: free the memory block allocated by osa_pages_alloc
 *
 * Params:      addr - the pointer to the memory block to free
 *
 * Returns:     none
 *
 * Notes:       no call in isr
 */
void osa_pages_free(void __kernel *addr)
{
	struct _pages_record *record = NULL;
	struct osa_list *ptr = NULL;
	void *phys;
	uint32_t nr;
	uint32_t i;

	OSA_ASSERT(addr);

	down(&_g_list_sema);
	osa_list_iterate(&_g_alloc_pages_list, ptr) {
		OSA_LIST_ENTRY(ptr, struct _pages_record, node, record);
		OSA_ASSERT(record);
		if (record->vir_addr == addr)
			break;
	}

	OSA_ASSERT(ptr != &_g_alloc_pages_list);

	phys = __virt_to_phys(addr);
	nr = record->size >> PAGE_SHIFT;
	for (i = 0; i < nr; i++) {
		ClearPageReserved(pfn_to_page
				  (((uint32_t) phys >> PAGE_SHIFT) + i));
	}

	/* find record in the list */
	free_pages((ulong_t) addr, get_order(record->size));
	osa_list_del(&record->node);
	up(&_g_list_sema);
	kfree(record);
}
OSA_EXPORT_SYMBOL(osa_pages_free);

osa_err_t osa_phys_mem_pool_init(void)
{
	return OSA_OK;
}

void osa_phys_mem_pool_cleanup(void)
{
	return;
}

#if 0

void *osa_alloc_phys_mem(uint32_t size, bool is_cached, uint32_t alignment,
			 void **phys)
{
	uint32_t mem_blk_size;
	void *alloc_virt_addr;
	void *map_virt_addr;
	void *ret_virt_addr;
	struct _phys_mem_info_header *rec_virt_addr;

	if (!size || !phys)
		return NULL;

	mem_blk_size = size + sizeof(struct _phys_mem_info_header) + alignment;

	alloc_virt_addr = osa_kmem_alloc(mem_blk_size);
	if (NULL == alloc_virt_addr) {
		*phys = NULL;
		return NULL;
	}

	/* flush the dcache for shared physical memory with ioremap. */
	osa_flush_dcache_all();
	osa_flush_l2dcache_all();

	if (is_cached) {
		map_virt_addr =
		    osa_ioremap_cached(osa_virt_to_phys(alloc_virt_addr),
				       mem_blk_size);
	} else {
		map_virt_addr =
		    osa_ioremap(osa_virt_to_phys(alloc_virt_addr),
				mem_blk_size);
	}
	if (NULL == map_virt_addr) {
		*phys = NULL;
		osa_kmem_free(alloc_virt_addr);
		return NULL;
	}

	ret_virt_addr = (void *)(((uint32_t)
				  map_virt_addr + alignment - 1) & ~(alignment -
								     1));
	do {
		if (ret_virt_addr - map_virt_addr
		    >= sizeof(struct _phys_mem_info_header)) {
			rec_virt_addr =
			    (struct _phys_mem_info_header *)((uint32_t)
				     ret_virt_addr -
				     sizeof(struct
					 _phys_mem_info_header));
			break;
		} else {
			ret_virt_addr = (void *)((uint32_t) ret_virt_addr +
						 alignment);
		}
	} while (1);

	OSA_ASSERT((uint32_t) rec_virt_addr >= (uint32_t) map_virt_addr);
	OSA_ASSERT((uint32_t) ret_virt_addr + size
		   <= (uint32_t) map_virt_addr + mem_blk_size);

	rec_virt_addr->alloc_virt_addr = alloc_virt_addr;
	rec_virt_addr->map_virt_addr = map_virt_addr;
	rec_virt_addr->map_size = mem_blk_size;
	rec_virt_addr->is_cached = is_cached;

	*phys = osa_virt_to_phys(ret_virt_addr);

	return ret_virt_addr;
}
OSA_EXPORT_SYMBOL(osa_alloc_phys_mem);

void osa_free_phys_mem(void *virt)
{
	struct _phys_mem_info_header *rec_virt_addr;
	void *map_virt_addr;
	uint32_t map_size;
	void *alloc_virt_addr;

	OSA_ASSERT(virt);

	rec_virt_addr = (struct _phys_mem_info_header *)((uint32_t) virt -
				 sizeof(struct
				 _phys_mem_info_header));

	alloc_virt_addr = rec_virt_addr->alloc_virt_addr;
	map_virt_addr = rec_virt_addr->map_virt_addr;
	map_size = rec_virt_addr->map_size;

	if (rec_virt_addr->is_cached)
		osa_iounmap_cached(map_virt_addr, map_size);
	else
		osa_iounmap(map_virt_addr, map_size);

	osa_kmem_free(alloc_virt_addr);
}
OSA_EXPORT_SYMBOL(osa_free_phys_mem);

#else

void *osa_alloc_phys_mem(uint32_t size, bool is_cached, uint32_t alignment,
			 void **phys)
{
	uint32_t mem_blk_size;
	void *alloc_virt_addr;
	void *alloc_phys_addr;
	void *ret_virt_addr;
	struct _phys_mem_info_header *rec_virt_addr;

	if (!size || !phys)
		return NULL;

	mem_blk_size = size + sizeof(struct _phys_mem_info_header) + alignment;

	if (is_cached) {
		alloc_virt_addr = kmalloc(mem_blk_size, GFP_KERNEL);
		if (NULL == alloc_virt_addr) {
			*phys = NULL;
			return NULL;
		}
		alloc_phys_addr = __virt_to_phys(alloc_virt_addr);
	} else {
		alloc_virt_addr =
		    dma_alloc_coherent(NULL, mem_blk_size,
				       (dma_addr_t *)&alloc_phys_addr,
				       GFP_KERNEL);
		if (NULL == alloc_virt_addr) {
			*phys = NULL;
			return NULL;
		}
	}

	/* calculate the virtual address returned */
	ret_virt_addr = (void *)(((uint32_t)
				  alloc_virt_addr + alignment -
				  1) & (~(alignment - 1)));
	do {
		if (ret_virt_addr - alloc_virt_addr
		    >= sizeof(struct _phys_mem_info_header)) {
			rec_virt_addr =
			    (struct _phys_mem_info_header *)((uint32_t)
				     ret_virt_addr -
				     sizeof(struct
					    _phys_mem_info_header));
			break;
		} else {
			ret_virt_addr = (void *)((uint32_t) ret_virt_addr +
						 alignment);
		}
	} while (1);

	OSA_ASSERT((uint32_t) rec_virt_addr >= (uint32_t) alloc_virt_addr);
	OSA_ASSERT((uint32_t) ret_virt_addr + size
		   <= (uint32_t) alloc_virt_addr + mem_blk_size);

	rec_virt_addr->alloc_virt_addr = alloc_virt_addr;
	rec_virt_addr->alloc_phys_addr = alloc_phys_addr;
	rec_virt_addr->alloc_size = mem_blk_size;
	rec_virt_addr->is_cached = is_cached;

	*phys = (void *)((uint32_t) alloc_phys_addr +
			 ((uint32_t) ret_virt_addr -
			  (uint32_t) alloc_virt_addr));

	return ret_virt_addr;
}
OSA_EXPORT_SYMBOL(osa_alloc_phys_mem);

void osa_free_phys_mem(void *virt)
{
	struct _phys_mem_info_header *rec_virt_addr;

	OSA_ASSERT(virt);

	rec_virt_addr = (struct _phys_mem_info_header *)((uint32_t) virt -
					 sizeof(struct
						_phys_mem_info_header));

	if (rec_virt_addr->is_cached) {
		kfree(rec_virt_addr->alloc_virt_addr);
	} else {
		dma_free_coherent(NULL, rec_virt_addr->alloc_size,
				  rec_virt_addr->alloc_virt_addr,
				  (dma_addr_t) rec_virt_addr->alloc_phys_addr);
	}
}
OSA_EXPORT_SYMBOL(osa_free_phys_mem);

#endif
#if 0
/*
 * Name:        _osa_ioremap
 *
 * Description: map a physical-sequential addressing space to kernel space
 *
 * Params:      phy_addr - the physical address of the addressing space
 *              size     - the size of the addressing space (1M aligned)
 *              flags    - the flags in pte. see asm/pgtable.h
 *
 * Returns:     void __kernel * - the virtual address of the mapping block
 *                                NULL for an error occures.
 *
 * Notes:       none
 */
static void __kernel *_osa_ioremap(void *phy_addr, uint32_t size, ulong_t flags)
{
	/* flags is no checked since it is not exported to user */
	if (flags & (L_PTE_CACHEABLE | L_PTE_BUFFERABLE))
		return ioremap_cached((ulong_t) phy_addr, size);
	else
		return ioremap((ulong_t) phy_addr, size);

}
#endif
/*
 * Name:        _osa_iounmap
 *
 * Description: unmap a physical-sequential addressing space from kernel space
 *
 * Params:      vir_addr - the virtual address to be unmapped from
 *              size     - the size of mapping memory block (no use)
 *
 * Returns:     status code
 *              TZ_OK          - success
 *              TZ_OSA_BAD_ARG - bad argument
 *
 * Notes:       none
 */
static osa_err_t _osa_iounmap(void __kernel *vir_addr, uint32_t size)
{
#ifdef MK2
	__iounmap((void __iomem *)vir_addr);
#else
	__arm_iounmap((void __iomem *)vir_addr);
#endif
	return OSA_OK;
}

/*
 * Name:        osa_ioremap
 *
 * Description: the wrapper for mapping noncacheable-nonbufferable space
 *
 * Params:      phy_addr - the physical address of the addressing space
 *              size     - the size of the addressing space (1M aligned)
 *
 * Returns:     void __kernel * - the virtual address of the mapping block
 *                                NULL for an error occures.
 *
 * Notes:       none
 */
void __kernel *osa_ioremap(void *phy_addr, uint32_t size)
{
	return ioremap((ulong_t) phy_addr, size);
}
OSA_EXPORT_SYMBOL(osa_ioremap);

/*
 * Name:        osa_iounmap
 *
 * Description: the wrapper for unmapping noncacheable-nonbufferable space
 *
 * Params:      vir_addr - the virtual address to be unmapped from
 *              size     - the size of mapping memory block (no use)
 *
 * Returns:     status code
 *              TZ_OK          - success
 *              TZ_OSA_BAD_ARG - bad argument
 * Notes:       none
 */
osa_err_t osa_iounmap(void __kernel *vir_addr, uint32_t size)
{
	return _osa_iounmap((void __iomem *)vir_addr, size);
}
OSA_EXPORT_SYMBOL(osa_iounmap);

/*
 * Name:        osa_ioremap_cached
 *
 * Description: the wrapper for mapping cacheable-bufferable space
 *
 * Params:      phy_addr - the physical address of the addressing space
 *              size     - the size of the addressing space (1M aligned)
 *
 * Returns:     void __kernel * - the virtual address of the mapping block
 *                                NULL for an error occures.
 *
 * Notes:       none
 */
void __kernel *osa_ioremap_cached(void *phy_addr, uint32_t size)
{
	return ioremap_cached((ulong_t) phy_addr, size);
}
OSA_EXPORT_SYMBOL(osa_ioremap_cached);

/*
 * Name:        osa_iounmap_cached
 *
 * Description: the wrapper for unmapping cacheable-bufferable space
 *
 * Params:      vir_addr - the virtual address to be unmapped from
 *              size     - the size of mapping memory block (no use)
 *
 * Returns:     status code
 *              TZ_OK          - success
 *              TZ_OSA_BAD_ARG - bad argument
 *
 * Notes:       none
 */
osa_err_t osa_iounmap_cached(void __kernel *vir_addr, uint32_t size)
{
	return _osa_iounmap((void __iomem *)vir_addr, size);
}
OSA_EXPORT_SYMBOL(osa_iounmap_cached);

static void *_virt_to_phys_by_pg_mapping(struct mm_struct *mm, void *va)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	ulong_t pfn;
#ifdef pte_offset_map_lock
	spinlock_t *ptl;
#endif
	void *ret = INVALID_PHYS_ADDR;

	down_read(&mm->mmap_sem);

	pgd = pgd_offset(mm, (ulong_t) va);
	if (!pgd_present(*pgd))
		goto _no_pgd;

	/* pmd = pmd_offset(pgd, (ulong_t)va); */
	/* update for kernel version */
	pmd = pmd_offset((pud_t *) pgd, (ulong_t) va);
	if (!pmd_present(*pmd))
		goto _no_pmd;

#ifdef pte_offset_map_lock
	pte = pte_offset_map_lock(mm, pmd, (ulong_t) va, &ptl);
#else
	pte = pte_offset_map(pmd, (ulong_t) va);
#endif
	if (!pte_present(*pte))
		goto _no_pte;

	pfn = pte_pfn(*pte);

	ret = (void *)((pfn << PAGE_SHIFT)
		       + ((ulong_t) va & (~PAGE_MASK)));
_no_pte:
#ifdef pte_offset_map_lock
	pte_unmap_unlock(pte, ptl);
#endif

_no_pmd:
_no_pgd:
	up_read(&mm->mmap_sem);

	return ret;
}

static void *_osa_virt_to_phys_by_cp15(uint32_t virt)
{
	ulong_t flags;
	volatile uint32_t ret;

	spin_lock_irqsave(&_g_pg_walk_by_cp15_lock, flags);

	__asm__ __volatile__("mcr p15, 0, %1, c7, c8, 1\n\t"
		"mrc p15, 0, %0, c7, c4, 0\n\t" : "=r"(ret) : "r"
		(virt));

	spin_unlock_irqrestore(&_g_pg_walk_by_cp15_lock, flags);

	ret &= ~(PAGE_SIZE - 1);
	ret |= (virt & (PAGE_SIZE - 1));

	return (void *)ret;
}

/*
 * Name:        osa_virt_to_phys
 *
 * Description: translate virtual address to physical address
 *
 * Params:      virt_addr - the virtual address
 *
 * Returns:     void * - the physical address
 *
 * Notes:       page table directory is saved in swapper_pg_dir
 */
void *osa_virt_to_phys(void *virt_addr)
{
	if ((uint32_t) virt_addr < PAGE_OFFSET)
		return _virt_to_phys_by_pg_mapping(current->mm, virt_addr);
	else if (virt_addr < high_memory)
		return __virt_to_phys(virt_addr);
	else
		return _osa_virt_to_phys_by_cp15((uint32_t) virt_addr);

}
OSA_EXPORT_SYMBOL(osa_virt_to_phys);

/*
 * Name:        osa_invalidate_dcache
 *
 * Description: invalidate the data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       invalidate the entry of the data cache only
 */
void osa_invalidate_dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
	ulong_t end = start + size;

	start = (start & L1_CACHE_LINE_MASK);
	end = ((end + L1_CACHE_LINE_SIZE - 1) & L1_CACHE_LINE_MASK);

	asm volatile (" \
		    mov ip, #0; \
		 1: mcr p15, 0, %0, c7, c6, 1; \
		    add %0, %0, #32; \
		    cmp %0, %1; \
		    blo 1b; \
		    mrc p15, 0, ip, c7, c10, 4; @ drain write buffer \
		    mov ip, ip; \
		    sub pc, pc, #4; \
		    "
		    :
		    : "r" (start), "r"(end)
		    : "ip");
	return;
}
OSA_EXPORT_SYMBOL(osa_invalidate_dcache);

/*
 * Name:        osa_clean_dcache
 *
 * Description: clean the data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       write back the content of cache to the external memory only
 */
void osa_clean_dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
	ulong_t end = start + size;

	start = (start & L1_CACHE_LINE_MASK);
	end = ((end + L1_CACHE_LINE_SIZE - 1) & L1_CACHE_LINE_MASK);

	asm volatile (" \
		    mov ip, #0; \
		 1: mcr p15, 0, %0, c7, c10, 1; \
		    add %0, %0, #32; \
		    cmp %0, %1; \
		    blo 1b; \
		    mrc p15, 0, ip, c7, c10, 4; @ drain write buffer \
		    mov ip, ip; \
		    sub pc, pc, #4; \
		    "
		    :
		    : "r" (start), "r"(end)
		    : "ip");
	return;
}
OSA_EXPORT_SYMBOL(osa_clean_dcache);

/*
 * Name:        osa_flush_dcache
 *
 * Description: flush the data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       flush = clean + invalidate
 */
void osa_flush_dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
#if 0
	ulong_t end = start + size;

	start = (start & L1_CACHE_LINE_MASK);
	end = ((end + L1_CACHE_LINE_SIZE - 1) & L1_CACHE_LINE_MASK);

	asm volatile (" \
		    mov ip, #0; \
		 1: mcr p15, 0, %0, c7, c14, 1; @ clean & invalidate D line \
		    add %0, %0, #32; \
		    cmp %0, %1; \
		    blo 1b; \
		    mrc p15, 0, ip, c7, c10, 4; @ drain write buffer \
		    mov ip, ip; \
		    sub pc, pc, #4; \
		    "
		    :
		    : "r" (start), "r"(end)
		    : "ip");
	return;
#else
	/* flush_cache_all(); */
	dmac_flush_range((void *)start, (void *)(start + size));
#endif
}
OSA_EXPORT_SYMBOL(osa_flush_dcache);

/*
 * Name:        osa_flush_dcache_all
 *
 * Description: flush all the data cache
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       flush = clean + invalidate
 */
void osa_flush_dcache_all(void)
{
	flush_cache_all();
}
OSA_EXPORT_SYMBOL(osa_flush_dcache_all);

#if 0

/*
 * Name:        _l2cache_clean_va
 *
 * Description: invalidate the l2 data cache line by the virtual address
 *
 * Params:      addr - the virtual address
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_inv_va(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c7, 1"
			:
			: "r"(addr));
}

/*
 * Name:        _l2cache_clean_va
 *
 * Description: clean the l2 data cache line by the virtual address
 *
 * Params:      addr - the virtual address
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_clean_va(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c11, 1"
			:
			: "r"(addr));
}

/*
 * Name:        _l2cache_flush_va
 *
 * Description: flush the l2 data cache line by the virtual address
 *
 * Params:      addr - the virtual address
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_flush_va(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c15, 1"
			:
			: "r"(addr));
}

/*
 * Name:        _l2cache_inv_all
 *
 * Description: invalidate all the l2 data cache lines
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_inv_all(void)
{
	__asm__ __volatile__("mcr p15, 1, %0, c7, c7, 0"
						:
						: "r"(0));
	dsb();
}

/*
 * Name:        _l2cache_clean_all
 *
 * Description: clean all the l2 data cache lines
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_clean_all(void)
{
	__asm__ __volatile__("mcr p15, 1, %0, c7, c11, 0"
						:
						: "r"(0));
	dsb();
}

/*
 * Name:        _l2cache_flush_all
 *
 * Description: flush all the l2 data cache lines
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
static inline void _l2cache_flush_all(void)
{
	__asm__ __volatile__("mcr p15, 1, %0, c7, c11, 0"
						:
						: "r"(0));
	__asm__ __volatile__("mcr p15, 1, %0, c7, c7, 0"
						:
						: "r"(0));
	dsb();
}

#endif

/*
 * Name:        osa_invalidate_l2dcache
 *
 * Description: invalidate the data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       invalidate the entry of the data cache only
 */
void osa_invalidate_l2dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
#if 0
	ulong_t end;

	if ((0 == start) && ((uint32_t) (-1) == size)) {
		_l2cache_inv_all();
		return;
	}

	end = start + size;

	if (start & (L2_CACHE_LINE_SIZE - 1)) {
		_l2cache_flush_va(start & ~(L2_CACHE_LINE_SIZE - 1));
		start = (start | (L2_CACHE_LINE_SIZE - 1)) + 1;
	}

	if (start < end && (end & (L2_CACHE_LINE_SIZE - 1))) {
		_l2cache_flush_va(end & ~(L2_CACHE_LINE_SIZE - 1));
		end &= ~(L2_CACHE_LINE_SIZE - 1);
	}

	while (start < end) {
		_l2cache_inv_va(start);
		start += L2_CACHE_LINE_SIZE;
	}

	dsb();
#else
	ulong_t phys = (ulong_t) osa_virt_to_phys((void *)start);
	outer_inv_range(phys, phys + size);
	OSA_ASSERT(0);
#endif
}
OSA_EXPORT_SYMBOL(osa_invalidate_l2dcache);

/*
 * Name:        osa_l2clean_dcache
 *
 * Description: clean the l2 data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       write back the content of cache to the external memory only
 */
void osa_clean_l2dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
#if 0
	ulong_t end = start + size;

	if ((0 == start) && ((uint32_t) (-1) == size)) {
		_l2cache_clean_all();
		return;
	}

	start &= ~(L2_CACHE_LINE_SIZE - 1);
	while (start < end) {
		_l2cache_clean_va(start);
		start += L2_CACHE_LINE_SIZE;
	}

	dsb();
#else
	ulong_t phys = (ulong_t) osa_virt_to_phys((void *)start);
	outer_clean_range(phys, phys + size);
	OSA_ASSERT(0);
#endif
}
OSA_EXPORT_SYMBOL(osa_clean_l2dcache);

/*
 * Name:        osa_flush_l2dcache
 *
 * Description: flush the l2 data cache by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       flush = clean + invalidate
 */
void osa_flush_l2dcache(osa_mode_t mode, ulong_t start, uint32_t size)
{
#if 0
	ulong_t end;

	if ((0 == start) && ((uint32_t) (-1) == size)) {
		_l2cache_flush_all();
		return;
	}

	end = start + size;

	start &= ~(L2_CACHE_LINE_SIZE - 1);
	while (start < end) {
		_l2cache_flush_va(start);
		start += L2_CACHE_LINE_SIZE;
	}

	dsb();
#else
/*
	outer_flush_range(start, start + size);

 result not correct!
	void *phys = osa_virt_to_phys((void *)start);
	outer_flush_range((uint32_t)phys, (uint32_t)phys + size);

 crash
	outer_flush_range(0, -1UL);

 result not correct!
	unsigned long set_way;
	int set, way;

	for (set = 0; set < 0x800; set++) {
		for (way = 0; way < 8; way++) {
			set_way = (way << 29) | (set << 5);
			__asm__("mcr p15, 0, %0, c7, c7, 2"
					:
					: "r"(set_way));
		}
	}

 result not correct!
*/
	/* clean & invalidate all of l2 dcache */
/*
	asm volatile ("mrc p15, 0, %0, c5, c14, 0"
				:
				: "r"(0));
	dsb();

 result not correct!
*/
#if 0
	asm volatile (" \
			mrc p15, 0, %0, c5, c10, 0; \
			mrc p15, 0, %0, c5, c6, 0;  \
			"
			:
			: "r" (0));
	dsb();
#endif /* 0 */
/*
result not correct
	ulong_t phys = (ulong_t)osa_virt_to_phys((void *)start);
	outer_flush_range(phys, phys + size);
*/
	{
		ulong_t flags;
		local_irq_save(flags);
		outer_flush_range(0, -1UL);
		local_irq_restore(flags);
	}
#endif
}
OSA_EXPORT_SYMBOL(osa_flush_l2dcache);

/*
 * Name:        osa_flush_l2dcache_all
 *
 * Description: flush all the L2 data cache
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       flush = clean + invalidate
 */
void osa_flush_l2dcache_all(void)
{
#if 0
	_l2cache_flush_all();
#else
	outer_flush_range(0, -1UL);
	OSA_ASSERT(0);
#endif
}
OSA_EXPORT_SYMBOL(osa_flush_l2dcache_all);

/*
 * Name:        osa_flush_tlb
 *
 * Description: flush the tlb by the range of [start, start + size)
 *
 * Params:      mode  - kernel or user mode
 *              start - start address
 *              size  - size of the range
 *
 * Returns:     none
 *
 * Notes:       flush = invalidate
 */
void osa_flush_tlb(osa_mode_t mode, ulong_t start, uint32_t size)
{
	/* FIXME: temporary implementation. */
	/* flush_tlb_all(); */
}
OSA_EXPORT_SYMBOL(osa_flush_tlb);

/*
 * Name:        osa_flush_tlb_all
 *
 * Description: flush all the tlb
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_flush_tlb_all(void)
{
	/* flush_tlb_all(); */
}
OSA_EXPORT_SYMBOL(osa_flush_tlb_all);

#if 0

/*
 * Name:        osa_validate_vmem
 *
 * Description: validate the memory in user space
 *              the memory will be kept from swap
 *
 * Params:      vir_addr - the virtual address of the memory block
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_BAD_USER_VIR_ADDR - bad virtual address in user space
 *
 * Notes:       the function enforces to allocate *real* memory for user space
 *              no copy_xxx_user needed then
 *              FIXME - SetPageLocked not invoked.
 */
osa_err_t osa_validate_vmem(void __user *vir_addr, uint32_t size)
{

	uint32_t va = (uint32_t) vir_addr;
	uint32_t sz = size, sz_bak;
	uint32_t range;

	OSA_ASSERT(vir_addr && size);

	if ((uint32_t) vir_addr >= PAGE_OFFSET)
		return OSA_BAD_USER_VIR_ADDR;

	/*
	 * since no swap action in arm-linux,
	 * here we need not hold the mmap_sem and SetPageLocked.
	 */
	do {
		MAKE_PAGE_VALID(va);

		range = PAGE_SIZE - (va % PAGE_SIZE);
		va += range;
		sz_bak = sz;
		sz -= range;
	} while (sz_bak > range);

	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_validate_vmem);

/*
 * Name:        osa_invalidate_vmem
 *
 * Description: invalidate the memory in user space.
 *              the memory is again suitable to be swapped by kswapd.
 *
 * Params:      vir_addr - the virtual address of the memory block
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_BAD_USER_VIR_ADDR - bad virtual address in user space
 *
 * Notes:       since there is no swap action in arm linux,
 *              this function is set (almost) NULL.
 *              FIXME - ClearPageLocked not invoked.
 */
osa_err_t osa_invalidate_vmem(void __user *vir_addr, uint32_t size)
{
	OSA_ASSERT(vir_addr && size);

	if ((uint32_t) vir_addr >= PAGE_OFFSET)
		return OSA_BAD_USER_VIR_ADDR;

	/*
	 *  if swap is enabled, we need to clear
	 *  the locked flag for each page here.
	 */

	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_invalidate_vmem);

#else

osa_err_t osa_validate_vmem(void __user *vir_addr, uint32_t size)
{
	ulong_t start = ((ulong_t) vir_addr & PAGE_MASK);
	ulong_t end = ((ulong_t) vir_addr + size + PAGE_SIZE - 1) & PAGE_MASK;

	struct vm_area_struct *vma;
	ulong_t cur = start, cur_end;
	int32_t err, write;

	if (!size)
		return OSA_OK;

	down_read(&current->mm->mmap_sem);

	do {
		vma = find_vma(current->mm, cur);
		if (!vma) {
			up_read(&current->mm->mmap_sem);
			return OSA_ERR;
		}
		cur_end = ((end <= vma->vm_end) ? end : vma->vm_end);

		/* this vma is NOT mmap-ed and pfn-direct mapped */
		if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
			write = ((vma->vm_flags & VM_WRITE) != 0);
			err = get_user_pages(current, current->mm,
					     (ulong_t) cur,
					     (cur_end - cur) >> PAGE_SHIFT,
					     write, 0, NULL, NULL);
			if (err < 0) {
				up_read(&current->mm->mmap_sem);
				return OSA_ERR;
			}
		}

		cur = cur_end;
	} while (cur_end < end);

	up_read(&current->mm->mmap_sem);

	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_validate_vmem);

osa_err_t osa_invalidate_vmem(void __user *vir_addr, uint32_t size)
{
	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_invalidate_vmem);

#endif

/*
 * Name:        osa_validate_string
 *
 * Description: validate the space of string in user space
 *
 * Params:      string - the virtual address of the string
 *
 * Returns:     status code
 *              OSA_OK      - no error
 *              OSA_BAD_ARG - bad argument
 *
 * Notes:       no size required in string
 *              when allocating a *real* block for it.
 */
osa_err_t osa_validate_string(void __user *string)
{
	OSA_ASSERT(string);

	return osa_validate_vmem(string, strlen_user(string) + 1);
}
OSA_EXPORT_SYMBOL(osa_validate_string);

/*
 * Name:        osa_invalidate_string
 *
 * Description: invalidate the memory of string in user space.
 *              the memory is again suitable to be swapped by kswapd.
 *
 * Params:      string - the virtual address of the string
 *
 * Returns:     status code
 *              OSA_OK      - no error
 *              OSA_BAD_ARG - bad argument
 *
 * Notes:       since there is no swap action in arm linux,
 *              this function is set (almost) NULL.
 */
osa_err_t osa_invalidate_string(void __user *string)
{
	OSA_ASSERT(string);

	return osa_invalidate_vmem(string, strlen_user(string) + 1);
}
OSA_EXPORT_SYMBOL(osa_invalidate_string);
