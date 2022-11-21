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
 * Filename     : osa_mem.h
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the header file of memory-related functions in osa.
 *
 */

#ifndef _OSA_MEM_H_
#define _OSA_MEM_H_

/*
 ******************************
 *          HEADERS
 ******************************
 */

#include <osa.h>

/*
 ******************************
 *           MACROS
 ******************************
 */

#ifndef PAGE_SHIFT
#define PAGE_SHIFT                  (12)
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE                   (1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
#define PAGE_MASK                   (~(PAGE_SIZE - 1))
#endif

#ifndef SECTION_SHIFT
#define SECTION_SHIFT               (20)
#endif
#ifndef SECTION_SIZE
#define SECTION_SIZE                (1UL << SECTION_SHIFT)
#endif
#ifndef SECTION_MASK
#define SECTION_MASK                (~(SECTION_SIZE - 1))
#endif

#define INVALID_PHYS_ADDR           ((void *)0xFFFFFFFF)
#define INVALID_USER_VIRT_ADDR      ((void *)0xFFFFFFFF)

/* for OSA in secure world */
#define OSA_MAP_EC                  (0x1)
#define OSA_MAP_EB                  (0x2)
#define OSA_MAP_RO                  (0x4)
#define OSA_MAP_WO                  (0x8)
#define OSA_MAP_EWR                 (OSA_MAP_RO | OSA_MAP_WO)

/*
 ******************************
 *        ENUMERATIONS
 ******************************
 */

typedef enum osa_mem_attr {
	OSA_MEM_NO_ACCESS,	/* can not access */
	OSA_MEM_READ_ONLY,	/* read only mapping */
	OSA_MEM_WRITE_ONLY,	/* write only mapping */
	OSA_MEM_READ_WRITE	/* read and write */
} osa_mem_attr_t;

/* for cache/TLB only */
typedef enum _osa_mode_t {
	MODE_USER,
	MODE_KERNEL
} osa_mode_t;

/* for OSA in secure world */
typedef enum osa_mmap_type {
	OSA_MMAP_SECTION,	/* mapping as section */
	OSA_MMAP_LARGE_PAGE,	/* mapping as 16k page */
	OSA_MMAP_SMALL_PAGE,	/* mapping as 4k page */
	OSA_MMAP_TINY_PAGE,	/* mapping as 1k page */
} osa_mmap_type_t;

/* for OSA in secure world */
typedef enum osa_mpool_type {
	OSA_MPOOL_FIX,		/* fixed memory pool */
	OSA_MPOOL_VAR,		/* variable memory pool */
	OSA_MPOOL_SEP,		/* sepmeta memory pool */
	OSA_MPOOL_DL		/* dynamic memory pool */
} osa_mpool_type_t;

/*
 ******************************
 *         STRUCTURES
 ******************************
 */

struct osa_page_entry {
	void *phy_addr;
	uint32_t len;

	struct osa_page_entry *next;
};

/* for OSA in secure world */
struct osa_mpool_info {
	osa_mpool_type_t type;
	void *base;
	uint32_t size;
	uint32_t block_size;
	void *meta_addr;
	uint32_t meta_len;
	uint32_t alignment;
};

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

/* allocate/free physical and virtual continuous memory block */
extern void __kernel *osa_kmem_alloc(uint32_t size);
extern void osa_kmem_free(void __kernel *addr);

/* allocate/free virtual continuous memory block */
extern void __kernel *osa_vmem_alloc(uint32_t size, osa_mem_attr_t attr);
extern void osa_vmem_free(void __kernel *addr);

/* allocate/free pages */
extern void __kernel *osa_pages_alloc(uint32_t nr);
extern void osa_pages_free(void __kernel *addr);

/* allocate/free physical continuous memory block w' cache & alignment */
extern osa_err_t osa_phys_mem_pool_init(void);
extern void osa_phys_mem_pool_cleanup(void);
extern void __kernel *osa_alloc_phys_mem(uint32_t size, bool is_cached,
					 uint32_t alignment, void **phys);
extern void osa_free_phys_mem(void __kernel *virt);

/* memory map/unmap */
extern osa_err_t osa_iomap_rgn_init(void);
extern void osa_iomap_rgn_cleanup(void);
extern void __kernel *osa_ioremap(void *phy_addr, uint32_t size);
extern osa_err_t osa_iounmap(void __kernel *vir_addr, uint32_t size);
extern void __kernel *osa_ioremap_cached(void *phy_addr, uint32_t size);
extern osa_err_t osa_iounmap_cached(void __kernel *vir_addr, uint32_t size);

/* the following two functions should be called in process context */
extern osa_err_t osa_map_to_user(void __user *vir_addr, void *phy_addr,
				 uint32_t size, uint32_t prot);
extern osa_err_t osa_unmap_from_user(void __user *vir_addr);

/* memory map/unmap */
extern osa_err_t osa_map_section(void *phy_addr, void *vir_addr, uint32_t size,
				 uint32_t property);
extern osa_err_t osa_unmap_section(void *vir_addr, uint32_t size);
extern osa_err_t osa_map_page(void *phy_addr, void *vir_addr,
			      void *pte_phy_addr, void *pte_vir_addr,
			      uint32_t size, uint32_t property);
extern osa_err_t osa_unmap_page(void *vir_addr, uint32_t size);
extern osa_err_t osa_set_map_property(void *vir_addr, uint32_t size,
				      osa_mmap_type_t type, uint32_t property);

/* memory pool related interfaces */
extern osa_mpool_t osa_mpool_create(struct osa_mpool_info *arg);
extern osa_err_t osa_mpool_destroy(osa_mpool_t handle);
extern void *osa_mpool_alloc(osa_mpool_t handle, uint32_t len);
extern void osa_mpool_free(osa_mpool_t handle, void *addr);
extern osa_err_t osa_mpool_get_info(osa_mpool_t handle,
				    struct osa_mpool_info *mpool_info);

/* NW only */
/* caller needs to free the list generated by osa_vmem_to_pages */
extern struct osa_page_entry *osa_vmem_to_pages(void __user *vir_addr,
						uint32_t size);

/* memory will be real-allocated by calling this function */
/* NOTE: no copy_xxx_user needed then */
extern osa_err_t osa_validate_vmem(void __user *vir_addr, uint32_t size);
/* re-enable the swap-able attribute of the memory block */
extern osa_err_t osa_invalidate_vmem(void __user *vir_addr, uint32_t size);
/* memory will be real-allocated for a string */
extern osa_err_t osa_validate_string(void __user *string);
/* re-enable the swap-able attribute of the memory block */
extern osa_err_t osa_invalidate_string(void __user *string);

/* translate virtual address to physical address */
extern void *osa_virt_to_phys(void *virt_addr);
/* for SW only */
extern void *osa_phys_to_virt(void __kernel *phys_addr);

/* cache-related and TLB-related interfaces */
extern void osa_invalidate_dcache(osa_mode_t mode, ulong_t start,
				uint32_t size);
extern void osa_clean_dcache(osa_mode_t mode, ulong_t start, uint32_t size);
extern void osa_flush_dcache(osa_mode_t mode, ulong_t start, uint32_t size);
extern void osa_flush_dcache_all(void);
extern void osa_invalidate_l2dcache(osa_mode_t mode, ulong_t start,
				uint32_t size);
extern void osa_clean_l2dcache(osa_mode_t mode, ulong_t start, uint32_t size);
extern void osa_flush_l2dcache(osa_mode_t mode, ulong_t start, uint32_t size);
extern void osa_flush_l2dcache_all(void);
extern void osa_flush_tlb(osa_mode_t mode, ulong_t start, uint32_t size);
extern void osa_flush_tlb_all(void);

#endif /* _OSA_MEM_H_ */
